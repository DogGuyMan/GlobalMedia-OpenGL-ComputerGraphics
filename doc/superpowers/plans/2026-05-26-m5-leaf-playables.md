# M5 Leaf Playable 통합 — 구현 Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **본 plan 의 commit 정책**: 프로젝트 컨벤션 (`.claude/CLAUDE.md`) 에 의거 *사용자 명시 트리거 시에만 commit*. 각 Task 의 마지막 step "사용자 commit 트리거 확인" 은 *blocking* — 사용자가 "commit 해" 라고 명시할 때만 실행. 단순 build/실행 검증은 사용자 트리거 없이 진행 가능.
>
> **테스트 정책**: 본 plan 은 *단위 테스트 생성 0* (`no_auto_tests.md` memory). 시각 검증 + build 통과 만으로 task 완료 판정.

**Goal**: M3.5 IPlayable/PlayableBase/Composite 위에 FMOD Core/Studio + Effekseer + Tweeny 4 leaf Playable 을 _MyApp_ Client 거주로 도입하고, Sequence/Parallel Composite 의 시각 검증을 완료한다.

**Architecture**: Engine 측은 `SJH::ResourceRegistry` 에 `CreateSound` / `CreateEffect` 2 메소드 + Sound/Effect class 신설 (game_deps 가 engine 으로 누출되는 큰 변경). Client 측은 `TopdownShooter::Director` 싱글톤으로 PhysicsSystem + AudioSystem + VFXSystem 집계. M5 skeleton 4 파일은 snake_case → PascalCase rename 후 활성화.

**Tech Stack**: C++17, CMake 3.14+, FMOD Studio 2.02+ (Core+Studio), Effekseer 1.7.3.0 + EffekseerRendererGL, Tweeny header-only. macOS Ninja + Windows MSVC2019/2022.

**Spec**: [`doc/superpowers/specs/2026-05-26-m5-leaf-playables-design.md`](../specs/2026-05-26-m5-leaf-playables-design.md) (7 결정 확정)

**작업 단위 표**:

| Phase | Tasks | 목적 |
|---|---|---|
| Engine 확장 | 1~5 | game_deps PUBLIC + Sound + Effect + Registry 메소드 + 빌드 검증 |
| Client Rename | 6 | skeleton 4 파일 snake → Pascal + #include + 헤더 가드 + CMakeLists |
| Client Subsystem | 7~9 | AudioSystem + VFXSystem + Director |
| Client Director 통합 | 10 | main.cpp 의 mPhysics → director_.Physics() |
| Client Leaf 활성화 | 11~14 | FmodStudio(T3) + Fmod(T1) + Effekseer(T2) + Tween(T4) |
| Composite 통합 | 15~16 | Sequence/Parallel 시나리오 부착 |
| POST_BUILD + 정리 | 17~18 | 자원 copy + 문서 갱신 |

---

## File Structure

### 신설 파일 (Engine + Client)

```
src/resource_registry/
├─ sound.h                    [신설 — SJH::Sound = FMOD::Sound* RAII wrap]
├─ sound.cpp                  [신설]
├─ effect.h                   [신설 — SJH::Effect = Effekseer::EffectRef wrap]
└─ effect.cpp                 [신설]

apps/_MyApp_/src/
├─ Director.h                 [신설 — TopdownShooter::Director 싱글톤]
├─ Director.cpp               [신설]
├─ apps/_MyApp_/src/Audio/AudioSystem.h        [신설 — FMOD Core + Studio init/update/release + LoadEvent 캐시]
├─ apps/_MyApp_/src/Audio/AudioSystem.cpp      [신설]
├─ apps/_MyApp_/src/VFX/VFXSystem.h            [신설 — Effekseer Manager + Renderer]
└─ apps/_MyApp_/src/VFX/VFXSystem.cpp          [신설]

apps/_MyApp_/resources/
├─ audio/Laser.wav            [복사 — effekseer_demo/demo1/resources/Sound/]
└─ vfx/distortion.efk         [복사 — effekseer_demo/demo1/resources/]
```

### 수정/Rename 파일

```
src/resource_registry/
├─ CMakeLists.txt             [수정 — target_link_libraries PUBLIC game_deps 추가]
├─ resource_registry.h        [수정 — CreateSound/CreateEffect 선언 + Sound/Effect include + cache]
└─ resource_registry.cpp      [수정 — 두 메소드 구현]

apps/_MyApp_/src/
├─ Audio/fmod_playable.{h,cpp}        → FmodPlayable.{h,cpp}        [rename + 활성화]
├─ Audio/fmod_studio_playable.{h,cpp} → FmodStudioPlayable.{h,cpp}  [rename + 활성화]
├─ Audio/CMakeLists.txt               [수정 — 소스 파일명 갱신]
├─ VFX/effekseer_playable.{h,cpp}     → EffekseerPlayable.{h,cpp}   [rename + 활성화 + TrackPolicy enum]
├─ VFX/CMakeLists.txt                 [수정 — 소스 파일명 갱신]
├─ <Tween>/tween_playable.h             → TweenPlayable.h             [rename + 활성화 + template 구현]
└─ Tween/CMakeLists.txt               [수정 — 없음 (header-only INTERFACE 라 소스 목록 없음, include 경로만)]

apps/_MyApp_/
├─ main.cpp                   [수정 — Director 통합 + leaf Playable 부착 + onMouseButton + onKey]
└─ CMakeLists.txt             [확인만 — POST_BUILD 가 resources/* 재귀 copy 인지]

apps/_MyApp_/src/CMakeLists.txt  [수정 — Director.cpp 합류]
```

### 갱신 문서

```
doc/topdown-shooter-progress.md   [수정 — M5 섹션 status 갱신 + commits 표]
.claude/CLAUDE.md                 [수정 — SJH::resource_registry 의존 표에 + game_deps PUBLIC 명시]
~/.claude/projects/.../memory/MEMORY.md  [선택 갱신 — 결정 신규 항목 시]
```

---

# Phase 1 — Engine 확장 (SP1~SP4)

## Task 1: `src/resource_registry/CMakeLists.txt` 에 game_deps PUBLIC link 추가

**Files:**
- Modify: `src/resource_registry/CMakeLists.txt:14-22`

- [ ] **Step 1: CMakeLists.txt 수정**

기존 14~22 행:
```cmake
target_link_libraries(sjhopengl_resource_registry
    PUBLIC  SJH::common
            SJH::object
            SJH::material
            SJH::program
            project_deps
            spdlog
    PRIVATE SJH::diagnostics
)
```

다음으로 교체 (한 줄 추가):
```cmake
target_link_libraries(sjhopengl_resource_registry
    PUBLIC  SJH::common
            SJH::object
            SJH::material
            SJH::program
            project_deps
            spdlog
            game_deps          # M5 — Sound/Effect 헤더가 FMOD/Effekseer 노출 (spec §6.1)
    PRIVATE SJH::diagnostics
)
```

- [ ] **Step 2: 빌드 검증 — _MyApp_ 정상 통과**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상 빌드. (game_deps 가 합류해도 사용 0 이라 link 영향 미미. include 경로만 expanding)

- [ ] **Step 3: 영향 범위 측정 — `migrate_demo` / `audio_demo` 도 정상**

Run:
```bash
cmake --build --preset ninja --target migrate_demo
cmake --build --preset ninja --target audio_demo
```

Expected: 둘 다 정상. (audio_demo 는 이미 game_deps 사용 중 — 무영향)

- [ ] **Step 4: 사용자 commit 트리거 확인**

사용자가 "commit 해" 라고 명시할 때까지 *대기*. 명시 시:
```bash
git add src/resource_registry/CMakeLists.txt
git commit -m "[dev] : ResourceRegistry 에 game_deps PUBLIC link (M5 SP1)"
```

---

## Task 2: `SJH::Sound` 클래스 신설 (`src/resource_registry/sound.{h,cpp}`)

**Files:**
- Create: `src/resource_registry/sound.h`
- Create: `src/resource_registry/sound.cpp`

- [ ] **Step 1: `sound.h` 작성**

`src/resource_registry/sound.h`:
```cpp
#ifndef __SJH_RESOURCE_REGISTRY_SOUND_H__
#define __SJH_RESOURCE_REGISTRY_SOUND_H__

#include "common/common.h"

// fwd — FMOD 헤더는 .cpp 안에서만
namespace FMOD { class Sound; }

namespace SJH
{
    CLASS_PTR(Sound)
    /// @brief FMOD::Sound* RAII wrap — ResourceRegistry::CreateSound 가 캐시 entry 로 보유.
    /// @details
    ///   - 외부 (FmodPlayable) 는 Raw() 로 FMOD::Sound* 조회만. 직접 release 금지.
    ///   - dtor 가 FMOD::Sound::release() 호출 — ResourceRegistry::Clear() 시 일괄 정리.
    class Sound
    {
      public:
        explicit Sound(::FMOD::Sound* raw) : mRaw(raw) {}
        ~Sound();

        ::FMOD::Sound* Raw() const { return mRaw; }

        Sound(const Sound&)            = delete;
        Sound& operator=(const Sound&) = delete;
        Sound(Sound&&)                 = delete;
        Sound& operator=(Sound&&)      = delete;

      private:
        ::FMOD::Sound* mRaw = nullptr;
    };
}

#endif // __SJH_RESOURCE_REGISTRY_SOUND_H__
```

- [ ] **Step 2: `sound.cpp` 작성**

`src/resource_registry/sound.cpp`:
```cpp
#include "sound.h"
#include <fmod/fmod.hpp>

namespace SJH
{
    Sound::~Sound()
    {
        if (mRaw)
        {
            mRaw->release();
            mRaw = nullptr;
        }
    }
}
```

- [ ] **Step 3: `src/resource_registry/CMakeLists.txt` 에 sound.cpp 추가**

기존 1~5 행:
```cmake
add_library(sjhopengl_resource_registry STATIC
    resource_registry.cpp
    texture.cpp
    image.cpp
)
```

다음으로 교체:
```cmake
add_library(sjhopengl_resource_registry STATIC
    resource_registry.cpp
    texture.cpp
    image.cpp
    sound.cpp                # M5 — FMOD::Sound RAII wrap
)
```

- [ ] **Step 4: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 5: 사용자 commit 트리거 확인 후**:
```bash
git add src/resource_registry/sound.h src/resource_registry/sound.cpp src/resource_registry/CMakeLists.txt
git commit -m "[dev] : SJH::Sound RAII wrap 신설 (M5 SP2)"
```

---

## Task 3: `SJH::Effect` 클래스 신설 (`src/resource_registry/effect.{h,cpp}`)

**Files:**
- Create: `src/resource_registry/effect.h`
- Create: `src/resource_registry/effect.cpp`

- [ ] **Step 1: `effect.h` 작성**

`src/resource_registry/effect.h`:
```cpp
#ifndef __SJH_RESOURCE_REGISTRY_EFFECT_H__
#define __SJH_RESOURCE_REGISTRY_EFFECT_H__

#include "common/common.h"
#include <Effekseer.h>   // EffectRef = std::shared_ptr<Effect> 정의 필요

namespace SJH
{
    CLASS_PTR(Effect)
    /// @brief Effekseer::EffectRef wrap — ResourceRegistry::CreateEffect 가 캐시 entry 로 보유.
    /// @details
    ///   - EffectRef 는 shared_ptr 류 — dtor 자동 정리, 명시 release 불요.
    ///   - 외부 (EffekseerPlayable) 는 Ref() 로 EffectRef 조회. manager_->Play(ref, pos) 인자로 직접 전달 가능.
    class Effect
    {
      public:
        explicit Effect(::Effekseer::EffectRef ref) : mRef(ref) {}
        ~Effect() = default;

        ::Effekseer::EffectRef Ref() const { return mRef; }

        Effect(const Effect&)            = delete;
        Effect& operator=(const Effect&) = delete;
        Effect(Effect&&)                 = delete;
        Effect& operator=(Effect&&)      = delete;

      private:
        ::Effekseer::EffectRef mRef;
    };
}

#endif // __SJH_RESOURCE_REGISTRY_EFFECT_H__
```

- [ ] **Step 2: `effect.cpp` 작성**

`src/resource_registry/effect.cpp`:
```cpp
// SJH::Effect 는 inline-only — vtable anchor 만 필요할 시 future 작업.
// 현재는 dtor = default + Ref() inline 이라 별도 정의 없음. 단 add_library 에서 소스 누락 시 link 단계 무영향.
#include "effect.h"

namespace SJH
{
    // (intentionally empty — Effect 는 inline-only)
}
```

- [ ] **Step 3: `src/resource_registry/CMakeLists.txt` 에 effect.cpp 추가**

기존 (Task 2 적용 후) 1~6 행:
```cmake
add_library(sjhopengl_resource_registry STATIC
    resource_registry.cpp
    texture.cpp
    image.cpp
    sound.cpp
)
```

다음으로 교체:
```cmake
add_library(sjhopengl_resource_registry STATIC
    resource_registry.cpp
    texture.cpp
    image.cpp
    sound.cpp                # M5 — FMOD::Sound RAII wrap
    effect.cpp               # M5 — Effekseer::EffectRef wrap
)
```

- [ ] **Step 4: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 5: 사용자 commit 트리거 확인 후**:
```bash
git add src/resource_registry/effect.h src/resource_registry/effect.cpp src/resource_registry/CMakeLists.txt
git commit -m "[dev] : SJH::Effect RAII wrap 신설 (M5 SP3)"
```

---

## Task 4: `ResourceRegistry::CreateSound` + `CreateEffect` 메소드 추가

**Files:**
- Modify: `src/resource_registry/resource_registry.h` (선언 + cache 멤버)
- Modify: `src/resource_registry/resource_registry.cpp` (구현)

- [ ] **Step 1: `resource_registry.h` 에 include + 선언 + cache 추가**

기존 include 블록 (23~34 행) 끝에 추가:
```cpp
#include "sound.h"            // M5 — CreateSound/FindSound
#include "effect.h"           // M5 — CreateEffect/FindEffect
```

또한 fwd 추가 (FMOD::System / Effekseer::ManagerRef 가 메소드 시그니처에 등장 — 헤더 안에서):
```cpp
namespace FMOD { class System; }
// Effekseer.h 는 effect.h 가 이미 include 함 — ManagerRef 자동 노출
```

`CreateUniformAtlas` 선언 (127~129 행) 아래에 추가:
```cpp
    /// @brief FMOD .wav / .ogg 를 *로드*해 @p key 로 캐시. 이미 있거나 로드 실패 시 nullptr.
    /// @details
    ///   - @p sys = AudioSystem 의 FMOD::System* (외부 owner — 본 메소드는 ptr 만 사용, 보유 안 함).
    ///   - 내부적으로 sys->createSound(@p path, FMOD_DEFAULT, nullptr, &raw) → SJH::Sound RAII wrap → 캐시.
    Sound *CreateSound(::FMOD::System *sys, const std::string &key, const std::string &path);

    /// @brief @p key 로 캐시된 Sound *조회* (생성 안 함). 없으면 nullptr.
    Sound *FindSound(const std::string &key);

    /// @brief Effekseer .efk 를 *로드*해 @p key 로 캐시. 이미 있거나 로드 실패 시 nullptr.
    /// @details
    ///   - @p manager = VFXSystem 의 Effekseer::ManagerRef.
    ///   - @p path = utf-16 (Effekseer 표준). 호출자는 u"resources/vfx/foo.efk" 리터럴 사용.
    Effect *CreateEffect(::Effekseer::ManagerRef manager, const std::string &key, const char16_t *path);

    /// @brief @p key 로 캐시된 Effect *조회* (생성 안 함). 없으면 nullptr.
    Effect *FindEffect(const std::string &key);
```

cache 멤버 (146~153 행) 끝에 추가:
```cpp
    std::unordered_map<std::string, SoundUPtr> mSounds;
    std::unordered_map<std::string, EffectUPtr> mEffects;
```

- [ ] **Step 2: `resource_registry.cpp` 에 구현 추가**

파일 끝(`Clear()` 메소드 위 또는 적절한 위치) 에 추가:

```cpp
// M5 — FMOD Sound
Sound *ResourceRegistry::CreateSound(::FMOD::System *sys, const std::string &key, const std::string &path)
{
    if (!sys) { spdlog::error("[ResourceRegistry::CreateSound] sys=nullptr (key={})", key); return nullptr; }
    if (mSounds.count(key)) { spdlog::warn("[ResourceRegistry::CreateSound] key 중복: {}", key); return nullptr; }

    ::FMOD::Sound* raw = nullptr;
    FMOD_RESULT r = sys->createSound(path.c_str(), FMOD_DEFAULT, nullptr, &raw);
    if (r != FMOD_OK || !raw) {
        spdlog::error("[ResourceRegistry::CreateSound] createSound 실패 path={} FMOD_RESULT={}", path, int(r));
        return nullptr;
    }

    auto sound = std::make_unique<Sound>(raw);
    Sound* ret = sound.get();
    mSounds.emplace(key, std::move(sound));
    return ret;
}

Sound *ResourceRegistry::FindSound(const std::string &key)
{
    auto it = mSounds.find(key);
    return (it == mSounds.end()) ? nullptr : it->second.get();
}

// M5 — Effekseer Effect
Effect *ResourceRegistry::CreateEffect(::Effekseer::ManagerRef manager, const std::string &key, const char16_t *path)
{
    if (!manager) { spdlog::error("[ResourceRegistry::CreateEffect] manager=null (key={})", key); return nullptr; }
    if (mEffects.count(key)) { spdlog::warn("[ResourceRegistry::CreateEffect] key 중복: {}", key); return nullptr; }

    ::Effekseer::EffectRef ref = ::Effekseer::Effect::Create(manager, reinterpret_cast<const EFK_CHAR*>(path));
    if (!ref) {
        spdlog::error("[ResourceRegistry::CreateEffect] Effekseer::Effect::Create 실패 (key={})", key);
        return nullptr;
    }

    auto eff = std::make_unique<Effect>(ref);
    Effect* ret = eff.get();
    mEffects.emplace(key, std::move(eff));
    return ret;
}

Effect *ResourceRegistry::FindEffect(const std::string &key)
{
    auto it = mEffects.find(key);
    return (it == mEffects.end()) ? nullptr : it->second.get();
}
```

`Clear()` 메소드 내부에 캐시 초기화 추가 (예: 기존 `mAtlas.clear();` 옆에):
```cpp
    mSounds.clear();        // M5
    mEffects.clear();       // M5 (EffectRef shared_ptr 자동 정리)
```

`resource_registry.cpp` 의 include 블록 끝에 FMOD 헤더 추가:
```cpp
#include <fmod/fmod.hpp>    // M5 — CreateSound 의 createSound 호출
```

- [ ] **Step 3: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상. (호출처 0 이지만 컴파일 단계에서 시그니처 검증)

- [ ] **Step 4: 사용자 commit 트리거 확인 후**:
```bash
git add src/resource_registry/resource_registry.h src/resource_registry/resource_registry.cpp
git commit -m "[dev] : ResourceRegistry::CreateSound + CreateEffect 추가 (M5 SP4)"
```

---

## Task 5: Engine 변경 영향 측정 — 테스트 활성 빌드 검증

**Files:** (수정 없음 — 검증 only)

- [ ] **Step 1: ENABLE_TESTING=ON 으로 재구성**

Run:
```bash
cmake --preset ninja -DENABLE_TESTING=ON
```

Expected: configure 정상.

- [ ] **Step 2: 활성 단위 테스트 빌드**

Run:
```bash
cmake --build --preset ninja --target tests
```

Expected: 정상. game_deps 가 PUBLIC 으로 합류해도 test 들이 추가 헤더를 안 쓰니 *컴파일 영향 미미* 예상. 실패 시:
- game_deps 의 헤더 매크로 (예: NOMINMAX 누락) 충돌 점검
- 가장 흔한 원인: windows.h 의 min/max — 이미 CXXStandard.cmake 에서 가드됨

- [ ] **Step 3: 테스트 실행**

Run:
```bash
ctest --test-dir build_ninja --output-on-failure
```

Expected: 활성 ~21 + smoke 1 모두 PASS. game_deps 합류로 *기능 영향 0* (link 만 합류, 호출 0).

- [ ] **Step 4: 다시 ENABLE_TESTING=OFF 로 (기본 상태 복원)**

Run:
```bash
cmake --preset ninja
```

(이 step 은 commit 대상 0)

---

# Phase 2 — Client Rename (RN1)

## Task 6: Skeleton 파일 snake_case → PascalCase rename

**Files:**
- Rename: `<apps>/_MyApp_/src/Audio/fmod_playable.h` → `apps/_MyApp_/src/Audio/FmodPlayable.h`
- Rename: `<apps>/_MyApp_/src/Audio/fmod_playable.cpp` → `apps/_MyApp_/src/Audio/FmodPlayable.cpp`
- Rename: `<apps>/_MyApp_/src/Audio/fmod_studio_playable.h` → `apps/_MyApp_/src/Audio/FmodStudioPlayable.h`
- Rename: `<apps>/_MyApp_/src/Audio/fmod_studio_playable.cpp` → `apps/_MyApp_/src/Audio/FmodStudioPlayable.cpp`
- Rename: `<apps>/_MyApp_/src/VFX/effekseer_playable.h` → `apps/_MyApp_/src/VFX/EffekseerPlayable.h`
- Rename: `<apps>/_MyApp_/src/VFX/effekseer_playable.cpp` → `apps/_MyApp_/src/VFX/EffekseerPlayable.cpp`
- Rename: `<apps>/_MyApp_/src/Tween/tween_playable.h` → `apps/_MyApp_/src/Tween/TweenPlayable.h`
- Modify: `apps/_MyApp_/src/Audio/CMakeLists.txt`
- Modify: `apps/_MyApp_/src/VFX/CMakeLists.txt`
- Modify: 각 rename 된 `.h` 의 헤더 가드 + `.cpp` 의 include 경로

- [ ] **Step 1: `git mv` 7회 (rename history 보존)**

Run:
```bash
git mv <apps>/_MyApp_/src/Audio/fmod_playable.h         apps/_MyApp_/src/Audio/FmodPlayable.h
git mv <apps>/_MyApp_/src/Audio/fmod_playable.cpp       apps/_MyApp_/src/Audio/FmodPlayable.cpp
git mv <apps>/_MyApp_/src/Audio/fmod_studio_playable.h  apps/_MyApp_/src/Audio/FmodStudioPlayable.h
git mv <apps>/_MyApp_/src/Audio/fmod_studio_playable.cpp apps/_MyApp_/src/Audio/FmodStudioPlayable.cpp
git mv <apps>/_MyApp_/src/VFX/effekseer_playable.h      apps/_MyApp_/src/VFX/EffekseerPlayable.h
git mv <apps>/_MyApp_/src/VFX/effekseer_playable.cpp    apps/_MyApp_/src/VFX/EffekseerPlayable.cpp
git mv <apps>/_MyApp_/src/Tween/tween_playable.h        apps/_MyApp_/src/Tween/TweenPlayable.h
```

- [ ] **Step 2: 각 `.h` 의 헤더 가드 매크로 갱신**

`apps/_MyApp_/src/Audio/FmodPlayable.h`:
- `_TOPDOWNSHOOTER_AUDIO_FMOD_PLAYABLE_H__` → `_TOPDOWNSHOOTER_AUDIO_FMODPLAYABLE_H__`

`apps/_MyApp_/src/Audio/FmodStudioPlayable.h`:
- `_TOPDOWNSHOOTER_AUDIO_FMOD_STUDIO_PLAYABLE_H__` → `_TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__`

`apps/_MyApp_/src/VFX/EffekseerPlayable.h`:
- `_TOPDOWNSHOOTER_VFX_EFFEKSEER_PLAYABLE_H__` → `_TOPDOWNSHOOTER_VFX_EFFEKSEERPLAYABLE_H__`

`apps/_MyApp_/src/Tween/TweenPlayable.h`:
- `_TOPDOWNSHOOTER_TWEEN_TWEEN_PLAYABLE_H__` → `_TOPDOWNSHOOTER_TWEEN_TWEENPLAYABLE_H__`

각 파일의 `#ifndef` + `#define` + `#endif // ...` 3 곳 모두 갱신.

- [ ] **Step 3: 각 `.cpp` 의 `#include` 경로 갱신**

`apps/_MyApp_/src/Audio/FmodPlayable.cpp`:
- `#include "fmod_playable.h"` → `#include "FmodPlayable.h"`

`apps/_MyApp_/src/Audio/FmodStudioPlayable.cpp`:
- `#include "fmod_studio_playable.h"` → `#include "FmodStudioPlayable.h"`

`apps/_MyApp_/src/VFX/EffekseerPlayable.cpp`:
- `#include "effekseer_playable.h"` → `#include "EffekseerPlayable.h"`

- [ ] **Step 4: `Audio/CMakeLists.txt` 의 add_library 소스 목록 갱신**

기존 8~11 행:
```cmake
add_library(myapp_audio STATIC
    fmod_playable.cpp
    fmod_studio_playable.cpp
)
```

다음으로 교체:
```cmake
add_library(myapp_audio STATIC
    FmodPlayable.cpp
    FmodStudioPlayable.cpp
)
```

- [ ] **Step 5: `VFX/CMakeLists.txt` 의 add_library 소스 목록 갱신**

기존 7~9 행:
```cmake
add_library(myapp_vfx STATIC
    effekseer_playable.cpp
)
```

다음으로 교체:
```cmake
add_library(myapp_vfx STATIC
    EffekseerPlayable.cpp
)
```

- [ ] **Step 6: Tween/CMakeLists.txt 검증 (변경 없음)**

`apps/_MyApp_/src/Tween/CMakeLists.txt` 는 `add_library(myapp_tween INTERFACE)` 라 소스 목록 없음 — 변경 0. include 경로는 `${CMAKE_CURRENT_SOURCE_DIR}/..` 이라 자동 적용.

- [ ] **Step 7: main.cpp 의 #include 경로 점검**

Run:
```bash
grep -n "fmod_playable\|fmod_studio_playable\|effekseer_playable\|tween_playable" apps/_MyApp_/main.cpp
```

Expected: 출력 0 줄. (현재 main.cpp 는 leaf Playable 미포함이라 0 일 것)
출력이 있으면 PascalCase 경로로 교체:
- `#include "<Audio>/fmod_playable.h"` → `#include "apps/_MyApp_/src/Audio/FmodPlayable.h"` 식.

- [ ] **Step 8: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상. (skeleton stub 만 있어 link 영향 0)

- [ ] **Step 9: 사용자 commit 트리거 확인 후**:
```bash
git add apps/_MyApp_/src/Audio apps/_MyApp_/src/VFX apps/_MyApp_/src/Tween
git commit -m "[dev] : M5 skeleton snake_case → PascalCase rename (RN1)"
```

---

# Phase 3 — Client Subsystem (CL1~CL3)

## Task 7: `AudioSystem.{h,cpp}` 신설

**Files:**
- Create: `apps/_MyApp_/src/Audio/AudioSystem.h`
- Create: `apps/_MyApp_/src/Audio/AudioSystem.cpp`
- Modify: `apps/_MyApp_/src/Audio/CMakeLists.txt` (소스 추가)

- [ ] **Step 1: `AudioSystem.h` 작성**

`apps/_MyApp_/src/Audio/AudioSystem.h`:
```cpp
#ifndef _TOPDOWNSHOOTER_AUDIO_AUDIOSYSTEM_H__
#define _TOPDOWNSHOOTER_AUDIO_AUDIOSYSTEM_H__

#include <string>
#include <unordered_map>
#include <vector>

// fwd — FMOD 헤더는 .cpp 안에서만
namespace FMOD
{
    class System;
    namespace Studio
    {
        class System;
        class Bank;
        class EventDescription;
    }
}

namespace TopdownShooter::Audio
{
    /// @brief FMOD Core (System) + Studio (System + Bank + EventDescription 캐시) owner.
    /// @details Director 멤버로 거주. main 의 startup 에서 Init, render 마다 Update(dt), shutdown 에서 Shutdown.
    class AudioSystem
    {
      public:
        AudioSystem()  = default;
        ~AudioSystem() = default;
        AudioSystem(const AudioSystem&)            = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;

        void Init();
        void Update(float dt);
        void Shutdown();

        ::FMOD::System*         GetSystem()       { return mSystem; }
        ::FMOD::Studio::System* GetStudioSystem() { return mStudioSystem; }

        /// @brief @p path (예: "resources/banks/Master.bank") 의 bank 를 *로드*.
        void LoadBank(const std::string& path);

        /// @brief @p eventPath (예: "event:/BGM") 의 EventDescription 을 *조회/캐시* 후 반환.
        ///        Studio bank 가 사전에 LoadBank 로 로드된 상태여야 한다. 못 찾으면 nullptr.
        ::FMOD::Studio::EventDescription* LoadEvent(const std::string& eventPath);

      private:
        ::FMOD::System*         mSystem       = nullptr;
        ::FMOD::Studio::System* mStudioSystem = nullptr;
        std::vector<::FMOD::Studio::Bank*> mBanks;
        std::unordered_map<std::string, ::FMOD::Studio::EventDescription*> mEventCache;
    };
}

#endif // _TOPDOWNSHOOTER_AUDIO_AUDIOSYSTEM_H__
```

- [ ] **Step 2: `AudioSystem.cpp` 작성**

`apps/_MyApp_/src/Audio/AudioSystem.cpp`:
```cpp
#include "AudioSystem.h"

#include <fmod/fmod.hpp>
#include <fmod/fmod_studio.hpp>
#include <<spdlog>/spdlog.h>

namespace TopdownShooter::Audio
{
    namespace
    {
        bool ck(FMOD_RESULT r, const char* what)
        {
            if (r != FMOD_OK) { spdlog::error("[AudioSystem] {} FMOD_RESULT={}", what, int(r)); return false; }
            return true;
        }
    }

    void AudioSystem::Init()
    {
        if (!ck(::FMOD::Studio::System::create(&mStudioSystem), "Studio::System::create")) return;
        if (!ck(mStudioSystem->initialize(512,
                                          FMOD_STUDIO_INIT_NORMAL,
                                          FMOD_INIT_NORMAL,
                                          nullptr), "Studio::System::initialize")) return;
        if (!ck(mStudioSystem->getCoreSystem(&mSystem), "Studio::System::getCoreSystem")) return;
        spdlog::info("[AudioSystem] init OK (Studio + Core)");
    }

    void AudioSystem::Update(float /*dt*/)
    {
        if (mStudioSystem) mStudioSystem->update();   // FMOD 는 자체 dt 추적
    }

    void AudioSystem::Shutdown()
    {
        if (mStudioSystem)
        {
            for (auto* bank : mBanks) if (bank) bank->unload();
            mBanks.clear();
            mEventCache.clear();
            mStudioSystem->release();
            mStudioSystem = nullptr;
            mSystem       = nullptr;   // Studio 가 Core 소유 — getCoreSystem 으로 받은 ptr 은 별도 release 불요
        }
        spdlog::info("[AudioSystem] shutdown OK");
    }

    void AudioSystem::LoadBank(const std::string& path)
    {
        if (!mStudioSystem) { spdlog::error("[AudioSystem::LoadBank] Studio 미초기화"); return; }
        ::FMOD::Studio::Bank* bank = nullptr;
        if (!ck(mStudioSystem->loadBankFile(path.c_str(), FMOD_STUDIO_LOAD_BANK_NORMAL, &bank),
                ("loadBankFile " + path).c_str())) return;
        mBanks.push_back(bank);
        spdlog::info("[AudioSystem::LoadBank] OK {}", path);
    }

    ::FMOD::Studio::EventDescription* AudioSystem::LoadEvent(const std::string& eventPath)
    {
        if (auto it = mEventCache.find(eventPath); it != mEventCache.end()) return it->second;
        if (!mStudioSystem) { spdlog::error("[AudioSystem::LoadEvent] Studio 미초기화"); return nullptr; }

        ::FMOD::Studio::EventDescription* desc = nullptr;
        FMOD_RESULT r = mStudioSystem->getEvent(eventPath.c_str(), &desc);
        if (r != FMOD_OK || !desc) { spdlog::warn("[AudioSystem::LoadEvent] 미존재 {} FMOD_RESULT={}", eventPath, int(r)); return nullptr; }
        mEventCache[eventPath] = desc;
        return desc;
    }
}
```

- [ ] **Step 3: `Audio/CMakeLists.txt` 에 AudioSystem.cpp 추가**

기존 (Task 6 적용 후) 8~11 행:
```cmake
add_library(myapp_audio STATIC
    FmodPlayable.cpp
    FmodStudioPlayable.cpp
)
```

다음으로 교체:
```cmake
add_library(myapp_audio STATIC
    AudioSystem.cpp           # M5 CL1 — FMOD Core+Studio owner
    FmodPlayable.cpp
    FmodStudioPlayable.cpp
)
```

- [ ] **Step 4: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 5: 사용자 commit 트리거 확인 후**:
```bash
git add apps/_MyApp_/src/Audio/AudioSystem.h apps/_MyApp_/src/Audio/AudioSystem.cpp apps/_MyApp_/src/Audio/CMakeLists.txt
git commit -m "[dev] : AudioSystem 신설 — FMOD Core+Studio + LoadEvent 캐시 (M5 CL1)"
```

---

## Task 8: `VFXSystem.{h,cpp}` 신설

**Files:**
- Create: `apps/_MyApp_/src/VFX/VFXSystem.h`
- Create: `apps/_MyApp_/src/VFX/VFXSystem.cpp`
- Modify: `apps/_MyApp_/src/VFX/CMakeLists.txt` (소스 추가)

- [ ] **Step 1: `VFXSystem.h` 작성**

`apps/_MyApp_/src/VFX/VFXSystem.h`:
```cpp
#ifndef _TOPDOWNSHOOTER_VFX_VFXSYSTEM_H__
#define _TOPDOWNSHOOTER_VFX_VFXSYSTEM_H__

#include <Effekseer.h>
#include <EffekseerRendererGL.h>

namespace TopdownShooter::VFX
{
    /// @brief Effekseer Manager + EffekseerRendererGL Renderer owner.
    /// @details Director 멤버로 거주. Update(dt) 는 main update 단계, Draw(view, proj) 는 render 단계.
    class VFXSystem
    {
      public:
        VFXSystem()  = default;
        ~VFXSystem() = default;
        VFXSystem(const VFXSystem&)            = delete;
        VFXSystem& operator=(const VFXSystem&) = delete;

        void Init(int maxSprites = 8000);
        void Update(float dt);   // manager_->Update(dt * 60.0f)
        void Draw(const float* viewMat, const float* projMat);
        void Shutdown();

        ::Effekseer::ManagerRef           GetManager()  { return mManager; }
        ::EffekseerRendererGL::RendererRef GetRenderer() { return mRenderer; }

      private:
        ::EffekseerRendererGL::RendererRef mRenderer;
        ::Effekseer::ManagerRef            mManager;
    };
}

#endif // _TOPDOWNSHOOTER_VFX_VFXSYSTEM_H__
```

- [ ] **Step 2: `VFXSystem.cpp` 작성**

`apps/_MyApp_/src/VFX/VFXSystem.cpp`:
```cpp
#include "VFXSystem.h"

#include <Effekseer.h>
#include <EffekseerRendererGL.h>
#include <<spdlog>/spdlog.h>

namespace TopdownShooter::VFX
{
    void VFXSystem::Init(int maxSprites)
    {
        ::EffekseerRendererGL::OpenGLDeviceType deviceType = ::EffekseerRendererGL::OpenGLDeviceType::OpenGL3;
        auto graphicsDevice = ::EffekseerRendererGL::CreateGraphicsDevice(deviceType);
        mRenderer = ::EffekseerRendererGL::Renderer::Create(graphicsDevice, maxSprites);
        mManager  = ::Effekseer::Manager::Create(maxSprites);

        mManager->SetSpriteRenderer(mRenderer->CreateSpriteRenderer());
        mManager->SetRibbonRenderer(mRenderer->CreateRibbonRenderer());
        mManager->SetRingRenderer(mRenderer->CreateRingRenderer());
        mManager->SetTrackRenderer(mRenderer->CreateTrackRenderer());
        mManager->SetModelRenderer(mRenderer->CreateModelRenderer());

        mManager->SetTextureLoader(mRenderer->CreateTextureLoader());
        mManager->SetModelLoader(mRenderer->CreateModelLoader());
        mManager->SetMaterialLoader(mRenderer->CreateMaterialLoader());
        mManager->SetCurveLoader(::Effekseer::MakeRefPtr<::Effekseer::CurveLoader>());

        spdlog::info("[VFXSystem] init OK (max={})", maxSprites);
    }

    void VFXSystem::Update(float dt)
    {
        if (!mManager) return;
        // Effekseer 의 표준 dt 는 frame 단위 (60fps 기준). 초 dt 를 frame 으로 변환.
        mManager->Update(dt * 60.0f);
    }

    void VFXSystem::Draw(const float* viewMat, const float* projMat)
    {
        if (!mManager || !mRenderer) return;

        ::Effekseer::Matrix44 view, proj;
        std::memcpy(view.Values, viewMat, sizeof(float) * 16);
        std::memcpy(proj.Values, projMat, sizeof(float) * 16);
        mRenderer->SetCameraMatrix(view);
        mRenderer->SetProjectionMatrix(proj);

        mRenderer->BeginRendering();
        mManager->Draw();
        mRenderer->EndRendering();
    }

    void VFXSystem::Shutdown()
    {
        // RefPtr 이라 dtor 가 자동 release. 명시 reset 으로 순서 강제.
        mManager.Reset();
        mRenderer.Reset();
        spdlog::info("[VFXSystem] shutdown OK");
    }
}
```

`#include <cstring>` 추가 (std::memcpy):
```cpp
#include <cstring>
```

- [ ] **Step 3: `VFX/CMakeLists.txt` 에 VFXSystem.cpp 추가**

기존 (Task 6 적용 후) 7~9 행:
```cmake
add_library(myapp_vfx STATIC
    EffekseerPlayable.cpp
)
```

다음으로 교체:
```cmake
add_library(myapp_vfx STATIC
    VFXSystem.cpp             # M5 CL2 — Effekseer Manager+Renderer owner
    EffekseerPlayable.cpp
)
```

- [ ] **Step 4: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 5: 사용자 commit 트리거 확인 후**:
```bash
git add apps/_MyApp_/src/VFX/VFXSystem.h apps/_MyApp_/src/VFX/VFXSystem.cpp apps/_MyApp_/src/VFX/CMakeLists.txt
git commit -m "[dev] : VFXSystem 신설 — Effekseer Manager+Renderer (M5 CL2)"
```

---

## Task 9: `Director.{h,cpp}` 신설

**Files:**
- Create: `<apps>/_MyApp_/src/Director.h`
- Create: `<apps>/_MyApp_/src/Director.cpp`
- Modify: `apps/_MyApp_/src/CMakeLists.txt` (Director.cpp 합류)

- [ ] **Step 1: `Director.h` 작성**

`<apps>/_MyApp_/src/Director.h`:
```cpp
#ifndef _TOPDOWNSHOOTER_DIRECTOR_H__
#define _TOPDOWNSHOOTER_DIRECTOR_H__

#include "apps/_MyApp_/src/Audio/AudioSystem.h"
#include "apps/_MyApp_/src/VFX/VFXSystem.h"
#include "<Physics>/physics_system.h"

namespace TopdownShooter
{
    /// @brief Client-side Director — PhysicsSystem + AudioSystem + VFXSystem 집계 싱글톤.
    /// @details
    ///   - **`SJH::Scene::Director` (engine) 와 무관** — namespace 분리, Cocos cc::Director 정통 *별도 인스턴스*.
    ///   - main 의 startup 에서 Init, render 에서 Update(dt), shutdown 에서 Shutdown 호출.
    ///   - 멤버는 값 보유 (싱글톤 자체 라이프타임 = 프로그램 종료까지).
    class Director
    {
      public:
        static Director& Get();

        void Init();
        void Update(float dt);
        void Shutdown();

        Audio::AudioSystem&            Audio()   { return mAudio; }
        VFX::VFXSystem&                VFX()     { return mVFX; }
        TopdownShooter::PhysicsSystem& Physics() { return mPhysics; }

        Director(const Director&)            = delete;
        Director& operator=(const Director&) = delete;
        Director(Director&&)                 = delete;
        Director& operator=(Director&&)      = delete;

      private:
        Director()  = default;
        ~Director() = default;

        Audio::AudioSystem            mAudio;
        VFX::VFXSystem                mVFX;
        TopdownShooter::PhysicsSystem mPhysics;
    };
}

#endif // _TOPDOWNSHOOTER_DIRECTOR_H__
```

**주의** — `PhysicsSystem` 의 namespace 가 무엇인지 확인 필요. 정확한 명칭으로 교체:

Run:
```bash
grep -n "namespace" <apps>/_MyApp_/src/Physics/physics_system.h
```

출력에서 namespace 를 발견 (예: `namespace TopdownShooter::Physics` 등) → `Director.h` 의 `TopdownShooter::PhysicsSystem` 부분을 *정확한 namespace* 로 교체.

- [ ] **Step 2: `Director.cpp` 작성**

`<apps>/_MyApp_/src/Director.cpp`:
```cpp
#include "Director.h"

#include <<spdlog>/spdlog.h>

namespace TopdownShooter
{
    Director& Director::Get()
    {
        static Director instance;
        return instance;
    }

    void Director::Init()
    {
        mAudio.Init();
        mVFX.Init(/*maxSprites=*/8000);
        mPhysics.Init();
        spdlog::info("[Director] init OK (Audio + VFX + Physics)");
    }

    void Director::Update(float dt)
    {
        mAudio.Update(dt);
        mVFX.Update(dt);
        mPhysics.Step(dt);
    }

    void Director::Shutdown()
    {
        // 역순 — Physics 가 다른 시스템 참조 없으므로 임의 순서 OK 이지만 명시.
        mPhysics.Shutdown();
        mVFX.Shutdown();
        mAudio.Shutdown();
        spdlog::info("[Director] shutdown OK");
    }
}
```

- [ ] **Step 3: `apps/_MyApp_/src/CMakeLists.txt` 에 Director.cpp 합류**

확인 — 현재 `CMakeLists.txt` 가 어떻게 구성되어 있는지:

Run:
```bash
cat apps/_MyApp_/src/CMakeLists.txt
```

만약 STATIC 라이브러리가 없고 *서브디렉토리만* 모으는 우산 INTERFACE 라면, Director.{h,cpp} 를 위한 새 라이브러리 정의 필요. 기존 구조:
```cmake
add_subdirectory(Algebraic)
add_subdirectory(Entity)
add_subdirectory(InputHandler)
add_subdirectory(Physics)
add_subdirectory(Audio)
add_subdirectory(VFX)
add_subdirectory(Tween)

add_library(myapp_client INTERFACE)
add_library(MyApp::Client ALIAS myapp_client)

target_link_libraries(myapp_client INTERFACE
    MyApp::Algebraic
    MyApp::Entity
    MyApp::InputHandler
    MyApp::Physics
    MyApp::Audio
    MyApp::VFX
    MyApp::Tween
)
```

→ Director 는 *top-level* 에 위치 — 별도 STATIC 라이브러리 만들기. 다음 추가:
```cmake
# M5 CL3 — Director 싱글톤 (Audio + VFX + Physics 집계)
add_library(myapp_director STATIC
    Director.cpp
)
add_library(MyApp::Director ALIAS myapp_director)
target_include_directories(myapp_director
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(myapp_director
    PUBLIC
        MyApp::Audio
        MyApp::VFX
        MyApp::Physics
    PRIVATE
        spdlog
)
target_compile_features(myapp_director PUBLIC cxx_std_17)
```

그리고 INTERFACE 우산에 추가:
```cmake
target_link_libraries(myapp_client INTERFACE
    MyApp::Algebraic
    MyApp::Entity
    MyApp::InputHandler
    MyApp::Physics
    MyApp::Audio
    MyApp::VFX
    MyApp::Tween
    MyApp::Director   # M5 CL3
)
```

- [ ] **Step 4: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 5: 사용자 commit 트리거 확인 후**:
```bash
git add <apps>/_MyApp_/src/Director.h <apps>/_MyApp_/src/Director.cpp apps/_MyApp_/src/CMakeLists.txt
git commit -m "[dev] : TopdownShooter::Director 신설 — Audio+VFX+Physics 집계 (M5 CL3)"
```

---

# Phase 4 — Client Director 통합

## Task 10: `main.cpp` 의 mPhysics → Director 로 이동

**Files:**
- Modify: `apps/_MyApp_/main.cpp`

- [ ] **Step 1: `main.cpp` 의 include 갱신**

기존:
```cpp
#include "<Physics>/physics_system.h"
```

다음으로 교체 (Director 가 PhysicsSystem 을 transitively include):
```cpp
#include "Director.h"
```

- [ ] **Step 2: `mPhysics` 멤버 제거 + Director 사용 치환**

main.cpp 내 모든 `mPhysics` 사용처를 `TopdownShooter::Director::Get().Physics()` 로 교체:

Run:
```bash
grep -n "mPhysics" apps/_MyApp_/main.cpp
```

각 라인에서:
- `mPhysics.Init()` → `TopdownShooter::Director::Get().Physics().Init()` (또는 Director::Init() 에 통합되므로 *제거*)
- `mPhysics.Step(dt)` → `TopdownShooter::Director::Get().Update(dt)` (Director 가 Physics.Step 포함 — 호출 통합)
- `mPhysics.SyncToTransform(...)` → `TopdownShooter::Director::Get().Physics().SyncToTransform(...)`
- `mPhysics.Shutdown()` → `TopdownShooter::Director::Get().Shutdown()` (Director 가 통합)

멤버 선언 제거 (보통 클래스 멤버 섹션 — 보통 `PhysicsSystem mPhysics;` 한 줄):
```cpp
// PhysicsSystem mPhysics;   // M5 — Director 로 이동, 줄 제거
```

- [ ] **Step 3: startup() 첫 줄에 Director::Init() 호출 추가**

`startup()` 진입부 (보통 `auto& reg = SJH::ResourceRegistry::Get();` 부근) 위쪽에:
```cpp
TopdownShooter::Director::Get().Init();
```

- [ ] **Step 4: shutdown() 마지막에 Director::Shutdown() 호출 추가**

`shutdown()` 안에 `mPhysics.Shutdown()` 또는 그 자리에:
```cpp
TopdownShooter::Director::Get().Shutdown();
```

- [ ] **Step 5: render() 의 Update 흐름에 Director::Update(dt) 통합**

기존:
```cpp
SJH::Scene::Director::Get().Update(dt);
mPhysics.Step(dt);
```

다음으로 교체:
```cpp
TopdownShooter::Director::Get().Update(dt);  // Audio + VFX + Physics.Step 일괄
SJH::Scene::Director::Get().Update(dt);       // engine scene (Actor.Update)
```

- [ ] **Step 6: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 7: 실행 검증 — 기존 동작 유지**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

Expected:
- spdlog 로그에 `[AudioSystem] init OK`, `[VFXSystem] init OK`, `[Director] init OK` 순으로 출력
- 기존 sprite 애니메이션 정상 (mPhysics 가 Director 안으로 이동했어도 동작 동일)
- 종료 시 `[Director] shutdown OK` 출력
- crash 0, leak 0 (FMOD/Effekseer 시스템만 init/release, 자원 로드 0 단계)

ESC 또는 창 닫기로 종료.

- [ ] **Step 8: 사용자 commit 트리거 확인 후**:
```bash
git add apps/_MyApp_/main.cpp
git commit -m "[dev] : main.cpp 에 Director 통합 — mPhysics 폐기 (M5)"
```

---

# Phase 5 — Client Leaf 활성화 (LP1~LP4)

## Task 11: `FmodStudioPlayable` 활성화 (T3) + main.cpp BGM 부착

**Files:**
- Modify: `apps/_MyApp_/src/Audio/FmodStudioPlayable.h`
- Modify: `apps/_MyApp_/src/Audio/FmodStudioPlayable.cpp`
- Modify: `apps/_MyApp_/main.cpp` (startup 에 BGM 부착)

- [ ] **Step 1: `FmodStudioPlayable.h` 활성화**

기존 skeleton 의 `[[maybe_unused]]` 제거 + Pause/OnPlay/OnStop/OnUpdate 정식 선언. 기존 fmod_studio_playable.h (rename 후 FmodStudioPlayable.h) 를 다음으로 *교체*:

```cpp
#ifndef _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__
#define _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__

#include "playable/playable_base.h"

namespace FMOD::Studio { class EventDescription; class EventInstance; }

namespace TopdownShooter::Audio
{
    /// @brief FMOD Studio EventDescription 을 *createInstance + start* 하여 재생 (M5 T3).
    /// @details
    ///   - EventDescription = 외부 owner (AudioSystem 캐시). EventInstance = 자체 owner.
    ///   - OnPlay 마다 새 instance 생성 (one-shot 도 가능, loop event 는 isLoop_=true 동반).
    class FmodStudioPlayable : public SJH::Playable::PlayableBase
    {
      public:
        explicit FmodStudioPlayable(::FMOD::Studio::EventDescription* desc);
        ~FmodStudioPlayable() override;

        void Pause() override;

      protected:
        void OnPlay() override;
        void OnStop() override;
        void OnUpdate(float dt) override;

      private:
        ::FMOD::Studio::EventDescription* mDesc     = nullptr;
        ::FMOD::Studio::EventInstance*    mInstance = nullptr;
    };
}

#endif // _TOPDOWNSHOOTER_AUDIO_FMODSTUDIOPLAYABLE_H__
```

- [ ] **Step 2: `FmodStudioPlayable.cpp` 활성화**

기존 skeleton 의 vtable anchor 만 있는 .cpp 를 다음으로 교체:
```cpp
#include "FmodStudioPlayable.h"

#include <fmod/fmod_studio.hpp>
#include <<spdlog>/spdlog.h>

namespace TopdownShooter::Audio
{
    FmodStudioPlayable::FmodStudioPlayable(::FMOD::Studio::EventDescription* desc)
        : mDesc(desc)
    {
        if (!mDesc) spdlog::warn("[FmodStudioPlayable] ctor: desc=nullptr (event 미존재?)");
    }

    FmodStudioPlayable::~FmodStudioPlayable()
    {
        if (mInstance) { mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE); mInstance->release(); mInstance = nullptr; }
    }

    void FmodStudioPlayable::Pause()
    {
        SJH::Playable::PlayableBase::Pause();   // paused_ = true
        if (mInstance) mInstance->setPaused(true);
    }

    void FmodStudioPlayable::OnPlay()
    {
        if (!mDesc) return;
        mDesc->createInstance(&mInstance);
        if (mInstance) mInstance->start();
    }

    void FmodStudioPlayable::OnStop()
    {
        if (mInstance)
        {
            mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
            mInstance->release();
            mInstance = nullptr;
        }
    }

    void FmodStudioPlayable::OnUpdate(float /*dt*/)
    {
        if (!mInstance) return;
        FMOD_STUDIO_PLAYBACK_STATE state;
        if (mInstance->getPlaybackState(&state) == FMOD_OK
            && state == FMOD_STUDIO_PLAYBACK_STOPPED && !isLoop_)
        {
            finished_ = true;
        }
    }
}
```

- [ ] **Step 3: main.cpp 의 startup() 에 BGM 부착**

`startup()` 의 Director.Init() 호출 직후에 추가:

```cpp
// === M5 T3 — BGM (FMOD Studio) ===
{
    auto& audio = TopdownShooter::Director::Get().Audio();
    audio.LoadBank("resources/banks/Master.bank");
    audio.LoadBank("resources/banks/Master.strings.bank");

    auto* bgmEvent = audio.LoadEvent("event:/BGM");
    if (bgmEvent)
    {
        auto* actor = SJH::Scene::Director::Get().Root().AddChild("BgmActor");
        auto* p = actor->AddComponent<TopdownShooter::Audio::FmodStudioPlayable>(bgmEvent);
        p->SetIsLoop(true);
        p->Play();
    }
}
```

main.cpp 의 include 추가:
```cpp
#include "apps/_MyApp_/src/Audio/FmodStudioPlayable.h"
```

- [ ] **Step 4: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 5: 실행 검증 — BGM loop 재생**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

Expected:
- spdlog: `[AudioSystem::LoadBank] OK resources/banks/Master.bank` + Strings 둘 다 출력
- 즉시 BGM 재생 시작 (audible — 헤드폰/스피커 확인)
- 10초 이상 들어 loop 동작 확인
- ESC 또는 창 닫기 → `[AudioSystem] shutdown OK` → 깔끔히 정지 (audio click/pop 없음)

만약 BGM 안 들리면:
- 시스템 볼륨 확인
- `resources/banks/` 가 실행 디렉토리에 복사되어 있는지: `ls build_ninja/apps/_MyApp_/resources/banks/`
- `event:/BGM` 이름이 정확한지 (<audio_demo>/demo2/main.cpp:103 참조)

- [ ] **Step 6: 사용자 commit 트리거 확인 후**:
```bash
git add apps/_MyApp_/src/Audio/FmodStudioPlayable.h apps/_MyApp_/src/Audio/FmodStudioPlayable.cpp apps/_MyApp_/main.cpp
git commit -m "[dev] : FmodStudioPlayable 활성화 + BGM 부착 (M5 LP1)"
```

---

## Task 12: `FmodPlayable` 활성화 (T1) + 마우스 좌클릭 Laser.wav 부착

**Files:**
- Copy: `apps/_MyApp_/resources/audio/Laser.wav` (수동 — 사용자 또는 git mv/cp)
- Modify: `apps/_MyApp_/src/Audio/FmodPlayable.h`
- Modify: `apps/_MyApp_/src/Audio/FmodPlayable.cpp`
- Modify: `apps/_MyApp_/main.cpp` (onMouseButton)

- [ ] **Step 1: Laser.wav 복사**

Run:
```bash
mkdir -p apps/_MyApp_/resources/audio
cp apps/effekseer_demo/demo1/resources/Sound/Laser.wav apps/_MyApp_/resources/audio/
ls apps/_MyApp_/resources/audio/Laser.wav
```

Expected: 파일 존재 확인.

- [ ] **Step 2: `FmodPlayable.h` 활성화**

기존 skeleton 을 다음으로 교체:
```cpp
#ifndef _TOPDOWNSHOOTER_AUDIO_FMODPLAYABLE_H__
#define _TOPDOWNSHOOTER_AUDIO_FMODPLAYABLE_H__

#include "playable/playable_base.h"
#include "resource_registry/sound.h"   // SJH::Sound

namespace FMOD { class System; class Channel; }

namespace TopdownShooter::Audio
{
    /// @brief FMOD Core API .wav/.ogg 재생 leaf Playable (M5 T1).
    class FmodPlayable : public SJH::Playable::PlayableBase
    {
      public:
        FmodPlayable(::FMOD::System* sys, SJH::Sound* sound);
        ~FmodPlayable() override;

        void Pause() override;

      protected:
        void OnPlay() override;
        void OnStop() override;
        void OnUpdate(float dt) override;

      private:
        ::FMOD::System*  mSys     = nullptr;
        SJH::Sound*      mSound   = nullptr;
        ::FMOD::Channel* mChannel = nullptr;
    };
}

#endif // _TOPDOWNSHOOTER_AUDIO_FMODPLAYABLE_H__
```

- [ ] **Step 3: `FmodPlayable.cpp` 활성화**

```cpp
#include "FmodPlayable.h"

#include <fmod/fmod.hpp>
#include <<spdlog>/spdlog.h>

namespace TopdownShooter::Audio
{
    FmodPlayable::FmodPlayable(::FMOD::System* sys, SJH::Sound* sound)
        : mSys(sys), mSound(sound)
    {
        if (!mSys || !mSound) spdlog::warn("[FmodPlayable] ctor: sys 또는 sound nullptr");
    }

    FmodPlayable::~FmodPlayable()
    {
        if (mChannel) { mChannel->stop(); mChannel = nullptr; }
    }

    void FmodPlayable::Pause()
    {
        SJH::Playable::PlayableBase::Pause();
        if (mChannel) mChannel->setPaused(true);
    }

    void FmodPlayable::OnPlay()
    {
        if (!mSys || !mSound) return;
        mSys->playSound(mSound->Raw(), nullptr, /*paused=*/false, &mChannel);
    }

    void FmodPlayable::OnStop()
    {
        if (mChannel) { mChannel->stop(); mChannel = nullptr; }
    }

    void FmodPlayable::OnUpdate(float /*dt*/)
    {
        if (!mChannel) return;
        bool playing = false;
        mChannel->isPlaying(&playing);
        if (!playing && !isLoop_) finished_ = true;
    }
}
```

- [ ] **Step 4: main.cpp 에서 Laser.wav 로드 + 마우스 클릭 부착**

`startup()` 의 BGM 블록 직후에 추가:
```cpp
// === M5 T1 — Shot SFX 로드 (마우스 클릭 시 재생) ===
{
    auto& reg = SJH::ResourceRegistry::Get();
    auto* sys = TopdownShooter::Director::Get().Audio().GetSystem();
    reg.CreateSound(sys, "shot", "resources/audio/Laser.wav");
}
```

`onMouseButton` (또는 비슷한 핸들러) 에 leftClick 분기:

```cpp
void onMouseButton(int button, int action) override
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        auto& reg   = SJH::ResourceRegistry::Get();
        auto& audio = TopdownShooter::Director::Get().Audio();
        auto* shot  = reg.FindSound("shot");
        if (shot)
        {
            auto* actor = SJH::Scene::Director::Get().Root().AddChild("ShotActor");
            auto* p = actor->AddComponent<TopdownShooter::Audio::FmodPlayable>(audio.GetSystem(), shot);
            p->Play();
        }
    }
}
```

main.cpp include 추가:
```cpp
#include "apps/_MyApp_/src/Audio/FmodPlayable.h"
```

**주의** — `AddChild` 시 매 클릭마다 ShotActor 누적된다. 일회성 Actor 의 자동 destroy 가 본 프로젝트 Scene 에 정착되어 있는지 확인:

Run:
```bash
grep -n "AutoDestroy\|RemoveChild\|finished" src/scene/actor.h src/scene/scene.h
```

자동 정리가 없다면 본 M5 검증 목적상 한 가지 ShotActor 를 재사용하는 패턴으로 변경:
- startup 에서 1회 AddChild("ShotActor") + AddComponent<FmodPlayable>
- onMouseButton 에서 그 component 의 `Stop()` + `Play()` 호출 (Stop 가 reset, Play 가 처음부터)

- [ ] **Step 5: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 6: 실행 검증**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

Expected:
- BGM 정상 (Task 11)
- 마우스 좌클릭 → "쉭" 발사음 (Laser.wav) 즉시 재생
- 빠르게 5회 클릭 → 매 클릭마다 발사음 *겹쳐* 재생 (FMOD::Channel 이 매 호출마다 새로 받음)
- ESC 종료 시 정상

- [ ] **Step 7: 사용자 commit 트리거 확인 후**:
```bash
git add apps/_MyApp_/resources/audio/Laser.wav apps/_MyApp_/src/Audio/FmodPlayable.h apps/_MyApp_/src/Audio/FmodPlayable.cpp apps/_MyApp_/main.cpp
git commit -m "[dev] : FmodPlayable 활성화 + Laser.wav 마우스 클릭 부착 (M5 LP2)"
```

---

## Task 13: `EffekseerPlayable` 활성화 (T2) + distortion.efk 부착

**Files:**
- Copy: `apps/_MyApp_/resources/vfx/distortion.efk`
- Modify: `apps/_MyApp_/src/VFX/EffekseerPlayable.h` (TrackPolicy enum 추가)
- Modify: `apps/_MyApp_/src/VFX/EffekseerPlayable.cpp`
- Modify: `apps/_MyApp_/main.cpp` (VFXSystem.Draw 호출 + 클릭 시 spawn)

- [ ] **Step 1: distortion.efk 복사**

Run:
```bash
mkdir -p apps/_MyApp_/resources/vfx
cp apps/effekseer_demo/demo1/resources/distortion.efk apps/_MyApp_/resources/vfx/
ls apps/_MyApp_/resources/vfx/distortion.efk
```

Expected: 파일 존재.

- [ ] **Step 2: `EffekseerPlayable.h` 활성화 + TrackPolicy enum**

기존 skeleton 을 다음으로 교체:
```cpp
#ifndef _TOPDOWNSHOOTER_VFX_EFFEKSEERPLAYABLE_H__
#define _TOPDOWNSHOOTER_VFX_EFFEKSEERPLAYABLE_H__

#include "playable/playable_base.h"
#include "resource_registry/effect.h"   // SJH::Effect
#include <Effekseer.h>
#include <vmath.h>

namespace TopdownShooter::VFX
{
    /// @brief 3D 위치 추적 정책 (Phase 4 결정).
    enum class TrackPolicy
    {
        Static,        // OnPlay 시 1회 SetLocation (단발 muzzle/폭발)
        FollowOwner    // OnUpdate 매 frame Actor.Transform.Translate 추적 (오라/이펙트)
    };

    /// @brief Effekseer Effect spawn + lifecycle 관리 leaf Playable (M5 T2).
    class EffekseerPlayable : public SJH::Playable::PlayableBase
    {
      public:
        EffekseerPlayable(::Effekseer::ManagerRef manager,
                          SJH::Effect*            effect,
                          const vmath::vec3&      spawnPos = vmath::vec3(0.0f),
                          TrackPolicy             track    = TrackPolicy::Static);
        ~EffekseerPlayable() override;

      protected:
        void OnPlay() override;
        void OnStop() override;
        void OnUpdate(float dt) override;

      private:
        ::Effekseer::ManagerRef mManager;
        SJH::Effect*            mEffect   = nullptr;
        ::Effekseer::Handle     mHandle   = -1;   // -1 = invalid
        vmath::vec3             mSpawnPos{0.0f};
        TrackPolicy             mTrack    = TrackPolicy::Static;
    };
}

#endif // _TOPDOWNSHOOTER_VFX_EFFEKSEERPLAYABLE_H__
```

- [ ] **Step 3: `EffekseerPlayable.cpp` 활성화**

```cpp
#include "EffekseerPlayable.h"

#include "scene/actor.h"        // GetOwner() (FollowOwner 정책에서 Transform 조회)
#include <<spdlog>/spdlog.h>

namespace TopdownShooter::VFX
{
    EffekseerPlayable::EffekseerPlayable(::Effekseer::ManagerRef manager,
                                         SJH::Effect*            effect,
                                         const vmath::vec3&      spawnPos,
                                         TrackPolicy             track)
        : mManager(manager), mEffect(effect), mSpawnPos(spawnPos), mTrack(track)
    {
        if (!mManager || !mEffect) spdlog::warn("[EffekseerPlayable] ctor: manager 또는 effect nullptr");
    }

    EffekseerPlayable::~EffekseerPlayable()
    {
        if (mManager && mHandle >= 0) { mManager->StopEffect(mHandle); mHandle = -1; }
    }

    void EffekseerPlayable::OnPlay()
    {
        if (!mManager || !mEffect) return;
        mHandle = mManager->Play(mEffect->Ref(),
                                 ::Effekseer::Vector3D(mSpawnPos[0], mSpawnPos[1], mSpawnPos[2]));
    }

    void EffekseerPlayable::OnStop()
    {
        if (mManager && mHandle >= 0)
        {
            mManager->StopEffect(mHandle);
            mHandle = -1;
        }
    }

    void EffekseerPlayable::OnUpdate(float /*dt*/)
    {
        if (!mManager) return;

        // FollowOwner — Actor 의 Transform.Translate 를 매 frame 추적
        if (mTrack == TrackPolicy::FollowOwner && mHandle >= 0)
        {
            if (auto* owner = GetOwner())
            {
                const auto& p = owner->Transform.Translate;
                mManager->SetLocation(mHandle, ::Effekseer::Vector3D(p[0], p[1], p[2]));
            }
        }

        // 자연 종료 — Effect 가 더 이상 존재 안 하면 finished
        if (mHandle >= 0 && !mManager->Exists(mHandle) && !isLoop_)
        {
            mHandle    = -1;
            finished_  = true;
        }
    }
}
```

- [ ] **Step 4: main.cpp 의 render() 에 VFXSystem::Draw 호출 추가**

기존 `render()` 의 마지막 (SceneRenderer.Draw 직후, swap 전) 에 추가:

```cpp
// === M5 — Effekseer 렌더 ===
{
    auto& vfx = TopdownShooter::Director::Get().VFX();
    // view / proj 행렬 — Scene 의 active camera 에서 추출.
    auto* cam = SJH::Scene::Director::Get().GetActiveCamera();
    if (cam)
    {
        vfx.Draw(cam->GetViewMatrix(), cam->GetProjectionMatrix());
    }
}
```

**주의** — `GetViewMatrix()` / `GetProjectionMatrix()` 가 `const float*` 를 반환하는지 확인:
```bash
grep -n "GetViewMatrix\|GetProjectionMatrix\|view_\|proj_" src/scene/camera.h
```

만약 vmath::mat4 반환이면:
```cpp
vmath::mat4 view = cam->GetViewMatrix();
vmath::mat4 proj = cam->GetProjectionMatrix();
vfx.Draw(&view[0][0], &proj[0][0]);   // mat4 의 첫 원소 ptr 이 column-major float[16]
```

- [ ] **Step 5: main.cpp 의 startup() 에 distortion.efk 로드 + 클릭 시 spawn**

`startup()` 의 Shot SFX 블록 직후:
```cpp
// === M5 T2 — Muzzle VFX 로드 ===
{
    auto& reg = SJH::ResourceRegistry::Get();
    auto  mgr = TopdownShooter::Director::Get().VFX().GetManager();
    reg.CreateEffect(mgr, "muzzle", u"resources/vfx/distortion.efk");
}
```

`onMouseButton` 의 좌클릭 분기 (Task 12) 안에 추가:
```cpp
auto* muzzle = reg.FindEffect("muzzle");
if (muzzle)
{
    auto  mgr = TopdownShooter::Director::Get().VFX().GetManager();
    auto* vfxActor = SJH::Scene::Director::Get().Root().AddChild("MuzzleActor");
    auto* p = vfxActor->AddComponent<TopdownShooter::VFX::EffekseerPlayable>(
        mgr, muzzle, /*spawnPos*/ vmath::vec3(0.0f), TopdownShooter::VFX::TrackPolicy::Static);
    p->Play();
}
```

include 추가:
```cpp
#include "apps/_MyApp_/src/VFX/EffekseerPlayable.h"
```

- [ ] **Step 6: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 7: 실행 검증 — muzzle VFX 시각 확인**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

Expected:
- BGM 정상
- 마우스 좌클릭 → Laser.wav + muzzle VFX (distortion 효과) *동시* 발생
- VFX 가 카메라 화면 안에 보이는지 (보통 (0,0,0) 부근). 안 보이면 카메라 위치 조정 또는 spawnPos 변경
- 종료 시 정상

- [ ] **Step 8: 사용자 commit 트리거 확인 후**:
```bash
git add apps/_MyApp_/resources/vfx/distortion.efk apps/_MyApp_/src/VFX/EffekseerPlayable.h apps/_MyApp_/src/VFX/EffekseerPlayable.cpp apps/_MyApp_/main.cpp
git commit -m "[dev] : EffekseerPlayable 활성화 + distortion.efk 마우스 클릭 부착 + VFXSystem.Draw (M5 LP3)"
```

---

## Task 14: `TweenPlayable` 활성화 (T4) + G 키 카메라 shake

**Files:**
- Modify: `apps/_MyApp_/src/Tween/TweenPlayable.h`
- Modify: `apps/_MyApp_/main.cpp` (onKey 핸들러)

- [ ] **Step 1: `TweenPlayable.h` 활성화**

기존 skeleton 을 다음으로 교체:
```cpp
#ifndef _TOPDOWNSHOOTER_TWEEN_TWEENPLAYABLE_H__
#define _TOPDOWNSHOOTER_TWEEN_TWEENPLAYABLE_H__

#include "playable/playable_base.h"
#include <<tweeny>/tweeny.h>
#include <cstdint>
#include <functional>

namespace TopdownShooter::Tween
{
    /// @brief Tweeny tween<T> wrap leaf Playable (M5 T4).
    /// @details
    ///   ⚠ step(int32_t ms) 오버로드 강제 — float 오버로드 사용 시 양 끝 깜빡임 폭주
    ///   (memory tweeny_step_overload_trap).
    template <typename T>
    class TweenPlayable : public SJH::Playable::PlayableBase
    {
      public:
        TweenPlayable(tweeny::tween<T> tween, std::function<void(T)> onStep)
            : mTween(std::move(tween)), mOnStep(std::move(onStep)) {}
        ~TweenPlayable() override = default;

      protected:
        void OnUpdate(float dt) override
        {
            // ⚠ int32_t (ms) 오버로드 명시
            int32_t dtMs = static_cast<int32_t>(dt * 1000.0f);
            T value = mTween.step(dtMs);
            if (mOnStep) mOnStep(value);
            if (mTween.progress() >= 1.0f && !isLoop_) finished_ = true;
        }

      private:
        tweeny::tween<T>       mTween;
        std::function<void(T)> mOnStep;
    };
}

#endif // _TOPDOWNSHOOTER_TWEEN_TWEENPLAYABLE_H__
```

- [ ] **Step 2: main.cpp 에 onKey 핸들러 — G 키 시 카메라 shake**

main.cpp 의 키 핸들러 (보통 `onKey` 또는 `glfwSetKeyCallback` 콜백) 에 추가:

```cpp
void onKey(int key, int action) override
{
    // (기존 키 핸들링)

    if (key == GLFW_KEY_G && action == GLFW_PRESS)
    {
        // === M5 T4 — 카메라 shake ===
        auto* shakeActor = SJH::Scene::Director::Get().Root().AddChild("ShakeActor");
        auto shakeTween = tweeny::from(0.0f).to(1.0f).during(100).via(tweeny::easing::sinusoidalInOut);
        shakeActor->AddComponent<TopdownShooter::Tween::TweenPlayable<float>>(
            std::move(shakeTween),
            [this](float v) {
                // sin 으로 ±5 픽셀 좌우 흔들기. 실제 camera offset 적용 위치는 카메라 클래스에 따라 다름.
                float offset = std::sin(v * 8.0f * 3.14159f) * 5.0f;
                // TODO: 실제 카메라 offset 필드 — TargetFollowableCameraController 의 멤버에 적용
                //       또는 단순 로깅으로 동작만 검증 (시각 검증 후 조정).
                spdlog::info("[shake] v={} offset={}", v, offset);
            }
        )->Play();
    }
}
```

include 추가:
```cpp
#include "apps/_MyApp_/src/Tween/TweenPlayable.h"
#include <<tweeny>/tweeny.h>
#include <cmath>
```

**주의** — 실제 카메라 offset 적용은 `TargetFollowableCameraController` 의 시그니처 확인 후 결정:
```bash
grep -n "offset\|shake\|cameraPos" <apps>/_MyApp_/src/InputHandler/TargetFollowableCameraController.h
```

offset 필드가 있으면 callback 안에서 직접 변경, 없으면 *로깅으로만 다형 동작 검증* (시각 효과는 후속 작업).

- [ ] **Step 3: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 4: 실행 검증 — G 키 shake 동작**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

Expected:
- 키 G 누르면 100ms 동안 spdlog 가 `[shake] v=... offset=...` 출력 (sin 곡선 따라 변동)
- 카메라 offset 필드를 callback 에서 갱신했다면 실제 화면 흔들림 보임
- shake 종료 후 `finished_=true` 로 다음 G 호출까지 비활성 (Stop+Play 또는 매번 신규 부착)

- [ ] **Step 5: 사용자 commit 트리거 확인 후**:
```bash
git add apps/_MyApp_/src/Tween/TweenPlayable.h apps/_MyApp_/main.cpp
git commit -m "[dev] : TweenPlayable 활성화 + G 키 카메라 shake (M5 LP4)"
```

---

# Phase 6 — Composite 통합 (CO1~CO2)

## Task 15: `onMouseButton` 을 Sequence(Effekseer → Parallel(Fmod + FmodStudio)) 로 교체

**Files:**
- Modify: `apps/_MyApp_/main.cpp`

- [ ] **Step 1: 기존 onMouseButton 의 leftClick 분기 전체를 Composite 로 교체**

기존 (Task 12/13 의 직접 leaf 부착):
```cpp
if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
{
    auto* shot   = reg.FindSound("shot");
    auto* muzzle = reg.FindEffect("muzzle");
    if (shot)   { actor1->AddComponent<FmodPlayable>(...)->Play(); }
    if (muzzle) { actor2->AddComponent<EffekseerPlayable>(...)->Play(); }
}
```

다음 Composite 형태로 교체:
```cpp
if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
{
    auto& reg   = SJH::ResourceRegistry::Get();
    auto& audio = TopdownShooter::Director::Get().Audio();
    auto& vfx   = TopdownShooter::Director::Get().VFX();
    auto* shot     = reg.FindSound("shot");
    auto* muzzle   = reg.FindEffect("muzzle");
    auto* slashEvt = audio.LoadEvent("event:/Slash");
    if (!shot || !muzzle || !slashEvt) return;

    // Sequence(
    //   Append( Effekseer.distortion ),
    //   Append( Parallel(
    //              Join( Fmod.Laser ),
    //              Join( FmodStudio.Slash )
    //          ))
    // )
    auto* composite = SJH::Scene::Director::Get().Root().AddChild("ShotComposite");
    auto* seq = composite->AddComponent<SJH::Playable::SequencePlayable>();

    seq->Append(std::make_unique<TopdownShooter::VFX::EffekseerPlayable>(
        vfx.GetManager(), muzzle, vmath::vec3(0.0f), TopdownShooter::VFX::TrackPolicy::Static));

    auto par = std::make_unique<SJH::Playable::ParallelPlayable>();
    par->Join(std::make_unique<TopdownShooter::Audio::FmodPlayable>(audio.GetSystem(), shot));
    par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(slashEvt));
    seq->Append(std::move(par));

    seq->Play();
}
```

include 추가:
```cpp
#include "playable/composite_playable.h"   // SequencePlayable / ParallelPlayable
```

**주의** — `SequencePlayable` / `ParallelPlayable` 의 정확한 namespace 와 fluent Builder 시그니처 확인:
```bash
grep -n "class SequencePlayable\|class ParallelPlayable\|Append\|Join" src/playable/composite_playable.h
```

시그니처가 spec §4 와 다르면 위 코드를 그에 맞춰 수정 (예: `Append(unique_ptr<IPlayable>)` vs `Append(IPlayable*)`).

- [ ] **Step 2: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 3: 실행 검증 — Composite 시각 확인**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

Expected:
- BGM 정상
- 마우스 좌클릭 →
  1. 먼저 muzzle VFX (distortion) 발생 — Effekseer 가 자연 수명 종료 (`Exists`=false) 까지 진행
  2. 이어서 Laser.wav + Slash event 가 *동시* 재생 (Parallel)
- Sequence 순서 (muzzle → 발사음) + Parallel (Laser ∥ Slash) 가 *시각/청각으로 분리* 확인
- 사용자 확인 ✅: "M3.5 잔여 Composite 시각 검증 완료"

- [ ] **Step 4: 사용자 commit 트리거 확인 후**:
```bash
git add apps/_MyApp_/main.cpp
git commit -m "[dev] : onMouseButton Composite (Sequence + Parallel) 통합 (M5 CO1)"
```

---

## Task 16: `onKey(G)` 를 Parallel(TweenShake + FmodStudio Damaged) 로 교체

**Files:**
- Modify: `apps/_MyApp_/main.cpp`

- [ ] **Step 1: 기존 onKey(G) 분기를 Parallel 로 교체**

기존 (Task 14 의 단일 TweenPlayable):
```cpp
if (key == GLFW_KEY_G && action == GLFW_PRESS)
{
    auto* shakeActor = ...->AddChild("ShakeActor");
    shakeActor->AddComponent<TweenPlayable<float>>(...);
}
```

다음으로 교체:
```cpp
if (key == GLFW_KEY_G && action == GLFW_PRESS)
{
    auto& audio = TopdownShooter::Director::Get().Audio();
    auto* damagedEvt = audio.LoadEvent("event:/Damaged");
    if (!damagedEvt) return;

    auto* damageActor = SJH::Scene::Director::Get().Root().AddChild("DamageComposite");
    auto* par = damageActor->AddComponent<SJH::Playable::ParallelPlayable>();

    auto shakeTween = tweeny::from(0.0f).to(1.0f).during(100).via(tweeny::easing::sinusoidalInOut);
    par->Join(std::make_unique<TopdownShooter::Tween::TweenPlayable<float>>(
        std::move(shakeTween),
        [this](float v) {
            float offset = std::sin(v * 8.0f * 3.14159f) * 5.0f;
            spdlog::info("[shake] v={} offset={}", v, offset);
        }
    ));
    par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(damagedEvt));

    par->Play();
}
```

- [ ] **Step 2: 빌드 검증**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: 정상.

- [ ] **Step 3: 실행 검증 — G 키 Parallel 동작**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

Expected:
- G 키 누르면 shake 로깅 + Damaged event 가 *동시* 시작
- 100ms 후 shake 종료, Damaged 는 event 길이에 따라 별도 종료

- [ ] **Step 4: 사용자 commit 트리거 확인 후**:
```bash
git add apps/_MyApp_/main.cpp
git commit -m "[dev] : onKey(G) Parallel(TweenShake + Damaged) 통합 (M5 CO2)"
```

---

# Phase 7 — POST_BUILD + 정리 (PB1~PB2, END1~END3)

## Task 17: POST_BUILD 자원 copy 검증

**Files:**
- Verify: `apps/_MyApp_/CMakeLists.txt` 의 POST_BUILD step

- [ ] **Step 1: 현재 POST_BUILD 정책 확인**

Run:
```bash
grep -n "POST_BUILD\|copy_directory\|copy_if_different" apps/_MyApp_/CMakeLists.txt
```

Expected: 보통 `${CMAKE_CURRENT_SOURCE_DIR}/resources` 전체를 실행 파일 디렉토리로 `copy_directory` 또는 `copy_if_different` 재귀 복사. 그러면 audio/, vfx/, banks/ 모두 자동 합류.

만약 *재귀 copy 가 아니라* 특정 파일 명시 패턴이라면 banks/, audio/, vfx/ 추가 명시 필요.

- [ ] **Step 2: 실행 디렉토리에 자원 모두 존재 확인**

Run:
```bash
ls build_ninja/apps/_MyApp_/resources/
ls build_ninja/apps/_MyApp_/resources/banks/
ls build_ninja/apps/_MyApp_/resources/audio/
ls build_ninja/apps/_MyApp_/resources/vfx/
```

Expected: 4 디렉토리 모두 존재, banks/Master.bank + Master.strings.bank, audio/Laser.wav, vfx/distortion.efk 모두 복사됨.

- [ ] **Step 3: FMOD dll copy 확인 (Windows 만 — macOS 는 dylib 자동)**

Windows 호스트에서:
```bash
ls build_msvc/apps/_MyApp_/Debug/   # 또는 Release/
```

Expected: `fmod.dll`, `fmodstudio.dll` (Debug 라면 `fmodL.dll`, `fmodstudioL.dll`) 이 실행 파일 옆에 존재. apps/_MyApp_/CMakeLists.txt 의 POST_BUILD copy_if_different 명령이 작동했는지.

(macOS 는 dylib 가 RPATH 로 link 되어 별도 copy 불요 — `otool -L build_ninja/apps/_MyApp_/_MyApp_` 로 fmod.dylib 경로 확인)

- [ ] **Step 4: leak 검증 (macOS 전용)**

Run:
```bash
sh <shell>/CMakeExecute.sh debug _MyApp_ leaks
```

Expected: 실행 후 ESC 종료 → leaks 출력에서 FMOD/Effekseer 관련 누수 0.

- [ ] **Step 5: 본 task 는 commit 대상 0 — 검증만**

---

## Task 18: 문서 정리 (END1~END3)

**Files:**
- Modify: `doc/topdown-shooter-progress.md` (M5 섹션 status)
- Modify: `.claude/CLAUDE.md` (SJH::resource_registry 의존)
- Modify (선택): `~/.claude/projects/.../memory/MEMORY.md`

- [ ] **Step 1: `doc/topdown-shooter-progress.md` M5 섹션 상태 갱신**

M5 섹션을 찾아 다음 항목으로 갱신:
- `M5 = 완료 (2026-05-26)` 로 표시
- commits 표에 본 작업의 commit hash 추가 (사용자가 push 후 hash 확보)
- "다음 = M4 (PlayerStateMachine + 발사 + 적)" 같이 다음 마일스톤 명시

구체 편집은 진행자가 progress.md 의 현 구조 따라 수행. 양식:
```markdown
## M5 — Leaf Playable (Effekseer + FMOD + Tweeny) [완료 2026-05-26]

| Task | 상태 | commit |
|---|---|---|
| SP1~SP4 ResourceRegistry 확장 | ✅ | <hash> |
| RN1 PascalCase rename | ✅ | <hash> |
| CL1~CL3 Subsystem + Director | ✅ | <hash> |
| LP1~LP4 leaf 활성화 | ✅ | <hash> |
| CO1~CO2 Composite 통합 | ✅ | <hash> |
| PB1 POST_BUILD 검증 | ✅ | (검증만) |

다음 = M4 (PlayerStateMachine — leaf Playable 활용해 AttackState 가 muzzle 시퀀스 호출)
```

- [ ] **Step 2: `.claude/CLAUDE.md` 의 SJH::resource_registry 행 갱신**

`### Src 모듈 레이아웃 (src/<module>/)` 표에서 `SJH::resource_registry` 행을 찾아 다음 내용으로 갱신:

기존:
> `SJH::resource_registry` | 텍스처/리소스 캐시 레지스트리 — `Texture / Material / Model` 만 캐싱 (SP3 시점). `Program / Mesh` 는 미지원 → 데모/app 이 임시 owner ...

다음 추가/교체:
> `SJH::resource_registry` | 텍스처/리소스 캐시 레지스트리 — `Texture / Material / Model / Program / Mesh / Framebuffer / UniformAtlas / Sound / Effect` 9종. **M5 에서 game_deps PUBLIC link 합류 (Sound=FMOD::Sound, Effect=Effekseer::EffectRef)** — SJH::engine 우산 link 하는 모든 consumer 가 FMOD/Effekseer 자동 합류. 자세한 *자원 보유 컨벤션* 은 `.claude/architecture.md §11.3` 필독

- [ ] **Step 3: memory 갱신 (선택)**

새 memory 후보:
- `resource_registry_game_deps.md` — "M5 시점 SJH::ResourceRegistry 가 game_deps PUBLIC link — SJH::engine 우산 link 하는 모든 consumer 가 FMOD/Effekseer 자동 합류. Sound/Effect 캐시 추가 정통."

만약 사용자가 동의하면 위 내용으로 신규 memory 작성 + MEMORY.md 인덱스 한 줄 추가.

- [ ] **Step 4: 사용자 commit 트리거 확인 후**:
```bash
git add doc/topdown-shooter-progress.md .claude/CLAUDE.md
git commit -m "[dev] : M5 완료 — progress.md + CLAUDE.md ResourceRegistry 갱신"
```

---

# Self-Review (plan 작성자 inline check)

**1. Spec coverage 점검:**

| spec §1 결정 | plan task |
|---|---|
| Phase 1 — Client Director | Task 9 (Director.h/.cpp) + Task 10 (main.cpp 통합) |
| Phase 2 — Engine ResourceRegistry::CreateSound/CreateEffect | Task 1 (game_deps PUBLIC) + Task 2 (Sound) + Task 3 (Effect) + Task 4 (메소드) |
| Phase 3 — Stop 즉시 정지 | Task 11/12 의 OnStop = channel/instance stop only (fade 없음) |
| Phase 4 — TrackPolicy enum | Task 13 의 EffekseerPlayable.h ctor 인자 + OnUpdate FollowOwner 분기 |
| Phase 5 — TweenPlayable M5 포함 | Task 14 (활성화) + Task 16 (Parallel 검증) |
| Phase 6 — 리소스 복사 | Task 12 (Laser.wav) + Task 13 (distortion.efk). banks 는 이미 복사됨 |
| Phase 6-tris — PascalCase | Task 6 (rename) — 7 파일 모두 명시 |
| 시각 검증 (spec §4) | Task 11 (BGM), 12 (Laser), 13 (muzzle), 14 (shake), 15 (Composite muzzle), 16 (Composite damage) — 모든 트리거 cover |
| game_deps 누출 영향 측정 (spec §6.1) | Task 5 (ENABLE_TESTING=ON 빌드 + ctest) |

→ 모든 spec 요구사항 cover ✅

**2. Placeholder scan:** TBD / TODO 검색 — 본 plan 안에 *spec scope 외 future work* 표기로 2 곳 (Task 14 의 카메라 offset 적용은 "TODO: 실제 카메라 offset 필드 ..." 인데 이는 *조건부 검증 path* — 카메라 컨트롤러 시그니처에 따라 *callback 안에서만 적용 또는 로깅으로만 검증* 양자택일을 위한 의도된 마커. 실행자가 검증 시점에 둘 중 하나 선택). 진짜 placeholder (구현 미정) 아님.

**3. Type consistency:**
- `SJH::Sound* / SJH::Effect*` — Task 2~4 + 12~13 일관 사용 ✅
- `::Effekseer::Handle (= int)` — Task 13 의 mHandle 타입 ✅ (spec §3.7 + plan Task 13 동일)
- `TopdownShooter::Director::Get()` — Task 9 정의 + Task 10/11/12/13/14/15/16 사용 일관 ✅
- `FMOD::Studio::EventDescription*` — Task 7 AudioSystem 반환 + Task 11/15/16 사용 일관 ✅
- `SequencePlayable.Append(unique_ptr) / ParallelPlayable.Join(unique_ptr)` — Task 15/16 사용 — *spec §4 + composite_playable.h 의 실제 시그니처 검증* 을 Task 15 step 1 의 grep 으로 안전망 둠 ✅

**4. Scope check:** 18 task — 단일 spec implementation plan 으로 적정. 하나의 마일스톤 (M5) cover, 다른 모듈 (M4 등) 침범 0.

---

# Execution Handoff

**Plan complete and saved to `doc/superpowers/plans/2026-05-26-m5-leaf-playables.md`.** Two execution options:

**1. Subagent-Driven (recommended)** — Fresh subagent per task, two-stage review between tasks, fast iteration. 본 plan 의 18 task 가 대부분 *독립적이지 않고 순차 의존* (SP1→SP2→SP3→SP4→SP5→RN1→...) 이라 subagent 가 매번 같은 컨텍스트 다시 로드. 본 case 는 *작업 단위가 크지 않고 의존이 강해서 inline 이 더 효율적* 일 가능성.

**2. Inline Execution** — 본 session 안에서 executing-plans 로 순차 batch 진행. 사용자 checkpoint 는 *각 commit 트리거 시점* 으로 자연 정착 (commit 트리거 = 사용자 명시 → 자동 review gate). 본 prj 의 *사용자 명시 commit 정책* 과 정합.

**어느 방식으로 진행할까요?** 추천: Inline Execution (본 plan 의 강한 순차 의존 + commit gate 가 이미 사용자 review 역할).