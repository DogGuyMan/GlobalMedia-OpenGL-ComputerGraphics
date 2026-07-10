# CLAUDE.md — 확장 전문 (2026-07-10 D-1 압축 이관 보존본)
> `.claude/CLAUDE.md`가 ~80줄 나침반으로 압축되며 이관된 상세 전문. 최신 나침반은 `.claude/CLAUDE.md`, 모듈별 상세는 `src/CLAUDE.md` 등 6개 모듈 CLAUDE.md + `ARCHITECTURE.md` + `doc/api/EngineAPI.md` 참조.

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OpenGL Computer Graphics — C++17 CMake project. 출발점은 SuperBible 7th Edition 코스워크였으나 현재 브랜치(`game/main`)는 *Core/Client 분리* + *Actor/Component 씬 그래프* + *SceneRenderer* 으로 전환된 데모/엔진 단계. 챕터/데모별로 독립된 실행 파일, 셰이더, 리소스를 갖는 구조. macOS/Linux (Ninja) 및 Windows (MSVC) 지원. 의존성은 **vcpkg (manifest 모드, 외부 `$env{VCPKG_ROOT}`)** 로 관리 — 게임/엔진 라이브러리(box2d/assimp/spdlog/tweeny/stb/catch2)는 vcpkg `find_package`, 잔류 의존(glfw3·sb7·Effekseer·FMOD·imgui)만 `lib/`·`include/` 사전 빌드 체크인 유지. (과거 "교수 제출용 vcpkg 미사용 + 전량 prebuilt 자체완결" 제약은 2026-06-19 평가 종료로 해제 → vcpkg 전이 2026-06-20 완료. builtin-baseline 은 로컬 vcpkg HEAD, box2d 2.4.1·assimp 5.4.3 만 override 핀.)

## Build Commands

```bash
# Configure (macOS/Linux)
cmake --preset ninja                                    # Debug
cmake --preset ninja-release                            # Release

# 데모별 빌드 + 실행 (현재 활성 타겟: _MyApp_ 단독 — apps/CMakeLists.txt 에서 나머지 5종 주석)
cmake --build --preset ninja --target _MyApp_           # _MyApp_만 빌드
cd build_ninja/apps/_MyApp_ && ./_MyApp_                # 실행 (리소스 상대경로 때문에 cd 필요)

# Windows (MSVC) — VS 2019 (저사양/학교 PC 기본)
cmake --preset msvc
cmake --build --preset msvc --target _MyApp_              # Debug
cmake --build --preset msvc-release --target _MyApp_      # Release

# Windows (MSVC) — VS 2022 (신형 머신 / CI)
cmake --preset msvc-2022
cmake --build --preset msvc-2022 --target _MyApp_         # Debug
cmake --build --preset msvc-2022-release --target _MyApp_ # Release

# ARM64 Windows 호스트에서는 x64 타겟 강제 필요 (사전 빌드된 lib가 x64)
cmake --preset msvc-2022 -A x64

# 통합 개발 CLI (구 shell/ 스크립트 통합 — 크로스 플랫폼 단일 진입점)
python3 scripts/dev.py all debug _MyApp_          # clean + configure + build + run
python3 scripts/dev.py all release migrate_demo   # release 빌드 + 실행
python3 scripts/dev.py run debug _MyApp_ --leaks  # 메모리 누수 체크 (macOS leaks)

# Windows 에서도 동일 (python 진입점 하나 — 플랫폼 자동 감지)
python scripts\dev.py all debug _MyApp_
python3 scripts/dev.py --help                     # 전체 서브커맨드 보기

cmake --list-presets                            # 모든 프리셋 보기
```

### 테스트 실행 (Catch2 v3 + CTest)

```bash
# test/ 는 2026-06-25~27 재구축 완료 — 구 21개(2026-06-20 폐기) 와 별개의 신규 설계, ctest 122개 (2026-06-27 기준):
#   구성 = test/{smoke, gpu, golden, golden_compare}
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target tests
ctest --test-dir build_ninja --output-on-failure
```

테스트는 **Catch2 v3 (vcpkg manifest, `find_package(Catch2 3 CONFIG REQUIRED)`)** 기반. 루트 CMakeLists.txt 의 `ENABLE_TESTING` 옵션(기본 OFF) 분기로 활성화. 구성 (2026-07-10 실측): `test/smoke`(빌드 스모크) / `test/gpu`(GL 컨텍스트 픽스처 + 셰이더 링크·상태캐시·roundtrip·상태누수·bitmap font) / `test/golden` + `test/golden_compare`(골든 이미지 비교 게이트). 정본 spec = [`doc/superpowers/specs/2026-06-27-test-expansion-design.md`](../doc/superpowers/specs/2026-06-27-test-expansion-design.md). (과거 `extern/Catch2` 서브모듈 + `test/support/` 헬퍼는 제거됨 — catch2 는 vcpkg 의존.)

## Active Target Management — CRITICAL

`apps/CMakeLists.txt` 는 데모 타겟을 `add_subdirectory(...)` 로 나열하지만 **한 번에 하나/소수만 활성화** 하는 것이 컨벤션. 새 데모를 빌드하려면 해당 줄의 주석을 해제해야 한다. 빌드 실패 시 가장 먼저 확인할 곳. (과거 `chapter1~9 / exercise* / engine_exercise* / extra* / modeltest / prevmidterm` 류 디렉토리는 본 브랜치(`game/main`)에서 제거되었음 — 다른 브랜치 참고.)

현재 `apps/` 하위 실재 디렉토리 (`git status` 시점):
- **활성 (`apps/CMakeLists.txt` 주석 해제됨 — `_MyApp_` 단독):**
  - `_MyApp_` — 탑다운 슈터 게임 (M1 + M2 P2 + M3 Box2D + **M3.5 Playable 코어 정착 완료** 2026-05-26, M5 leaf Playable(Effekseer/FMOD, `apps/_MyApp_/src/VFX/`·`Audio/`) 도입 완료). SJH::sprite atlas + Director + SceneRenderer + Material 패턴 + Player WASD (dt fps-independent + 대각이동) + `TargetFollowableCameraController` (별도 클래스) + **`SpriteSequencePlayable` (SpriteFrameClip 주입 — `SetIsLoop(true).Play()` 패턴; SpriteAnimator 폐기 완료 2026-05-26)** + `UniformAtlas` Fluent Builder (`LoadFromPNG().SetGrid(cols,rows)`). Box2D Client 한정 (`apps/_MyApp_/src/Physics/`) — `Components::Physics` abstract + `BoxBody/CircleBody` concrete + `IContactable` + `PhysicsLayer : uint64_t` + `FindPhysics(Actor*)`. M4 도메인 선행 — Stat + Entity Components (Life/Movement/Weapon + IMovable/IAttackable/ILivable/IDieable/IDamageable) + `PlayerActor` Pattern C factory (PoD Config). **FSM 모듈은 Stage 4 진화** (`SJH::FSM::StateMachine<TState, TOwner>` + `IFsmState<TOwner>` 그래프 응집, TTransit template parameter 제거) — 정본 spec [`doc/superpowers/specs/2026-05-25-fsm-object-state-machine-design.md`](../doc/superpowers/specs/2026-05-25-fsm-object-state-machine-design.md). **IPlayable 정본 spec** [`doc/superpowers/specs/2026-05-26-playable-component-interface-design.md`](../doc/superpowers/specs/2026-05-26-playable-component-interface-design.md) (7 결정 + Tweeny/DOTween 정통 Builder). M7 Stage FSM 구현 완료 (2026-06-04 — `apps/_MyApp_/src/Stage/State/` StageFSMState·StageStateMachine, Title/CombatPlay/Pause/GameOver). 진행 보고서 [`doc/topdown-shooter-progress.md`](../doc/topdown-shooter-progress.md). 정통 데모 — 다른 SJH::engine 사용 데모 작성 시 main.cpp 참조 우선순위.
- **임시 비활성 (주석 처리 — 5종):**
  - `migrate_demo` — 진행 중 마이그레이션 워크스페이스 (working tree 다수 변경)
  - `audio_demo` — FMOD Studio + ImGui 파라미터 데모
  - `box2d_demo` — Box2D v2.4.1 물리 데모. 내부 `common` STATIC + `demo1`/`demo2`/`demo3` 서브타겟 3종 (실 게임은 `_MyApp_` 가 흡수)
  - `effekseer_demo` — Effekseer 1.7.3.0 파티클 데모. 현재 `demo1` 1종
  - `tweeny_demo` — Tweeny 헤더 온리 트위닝 데모. `step(int32_t ms)` vs `step(float ratio)` 오버로드 함정 주의

## Architecture

### CMake 빌드 흐름 (root `CMakeLists.txt`)
1. `cmake/CXXStandard.cmake` — C++17, `_USE_MATH_DEFINES` 전역. Debug 시:
   - GCC/Clang: `-Wall -Werror -g -O0`, sb7 헤더의 `#warning`은 에러 미처리 (`-Wno-error=#warnings`), 일부 unused 경고 완화
   - MSVC: `/utf-8` (한글 주석 깨짐 방지) + `/Zc:__cplusplus` 항상, Debug에서는 narrowing 경고 `/wd4244 /wd4305 /wd4267` 침묵 (Release에서는 다시 활성화되어 실제 narrowing 버그 노출)
2. `cmake/Dependency.cmake` — 전이 의존(box2d/assimp/spdlog/tweeny/stb/catch2)은 vcpkg `find_package(X CONFIG REQUIRED)`, 잔류 의존(glfw3/sb7/Effekseer/FMOD)은 `lib/{macos,windows}/` 사전 빌드 IMPORTED 로 등록 → `project_deps` / `game_deps` INTERFACE 타겟으로 집약. (vcpkg toolchain 은 `CMakePresets.json` `base` 의 `CMAKE_TOOLCHAIN_FILE=$env{VCPKG_ROOT}/...`, manifest 는 루트 `vcpkg.json` + `vcpkg-configuration.json`(box2d arm64 테스트 버그 교정 overlay-port `vcpkg-overlay-ports/box2d/`).)
3. `src/` — 17개 코어 모듈 STATIC 라이브러리 (`SJH::<module>` ALIAS 컨벤션) — buffer, common, diagnostics, **fsm**, input, layout, material, object, **playable**, program, render, resource_registry, scene, shader, **sprite**, **text**, **timer**. 17개 모두를 묶은 **INTERFACE 우산 타겟 `SJH::engine`** ([src/CMakeLists.txt:19-40](src/CMakeLists.txt#L19-L40)) 로 단일 link 가능 — Cocos `libcocos2d.a` / Unreal Runtime / Godot `tools.dll` 통합 binary 정통. 각 모듈은 자체 `CMakeLists.txt` 로 의존성 명시 (`target_link_libraries`). 자세한 설계는 `.claude/architecture.md`. ※ 과거 `SJH::context` / `SJH::imgui` 모듈은 폐기됨. **M3.5 Playable 코어 정착 완료 (2026-05-26)** — `SJH::playable` 합류 (IPlayable + PlayableBase + Composite). spec §1.6 의 `SJH::sprite_sequence` 는 별도 모듈 안 되고 **`SJH::sprite` 안에 통합** (`sprite_sequence_playable.{h,cpp}` + `sprite_frame_clip.h`). 다음 = M5 leaf Playable (Effekseer/FMOD) — Client 거주. 자세한 진행은 [`doc/topdown-shooter-progress.md`](../doc/topdown-shooter-progress.md) M3.5 참조.
4. `apps/` — 활성화된 데모 실행 파일
5. `cmake/Doxygen.cmake` (+ `option(SJH_OPENGL_BUILD_DOCS ON)`) — `sjhopengl_setup_doxygen()` 로 `doxygen` 커스텀 타겟 등록 (`cmake --build build_ninja --target doxygen` → `doc/html/`). Doxygen 미설치 시 `find_package(Doxygen QUIET)` 가 조용히 스킵. 테스트는 `option(ENABLE_TESTING OFF)` 분기 (위 *테스트 실행* 섹션 참조). ※ `cmake/` 디렉토리에는 `CXXStandard.cmake` / `Dependency.cmake` / `Doxygen.cmake` **3개** 가 존재.

### Dependency layer (`cmake/Dependency.cmake`)
- **glfw3** / **sb7**: `IMPORTED STATIC`, Debug 시 `_d` 접미사 라이브러리 자동 선택
- **`project_deps`** (INTERFACE) = sb7 + glfw3 + OpenGL + 플랫폼 프레임워크
  - macOS: Cocoa, IOKit, CoreVideo, CoreFoundation + `GL_SILENCE_DEPRECATION`, `__glext_h_`
  - Windows: opengl32, gdi32, winmm
- 헤더는 `${CMAKE_SOURCE_DIR}/include` 에서 노출 (sb7.h, vmath.h, GL/ 등)
- **`game_deps`** (INTERFACE) = box2d + EffekseerRendererGL(+Effekseer) + assimp + spdlog + tweeny + stb_extra + (조건부) FMOD. 게임/엔진 챕터만 `project_deps` 와 함께 옵트인 링크 (`target_link_libraries(타겟 PRIVATE project_deps game_deps)`). 일반 챕터는 `project_deps` 만 링크해 물리/파티클 엔진을 끌어들이지 않는다. **box2d/assimp/spdlog/tweeny/stb 는 vcpkg `find_package` 타겟**(`box2d::box2d`/`assimp::assimp`/`spdlog::spdlog`/`tweeny`/`Stb_INCLUDE_DIR`) — 단 일부 모듈 CMakeLists 가 bare `assimp`/`spdlog` 로 링크해 동명 INTERFACE 래퍼로 흡수, stb 는 `stb_extra` INTERFACE 가 `Stb_INCLUDE_DIR` 전파. spdlog 의 `SPDLOG_COMPILED_LIB` 는 vcpkg 타겟이 자동 전파(수동 정의 제거). **Effekseer/FMOD 는 잔류 — `IMPORTED`** (Effekseer STATIC, FMOD SHARED). `game_deps` 의 `include/` SYSTEM 인클루드는 잔류 헤더(Effekseer/FMOD)용으로 유지. **FMOD Core + Studio** 는 독점 SDK 라 헤더 존재 시에만 등록되는 조건부 멤버 — Core 는 `include/fmod/fmod.h` 가드로 `fmod` SHARED IMPORTED (Release=`fmod`, Debug=`fmodL` 로깅 빌드), Studio (`.bank` 이벤트 시스템) 는 `include/fmod/fmod_studio.h` 가드로 `fmodstudio` 추가 + `fmod` INTERFACE 의존 (Studio 가 Core 심볼 참조). 미설치 환경은 해당 타겟이 안 만들어지고 STATUS 메시지만 → game 챕터 빌드 시 link 에러로 막힘. **둘 다 dynamic-only 라 game_deps 사용 챕터는 POST_BUILD 에서 `$<TARGET_FILE:fmod>` + (있으면) `$<TARGET_FILE:fmodstudio>` 를 실행 파일 옆으로 `copy_if_different` 필수** (자세한 셋업/등록/사용 패턴은 `doc/api/FMOD_Setup.md`).

### 챕터 CMakeLists.txt — 두 가지 패턴

**패턴 A (Full, chapter1~2):** `entry.h` + `entry.cpp` + `main.cpp`
- `sb7::application` 상속을 `entry.h/.cpp` 에 분리
- `project_deps` + `SJH` OBJECT 라이브러리 링크
- `src/` 하위 모듈 지원 (`${CHAPTER_NAME}_src` 타겟)

**패턴 B (Simplified, chapter3 이후 + 거의 모든 exercise):** `main.cpp` only
- 클래스 선언/구현/`DECLARE_MAIN` 모두 한 파일
- `project_deps` 만 링크
- 신규 챕터의 기본 패턴

공통: `get_filename_component(CHAPTER_NAME ...)` 으로 디렉토리명에서 타겟명 자동 추출. POST_BUILD 에서 `resources/` 전체가 실행 파일 디렉토리로 복사된다.

### 챕터 코드 패턴

```cpp
// 패턴 A — entry.h: 헤더 가드 __CHAPTER_N_ENTRY_H__ 형식
#ifndef __CHAPTER_1_ENTRY_H__
#define __CHAPTER_1_ENTRY_H__
namespace SJH::Chapter1 {
    class my_application : public sb7::application {
        virtual void render(double currentTime);
    };
}
#endif

// main.cpp
#include "entry.h"
DECLARE_MAIN(SJH::Chapter1::my_application);
```

```cpp
// 패턴 B — main.cpp 단일 파일
class my_application : public sb7::application {
    void render(double currentTime) override { ... }
};
DECLARE_MAIN(my_application);
```

### 리소스 & 셰이더
- 챕터별 리소스: `apps/chapterN/resources/`
- 셰이더: `apps/chapterN/resources/shaders/` (`.vert .frag .geom .tesc .tese .comp .glsl`)
- POST_BUILD 단계에서 `resources/` 가 실행 파일 디렉토리로 복사됨
- **상대경로로 리소스를 로드하므로 실행 파일 디렉토리에서 실행해야 한다** (`cd build_ninja/apps/chapterN && ./chapterN` 또는 shell 스크립트 사용)

### Src 모듈 레이아웃 (`src/<module>/`)

17개 STATIC 라이브러리가 `SJH::<module>` ALIAS 로 노출되고, 17개 모두를 묶은 INTERFACE 우산 `SJH::engine` 도 제공된다 (Core/Client 분리 — 옵션 A). 각 모듈은 자체 `CMakeLists.txt` 에서 의존성을 명시한다 (heuristic: 헤더에 노출되는 의존은 `PUBLIC`, .cpp 내부 전용은 `PRIVATE`).

| 모듈 | 책임 (요약) |
|---|---|
| `SJH::common` | 공통 유틸 (`common.h`), GL 로더 비의존 |
| `SJH::diagnostics` | GL 호출/셰이더/uniform/상태/엔진 단위 진단 + `GLValidate` Cat A–F |
| `SJH::buffer` | VBO/EBO 통합 RAII (Buffer) |
| `SJH::shader` | 셰이더 컴파일 + InfoLog (`Shader::CreateFromSource`) |
| `SJH::program` | 프로그램 링킹 + uniform 핸들/캐시 (`mUniformCache`) |
| `SJH::layout` | Vertex 레이아웃 정의 (`vertex.h` 포함), VAO + attribute setter |
| `SJH::material` | Phong/PBR Material 값 클래스 + `Texture*` / `Program*` 보관 |
| `SJH::object` | Mesh + Geometry 생성기 (Box/Plane/Cone/...) + `Light` / `Transform` POD |
| `SJH::scene` | Actor + Component + Scene 그래프 (`actor.h`, `components.h`, `compound_actor.h`). Actor 비상속 — 특수 속성은 Component 로만 |
| `SJH::sprite` | 2D 스프라이트 atlas (UV/픽셀아트 NEAREST). `ComputeUVRect` 자유 함수 + `UniformAtlas` (M1) + **`SpriteSequencePlayable` + `SpriteFrameClip` (M3.5 — spec §1.6 의 sprite_sequence 별도 모듈 대신 sprite 안에 통합 정착)**. stb_image/Texture 직접 호출 금지 — `SJH::Image::Load` + `SJH::Texture::CreateTexture` 위임. ※ 과거 `SpriteAnimator` 는 폐기 (2026-05-26) — `SpriteSequencePlayable` 가 상위 호환 |
| `SJH::fsm` *(M2 추가)* | `StateMachine<TState, TOwner>` (Aggregate Root) + `IFsmState<TOwner>` (Entity within Aggregate, `GetStateFlag`/`GetTransitFlag` self-identifying). Stage 4 — TTransit template parameter 폐기, 그래프 응집 패턴. 정본 spec [`doc/superpowers/specs/2026-05-25-fsm-object-state-machine-design.md`](../doc/superpowers/specs/2026-05-25-fsm-object-state-machine-design.md). 사용처: M4 PlayerStateMachine 도입 시점 (현재 사용 0) |
| `SJH::playable` *(M3.5 추가)* | `IPlayable` pure interface (Play/Pause/Stop/GetIsLoop/IsFinished) + `PlayableBase : IPlayable, Component` abstract (다중 상속, paused_/finished_/elapsed_/isLoop_ 보유, Update→OnUpdate hook) + `SequencePlayable` / `ParallelPlayable` Composite (`vector<unique_ptr<IPlayable>>` + Append/Insert/Join fluent Builder). 정본 spec [`doc/superpowers/specs/2026-05-26-playable-component-interface-design.md`](../doc/superpowers/specs/2026-05-26-playable-component-interface-design.md). leaf Playable (FmodPlayable/EffekseerPlayable) 은 *Client 거주* (game_deps 의존, M5 도입 예정) |
| `SJH::render` | DeviceContext (glUseProgram owner — SP2) + SceneRenderer (Light 컴포넌트 수집 + 모든 program 송신 — SP5 T3/T4/T5) + MeshPassProcessor |
| `SJH::input` | KeyboardInput\<TAction\> / MouseInput 드래그 등 입력 디스패치 |
| `SJH::resource_registry` | 텍스처/리소스 캐시 레지스트리 — `Texture / Material / Model / Program / Mesh / Framebuffer / UniformAtlas / Sound / Effect` 9종. **M5 (2026-05-26) 에서 game_deps PUBLIC link 합류** — Sound=`FMOD::Sound*` RAII wrap, Effect=`Effekseer::EffectRef` wrap. `SJH::engine` 우산 link 하는 모든 consumer 가 FMOD/Effekseer 자동 합류 (spec [`2026-05-26-m5-leaf-playables-design.md`](../doc/superpowers/specs/2026-05-26-m5-leaf-playables-design.md) §6.1). 자세한 *자원 보유 컨벤션* 은 `.claude/architecture.md §11.3` 필독 |
| `SJH::timer` *(spec 2026-06-01)* | 게임플레이 타이머 — `SJH::Timer::Timer` (단발/경과) + `MultipleTimer` (다중 트랙). BaseEntity 중앙 등록 핸들(`player.attack` / `life.iframe`·`life.die` 등)이 raw-float 분산 산술 대신 Timer 객체로 통합 |
| `SJH::text` *(spec 2026-06-02 — World Text)* | 월드 공간 텍스트 렌더링 — `BitmapFont` (`Glyph` atlas, `UniformAtlas` 기반) + `TextRenderer`. 네임스페이스 `SJH::Text` |
| `SJH::engine` *(INTERFACE)* | 위 17개 코어 모듈 우산 — `target_link_libraries(<demo> PRIVATE project_deps SJH::engine)` 한 줄로 전체 코어 link + include 자동 전파. M3.5 Playable 정착 (2026-05-26) — `SJH::playable` 합류, `sprite_sequence` 는 `SJH::sprite` 안에 통합 (별도 모듈 아님). `SJH::timer`(2026-06-01) + `SJH::text`(2026-06-02) 합류로 17 모듈 |

레거시 클래스(`Engine::Model::VAO` 등)는 일부 데모 `main.cpp` 에 인라인으로 남아 있을 수 있다 — 신규 데모는 가급적 `SJH::engine` 또는 필요한 `SJH::<module>` 만 직접 링크해 사용한다.

#### Diagnostics 세부

- `SJH::diagnostics` STATIC 라이브러리. 헤더에서 `GLuint`/`GLenum` 등을 쓰므로 `project_deps` 를 **PUBLIC** 전파 (GL 인클루드 경로 + OpenGL 링크가 consumer 로 전달됨).
- **★ 예외 모듈 지위 (cycle-exempt, 2026-06-28)**: diagnostics 는 *진단·에러검증·캡처용 엔진-독립 관측 모듈* 이라 전역 Skill `modular-build-discipline` 의 **무순환/의존-계층 규율에서 예외**다. 진단은 본질적으로 *어느 계층이든 관측* 해야 하므로 상위 모듈(`render` 의 `PassIterator` / `buffer` 의 `RenderTarget` 등)을 **상향 의존**해도 허용 — `render→diagnostics` 와 맞물려 `diagnostics→render` **순환이 생겨도 감수**(CMake 가 STATIC lib 순환을 link-line 반복으로 해소). 예: 골든 캡처 orchestration(`frame_capture`/`pass_capture`)이 diagnostics 에 거주. **이 예외는 diagnostics 에만 — 다른 모듈은 무순환 규율 유지.** (근거: 사이클 규율 출처 = `modular-build-discipline` + 2026-06-11 엔진 의존사이클 리팩토링. diagnostics 는 그 계층화의 *바깥* 관측자.)
- 네임스페이스 `SJH::Diagnostics` — 챕터들은 관용적으로 `namespace diag = SJH::Diagnostics;` 별칭 사용.
- 헤더 노출은 `src/` 기준 상대경로: `#include "diagnostics/gl_log.h"` 형식. 진단 헤더 책임 분담 (2026-07-10 실측 — `src/diagnostics/` 현행 8종):
  - `gl_log.h` (`GLDebug`/`GLObjectLog`) — GL 호출 직후 `glGetError`, 셰이더 컴파일/링크/검증
  - `uniform_diagnostics.h` — 누락 uniform warn-once / 타입 불일치
  - `gl_state_log.h` + `gl_state_fields.h` — 현재 GL 상태 덤프 (`Dump`) + KHR_debug 콜백 (macOS GL 3.3 은 KHR_debug 미지원 → no-op)
  - `gl_validate.h` — `GLValidate` Cat A–F 검증
  - `frame_capture.h` / `pass_capture.h` — 골든 이미지 캡처 orchestration (cycle-exempt 지위의 근거 사례)
  - `effekseer_diagnostics.h` — Effekseer 로드/버전 진단 (2026-06-02 신설)
  - ※ 과거 `engine_diagnostics.h` · `log_util.h` 는 재편으로 현재 부재 — 참조 금지
- 현재 consumer: 활성 데모 (`_MyApp_`) 가 `SJH::engine` 우산을 통해 자동 link. 모듈 직접 link 가 필요하면 자기 `CMakeLists.txt` 의 `target_link_libraries` 에 `SJH::diagnostics` 추가. (과거 chapter7/8/9 가 직접 link 하던 패턴은 챕터 디렉토리 자체가 제거되어 현재 본 브랜치에 없음.)
- spdlog/fmt 미사용 (`diagnostics` 모듈 한정. 게임/엔진 코드는 `game_deps` 의 spdlog 정적 라이브러리를 직접 쓸 수 있다.)
- **주의 — stb_image 정의 책임**: `diagnostics` 라이브러리는 stb_image 를 정의하지 않는다. stb_image 를 쓰는 데모 `main.cpp` 가 정확히 한 곳에서 `#define STB_IMAGE_IMPLEMENTATION` 후 `#include "stb_image.h"` 해야 링크 심볼이 만들어진다. 빠뜨리면 `undefined reference to stbi_load` 류 링크 에러.

### Extern 라이브러리 부트스트랩 (재생성이 필요할 때만)
- `lib/`, `include/` 의 사전 빌드 산출물은 체크인되어 있어 평소엔 재생성 불필요
- 재빌드: `python3 scripts/dev.py extern` (macOS/Linux/Windows 공통 — 플랫폼 자동 감지) — `extern/sb7code` 서브모듈에서 glfw3 + sb7 Release/Debug → `build_extern/output/`
- 생성물은 `lib/{macos,windows}/` 와 `include/` 로 **수동 복사** (스크립트 마지막에 안내 출력, Windows 는 자동 복사)
- macOS 경로는 `sb7.h` 의 `GLFW_INCLUDE_GLCOREARB` → `GLFW_INCLUDE_NONE` 패치를 자동 적용 (gl3w/GLFW 간 gl3.h 중복 포함 방지)
- 동일 작업을 GitHub Actions 의 `build-extern-libs.yml` / `build-msvc.yml` 에서도 수행

### 개발 CLI (`scripts/dev.py`) — 구 `shell/` 통합
sh/bat/ps1 삼중 스크립트를 크로스 플랫폼 Python 단일 진입점으로 통합했다. 플랫폼 감지(macOS/Linux=Ninja, Windows=MSVC)는 한 곳에서 처리한다.

| 서브커맨드 | 인자 | 설명 (구 스크립트) |
|---------|------|------|
| `all` | `<debug\|release> <타겟>` | clean → configure → build → run (구 `CMakeALL`) |
| `configure` | `[debug\|release]` | cmake --preset ninja/ninja-release/msvc-2022 (구 `CMakeConfigureAndGenerate`) |
| `build` | `[debug\|release] [타겟]` | cmake --build, 타겟 지정 시 `--target` (구 `CMakeBuild`) |
| `run` | `<debug\|release> <타겟> [--leaks]` | `cd` 후 실행, `--leaks` 시 `leaks --atExit` (macOS 전용) (구 `CMakeExecute`) |
| `prepare` | (없음) | `build*` 디렉토리 전체 삭제 (구 `CMakePrepare`) |
| `extern` | (없음) | extern 라이브러리 재빌드 → `build_extern/output/` (구 `BuildExternLibs`) |
| `doxygen` | `[포트]` | doxygen 문서 빌드 + 로컬 서빙 + 브라우저 오픈 (구 `Doxygen.sh`) |
| `move-shaders` | (없음) | `apps/*/shaders` → `resources/shaders` 이동 (구 `MoveShaders.sh`) |
| `copy-skills` | (없음) | 전역 skill 10종 → `.claude/skills` 복사 (구 `CopyGlobalSkills.sh`) |
| `resume-claude` | `[--session ID]` | 끊긴 claude 세션 이어서 진행 (구 `ResumeClaude.sh`) |
| `schedule-resume` | `[--at HH:MM]` | 지정 시각에 resume-claude 1회 예약 (구 `ScheduleResumeOnce.sh`) |

## Conventions

- **주석은 한국어**, 사용자와의 소통도 한국어 선호
- 빌드 디렉토리는 프리셋 이름 기반: `build_ninja`, `build_ninja-release`, `build_msvc`
- VSCode IntelliSense / clangd: `build_ninja/compile_commands.json` 사용. GLSL 린팅은 `glslangValidator`
- 네임스페이스: 패턴 A 는 `SJH::ChapterN`, 패턴 B 는 자유
- **헤더 가드 `__CHAPTER_N_ENTRY_H__` 형식 (대소문자 + 언더스코어). `#pragma once` 미사용**
- 식별자 명명(멤버 `m`PascalCase / 지역 camelCase / bool `mIs*` / 포인터 `*Ptr`): 전역 Skill `personal-naming-conventions`
- 포맷팅: `.clang-format` (Microsoft, Tab indent / TabWidth=4, ColumnLimit=0, NamespaceIndentation=All, UnusedIncludes/MissingIncludes=Strict)

### 크로스 플랫폼 코딩 규칙
교수 제출 + macOS/Windows 양쪽 빌드 가정. 새 코드 작성 시 준수: (규칙의 *왜* 는 전역 Skill `modular-build-discipline`)
- `windows.h` 는 `#ifdef _WIN32` 안에서만 include, `WIN32_LEAN_AND_MEAN` + `NOMINMAX` 동반
- `long` 금지 → `int32_t`, `uint64_t` 등 고정 크기 타입 사용
- 파일 경로는 슬래시(`/`) 통일
- 파일 I/O 는 바이너리 모드
- `_WIN32` 매크로는 mingw/MSVC 에서 정의, macOS clang++ 에서는 미정의 (분기 기준)

### 도구 사용 가드레일 — 무거운 리서치/오케스트레이션 하니스 (Workflow / deep-research)

`deep-research`(내장 워크플로, ~96 에이전트·다수 WebFetch·수백만 토큰·계정 spend limit 도달 가능)와 일반 `Workflow` 팬아웃은 **비용이 사용자에게 안 보이므로 남용 금지**.

- **deep-research / 무거운 Workflow 는 *진짜 모르는·논쟁적·다출처 사실 검증*에만.** 학습지식에 있는 표준 용어·단일출처 lookup·코드/내부 질문은 **직접 답** 하거나 표적 WebFetch 1~2회로 끝낼 것.
- **착수 전 비용 고지 + 승인**: 무거운 워크플로를 돌리기 전에 "예상 에이전트 수 · 대략 비용 · 왜 직접 답으로 안 되는지"를 먼저 말하고 사용자 승인을 받는다. (예고 없이 96-에이전트 팬아웃 금지.)
- **보고서/검증성 작업의 WebFetch 는 최소화** — 이미 확신하는 표준 사실은 출처만 인용(확신도 라벨), 새 fetch 는 정말 불확실한 항목만.
- 가능하면 경량 대안(직접 답 / 표적 검색 / 캡 낮춘 로컬 워크플로) 우선.

### 설계 결정 가드레일 (2026-07-10, `doc/번복기록/2026-07-10-쟁점별-비교분석-및-파이프라인개선안.md` 근거)

- **결정 소유권 비이양**: 구조적 설계 결정의 확정 주체는 항상 사용자. AI 는 실측·옵션 비교표·추천까지만 — "진행하세요" 류 단독 확정 지시 금지. 사용자 수용 판정 전에는 spec/As-Built 에 확정형 기록 금지 (`[제안됨]` 상태로만 기록).
- **국면별 경계 오류**: 기능 배치·소유권 결정 국면 = 증분 패치형 오판 경계 → 기존 소유자 grep 선행 (`design-decision-discipline` §6.1) + 모호하면 후보별 소유권 그래프 (graphviz-class-diagram 결정 게이트 모드). 마이그레이션·설계 국면 = "정통 패턴" 논변의 구조물 증식 경계 → 신규 구조물 신고 4항 (같은 스킬 §2.5).
- **정본 서열**: lock 된 결정의 정본 = `doc/superpowers/specs/` 해당 spec + MEMORY 의 lock 표기. 새 제안은 산출 전 정본 D# 정합표 선행 — 상충 항목은 "번복 제안" 으로 명시 분리.
- **추천 라벨**: 모든 설계 추천 발화에 🔵 실측/인용 vs 💭 판단 라벨 + 등급별 행동 바인딩 (`confidence-and-sourcing` §1.5).

## 새 데모/챕터 추가 방법

1. `apps/<demo_name>/` 디렉토리 생성
2. `main.cpp` — `sb7::application` 상속 + `DECLARE_MAIN(...)` (패턴 B 권장)
3. `CMakeLists.txt` — 동일 패턴의 기존 `apps/_MyApp_/CMakeLists.txt` (하위 STATIC + 얇은 entry, 패턴 A 변형) 또는 `apps/migrate_demo/CMakeLists.txt` (패턴 B) 등을 복사 (디렉토리명에서 타겟명 자동 추출)
4. `apps/CMakeLists.txt` 의 `add_subdirectory(<demo_name>)` 줄을 **새로 추가하거나 주석 해제** — 한 번에 소수만 활성화하는 컨벤션
5. (선택) `resources/shaders/` 에 셰이더 배치
6. 코어 모듈 link: 단일 `SJH::engine` (INTERFACE 우산) 또는 필요한 `SJH::<module>` 만 선별 link

### 자원 보유 컨벤션 (필독)

데모 `main.cpp` 의 멤버는 **씬과 시스템만** — 자원 객체 (Texture/Material/Model) 는 `SJH::ResourceRegistry` 에 위탁. Cocos `cc::Director` / Unity `Resources.Load` 정통. 자세한 권장/안티 패턴 + 현재 미지원 자원 (Program/Mesh) 처리는 `.claude/architecture.md §11.3` 필독.

## Reference

### 전역 Skill (범용 가치관·방법론 — `.claude/skills/`, 2026-06-18 추출)
프로젝트 독립 가치관은 `.claude/*.md` 산문이 아니라 **전역 Skill** 로 관리한다 (`~/.claude/skills/` 가 SSOT, 프로젝트 사본은 `python3 scripts/dev.py copy-skills` 단방향 복사). 문서에 중복 서술하지 말 것.
- **design-decision-discipline** — 변동성≠다형성 / 다형성 3조건 / composition>inheritance / 소유권·에러 철학 (← architecture.md §3 의 *왜*)
- **modular-build-discipline** — 명시적 의존 PUBLIC/PRIVATE / self-contained 모듈 / 크로스플랫폼 가드 (← architecture.md §4)
- **code-design-review-lenses** — 5렌즈 설계 평가 + 객관/주관 분리
- **architecture-design-workflow** — 4-Phase + Decision Log + 옵션표+추천 + spec/plan 템플릿 (← architecture-design-agent.md 범용분)
- **benchmark-research-method** — 모범 구현 N종 must-have/common/optional 분류
- **agent-orchestration-anti-gaming** — 역할 권한 분리 + 안티게이밍 (← Graphics-Testing-Prompt §C/§D 원칙)
- **response-quality-calibration** + **confidence-and-sourcing** — 응답 품질 / 확신도·근거 (← 구 Anti-Hallucination, 삭제됨)
- **personal-naming-conventions** — 식별자 명명 규칙
- **socratic-tutor** — 학습 튜터링 (← 구 충돌검출 커리큘럼, 삭제됨)

### 프로젝트 문서
- `.claude/architecture.md` — `SJH::<module>` STATIC 라이브러리 패턴, 명시적 의존성 선언 규칙, include 형식 구분(`<vendor/>` vs `"module/"`). 새 모듈 추가 시 필독. (§3·§4 범용 원칙은 위 Skill)
- 응답 품질 13-체크포인트 (사전 검증/확신도/출처) → 전역 Skill `response-quality-calibration` + `confidence-and-sourcing` (`.claude/skills/`) 로 이관 (2026-06-18).
- `doc/testplan/Graphics-Testing-Prompt.md` — 그래픽스 도메인 리팩토링 가드레일 (결정성, 골든 이미지 임계값, 5-에이전트 권한 매트릭스).
- `.claude/agents/render-*.md` + `.claude/commands/refactor-pass.md` — 5-에이전트 리팩토링 파이프라인 (analysis → refactor → test-debug → quality-gate → pm). `doc/testplan/AGENTS_GUIDE.md` 가 설계 근거 (D 논문 5-Agent 구조 + A 논문 anti-gaming).
- `doc/testplan/` — 테스트 설계 문서 (`testing-curriculum.md`, `AGENTS_GUIDE.md`, `PHASE1_RUNBOOK.md`, `test-quality-drill/`). (※ 구 `test/` 의 Catch2 v3 21개 단위 테스트는 2026-06-20 엔진 API 진화로 OutDated 판정 → 전량 폐기, 재작성 대기. 위 *테스트 실행* 섹션 참조.)
- `doc/handoffs/2026-05-10-window11-env-handoff.md` — UTM 위 Windows 11 ARM64 + MSVC 빌드 환경 세팅 인수인계 (SMB 공유, virtio-net-pci, VS Build Tools 2022 설치, ARM64 호스트 x64 크로스 빌드 등). MSVC 쪽 빌드 환경에서 막힐 때 먼저 참고.
- `doc/testplan/STUDY_NOTE.md`, `doc/버그리포트.md` — 학습 노트 / 디버깅 기록 (한국어). (※ 구 `Material_Texture.md`·`BugReport.md`·`멀티플라이팅.md` 는 개명·이동으로 부재 — 2026-07-10 실측.)
- `doc/api/EngineAPI.md` — **SJH 엔진 코어 API 레퍼런스 (정본)**. `src/<module>/` 12개 모듈의 공개 시그니처 + 8개 사용 컨벤션 (Compound Actor / Component Builder / Material 셋업 / Camera Free·TargetLock / Uniform 송신 두 layer / Lighting 셰이더 schema / Pass 컨벤션 / Retina HiDPI). *대상 독자 = 본 프로젝트에 참여하는 다른 AI 에이전트 + 개발자*. ※ 본 문서의 모듈 목차는 12개 시점 — `SJH::sprite` / `SJH::fsm` / `SJH::playable` (M1~M3.5) / `SJH::timer` (2026-06-01) / `SJH::text` (2026-06-02) 신설분 미반영 (현재 17 모듈). 신규 모듈 사용 시 [`src/<module>/`](src/) 헤더 직접 참조.
- `doc/api/Box2DAPI.md` — Box2D v2.4.1 의 *실사용* API 레퍼런스. `apps/box2d_demo/` 3 데모 (Tumbler / Car / Bridge) + `box2d_common/debug_draw` 에서 호출된 b2World / b2Body / Joint (Revolute·Wheel) / b2Draw 만 정리. 게임 물리 코드 (`apps/_MyApp_/src/Physics/`) 작성 시 1차 참조.
- `doc/api/FMODAPI.md` — FMOD Studio API 학습 노트. `apps/audio_demo/{demo1,demo2}` 에서 검증된 호출 발췌 (System / Bank / Event / 파라미터 / FMOD_RESULT 에러 처리). Studio 만 사용 (Core 는 Studio 가 내부에서 호출). 설치/CMake 등록은 [`doc/api/FMOD_Setup.md`](../doc/api/FMOD_Setup.md) 와 짝.
- `doc/api/EffekseerAPI.md` — Effekseer 1.7.3.0 *실사용* API 레퍼런스 (Context7 `/effekseer/effekseer` 교차검증). `apps/effekseer_demo/demo1` (저수준 1파일) + `_MyApp_` VFX (**VFXSystem owner / EffekseerPlayable leaf / SJH::Effect registry wrap 3분할**) 에서 검증된 호출 정리 — Manager/Renderer 부트(5 SubRenderer+4 Loader) / `Effect::Create(u"…efkefc")` char16_t / `Play`→`Handle` lifecycle(`StopEffect`/`Exists`/`SetLocation`) / `Update(dt*60)` deltaFrame / `Matrix44` RH. **함정 6종** (`.efkefc` export 버전 mismatch 시 조용한 nullptr · 텍스처 base경로 · VAO EBO 오염 · deltaFrame 단위 · Handle=정수ID RAII 없음). `apps/_MyApp_/src/VFX/` 코드 작성 시 1차 참조.
- `doc/superpowers/specs/` — 챕터별 설계 스펙 (날짜-주제 형식, 예: `2026-05-12-chapter7-engine-diagnostics-design.md`, `2026-05-12-chapter9-single-shader-quad-design.md`). 새 챕터 착수 전 동일 패턴으로 설계 노트 추가 권장.
  - **현재 진행 중 (탑다운 슈터 — `_MyApp_`)**:
    - `2026-05-24-topdown-shooter-design.md` — 정본 spec (1782줄, 19 확정 결정). §1.5 leaf Playable 거주 위치 + §1.6 SpriteSequencePlayable. M3.5 본격 도입 시 **2026-05-26 spec 으로 진화** (아래 참조).
    - `2026-05-25-fsm-object-state-machine-design.md` — `SJH::fsm` Stage 4 정본 (TTransit 폐기, `IFsmState<TOwner>` 그래프 응집).
    - `2026-05-25-M3-physics-box2d.md` — M3 Box2D plan + 결정 #18 진화 회고.
    - **`2026-05-26-playable-component-interface-design.md` — IPlayable 정본 spec (M3.5, 7 결정)**. IPlayable pure interface + PlayableBase abstract (Component 다중 상속) + Composite (`vector<unique_ptr<IPlayable>>` + Tweeny/DOTween 정통 fluent Builder Append/Insert/Join + 계층 무제한 중첩). spec §1.5 `PlayablePlayerComponent` + `PlayableTickSystem` 폐기 (Component 시스템이 동일 역할).
- `doc/topdown-shooter-progress.md` — `_MyApp_` 마일스톤 진행 보고서 (M1~M7). 다음 작업 진입점 — M3 완료(2026-05-25) + M3.5 코어 정착(2026-05-26), **다음 = M5 Effekseer/FMOD leaf Playable 화** (사용자 결정 2026-05-26). M4 (PlayerStateMachine + 발사 + 적) 는 leaf Playable 도착 후 본격.
- `extern/sb7code/` — SuperBible 7 원본 (Git 서브모듈, glfw3 + sb7 소스)
- `extern/Effekseer` (1.7.3.0) — 게임/엔진 라이브러리 서브모듈(잔류). 버전 추적·재빌드 소스용이며 빌드는 `lib/`·`include/` 체크인 산출물에 의존. 재빌드는 `python3 scripts/dev.py extern`. (※ `extern/box2d`·`extern/assimp`·`extern/spdlog`·`extern/tweeny`·`extern/stb` 는 2026-06-20 vcpkg 전이로 **제거됨** — vcpkg manifest 가 버전 핀 + 소스 빌드 담당. 설계 회고: `doc/superpowers/specs/2026-05-19-game-engine-deps-registration-design.md`)
- `extern/imgui` (v1.53) / `extern/Catch2` (v3.15.0) — 다른 라이브러리와 달리 **사전 빌드 lib 없음**. 과거 `src/imgui/` 가 v1.53 의 코어 3 + 결합 backend 1 파일을 인라인 컴파일하여 `SJH::imgui` 정적 라이브러리를 생성했으나 **현재 본 브랜치에서 `src/imgui/` 모듈은 폐기**됨 (`src/CMakeLists.txt` 의 imgui add_subdirectory 가 주석 처리). 현재 ImGui 는 `_MyApp_` 가 client-side 로 직접 컴파일하는 패턴 — `extern/imgui` v1.53 코어 소스 + `opengl3_example` backend 를 executable 타겟에 직접 add (`apps/_MyApp_/CMakeLists.txt` 의 `IMGUI_SRC`). `audio_demo`/`migrate_demo` 도 동일 패턴이나 현재 둘 다 비활성. ImGui 는 GLFW(3.0.4) 호환 한계로 **v1.53 핀 필수** (v1.54+ 는 GLFW 3.1+ cursor API unconditional 사용 — vcpkg imgui 1.92.8 전이 불가, glfw-binding 이 vcpkg glfw3 끌어와 sb7 prebuilt 3.0.4 와 충돌). `extern/imgui` 는 잔류 서브모듈. **`extern/Catch2` 는 2026-06-20 제거됨 — Catch2 는 vcpkg `find_package(Catch2 3)` 로 전이** (단 기존 test/ 21개는 OutDated 전량 폐기, 재작성 대기).
- `doc/api/FMOD_Setup.md` — FMOD Core API 수동 설치 가이드 (독점 SDK 라 서브모듈 불가) + Dependency.cmake 등록 명세 (game_deps 자동 합류, POST_BUILD copy 의무)
