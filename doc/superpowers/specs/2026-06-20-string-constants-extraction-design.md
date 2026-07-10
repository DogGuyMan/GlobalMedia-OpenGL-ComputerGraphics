# 하드코딩 문자열 -> 모듈 Constants.h 상수화 설계 (2026-06-20)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **상태**: 설계 승인 완료(brainstorm, scope C). 다음 = writing-plans.
> **맥락**: init-scheduler 리팩토링(커밋 `1000727`) + orbital 회귀가 *cross-file 문자열 typo = silent 버그* 위험 부각. main.cpp 등의 하드코딩 문자열을 **기존 `*/Constants.h` 모듈별 컨벤션**으로 상수화.
> **브랜치/HEAD**: `game/main` / `1000727` 위 (EInitTask enum 미커밋 동거).
> **정정**: 초안의 신규 `ActorNames.h` + `PassName` 네임스페이스 폐기 -> 프로젝트 기존 `Constants.h` 컨벤션 준수(사용자 지시 2026-06-20).

## 1. 배경 / 문제

main.cpp 및 관련 파일에 하드코딩된 문자열 식별자가 흩어져 있다. 특히 *서로 다른 파일이 손으로 같은 문자열을 맞춰야* 하는 경우 typo 가 컴파일을 통과하고 런타임에 조용히 깨진다(orbital 류 = `FindEffect`/`FindChild` nullptr no-op). 프로젝트는 이미 `*/Constants.h` 모듈별 상수 컨벤션(Audio/Bootstrap/Entity/HUD/Physics/Playable/Stage/VFX 8종 + 엔진 `src/common/constants.h`)을 갖추고 있어, 이를 단일 출처로 활용한다.

## 2. 목표 / 비목표

**목표**
- main.cpp 의 하드코딩 문자열 리터럴을 **named constant** 로 (scope C = 전부).
- **기존 `*/Constants.h` 컨벤션 준수**: `namespace TopdownShooter::<Module>` + `constexpr const char* UPPER_SNAKE` + Doxygen `@file`/`@brief` + 가드 `_TOPDOWNSHOOTER_<MODULE>_CONSTANTS__`.
- 도메인 소속 문자열은 **도메인 모듈 Constants.h** 에, main-only 오케스트레이션 문자열은 **신규 app-root `apps/_MyApp_/src/Constants.h`** 에 (엔진 `src/common/constants.h` 식 멀티섹션 grab-bag).

**비목표**
- **로그 포맷 문자열**(`"[vfx-test] load failed: {}"` 등) — 기존 컨벤션이 명시 제외("로그 포맷은 local 이라 상수화 이득 없음", src/common/constants.h §비-책임). 유지.
- **doc-comment 내 예시 문자열** — 기능 코드 아님, 범위 밖.
- **기존 `POSTFX_PROGRAM_CONFIGS` 자체 이동**(PostFXConstants.h -> apps/_MyApp_/src/Playable/Constants.h) — 범위 밖. configs 는 자리 유지하고 PASS_* 만 참조.
- **CMake / 엔진 src/ 변경** — 전부 헤더온리 클라 Constants.h.

## 3. 확정 결정 (brainstorm)

| ID | 결정 | 근거 |
|----|------|------|
| **S-1** | scope = **C** (main.cpp 전체 리터럴, one-off 포함) | 사용자 "싸그리 다". src/common/constants.h 가 uniform/path/label 까지 중앙화한 선례 |
| **S-2** | **기존 `*/Constants.h` 컨벤션** (신규 `ActorNames.h`/`PassName` 폐기) | 사용자 지시. 프로젝트 8 모듈 Constants.h 패턴 일관 |
| **S-3** | **배치 = 도메인 모듈 + app-root 분리** — 도메인 actor/pass 는 모듈 Constants.h, main-only 는 신규 app-root `apps/_MyApp_/src/Constants.h` | 교차참조는 도메인 모듈(소비 모듈이 자기 Constants.h include), 오케스트레이션은 app-root grab-bag |
| **S-4** | `constexpr const char *` UPPER_SNAKE (기존 apps/_MyApp_/src/Audio/Constants.h 식) | std::string 암묵변환(Actor ctor/FindChild/`==`/CreateTexture key). 헤더온리 다중 TU 안전 |

## 4. 아키텍처 — 배치

### 4.1 신규 `apps/_MyApp_/src/Constants.h` (namespace `TopdownShooter`)

main.cpp 전용(다른 모듈 미참조) 문자열. 멀티섹션 grab-bag (src/common/constants.h 정통):

```cpp
#ifndef _TOPDOWNSHOOTER_CONSTANTS__
#define _TOPDOWNSHOOTER_CONSTANTS__

namespace TopdownShooter
{
	// -- Scene actor 이름 (main 오케스트레이션 - 생성만, 포인터 전달) --
	constexpr const char *ACTOR_SCREEN_CAMERA = "ScreenCamera"; ///< CreateScreenCameraActor.
	constexpr const char *ACTOR_FX_ROOT       = "FxRoot";       ///< 단발 시퀀스 부모 (SetSpawnContext 포인터 전달).

	// -- UI 오버레이 텍스처 (ResourceRegistry 키 + Image::Load 경로) --
	constexpr const char *STR_UI_TITLE     = "ui_title";
	constexpr const char *STR_UI_PAUSE     = "ui_pause";
	constexpr const char *STR_UI_GAMEOVER  = "ui_gameover";
	constexpr const char *PATH_UI_TITLE    = "resources/texture/Title.png";
	constexpr const char *PATH_UI_PAUSE    = "resources/texture/Pause.png";
	constexpr const char *PATH_UI_GAMEOVER = "resources/texture/GameOver.png";

	// -- 클라 머티리얼 uniform 이름 (Material::Properties 키) --
	constexpr const char *UNI_SKYBOX_TIME  = "u_time";            ///< 스카이박스 시간.
	constexpr const char *UNI_FOG_INV_PROJ = "uInverseProjection"; ///< fog projection 역행렬.
	constexpr const char *UNI_FOG_DEPTH    = "uDepth";            ///< fog depth 텍스처.
}

#endif // _TOPDOWNSHOOTER_CONSTANTS__
```

### 4.2 기존 모듈 Constants.h 추가 (도메인 + 교차참조)

| 파일 | 추가 상수 | 교차참조 |
|------|-----------|----------|
| `apps/_MyApp_/src/Stage/Constants.h` (`TopdownShooter::Stage`) | `ACTOR_GAME_CONTEXT = "GameContext"`<br>`ACTOR_WAVE_SPAWNER = "WaveSpawner"` | main 생성 <-> StageState.Impl.h GetCtx FindChild (GameContext); main 생성+FindChild (WaveSpawner) |
| `apps/_MyApp_/src/Audio/Constants.h` (`TopdownShooter::Audio`) | `ACTOR_BGM = "BgmActor"` | AmbientSequences.cpp 생성 <-> main FindChild |
| `apps/_MyApp_/src/Playable/Constants.h` (`TopdownShooter::Playable`) | `PASS_GAMMA="gamma"` `PASS_SHARPENING="sharpening"` `PASS_BLOOM="bloom"` `PASS_FOG="fog"` `PASS_GRAYSCALE_VIGNETTING="grayscale_vignetting"` `PASS_INVERT="invert"` `PASS_BLURRING="blurring"` `PASS_SOBEL="sobel"` | PostFXConstants.h configs Name + main + PostFXDebugLayer + PlayerBuilder |

> 추가 위치는 각 Constants.h 의 기존 상수 뒤 새 섹션(`// -- Scene actor / PostFX 패스명 --`). 기존 내용 무변경.

## 5. 소비처 마이그레이션 (리터럴 -> 상수)

| 파일 | 리터럴 -> 상수 | include 추가 |
|------|----------------|--------------|
| `main.cpp` | ScreenCamera/FxRoot -> `ACTOR_*`; ui_*/png -> `STR_UI_*`/`PATH_UI_*`; u_time/uInverseProjection/uDepth -> `UNI_*` (전부 app-root); GameContext/WaveSpawner -> `Stage::ACTOR_*`; BgmActor -> `Audio::ACTOR_BGM`; invert/blurring/sobel/grayscale_vignetting/fog -> `Playable::PASS_*` | `"Constants.h"`, `"apps/_MyApp_/src/Audio/Constants.h"`; (`apps/_MyApp_/src/Stage/Constants.h` 이미 include; `Playable::PASS_*` 는 PostFXConstants.h 경유 또는 명시) |
| `apps/_MyApp_/src/Stage/State/StageState.Impl.h` | `FindChild("GameContext")` -> `Stage::ACTOR_GAME_CONTEXT` | `"apps/_MyApp_/src/Stage/Constants.h"` (필요 시) |
| `apps/_MyApp_/src/Spawns/AmbientSequences.cpp` | `Actor("BgmActor")` -> `Audio::ACTOR_BGM` | `"apps/_MyApp_/src/Audio/Constants.h"` |
| `<Playable>/PostFXConstants.h` | `POSTFX_PROGRAM_CONFIGS` Name 8개 -> `PASS_*` | `"apps/_MyApp_/src/Playable/Constants.h"` |
| `<UI>/PostFXDebugLayer.cpp` | `entry.Name == "gamma"/"fog"/"bloom"/"grayscale_vignetting"` -> `PASS_*` | `"apps/_MyApp_/src/Playable/Constants.h"` |
| `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` | `HpGrayscalePostFX(...)` + `PostFXTweenPlayable(...)` 의 `"grayscale_vignetting"` (2곳) -> `Playable::PASS_GRAYSCALE_VIGNETTING` | `"apps/_MyApp_/src/Playable/Constants.h"` |

> include 경로: 모듈 Constants.h 는 `apps/_MyApp_/src` 가 include 경로라 `"apps/_MyApp_/src/Stage/Constants.h"` 등으로 해소. app-root 는 `"Constants.h"`.

## 6. 에러 처리 / 결정성

- 순수 *리터럴 -> 동일 값 named constant* 치환 -> **문자열 값 불변 = 거동 무변경**.
- 안전성 이득: 단일 출처. 오타 시 *미정의 식별자 컴파일 에러*. 교차참조(GameContext/BgmActor/PASS_*)는 한 곳 수정으로 전파.

## 7. 마이그레이션 / 영향

| 구분 | 파일 |
|------|------|
| 신규 | `apps/_MyApp_/src/Constants.h` |
| 모듈 Constants.h 추가 | `apps/_MyApp_/src/Stage/Constants.h`, `apps/_MyApp_/src/Audio/Constants.h`, `apps/_MyApp_/src/Playable/Constants.h` |
| 소비처 | `main.cpp`, `apps/_MyApp_/src/Stage/State/StageState.Impl.h`, `apps/_MyApp_/src/Spawns/AmbientSequences.cpp`, `<Playable>/PostFXConstants.h`, `<UI>/PostFXDebugLayer.cpp`, `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` |
| CMake | **변경 없음** (전부 헤더온리/기존 .cpp) |

**검증**: 빌드 GREEN + GUI 육안(문자열 값 동일 -> 전체 거동·orbital·PostFX 토글·BGM·FSM 동일해야 정상). `grep` 으로 main.cpp 기능 코드의 대상 리터럴 0 확인(로그/주석 제외).

## 8. Decision Log

| 날짜 | 결정 | 비고 |
|------|------|------|
| 2026-06-20 | S-1 scope C (전체) | "싸그리 다" |
| 2026-06-20 | S-2 기존 Constants.h 컨벤션 | ActorNames.h/PassName 초안 폐기 |
| 2026-06-20 | S-3 도메인 모듈 + app-root 분리 | 교차참조=도메인, 오케스트레이션=app-root |
| 2026-06-20 | S-4 constexpr const char* UPPER_SNAKE | 기존 apps/_MyApp_/src/Audio/Constants.h 식 |
