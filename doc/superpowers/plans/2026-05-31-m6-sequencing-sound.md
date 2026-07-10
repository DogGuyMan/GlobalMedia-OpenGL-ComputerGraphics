# M6 Sequencing 빌더 + 사운드 본격 — Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **정본 spec**: [`doc/superpowers/specs/2026-05-31-m6-sequencing-sound-design.md`](../specs/2026-05-31-m6-sequencing-sound-design.md)

**Goal:** M5 leaf Playable(FMOD/Effekseer/Tweeny) + Composite를 실제 게임플레이 행동(발사·피격·대시·사망·BGM)에 결합하고, 시퀀스를 `Spawns/` 자유함수로 자산화하며, main.cpp의 임시 인라인 시퀀스를 제거한다.

**Architecture:** Ports & Adapters — `IPlayable`(port) ← leaf adapters(FMOD/Effekseer/Tweeny). 재사용 슬롯 = 플레이어 자식 Actor의 Composite Component(Scene 자동 tick), 단발 = FX 컨테이너 child + `AutoDespawnOnFinish` + 매 프레임 지연 sweep(Cocos auto-cleanup 정통). 빌더는 명시적 `SequenceContext` 주입. 신규 `MyApp::Spawns` lib는 `MyApp::Entity`에 **단방향** 의존(순환 회피: Entity 소비자는 `std::function` FX delegate 사용 — 기존 `BulletFactory` 정통).

**Tech Stack:** C++17, CMake(Ninja/MSVC), FMOD Core+Studio, Effekseer, Tweeny, Box2D, `SJH::engine`(playable/sprite/scene), spdlog.

---

## ⚠ 검증 방식 (no_auto_tests 정책)

본 프로젝트는 메모리 `[[no_auto_tests]]` 정책상 **단위 테스트를 자발 추가하지 않는다.** writing-plans 스킬의 TDD(red-green) 사이클 대신 각 Task는 **(1) 빌드 성공(link) + (2) 데모 실행 시각/로그 검증**으로 검증한다. 빌드 명령은 전 Task 공통:

```bash
cmake --build --preset ninja --target _MyApp_
```
실행:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
(최초 1회 또는 CMake 파일 변경 시 `cmake --preset ninja` 재configure 필요.)

**커밋은 사용자 트리거 blocking step** — 각 Task 끝의 커밋 step은 *사용자에게 커밋 여부를 묻고 승인 시에만* 실행한다 (프로젝트 git 규칙).

---

## 전제조건 (구현 진입 전 확인)

- [ ] **P0 — ParticleStage 배선 확인 (이미 완료된 것으로 보임).** spec §9.1은 ParticleStage `mStages` 배선을 0순위 전제조건으로 명시했으나, **working tree `main.cpp:132-147`에 이미 배선됨**([worldCam, ParticleStage, screenCam, ScreenQuadStage]) + `VFX().Draw()` 직접 호출 없음. 빌드+실행으로 Effekseer 파티클이 PostFX 경로를 타는지 시각 확인. **revert/미배선 시에만** [`2026-05-27-particle-stage-design.md`](../specs/2026-05-27-particle-stage-design.md) §4.5 적용. (이미 배선됐다면 별도 Task 불요.)

---

## 파일 구조 맵

### 신규 lib: `MyApp::Spawns` (`apps/_MyApp_/src/Spawns/`)
| 파일 | 책임 |
|---|---|
| `SequenceContext.h` | `struct SequenceContext{ AudioSystem*; VFXSystem*; ResourceRegistry*; b2World*; SJH::Scene::Actor* sceneRoot; SJH::Scene::Actor* fxRoot; }` POD |
| `AutoDespawnOnFinish.h` | Component — 감시 `IPlayable*`가 `IsFinished()`면 `mDone=true` |
| `OneShotSweeper.{h,cpp}` | `SweepFinishedChildren(Actor& fxParent)` 자유함수 |
| `CombatSequences.{h,cpp}` | 단발 `SpawnHitSpark`/`SpawnEnemyDeathFX`/`SpawnPickupChime` |
| `AmbientSequences.{h,cpp}` | `BuildBGM` |
| `PlayerSequences.{h,cpp}` | 슬롯 `BuildPlayerAttack`/`Hit`/`Dash`/`Die`/`MoveEffect` |
| `PlayerFactory.{h,cpp}` | `CreatePlayerActorFull(config, ctx)` — 플레이어 composition root |
| `CMakeLists.txt` | `myapp_spawns` STATIC + `MyApp::Spawns` ALIAS |

### 수정
| 파일 | 변경 |
|---|---|
| `apps/_MyApp_/src/Audio/AudioSystem.{h,cpp}` | `SetListener(pos,fwd,up)` 추가 |
| `apps/_MyApp_/src/Audio/FmodPlayable.{h,cpp}` | ctor `std::optional<vmath::vec3> worldPos` |
| `apps/_MyApp_/src/Audio/FmodStudioPlayable.{h,cpp}` | ctor `std::optional<vmath::vec3> worldPos` |
| `apps/_MyApp_/src/Entity/Bullet/BulletContactHandler.{h,cpp}` | `std::function<void(vmath::vec3)> mOnHitFx` delegate |
| `apps/_MyApp_/src/Entity/Enemy/EnemyDeathHandler.{h,cpp}` | **신규** Component (self-poll Life + despawn + death FX delegate) |
| `<apps>/_MyApp_/src/Entity/Enemy/enemy_factory.h` | `EnemyDeathHandler` 부착 + delegate config |
| `apps/_MyApp_/src/Entity/Player/PlayerActor.h` | `PlayerActorConfig`에 `SpriteCfg`/`BehaviorCfg` 추가 |
| `apps/_MyApp_/src/Entity/CMakeLists.txt` | `EnemyDeathHandler.cpp` 추가 + `MyApp::Audio/VFX/Tween` link |
| `apps/_MyApp_/src/CMakeLists.txt` | `add_subdirectory(Spawns)` + 우산에 `MyApp::Spawns` |
| `apps/_MyApp_/main.cpp` | fxRoot + sweep + listener + BGM 이관 + 입력 트리거 + 인라인 제거 |

---

## Task 1: Spawns lib 골격 + SequenceContext + 단발 정리 인프라

**Files:**
- Create: `apps/_MyApp_/src/Spawns/SequenceContext.h`
- Create: `apps/_MyApp_/src/Spawns/AutoDespawnOnFinish.h`
- Create: `apps/_MyApp_/src/Spawns/OneShotSweeper.h`
- Create: `apps/_MyApp_/src/Spawns/OneShotSweeper.cpp`
- Create: `apps/_MyApp_/src/Spawns/CMakeLists.txt`
- Modify: `apps/_MyApp_/src/CMakeLists.txt`

- [ ] **Step 1: SequenceContext.h 작성**

```cpp
#ifndef __TOPDOWNSHOOTER_SPAWNS_SEQUENCE_CONTEXT_H__
#define __TOPDOWNSHOOTER_SPAWNS_SEQUENCE_CONTEXT_H__

// fwd — 모두 포인터 보유라 전방 선언으로 충분
namespace SJH { class ResourceRegistry; }
namespace SJH::Scene { class Actor; }
namespace TopdownShooter::Audio { class AudioSystem; }
namespace TopdownShooter::VFX   { class VFXSystem; }
class b2World;

namespace TopdownShooter::Spawns
{
    /// @brief 시퀀스 빌더가 필요로 하는 의존 묶음 (명시적 주입 — Q2).
    ///        Manager::Get() + ResourceRegistry::Get() 으로 호출 측이 1회 조립.
    struct SequenceContext
    {
        Audio::AudioSystem*  audio    = nullptr;
        VFX::VFXSystem*      vfx      = nullptr;
        SJH::ResourceRegistry* reg    = nullptr;
        b2World*             world    = nullptr;  // bullet spawn 용
        SJH::Scene::Actor*   sceneRoot = nullptr; // bullet/enemy 부모
        SJH::Scene::Actor*   fxRoot   = nullptr;  // 단발 FX 부모 (sweep 대상)
    };
}

#endif // __TOPDOWNSHOOTER_SPAWNS_SEQUENCE_CONTEXT_H__
```

- [ ] **Step 2: AutoDespawnOnFinish.h 작성**

```cpp
#ifndef __TOPDOWNSHOOTER_SPAWNS_AUTO_DESPAWN_ON_FINISH_H__
#define __TOPDOWNSHOOTER_SPAWNS_AUTO_DESPAWN_ON_FINISH_H__

#include "playable/iplayable.h"
#include "scene/actor.h"

namespace TopdownShooter::Spawns
{
    /// @brief 감시 Composite(IPlayable*)가 IsFinished() 면 mDone=true 마킹.
    ///        실제 RemoveChild 는 SweepFinishedChildren 가 Update 밖에서 수행 (반복자 무효화 회피).
    class AutoDespawnOnFinish : public SJH::Scene::Component
    {
      public:
        explicit AutoDespawnOnFinish(SJH::Playable::IPlayable* watched) : mWatched(watched) {}

        bool IsDone() const { return mDone; }

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float /*dt*/) override
        {
            if (!mDone && mWatched && mWatched->IsFinished())
                mDone = true;
        }

      private:
        SJH::Playable::IPlayable* mWatched = nullptr;
        bool                      mDone    = false;
    };
}

#endif // __TOPDOWNSHOOTER_SPAWNS_AUTO_DESPAWN_ON_FINISH_H__
```

- [ ] **Step 3: OneShotSweeper.h 작성**

```cpp
#ifndef __TOPDOWNSHOOTER_SPAWNS_ONE_SHOT_SWEEPER_H__
#define __TOPDOWNSHOOTER_SPAWNS_ONE_SHOT_SWEEPER_H__

namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Spawns
{
    /// @brief fxParent 자식 중 AutoDespawnOnFinish.IsDone() 인 child 를 RemoveChild.
    ///        매 프레임 씬 Update *직후* 1회 호출 (Cocos end-of-frame cleanup 정통).
    void SweepFinishedChildren(SJH::Scene::Actor& fxParent);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_ONE_SHOT_SWEEPER_H__
```

- [ ] **Step 4: OneShotSweeper.cpp 작성**

```cpp
#include "apps/_MyApp_/src/Spawns/OneShotSweeper.h"
#include "apps/_MyApp_/src/Spawns/AutoDespawnOnFinish.h"
#include "scene/actor.h"

namespace TopdownShooter::Spawns
{
    void SweepFinishedChildren(SJH::Scene::Actor& fxParent)
    {
        // 완료(IsDone) 단발 FX 자식을 찾아 파괴. Actor::FindChildIf (술어 탐색, 2026-05-31 추가) 활용.
        // - 단발 FX 는 *파괴* 대상 → RemoveChild (OnExit 후 erase). leaf dtor 가 FMOD/Effekseer 자원 정리.
        //   (보존+재부착이 필요하면 DetachChild 지만, 여기선 폐기이므로 RemoveChild 가 정확.)
        // - FindChildIf 는 매 호출 새 스캔 → RemoveChild 직후 반복자 무효화 문제 없음 (live iterator 미보유).
        const auto isDone = [](const SJH::Scene::Actor* a) {
            auto* ad = a->GetComponent<AutoDespawnOnFinish>();
            return ad && ad->IsDone();
        };
        while (SJH::Scene::Actor* done = fxParent.FindChildIf(isDone))
            fxParent.RemoveChild(done);
    }
}
```
> **엔진 전제 (2026-05-31 추가됨)**: `SJH::Scene::Actor` 에 트리 탐색/소유권 이전 API 3종이 추가됨 — `DetachChild(Actor*)→unique_ptr`(파괴 없이 분리, Godot `remove_child` 정통), `FindChild(name, recursive)`, `FindChildIf(Fn, recursive)`(술어 탐색). `OneShotSweeper` 는 `FindChildIf` 를 사용. `src/scene/actor.{h,cpp}` 에 이미 구현됨.

- [ ] **Step 5: Spawns/CMakeLists.txt 작성**

```cmake
add_library(myapp_spawns STATIC
    OneShotSweeper.cpp
)
add_library(MyApp::Spawns ALIAS myapp_spawns)

target_include_directories(myapp_spawns
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# SJH::scene/playable — Actor/Component/IPlayable 헤더 노출.
# MyApp::Entity/Audio/VFX/Tween — 후속 Task 의 빌더가 사용 (지금은 OneShotSweeper 만이라 scene 만 필수지만 미리 선언).
target_link_libraries(myapp_spawns
    PUBLIC
        SJH::scene
        SJH::playable
        MyApp::Entity
        MyApp::Audio
        MyApp::VFX
        MyApp::Tween
    PRIVATE
        game_deps
)
target_compile_features(myapp_spawns PUBLIC cxx_std_17)
```

- [ ] **Step 6: src/CMakeLists.txt 에 Spawns 추가**

`apps/_MyApp_/src/CMakeLists.txt` 의 `add_subdirectory(Tween)` 다음 줄에 추가:
```cmake
add_subdirectory(Spawns)
```
그리고 `target_link_libraries(myapp_client INTERFACE ...)` 목록에 추가:
```cmake
    MyApp::Spawns
```

- [ ] **Step 7: 빌드 검증**

Run: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_`
Expected: `Linking CXX executable apps/_MyApp_/_MyApp_` 성공 (신규 lib 컴파일, 동작 변화 없음).

- [ ] **Step 8: 커밋 (사용자 승인 후)**

사용자에게 커밋 여부 확인 후:
```bash
git add apps/_MyApp_/src/Spawns apps/_MyApp_/src/CMakeLists.txt
git commit -m "feat(_MyApp_/M6): Spawns lib 골격 — SequenceContext + AutoDespawnOnFinish + OneShotSweeper"
```

---

## Task 2: fxRoot 컨테이너 + 매 프레임 sweep 배선

**Files:**
- Modify: `apps/_MyApp_/main.cpp` (멤버 추가 + startup + render)

- [ ] **Step 1: 멤버 + include 추가**

`main.cpp` 상단 include 군에 추가:
```cpp
#include "apps/_MyApp_/src/Spawns/SequenceContext.h"
#include "apps/_MyApp_/src/Spawns/OneShotSweeper.h"
```
멤버 영역(`SJH::Scene::Actor* mSpriteActor` 근처)에 추가:
```cpp
SJH::Scene::Actor* mFxRoot = nullptr; // 단발 시퀀스 전용 부모 (sweep 대상)
```

- [ ] **Step 2: startup 에서 fxRoot 생성**

`startup()` 의 `dir.Enter();` **직전**에 추가 (dir = `SJH::Scene::Director::Get()`):
```cpp
mFxRoot = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("FxRoot"));
```

- [ ] **Step 3: render() 에 sweep 호출**

`render()` 의 `SJH::Scene::Director::Get().Update(dt);`([main.cpp:189]) **직후**에 추가:
```cpp
if (mFxRoot) TopdownShooter::Spawns::SweepFinishedChildren(*mFxRoot);
```

- [ ] **Step 4: shutdown 정리**

`shutdown()` 의 멤버 nullptr 군에 추가:
```cpp
mFxRoot = nullptr;
```

- [ ] **Step 5: 빌드 + 실행 검증**

Run: `cmake --build --preset ninja --target _MyApp_` → 성공.
실행 후 기존과 동일 동작(FxRoot 비어 있음, sweep no-op). 크래시/렌더 회귀 없음 확인.

- [ ] **Step 6: 커밋 (사용자 승인 후)**

```bash
git add apps/_MyApp_/main.cpp
git commit -m "feat(_MyApp_/M6): fxRoot 컨테이너 + 매 프레임 SweepFinishedChildren 배선"
```

---

## Task 3: 2D 공간오디오 — AudioSystem::SetListener + leaf worldPos

**Files:**
- Modify: `apps/_MyApp_/src/Audio/AudioSystem.{h,cpp}`
- Modify: `apps/_MyApp_/src/Audio/FmodPlayable.{h,cpp}`
- Modify: `apps/_MyApp_/src/Audio/FmodStudioPlayable.{h,cpp}`
- Modify: `apps/_MyApp_/main.cpp` (render 에서 listener 갱신)

- [ ] **Step 1: AudioSystem.h 에 SetListener 선언**

`#include <vmath.h>` 추가 후, public 메서드에:
```cpp
/// @brief listener 위치/방향 갱신 — render 마다 카메라 Transform 으로 호출 (velocity=0 → doppler 없음).
void SetListener(const vmath::vec3 &pos,
                 const vmath::vec3 &forward = vmath::vec3(0.0f, 0.0f, -1.0f),
                 const vmath::vec3 &up      = vmath::vec3(0.0f, 1.0f, 0.0f));
```

- [ ] **Step 2: AudioSystem.cpp 에 SetListener 구현**

```cpp
#include <fmod/fmod.hpp>
#include <fmod/fmod_studio.hpp>
// ...
void AudioSystem::SetListener(const vmath::vec3 &pos, const vmath::vec3 &forward, const vmath::vec3 &up)
{
    FMOD_3D_ATTRIBUTES attr = {};
    attr.position = { pos[0], pos[1], pos[2] };
    attr.velocity = { 0.0f, 0.0f, 0.0f };
    attr.forward  = { forward[0], forward[1], forward[2] };
    attr.up       = { up[0], up[1], up[2] };
    if (mStudioSystem) mStudioSystem->setListenerAttributes(0, &attr);

    if (mSystem)
    {
        FMOD_VECTOR p = { pos[0], pos[1], pos[2] };
        FMOD_VECTOR v = { 0.0f, 0.0f, 0.0f };
        FMOD_VECTOR f = { forward[0], forward[1], forward[2] };
        FMOD_VECTOR u = { up[0], up[1], up[2] };
        mSystem->set3DListenerAttributes(0, &p, &v, &f, &u);
    }
}
```
> FMOD 호출 시그니처는 `doc/api/FMODAPI.md` 와 기존 `AudioSystem.cpp` 의 include 패턴 준수.

- [ ] **Step 3: FmodPlayable worldPos 확장**

`FmodPlayable.h` — `#include <optional>` + `#include <vmath.h>` 추가, ctor 변경 + 멤버:
```cpp
FmodPlayable(::FMOD::System *sys, SJH::Sound *sound,
             std::optional<vmath::vec3> worldPos = std::nullopt);
// private:
std::optional<vmath::vec3> mWorldPos;
```
`FmodPlayable.cpp` — ctor 에 `mWorldPos(worldPos)` 추가, `OnPlay()` 끝에:
```cpp
void FmodPlayable::OnPlay()
{
    if (!mSys || !mSound) return;
    mSys->playSound(mSound->Raw(), nullptr, /*paused=*/false, &mChannel);
    if (mChannel && mWorldPos)
    {
        mChannel->setMode(FMOD_3D);                  // 채널 단위 3D 오버라이드 (Sound 는 FMOD_DEFAULT 로 생성됨)
        FMOD_VECTOR p = { (*mWorldPos)[0], (*mWorldPos)[1], (*mWorldPos)[2] };
        FMOD_VECTOR v = { 0.0f, 0.0f, 0.0f };
        mChannel->set3DAttributes(&p, &v);
    }
}
```

- [ ] **Step 4: FmodStudioPlayable worldPos 확장**

`FmodStudioPlayable.h` — `#include <optional>` + `#include <vmath.h>`, ctor + 멤버:
```cpp
explicit FmodStudioPlayable(::FMOD::Studio::EventDescription *desc,
                            std::optional<vmath::vec3> worldPos = std::nullopt);
// private:
std::optional<vmath::vec3> mWorldPos;
```
`FmodStudioPlayable.cpp` — ctor 에 `mWorldPos(worldPos)`, `OnPlay()` 의 start 전에:
```cpp
void FmodStudioPlayable::OnPlay()
{
    if (!mDesc) return;
    mDesc->createInstance(&mInstance);
    if (mInstance)
    {
        if (mWorldPos)
        {
            FMOD_3D_ATTRIBUTES attr = {};
            attr.position = { (*mWorldPos)[0], (*mWorldPos)[1], (*mWorldPos)[2] };
            attr.forward  = { 0.0f, 0.0f, -1.0f };
            attr.up       = { 0.0f, 1.0f, 0.0f };
            mInstance->set3DAttributes(&attr);
        }
        mInstance->start();
    }
}
```

- [ ] **Step 5: main.cpp render 에서 listener 갱신**

`render()` 의 `TopdownShooter::Manager::Get().Update(dt);`([main.cpp:188]) **직전**에 추가 (카메라 = listener=카메라, Q5):
```cpp
if (mCamera && mCamera->GetOwner())
{
    const vmath::vec3 camPos = mCamera->GetOwner()->GetTransform().Translate;
    TopdownShooter::Manager::Get().Audio().SetListener(camPos);
}
```

- [ ] **Step 6: 빌드 + 실행 검증**

Run: `cmake --build --preset ninja --target _MyApp_` → 성공.
실행: 기존 BGM/G키/좌클릭 사운드가 그대로 들림(2D, worldPos 미전달). 회귀 없음.

- [ ] **Step 7: 커밋 (사용자 승인 후)**

```bash
git add apps/_MyApp_/src/Audio apps/_MyApp_/main.cpp
git commit -m "feat(_MyApp_/M6): 2D 공간오디오 — AudioSystem::SetListener + FmodPlayable/FmodStudioPlayable worldPos"
```

---

## Task 4: 단발 빌더 — CombatSequences + AmbientSequences

**Files:**
- Create: `apps/_MyApp_/src/Spawns/CombatSequences.{h,cpp}`
- Create: `apps/_MyApp_/src/Spawns/AmbientSequences.{h,cpp}`
- Modify: `apps/_MyApp_/src/Spawns/CMakeLists.txt`

- [ ] **Step 1: CombatSequences.h**

```cpp
#ifndef __TOPDOWNSHOOTER_SPAWNS_COMBAT_SEQUENCES_H__
#define __TOPDOWNSHOOTER_SPAWNS_COMBAT_SEQUENCES_H__

#include "apps/_MyApp_/src/Spawns/SequenceContext.h"
#include <vmath.h>

namespace TopdownShooter::Spawns
{
    /// @brief 적 피격 임팩트 — Parallel(Effekseer spark ∥ Fmod "hit"@pos). fxRoot 밑 child + AutoDespawn.
    void SpawnHitSpark(const SequenceContext& ctx, const vmath::vec3& worldPos);

    /// @brief 적 사망 — Parallel(Effekseer explosion ∥ FmodStudio "EnemyDeath"@pos).
    void SpawnEnemyDeathFX(const SequenceContext& ctx, const vmath::vec3& worldPos);

    /// @brief 픽업 — Fmod "pickup"@pos (선택).
    void SpawnPickupChime(const SequenceContext& ctx, const vmath::vec3& worldPos);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_COMBAT_SEQUENCES_H__
```

- [ ] **Step 2: CombatSequences.cpp**

```cpp
#include "apps/_MyApp_/src/Spawns/CombatSequences.h"
#include "apps/_MyApp_/src/Spawns/AutoDespawnOnFinish.h"
#include "apps/_MyApp_/src/Audio/AudioSystem.h"
#include "apps/_MyApp_/src/Audio/FmodPlayable.h"
#include "apps/_MyApp_/src/Audio/FmodStudioPlayable.h"
#include "apps/_MyApp_/src/VFX/VFXSystem.h"
#include "apps/_MyApp_/src/VFX/EffekseerPlayable.h"
#include "playable/composite_playable.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include <memory>

namespace TopdownShooter::Spawns
{
    namespace
    {
        // 공통 — fxRoot 밑에 child Actor 생성 + Parallel Composite 부착 + AutoDespawn + Play.
        SJH::Playable::ParallelPlayable* SpawnParallelChild(const SequenceContext& ctx, const char* name)
        {
            auto* child = ctx.fxRoot->AddChild(std::make_unique<SJH::Scene::Actor>(name));
            auto* par   = child->AddComponent<SJH::Playable::ParallelPlayable>();
            child->AddComponent<AutoDespawnOnFinish>(par);
            return par; // 호출 측이 Join 후 Play
        }
    }

    void SpawnHitSpark(const SequenceContext& ctx, const vmath::vec3& worldPos)
    {
        auto* spark = ctx.reg->FindEffect("spark");
        auto* hit   = ctx.reg->FindSound("hit");
        auto* par   = SpawnParallelChild(ctx, "HitSpark");
        if (spark)
            par->Join(std::make_unique<VFX::EffekseerPlayable>(
                ctx.vfx->GetManager(), spark, worldPos, VFX::TrackPolicy::Static));
        if (hit)
            par->Join(std::make_unique<Audio::FmodPlayable>(ctx.audio->GetSystem(), hit, worldPos));
        par->Play();
    }

    void SpawnEnemyDeathFX(const SequenceContext& ctx, const vmath::vec3& worldPos)
    {
        auto* boom = ctx.reg->FindEffect("explosion");
        auto* evt  = ctx.audio->LoadEvent("event:/EnemyDeath");
        auto* par  = SpawnParallelChild(ctx, "EnemyDeathFX");
        if (boom)
            par->Join(std::make_unique<VFX::EffekseerPlayable>(
                ctx.vfx->GetManager(), boom, worldPos, VFX::TrackPolicy::Static));
        if (evt)
            par->Join(std::make_unique<Audio::FmodStudioPlayable>(evt, worldPos));
        par->Play();
    }

    void SpawnPickupChime(const SequenceContext& ctx, const vmath::vec3& worldPos)
    {
        auto* snd = ctx.reg->FindSound("pickup");
        auto* par = SpawnParallelChild(ctx, "PickupChime");
        if (snd)
            par->Join(std::make_unique<Audio::FmodPlayable>(ctx.audio->GetSystem(), snd, worldPos));
        par->Play();
    }
}
```
> 리소스 키("spark"/"explosion"/"hit"/"pickup", `event:/EnemyDeath`)는 startup 의 `reg.CreateSound`/`CreateEffect`/`LoadEvent` 에서 사전 등록 (Task 8 startup 보강 또는 placeholder 로 기존 "muzzle"/"shot" 재사용 가능). 미존재 시 `Find*` 가 nullptr → 해당 Join skip (크래시 없음).

- [ ] **Step 3: AmbientSequences.h / .cpp**

`AmbientSequences.h`:
```cpp
#ifndef __TOPDOWNSHOOTER_SPAWNS_AMBIENT_SEQUENCES_H__
#define __TOPDOWNSHOOTER_SPAWNS_AMBIENT_SEQUENCES_H__

#include "apps/_MyApp_/src/Spawns/SequenceContext.h"

namespace TopdownShooter::Spawns
{
    /// @brief BGM — root-level Parallel(FmodStudio "BGM") loop 무한. sceneRoot 밑 "BgmActor" 에 부착.
    void BuildBGM(const SequenceContext& ctx);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_AMBIENT_SEQUENCES_H__
```
`AmbientSequences.cpp`:
```cpp
#include "apps/_MyApp_/src/Spawns/AmbientSequences.h"
#include "apps/_MyApp_/src/Audio/AudioSystem.h"
#include "apps/_MyApp_/src/Audio/FmodStudioPlayable.h"
#include "playable/composite_playable.h"
#include "scene/actor.h"
#include <memory>

namespace TopdownShooter::Spawns
{
    void BuildBGM(const SequenceContext& ctx)
    {
        auto* evt = ctx.audio->LoadEvent("event:/BGM");
        if (!evt) return;
        auto* bgmActor = ctx.sceneRoot->AddChild(std::make_unique<SJH::Scene::Actor>("BgmActor"));
        auto* par = bgmActor->AddComponent<SJH::Playable::ParallelPlayable>();
        par->Join(std::make_unique<Audio::FmodStudioPlayable>(evt)); // 2D — worldPos 미전달
        par->SetIsLoop(true);
        par->Play();
    }
}
```

- [ ] **Step 4: CMakeLists.txt 갱신**

`Spawns/CMakeLists.txt` 의 `add_library(myapp_spawns STATIC ...)` 에 추가:
```cmake
    CombatSequences.cpp
    AmbientSequences.cpp
```

- [ ] **Step 5: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_` → 성공 (빌더 컴파일, 아직 호출 안 됨).

- [ ] **Step 6: 커밋 (사용자 승인 후)**

```bash
git add apps/_MyApp_/src/Spawns
git commit -m "feat(_MyApp_/M6): 단발 빌더 CombatSequences(HitSpark/DeathFX/Pickup) + AmbientSequences(BGM)"
```

---

## Task 5: BGM 이관 (인라인 제거)

**Files:**
- Modify: `apps/_MyApp_/main.cpp` (`WramupFMOD`)

- [ ] **Step 1: WramupFMOD 의 인라인 BGM 제거 + BuildBGM 호출**

`main.cpp:424-431`([WramupFMOD])의 `auto *bgmEvent = ...` ~ `bgm->Play();` 블록을 삭제하고 다음으로 대체:
```cpp
#include "apps/_MyApp_/src/Spawns/AmbientSequences.h"   // (상단 include)
// ... WramupFMOD 내부, LoadBank 2줄 다음:
TopdownShooter::Spawns::SequenceContext bgmCtx;
bgmCtx.audio     = &audio;
bgmCtx.sceneRoot = &dir.Root();
TopdownShooter::Spawns::BuildBGM(bgmCtx);
```
> `WramupFMOD(dir, reg, audio)` 시그니처는 `dir`=`SJH::Scene::Director&` 보유 — `dir.Root()` 사용 가능.

- [ ] **Step 2: 빌드 + 실행 검증**

Run: `cmake --build --preset ninja --target _MyApp_` → 성공.
실행: **BGM 이 이전과 동일하게 루프 재생됨**. (인라인 → BuildBGM 동치 확인.)

- [ ] **Step 3: 커밋 (사용자 승인 후)**

```bash
git add apps/_MyApp_/main.cpp
git commit -m "refactor(_MyApp_/M6): BGM 인라인 → Spawns::BuildBGM 이관"
```

---

## Task 6: 슬롯 빌더 — PlayerSequences

**Files:**
- Create: `apps/_MyApp_/src/Spawns/PlayerSequences.{h,cpp}`
- Modify: `apps/_MyApp_/src/Spawns/CMakeLists.txt`

- [ ] **Step 1: PlayerSequences.h**

```cpp
#ifndef __TOPDOWNSHOOTER_SPAWNS_PLAYER_SEQUENCES_H__
#define __TOPDOWNSHOOTER_SPAWNS_PLAYER_SEQUENCES_H__

#include "apps/_MyApp_/src/Spawns/SequenceContext.h"
#include "playable/iplayable.h"

namespace SJH::Scene { class Actor; }
namespace TopdownShooter::Entity::Player { class PlayerBehavior; }

namespace TopdownShooter::Spawns
{
    /// @brief 슬롯 빌더 — host(=player Actor) 밑에 전용 child Actor + Composite Component 생성, raw 반환.
    ///        반환값을 pb->Set*Playable() 에 전달. host 소유 + Scene 자동 tick (Q1 Option A).
    SJH::Playable::IPlayable* BuildPlayerAttack(SJH::Scene::Actor& host, const SequenceContext& ctx,
                                                Entity::Player::PlayerBehavior* pb);
    SJH::Playable::IPlayable* BuildPlayerHit (SJH::Scene::Actor& host, const SequenceContext& ctx);
    SJH::Playable::IPlayable* BuildPlayerDash(SJH::Scene::Actor& host, const SequenceContext& ctx);
    SJH::Playable::IPlayable* BuildPlayerDie (SJH::Scene::Actor& host, const SequenceContext& ctx);
    SJH::Playable::IPlayable* BuildPlayerMoveEffect(SJH::Scene::Actor& host, const SequenceContext& ctx);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_PLAYER_SEQUENCES_H__
```

- [ ] **Step 2: PlayerSequences.cpp**

```cpp
#include "<Spawns>/PlayerSequences.h"
#include "apps/_MyApp_/src/Spawns/CombatSequences.h"
#include "apps/_MyApp_/src/Audio/AudioSystem.h"
#include "apps/_MyApp_/src/Audio/FmodPlayable.h"
#include "apps/_MyApp_/src/Audio/FmodStudioPlayable.h"
#include "apps/_MyApp_/src/VFX/VFXSystem.h"
#include "apps/_MyApp_/src/VFX/EffekseerPlayable.h"
#include "apps/_MyApp_/src/Tween/TweenPlayable.h"
#include "<Entity>/Player/PlayerBehavior.h"
#include "<Entity>/Player/BulletSpawnPlayable.h"
#include "apps/_MyApp_/src/Entity/Bullet/bullet_factory.h"
#include "playable/composite_playable.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include <<tweeny>/tweeny.h>
#include <cmath>
#include <memory>

namespace TopdownShooter::Spawns
{
    namespace
    {
        // host 밑 전용 child Actor 에 ParallelPlayable 부착 후 반환.
        SJH::Playable::ParallelPlayable* MakeSlot(SJH::Scene::Actor& host, const char* name)
        {
            auto* child = host.AddChild(std::make_unique<SJH::Scene::Actor>(name));
            return child->AddComponent<SJH::Playable::ParallelPlayable>();
        }
    }

    SJH::Playable::IPlayable* BuildPlayerAttack(SJH::Scene::Actor& host, const SequenceContext& ctx,
                                                Entity::Player::PlayerBehavior* pb)
    {
        auto* par = MakeSlot(host, "Player_AtkSeq");

        // BulletSpawnPlayable — 발사 (BulletFactory delegate 로 Physics 의존 회피, Entity 정통).
        b2World* world = ctx.world;
        SequenceContext fxCtx = ctx; // 람다 캡처용 복사
        Entity::Player::BulletSpawnPlayable::BulletFactory factory =
            [world](vmath::vec2 pos, vmath::vec2 dir) -> std::unique_ptr<SJH::Scene::Actor>
            {
                Entity::Bullet::BulletConfig bc;
                bc.world = world; bc.pos = pos; bc.dir = dir;
                return Entity::Bullet::CreateBulletActor(bc);
            };
        par->Join(std::make_unique<Entity::Player::BulletSpawnPlayable>(pb, ctx.sceneRoot, std::move(factory)));

        // muzzle (Effekseer) + gun_shot (Fmod) — 위치는 플레이어 기준(여기선 단순화: 단발은 spawnPos(0)).
        if (auto* muzzle = ctx.reg->FindEffect("muzzle"))
            par->Join(std::make_unique<VFX::EffekseerPlayable>(
                ctx.vfx->GetManager(), muzzle, vmath::vec3(0.0f), VFX::TrackPolicy::Static));
        if (auto* shot = ctx.reg->FindSound("shot"))
            par->Join(std::make_unique<Audio::FmodPlayable>(ctx.audio->GetSystem(), shot));
        return par;
    }

    SJH::Playable::IPlayable* BuildPlayerHit(SJH::Scene::Actor& host, const SequenceContext& ctx)
    {
        auto* par = MakeSlot(host, "Player_HitSeq");
        // TweenShake — 인라인 G키(main.cpp:255-261) 이관. step(int32_t ms) 강제 (tweeny_step_overload_trap).
        auto tween = tweeny::from(0.0f).to(1.0f).during(100).via(tweeny::easing::sinusoidalInOut);
        par->Join(std::make_unique<Tween::TweenPlayable<float>>(
            std::move(tween),
            [](float v) { float offset = std::sin(v * 8.0f * 3.14159f) * 5.0f; (void)offset; }));
        if (auto* evt = ctx.audio->LoadEvent("event:/Damaged"))
            par->Join(std::make_unique<Audio::FmodStudioPlayable>(evt));
        return par;
    }

    SJH::Playable::IPlayable* BuildPlayerDash(SJH::Scene::Actor& host, const SequenceContext& ctx)
    {
        auto* par = MakeSlot(host, "Player_DashSeq");
        if (auto* trail = ctx.reg->FindEffect("dashTrail"))
            par->Join(std::make_unique<VFX::EffekseerPlayable>(
                ctx.vfx->GetManager(), trail, vmath::vec3(0.0f), VFX::TrackPolicy::Static));
        if (auto* snd = ctx.reg->FindSound("dash"))
            par->Join(std::make_unique<Audio::FmodPlayable>(ctx.audio->GetSystem(), snd));
        return par;
    }

    SJH::Playable::IPlayable* BuildPlayerDie(SJH::Scene::Actor& host, const SequenceContext& ctx)
    {
        auto* par = MakeSlot(host, "Player_DieSeq");
        if (auto* evt = ctx.audio->LoadEvent("event:/Death"))
            par->Join(std::make_unique<Audio::FmodStudioPlayable>(evt));
        return par;
    }

    SJH::Playable::IPlayable* BuildPlayerMoveEffect(SJH::Scene::Actor& host, const SequenceContext& ctx)
    {
        auto* par = MakeSlot(host, "Player_MoveFx");
        if (auto* dust = ctx.reg->FindEffect("footDust"))
            par->Join(std::make_unique<VFX::EffekseerPlayable>(
                ctx.vfx->GetManager(), dust, vmath::vec3(0.0f), VFX::TrackPolicy::FollowOwner));
        par->SetIsLoop(true); // Move 동안 반복 (PlayClipInternal 이 Play/Stop)
        return par;
    }
}
```
> 리소스 키가 미등록이면 `Find*` nullptr → Join skip. dash/footDust/dashTrail 등 미존재 리소스는 placeholder 로 기존 "muzzle"/"shot" 재사용하거나 startup 에서 추가 등록 (Task 8).

- [ ] **Step 3: CMakeLists.txt 갱신**

`Spawns/CMakeLists.txt` 의 STATIC 목록에 추가:
```cmake
    PlayerSequences.cpp
```

- [ ] **Step 4: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_` → 성공.

- [ ] **Step 5: 커밋 (사용자 승인 후)**

```bash
git add apps/_MyApp_/src/Spawns
git commit -m "feat(_MyApp_/M6): 슬롯 빌더 PlayerSequences(Attack/Hit/Dash/Die/MoveEffect)"
```

---

## Task 7: 플레이어 composition root — PlayerFactory + multi-clip + 슬롯 SET

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerActor.h` (config 확장 — base factory 유지)
- Create: `apps/_MyApp_/src/Spawns/PlayerFactory.{h,cpp}`
- Modify: `apps/_MyApp_/src/Spawns/CMakeLists.txt`
- Modify: `apps/_MyApp_/main.cpp` (`WramupPlayer` 가 CreatePlayerActorFull 호출)

- [ ] **Step 1: PlayerActorConfig 확장**

`PlayerActor.h` 의 `struct PlayerActorConfig` 에 nested cfg 추가 (`#include "sprite/uniform_atlas.h"` + `#include "sprite/sprite_frame_clip.h"` + `<vector>` 동반):
```cpp
struct SpriteCfg
{
    SJH::Sprite::UniformAtlas* atlas = nullptr;
    std::vector<SJH::SpriteSequence::SpriteFrameClip> clips; // [Idle, Move, Attack, Hit]
};
struct BehaviorCfg
{
    float normalSpeed      = 3.0f;
    float dashSpeed        = 7.5f;
    float dashDuration     = 0.3f;
    float dashCooldown     = 0.8f;
    float hitInvincibility = 0.5f;
};
SpriteCfg   sprite;
BehaviorCfg behavior;
```
> base `CreatePlayerActor(cfg)` 는 **변경 없음** (Life/Physics/Controller). sprite/behavior 는 PlayerFactory(Spawns) 가 사용.

- [ ] **Step 2: PlayerFactory.h**

```cpp
#ifndef __TOPDOWNSHOOTER_SPAWNS_PLAYER_FACTORY_H__
#define __TOPDOWNSHOOTER_SPAWNS_PLAYER_FACTORY_H__

#include "apps/_MyApp_/src/Entity/Player/PlayerActor.h"
#include "apps/_MyApp_/src/Spawns/SequenceContext.h"
#include "scene/actor.h"
#include <memory>

namespace TopdownShooter::Spawns
{
    /// @brief 플레이어 composition root — base CreatePlayerActor + sprite(multi-clip) + PlayerBehavior + 5슬롯.
    ///        반환된 Actor 를 호출 측이 Root 에 AddChild.
    std::unique_ptr<SJH::Scene::Actor>
    CreatePlayerActorFull(const Entity::Player::PlayerActorConfig& cfg, const SequenceContext& ctx);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_PLAYER_FACTORY_H__
```

- [ ] **Step 3: PlayerFactory.cpp**

```cpp
#include "<Spawns>/PlayerFactory.h"
#include "<Spawns>/PlayerSequences.h"
#include "<Entity>/Player/PlayerBehavior.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_sequence_playable.h"
#include <<box2d>/box2d.h>

namespace TopdownShooter::Spawns
{
    using Entity::Player::PlayerBehavior;
    using SJH::SpriteSequence::SpriteSequencePlayable;
    using SJH::SpriteSequence::SpriteFrameClip;

    std::unique_ptr<SJH::Scene::Actor>
    CreatePlayerActorFull(const Entity::Player::PlayerActorConfig& cfg, const SequenceContext& ctx)
    {
        // 1) base — Life/Physics/Controller
        auto actor = Entity::Player::CreatePlayerActor(cfg);

        // 2) sprite + multi-clip
        auto* sprite = actor->AddComponent<SJH::Sprite::SpriteRenderer>(cfg.sprite.atlas);
        // clips_ 의 lifetime 은 SpriteSequencePlayable 가 const* 만 보유 → cfg.sprite.clips 가 살아있어야 함.
        // 안전을 위해 Actor 수명에 묶인 보관소가 필요 → PlayerBehavior 가 clip 보관 (아래) 또는
        // 정적 보관. 여기서는 atlas 전체 fallback 클립을 기본으로 두고 RegisterClip 로 4종 등록.
        static thread_local std::vector<SpriteFrameClip> sClipStore; // 단일 플레이어 가정 (데모)
        sClipStore = cfg.sprite.clips;
        auto* seq = actor->AddComponent<SpriteSequencePlayable>(
            sprite, sClipStore.empty() ? nullptr : &sClipStore[0]);
        for (int i = 0; i < static_cast<int>(sClipStore.size()); ++i)
            seq->RegisterClip(i, &sClipStore[i]);
        seq->SetIsLoop(true);
        seq->PlayClip(0); // Idle

        // 3) PlayerBehavior
        b2Body* body = nullptr;
        if (auto* box = actor->GetComponent<Physics::Components::BoxBody>()) body = box->GetBody();
        auto* pb = actor->AddComponent<PlayerBehavior>();
        pb->Init(seq, cfg.behavior.normalSpeed, cfg.behavior.dashSpeed,
                 cfg.behavior.dashDuration, cfg.behavior.dashCooldown, cfg.behavior.hitInvincibility);
        pb->SetBody(body);
        pb->SetSceneRoot(ctx.sceneRoot);

        // 4) 5슬롯 — child Actor + Composite, raw 를 슬롯에 SET
        pb->SetAttackPlayable(BuildPlayerAttack(*actor, ctx, pb));
        pb->SetHitPlayable   (BuildPlayerHit(*actor, ctx));
        pb->SetDashPlayable  (BuildPlayerDash(*actor, ctx));
        pb->SetDiePlayable   (BuildPlayerDie(*actor, ctx));
        pb->SetMoveEffect    (BuildPlayerMoveEffect(*actor, ctx));

        return actor;
    }
}
```
> ⚠ **clip lifetime**: `SpriteSequencePlayable` 는 `const SpriteFrameClip*` 만 보유한다. cfg.sprite.clips 가 함수 종료 후 파괴되면 dangling. 구현 시 클립 보관소를 Actor 수명에 묶어야 함(예: PlayerBehavior 멤버에 `std::vector<SpriteFrameClip>` 추가, 또는 ResourceRegistry 위탁). 위 `static thread_local` 은 *단일 플레이어 데모* 한정 임시 — 다중 플레이어/엄밀성 필요 시 PlayerBehavior 멤버로 승격. **이 결정은 구현자가 Task 7 착수 시 확정** (권장: PlayerBehavior 에 clip 보관 벡터 추가).

- [ ] **Step 4: CMakeLists.txt 갱신**

`Spawns/CMakeLists.txt` STATIC 목록에 추가:
```cmake
    PlayerFactory.cpp
```

- [ ] **Step 5: WramupPlayer 가 CreatePlayerActorFull 호출**

`main.cpp` 의 `WramupPlayer`([main.cpp:519-561]) 를 다음으로 교체 (atlas/clip 준비 + ctx 조립 + 호출):
```cpp
#include "<Spawns>/PlayerFactory.h"
// ...
void WramupPlayer(SJH::ResourceRegistry &reg, SJH::Scene::Director &dir, Physics::PhysicsSystem &phys)
{
    auto *atlas = reg.CreateUniformAtlas("test_pattern", "resources/texture/TestPattern.png", 4, 4);
    if (!atlas) { spdlog::error("[M6] atlas load failed"); return; }

    TopdownShooter::Entity::Player::PlayerActorConfig pac;
    pac.name = "PlayerSprite";
    pac.life.hp = 100;
    pac.movement.speed = 3.0f;
    pac.controller.keyboard = &mKeyboard;
    pac.physics.world = &phys.World();
    pac.physics.size = vmath::vec2(1.0f, 1.0f);
    pac.physics.startPosition = vmath::vec2(0.0f, 0.0f);
    pac.physics.density = 1.0f;
    pac.physics.linearDamping = 5.0f;
    pac.physics.categoryBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Player);
    pac.physics.maskBits     = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PlayerMask);
    pac.sprite.atlas = atlas;
    // 4-clip (placeholder — TestPattern 4×4=16 frame 을 4구간으로): Idle/Move/Attack/Hit
    pac.sprite.clips = {
        SJH::SpriteSequence::SpriteFrameClip{0,  4, 4.0f},   // Idle
        SJH::SpriteSequence::SpriteFrameClip{4,  4, 8.0f},   // Move
        SJH::SpriteSequence::SpriteFrameClip{8,  4, 12.0f},  // Attack
        SJH::SpriteSequence::SpriteFrameClip{12, 4, 8.0f},   // Hit
    };
    pac.behavior.normalSpeed = 3.0f;

    TopdownShooter::Spawns::SequenceContext ctx;
    ctx.audio = &TopdownShooter::Manager::Get().Audio();
    ctx.vfx   = &TopdownShooter::Manager::Get().VFX();
    ctx.reg   = &reg;
    ctx.world = &phys.World();
    ctx.sceneRoot = &dir.Root();
    ctx.fxRoot    = mFxRoot;

    auto playerActor = TopdownShooter::Spawns::CreatePlayerActorFull(pac, ctx);
    mSprite = playerActor->GetComponent<SJH::Sprite::SpriteRenderer>();
    mSpriteSeq = playerActor->GetComponent<SJH::SpriteSequence::SpriteSequencePlayable>();
    mSpriteActor = dir.Root().AddChild(std::move(playerActor));
    mCamera->GetOwner()->GetComponent<Controller::TargetFollowableCameraController>()
        ->SetFollowTarget(mSpriteActor)
        .SetFollowOffset(vmath::vec3(0.0f, 10.0f, 10.0f));
}
```
> ⚠ **호출 순서**: `WramupPlayer` 는 `mFxRoot` 가 필요하므로, startup 에서 **mFxRoot 생성(Task 2 Step 2)을 `WramupPlayer` 호출 전**으로 옮긴다 (현재 fxRoot 는 `dir.Enter()` 직전 — `WramupPlayer`(157) 보다 뒤). Task 7 착수 시 mFxRoot 생성을 `WramupSceneRenderer` 직후로 이동.
> `SpriteFrameClip` 의 정확한 필드(`{startFrame, frameCount, fps}`)는 `src/sprite/sprite_frame_clip.h` 확인 후 정합 (현재 main.cpp:551 의 `{0, atlas->FrameCount(), 4.0f}` 패턴 준수).

- [ ] **Step 6: 빌드 + 실행 검증**

Run: `cmake --build --preset ninja --target _MyApp_` → 성공.
실행: 플레이어 표시 + WASD 이동 시 **Move 클립**, 정지 시 **Idle 클립** 자동 전환(PlayerBehavior 속도 기반). 카메라 follow 정상. (Attack/Dash 입력은 Task 8 에서.)

- [ ] **Step 7: 커밋 (사용자 승인 후)**

```bash
git add apps/_MyApp_/src/Spawns apps/_MyApp_/src/Entity/Player/PlayerActor.h apps/_MyApp_/main.cpp
git commit -m "feat(_MyApp_/M6): PlayerFactory composition root — PlayerBehavior 부착 + multi-clip + 5슬롯 SET"
```

---

## Task 8: 입력 트리거 복구 + 인라인 시퀀스 제거

**Files:**
- Modify: `apps/_MyApp_/main.cpp` (`onMouseButton` / `onKey` / 인라인 제거 / startup 리소스 등록)

- [ ] **Step 1: 좌클릭 → PlayerBehavior::Attack (인라인 ShotComposite 제거)**

`onMouseButton`([main.cpp:281-308])의 인라인 ShotComposite 블록 전체를 다음으로 대체:
```cpp
if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
{
    if (mSpriteActor)
    {
        auto* pb = mSpriteActor->GetComponent<TopdownShooter::Entity::Player::PlayerBehavior>();
        if (pb)
        {
            // NDC 방향 → 공격 방향 (기존 발사 방향 산출 패턴 준수)
            double cx = 0.0, cy = 0.0; glfwGetCursorPos(window, &cx, &cy);
            int w = 0, h = 0; glfwGetFramebufferSize(window, &w, &h);
            float ndcX = static_cast<float>(cx / w * 2.0 - 1.0);
            float ndcY = static_cast<float>(1.0 - cy / h * 2.0);
            pb->Attack(vmath::vec2(ndcX, ndcY));
        }
    }
}
```
> `#include "<Entity>/Player/PlayerBehavior.h"` 를 main.cpp 상단에 추가.

- [ ] **Step 2: G키 인라인 DamageComposite 제거**

`onKey`([main.cpp:244-266])의 `// === M5 CO2 ... G 키 ===` 블록 전체 삭제. (피격은 `EnemyContactHandler`→`pb->Hit`→Hit 슬롯이 담당.)

- [ ] **Step 3: Shift → Dash 추가**

`onKey` 의 `mKeyboard.Dispatch(key, action);` 다음에 추가:
```cpp
if ((key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) && action == GLFW_PRESS && mSpriteActor)
{
    auto* pb = mSpriteActor->GetComponent<TopdownShooter::Entity::Player::PlayerBehavior>();
    if (pb)
    {
        // WASD 현재 방향 (없으면 마지막 공격/바라보는 방향 fallback)
        vmath::vec2 dir = pb->GetAttackDirection();
        pb->Dash(dir);
    }
}
```

- [ ] **Step 4: startup 리소스 등록 보강**

`WramupFMOD` 또는 startup 의 `reg.CreateSound(audio.GetSystem(), "shot", "resources/audio/Laser.wav");` 근처에 카탈로그가 참조하는 키를 등록 (없는 .wav/.efk 는 기존 자산 재사용 placeholder):
```cpp
reg.CreateSound(audio.GetSystem(), "hit",  "resources/audio/Laser.wav");   // placeholder
reg.CreateSound(audio.GetSystem(), "dash", "resources/audio/Laser.wav");   // placeholder
reg.CreateEffect(vfxs.GetManager(), "spark",     u"resources/vfx/distortion.efk"); // placeholder
reg.CreateEffect(vfxs.GetManager(), "explosion", u"resources/vfx/distortion.efk"); // placeholder
```
> 실제 전용 자산(hit.wav/explosion.efk 등) 추가는 선택 — 미등록 키는 `Find*` nullptr 로 자연 skip 되므로 빌드/실행은 무관. `event:/EnemyDeath`/`event:/Death` 미존재 시 `LoadEvent` nullptr → skip.

- [ ] **Step 5: 빌드 + 실행 검증**

Run: `cmake --build --preset ninja --target _MyApp_` → 성공.
실행 시각 검증:
- 좌클릭 → **Attack 클립 + 총알 발사**(BulletSpawnPlayable) + muzzle + shot 사운드
- Shift → **Dash**(쿨타임 0.8s) + dash 효과
- (적 접촉 시 Hit 은 Task 9 적 FX 와 함께 확인)

- [ ] **Step 6: 커밋 (사용자 승인 후)**

```bash
git add apps/_MyApp_/main.cpp
git commit -m "feat(_MyApp_/M6): 입력 트리거 복구(좌클릭→Attack/Shift→Dash) + G키·ShotComposite 인라인 제거"
```

---

## Task 9: Enemy 피격/사망 — BulletContactHandler hit FX + EnemyDeathHandler

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Bullet/BulletContactHandler.{h,cpp}`
- Create: `apps/_MyApp_/src/Entity/Enemy/EnemyDeathHandler.{h,cpp}`
- Modify: `<apps>/_MyApp_/src/Entity/Enemy/enemy_factory.h`
- Modify: `apps/_MyApp_/src/Entity/CMakeLists.txt`
- Modify: `<apps>/_MyApp_/src/Spawns/PlayerSequences.cpp` (bullet factory 가 hit FX delegate 바인딩)

- [ ] **Step 1: BulletContactHandler 에 hit FX delegate**

`BulletContactHandler.h` — `#include <functional>` + `<vmath.h>`, 멤버 + setter:
```cpp
using HitFx = std::function<void(const vmath::vec3&)>;
void SetOnHitFx(HitFx fx) { mOnHitFx = std::move(fx); }
// private:
HitFx mOnHitFx;
```
`BulletContactHandler.cpp` — `HandleHit` 의 `if (life) life->DoDamaged(mDamage);` 다음에:
```cpp
if (life && mOnHitFx)
    mOnHitFx(other->GetTransform().Translate); // 임팩트 = 충돌 이벤트 소유 (Q4)
```
> `#include "scene/actor.h"` 는 이미 포함. delegate 는 Entity→Spawns 의존 회피 (BulletFactory 정통).

- [ ] **Step 2: EnemyDeathHandler.h (신규)**

```cpp
#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_DEATH_HANDLER_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_DEATH_HANDLER_H__

#include "scene/actor.h"
#include <functional>
#include <vmath.h>

namespace TopdownShooter::Entity::Enemy
{
    /// @brief 적 사망 self-poll — Life::IsAlive()==false 감지 시 death FX delegate + despawn (Q4).
    ///        PlayerBehavior::Die self-poll 패턴 미러. death 로직은 적(자신)이 소유.
    class EnemyDeathHandler : public SJH::Scene::Component
    {
      public:
        using DeathFx = std::function<void(const vmath::vec3&)>;
        explicit EnemyDeathHandler(DeathFx fx) : mOnDeathFx(std::move(fx)) {}

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;

      private:
        DeathFx mOnDeathFx;
        bool    mDead = false;
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_DEATH_HANDLER_H__
```

- [ ] **Step 3: EnemyDeathHandler.cpp (신규)**

```cpp
#include "<Entity>/Enemy/EnemyDeathHandler.h"
#include "apps/_MyApp_/src/Entity/Components/LifeComponents.h"

namespace TopdownShooter::Entity::Enemy
{
    void EnemyDeathHandler::Update(float /*dt*/)
    {
        if (mDead || !GetOwner()) return;
        auto* life = GetOwner()->GetComponent<Components::Life>();
        if (life && !life->IsAlive())
        {
            mDead = true;
            if (mOnDeathFx) mOnDeathFx(GetOwner()->GetTransform().Translate);
            GetOwner()->SetActive(false); // despawn (Bullet 의 SetActive(false) 정통과 정합)
        }
    }
}
```

- [ ] **Step 4: enemy_factory 에 EnemyDeathHandler 부착**

`enemy_factory.h` — `EnemyConfig` 에 delegate 추가 + 부착:
```cpp
#include "<Entity>/Enemy/EnemyDeathHandler.h"
// EnemyConfig 에:
EnemyDeathHandler::DeathFx onDeathFx; // 외부(main/Spawns)에서 SpawnEnemyDeathFX 바인딩
// CreateEnemyActor 내부, EnemyContactHandler 부착 다음:
if (cfg.onDeathFx)
    actor->AddComponent<EnemyDeathHandler>(cfg.onDeathFx);
```

- [ ] **Step 5: 적 스폰 측에서 death delegate 바인딩**

적은 `WaveController`(Stage) 또는 startup 의 수동 spawn 으로 생성된다. 적을 스폰하는 자리(main startup 의 수동 enemy spawn 또는 WaveController 의 spawn config)에서 `cfg.onDeathFx` 에 `SpawnEnemyDeathFX` 바인딩:
```cpp
// 적 스폰 코드 (예: main startup 또는 WaveController spawn lambda):
TopdownShooter::Spawns::SequenceContext fxCtx; /* audio/vfx/reg/fxRoot 채움 */
ecfg.onDeathFx = [fxCtx](const vmath::vec3& pos) {
    TopdownShooter::Spawns::SpawnEnemyDeathFX(fxCtx, pos);
};
```
> WaveController 가 적 스폰을 소유한다면 `WaveController` 에 `SequenceContext`(또는 death delegate factory)를 주입하는 setter 추가. WaveController 위치는 `apps/_MyApp_/src/Stage/WaveController.{h,cpp}` — 구현자가 spawn 지점 확인 후 delegate 주입.

- [ ] **Step 6: 총알 hit FX delegate 바인딩 (PlayerSequences 의 BulletFactory)**

`PlayerSequences.cpp` 의 `BuildPlayerAttack` 안 BulletFactory 람다에서, 생성한 bullet 의 `BulletContactHandler` 에 hit FX 바인딩:
```cpp
SequenceContext fxCtx = ctx;
Entity::Player::BulletSpawnPlayable::BulletFactory factory =
    [world, fxCtx](vmath::vec2 pos, vmath::vec2 dir) -> std::unique_ptr<SJH::Scene::Actor>
    {
        Entity::Bullet::BulletConfig bc;
        bc.world = world; bc.pos = pos; bc.dir = dir;
        auto bullet = Entity::Bullet::CreateBulletActor(bc);
        if (auto* bch = bullet->GetComponent<Entity::Bullet::BulletContactHandler>())
            bch->SetOnHitFx([fxCtx](const vmath::vec3& p) { SpawnHitSpark(fxCtx, p); });
        return bullet;
    };
```
> `#include "<Entity>/Bullet/BulletContactHandler.h"` 를 PlayerSequences.cpp 에 추가.

- [ ] **Step 7: Entity CMakeLists 갱신**

`Entity/CMakeLists.txt` 의 `add_library(myapp_entity STATIC ...)` 에 추가:
```cmake
    <Enemy>/EnemyDeathHandler.cpp
```
그리고 `target_link_libraries(myapp_entity ... PRIVATE ...)` — `EnemyDeathHandler`/`BulletContactHandler` 가 leaf/Spawns 를 직접 link 하지 **않음**(delegate 라). 추가 link 불요. (단 `bullet_factory`/`PlayerSequences` 가 사용하는 leaf 는 Spawns lib 가 link.)

- [ ] **Step 8: 빌드 + 실행 검증**

Run: `cmake --build --preset ninja --target _MyApp_` → 성공.
실행 시각 검증:
- 적에게 총알 명중 → **hit spark + 피격음**(적 위치)
- 적 HP 0 → **폭발 FX + 사망음 + 적 despawn**(화면에서 사라짐)
- 단발 FX Actor 가 `FxRoot` 밑에서 finished 후 sweep 으로 제거됨(누수 없음 — 로그/디버거로 child 수 확인 가능)

- [ ] **Step 9: 커밋 (사용자 승인 후)**

```bash
git add apps/_MyApp_/src/Entity <apps>/_MyApp_/src/Spawns/PlayerSequences.cpp
git commit -m "feat(_MyApp_/M6): Enemy 피격(BulletContactHandler hit FX) + 사망(EnemyDeathHandler self-poll + despawn)"
```

---

## Task 10: 최종 통합 시각 검증

**Files:** (코드 변경 없음 — 검증 + 미세 조정)

- [ ] **Step 1: 전체 실행**

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`

- [ ] **Step 2: 체크리스트 (spec §10 수용 기준)**

- [ ] BGM 루프 재생 (BuildBGM)
- [ ] WASD 이동 → Move 클립 / 정지 → Idle 클립 자동 전환
- [ ] 좌클릭 → Attack 클립 + 총알 발사 + muzzle + 총소리(공간감)
- [ ] Shift → Dash (쿨타임)
- [ ] 적 접촉 → Hit 클립 + 피격 연출(TweenShake + Damaged) → Idle 복귀
- [ ] 총알이 적 명중 → hit spark + 피격음
- [ ] 적 사망 → 폭발 FX + 사망음 + despawn
- [ ] 단발 FX 누적/누수 없음 (FxRoot child 수가 발생 후 회수됨)
- [ ] main.cpp 에 인라인 G키/ShotComposite/BGM 잔존 없음 (grep 확인)

- [ ] **Step 3: 누수 점검 (선택, macOS)**

Run: `sh <shell>/CMakeExecute.sh debug _MyApp_ leaks`
Expected: 단발 시퀀스 반복 발생 후에도 Actor/Playable 누수 없음 (sweep 동작 확인).

- [ ] **Step 4: 진행 보고서 갱신 + 커밋 (사용자 승인 후)**

`doc/topdown-shooter-progress.md` 의 M6 행을 `❌ 미시작 0%` → `✅ 완료 100%` (또는 실제 달성도)로 갱신 + 변경 기록 추가:
```bash
git add doc/topdown-shooter-progress.md
git commit -m "docs(progress): M6 시퀀싱+사운드 완료 반영"
```

---

## Self-Review (작성자 체크)

**1. Spec 커버리지:**
- (a) 2D 오디오 → Task 3 ✅ / (b) 발사 시퀀스 → Task 6 #1 + Task 8 ✅ / (c) 적 피격+사망 → Task 4 + Task 9 ✅ / (d) BGM → Task 4 + Task 5 ✅ / (e) 시퀀스 자산화 → Task 4·6 (9종) ✅
- PlayerBehavior 슬롯 5개 → Task 7 에서 전부 SET ✅
- 인라인 G키/좌클릭/BGM 이관 → Task 5·8 ✅
- ParticleStage 전제조건 → P0 (이미 배선됨) ✅

**2. Placeholder 스캔:** 리소스 키 placeholder(hit/dash/spark/explosion)는 의도적(미존재 시 자연 skip) — 명시함. clip lifetime(Task 7 Step 3)은 구현자 확정 지점으로 명시(권장안 제시). 코드 step 은 전부 실제 코드 보유.

**3. 타입 일관성:** `SequenceContext` 필드 / `Build*`·`Spawn*` 시그니처 / leaf ctor(worldPos `std::optional`) / `BulletFactory`·`HitFx`·`DeathFx` delegate 타입 — Task 간 일치 확인.

**알려진 구현자 확정 지점(plan 내 명시):**
- Task 7 Step 3 — SpriteFrameClip clip 보관 lifetime (권장: PlayerBehavior 멤버 벡터)
- Task 7 Step 5 — mFxRoot 생성을 WramupPlayer 전으로 이동
- Task 9 Step 5 — 적 스폰 지점(WaveController vs startup)에 death delegate 주입
- `SpriteFrameClip` 필드명은 `src/sprite/sprite_frame_clip.h` 확인 후 정합

---

## 다음 세션 구현 진입점

1. `cmake --preset ninja` 재configure (CMake 파일 신규 — Spawns lib).
2. **Task 1** 부터 순차. 각 Task 는 빌드 성공 + 실행 검증 + (사용자 승인) 커밋.
3. 권장 실행 방식: **subagent-driven-development** (Task 당 fresh subagent + Task 간 리뷰).
4. 막히면 우선 확인: `src/sprite/sprite_frame_clip.h`(clip 필드), `doc/api/FMODAPI.md`(3D 호출), `apps/_MyApp_/src/Stage/WaveController.{h,cpp}`(적 스폰 지점).

---

## 변경 기록

| 일자 | 변경 |
|---|---|
| 2026-05-31 | M6 plan 초안 — 10 Task. Spawns lib(SequenceContext/AutoDespawn/Sweeper/Combat/Ambient/Player Sequences/PlayerFactory) + 2D 오디오 leaf 확장 + Enemy 피격/사망(delegate) + 입력 트리거 복구 + 인라인 제거. 의존 사이클 해소(Spawns→Entity 단방향 + BulletFactory 정통 delegate). no_auto_tests 정책상 빌드+실행 검증 + 사용자 커밋 trigger. 작성 Claude |
