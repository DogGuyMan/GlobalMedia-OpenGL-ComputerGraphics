# IPlayable Component Interface 설계 (M3.5 — Playable 본격 도입)

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **작성일**: 2026-05-26
> **브랜치**: `game/module/rendertarget` (M3 완료 직후)
> **관련 spec**:
> - [`2026-05-24-topdown-shooter-design.md`](2026-05-24-topdown-shooter-design.md) §1.5/§1.6 (원본 — 본 spec 이 명시적으로 *수정/폐기* 항목 정리)
> - [`2026-05-25-fsm-object-state-machine-design.md`](2026-05-25-fsm-object-state-machine-design.md) (인접 — `SJH::FSM::StateMachine` 가 Playable 과 결합되는 방식)
> **진행 보고서**: [`doc/topdown-shooter-progress.md`](../../../doc/topdown-shooter-progress.md) M3.5
> **결정 횟수**: 7건 (Phase 1~5, Phase 4-bis 포함)

---

## §0 Goal

`SJH::playable` + `SJH::sprite_sequence` 코어 모듈을 별도화한다. 사용자(Client) 가 `Play / Pause / Stop / GetIsLoop` 4-method 만 인지하면 단일/시퀀스/병렬 시간축 추상화를 모두 다룰 수 있다. 호스트 컴포넌트는 *별도 도입하지 않는다* — 본 저장소 `SJH::Scene::Component` 시스템이 이미 동일 역할.

**Tweeny + DOTween 정통**:
- Tweeny: header-only fluent chaining (`.from().to().during().via()` 가 `*this` 반환)
- DOTween: Composite Sequence (`.Append().Insert().Join()` + *hierarchical nesting without depth limits*)

본 spec 은 두 라이브러리의 *Building 패턴 정수* 를 `IPlayable` Composite 에 적용 — `vector<unique_ptr<IPlayable>>` 컨테이너 + 다형 중첩 + `*this` 반환 fluent Builder.

---

## §1 핵심 결정 (7건)

| # | 항목 | 결정 |
|---|---|---|
| 1 | **`Stop()` 의미** | Reset 후 재사용 가능 — tick 중단 + 내부 상태 (paused_/finished_/elapsed_/cursor_) 리셋 + `IsFinished()=false`. 다음 `Play()` 시 첫 프레임부터. Pause 와 차이: Pause = 재개 대기, Stop = 리셋 후 재사용 대기. (Unity AudioSource.Stop 정통) |
| 2 | **`GetIsLoop()` 동작** | loop=true 면 Playable 내부에서 자동 재시작 (raw frame %= frameCount 식의 wrap) — `IsFinished()` 절대 true 안 됨. BGM/loop sound 류는 `Stop()` 호출 전까지 영원 재생. spec §1.6 의 현재 wrap 로직 유지. (호출자는 IsFinished + Stop 만 인지) |
| 3 | **Tick / IsFinished 위치** | (a) 별도 `Tick(dt)` 메서드 폐기 — `SJH::Scene::Component::Update(float dt)` 가 그 역할. `Actor::Update` / `Scene::Update` 가 매 프레임 자동 디스패치 (이미 존재). (b) `IsFinished()` 는 IPlayable 의 5번째 public — Composite 가 child 종료 감지 + 외부 (Bullet despawn 등) 가 조회. (c) spec §1.5 `PlayablePlayerComponent` + `PlayableTickSystem` 자유함수 **폐기** |
| 4 | **Composite 구조** | `SequencePlayable` / `ParallelPlayable` 도 IPlayable(=PlayableBase) 구현체. **한 Actor 에 1개 root Composite Component 부착 (강제 아닌 컨벤션 — `Actor::AddComponent` 가 시스템적으로는 다중 부착 허용)**. children = `std::vector<std::unique_ptr<IPlayable>>` — children 은 Component 이지만 `Actor::AddComponent` 를 거치지 않아 `mOwner=nullptr`, `Actor::GetComponent<T>` 로 검색 안 됨. Composite 가 OnEnter/OnExit/Update/Play/Stop 을 children 에 수동 디스패치 |
| 4 (Stop 전파) | **Composite.Stop** | 재귀 reset — 모든 `children_[i]->Stop()` + 자기 `cursor=0 / elapsed=0 / finished=false / paused=false` |
| 4 (Loop) | **Composite.GetIsLoop** | true 면 모든 children 종료 시 `cursor=0` + 자동 `children_[0]->Play()` 재시작 — 무한 반복 |
| 4-bis | **컨테이너 + Builder + 중첩** | `std::vector<std::unique_ptr<IPlayable>>` 명시 (concrete Composite 만). Tweeny `*this` + DOTween 정통 — `Sequence.Append(child)` / `Sequence.Insert(pos, child)` / `Parallel.Join(child)` 가 *derived 타입* 참조 반환 (`SequencePlayable&` / `ParallelPlayable&`) 라서 `.Append(...).Append(...)` 체이닝 + IPlayable 다형으로 *Sequence 안 Parallel 안 Sequence ...* 무제한 중첩 |
| 5 | **abstract 레벨 (2-tier)** | `class IPlayable` = pure interface (저장소 `IMovable`/`ILivable` 컨벤션 준수, 모든 멤버 `=0`, protected ctor + delete copy/move). `class PlayableBase : public IPlayable, public SJH::Scene::Component` = 다중 상속 abstract base — paused_/finished_/elapsed_/isLoop_ 보유 + Play/Pause/Stop trivial impl + Update→OnUpdate hook 디스패치. concrete (FmodPlayable / SpriteSequencePlayable / ParticlePlayable / SequencePlayable / ParallelPlayable) 는 PlayableBase 상속, `OnUpdate / OnPlay / OnStop` hook 만 override |
| 5-부속 | **`SetIsLoop(bool)` 위치** | IPlayable 인터페이스에는 **포함 안 함** (Client 우선 4-method 의 readonly 변형은 `GetIsLoop` 만 유지). `SetIsLoop` 는 PlayableBase 의 *추가 public 메서드* — IPlayable* 핸들로는 호출 불가, PlayableBase*/concrete 핸들에서만 호출. 이유: spec §1 결정 1 (Stop=reset) 과 결정 2 (loop=true 자동 재시작) 로부터 *Loop 토글이 라이프사이클의 일부* 가 아니라 *세팅 단계 속성* 이므로 인터페이스에서 분리 — Builder/세팅 후 변경 없는 일반 케이스 우선 |

---

## §2 모듈 레이아웃

### §2.1 신규 모듈 (코어 2개 → `src/CMakeLists.txt` 우산 합류 14 → 16)

```
src/playable/
├─ CMakeLists.txt                  # add_library(sjh_playable STATIC ...) + ALIAS SJH::playable
├─ iplayable.h                     # IPlayable pure interface (5 virtual: Play/Pause/Stop/GetIsLoop/IsFinished)
├─ playable_base.h                 # PlayableBase : IPlayable, Component — abstract base + default state/impl
├─ playable_base.cpp               # PlayableBase 의 Play/Pause/Stop/Update trivial 구현
├─ composite_playable.h            # SequencePlayable / ParallelPlayable : PlayableBase
└─ composite_playable.cpp          # OnPlay/OnStop/OnUpdate + Append/Insert/Join 정의

src/sprite_sequence/
├─ CMakeLists.txt                  # add_library(sjh_sprite_sequence STATIC ...) + ALIAS SJH::sprite_sequence
├─ sprite_frame_clip.h             # SpriteFrameClip POD (startFrame / frameCount / fps) — loop 필드 제거
└─ sprite_sequence_playable.h      # SpriteSequencePlayable : PlayableBase — OnUpdate 만 override
```

### §2.2 `src/CMakeLists.txt` 합류

```cmake
# 추가
add_subdirectory(playable)         # ← 15번째
add_subdirectory(sprite_sequence)  # ← 16번째

# 우산 INTERFACE 합류
target_link_libraries(sjhopengl_engine INTERFACE
    ...
    SJH::playable          # M3.5 — 15 모듈
    SJH::sprite_sequence   # M3.5 — 16 모듈
)
```

### §2.3 모듈 의존 그래프

```
SJH::playable          PUBLIC: SJH::scene (Component 베이스), SJH::common
                       PRIVATE: spdlog (warn-once)

SJH::sprite_sequence   PUBLIC: SJH::playable (PlayableBase), SJH::sprite (SpriteRenderer)

SJH::engine (INTERFACE) ← 위 둘 합류

leaf — Client 거주 (game_deps 의존)
  <apps>/_MyApp_/src/Audio/fmod_playable.h         : PlayableBase   ← fmod
  <apps>/_MyApp_/src/VFX/effekseer_playable.h      : PlayableBase   ← Effekseer
```

---

## §3 인터페이스/베이스 정의

### §3.1 `IPlayable` — pure interface (저장소 컨벤션 준수)

```cpp
// src/playable/iplayable.h
#ifndef __SJH_PLAYABLE_IPLAYABLE_H__
#define __SJH_PLAYABLE_IPLAYABLE_H__

namespace SJH::Playable
{
    /// @brief 시간축 추상화 — Client 우선 4 메서드 (Play/Pause/Stop/GetIsLoop) + 2급 IsFinished.
    /// @details
    ///   - 저장소 `I*` 컨벤션 준수: pure interface, 모든 멤버 =0, protected ctor + delete copy/move.
    ///   - 실제 상태/디폴트 임플리먼테이션은 `PlayableBase` (다중 상속 abstract base) 가 흡수.
    ///   - Tick 메서드 없음 — Component 시스템의 `Update(float dt)` 가 그 역할 (PlayableBase 흡수).
    class IPlayable
    {
      protected:
        IPlayable() = default;

      public:
        virtual ~IPlayable() = default;
        IPlayable(const IPlayable&)            = delete;
        IPlayable& operator=(const IPlayable&) = delete;
        IPlayable(IPlayable&&)                 = delete;
        IPlayable& operator=(IPlayable&&)      = delete;

        // === Client 우선 4-method ===
        virtual void Play()  = 0;       // 재생 시작 / Pause 후 재개 / Stop 후 첫 프레임부터
        virtual void Pause() = 0;       // 일시정지 (상태 보존, Play 로 재개)
        virtual void Stop()  = 0;       // 리셋 후 정지 (재사용 대기, 다음 Play 는 첫 프레임)
        virtual bool GetIsLoop() const = 0;

        // === 2급 공개 — Composite 의 child 종료 감지 + 외부 despawn 결정 ===
        virtual bool IsFinished() const = 0;
    };
}

#endif // __SJH_PLAYABLE_IPLAYABLE_H__
```

### §3.2 `PlayableBase` — abstract base + Component 다중 상속

```cpp
// src/playable/playable_base.h
#ifndef __SJH_PLAYABLE_PLAYABLE_BASE_H__
#define __SJH_PLAYABLE_PLAYABLE_BASE_H__

#include "playable/iplayable.h"
#include "scene/actor.h"   // SJH::Scene::Component

namespace SJH::Playable
{
    /// @brief IPlayable + Component 다중 상속 abstract base.
    ///        공통 상태 (paused_/finished_/elapsed_/isLoop_) + Play/Pause/Stop trivial 구현 + Update→OnUpdate hook.
    /// @note  concrete (FmodPlayable / SpriteSequencePlayable / SequencePlayable / ...) 는 OnUpdate 만 필수 override.
    ///        OnPlay / OnStop 은 default empty — 필요한 concrete 만 override.
    class PlayableBase : public IPlayable, public SJH::Scene::Component
    {
      protected:
        PlayableBase() = default;

      public:
        virtual ~PlayableBase() = default;

        // === IPlayable 4-method + IsFinished ===
        void Play()  override
        {
            paused_   = false;
            finished_ = false;
            OnPlay();
        }
        void Pause() override { paused_ = true; }   // ← OnPause hook 없음 — Pause 는 trivial flag 만. leaf 의 외부 자원 (FMOD/Effekseer) 일시정지는 concrete 가 Pause 자체를 override 해서 처리
        void Stop()  override
        {
            paused_   = false;
            finished_ = false;
            elapsed_  = 0.0f;
            OnStop();
        }
        bool GetIsLoop()  const override { return isLoop_; }
        bool IsFinished() const override { return finished_; }

        // === Loop setter — 인터페이스 외 추가 (사용자 결정 4-bis) ===
        void SetIsLoop(bool v) { isLoop_ = v; }

        // === Component 3 hook ===
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) final
        {
            if (!IsEnabled() || paused_ || finished_) return;
            elapsed_ += dt;
            OnUpdate(dt);
        }

      protected:
        // === 파생 hook — concrete 가 override ===
        virtual void OnPlay()   {}
        virtual void OnStop()   {}
        virtual void OnUpdate(float dt) = 0;   // 유일 필수

        // === 파생 공유 상태 (protected) ===
        bool  paused_   = false;
        bool  finished_ = false;
        bool  isLoop_   = false;
        float elapsed_  = 0.0f;
    };
}

#endif // __SJH_PLAYABLE_PLAYABLE_BASE_H__
```

**다중 상속 안전성** — `IPlayable` 과 `SJH::Scene::Component` 는 공통 베이스가 없으므로 diamond 문제 없음. 두 베이스 모두 virtual dtor 보유라 cleanup OK.

### §3.3 `SequencePlayable` / `ParallelPlayable` — Composite

```cpp
// src/playable/composite_playable.h
#ifndef __SJH_PLAYABLE_COMPOSITE_PLAYABLE_H__
#define __SJH_PLAYABLE_COMPOSITE_PLAYABLE_H__

#include "playable/playable_base.h"
#include <memory>
#include <vector>

namespace SJH::Playable
{
    /// @brief 순차 — children 한 번에 1개. 현재 child 가 IsFinished 면 다음 child Play.
    ///        Loop=true 면 모든 children 종료 시 cursor=0 + Play() 재시작.
    /// @note  Builder Append/Insert 가 `SequencePlayable&` 반환 (derived 타입) — 체이닝 시 derived 메서드 유지.
    class SequencePlayable : public PlayableBase
    {
      public:
        // === Fluent Builder (Tweeny `*this` + DOTween Append/Insert 정통) ===
        SequencePlayable& Append(std::unique_ptr<IPlayable> child);
        SequencePlayable& Insert(std::size_t pos, std::unique_ptr<IPlayable> child);

        // 디버그/관찰 (옵션)
        std::size_t Size()   const { return children_.size(); }
        std::size_t Cursor() const { return cursor_; }

      protected:
        void OnPlay()   override;
        void OnStop()   override;
        void OnUpdate(float dt) override;

      private:
        std::vector<std::unique_ptr<IPlayable>> children_;   // ← 사용자 명문화
        std::size_t cursor_ = 0;
    };

    /// @brief 병렬 — children 모두 동시 Play. 모두 IsFinished 일 때 자기 IsFinished.
    ///        Loop=true 면 모든 children 종료 시 자동 Play 재시작 (= 무한 반복).
    class ParallelPlayable : public PlayableBase
    {
      public:
        ParallelPlayable& Join(std::unique_ptr<IPlayable> child);

        std::size_t Size() const { return children_.size(); }

      protected:
        void OnPlay()   override;
        void OnStop()   override;
        void OnUpdate(float dt) override;

      private:
        std::vector<std::unique_ptr<IPlayable>> children_;   // ← 사용자 명문화
    };
}

#endif // __SJH_PLAYABLE_COMPOSITE_PLAYABLE_H__
```

```cpp
// src/playable/composite_playable.cpp
#include "playable/composite_playable.h"

namespace SJH::Playable
{
    // ───── SequencePlayable ─────

    SequencePlayable& SequencePlayable::Append(std::unique_ptr<IPlayable> child)
    {
        children_.push_back(std::move(child));
        return *this;
    }

    SequencePlayable& SequencePlayable::Insert(std::size_t pos, std::unique_ptr<IPlayable> child)
    {
        if (pos > children_.size()) pos = children_.size();
        children_.insert(children_.begin() + pos, std::move(child));
        return *this;
    }

    void SequencePlayable::OnPlay()
    {
        cursor_ = 0;
        if (!children_.empty()) children_[0]->Play();
    }

    void SequencePlayable::OnStop()
    {
        for (auto& c : children_) c->Stop();    // ← 재귀 reset
        cursor_ = 0;
    }

    void SequencePlayable::OnUpdate(float dt)
    {
        // 빈 컨테이너 가드 — isLoop_=true 라도 무한 빈 루프 회피
        if (children_.empty()) { finished_ = true; return; }

        if (cursor_ >= children_.size())
        {
            // 모든 children 종료
            if (isLoop_)
            {
                cursor_ = 0;
                children_[0]->Play();          // ← Loop=true 자동 재시작 (empty 가드 통과했으므로 [0] 안전)
            }
            else
            {
                finished_ = true;
            }
            return;
        }

        // children 의 Tick 진입점 — children 은 Component 이지만 Actor 미부착이라
        // Actor::Update 가 닿지 않음. Composite 가 명시 Update 디스패치.
        auto* head = dynamic_cast<SJH::Scene::Component*>(children_[cursor_].get());
        if (head && head->IsEnabled()) head->Update(dt);

        if (children_[cursor_]->IsFinished())
        {
            ++cursor_;
            if (cursor_ < children_.size()) children_[cursor_]->Play();
        }
    }

    // ───── ParallelPlayable ─────

    ParallelPlayable& ParallelPlayable::Join(std::unique_ptr<IPlayable> child)
    {
        children_.push_back(std::move(child));
        return *this;
    }

    void ParallelPlayable::OnPlay()
    {
        for (auto& c : children_) c->Play();
    }

    void ParallelPlayable::OnStop()
    {
        for (auto& c : children_) c->Stop();
    }

    void ParallelPlayable::OnUpdate(float dt)
    {
        // 빈 컨테이너 가드 — vacuously true 로 finished_ 잘못 신호 회피
        if (children_.empty()) { finished_ = true; return; }

        bool allDone = true;
        for (auto& c : children_)
        {
            if (!c->IsFinished())
            {
                if (auto* head = dynamic_cast<SJH::Scene::Component*>(c.get()))
                    if (head->IsEnabled()) head->Update(dt);
                if (!c->IsFinished()) allDone = false;
            }
        }
        if (allDone)
        {
            if (isLoop_)
            {
                for (auto& c : children_) c->Play();   // ← Loop=true 자동 재시작
            }
            else
            {
                finished_ = true;
            }
        }
    }
}
```

**dynamic_cast 가 거슬리는 이유와 trade-off** — children 은 `unique_ptr<IPlayable>` 이라 Component::Update 직접 호출 불가. Composite 가 *반드시 PlayableBase children 만 보유* 한다는 정책을 강제하면 `unique_ptr<PlayableBase>` 로 바꾸는 것도 가능 (dynamic_cast 제거). 본 spec 은 *유연성 우선* 으로 IPlayable 컨테이너 유지 + dynamic_cast (children 은 PlayableBase 상속이 사실상 보장됨 — IPlayable 다른 구현은 없음). 미래에 IPlayable 의 다른 구현이 생기면 재고.

### §3.4 `SpriteSequencePlayable` — leaf (sprite_sequence 모듈)

```cpp
// src/sprite/sprite_frame_clip.h
#ifndef __SJH_SPRITE_SEQUENCE_SPRITE_FRAME_CLIP_H__
#define __SJH_SPRITE_SEQUENCE_SPRITE_FRAME_CLIP_H__

namespace SJH::SpriteSequence
{
    /// @brief atlas frame index 시퀀스 정의 POD. loop 필드는 폐기 — PlayableBase.isLoop_ 가 흡수.
    struct SpriteFrameClip
    {
        int   startFrame;
        int   frameCount;
        float fps;
    };
}

#endif
```

```cpp
// src/sprite/sprite_sequence_playable.h
#ifndef __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__
#define __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__

#include "playable/playable_base.h"
#include "src/sprite/sprite_frame_clip.h"
#include "sprite/sprite_component.h"   // SJH::Sprite::SpriteRenderer (frameIdx 갱신 대상)

namespace SJH::SpriteSequence
{
    class SpriteSequencePlayable : public SJH::Playable::PlayableBase
    {
      public:
        SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                const SpriteFrameClip*       clip)
            : sprite_(spriteRef), clip_(clip) {}

      protected:
        void OnUpdate(float dt) override
        {
            if (!clip_ || !sprite_) return;
            // elapsed_ 는 PlayableBase 가 이미 += dt 처리
            const float frameDur = 1.0f / clip_->fps;
            int raw = int(elapsed_ / frameDur);
            if (isLoop_)
            {
                raw %= clip_->frameCount;       // ← Phase 2 결정: loop=true 자동 wrap
            }
            else if (raw >= clip_->frameCount)
            {
                raw = clip_->frameCount - 1;
                finished_ = true;                // ← non-loop 자연 종료
            }
            sprite_->frameIdx = clip_->startFrame + raw;
        }

      private:
        SJH::Sprite::SpriteRenderer* sprite_;
        const SpriteFrameClip*        clip_;
    };
}

#endif
```

---

## §4 사용 예 (Tweeny + DOTween 정통 fluent Builder)

### §4.1 단일 leaf — Actor 직접 부착

```cpp
// 사격 시 muzzle SFX 1회 — leaf 단독
auto* muzzle = bulletActor->AddComponent<myapp::FmodPlayable>("shot.wav");
muzzle->Play();
```

### §4.2 Sequence — fluent chain

```cpp
// 사망 시퀀스: 사운드 → death anim → despawn 신호
auto root = std::make_unique<SequencePlayable>();
(*root)
    .Append(std::make_unique<myapp::FmodPlayable>("death.wav"))
    .Append(std::make_unique<SpriteSequence::SpriteSequencePlayable>(spriteRef, &deathClip));
// (deathClip.frameCount 완료 시 IsFinished — Bullet 의 main loop 가 외부 despawn 결정)

auto* mounted = enemyActor->AddComponent(std::move(root));
mounted->Play();
```

### §4.3 계층 중첩 — Sequence 안에 Parallel 안에 leaf (3단)

```cpp
// 발사 시퀀스: muzzle effect → (소리 + 발사 anim 동시) → idle 복귀
auto par = std::make_unique<ParallelPlayable>();
(*par)
    .Join(std::make_unique<myapp::FmodPlayable>("gun_shot.wav"))
    .Join(std::make_unique<SpriteSequence::SpriteSequencePlayable>(spriteRef, &attackClip));

auto root = std::make_unique<SequencePlayable>();
(*root)
    .Append(std::make_unique<myapp::EffekseerPlayable>(muzzleEffect))   // ← leaf
    .Append(std::move(par))                                              // ← Composite (Parallel) 가 child
    .Append(std::make_unique<SpriteSequence::SpriteSequencePlayable>(spriteRef, &idleClip));

auto* mounted = playerActor->AddComponent(std::move(root));
mounted->SetIsLoop(false);
mounted->Play();
```

### §4.4 BGM — loop=true 일 때

```cpp
// Scene root Actor 에 글로벌 BGM
auto bgm = std::make_unique<myapp::FmodPlayable>("bgm.ogg");
auto* mounted = sceneRoot->AddComponent(std::move(bgm));
mounted->SetIsLoop(true);
mounted->Play();
// IsFinished() 절대 true 안 됨 — Scene 종료 시 mounted->Stop() 명시 호출
```

### §4.5 FSM 결합 — `SJH::FSM::StateMachine` 가 Playable 보유

spec §1.5 부록 B.18 의 *Playable + StateMachine 둘 다 부착 금지* 정책은 **재해석**:
- Actor 에 *root IPlayable Component 1개* + *StateMachine 1개* 부착 가능
- 단, 같은 sprite 갱신원 (SpriteRenderer.frameIdx) 이 두 곳에서 갈리지 않도록 의도적으로 사용
- 예: PlayerStateMachine 이 `unordered_map<State, unique_ptr<IPlayable>>` 보유 + 현재 state 의 Playable 만 `Update` (전이 시 이전 Stop, 다음 Play)

자세한 결합 패턴은 [`2026-05-25-fsm-object-state-machine-design.md`](2026-05-25-fsm-object-state-machine-design.md) 참조.

---

## §5 spec §1.5/§1.6 폐기/유지/수정 표

| spec 항목 | 처리 | 사유 |
|---|---|---|
| §1.5 `Playable` 베이스 (Play/Pause/Tick 3-method + paused_) | **수정** | `IPlayable` pure interface + `PlayableBase` abstract (Component 다중 상속), Stop 추가, Tick 폐기, IsFinished/IsLoop 인터페이스화 |
| §1.5 `SequencePlayable` / `ParallelPlayable` Composite | **유지+수정** | PlayableBase 상속 + children OnEnter/OnExit 수동 디스패치 추가 + Append/Insert/Join Builder 명문화 |
| §1.5 `PlayablePlayerComponent` | **폐기** | Component 시스템이 이미 동일 호스트 역할 (Actor::AddComponent + Actor::Update) |
| §1.5 `PlayableTickSystem` 자유함수 | **폐기** | `Actor::Update` / `Scene::Update` 가 이미 자동 디스패치 |
| §1.5 부록 B.18 "Playable+FSM 둘 다 부착 금지" | **재해석** | 둘 다 부착 OK — 단 같은 갱신원이 갈리지 않도록 사용자 책임. FSM 이 unordered_map<State, unique_ptr<Playable>> 보유 |
| §1.6 `SpriteFrameClip.loop` 필드 | **폐기** | `PlayableBase.isLoop_` 가 흡수 (인터페이스 승격) |
| §1.6 `SpriteSequencePlayable.elapsed_` 자기 보유 | **수정** | PlayableBase.elapsed_ 흡수, OnUpdate 에서 elapsed_/frameDur 만 사용 |
| §1.6 `SpriteSequencePlayable.finished_` 자기 보유 | **수정** | PlayableBase.finished_ 흡수, OnUpdate 에서 non-loop 종료 시 finished_=true |

---

## §6 _MyApp_ 마이그레이션 가이드

### §6.1 SpriteAnimator 폐기 (M3.5 정착 후)

`<src>/sprite/sprite_animator.h` 의 경량 자동 wrap 은 `SpriteSequencePlayable` 로 자연 대체. `_MyApp_` 의 PlayerActor 가 사용 중인 SpriteAnimator 직접 호출을 다음 패턴으로 치환:

```cpp
// (Before) SpriteAnimator 직접
spriteAnim_.Update(dt);
playerSprite_->frameIdx = spriteAnim_.GetCurrentFrame();

// (After) SpriteSequencePlayable Component
auto seq = std::make_unique<SpriteSequence::SpriteSequencePlayable>(playerSprite_, &idleClip);
auto* mounted = playerActor->AddComponent(std::move(seq));
mounted->SetIsLoop(true);
mounted->Play();
// 이후 Actor::Update 자동 호출 — main loop 의 spriteAnim_.Update / frameIdx 갱신 코드 제거
```

### §6.2 차기 Effekseer/FMOD leaf 도입 위치 (Client)

`game_deps` 의존 leaf 는 *Client 거주* — `<apps>/_MyApp_/src/Audio/fmod_playable.h` / `<apps>/_MyApp_/src/VFX/effekseer_playable.h`. `SJH::playable` 코어는 의존 0 (game_deps 비흡수) 유지.

---

## §7 미해결 / 후속

| 항목 | 처리 |
|---|---|
| Effekseer leaf `EffekseerPlayable` | M5 도입 — `apps/_MyApp_/src/VFX/` |
| FMOD leaf `FmodPlayable` (Core + Studio 분리) | M5 도입 — `apps/_MyApp_/src/Audio/`, Studio 는 `bank` 이벤트 wrap |
| DOTween `AppendInterval(seconds)` (지연 삽입) | 필요해지면 `IntervalPlayable` leaf 추가 (PlayableBase 상속, OnUpdate 가 elapsed_ ≥ duration 시 finished_=true) |
| Tweeny `from/to/via` 단일 tween 통합 | 별도 `TweenPlayable` leaf — Tweeny `tween<T>` 보유 + OnUpdate 에서 step. 현재 미도입 |
| 단위 테스트 | [[no_auto_tests]] — 사용자 명시 요청 시 |
| `SpriteAnimator` 실제 폐기 commit | M3.5 정착 후 별도 task — 본 spec 은 *예정* 만 명시 |

---

## §8 결정 회고 (사용자 결정 7건)

| Phase | 결정 | 사용자 답변 |
|---|---|---|
| 1 | Stop = Reset 후 재사용 가능 | C |
| 2 | GetIsLoop = 자동 재시작 (내부 처리) | B |
| 3 | Tick/IsFinished 위치 — Component 상속 + Tick 폐기 + PlayablePlayerComponent/TickSystem 폐기 | (자유 입력) |
| 4 | Composite = Component + 1 Actor 1 root + Stop 재귀 + Loop 자동 재시작 | A |
| 5 | 2-tier abstract (IPlayable pure + PlayableBase abstract base) | C |
| 4-bis | vector 명문화 + Append/Join fluent Builder + 계층 중첩 (Tweeny + DOTween 정통) | OK |
| 6 | design doc 작성 | OK |

**핵심 진화 vs spec §1.5/§1.6 원안**:
1. *4 엔진 정통* (Unity Playable / Unreal LevelSequencePlayer / Cocos Action / Godot AnimationPlayer) 에서 *Tweeny + DOTween 정통* 으로 빌더 모델 명확화
2. *호스트 컴포넌트 별도* (PlayablePlayerComponent) → *IPlayable 자체가 Component* (호스트 폐기)
3. *3-method (Play/Pause/Tick)* → *4-method Client 우선 (Play/Pause/Stop/GetIsLoop) + 2급 IsFinished + Component.Update 흡수*
4. *clip 의 loop 필드* → *IPlayable 인터페이스 GetIsLoop 승격*
