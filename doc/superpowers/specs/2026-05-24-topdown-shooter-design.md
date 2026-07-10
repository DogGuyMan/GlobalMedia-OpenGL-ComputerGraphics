# 2.5D 탑다운 슈터 설계 (본 저장소 정착판)

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 본 spec 은 [`doc/IMPLEMENTATION_GUIDE.md`](../../IMPLEMENTATION_GUIDE.md) 의 게임 비전 + 챕터별 설계를 **본 저장소 `game/main` 의 SJH 엔진** 정책에 맞게 *재서술* 한 단일 권위 문서다. 원본 가이드는 *2026-05-04 작성, `DogGuyMan/OpenGL-With-CMake` 베이스 가정* 으로 본 저장소와 19개 정책 충돌이 있으므로 그대로는 적용 불가 — 본 spec 이 코드 작업의 정본이며 원본은 비전/근거 출처로만 인용한다.

| 항목 | 값 |
|---|---|
| 작성 | 2026-05-24 |
| 베이스 | `game/main` 브랜치, SJH::engine 12 모듈, `apps/_MyApp_/` 클라이언트 |
| 대상 환경 | macOS + Windows (MSVC v142/v143), OpenGL **4.1 Core / GLSL 410** |
| 빌드 시스템 | CMake 3.14+, **`.gitmodules` + 사전 빌드 `lib/`·`include/` 체크인** (vcpkg/FetchContent 모두 미사용) |
| ECS | **자체 Actor + Component (비상속, Compound 패턴)** — EnTT 사용 금지 (user memory `entt_removed.md`) |
| 베이스 클래스 | **`sb7::application` 상속 필수** |
| 코드 변경 | 본 spec 자체는 0. M1 부터 코드 작성 시작 |

---

## 목차

- [§0 게임 컨셉 + 본 저장소 매핑](#0-게임-컨셉--본-저장소-매핑)
- [§1 빌드 시스템 (사전 빌드 lib + .gitmodules)](#1-빌드-시스템-사전-빌드-lib--gitmodules)
- [§2 렌더 — 빌보드 + Atlas (GLSL 410)](#2-렌더--빌보드--atlas-glsl-410)
- [§3 Actor + Component 카탈로그](#3-actor--component-카탈로그)
- [§4 물리 — Box2D v2.4.1](#4-물리--box2d-v241)
- [§5 파티클 — Effekseer 1.7.3.0](#5-파티클--effekseer-1730)
- [§6 애니메이션 + 두 FSM (Component)](#6-애니메이션--두-fsm-component)
- [§7 Tweeny 트위닝](#7-tweeny-트위닝)
- [§8 음향 — FMOD Core/Studio](#8-음향--fmod-corestudio)
- [§9 main.cpp 루프 + 시간 정책](#9-maincpp-루프--시간-정책)
- [§10 좌표계 + 렌더 패스 정책](#10-좌표계--렌더-패스-정책)
- [§11 카메라 + Input](#11-카메라--input)
- [§12 자원 보유 컨벤션 (SJH::resource_registry)](#12-자원-보유-컨벤션-sjhresource_registry)
- [부록 A — 디렉토리 구조 (`apps/_MyApp_/`)](#부록-a--디렉토리-구조-apps_myapp_)
- [부록 B — 함정 모음 (본 저장소 한정)](#부록-b--함정-모음-본-저장소-한정)
- [부록 C — 원본 가이드와의 번역 매트릭스](#부록-c--원본-가이드와의-번역-매트릭스)
- [부록 D — 마일스톤 분할 제안 (M1~M7)](#부록-d--마일스톤-분할-제안-m1m7)

---

## §0 게임 컨셉 + 본 저장소 매핑

### 0.1 게임 컨셉 (원본 가이드 §0.1 그대로)

- **장르**: 2.5D 탑다운 슈터, Cult of the Lamb 스타일 (3D 환경 + 빌보드 2D 스프라이트 캐릭터)
- **카메라**: 고정각 탑다운, 회전 불능, entity follow + Tweeny lerp + shake
- **캐릭터**: 빌보드 quad 에 frame-by-frame atlas 스프라이트
- **글로벌 게임 상태**: Startup → Combat → BossCombat → End (글로벌 FSM)
- **캐릭터 상태**: Idle / Walk / Attack / Hit / Die (per-entity FSM, Component)

### 0.2 본 저장소 정책 매핑 (확정 결정 19건)

| # | 항목 | 결정 |
|---|---|---|
| 1 | 빌드 | `.gitmodules` 서브모듈 + 사전 빌드 `lib/`·`include/` 체크인. FetchContent / vcpkg 모두 금지 |
| 2 | ECS | 자체 `Actor + Component` (`src/scene/`). EnTT 금지 (user memory `entt_removed.md`) |
| 3 | 베이스 클래스 | **`sb7::application` 상속** 필수. 데모 패턴 B (`main.cpp` only + `DECLARE_MAIN`) |
| 4 | 모듈 분할 | 저변동 코어 = `SJH::engine` 우산 (현 12 모듈 + 신규 `SJH::sprite` + `SJH::fsm` + `SJH::playable` + `SJH::sprite_sequence` = **16 모듈**, `SJH::pool` 은 M4 측정 후 결정). 고변동 클라이언트 = `apps/_MyApp_/` (재활성). `apps/_MyApp_/src/` STATIC 라이브러리 + 얇은 `main.cpp`. **재사용 가능한 도구 (sprite/fsm/playable/sprite_sequence 등) 는 SJH 코어로 승격하고, _MyApp_ 안에는 *게임 특화* 컴포넌트/시스템만 둔다** (2026-05-24 사용자 정정). Effekseer/Tweeny wrapper 및 FmodPlayable/EffekseerPlayable 등 *game_deps 흡수* leaf playable 은 Client 한정 |
| 5 | OpenGL | **GL 4.1 Core / GLSL 410** 강제 (user memory `glsl_410_project_policy.md`). 3.3 코드를 410 으로 마이그레이션 |
| 6 | Box2D | **v2.4.1** (`extern/box2d` 사전 빌드, `lib/{macos,windows}/`). 가이드의 v3 핸들 API (`b2BodyId`, `b2DefaultBodyDef`) 는 v2 포인터 API 로 번역 (§4) |
| 7 | Effekseer | **1.7.3.0** (`extern/Effekseer` + 사전 빌드 lib). 가이드 §5 코드 거의 그대로 |
| 8 | Tweeny | **`.gitmodules` extern + `<include>/tweeny.h` 헤더 체크인** (`tweeny` INTERFACE 타겟). v3.2.0 |
| 9 | FMOD | `game_deps` 자동 합류 (memory `fmod_game_deps_auto_join.md`), POST_BUILD copy 의무. `doc/FMOD_Setup.md` 참조 |
| 10 | 수학 | **vmath** (sb7 vendored). GLM 미체크인. `vmath::radians(int)` 함정 주의 (memory `vmath_radians_int_trap.md`) |
| 11 | GL 로더 | **gl3w** (sb7 vendored). glad 미체크인 |
| 12 | stb | **`stb_image` 만 사용**. `stb_rect_pack` 도입 **취소** (2026-05-24 정정 — PackedAtlas 폐기, 등간격 N×M 그리드만). `STB_IMAGE_IMPLEMENTATION` 정의 책임은 **`SJH::resource_registry`** 모듈 (`image.cpp`) 단일 위치 (2026-05-24 정정 — `SJH::sprite/uniform_atlas.cpp` 는 `SJH::Image::Load` + `SJH::Texture::CreateTexture` 위임으로 stb 직접 호출 0). 데모 main.cpp 는 정의 안 함 |
| 13 | ResourceManager | **`SJH::resource_registry` 재활용**. 신규 클래스 만들지 않음 (§12) |
| 14 | 셰이더 핫리로드 | **구현 안 함**. `SJH::shader::Shader::CreateFromSource` 그대로 사용 |
| 15 | spdlog / Assimp | 그대로 사용 (`game_deps` IMPORTED) |
| 16 | Client 위치 | `apps/_MyApp_/` 재활성 + 하위 STATIC `myapp_core` |
| 17 | Update 루프 호스트 | `main.cpp` 의 `render(double t)` 안에서 직접 서술 |
| 18 | Box2D 컴포넌트 거주 | **Client 쪽에만** (`apps/_MyApp_/src/physics/`). `SJH::engine` 은 box2d 의존을 흡수하지 않음 (저변동 원칙 보호 — box2d 는 한 가지 라이브러리 선택의 결과라 코어에 끼면 다른 물리 백엔드로 못 바꿈) |
| 19 | 작업 분할 | M0 (본 spec 작성) → 사용자 승인 → M1~M7 단계별 (부록 D) |

### 0.3 비기능 요구사항 (NFR)

| 항목 | 결정 | 근거 |
|---|---|---|
| 결합도 | 코어 ↔ 클라이언트는 *단방향* — `apps/_MyApp_/` 만 `SJH::engine` + `game_deps` 의존. 코어는 game 라이브러리 모름 | CLAUDE.md §11 재사용성 |
| 응집도 | 한 헤더/cpp 한 책임. Component 한 종류 한 헤더 | EngineAPI.md §1 |
| 컨벤션 | PascalCase 클래스, camelCase 멤버, 한국어 주석, `#ifndef __NAME_H__` (`#pragma once` 금지). `.h` 헤더 | CLAUDE.md "Conventions" |
| 헤더 책임 | Component 는 POD 또는 매우 얇은 클래스. 로직은 System 자유 함수 | EngineAPI.md §3.9 |
| 자원 owner | `SJH::resource_registry` 가 Texture/Material/Model 캐싱. Program/Mesh 는 데모 임시 owner | EngineAPI.md §11.3 |

---

## §1 빌드 시스템 (사전 빌드 lib + .gitmodules)

### 1.1 정책 (원본 §1 FetchContent 전면 거부)

| 라이브러리 | 통합 | 본 저장소 위치 |
|---|---|---|
| sb7 / glfw3 | 사전 빌드 STATIC IMPORTED (`_d` 접미사 Debug) | `lib/{macos,windows}/` + `include/` |
| Box2D v2.4.1 | 사전 빌드 STATIC IMPORTED | `lib/{macos,windows}/libbox2d*.a` + `include/box2d/` |
| Effekseer 1.7.3.0 | 사전 빌드 STATIC IMPORTED (Effekseer + EffekseerRendererGL) | `lib/...` + `include/Effekseer/` |
| assimp / spdlog | 사전 빌드 STATIC IMPORTED | `lib/...` |
| Tweeny v3.2.0 | 헤더 체크인 INTERFACE | `<include>/tweeny.h` (또는 dir) |
| stb_image | 헤더 체크인 INTERFACE (`stb_extra` 타겟) | `<include>/stb_image.h` |
| FMOD Core/Studio | SHARED IMPORTED, `game_deps` 조건부 합류 | `include/fmod/*.h` 가드 |
| 모든 서브모듈 추적 | `.gitmodules` 등록 | 이미 9종 등록 완료 (sb7code/box2d/Effekseer/tweeny/stb/assimp/spdlog/imgui/Catch2) |

### 1.2 신규 외부 라이브러리 작업 — 없음 (2026-05-24 정정)

기존 `extern/` 서브모듈 + `lib/`·`include/` 사전 빌드 산출물로 모든 요구 라이브러리 충족.
- ~~`stb_rect_pack.h` 추가~~ — **취소** (PackedAtlas 폐기, §1.4 / §2.4 참조)
- 기타 라이브러리 추가/업데이트 0건

### 1.3 활성화 절차

```bash
# CLAUDE.md "Active Target Management" 컨벤션 — 한 줄 주석 해제
# apps/CMakeLists.txt:
#   add_subdirectory(_MyApp_)   ← 주석 해제

cmake --preset ninja
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_   # 리소스 상대경로 — cd 필수
```

### 1.4 신규 코어 모듈 — `SJH::sprite` (2026-05-24 결정, 2026-05-24 정정)

등간격 atlas + 정적 sprite 데이터만. **Animation 관련 (Clip / StateComponent / System) 은 분리되어 신규 모듈 `SJH::sprite_sequence` (§1.6) 으로 이동**. **PackedAtlas / stb_rect_pack 도입 취소** — 등간격 N×M 그리드만.

```
src/sprite/
├─ CMakeLists.txt          # add_library(sjh_sprite STATIC ...) + ALIAS SJH::sprite
├─ uniform_atlas.h/.cpp    # 등간격 N×M 정사각 그리드 — SJH::Image/SJH::Texture 위임 (stb 직접 사용 0)
└─ sprite_component.h      # SpriteComponent (atlas*, frameIdx, size, tint, flipX) — 게임 무관 POD
```

**의존**:
- PUBLIC: `SJH::scene` (Component 베이스), `SJH::resource_registry` (Texture owner)
- PUBLIC: `stb_extra` (stb_image 헤더만)
- PRIVATE: `spdlog` (로깅), `project_deps` (GL/vmath)
- **game_deps 의존 0**

**SpriteComponent 의 정체성** — *어떤 게임에서도 동일한* atlas 인덱스 / size / tint / flipX 만 보유. *시간 축 갱신 (frameIdx 가 매 프레임 변하는 것) 은 SJH::sprite 의 책임이 아님* — SpriteSequencePlayable (§1.6) 이 frameIdx 를 갱신. 게임 특화 필드 (damage, owner_actor) 는 Client 별도 Component.

### 1.5 신규 코어 모듈 — `SJH::playable` (2026-05-24 결정, Composite 패턴)

Unity Playable API 정통의 *시간 축 추상화* — 단 **Mixer/Blending 없음** (사용자 결정). GoF Composite 패턴으로 Sequence (순차) / Parallel (병렬) 노드 조합 가능. tweeny 의 `.then() / .join()` 패턴 정통.

```
src/playable/
├─ CMakeLists.txt              # add_library(sjh_playable STATIC ...) + ALIAS SJH::playable
├─ playable.h                  # Playable 추상 베이스 — Play/Pause/Tick + IsFinished
├─ composite_playable.h        # SequencePlayable / ParallelPlayable (children 보유, 재귀 Tick)
├─ playable_player_component.h # PlayablePlayerComponent — Actor 에 부착, 루트 Playable 보유 + Tick 진입점
└─ playable_tick_system.h/.cpp # 자유함수 — Scene 의 모든 PlayablePlayerComponent 매 프레임 Tick
```

**핵심 추상화**:

```cpp
// <src>/playable/playable.h
namespace SJH::Playable {

class Playable {
public:
    virtual ~Playable() = default;

    // === lifecycle (사용자 결정 — Play/Pause/Tick 3-method) ===
    virtual void Play()  { paused_ = false; }   // 재생 시작 또는 일시정지 후 재개
    virtual void Pause() { paused_ = true; }    // 일시정지 — Play 재호출로 재개 (Stop 없음, 상태 보존)
    virtual void Tick(float dtSeconds) {
        if (paused_ || IsFinished()) return;
        OnTick(dtSeconds);
    }

    // === 종료 신호 (사용자 결정 — IsFinished 별도 메서드) ===
    virtual bool IsFinished() const = 0;

protected:
    virtual void OnTick(float /*dt*/) {}        // 파생 클래스 override — 실제 시간 진행
    bool paused_ = false;
};

}  // namespace SJH::Playable
```

```cpp
// src/playable/composite_playable.h
namespace SJH::Playable {

// 순차 — children 한 번에 1개씩, 현재 child 가 IsFinished 면 다음 child Play
class SequencePlayable : public Playable {
public:
    SequencePlayable& Then(std::unique_ptr<Playable> child) {
        children_.push_back(std::move(child));
        return *this;
    }
    void Play() override {
        Playable::Play();
        cursor_ = 0;
        if (!children_.empty()) children_[0]->Play();
    }
    void Pause() override {
        Playable::Pause();
        if (cursor_ < children_.size()) children_[cursor_]->Pause();
    }
    bool IsFinished() const override { return cursor_ >= children_.size(); }
protected:
    void OnTick(float dt) override {
        if (cursor_ >= children_.size()) return;
        children_[cursor_]->Tick(dt);
        if (children_[cursor_]->IsFinished()) {
            ++cursor_;
            if (cursor_ < children_.size()) children_[cursor_]->Play();
        }
    }
private:
    std::vector<std::unique_ptr<Playable>> children_;
    size_t cursor_ = 0;
};

// 병렬 — children 모두 동시 Play, 모두 IsFinished 일 때 자기 IsFinished
class ParallelPlayable : public Playable {
public:
    ParallelPlayable& Join(std::unique_ptr<Playable> child) {
        children_.push_back(std::move(child));
        return *this;
    }
    void Play() override {
        Playable::Play();
        for (auto& c : children_) c->Play();
    }
    void Pause() override {
        Playable::Pause();
        for (auto& c : children_) c->Pause();
    }
    bool IsFinished() const override {
        for (const auto& c : children_) if (!c->IsFinished()) return false;
        return true;
    }
protected:
    void OnTick(float dt) override {
        for (auto& c : children_) if (!c->IsFinished()) c->Tick(dt);
    }
private:
    std::vector<std::unique_ptr<Playable>> children_;
};

}  // namespace SJH::Playable
```

```cpp
// <src>/playable/playable_player_component.h — Actor 에 부착하는 진입점
namespace SJH::Playable {
class PlayablePlayerComponent : public SJH::scene::Component {
public:
    void SetRoot(std::unique_ptr<Playable> root) { root_ = std::move(root); }
    void Play()  { if (root_) root_->Play(); }
    void Pause() { if (root_) root_->Pause(); }
    void Tick(float dt) { if (root_) root_->Tick(dt); }
    bool IsFinished() const { return !root_ || root_->IsFinished(); }
    Playable* Root() { return root_.get(); }
private:
    std::unique_ptr<Playable> root_;
};
}
```

**의존**:
- PUBLIC: `SJH::scene` (Component 베이스), `SJH::common`
- **game_deps 의존 0** (FMOD/Effekseer 흡수 안 함) — leaf playable 중 game_deps 의존인 것들은 Client 거주

**Builder 패턴 — C++ immediate-mode 시퀀싱 (결정 3 옵션 ①)**:

```cpp
// 게임 코드 — 한 발사 시퀀스 (상태 없는 Actor — Bullet/Effect 같은 단발성)
auto seq = std::make_unique<SequencePlayable>();
seq->Then(std::make_unique<myapp::EffekseerPlayable>(muzzleEffect_))
   .Then(std::make_unique<ParallelPlayable>()
            ->Join(std::make_unique<myapp::FmodPlayable>("gun_shot.wav"))
            ->Join(std::make_unique<SJH::SpriteSequence::SpriteSequencePlayable>(spriteRef, attackClip)));

bulletActor->Get<PlayablePlayerComponent>().SetRoot(std::move(seq));
bulletActor->Get<PlayablePlayerComponent>().Play();
```

**PlayablePlayerComponent 의 사용 컨벤션 (2026-05-24 정정)**:

| Actor 종류 | Playable 호스트 | 이유 |
|---|---|---|
| 상태 없는 단발성 — Bullet / Muzzle Effect / 단일 시퀀스 SFX | `PlayablePlayerComponent` (코어, 단일 root) | 1개 root + 끝나면 Actor 자체 despawn — 자연스러움 |
| BGM (loop root) | Scene root Actor 의 `PlayablePlayerComponent` | 글로벌 무한 loop — 1개 root |
| 상태 기반 캐릭터 — Player / Enemy (Idle/Move/Attack/Die 다중 Playable) | **`SJH::FSM::StateMachineProcessor` 파생 클래스 자체가 `unordered_map<TState, unique_ptr<Playable>>` 컨테이너 보유** (§6.3) | state ↔ Playable 결합이 클래스 안에 캡슐화. PlayablePlayerComponent 미사용 |

→ 한 Actor 가 **PlayablePlayerComponent 와 StateMachine 중 하나만** 부착 (둘 다는 충돌 — 같은 sprite 갱신원 2개 위험). 부록 B.18 함정 참조.

### 1.6 신규 코어 모듈 — `SJH::sprite_sequence` (2026-05-24 결정 + 정정, 결정 1 옵션 C)

`SJH::sprite` + `SJH::playable` 양쪽 의존하는 분리 모듈. **SpriteSequencePlayable** 의 거주지 — *sprite atlas 의 frame indices 시퀀스를 시간 기반으로 진행하는 Playable*. (옛 명칭 `SJH::sprite_sequence` / `SpriteSequencePlayable` 폐기 — "animation" 이 모호해 *sprite frame sequence* 라는 정확한 명칭으로 정정.)

```
src/sprite_sequence/
├─ CMakeLists.txt                # add_library(sjh_sprite_sequence STATIC ...) + ALIAS SJH::sprite_sequence
├─ sprite_frame_clip.h           # SpriteFrameClip POD (startFrame, frameCount, fps, loop)
└─ sprite_sequence_playable.h/.cpp   # SpriteSequencePlayable — SJH::Playable::Playable 상속
                                 # ※ AnimationStateComponent 폐기 — Playable 자체가 elapsed/finished 보유 (단일 진실)
```

```cpp
// src/sprite/sprite_frame_clip.h
namespace SJH::SpriteSequence {
struct SpriteFrameClip {
    int   startFrame;        // atlas 의 시작 frame index
    int   frameCount;        // 이 clip 의 frame 수
    float fps;
    bool  loop;
    // ※ soundEventFrame / soundId 폐기 — sound 트리거는 Composite Playable (Sequence/Parallel) 로 표현
};
}
```

```cpp
// src/sprite/sprite_sequence_playable.h
namespace SJH::SpriteSequence {

class SpriteSequencePlayable : public SJH::Playable::Playable {
public:
    // binding 주입 — 어느 Sprite Component 의 frameIdx 를 갱신할지 + 어느 clip
    SpriteSequencePlayable(SJH::Sprite::SpriteComponent* spriteRef,
                            const SpriteFrameClip* clip)
        : sprite_(spriteRef), clip_(clip) {}

    void Play() override {
        Playable::Play();
        elapsed_ = 0.0f;
        finished_ = false;
    }
    bool IsFinished() const override { return finished_; }

protected:
    void OnTick(float dt) override {
        if (!clip_ || !sprite_) return;
        elapsed_ += dt;
        float frameDur = 1.0f / clip_->fps;
        int raw = int(elapsed_ / frameDur);
        if (clip_->loop) {
            raw %= clip_->frameCount;
        } else if (raw >= clip_->frameCount) {
            raw = clip_->frameCount - 1;
            finished_ = true;     // ← non-loop clip 자연 종료. loop clip 은 절대 finished 안 됨
        }
        sprite_->frameIdx = clip_->startFrame + raw;
    }

private:
    SJH::Sprite::SpriteComponent* sprite_;
    const SpriteFrameClip*        clip_;
    float elapsed_ = 0.0f;          // ← 진행 상태의 *유일한* 진실 (AnimationStateComponent 폐기)
    bool  finished_ = false;
};

}  // namespace SJH::SpriteSequence
```

**의존**:
- PUBLIC: `SJH::sprite` (SpriteComponent), `SJH::playable` (Playable 베이스)
- **game_deps 의존 0** — sound 트리거 책임 폐기 (시퀀스로 대체)

### 1.7 `apps/_MyApp_/CMakeLists.txt` 패턴

```cmake
# 패턴: 하위 STATIC + 얇은 entry main.cpp
get_filename_component(DEMO_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# 1) Client 코어 STATIC (myapp_core)
add_subdirectory(src)   # apps/_MyApp_/src/CMakeLists.txt 가 myapp_core ALIAS 노출

# 2) 얇은 entry
add_executable(${DEMO_NAME} main.cpp)
target_link_libraries(${DEMO_NAME} PRIVATE
    project_deps     # sb7 + glfw3 + OpenGL + 플랫폼 프레임워크
    game_deps        # box2d + Effekseer + assimp + spdlog + tweeny + stb_extra + fmod
    SJH::engine      # 12 코어 모듈 우산
    myapp_core       # Client (game/physics/animation/tween/...)
)

# 3) 리소스 복사 (POST_BUILD)
add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:${DEMO_NAME}>/resources)

# 4) FMOD DLL/dylib 복사 (game_deps 챕터 의무)
add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:fmod> $<TARGET_FILE_DIR:${DEMO_NAME}>)
if(TARGET fmodstudio)
    add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmodstudio> $<TARGET_FILE_DIR:${DEMO_NAME}>)
endif()
```

### 1.8 `apps/_MyApp_/src/CMakeLists.txt` 패턴

```cmake
add_library(myapp_core STATIC
    <game>/components.cpp       # 게임 특화 컴포넌트만 (Health, Bullet, Enemy, Player, ...)
    <game>/game_fsm.cpp         # 글로벌 FSM (게임 특화 상태) — SJH::FSM 상속
    <game>/character_fsm.cpp    # per-entity FSM (게임 특화 상태) — SJH::FSM 상속
    <game>/fsm_tick_system.cpp  # Scene 의 모든 FSM Component 매 프레임 Tick
    <physics>/physics_system.cpp
    <physics>/physics_body.cpp  # Box2D b2Body* 보유 Component (Client 한정 — 결정 #18)
    <physics>/contact_listener.cpp
    <audio>/audio_system.cpp    # FMOD Core C API 래퍼
    <audio>/fmod_playable.cpp   # SJH::Playable::Playable 상속 — game_deps 흡수
    <particle>/particle_system.cpp        # Effekseer manager+renderer 래퍼
    <particle>/effekseer_playable.cpp     # SJH::Playable::Playable 상속 — game_deps 흡수
    <tween>/tween_components.cpp          # TweenScale/TweenTint/TweenPosOffsetComponent (Tweeny 보유)
    <tween>/tween_system.cpp              # step(int32_t) 강제
    <render>/billboard_pass.cpp
    <render>/render_system.cpp
    <input>/input_actions.cpp
    <camera>/topdown_camera.cpp
    <effects>/shake_system.cpp
)
# 주의: 다음은 SJH 코어로 승격되어 여기 없음 (2026-05-24):
#   - src/sprite/uniform_atlas.cpp → SJH::sprite
#   - SpriteSequencePlayable / SpriteFrameClip → SJH::sprite_sequence
#   - Playable 베이스 / SequencePlayable / ParallelPlayable / PlayablePlayerComponent → SJH::playable
#   - 비트 enum StateMachineProcessor → SJH::fsm
# Client 한정 leaf playable (FmodPlayable, EffekseerPlayable) + Client 한정 StateMachine 파생 (myapp::CharacterFSM,
#   myapp::GameFSM) 이 여기 거주 — game_deps 흡수 위치 + game 특화 enum 보유.
target_include_directories(myapp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(myapp_core PUBLIC
    project_deps game_deps SJH::engine)
# SJH::engine 우산이 SJH::sprite / SJH::fsm / SJH::playable / SJH::sprite_sequence 모두 포함 → 자동 가용
```

---

## §2 렌더 — 빌보드 + Atlas (GLSL 410)

원본 §2 의 셰이더와 SpriteAtlas 설계는 **본질적으로 그대로 사용 가능**. 단 GLSL 버전만 330 → 410, GL 컨텍스트는 `init()` override 에서 4.1 명시.

### 2.1 GL 컨텍스트 강제 (CLAUDE.md / memory `glsl_410_project_policy.md`)

```cpp
// apps/_MyApp_/main.cpp — sb7::application::init() override
void init() override {
    sb7::application::init();
    info.majorVersion = 4;
    info.minorVersion = 1;
    info.flags.forwardCompat = 1;   // macOS 필수
    info.flags.debug = 1;           // KHR_debug 콜백 활성 (windows/linux)
    info.flags.coreProfile = 1;
    static const char title[] = "Topdown Shooter";
    memcpy(info.title, title, sizeof(title));
}
```

### 2.2 빌보드 + Atlas 결합 셰이더 (GLSL 410)

```glsl
// <resources>/shaders/billboard_atlas.vert
#version 410 core
layout(location = 0) in vec2 a_quad;   // (-0.5,-0.5) ~ (0.5,0.5)
layout(location = 1) in vec2 a_uv;     // (0,0) ~ (1,1)

uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec3 u_billboardCenter;        // world position
uniform vec2 u_billboardSize;          // (width, height)
uniform vec4 u_uvRect;                 // (uMin, vMin, uSize, vSize) — frame_idx → UV
uniform float u_flipX;                 // +1.0 or -1.0

out vec2 v_uv;

void main() {
    // View 행렬에서 카메라 right 벡터 추출 (Y축 고정 cylindrical billboard)
    vec3 cameraRight = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 cameraUp    = vec3(0.0, 1.0, 0.0);

    vec3 worldPos = u_billboardCenter
                  + cameraRight * a_quad.x * u_billboardSize.x * u_flipX
                  + cameraUp    * a_quad.y * u_billboardSize.y;

    v_uv = u_uvRect.xy + a_uv * u_uvRect.zw;
    gl_Position = u_proj * u_view * vec4(worldPos, 1.0);
}
```

```glsl
// <resources>/shaders/billboard_atlas.frag
#version 410 core
in vec2 v_uv;
uniform sampler2D u_atlas;
uniform vec4 u_tint;
out vec4 fragColor;

void main() {
    vec4 c = texture(u_atlas, v_uv);
    if (c.a < 0.01) discard;          // alpha-test (transparency sort 회피)
    fragColor = c * u_tint;
}
```

### 2.3 vmath ↔ 셰이더 uniform 송신

가이드의 GLM 코드는 vmath 로 번역:

| 가이드 (GLM) | 본 spec (vmath) |
|---|---|
| `glm::vec2 / vec3 / vec4 / mat4` | `vmath::vec2 / vec3 / vec4 / mat4` |
| `glm::value_ptr(m)` | `m[0]` 또는 cast (vmath 는 row/col 정렬이 GLM 과 다를 수 있음 — 셰이더 transpose 옵션으로 흡수) |
| `glm::lookAt / perspective` | `vmath::lookat / perspective` |
| `glm::radians(45.0f)` | `vmath::radians(45.0f)` ← **반드시 `.0f`**. `vmath::radians(45)` 는 T=int 추론으로 0 (memory `vmath_radians_int_trap.md`) |

Uniform 송신은 두 layer 컨벤션 (EngineAPI.md §4.5) 준수:

- `u_uvRect` / `u_tint` 등 Material 자기 데이터 → `Uniforms::Set*(Material&, ...)` (properties bag store)
- `u_view` / `u_proj` / `u_billboardCenter` / `u_billboardSize` / `u_flipX` → `Uniforms::Set*(const Program&, ...)` (즉시 `glUniform*`). 매 draw call 마다 다른 transient 값

### 2.4 SpriteAtlas — `SJH::sprite::UniformAtlas` 만 (등간격 N×M, 2026-05-24 정정)

원본 §2.4 의 *UniformAtlas 만* `SJH::sprite` 코어 모듈에 둔다. **PackedAtlas / stb_rect_pack 도입 폐기** — 가변 크기 atlas 요구가 발생하면 atlas 를 *복수 파일* 로 분리해 해결 (예: `player_atlas.png` 64×64 / `boss_atlas.png` 128×128 별도). Texture 는 `SJH::resource_registry` 위탁.

```cpp
// src/sprite/uniform_atlas.h
#ifndef __SJH_SPRITE_UNIFORM_ATLAS_H__
#define __SJH_SPRITE_UNIFORM_ATLAS_H__

#include <vmath.h>
#include <GL/gl3w.h>
#include "resource_registry/resource_registry.h"   // SJH::ResourceRegistry

namespace SJH::Sprite {

struct UniformAtlas {
    GLuint textureId  = 0;
    int    atlasWidth = 0;
    int    atlasHeight = 0;
    int    tileSize   = 64;
    int    cols       = 0;
    int    rows       = 0;

    bool LoadFromPNG(const char* path, int tilePx);
    void Release();

    vmath::vec4 GetUVRect(int frameIdx) const {
        int col = frameIdx % cols;
        int row = frameIdx / cols;
        float u  = float(col * tileSize) / float(atlasWidth);
        float v  = float(row * tileSize) / float(atlasHeight);
        float du = float(tileSize) / float(atlasWidth);
        float dv = float(tileSize) / float(atlasHeight);
        return vmath::vec4(u, v, du, dv);
    }

    int FrameCount() const { return cols * rows; }
};

}  // namespace SJH::Sprite
#endif
```

```cpp
// src/sprite/uniform_atlas.cpp  (SJH::sprite 모듈 내부 — 2026-05-24 Task 4 fixup 후 SJH::Image/Texture 위임)
#include "uniform_atlas.h"
#include "src/texture/image.h"           // SJH::Image::Load (stbi 흡수)
#include <<spdlog>/spdlog.h>

bool SJH::Sprite::UniformAtlas::LoadFromPNG(const char* path, int tilePx) {
    auto image = SJH::Image::Load(/*image_name=*/path, /*filepath=*/path);   // stbi 위임
    int w, h, channels;
    unsigned char* pixels = stbi_load(path, &w, &h, &channels, 4);
    if (!pixels) { spdlog::error("[UniformAtlas] {}", path); return false; }
    atlasWidth  = w; atlasHeight = h; tileSize = tilePx;
    cols = w / tilePx; rows = h / tilePx;

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(pixels);
    return true;
}
```

~~PackedAtlas 도 `src/sprite/packed_atlas.{h,cpp}` 에 동거주~~ — **2026-05-24 정정으로 폐기**. 가이드 §2.4.3 PackedAtlas / §2.4.4 선택 가이드 / stb_rect_pack 의존 모두 본 spec 에서 제외. *가변 크기 sprite 가 필요해지면* 별도 atlas 파일로 분리 (예: 64×64 일반 atlas + 128×128 보스 atlas) — 두 UniformAtlas 인스턴스로 처리.

### 2.5 데이터 흐름 (원본 §2.5 그대로)

```
Sprite Component (atlasId, frameIdx, size, tint, flipX)
        │
        │ SpriteSequencePlayable (시간 → frameIdx) — PlayablePlayer 또는 StateMachine container
        │ TweenSystem (tint, size 보간)
        │ AISystem (flipX 좌우)
        ▼
RenderSystem (BillboardPass)
        │ Atlas::GetUVRect(frameIdx) → u_uvRect
        │ Material 의 u_tint store, transient u_view/u_proj/center/size/flipX 즉시 송신
        ▼
glDrawArrays(GL_TRIANGLES, 0, 6)
```

---

## §3 Actor + Component 카탈로그

### 3.1 EnTT → SJH::scene 번역 원칙

가이드 §3 의 모든 `struct XxxComponent {...};` 는 **SJH::scene 의 Component 베이스 파생 클래스** 로 옮긴다 (EngineAPI.md §3.9). Actor 는 비상속 — 특수 속성은 오직 Component 부착으로만 부여. Compound Actor 컨벤션 (free factory in `src/scene/compound_actor.h`) 따라 자주 쓰이는 조합은 `<apps>/_MyApp_/src/game/compound_factories.h` 에 free function 으로.

| 가이드 (EnTT) | 본 spec (SJH) |
|---|---|
| `struct Transform { glm::vec3 position; ... };` | 이미 존재 — `SJH::object::Transform` 또는 `SJH::scene::TransformComponent` (현 구현 따라). 신규 정의 금지 |
| `struct B2Body { b2BodyId id; };` | **Client 한정** `myapp::PhysicsBody { b2Body* body; }` Component (결정 #18). `b2Body*` (v2 포인터 API) |
| `struct Sprite {...}` | **`SJH::Sprite::SpriteComponent`** (코어 — atlas 핸들 + frameIdx + size + tint + flipX, *시간 갱신 책임 없음 — frameIdx 만 보유*) |
| `struct Billboard { float heightOffset; };` | `myapp::BillboardComponent` (게임 카메라 정책 — 코어 아님) |
| `struct AnimationState {...}` | **삭제** (2026-05-24 정정) — 진행 상태는 `SpriteSequencePlayable` 자체의 elapsed/finished 가 단일 진실. 별도 Component 불필요 |
| *(신규)* PlayablePlayerComponent | **`SJH::Playable::PlayablePlayerComponent`** (코어 — 단일 root Playable 진입점, Bullet/Effect/BGM 등 *상태 없는* Actor 용) |
| *(신규)* StateMachine 파생 클래스 | Client 의 `myapp::PlayerStateMachine` / `myapp::EnemyStateMachine` 등 — `SJH::FSM::StateMachineProcessor` 상속 + 자체 `unordered_map<TState, unique_ptr<Playable>>` 보유 (상태 기반 Actor 용 — §6.3) |
| `struct TweenScale / TweenTint / TweenPosOffset` | `myapp::TweenScaleComponent` 등 — Tweeny 멤버 보유 (게임 트윈 정책) |
| `struct Health { int current; int max; };` | `myapp::HealthComponent` |
| `struct Bullet { float ttl; int damage; entt::entity owner; };` | `myapp::BulletComponent { float ttl; int damage; SJH::scene::Actor* owner; }` |
| `struct Enemy / Player` | tag 컴포넌트 — `myapp::EnemyComponent / PlayerComponent` |
| `struct SoundSource { FMOD::Sound* sound; FMOD::Channel* channel; };` | `myapp::SoundSourceComponent` |
| `struct Dead {}` | tag — `myapp::DeadComponent` |
| `struct DespawnAfter { float ttl; };` | `myapp::DespawnAfterComponent` |
| `struct CharacterStateTag { State state; };` | `myapp::CharacterStateComponent` (FSM 본체는 §6) |

### 3.2 시스템 자유함수 컨벤션

EngineAPI.md §3.10 의 SceneRenderer 가 Render 책임을 이미 가지므로, *Update 계열 System* 은 **`apps/_MyApp_/src/<domain>/<domain>_system.h` 의 자유함수** 로 둔다. Object-oriented `System` 클래스 강제 안 함 — 함수 1개로 충분한 책임은 함수 1개.

```cpp
// 예: <apps>/_MyApp_/src/game/fsm_tick_system.h
namespace myapp::FSMTickSystem {
    // Scene 의 모든 StateMachine 파생 Component (PlayerStateMachine, EnemyStateMachine, ...) Tick
    void Update(SJH::scene::Scene& scene, float dtSeconds);
}
```

### 3.3 Compound Factory 패턴 — 자유함수 영구 채택 (2026-05-24)

자주 쓰이는 Actor 조합은 자유 함수. **`Actor::Clone()` 또는 `Scene::Instantiate(prefab)` 같은 prefab 패턴은 도입 안 함** (사용자 결정) — Unity 의 Prefab 개념이 본 게임 규모에서는 *과추상*. factory 자유함수가 곧 *spawn 진입점* 이라 후속 풀 도입 시 함수 내부만 변경하면 호출처 영향 0.

```cpp
// <apps>/_MyApp_/src/game/compound_factories.h
namespace myapp::factories {
    SJH::scene::Actor* CreatePlayer(SJH::scene::Scene& scene, vmath::vec2 pos);
    SJH::scene::Actor* CreateEnemy(SJH::scene::Scene& scene, vmath::vec2 pos, EnemyKind kind);
    SJH::scene::Actor* CreateBullet(SJH::scene::Scene& scene,
                                    vmath::vec2 pos, vmath::vec2 dir,
                                    SJH::scene::Actor* owner);
    SJH::scene::Actor* CreatePickup(SJH::scene::Scene& scene, vmath::vec2 pos, PickupKind kind);
}
```

**ObjectPool 정책 (2026-05-24 결정)**:
- **M1~M3**: 풀 없이 `scene.CreateActor() + Add<Component>()` 깡으로 spawn/destroy. *"비효율적이라도 spawn 코드부터 완벽 작동 보장"* 우선
- **M4**: 발사 + 적 wave 구현 후 *Bullet spawn cost 측정*. 측정 도구는 `SJH::diagnostics` 의 timing 기능 활용 (또는 chrono 직접). 임계값 = *Bullet spawn 비용 > 0.5ms/프레임* 시 풀 도입 결정
- **풀 도입 시**: factory 자유함수 *내부만* 변경 (`new Bullet` → `pool.Acquire()`). 호출처 (`CreateBullet(...)`) 시그니처 동일 → 모든 사용처 영향 0
- **풀 자체**: M4 측정 결과 따라 (A) `SJH::pool` 코어 모듈 신설 또는 (B) Client 한정 `myapp::BulletPool` 단일 구현. 부록 D M4 마일스톤 회고 항목

EnTT registry 의 sparse-set 자동 풀링은 본 저장소에 없음 — 위 정책으로 보완.

---

## §4 물리 — Box2D v2.4.1

가이드 §4 의 **모든 v3 API 호출은 v2.4.1 로 번역 필수**. v3 핸들(`b2BodyId` opaque uint64) ↔ v2 포인터(`b2Body*`) 가 가장 큰 차이.

### 4.1 v3 → v2.4.1 API 번역표

| 가이드 v3.x | 본 spec v2.4.1 |
|---|---|
| `b2WorldDef wd = b2DefaultWorldDef(); wd.gravity = (b2Vec2){0,0}; b2WorldId world = b2CreateWorld(&wd);` | `b2World world(b2Vec2(0.0f, 0.0f));` (스택/heap 자유) |
| `b2World_Step(world, dt, 4);` | `world.Step(dt, velocityIterations=8, positionIterations=3);` |
| `b2DestroyWorld(world);` | 소멸자 자동 |
| `b2BodyDef bd = b2DefaultBodyDef(); bd.type = b2_dynamicBody; b2BodyId body = b2CreateBody(world, &bd);` | `b2BodyDef bd; bd.type = b2_dynamicBody; b2Body* body = world.CreateBody(&bd);` |
| `b2Circle c{ {0,0}, 0.5f }; b2ShapeDef sd = b2DefaultShapeDef(); sd.density = 1.0f; b2CreateCircleShape(body, &sd, &c);` | `b2CircleShape c; c.m_radius = 0.5f; b2FixtureDef fd; fd.shape = &c; fd.density = 1.0f; body->CreateFixture(&fd);` — **v2 는 Fixture 가 살아있다** (v3 가 제거한 것) |
| `sd.filter.categoryBits / maskBits` | `fd.filter.categoryBits / maskBits` (필드는 동일) |
| `sd.isSensor = true;` | `fd.isSensor = true;` |
| `b2Body_GetPosition(body); b2Body_GetRotation(body);` | `body->GetPosition(); body->GetAngle();` |
| `b2Rot_GetAngle(r)` | `body->GetAngle()` 가 이미 radians 반환 — 변환 불필요 |
| `bd.isBullet = true;` | `bd.bullet = true;` |
| `b2World_CastRayClosest(world, from, dir, b2DefaultQueryFilter());` | `class MyRayCallback : public b2RayCastCallback { float ReportFixture(...) override; }; world.RayCast(&cb, from, to);` — v2 는 callback 객체 패턴 |
| `b2Shape_GetUserData(shapeId)` | `fixture->GetUserData()` 또는 `body->GetUserData()` |

### 4.2 PhysicsSystem (Client 한정, 결정 #18)

```cpp
// <apps>/_MyApp_/src/physics/physics_system.h
#ifndef __MYAPP_PHYSICS_SYSTEM_H__
#define __MYAPP_PHYSICS_SYSTEM_H__

#include <<box2d>/box2d.h>
#include "scene/scene.h"

namespace myapp {

class PhysicsSystem {
public:
    void Init() { world_ = std::make_unique<b2World>(b2Vec2(0.0f, 0.0f)); }
    void Shutdown() { world_.reset(); }

    void Step(SJH::scene::Scene& scene, float dt);
    void SyncToTransform(SJH::scene::Scene& scene);
    void ProcessContacts(SJH::scene::Scene& scene);

    b2World& World() { return *world_; }

private:
    std::unique_ptr<b2World> world_;
};

}  // namespace myapp
#endif
```

### 4.3 Filter 카테고리 (가이드 §4.3 그대로 — uint16 권장)

v2.4.1 의 `categoryBits / maskBits` 는 **uint16**. 가이드의 `uint64_t` 는 v3 가 확장한 것이므로 6비트 (PLAYER/ENEMY/BULLET_PLAYER/BULLET_ENEMY/WALL/PICKUP) 는 무난.

```cpp
namespace myapp::filter {
    constexpr uint16_t PLAYER        = 1 << 0;
    constexpr uint16_t ENEMY         = 1 << 1;
    constexpr uint16_t BULLET_PLAYER = 1 << 2;
    constexpr uint16_t BULLET_ENEMY  = 1 << 3;
    constexpr uint16_t WALL          = 1 << 4;
    constexpr uint16_t PICKUP        = 1 << 5;
}
```

### 4.4 좌표계 동기화 (§10 참조)

```cpp
void PhysicsSystem::SyncToTransform(SJH::scene::Scene& scene) {
    for (auto* actor : scene.AllActorsWith<PhysicsBodyComponent, SJH::scene::TransformComponent>()) {
        auto& pb = actor->Get<PhysicsBodyComponent>();
        auto& tr = actor->Get<SJH::scene::TransformComponent>();
        b2Vec2 p = pb.body->GetPosition();
        float  a = pb.body->GetAngle();
        // 물리 XY → 렌더 XZ (Y 는 빌보드 height_offset)
        tr.SetPosition(vmath::vec3(p.x, tr.HeightOffset(), -p.y));
        tr.SetRotation(a);
    }
}
```

(`scene.AllActorsWith<...>()` 의 정확한 API 명은 `SJH::scene` 현 구현에 맞춰 M2 시점에 확정. 본 spec 은 의도만 명시.)

### 4.5 Sensor / Contact 처리

v2.4.1 은 `b2ContactListener` 패턴:

```cpp
class GameContactListener : public b2ContactListener {
public:
    void BeginContact(b2Contact* c) override;
    void EndContact(b2Contact* c) override;
};
// PhysicsSystem::Init() 에서:
world_->SetContactListener(&listener_);
```

피격/픽업 판정은 contact 의 양쪽 `fixture->GetUserData()` 로 Actor 포인터 회수 → `myapp::HealthComponent` 갱신.

### 4.6 빠른 총알 (v2.4.1)

`bd.bullet = true` 로 CCD. ray cast 패턴은 §4.1 표 참조 — callback 클래스 작성 필수.

---

## §5 파티클 — Effekseer 1.7.3.0

가이드 §5 코드는 **거의 그대로 사용 가능**. 다음만 본 저장소 정착에 맞춤.

### 5.1 정착 사항

- include 경로: `#include "Effekseer.h"` / `#include "EffekseerRendererGL.h"` (가이드와 동일, 본 `include/` 에 헤더 체크인됨)
- 버전 차이 (1.80.2 → 1.7.3.0): API 큰 변경 없음. `Effekseer::Manager::Create`, `Renderer::Create`, `manager->Update(dtSeconds * 60.0f)` 모두 동일
- 가이드 §5.3 의 GLFW 컨텍스트 힌트 강제는 **sb7::application::init() override 가 이미 처리** — 별도 작업 불필요
- macOS issue #671 은 sb7 가 이미 forward-compat 플래그 처리하므로 우회됨

### 5.2 vmath 변환

`Effekseer::Matrix44` 송신 시 vmath::mat4 → Effekseer 매트릭스 변환 헬퍼 필요:

```cpp
inline Effekseer::Matrix44 ToEfkMatrix(const vmath::mat4& m) {
    Effekseer::Matrix44 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r.Values[i][j] = m[i][j];   // row/col 정렬 확인 필요 — M5 측정
    return r;
}
```

### 5.3 효과 카탈로그 (가이드 §5.4 그대로)

| 효과 | Effekseer 패턴 | 트리거 |
|---|---|---|
| 총알 trail | Ribbon 노드 + 빌보드 텍스처 | `BulletComponent` 생성 시 |
| 적 폭발 | Sphere emitter + alpha fade | `Health.current <= 0` 시 |
| 피격 hit spark | Point emitter + 짧은 ttl | b2ContactListener BeginContact 시 |
| 머즐 플래시 | 단발 빌보드 + 0.05s ttl | 발사 함수 호출 시 |
| 픽업 빛 | 회전 메시 + emissive | 픽업 spawn 시 |

---

## §6 애니메이션 + 두 FSM (Component)

가이드 §6 의 *두 FSM 분리 정신* (글로벌 1Hz, per-entity per-frame) 은 그대로. EnTT registry 의존 제거. **추가로 FSM 자체는 신규 코어 모듈 `SJH::fsm` 으로 승격** (2026-05-24 결정).

---

> ## ⚠️ §6 진화 노트 — Stage 1 → Stage 4 (2026-05-25)
>
> 본 §6 의 *코드 예시* 들은 M2 P1 작업 *초기 (Stage 1)* 의 `StateMachineProcessor<TState, TTransit>` switch-on-enum 베이스 design 으로 작성됨. 2026-05-25 commit chain 으로 4 단계 진화하여 **현재 정본 ≠ 본 §6.0.2 / §6.3 / §6.4 의 코드**.
>
> **현재 정본**: [`2026-05-25-fsm-object-state-machine-design.md`](2026-05-25-fsm-object-state-machine-design.md)
>
> ### 진화 요약 (4 commits)
>
> | Stage | Commit | 변화 |
> |---|---|---|
> | Stage 1 | `78dea2a` | `StateMachineProcessor<TState, TTransit>` switch-on-enum (본 §6.0.2 정본) |
> | Stage 2 | `16a6cdd` | `ObjectStateMachine` + `IFsmState<TOwner>` 도입 (4 엔진 정통 흡수, State 객체화) |
> | Stage 3 | `6f5346c` | rename + Stage 1 폐기 — `state_machine_processor.h` + `test_fsm.cpp` 제거 |
> | **Stage 4** | `3bdf499` | **`StateMachine<TState, TOwner>` + `GetTransitFlag()` 응집** (TTransit template parameter 제거) |
>
> ### Stage 4 정본 핵심 차이
>
> | 측면 | Stage 1 (본 §6 코드) | Stage 4 (현재 정본) |
> |---|---|---|
> | Template 파라미터 | `<TState, TTransit>` | `<TState, TOwner>` (TTransit 제거) |
> | State 보관 | switch-on-enum (Processor 자체에 모든 분기) | `IFsmState<TOwner>` 파생 객체 + `unordered_map<bit, unique_ptr<IFsmState>>` |
> | 그래프 정보 위치 | `targetOf_` map (별도) | 각 State 의 `GetTransitFlag()` (응집) |
> | RegisterTransit | `Bind(transit, target)` 필요 | 제거됨 — State 가 자기 transit flag 보유 |
> | TryTransit 인자 | `TTransit` (from-bit OR) | `TState` (target 비트) |
>
> ### 폐기된 의존 (본 §6 에서 언급되나 Stage 4 에서 사라짐)
>
> - `myapp::FSMTickSystem` 자유함수 — Stage 2 의 `Component::Update(float)` 가 *자기 Tick* 위임으로 흡수. `Actor::Update` 재귀가 자동.
> - `Bind(transit, target)` API — Stage 4 에서 State 가 self-transitioning
> - `unordered_map<StateU, TState> targetOf_` 멤버 — Stage 4 에서 제거
>
> ### 본 §6 의 *Stage 1 코드 보존 이유*
>
> *역사 보존* + *진화 학습 자료*. Stage 1 의 switch-on-enum 접근이 *왜 Anemic Domain Model 안티 패턴* 인지, *어떻게 4 엔진 정통* (Unity/Unreal/Godot/Cocos2d) *과 4/4 어긋났는지* 의 reasoning 이 [`2026-05-25-fsm-object-state-machine-design.md`](2026-05-25-fsm-object-state-machine-design.md) §"진화 History" 에 기록됨.
>
> 이하 §6.0.x / §6.3 / §6.4 의 코드 예시는 *Stage 1 정본 그대로 보존*. M2 P2 작업 시 *Stage 4 정본 spec 참조*.

---

### 6.0 신규 코어 모듈 — `SJH::fsm` 설계

#### 6.0.1 핵심 아이디어 — 64bit 비트 마스크 인코딩, 테이블 free

- **State enum** `: uint64_t` — 각 상태가 1비트. `NONE = 0` 포함. 최대 64 상태/FSM
- **TransitionFlag enum** `: uint64_t` — 각 transition 값 = *허용된 from 상태들의 bitwise OR* (`|`). 즉 transition 자체가 *"어디서 이 transition 이 발동 가능한가"* 의 from set
- **target state 는 `Bind(transit, target)` 으로 외부 등록** — transition 이름은 `ToXxx` 컨벤션 (가독성), 실제 target 매핑은 런타임 bind
- **판정**: `((uint64_t)transit & (uint64_t)current) == (uint64_t)current` — current 가 from set 에 포함되는지 1-step AND
- **테이블/맵 불필요** (transit → target 매핑 하나만 보유)

사용자 명시 예시 (2026-05-24):
```cpp
enum class CharState : uint64_t {
    NONE   = 0,
    Idle   = 1ull << 0,
    Move   = 1ull << 1,
    Attack = 1ull << 2,
    Die    = 1ull << 3,
};

enum class CharTransit : uint64_t {
    ToIdle   = (uint64_t)CharState::Move   | (uint64_t)CharState::Attack,   // Move/Attack → Idle
    ToMove   = (uint64_t)CharState::Idle,                                    // Idle → Move
    ToAttack = (uint64_t)CharState::Idle   | (uint64_t)CharState::Move,     // Idle/Move → Attack
    ToDie    = (uint64_t)CharState::Idle   | (uint64_t)CharState::Move
             | (uint64_t)CharState::Attack,                                  // 어디서든 Die
};
```

#### 6.0.2 `SJH::fsm` 헤더 시그니처

```cpp
// <src>/fsm/state_machine_processor.h
#ifndef __SJH_FSM_STATE_MACHINE_PROCESSOR_H__
#define __SJH_FSM_STATE_MACHINE_PROCESSOR_H__

#include "<scene>/component.h"     // SJH::scene::Component
#include <unordered_map>
#include <cstdint>

namespace SJH::FSM {

// TState / TTransit 모두 enum class : uint64_t 가정.
// 사용자는 이 템플릿을 상속해 OnEnter/OnUpdate/OnExit 가상함수 override.
template<typename TState, typename TTransit>
class StateMachineProcessor : public SJH::scene::Component {
public:
    using StateU = uint64_t;

    StateMachineProcessor() = default;
    explicit StateMachineProcessor(TState startup) : current_(startup) {}

    // 1) transit → target 매핑 등록 (생성자에서 일괄 호출 권장)
    void Bind(TTransit transit, TState target) {
        targetOf_[(StateU)transit] = target;
    }

    // 2) transition 시도. 허용된 from 이면 OnExit → 상태 교체 → OnEnter 발화 후 true
    bool TryTransit(TTransit transit) {
        StateU allowedFrom = (StateU)transit;
        StateU curr        = (StateU)current_;
        if (curr == 0 || (allowedFrom & curr) != curr) return false;   // from 비매치
        auto it = targetOf_.find((StateU)transit);
        if (it == targetOf_.end()) return false;                       // target 미등록
        OnExit(current_);
        current_ = it->second;
        OnEnter(current_);
        return true;
    }

    // 3) 매 프레임 호출 (FSM Processor 가 부착된 Actor 의 owner 가 호출)
    void Tick(float dtSeconds) { OnUpdate(current_, dtSeconds); }

    TState State() const { return current_; }

    // === Client override ===
    virtual void OnEnter(TState /*s*/)              {}
    virtual void OnUpdate(TState /*s*/, float /*dt*/) {}
    virtual void OnExit(TState /*s*/)               {}

private:
    TState current_ = TState::NONE;   // enum 에 NONE = 0 약속
    std::unordered_map<StateU, TState> targetOf_;
};

}  // namespace SJH::FSM
#endif
```

#### 6.0.3 의존 그래프

- **PUBLIC**: `SJH::scene` (Component 베이스), `SJH::common` (만약 공통 유틸 필요)
- **PRIVATE**: 없음 (헤더 onlyに 가까움 — `.cpp` 는 explicit instantiation 정도)
- **game_deps 의존 0** — FSM 은 도메인 비종속. CharacterFSM 의 외부 도메인 호출 (clip swap, FMOD 발사음) 은 *Client 가 상속한 클래스* 의 OnEnter override 에서 처리

#### 6.0.4 Action 처리 방식 (결정 2026-05-24): **사용자 정의 클래스 상속 가상함수**

OnEnter/OnUpdate/OnExit override 안에서 Owner() 통해 Actor 접근 + 도메인 로직. 구체 패턴 (Playable container 보유 + active 교체) 은 §6.3 PlayerStateMachine 정본 참조 — 본 섹션은 *override 방식 자체* 만 결정.

#### 6.0.5 글로벌 FSM 호스트 — Scene 의 root Actor 부착 (사용자 결정)

사용자 결정 (2026-05-24): **글로벌 FSM = Scene 의 root Actor 에 StateMachineProcessor Component 부착**.

⚠️ **사전 작업** — `SJH::scene::Scene::Root()` (씬당 1개 root Actor 반환) helper 가 현 `SJH::scene` 구현에 존재하는지 *M2 진입 시 확인 필수*. 없으면 다음 둘 중 하나:
- **A**: `SJH::scene::Scene` 에 `Actor& Root()` 추가 (씬당 1개 암묵적 root 보장)
- **B**: 데모가 명시적으로 `scene_.CreateActor("root")` 후 보관 (Scene 본체 변경 없음)

M1~M2 진입 시 SJH::scene 헤더 확인 후 결정. spec 본문은 A 를 *기대 동작* 으로 기술.

### 6.1 SpriteFrameClip — `SJH::sprite_sequence` 거주 (코어)

§1.6 참조. *Sprite atlas 의 frame indices 시퀀스 정의*. `soundEventFrame / soundId` 필드 폐기 — sound 트리거는 Composite Playable (Sequence/Parallel) 로 표현. *진행 상태 (elapsed/finished) 는 SpriteSequencePlayable 자체가 보유 — 별도 Component 없음*.

### 6.2 SpriteSequencePlayable — `SJH::Playable::Playable` 상속

§1.6 참조. `SpriteSequencePlayable::OnTick(dt)` 이 `SpriteComponent.frameIdx` 갱신. `IsFinished()` 는 non-loop clip 의 마지막 frame 도달 시 true, loop clip 은 *never finished*.

**Bullet/Effect 같은 단발성 Actor — `PlayablePlayerComponent` 사용** (단일 root):

```cpp
// 단발성 Effect Actor 셋업 (factory 또는 spawn 함수 안)
auto* effect = scene.CreateActor();
auto* sprite = effect->Add<SJH::Sprite::SpriteComponent>();
auto* player = effect->Add<SJH::Playable::PlayablePlayerComponent>();
player->SetRoot(std::make_unique<SJH::SpriteSequence::SpriteSequencePlayable>(sprite, explosionClip));
player->Play();
// 끝나면 (player->IsFinished()) DespawnSystem 이 effect Actor destroy
```

**상태 기반 Actor (Player/Enemy) — StateMachine 파생 클래스 사용**: §6.3 참조 (PlayablePlayerComponent 미사용).

**PlayableTickSystem** — Scene 의 모든 `PlayablePlayerComponent` 매 프레임 Tick:

```cpp
// <src>/playable/playable_tick_system.cpp
void SJH::Playable::PlayableTickSystem::Tick(SJH::scene::Scene& scene, float dt) {
    for (auto* a : scene.AllActorsWith<PlayablePlayerComponent>()) {
        a->Get<PlayablePlayerComponent>()->Tick(dt);
    }
}
```

**FSMTickSystem** 은 별도 (§6.3) — StateMachine 파생 Component 들의 Tick 책임. 두 시스템 *분리 호출* (둘 다 main.cpp Update 단계에서 호출).

### 6.3 PlayerStateMachine — StateMachine 자체가 Playable container 보유 (2026-05-24 확정)

상태 기반 캐릭터 (Player/Enemy) 의 FSM. 비트 enum (`PlayerState` / `PlayerTransit`) + StateMachine 파생 클래스가 *unordered_map<PlayerState, unique_ptr<Playable>>* container 보유. OnEnter 가 active 교체, OnUpdate 가 active Tick + 자기 전이, OnExit 가 Pause. **PlayablePlayerComponent 미부착** — StateMachine 자기가 Playable 호스팅 책임 흡수.

```cpp
// <apps>/_MyApp_/src/game/player_state_machine.h
namespace myapp {

// === enum class 비트 — enumerator 이름이 "PlayerXxxState" 컨벤션 ===
enum class PlayerState : uint64_t {
    NONE              = 0,
    PlayerIdleState   = 1ull << 0,
    PlayerMoveState   = 1ull << 1,
    PlayerAttackState = 1ull << 2,
    PlayerDieState    = 1ull << 3,
};
enum class PlayerTransit : uint64_t {
    ToPlayerIdleState   = (uint64_t)PlayerState::PlayerMoveState
                        | (uint64_t)PlayerState::PlayerAttackState,
    ToPlayerMoveState   = (uint64_t)PlayerState::PlayerIdleState,
    ToPlayerAttackState = (uint64_t)PlayerState::PlayerIdleState
                        | (uint64_t)PlayerState::PlayerMoveState,
    ToPlayerDieState    = (uint64_t)PlayerState::PlayerIdleState
                        | (uint64_t)PlayerState::PlayerMoveState
                        | (uint64_t)PlayerState::PlayerAttackState,
};

// === Client StateMachine — Playable container 보유 + lifecycle 책임 ===
class PlayerStateMachine
    : public SJH::FSM::StateMachineProcessor<PlayerState, PlayerTransit> {
public:
    // 생성자에서 각 state 별 Playable 받아 등록 — 영구 보유 (heap 할당 1회)
    PlayerStateMachine(
        std::unique_ptr<SJH::Playable::Playable> idle,
        std::unique_ptr<SJH::Playable::Playable> move,
        std::unique_ptr<SJH::Playable::Playable> attack,
        std::unique_ptr<SJH::Playable::Playable> die)
    {
        playables_[PlayerState::PlayerIdleState]   = std::move(idle);
        playables_[PlayerState::PlayerMoveState]   = std::move(move);
        playables_[PlayerState::PlayerAttackState] = std::move(attack);
        playables_[PlayerState::PlayerDieState]    = std::move(die);

        Bind(PlayerTransit::ToPlayerIdleState,   PlayerState::PlayerIdleState);
        Bind(PlayerTransit::ToPlayerMoveState,   PlayerState::PlayerMoveState);
        Bind(PlayerTransit::ToPlayerAttackState, PlayerState::PlayerAttackState);
        Bind(PlayerTransit::ToPlayerDieState,    PlayerState::PlayerDieState);
    }

    void OnEnter(PlayerState s) override {
        if (current_) current_->Pause();      // 이전 active 일시정지 (상태 보존)
        current_ = playables_[s].get();
        if (current_) current_->Play();       // 새 active 시작
    }

    void OnUpdate(PlayerState s, float dt) override {
        if (current_) current_->Tick(dt);
        // === 자기 전이 (사용자 결정 Q3) — 자연 종료 시 자동 Idle ===
        if (s == PlayerState::PlayerAttackState && current_ && current_->IsFinished()) {
            TryTransit(PlayerTransit::ToPlayerIdleState);
        }
        // Die 는 영구 (transit 없음)
        // Idle / Move 는 외부 trigger (InputSystem 이 TryTransit 호출)
    }

    void OnExit(PlayerState /*s*/) override {
        if (current_) current_->Pause();
    }

private:
    std::unordered_map<PlayerState, std::unique_ptr<SJH::Playable::Playable>> playables_;
    SJH::Playable::Playable* current_ = nullptr;
};

}  // namespace myapp
```

**Actor 부착** — Player Actor 는 *SpriteComponent + PlayerStateMachine* 만 (PlayablePlayerComponent 미부착):

```cpp
auto* player = scene.CreateActor();
auto* sprite = player->Add<SJH::Sprite::SpriteComponent>();

// 각 state 별 Playable 미리 생성 (Composite 도 가능 — Attack 은 시퀀스로)
auto idle   = std::make_unique<SJH::SpriteSequence::SpriteSequencePlayable>(sprite, idleClip);
auto move   = std::make_unique<SJH::SpriteSequence::SpriteSequencePlayable>(sprite, moveClip);
auto attack = std::make_unique<SJH::SpriteSequence::SpriteSequencePlayable>(sprite, attackClip);
auto die    = std::make_unique<SJH::SpriteSequence::SpriteSequencePlayable>(sprite, dieClip);

player->Add<myapp::PlayerStateMachine>(
    std::move(idle), std::move(move), std::move(attack), std::move(die));

// 초기 상태 — 첫 transit
auto* psm = player->Get<myapp::PlayerStateMachine>();
psm->TryTransit(PlayerTransit::ToPlayerIdleState);   // OnEnter(Idle) 호출
```

**외부 trigger** (InputSystem 에서):
```cpp
auto* psm = player->Get<myapp::PlayerStateMachine>();
if (input.WasPressed(Action::Fire))     psm->TryTransit(PlayerTransit::ToPlayerAttackState);
if (input.IsDown(Action::MoveUp))       psm->TryTransit(PlayerTransit::ToPlayerMoveState);
if (!input.IsDown(Action::MoveAny))     psm->TryTransit(PlayerTransit::ToPlayerIdleState);
```

**FSMTickSystem** — Scene 의 모든 StateMachine Component Tick:
```cpp
// myapp::FSMTickSystem (Client) — 또는 SJH::fsm 의 자유함수로 승격 가능
void myapp::FSMTickSystem::Update(SJH::scene::Scene& scene, float dt) {
    for (auto* a : scene.AllActorsWith<PlayerStateMachine>()) {
        a->Get<PlayerStateMachine>()->Tick(dt);   // OnUpdate(current, dt) 발화
    }
    for (auto* a : scene.AllActorsWith<EnemyStateMachine>()) {
        a->Get<EnemyStateMachine>()->Tick(dt);
    }
    // ... 다른 StateMachine 종류
}
```

**Composite Playable 도 등록 가능** — Attack 을 *시퀀스* (muzzleFx → 동시(sfx, anim)) 로:
```cpp
auto attackSeq = std::make_unique<SJH::Playable::SequencePlayable>();
attackSeq->Then(std::make_unique<myapp::EffekseerPlayable>(muzzleFx))
         .Then(std::unique_ptr<SJH::Playable::ParallelPlayable>(
                  (new SJH::Playable::ParallelPlayable())
                      ->Join(std::make_unique<myapp::FmodPlayable>("gun_shot.wav"))
                      ->Join(std::make_unique<SJH::SpriteSequence::SpriteSequencePlayable>(sprite, attackClip))));
// → Attack state 의 Playable 로 등록
playables_[PlayerState::PlayerAttackState] = std::move(attackSeq);
```

→ State 가 단일 Playable 인지 Composite 트리인지 *유연* — Container 는 `unique_ptr<Playable>` 한 칸이라 어떤 트리든 OK.

**전이 호출** (Input 시스템 또는 AI 시스템에서):
```cpp
auto* fsm = player->Get<myapp::CharacterFSM>();
if (input.WasPressed(Action::Fire)) fsm->TryTransit(CharTransit::ToAttack);
if (input.IsDown(Action::MoveUp))   fsm->TryTransit(CharTransit::ToMove);
```

**Tick 호출** (매 프레임 시스템 — myapp::FSMTickSystem 자유함수가 scene 전체 순회):
```cpp
void myapp::FSMTickSystem::Update(SJH::scene::Scene& scene, float dt) {
    for (auto* a : scene.AllActorsWith<CharacterFSM>()) {
        a->Get<CharacterFSM>()->Tick(dt);   // OnUpdate(current, dt) 발화
    }
}
```

### 6.4 글로벌 GameFSM (Scene root Actor 에 부착, Playable container 선택적)

사용자 결정 — 글로벌 FSM 은 Scene root Actor 의 Component. 같은 `SJH::FSM::StateMachineProcessor` 상속. **Playable container 는 GameFSM 에선 선택적** — Startup/Combat/Boss/End 가 *시각/사운드 시퀀스* 보다 *씬 상태 변경* (wave spawn, boss spawn 등) 책임 위주라 OnEnter/OnUpdate 안에서 직접 scene 조작이 자연. 단 End 화면 등 *시각 시퀀스* 가 필요하면 PlayerStateMachine 처럼 container 도입 가능.

```cpp
// <apps>/_MyApp_/src/game/game_fsm.h
namespace myapp {
enum class GameState : uint64_t {
    NONE       = 0,
    Startup    = 1ull << 0,
    Combat     = 1ull << 1,
    BossCombat = 1ull << 2,
    End        = 1ull << 3,
};
enum class GameTransit : uint64_t {
    ToCombat     = (uint64_t)GameState::Startup,
    ToBossCombat = (uint64_t)GameState::Combat,
    ToEnd        = (uint64_t)GameState::BossCombat | (uint64_t)GameState::Combat,
    ToStartup    = (uint64_t)GameState::End,                                       // restart
};

class GameFSM : public SJH::FSM::StateMachineProcessor<GameState, GameTransit> {
public:
    explicit GameFSM(SJH::scene::Scene* scene);
    void OnEnter(GameState s) override;       // wave spawn, boss spawn, end UI
    void OnUpdate(GameState s, float dt) override;
private:
    SJH::scene::Scene* scene_;
    float combatTimer_ = 0.0f;
};
}  // namespace myapp
```

**부착** (TopdownShooter::startup() 안):
```cpp
auto& root = scene_.Root();   // ← SJH::scene::Scene::Root() — M2 사전 확인 필수
root.Add<myapp::GameFSM>(&scene_);
```

**Tick** (FSMTickSystem 이 root Actor 의 Component 도 순회하면 자동 — root 도 일반 Actor):
```cpp
// myapp::FSMTickSystem::Update 가 GameFSM 도 자동 Tick (Actor 차별 없음)
```

CharacterFSM 과 메커니즘 완전 동일 — 유일한 차이는 *어느 Actor 에 부착* 되는가. Scene 의 root Actor 는 한 개라 글로벌 의미 보장.

---

## §7 Tweeny 트위닝

가이드 §7 그대로 — Tweeny v3.2.0 헤더 `<include>/tweeny.h` 사용. user memory 와 일치하는 함정 1건만 강조.

### 7.1 step 오버로드 함정 (반드시 준수)

memory `tweeny_step_overload_trap.md`:

> `step(int32_t ms)` = 밀리초 / `step(float ratio)` = [0..1] 비율. float 로 ms 넘기면 양 끝 깜빡임 폭주. **dtMs 변수는 `int32_t` 로 선언**.

```cpp
// 올바른 사용 — <apps>/_MyApp_/src/tween/tween_system.cpp
void TweenSystem::Update(SJH::scene::Scene& scene, float dtSeconds) {
    int32_t dtMs = static_cast<int32_t>(dtSeconds * 1000.0f);   // ← int32_t 강제
    for (auto* a : scene.AllActorsWith<TweenScaleComponent>()) {
        auto& t = a->Get<TweenScaleComponent>().tween;
        float v = t.step(dtMs);   // step(int32_t) 오버로드 선택됨
        a->Get<SpriteComponent>().size = vmath::vec2(v, v);
        if (t.progress() >= 1.0f) a->Remove<TweenScaleComponent>();
    }
}
```

### 7.2 5가지 게임 패턴 (가이드 §7.4 그대로)

| 패턴 | 트윈 대상 | 콘텐츠 |
|---|---|---|
| 데미지 넘버 부상 | position offset (y) | 피격 시 +0.5 → 1.0s 동안 |
| 무기 발사 반동 | scale | 1.0 → 1.2 → 1.0, 100ms |
| 적 hit 깜빡임 | tint.rgb | white → red → white, 150ms |
| 카메라 shake | shake_offset (XZ) | quadOut 진폭 감쇠 (§11.3) |
| UI 페이드 | tint.a | 0 → 1, 300ms (메뉴) |

### 7.3 Tweeny ↔ SpriteSequencePlayable 분업 (사용자 의도 §0.3)

같은 SpriteComponent 를 둘이 독립 갱신:
- **SpriteSequencePlayable** (`SJH::sprite_sequence`): `frameIdx` (이산) — PlayableTickSystem 또는 StateMachine 안 active.Tick 으로 호출
- **TweenSystem** (Client): `size`, `tint`, position offset (연속) — myapp::TweenSystem 자유함수

레이스 컨디션 없음 — 둘이 *다른 필드* 만 쓴다.

---

## §8 음향 — FMOD Core/Studio

본 저장소는 [`doc/FMOD_Setup.md`](../../FMOD_Setup.md) 가 이미 FMOD 설치 + Dependency.cmake 등록 + POST_BUILD copy 명세를 보유. 본 spec 은 *게임 측 사용 패턴* 만 추가.

### 8.1 AudioSystem (Core API, C 함수 사용)

가이드 §8.2 의 `FMOD_System_Create / Init / Update / Release` C API 그대로. C++ 래퍼 클래스로 RAII:

```cpp
// <apps>/_MyApp_/src/audio/audio_system.h
#ifndef __MYAPP_AUDIO_SYSTEM_H__
#define __MYAPP_AUDIO_SYSTEM_H__

#include <fmod/fmod.h>
#include <string>
#include <unordered_map>

namespace myapp {
class AudioSystem {
public:
    bool Init();
    void Shutdown();
    void Update();   // 매 프레임 — FMOD_System_Update

    FMOD_SOUND* LoadSfx(const std::string& path);
    FMOD_SOUND* LoadMusic(const std::string& path);
    void PlaySfx(const std::string& id);
    void PlayMusic(const std::string& id, bool loop = true);
private:
    FMOD_SYSTEM* sys_ = nullptr;
    std::unordered_map<std::string, FMOD_SOUND*> sfx_;
    std::unordered_map<std::string, FMOD_SOUND*> music_;
};
}
#endif
```

### 8.2 SoundSystem (Scene 통합 — listener/source 갱신)

탑다운은 listener = Camera position, source = Actor 의 PhysicsBody 위치. 매 프레임:

```cpp
void myapp::SoundSystem::Update(SJH::scene::Scene& scene, AudioSystem& audio) {
    // 1) listener = camera
    auto* cam = scene.MainCamera();
    audio.SetListener3DAttributes(cam->Position(), cam->Forward(), cam->Up());

    // 2) 각 SoundSource Actor 의 channel 위치 갱신
    for (auto* a : scene.AllActorsWith<SoundSourceComponent, SJH::scene::TransformComponent>()) {
        auto& src = a->Get<SoundSourceComponent>();
        auto  pos = a->Get<SJH::scene::TransformComponent>().Position();
        if (src.channel) audio.SetChannel3DAttributes(src.channel, pos);
    }
}
```

### 8.3 라이선스 의무

- FMOD Free Indie 크레딧 라인 **`<apps>/_MyApp_/resources/README.md`** 또는 게임 시작 splash 화면에 포함 — `"FMOD Studio by Firelight Technologies Pty Ltd."`
- 매출/예산 한계 초과 시 라이선스 업그레이드 필요. 학생 과제 범위에서는 무관.

---

## §9 main.cpp 루프 + 시간 정책

결정 #17 — Update 루프는 `main.cpp` 의 `render(double currentTime)` 안에 직접 서술.

### 9.1 sb7::application 상속 + 멤버

```cpp
// apps/_MyApp_/main.cpp
#include "sb7.h"
#include "myapp_core.h"   // myapp 의 모든 system + component 일괄 노출

class TopdownShooter : public sb7::application {
public:
    void init() override;        // §2.1 — GL 4.1 강제
    void startup() override;     // scene + system 초기화
    void render(double currentTime) override;   // 9.2
    void shutdown() override;    // 해제 (§9.3)

    void onKey(int key, int action) override;
    void onMouseButton(int button, int action) override;

private:
    SJH::scene::Scene  scene_;
    myapp::AudioSystem audio_;
    myapp::PhysicsSystem physics_;
    myapp::ParticleSystem particle_;
    myapp::CharacterFSM characterFSM_;
    myapp::GameFSM gameFSM_;
    myapp::Input input_;
    myapp::TopdownCamera camera_;
    SJH::ResourceRegistry registry_;   // 자원 owner (§12)

    double prevTime_ = 0.0;
    bool   firstFrame_ = true;
};

DECLARE_MAIN(TopdownShooter);
```

### 9.2 render() 본문 — Update + Render 9 단계

```cpp
void TopdownShooter::render(double currentTime) {
    double dt = firstFrame_ ? 0.0 : (currentTime - prevTime_);
    dt = std::min(dt, 0.05);   // 디버거 break clamp
    prevTime_ = currentTime;
    firstFrame_ = false;
    float dts = static_cast<float>(dt);

    // === Update ===
    input_.BeginFrame();                                // 1
    myapp::InputSystem::Update(scene_, input_, dts);    // 2  → Velocity
    myapp::AISystem::Update(scene_, dts);               // 3  → Velocity / FSM
    physics_.Step(scene_, dts);                         // 4  Box2D world.Step
    physics_.SyncToTransform(scene_);                   // 5  b2Body → Transform
    SJH::Playable::PlayableTickSystem::Tick(scene_, dts);  // 6  모든 PlayablePlayer.Tick — SpriteSequencePlayable / FmodPlayable / EffekseerPlayable / Sequence / Parallel 재귀
    myapp::TweenSystem::Update(scene_, dts);            // 7  scale/tint/offset
    myapp::DespawnSystem::Update(scene_, dts);          // 8  ttl → Dead
    myapp::SoundSystem::Update(scene_, audio_);         // 9  listener/source
    audio_.Update();                                    // 10 FMOD_System_Update
    myapp::CleanupSystem::Update(scene_);               // 11 Dead → destroy
    gameFSM_.Update(scene_, dts);                       // 12 글로벌 상태

    // === Render ===
    camera_.Update(scene_, dts);
    myapp::RenderSystem::Draw(scene_, registry_, camera_);   // 빌보드 5단계 패스
    particle_.Draw(camera_);                                  // Effekseer
    // UI 는 ImGui 미사용 — 단순 텍스트 HUD 시 별도 OBJ/glDrawElements
}
```

### 9.3 Shutdown 순서 (자원 해제 의존성)

```cpp
void TopdownShooter::shutdown() {
    scene_.Clear();           // 1) Actor 제거 → Component 소멸자 (b2Body* 해제 포함)
    physics_.Shutdown();      // 2) b2World 파괴
    particle_.Shutdown();     // 3) Effekseer manager + renderer
    registry_.ReleaseAll();   // 4) Texture/Material/Model 해제
    audio_.Shutdown();        // 5) FMOD system close+release (마지막)
}
```

### 9.4 시간 단위 정책

| 시스템 | 받는 단위 | 변환 |
|---|---|---|
| `render(double currentTime)` 진입 | double seconds | sb7 가 `glfwGetTime()` 반환값 그대로 |
| Box2D | float seconds | `world.Step((float)dt, 8, 3)` |
| Tweeny | **int32_t ms** | `static_cast<int32_t>(dt * 1000.0f)` — float 오버로드 절대 금지 |
| Effekseer | float (60fps frame) | `manager->Update((float)(dt * 60.0))` |
| SpriteSequencePlayable | float seconds | `elapsed_ += (float)dt` (Playable 자체 멤버) |
| FMOD | (자동) | `Update()` 매 프레임 호출만 |

---

## §10 좌표계 + 렌더 패스 정책

### 10.1 좌표계 (가이드 §2.6 그대로)

| 공간 | 축 | 비고 |
|---|---|---|
| 물리 (Box2D 2.4.1) | 2D, +X 오른쪽, +Y 위 | meters |
| 렌더 (OpenGL, vmath) | 3D 오른손, +Y 위, -Z 앞 | 카메라가 +Y 에서 -Y 응시 |
| 매핑 | `render = vmath::vec3(b2.x, heightOffset, -b2.y)` | Billboard heightOffset 으로 떠 있는 거리 표현 |
| 단위 | 1m = 100 픽셀 (Box2D 권장 0.1~10m 준수) | atlas tile 64px ≈ 0.64m |

### 10.2 렌더 패스 5단계 (가이드 §부록 I 그대로)

```
1. Opaque       depth ON  / write ON  / blend OFF      배경/벽/지형
2. AlphaTest    depth ON  / write ON  / blend OFF      픽셀아트 캐릭터 (frag discard)
3. Transparent  depth ON  / write OFF / blend ON       반투명 빌보드 (back-to-front)
4. Particle     Effekseer 내부 state                   파티클
5. UI           depth OFF / write OFF / blend ON       HUD
```

### 10.3 정렬 키 함정 (가이드 부록 I.5)

- ❌ view space z 직접 정렬 — OpenGL forward = -Z 라 부호 헷갈림
- ✅ `dot(cam.Forward(), pos - cam.Position())` 큰 값 = 멀다 → 내림차순
- ✅ `std::stable_sort` (같은 깊이 깜빡임 방지)

---

## §11 카메라 + Input

### 11.1 TopdownCamera

가이드 §J.2 의 Camera 클래스를 vmath 로 번역 + `SJH::scene::Camera` 가 이미 존재한다면 wrapper. 회전 불능 + entity follow + shake.

```cpp
// <apps>/_MyApp_/src/camera/topdown_camera.h
namespace myapp {
class TopdownCamera {
public:
    vmath::vec3 Position() const { return position_ + shakeOffset_; }
    vmath::vec3 Forward()  const { return forward_; }
    vmath::vec3 Up()       const { return vmath::vec3(0, 1, 0); }
    vmath::mat4 View()       const;
    vmath::mat4 Projection() const;

    void SetFollowTarget(SJH::scene::Actor* a) { target_ = a; }
    void ApplyShakeOffset(vmath::vec3 o)       { shakeOffset_ = o; }

    void Update(SJH::scene::Scene& scene, float dt, float followLerp = 5.0f);

private:
    vmath::vec3 position_   = vmath::vec3(0, 10, 10);
    vmath::vec3 forward_    = vmath::vec3(0, -1, -0.5f);
    vmath::vec3 shakeOffset_= vmath::vec3(0, 0, 0);
    SJH::scene::Actor* target_ = nullptr;
};
}
```

`Update` 의 exponential lerp 는 `vmath::mix` 또는 직접 컴포넌트별 보간 (vmath 의 mix 시그니처에 따라).

### 11.2 Input (`SJH::input` 활용)

본 저장소에 `SJH::input` 모듈이 이미 존재 (KeyboardInput<TAction> / MouseInput). 가이드 §J.4 의 Input 클래스를 *처음부터* 만들지 말고 `SJH::input` 의 wrapper 로:

```cpp
// <apps>/_MyApp_/src/input/input_actions.h
namespace myapp {
enum class Action { MoveUp, MoveDown, MoveLeft, MoveRight, Fire, Reload };

using Input = SJH::input::KeyboardInput<Action>;   // (실제 템플릿 시그니처 확인 후 M2 확정)
}
```

GLFW key callback 은 `sb7::application::onKey` override 에서 `input_.OnKey(key, action)` 라우팅.

### 11.3 ShakeSystem (범용 — Tweeny 진폭 감쇠)

가이드 §J.3 그대로. Camera entity 에도 일반 Actor 에도 같은 ShakeEffectComponent 부착하면 동일하게 동작.

```cpp
struct ShakeEffectComponent : public SJH::scene::Component {
    tweeny::tween<float> magnitudeTween;
    vmath::vec3 axisMask = vmath::vec3(1, 0, 1);   // 탑다운 XZ 만
    int seed = 0;
};

namespace myapp::ShakeSystem {
    void Update(SJH::scene::Scene& scene, TopdownCamera& cam, int32_t dtMs);
}
```

### 11.4 마우스 → 월드 좌표 (에이밍)

가이드 §J.5 의 ray-plane 교차. vmath 로 번역:

```cpp
vmath::vec2 myapp::TopdownCamera::ScreenToWorldXZ(vmath::vec2 screen, int winW, int winH) const {
    vmath::vec4 ndc(
        (2.0f * screen[0]) / winW - 1.0f,
        1.0f - (2.0f * screen[1]) / winH,
        -1.0f, 1.0f);
    vmath::mat4 invVP = vmath::inverse(Projection() * View());
    vmath::vec4 world = invVP * ndc;
    world /= world[3];
    vmath::vec3 rayOrigin = Position();
    vmath::vec3 rayDir    = vmath::normalize(vmath::vec3(world[0], world[1], world[2]) - rayOrigin);
    float t = -rayOrigin[1] / rayDir[1];   // y=0 평면
    vmath::vec3 hit = rayOrigin + rayDir * t;
    return vmath::vec2(hit[0], hit[2]);
}
```

---

## §12 자원 보유 컨벤션 (`SJH::resource_registry`)

결정 #13 — 가이드 부록 G 의 신규 `ResourceManager` 클래스는 만들지 않는다. 본 저장소 `SJH::resource_registry` 가 이미 Texture/Material/Model 캐싱을 제공.

### 12.1 정합 매핑

| 가이드 ResourceManager | 본 저장소 `SJH::ResourceRegistry` |
|---|---|
| `GetOrLoadAtlas(path, tile)` | atlas 는 `myapp::UniformAtlas` 가 Texture 를 *생성/소유*. atlas 핸들 자체는 `<apps>/_MyApp_/src/animation/atlas_pool.h` 의 `std::unordered_map<string, UniformAtlas>` 로 owner. **Texture 객체는 등록 가능하면 ResourceRegistry 위탁** |
| `GetOrLoadSfx(path)` / `GetOrLoadMusic(path)` | `myapp::AudioSystem` 가 내부 unordered_map 으로 소유 (FMOD_SOUND* 는 SJH 자원 분류에 안 들어감) |
| `ReleaseAll()` | `SJH::ResourceRegistry::ReleaseAll()` + `AudioSystem::Shutdown()` + `physics_.Shutdown()` (§9.3) 으로 분산 |

### 12.2 미지원 자원 처리 (Program / Mesh)

EngineAPI.md §11.3:
> `SJH::ResourceRegistry` 는 **Texture / Material / Model 만 캐싱**. `Program / Mesh` 는 미지원 → 데모/app 이 임시 owner.

→ 빌보드 셰이더 Program 은 `TopdownShooter` 멤버로 직접 보유. 향후 `CreateProgram` 확장은 future work (본 게임 작업 범위 밖).

---

## 부록 A — 디렉토리 구조 (`src/sprite/` + `apps/_MyApp_/`)

### A.1 신규 SJH 코어 모듈 (2026-05-24)

```
src/sprite/                           # SJH::sprite STATIC + ALIAS (§1.4 정정 후)
├─ CMakeLists.txt
├─ uniform_atlas.h/.cpp               # 정사각 N×M (§2.4) — SJH::Image/Texture 위임 (stb 직접 사용 0, Task 4 fixup)
└─ sprite_component.h                 # SpriteComponent (게임 무관 POD — atlas*, frameIdx, size, tint, flipX)
                                      # ※ PackedAtlas 폐기, SpriteFrameClip/State/System 은 sprite_sequence 으로 이동

src/playable/                         # SJH::playable STATIC + ALIAS (§1.5, Composite 패턴)
├─ CMakeLists.txt
├─ playable.h                         # 추상 Playable — Play/Pause/Tick + IsFinished (game_deps 의존 0)
├─ composite_playable.h               # SequencePlayable / ParallelPlayable (children unique_ptr 보유)
├─ playable_player_component.h        # PlayablePlayerComponent — Actor 부착 진입점
└─ playable_tick_system.h/.cpp        # 자유함수 — Scene 의 모든 PlayablePlayerComponent Tick

src/sprite_sequence/                 # SJH::sprite_sequence STATIC + ALIAS (§1.6, 결정 1 옵션 C)
├─ CMakeLists.txt
├─ sprite_frame_clip.h               # SpriteFrameClip POD (startFrame/frameCount/fps/loop)
└─ sprite_sequence_playable.h/.cpp   # SpriteSequencePlayable : SJH::Playable::Playable
                                      # ※ AnimationStateComponent 폐기 — Playable 자체가 elapsed/finished 보유
                                      # sprite + playable 둘 다 의존

src/fsm/                              # SJH::fsm INTERFACE library (Stage 4 — 헤더-only 정착)
├─ CMakeLists.txt                     # add_library(sjh_fsm INTERFACE) + ALIAS SJH::fsm
├─ fsm_state.h                        # IFsmState<TOwner> — GetStateFlag/GetTransitFlag + Enter/Update/Exit
└─ state_machine.h                    # StateMachine<TState, TOwner> Aggregate Root (Component 베이스)
                                      # ※ Stage 1 의 state_machine_processor.h 는 폐기됨 (6f5346c)
                                      # ※ Stage 4 design = 별도 spec 정본 참조 (위 §6 진화 노트)
```

`src/CMakeLists.txt` 에 다음 줄 추가 + `SJH::engine` 우산에 모두 합류:
```cmake
add_subdirectory(sprite)            # M1
add_subdirectory(fsm)               # M2
add_subdirectory(playable)          # M3.5 (M3 ~ M4 사이)
add_subdirectory(sprite_sequence)  # M3.5 — playable 다음 차례
```

**의존 그래프 (4개 신규 모듈 사이)**:
```
SJH::scene ◄── SJH::sprite          (Sprite Component 가 scene Component 베이스 의존)
SJH::scene ◄── SJH::playable        (PlayablePlayerComponent 가 scene 의존)
SJH::scene ◄── SJH::fsm             (StateMachine Component 베이스가 scene 의존, Stage 4)
SJH::sprite + SJH::playable ◄── SJH::sprite_sequence   (SpriteSequencePlayable 가 둘 다 활용)
```

### A.2 Client 디렉토리 (sprite 모듈 승격 후)

```
apps/_MyApp_/                         # 결정 #16 — 재활성
├─ CMakeLists.txt                     # 패턴: src/ + main.cpp + POST_BUILD (§1.5)
├─ main.cpp                           # sb7::application 상속, 9-step render() (§9)
├─ src/
│  ├─ CMakeLists.txt                  # myapp_core STATIC (§1.5)
│  ├─ myapp_core.h                    # 일괄 노출 헤더 (main.cpp 한 줄 include)
│  ├─ game/
│  │  ├─ components.h                 # 게임 특화 Component 만 —
│  │  │                               # BillboardComponent / HealthComponent / BulletComponent /
│  │  │                               # EnemyComponent / PlayerComponent / DeadComponent /
│  │  │                               # DespawnAfterComponent
│  │  │                               # (SpriteComponent 은 SJH::sprite 거주, AnimationStateComponent 폐기)
│  │  ├─ player_state_machine.h/.cpp  # Stage 4: SJH::FSM::StateMachine<PlayerState, PlayerActor>
│  │  │                               # + IdleState/MoveState/AttackState/DieState (각 IFsmState 파생, GetStateFlag/GetTransitFlag 응집)
│  │  ├─ enemy_state_machine.h/.cpp   # 유사 패턴 (Enemy 도메인)
│  │  ├─ game_fsm.h/.cpp              # Stage 4: StateMachine<GameState, Scene*> + 글로벌 FSM Scene root Actor 부착 (§6.4)
│  │  ├─ # fsm_tick_system.h/.cpp     # ❌ 폐기 — Stage 2 이후 StateMachine::Update(dt) 가 자기 state hook 위임,
│  │  │                               # Actor::Update 재귀가 자동 호출. 별도 system 불요.
│  │  └─ compound_factories.h/.cpp    # CreatePlayer/Enemy/Bullet/Pickup (§3.3) — 풀 없는 ad-hoc spawn (M4 측정 후 내부 변경)
│  ├─ physics/
│  │  ├─ physics_body.h               # Box2D b2Body* 보유 Component (Client 한정 #18)
│  │  ├─ physics_system.h/.cpp        # b2World owner (§4.2)
│  │  ├─ contact_listener.h/.cpp      # b2ContactListener (§4.5)
│  │  └─ filter.h                     # category bits (§4.3)
│  ├─ tween/
│  │  ├─ tween_components.h           # TweenScale/TweenTint/TweenPosOffsetComponent (게임 트윈 정책)
│  │  └─ tween_system.h/.cpp          # step(int32_t) 강제 (§7.1)
│  ├─ audio/
│  │  ├─ audio_system.h/.cpp          # FMOD Core C API 래퍼 (§8.1)
│  │  └─ sound_system.h/.cpp          # listener/source 갱신 (§8.2)
│  ├─ particle/
│  │  └─ particle_system.h/.cpp       # Effekseer manager+renderer (§5)
│  ├─ render/
│  │  ├─ billboard_pass.h/.cpp        # billboard_atlas program (§2.2)
│  │  └─ render_system.h/.cpp         # 5단계 패스 (§10.2)
│  ├─ input/
│  │  └─ input_actions.h/.cpp         # SJH::input::KeyboardInput wrapper (§11.2)
│  ├─ camera/
│  │  └─ topdown_camera.h/.cpp        # follow + shake + ScreenToWorldXZ (§11)
│  └─ effects/
│     └─ shake_system.h/.cpp          # Tweeny 진폭 감쇠 (§11.3)
└─ resources/                         # POST_BUILD 로 실행파일 옆 복사
   ├─ shaders/
   │  ├─ billboard_atlas.vert      # GLSL 410 (§2.2)
   │  └─ billboard_atlas.frag
   ├─ sprites/                     # 64×64 또는 32×32 tile (Aseprite 출력)
   │  ├─ player_atlas.png
   │  ├─ enemy_small_atlas.png
   │  └─ boss_atlas.png
   ├─ audio/{sfx,bgm}/             # .wav (FMOD 디코딩)
   ├─ effects/                     # .efk (M5 이후)
   └─ README.md                    # FMOD Free Indie 크레딧 라인 (§8.3)
```

---

## 부록 B — 함정 모음 (본 저장소 한정)

가이드 부록 B 의 항목은 그대로 유효. 본 저장소 특정 함정만 추가/강조.

### B.1 vmath::radians(int) 함정 (memory `vmath_radians_int_trap.md`)
`vmath::radians(120)` 은 T=int 추론으로 **0** 반환. 반드시 `120.0f`. spec 의 모든 각도 리터럴은 `.0f` 확인 필수.

### B.2 GLSL 410 강제 (memory `glsl_410_project_policy.md`)
모든 셰이더 `#version 410 core`. sb7 base 의 macOS 기본 3.2 에 속지 말 것 — `init()` override 에서 `info.majorVersion = 4; info.minorVersion = 1;` 명시.

### B.3 sb7code 변경 금지 (memory `sb7code_immutable.md`)
`extern/sb7code` 절대 수정 금지. 의존성 충돌은 항상 본 spec 쪽에서 해결.

### B.4 Tweeny step 함정 (memory `tweeny_step_overload_trap.md`)
`step(int32_t ms)` ↔ `step(float ratio)`. dtMs 변수는 `int32_t`. §7.1 코드를 카피-페이스트할 것.

### B.5 stb_image 정의 책임 — `SJH::src/texture/image.cpp` 단일 owner (2026-05-24 재정정)
`STB_IMAGE_IMPLEMENTATION` 는 **`src/texture/image.cpp` 단 한 곳** (2026-05-24 정정 — 원래 이 파일이 owner 였음). `SJH::sprite/uniform_atlas.cpp` 는 `SJH::Image::Load` + `SJH::Texture::CreateTexture` 위임이라 stb 직접 호출 0 — 중복 정의 위험 자동 회피. ~~`STB_RECT_PACK_IMPLEMENTATION`~~ 도입 취소 (PackedAtlas 폐기). 데모 main.cpp 는 정의 *금지* — 정의하면 multiple definition 링크 에러. SJH::resource_registry 를 link 안 하는 데모가 stb_image 를 자체 쓰려면 그 데모 main.cpp 가 정의 — 단 **본 게임 데모는 항상 SJH::engine 우산 (resource_registry 포함) link 이라 직접 정의 절대 안 함**.

### B.6 FMOD POST_BUILD copy 누락 시 (memory `fmod_game_deps_auto_join.md` + `doc/FMOD_Setup.md`)
`game_deps` 링크 데모는 `$<TARGET_FILE:fmod>` (+ studio 있으면 `$<TARGET_FILE:fmodstudio>`) 를 실행 파일 옆으로 `copy_if_different` 의무. §1.4 의 CMakeLists.txt 패턴 그대로.

### B.7 Box2D v2 vs v3 코드 혼동
가이드의 `b2BodyId` / `b2DefaultBodyDef` / `b2World_Step` 류는 모두 v3 API — 본 저장소 v2.4.1 에서는 컴파일 안 됨. §4.1 번역표를 *반드시* 거쳐서 코드 작성. M3 (물리) 진입 시 첫 함정 1번째.

### B.8 ImGui 미사용
본 브랜치에서 `src/imgui/` 모듈 폐기 상태. HUD 가 필요하면 즉석 텍스트 quad 또는 별도 데모 분리. ImGui 도입 결정은 본 spec 범위 밖.

### B.9 EnTT 코드 패턴 자동 거부
가이드의 `entt::registry`, `reg.view<...>`, `reg.emplace<...>` 패턴이 보이면 모두 SJH::scene Actor + Component 로 *번역 후* 도입. EnTT 헤더 include 자체 금지 (memory `entt_removed.md`).

### B.10 SJH::fsm 비트 인코딩 함정 (`||` vs `|`)
**logical OR (`||`) 와 bitwise OR (`|`) 절대 혼동 금지**. `(uint64_t)A || (uint64_t)B` 는 `0 또는 1` 반환 (bool 변환). 의도는 항상 `(uint64_t)A | (uint64_t)B` (bitwise). 사용자 예시 (2026-05-24) 에 `||` 가 등장했지만 *의도는 `|`*. spec §6.0 / §6.3 / §6.4 코드가 정본.

### B.11 SJH::fsm target 미등록 함정
TransitionFlag enum 정의만 하고 `Bind(transit, target)` 호출 안 하면 `TryTransit` 가 *항상 false 반환* (target 매핑 없음). Client 클래스 생성자에서 *모든 transit 에 대해 Bind* 호출 누락 시 무음 실패 — 디버깅 어려움. **체크리스트: enum class XxxTransit 의 모든 enumerator 가 생성자에서 Bind 호출되었는가** 를 코드 리뷰 항목으로.

### B.12 SJH::scene::Scene::Root() 보장 (M2 사전 확인)
글로벌 GameFSM 이 `scene_.Root()` 에 의존 (§6.4). 현 `src/scene/` 구현에 root Actor 자동 보장이 *있는지* M2 진입 시 헤더 검토 필수. 없으면 둘 중:
- (A) SJH::scene 에 `Actor& Scene::Root()` 추가 — 모든 데모 영향
- (B) 데모가 명시적 root 생성 + `myapp::GameSystem::GetGlobalActor(scene)` 같은 helper 우회

### B.13 vmath ↔ Effekseer Matrix 정렬 (M5 시점 측정 필수)
`Effekseer::Matrix44.Values[i][j]` 의 row/col 정렬과 `vmath::mat4` 의 정렬이 일치하는지 M5 실측으로 확인. 일치 안 하면 transpose 필요.

### B.14 Playable IsFinished 무한 loop 함정
loop clip 의 SpriteSequencePlayable 은 `IsFinished()` 가 *영원히 false*. SequencePlayable 안에 loop child 를 넣으면 *Sequence 가 영원히 안 끝남* → 다음 child 로 절대 못 넘어감. 의도된 디자인이지만 실수로 BGM 같은 loop Playable 을 Sequence 에 넣으면 무음 hang. **체크리스트: SequencePlayable.Then() 호출 시 child 가 loop 가능성 있는지 확인** — loop 는 *Parallel 또는 root level* 에서만.

### B.15 Playable Stop 메서드 의도된 부재
Pause = 일시정지 (Play 재호출로 재개) 만 있고 Stop 메서드 없음 (사용자 결정 2026-05-24). 완전 종료는 *자연 종료* (IsFinished=true) 또는 *PlayablePlayerComponent.SetRoot(다른 root)* 로 표현. 강제 중단 + reset 이 필요하면 `PlayablePlayerComponent.SetRoot(nullptr)` 후 `SetRoot(new ...)` + `Play()` 패턴.

### B.16 Playable unique_ptr 소유권 + builder lambda capture
`Sequence().Then(make_unique<X>())` 의 X 는 Sequence 의 children vector 로 *완전 이전*. 외부 raw 포인터 보유 후 X 의 멤버 호출하면 dangling. **체크리스트**: builder 안에서 children 의 *내부 상태에 접근* 필요하면 lambda capture by reference 가 아니라 *별도 Component 로 분리*. SpriteSequencePlayable 이 SpriteComponent 가리키는 것은 OK (Component 가 Actor 가 평생 owner — Playable 보다 오래 삶).

### B.17 Playable 트리 깊이 + diamond
Composite 임의 깊이 허용 (사용자 결정 2026-05-24). 단 같은 Playable 인스턴스를 *두 부모* (Sequence A 의 child + Parallel B 의 child) 가 동시 보유하면 — `unique_ptr` 이라 컴파일 에러로 자동 방지됨. 그러나 *SpriteSequencePlayable 이 같은 SpriteComponent 를 두 곳에서 갱신* 하면 마지막 호출이 이김 (논리적 충돌, 컴파일 OK). PlayablePlayerComponent 1개 = root 1개 = 한 sprite 갱신원 1개 규칙으로 회피.

### B.18 PlayablePlayerComponent vs StateMachine 양자택일 (2026-05-24)
한 Actor 에 **둘 다 부착 금지**:
- *상태 없는 단발성 Actor* (Bullet/Effect/단일 시퀀스 SFX/BGM root) → `PlayablePlayerComponent` 만
- *상태 기반 Actor* (Player/Enemy 등 Idle/Move/Attack/Die 다중 Playable) → `myapp::PlayerStateMachine` (또는 유사 StateMachine 파생) 만

둘 다 부착 시 *같은 SpriteComponent.frameIdx 를 두 곳에서 갱신* — 마지막 Tick 호출이 이김 (어느 시스템이 마지막인지 호출 순서에 의존, 디버깅 어려움). **§6.3 PlayerStateMachine 은 자기가 Playable container + Tick 모두 책임 — PlayablePlayerComponent 가 들어올 자리 없음**.

### B.19 AnimationStateComponent 재활용 금지 (2026-05-24 정정)
옛 spec 의 `AnimationStateComponent` (clip*, elapsed, lastPlayedFrame, finished 보유) **완전 폐기**. 진행 상태의 단일 진실은 `SJH::SpriteSequence::SpriteSequencePlayable` 자체의 `elapsed_ / finished_` 멤버. 별도 Component 로 부활시키지 말 것 — *이중 진실* 동기화 위험. EnTT 가이드 잔재 패턴이다.

---

## 부록 C — 원본 가이드와의 번역 매트릭스

본 spec 이 원본 `IMPLEMENTATION_GUIDE.md` 에서 *재서술* 한 항목 요약. 사용자가 원본을 읽다가 본 spec 으로 점프할 때 참조.

| 원본 § | 본 spec § | 변경 사유 |
|---|---|---|
| §0.4 NFR | §0.3 | `Application/MainWindow` 클래스 가정 제거 (sb7::application 으로 대체) |
| §1 CMake FetchContent | §1 사전 빌드 lib + .gitmodules | 결정 #1 |
| §1.5 FMODImport.cmake | `doc/FMOD_Setup.md` 참조 | 본 저장소가 이미 game_deps 자동 합류 구현 |
| §2.2 빌보드 셰이더 GLSL 330 | §2.2 GLSL 410 | 결정 #5 |
| §2.4 atlas (GLM) | §2.4 (vmath) | 결정 #10 |
| §3 EnTT registry/struct | §3 SJH::scene::Component 클래스 | 결정 #2 |
| §3.4 자동 풀링 (EnTT sparse-set) | §3.3 free factory + 필요시 별도 ObjectPool | EnTT 의존 제거 |
| §4 Box2D v3 (b2BodyId) | §4 Box2D v2.4.1 (b2Body*) | 결정 #6, §4.1 번역표 |
| §5 Effekseer 1.80.2 | §5 Effekseer 1.7.3.0 | 결정 #7 |
| §5.3 GLFW 컨텍스트 힌트 직접 강제 | §2.1 sb7::init() override | sb7 가 흡수 |
| §6 EnTT 결합 AnimationSystem | §6 SJH::scene + Component | 결정 #2 |
| §7 Tweeny | §7 (동일 + step 함정 강조) | memory 일치 |
| §8 FMOD Core | §8 + `doc/FMOD_Setup.md` | 본 저장소 명세 재활용 |
| §9 Application::Update/Render | §9 main.cpp `render(double)` | 결정 #3, #17 |
| 부록 A 디렉토리 | 부록 A `apps/_MyApp_/` | 결정 #4, #16 |
| 부록 G ResourceManager | §12 SJH::resource_registry | 결정 #13 |
| 부록 H 시간 | §9.4 (동일 정신, sb7 currentTime) | sb7 가 glfwGetTime 흡수 |
| 부록 I 렌더 패스 | §10.2 (동일) | — |
| 부록 J Camera/Input | §11 (SJH::input 활용) | SJH 기존 모듈 재활용 |
| 부록 K 셰이더 매니저 핫리로드 | **삭제** | 결정 #14 |
| 부록 K.1 KHR/ARB 콜백 | `SJH::diagnostics` 가 이미 보유 — 추가 작업 불필요 | CLAUDE.md Diagnostics 섹션 |
| 부록 G ResourceManager | §12 SJH::resource_registry + **신규 SJH::sprite 가 atlas owner** | 2026-05-24 결정 — sprite 코어 모듈 |
| §2.4 atlas | **`SJH::sprite` (UniformAtlas + SpriteComponent 만)** — PackedAtlas / stb_rect_pack 도입 폐기 | 2026-05-24 정정 — 등간격 N×M 그리드만, 가변 크기는 atlas 파일 분리로 |
| §6 EnTT AnimationSystem (자유함수, sound 직접 호출) | **`SJH::playable` (Composite Playable) + `SJH::sprite_sequence` (SpriteSequencePlayable)** | 2026-05-24 결정 — Unity Playable 정통 + Composite 패턴. AnimationSystem 폐기, PlayableTickSystem 으로 대체 |
| (가이드 부재) Sequencing 패턴 | **C++ immediate-mode builder** (`Sequence().Then(...).Then(...)` / `Parallel().Join(...)`) | 2026-05-24 결정 — JSON/Lua 미사용 |
| 사운드 트리거 (Animation soundEventFrame) | **시퀀스로 표현** — `Sequence().Then(FmodPlayable).Then(SpriteSequencePlayable)` 또는 Parallel | 2026-05-24 결정 — SoundCallback 람다 폐기. SpriteFrameClip 의 soundEventFrame/soundId 필드 제거 |
| Mixer / weight blending | **삭제** | 2026-05-24 결정 — 단순화. 동시 재생은 *서로 다른 binding 의 독립 Playable* 로 (animation 은 SpriteComponent, audio 는 FMOD::Channel, 충돌 0) |
| (가이드 부재) sprite animation 모듈명 | `SJH::sprite_sequence` + `SpriteSequencePlayable` + `SpriteFrameClip` | 2026-05-24 정정 — "Animation" 이 모호해 *sprite frame sequence* 라는 정확한 명칭. AnimationStateComponent 폐기 (Playable 자체가 진행 상태 보유) |
| (가이드 부재) StateMachine 의 Playable 호스팅 | **PlayerStateMachine 등 StateMachine 파생 클래스가 `unordered_map<TState, unique_ptr<Playable>>` container 직접 보유** + OnEnter 가 active 교체 (Pause/Play). PlayablePlayerComponent 미부착 | 2026-05-24 결정 — 상태 기반 Actor 는 StateMachine 1개가 player+container 책임 흡수. 단발성 Actor (Bullet/Effect) 만 PlayablePlayerComponent |
| §6 EnTT 기반 FSM (자유함수) | **신규 `src/fsm/` 코어 모듈** — `SJH::FSM::StateMachineProcessor<TState, TTransit>` 비트 기반 + 가상함수 + Bind 패턴 | 2026-05-24 사용자 결정 — uint64_t 비트 인코딩 / Action = 가상함수 |
| §6.4 글로벌 FSM 가 Application 의 GameFSM 멤버 | **Scene 의 root Actor 에 GameFSM Component 부착** — per-entity FSM 과 메커니즘 완전 동일 | 2026-05-24 사용자 결정 — *호스트 = root Actor* |
| §3.4 EnTT registry sparse-set 자동 풀링 | **M1~M3 깡 spawn → M4 측정 후 풀 결정** (factory 자유함수 패턴 영구 — Instantiate/Clone 도입 안 함) | 2026-05-24 사용자 결정 — "spawn 완벽 작동 우선" |

---

## 부록 D — 마일스톤 분할 제안 (M1~M7)

본 spec 승인 후 *순차적* 으로 진행. 각 마일스톤은 동작하는 실행 파일 + 시각/기능 검증 가능한 상태.

| M | 범위 | 산출물 | 검증 |
|---|---|---|---|
| **M1** | 빌드 인프라 + **SJH::sprite 모듈 신설** + 빌보드 1장 정적 표시 | (a) `apps/_MyApp_/` 재활성. (b) **`src/sprite/` 신규 모듈** — `uniform_atlas.{h,cpp}` (STB_IMAGE 정의) / `sprite_component.h` POD. `src/CMakeLists.txt` 에 `add_subdirectory(sprite)` + `SJH::engine` 우산에 합류. (c) `billboard_atlas.{vert,frag}` GLSL 410. (d) `TopdownShooter::startup` 에서 atlas 1장 로드 + quad 1개 직접 그리기 | 화면에 Player atlas 64×64 빌보드 1장 정적 표시. `SJH::Sprite::UniformAtlas::LoadFromPNG` + `GetUVRect(0)` 동작 |
| **M2** | Actor + Component + Input 이동 + **SJH::fsm 모듈 신설** + Scene::Root() 확인 | (a) SJH::scene 위 PlayerActor + `SJH::Sprite::SpriteComponent` + (Client) Velocity. `SJH::input::KeyboardInput` 와이어업. WASD 키로 이동. (b) **`src/fsm/` 신규** — `SJH::FSM::StateMachineProcessor<TState, TTransit>` template (§6.0.2) 헤더-only. (c) **`SJH::scene::Scene::Root()` + `Component::Owner()` 존재 확인** — 없으면 추가 (B.12, 결정 4). (d) FSMTickSystem 자유함수 | Player 가 WASD 로 XZ 평면 이동, 카메라 follow. `SJH::FSM` 단위 테스트 (transit AND/XOR 판정) 통과 |
| **M3** | Box2D v2.4.1 통합 (Client 한정) | `myapp::PhysicsBodyComponent`, `myapp::PhysicsSystem`, b2World, SyncToTransform. 벽 4개 정적 body. 충돌 시 정지 | Player 가 벽에서 멈춤, 두 dynamic body 가 부딪힘 |
| **M3.5** | **SJH::playable + SJH::sprite_sequence 모듈 신설** | (a) `src/playable/` — `Playable` 추상 (Play/Pause/Tick/IsFinished) + `SequencePlayable` + `ParallelPlayable` + `PlayablePlayerComponent` + `PlayableTickSystem`. game_deps 의존 0. (b) `src/sprite_sequence/` — `SpriteFrameClip` POD + `SpriteSequencePlayable : Playable`. (c) 단위 테스트 — Sequence(child1, child2) tick 시 cursor 진행, Parallel(child1, child2) 동시 진행, loop SpriteSequencePlayable IsFinished=false 영구. (d) (단독 단발성) PlayerActor 가 단일 SpriteSequencePlayable 을 PlayablePlayerComponent.root 로 set 후 Tick (M4 의 PlayerStateMachine 도입 전 임시) | Sequence + Parallel + SpriteSequencePlayable 단위 테스트 통과. 단발 빌보드 애니 1개 동작 |
| **M4** | **PlayerStateMachine container 패턴** + 발사 + 적 + **Spawn cost 측정** | (a) Client `myapp::PlayerStateMachine` (PlayerIdleState/MoveState/AttackState/DieState enum) — `SJH::FSM` 상속 + `unordered_map<PlayerState, unique_ptr<Playable>>` container 보유. 생성자에서 Idle/Move/Attack/Die 4개 Playable 등록 + Bind 호출. OnEnter 가 active 교체 (Pause/Play). OnUpdate 가 active.Tick + Attack 자연 종료 시 자동 ToIdle. **PlayablePlayerComponent 미부착**. (b) 마우스 클릭 발사 (Bullet Actor + b2 dynamic + ray cast). (c) 적 1종 + 간단 AI. (d) **Bullet spawn cost 측정 — chrono 로 frame 당 Bullet new/destroy 비용 격리. > 0.5ms/프레임 시 풀 도입 결정** | Player Idle/Move/Attack 애니 자동 swap (heap 0 — 모든 Playable 영구 보유), 총알 발사, 적 hit 시 Health 감소. 측정 결과 spec 회고에 기록 → 풀 도입 여부 결정 |
| **M5** | **Client EffekseerPlayable + FmodPlayable + Tweeny TweenPlayable** + 파티클/효과 | (a) `myapp::EffekseerPlayable : SJH::Playable::Playable` — Effekseer Handle 보유, Process 에서 active 체크. (b) `myapp::FmodPlayable : SJH::Playable::Playable` — FMOD::Channel 보유, Process 에서 isPlaying 체크. (c) (선택) `myapp::TweenPlayable : SJH::Playable::Playable` — Tweeny tween wrap. (d) 카메라 shake (`myapp::ShakeSystem` + Tweeny). (e) 데미지 넘버 부상 | 피격 hit spark 파티클 + camera shake 동작. 단독 leaf Playable 들 동작 |
| **M6** | **Sequencing 빌더 + 시퀀스 자산화** + FMOD 사운드 본격 통합 | (a) AudioSystem (Core C API) listener/source 셋업. (b) **첫 시퀀스 — 발사 시 `Sequence().Then(EffekseerPlayable(muzzle)).Then(Parallel().Join(FmodPlayable("gun_shot")).Join(SpriteSequencePlayable(attackClip)))`** 식 빌더. (c) 적 피격 시 시퀀스 (hit spark + 피격음 + Hit 애니). (d) BGM = root level Parallel (loop 무한). (e) 시퀀스 5~10개를 Client 안 `myapp::sequences::*.cpp` 자유함수로 정리 | 발사음/피격음 + 시각 효과 + 애니 동시 발생. 시퀀스가 *코드로* 정의되어 readability OK |
| **M7** | GameFSM (글로벌 FSM = Scene root Actor 부착) + 보스 + 종료 흐름 | `myapp::GameFSM` (`SJH::FSM::StateMachineProcessor` 상속) 를 `scene_.Root().Add<GameFSM>(&scene_)` 부착. Startup → Combat (적 wave) → BossCombat (보스용 별도 atlas 파일 — 128×128 `boss_atlas.png` 신규 UniformAtlas 인스턴스) → End (재시작). FSMTickSystem 이 root Actor 도 자동 Tick | 한 판 완주 가능, 보스 격파 후 End 화면. GameFSM 의 OnEnter/OnUpdate 가 wave spawn / boss spawn / end UI 트리거 |

각 마일스톤은 **별도 spec 으로 분화 가능** (예: `<doc>/superpowers/specs/2026-XX-XX-topdown-shooter-M3-physics-design.md`) — 본 spec 은 전체 비전, 마일스톤 spec 은 *구현 단계의 plan*.

---

## 변경 기록

| 일자 | 변경 | 작성 |
|---|---|---|
| 2026-05-24 | M0 — 본 spec 초안 (원본 IMPLEMENTATION_GUIDE.md 19개 결정으로 본 저장소 정책에 정합화) | Claude (DogGuyMan 결정 19건 반영) |
| 2026-05-24 | 정정 — **`SJH::sprite` 모듈 신설** (등간격 atlas + Sprite 애니메이션을 Client → SJH 코어로 승격). stb 두 매크로 정의 책임 흡수. AnimationSystem 의 사운드 트리거는 SoundCallback 람다 주입으로 game_deps 의존 회피. ObjectPool / FSM 모듈화는 후속 결정 보류 | DogGuyMan 정정 |
| 2026-05-24 | 확정 — **`SJH::fsm` 모듈 신설** (`StateMachineProcessor<TState, TTransit>` template, uint64_t 비트 인코딩, 사용자 정의 클래스 상속 가상함수, Bind 로 transit→target 매핑). **CharacterFSM = per-entity Component, GameFSM = Scene root Actor Component** (두 FSM 메커니즘 동일). **ObjectPool = M4 측정 후 결정** (M1~M3 깡 spawn). **Actor::Instantiate/Clone 도입 안 함** (factory 자유함수 영구). **Effekseer/Tweeny wrapper = Client 한정** (코어 game_deps 흡수 회피). | DogGuyMan 확정 — 4건 |
| 2026-05-24 | 큰 정정 — **`SJH::playable` + `SJH::sprite_sequence` 모듈 신설** (Unity Playable 정통, Composite 패턴). **Mixer/blending 폐기**, Play/Pause/Tick + IsFinished 4-method. SequencePlayable (then) + ParallelPlayable (join) 임의 깊이 트리. **`SJH::sprite` 슬림화** — SpriteComponent + UniformAtlas 만, Animation/PackedAtlas 모두 이탈. **stb_rect_pack 도입 취소** (등간격 N×M 만, 가변 크기는 atlas 파일 분리). **SoundCallback 람다 폐기** — sound 트리거는 Composite Playable (Sequence/Parallel) 로 표현. **시퀀싱 = C++ immediate-mode builder**. 모듈 카운트 14 → 16. | DogGuyMan 확정 — Playable 모델 전면 도입 |
| 2026-05-24 | 명명 정정 + 패턴 확정 — (1) `AnimationPlayable` → **`SpriteSequencePlayable`** ("animation" 모호함 정정 — *sprite frame 시퀀스* 의미 명확화). (2) `SJH::sprite_animation` → **`SJH::sprite_sequence`** (모듈명 클래스명 일치). (3) `AnimationClip` → **`SpriteFrameClip`**. (4) **`AnimationStateComponent` 완전 폐기** — Playable 자체가 elapsed/finished 보유 (단일 진실). (5) **PlayerStateMachine container 패턴 확정** — StateMachine 파생 클래스가 `unordered_map<TState, unique_ptr<Playable>>` container + active 보유. OnEnter 가 Pause/Play 교체, OnUpdate 가 active.Tick + 자기 전이, OnExit 가 Pause. 상태 기반 Actor 는 *StateMachine 만* 부착 (PlayablePlayerComponent 와 양자택일, 부록 B.18). 단발성 Actor (Bullet/Effect) 가 PlayablePlayerComponent. heap 할당 0 — 모든 Playable 영구 보유. | DogGuyMan 확정 — 4건 명명 + container 패턴 |
| 2026-05-24 | M1 정착 — 사용자 발견 후 정정 — **stb_image 단일 owner = `SJH::src/texture/image.cpp`** 으로 재정정 (spec §B.5). UniformAtlas 가 `SJH::Image::Load + SJH::Texture::CreateTexture` 위임 → stb 직접 호출 0, 중복 정의 폭발 자동 회피, test_texture link regression 자동 해결. 셰이더/main.cpp 컨벤션 정정 — GLSL uniform `u_x` → `uX` (lighting.vs/.fs 일치), `uBillboardCenter/Size` 제거 → `uModel` 흡수 (Transform.Translate=center, Scale=size). main.cpp 전면 재작성 — Director + SceneRenderer + Material + MeshRenderer 패턴, 직접 GL 호출 0 (migrate_demo / tweeny_demo 정통). | DogGuyMan 정정 — M1 사후 정착 |
