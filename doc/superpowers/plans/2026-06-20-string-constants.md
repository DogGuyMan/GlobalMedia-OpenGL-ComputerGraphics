# 하드코딩 문자열 -> 모듈 Constants.h 상수화 Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** main.cpp 의 하드코딩 문자열 리터럴(actor명/PostFX 패스명/UI 텍스처/uniform)을 기존 `*/Constants.h` 컨벤션의 named constant 로 추출 — 단일 출처 + cross-file typo 차단.

**Architecture:** 도메인 소속 문자열은 도메인 모듈 Constants.h(Stage/Audio/Playable)에, main 전용 오케스트레이션 문자열은 신규 app-root `apps/_MyApp_/src/Constants.h`(엔진 src/common/constants.h 식 멀티섹션)에. 전부 `constexpr const char* UPPER_SNAKE`. 소비처 6파일이 리터럴을 상수로 치환.

**Tech Stack:** C++17, CMake + Ninja(vcpkg). 정본 spec = [`doc/superpowers/specs/2026-06-20-string-constants-extraction-design.md`](2026-06-20-string-constants-extraction-design.md) (scope C, S-1~S-4).

---

## 검증 철학

`no_auto_tests` + 그래픽스 -> 검증 = **빌드 GREEN**(에이전트) + **GUI 육안**(사용자). 순수 *리터럴 -> 동일 값 상수* 라 spec §6 상 **거동 무변경** — GUI 는 sanity.

## 가드레일

- **커밋 = 사용자 게이트** + path-scoped + `git add -A` 금지 + `Co-Authored-By` 미사용. ⚠ `main.cpp` 는 EInitTask 미커밋 + 사용자 `Playable::` drift 와 intertwined — granularity 사용자 관리.
- **컨벤션**: 주석 한국어 ASCII+한글, Tab indent, 가드 `_TOPDOWNSHOOTER_<MODULE>_CONSTANTS__`, `#pragma once` 금지. `constexpr const char*` UPPER_SNAKE (기존 apps/_MyApp_/src/Audio/Constants.h 식).
- **불변 보존**: 상수 *값* 은 기존 리터럴과 1바이트도 다르면 안 됨(거동 무변경 보장). 특히 경로 `"resources/texture/Title.png"`(`./` 없음), uniform `"u_time"` 등 원본 그대로.

## Pre-flight

- [ ] `cmake --build --preset ninja --target _MyApp_` baseline GREEN 확인.
- [ ] `git status --short` 재측정 (main.cpp 미커밋 동거 확인).

---

## File Structure

- **신규** `apps/_MyApp_/src/Constants.h` — main 전용 문자열(actor 2 + UI 텍스처 6 + uniform 3). `namespace TopdownShooter`.
- **수정** `apps/_MyApp_/src/Stage/Constants.h` — actor명 2 추가.
- **수정** `apps/_MyApp_/src/Audio/Constants.h` — actor명 1 추가.
- **수정** `apps/_MyApp_/src/Playable/Constants.h` — PostFX 패스명 8 추가.
- **수정(소비처)** `main.cpp` / `apps/_MyApp_/src/Stage/State/StageState.Impl.h` / `apps/_MyApp_/src/Spawns/AmbientSequences.cpp` / `<Playable>/PostFXConstants.h` / `<UI>/PostFXDebugLayer.cpp` / `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp`.
- **CMake**: 변경 0.

---

## Task 1: 상수 정의 (4개 Constants.h)

순수 추가 — 아직 아무도 참조 안 하므로 빌드 거동 불변(GREEN).

**Files:**
- Create: `apps/_MyApp_/src/Constants.h`
- Modify: `apps/_MyApp_/src/Stage/Constants.h`, `apps/_MyApp_/src/Audio/Constants.h`, `apps/_MyApp_/src/Playable/Constants.h`

- [ ] **Step 1: app-root `apps/_MyApp_/src/Constants.h` 생성** — 정확히:

```cpp
/**
 * @file Constants.h
 * @brief 앱 루트(main) 전용 문자열 상수 - scene actor 이름 / UI 오버레이 텍스처 / 클라 머티리얼 uniform.
 *
 * @details
 *  main.cpp 가 오케스트레이션으로 쓰는(다른 모듈이 참조하지 않는) 문자열을 단일 출처로 모은다.
 *  도메인 소속 문자열(Stage/Audio/Playable actor·패스명)은 각 모듈 Constants.h 에 둔다.
 *  엔진 @c src/common/constants.h 의 멀티섹션 grab-bag 컨벤션과 동일.
 * @note 로그 포맷 문자열은 제외(사용처 local 이라 상수화 이득 없음 - 기존 컨벤션).
 */
#ifndef _TOPDOWNSHOOTER_CONSTANTS__
#define _TOPDOWNSHOOTER_CONSTANTS__

namespace TopdownShooter
{
	// -- Scene actor 이름 (main 생성 - 포인터로 전달, 문자열 조회 없음) --
	constexpr const char *ACTOR_SCREEN_CAMERA = "ScreenCamera"; ///< CreateScreenCameraActor 이름.
	constexpr const char *ACTOR_FX_ROOT       = "FxRoot";       ///< 단발 시퀀스 부모 (SetSpawnContext 포인터 전달).

	// -- UI 오버레이 텍스처 (ResourceRegistry 키 + Image::Load 경로) --
	constexpr const char *STR_UI_TITLE     = "ui_title";    ///< Title 오버레이 텍스처 키.
	constexpr const char *STR_UI_PAUSE     = "ui_pause";    ///< Pause 오버레이 텍스처 키.
	constexpr const char *STR_UI_GAMEOVER  = "ui_gameover"; ///< GameOver 오버레이 텍스처 키.
	constexpr const char *PATH_UI_TITLE    = "resources/texture/Title.png";
	constexpr const char *PATH_UI_PAUSE    = "resources/texture/Pause.png";
	constexpr const char *PATH_UI_GAMEOVER = "resources/texture/GameOver.png";

	// -- 클라 머티리얼 uniform 이름 (Material::Properties 키) --
	constexpr const char *UNI_SKYBOX_TIME  = "u_time";             ///< 스카이박스 시간 (Floats).
	constexpr const char *UNI_FOG_INV_PROJ = "uInverseProjection"; ///< fog projection 역행렬 (Mat4s).
	constexpr const char *UNI_FOG_DEPTH    = "uDepth";             ///< fog depth 텍스처 (Textures).
}

#endif // _TOPDOWNSHOOTER_CONSTANTS__
```

- [ ] **Step 2: `apps/_MyApp_/src/Stage/Constants.h` 에 actor명 추가** — 닫는 `}` 직전(`WALL_THICKNESS` 줄 다음)에:

```cpp
	// -- Scene actor 이름 (생성 <-> FindChild 단일 출처) --
	constexpr const char *ACTOR_GAME_CONTEXT = "GameContext"; ///< main 생성 <-> StageState GetCtx FindChild.
	constexpr const char *ACTOR_WAVE_SPAWNER = "WaveSpawner"; ///< main 생성 + FindChild (웨이브 와이어링).
```
(기존 `namespace TopdownShooter::Stage { ... }` 안, `WALL_THICKNESS` 정의 아래 줄에 삽입.)

- [ ] **Step 3: `apps/_MyApp_/src/Audio/Constants.h` 에 actor명 추가** — `BUS_SFX` 줄 다음에:

```cpp

	// -- Scene actor 이름 --
	constexpr const char *ACTOR_BGM = "BgmActor"; ///< AmbientSequences 생성 <-> main FindChild.
```
(기존 `namespace TopdownShooter::Audio { ... }` 안, `BUS_SFX` 정의 아래.)

- [ ] **Step 4: `apps/_MyApp_/src/Playable/Constants.h` 에 PostFX 패스명 추가** — 닫는 `}; // namespace TopdownShooter::Playable` 직전(`VIGNETTE_DURATION_MS` 줄 다음)에:

```cpp

	// -- PostFX 패스명 (POSTFX_PROGRAM_CONFIGS Name + 토글/연출/UI 단일 출처) --
	constexpr const char *PASS_GAMMA                = "gamma";
	constexpr const char *PASS_SHARPENING           = "sharpening";
	constexpr const char *PASS_BLOOM                = "bloom";
	constexpr const char *PASS_FOG                  = "fog";
	constexpr const char *PASS_GRAYSCALE_VIGNETTING = "grayscale_vignetting";
	constexpr const char *PASS_INVERT               = "invert";
	constexpr const char *PASS_BLURRING             = "blurring";
	constexpr const char *PASS_SOBEL                = "sobel";
```

- [ ] **Step 5: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: **에러 0** (순수 헤더 추가, 미참조 - 거동 불변). (app-root Constants.h 는 아직 어떤 .cpp 도 include 안 해 미컴파일 - Task 2 에서 검증.)

- [ ] **Step 6: 커밋 (사용자 게이트 — 제안만)**

```bash
git add apps/_MyApp_/src/Constants.h apps/_MyApp_/src/Stage/Constants.h apps/_MyApp_/src/Audio/Constants.h apps/_MyApp_/src/Playable/Constants.h
git commit -m "[refactor] : 하드코딩 문자열 상수 정의 (actor/PostFX/UI/uniform)"
```

---

## Task 2: 소비처 6파일 리터럴 -> 상수 치환

상수 참조 시작 -> 빌드는 마지막에 GREEN 확인.

**Files:** main.cpp / StageState.Impl.h / AmbientSequences.cpp / PostFXConstants.h / PostFXDebugLayer.cpp / PlayerBuilder.cpp

- [ ] **Step 1: `<Playable>/PostFXConstants.h` — configs Name -> PASS_* + include**

(a) 파일 상단 include 블록(`#include "<render_bootstrap>/render_pipeline.h"` 근처)에 추가:
```cpp
#include "apps/_MyApp_/src/Playable/Constants.h"   // PASS_* 패스명
```
(b) `POSTFX_PROGRAM_CONFIGS` 의 각 Name 리터럴(8개)을 `PASS_*` 로 (같은 `TopdownShooter::Playable` 네임스페이스라 직접 참조). 각 줄 첫 필드:
```
{"gamma", ...}                -> {PASS_GAMMA, ...}
{"sharpening", ...}           -> {PASS_SHARPENING, ...}
{"bloom", ...}                -> {PASS_BLOOM, ...}
{"fog", ...}                  -> {PASS_FOG, ...}
{"grayscale_vignetting", ...} -> {PASS_GRAYSCALE_VIGNETTING, ...}
{"invert", ...}               -> {PASS_INVERT, ...}
{"blurring", ...}             -> {PASS_BLURRING, ...}
{"sobel", ...}                -> {PASS_SOBEL, ...}
```
(VS/FS 경로/InitFloats 무변경. `PASSTHOURH_PROGRAM_CONFIG` 의 `"screen_passthrough"` 는 PostFX 패스 아님 -> 무변경.)

- [ ] **Step 2: `main.cpp` — include 2개 추가**

기존 include 블록(`#include "GameSystems.h"` 근처)에 추가:
```cpp
#include "Constants.h"          // app-root: ACTOR_SCREEN_CAMERA / STR_UI_* / UNI_* 등
#include "apps/_MyApp_/src/Audio/Constants.h"    // Audio::ACTOR_BGM
```
(`apps/_MyApp_/src/Stage/Constants.h` 는 이미 include[line ~37]. `Playable::PASS_*` 는 PostFXConstants.h 가 apps/_MyApp_/src/Playable/Constants.h 를 include 하므로 transitive 해소.)

- [ ] **Step 3: `main.cpp` — 리터럴 치환 (interior substring 매칭 권장)**

```
"ScreenCamera"                              -> ACTOR_SCREEN_CAMERA
"FxRoot"                                    -> ACTOR_FX_ROOT
std::make_unique<SJH::Scene::Actor>("WaveSpawner")  -> ...Actor>(Stage::ACTOR_WAVE_SPAWNER)
dir.Root().FindChild("WaveSpawner")         -> dir.Root().FindChild(Stage::ACTOR_WAVE_SPAWNER)
std::make_unique<SJH::Scene::Actor>("GameContext")  -> ...Actor>(Stage::ACTOR_GAME_CONTEXT)
dir.Root().FindChild("BgmActor")            -> dir.Root().FindChild(Audio::ACTOR_BGM)
name == "invert"                            -> name == Playable::PASS_INVERT
name == "blurring"                          -> name == Playable::PASS_BLURRING
name == "sobel"                             -> name == Playable::PASS_SOBEL
FindPassMaterial("grayscale_vignetting")    -> FindPassMaterial(Playable::PASS_GRAYSCALE_VIGNETTING)
POSTFX_PROGRAM_CONFIGS[i].Name == "blurring" -> ... == Playable::PASS_BLURRING
FindPassMaterial("fog")                     -> FindPassMaterial(Playable::PASS_FOG)
```
UI 텍스처 3줄 (각 줄 키 2회 + 경로 1회):
```cpp
mCtx->titleTex    = reg.CreateTexture(STR_UI_TITLE,    SJH::Image::Load(STR_UI_TITLE,    PATH_UI_TITLE).get());
mCtx->pauseTex    = reg.CreateTexture(STR_UI_PAUSE,    SJH::Image::Load(STR_UI_PAUSE,    PATH_UI_PAUSE).get());
mCtx->gameOverTex = reg.CreateTexture(STR_UI_GAMEOVER, SJH::Image::Load(STR_UI_GAMEOVER, PATH_UI_GAMEOVER).get());
```
uniform 3곳:
```
mSkyboxMat->Properties.Floats["u_time"]          -> Properties.Floats[UNI_SKYBOX_TIME]
fogMat->Properties.Mat4s["uInverseProjection"]   -> Properties.Mat4s[UNI_FOG_INV_PROJ]
fogMat->Properties.Textures["uDepth"]            -> Properties.Textures[UNI_FOG_DEPTH]
```
(`ACTOR_*`/`STR_*`/`PATH_*`/`UNI_*` 는 app-root `TopdownShooter::` 라 main.cpp 에서 직접 참조. `Stage::`/`Audio::`/`Playable::` 는 한정.)

> ⚠ main.cpp 의 `name == "invert"||...` 줄: `const auto &name = Playable::POSTFX_PROGRAM_CONFIGS[i].Name`(std::string) 이라 `name == Playable::PASS_INVERT`(std::string==const char*) 정상.

- [ ] **Step 4: `apps/_MyApp_/src/Stage/State/StageState.Impl.h` — GameContext FindChild**

(a) include 추가 (기존 `#include "apps/_MyApp_/src/Stage/Stage.h"` 근처):
```cpp
#include "apps/_MyApp_/src/Stage/Constants.h"   // ACTOR_GAME_CONTEXT
```
(b) `GetCtx` 의 `root.FindChild("GameContext")` -> `root.FindChild(ACTOR_GAME_CONTEXT)` (이미 `namespace TopdownShooter::Stage` 라 직접 참조). 위 줄 doc 주석의 `"GameContext"` 는 예시 -> 무변경.

- [ ] **Step 5: `apps/_MyApp_/src/Spawns/AmbientSequences.cpp` — BgmActor 생성**

(a) include 추가:
```cpp
#include "apps/_MyApp_/src/Audio/Constants.h"   // ACTOR_BGM
```
(b) `AddChild(std::make_unique<SJH::Scene::Actor>("BgmActor"))` -> `...Actor>(Audio::ACTOR_BGM)`. doc 주석의 "BgmActor" 무변경.

- [ ] **Step 6: `<UI>/PostFXDebugLayer.cpp` — 패스명 비교**

(a) include 추가:
```cpp
#include "apps/_MyApp_/src/Playable/Constants.h"   // PASS_*
```
(b) 패스명 비교 4곳 (uniform key `props.Floats["gamma"]` 은 패스명 아님 -> 무변경):
```
entry.Name == "gamma"                -> entry.Name == Playable::PASS_GAMMA
entry.Name == "fog"                  -> entry.Name == Playable::PASS_FOG
entry.Name == "bloom"                -> entry.Name == Playable::PASS_BLOOM
entry.Name == "grayscale_vignetting" -> entry.Name == Playable::PASS_GRAYSCALE_VIGNETTING
```

- [ ] **Step 7: `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` — grayscale_vignetting 생성 2곳**

(a) include 추가:
```cpp
#include "apps/_MyApp_/src/Playable/Constants.h"   // PASS_GRAYSCALE_VIGNETTING
```
(b) 2곳 치환:
```
PostFXTweenPlayable>("grayscale_vignetting", "uVignetteAmount", ...)
    -> PostFXTweenPlayable>(Playable::PASS_GRAYSCALE_VIGNETTING, "uVignetteAmount", ...)
HpGrayscalePostFX>("grayscale_vignetting", "uGrayscaleAmount")
    -> HpGrayscalePostFX>(Playable::PASS_GRAYSCALE_VIGNETTING, "uGrayscaleAmount")
```
(uniform `"uVignetteAmount"`/`"uGrayscaleAmount"` 은 패스명 아님 -> 무변경. scope C 라도 PlayerBuilder-local uniform 이라 본 작업 대상 외.)

- [ ] **Step 8: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: **에러 0**. 흔한 실패: (a) 네임스페이스 한정 누락(`PASS_FOG` -> `Playable::PASS_FOG`), (b) include 누락 -> 미정의 식별자, (c) 상수 값 오타(원본과 불일치는 빌드는 통과하나 거동 변경 - GUI 에서 발견).
검증: `grep -n '"GameContext"\|"BgmActor"\|"WaveSpawner"\|"ScreenCamera"\|"FxRoot"\|"ui_title"\|"u_time"\|"grayscale_vignetting"' apps/_MyApp_/main.cpp` 의 *기능 코드* 결과 0 (주석 제외).

- [ ] **Step 9: GUI 육안 검증 (사용자)**

`./_MyApp_` — 문자열 값 동일이라 전체 거동(orbital 배경 / PostFX 토글 F1 / HP 무채색·비네팅 / BGM / Title-Pause-GameOver 오버레이) 이전과 동일해야 정상.

- [ ] **Step 10: 커밋 (사용자 게이트 — 제안만)**

```bash
git add apps/_MyApp_/main.cpp apps/_MyApp_/src/Stage/State/StageState.Impl.h apps/_MyApp_/src/Spawns/AmbientSequences.cpp <apps>/_MyApp_/src/Playable/PostFXConstants.h <apps>/_MyApp_/src/UI/PostFXDebugLayer.cpp apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp
git commit -m "[refactor] : 하드코딩 문자열을 모듈 Constants.h 상수로 치환"
```
⚠ `main.cpp` 는 EInitTask/drift 미커밋 동거 — 사용자 granularity 관리.

---

## Self-Review (spec 대비)

**1. Spec coverage:**
- S-1 scope C (main 전체 리터럴) -> Task 2 Step 3 (actor/UI/uniform/PostFX 전부) ✅
- S-2 기존 Constants.h 컨벤션 -> Task 1 (constexpr const char* UPPER_SNAKE, 가드, Doxygen) ✅
- S-3 도메인 모듈 + app-root 분리 -> Task 1 (Stage/Audio/Playable + app-root Constants.h) ✅
- S-4 inline constexpr const char* -> Task 1 ✅
- §6 거동 무변경 -> Step 9 GUI + "상수 값 = 원본 리터럴" 가드레일 ✅
- 로그/doc-comment 제외 -> Step 3/4/5 주석 무변경 명시 ✅

**2. Placeholder scan:** 전체 코드/치환 제공, "TBD" 없음. ✅

**3. Type consistency:** `constexpr const char*` 일관, `ACTOR_*`(app-root/Stage/Audio) / `PASS_*`(Playable) / `STR_UI_*`/`PATH_UI_*`/`UNI_*`(app-root) 명명 Task 1·2 일치. include 경로(`"Constants.h"`/`"apps/_MyApp_/src/Stage/Constants.h"`/`"apps/_MyApp_/src/Audio/Constants.h"`/`"apps/_MyApp_/src/Playable/Constants.h"`) 일관. ✅
