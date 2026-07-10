# 모듈별 Constants.h 튜닝값 중앙화 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `apps/_MyApp_/src/` 의 흩어진 게임 밸런스 튜닝 상수를 모듈별 `Constants.h` 로 모으고, Config struct 기본값/참조처가 그 상수를 가리키게 한다 (단일 소스 + 불일치 소거).

**Architecture:** top-level 모듈마다 `<Module>/Constants.h` 신설/확장 → `SCREAMING_SNAKE`+prefix `constexpr`/`const` 정의. A1 부착 — Config struct default-member-init 이 상수 참조 + Builder 중복 raw override 삭제. behavior-preserving (값 불변). 정본 spec: [`doc/superpowers/specs/2026-06-03-module-constants-centralization-design.md`](../specs/2026-06-03-module-constants-centralization-design.md).

**Tech Stack:** C++17, CMake/Ninja, header-only `constexpr`/`const`(내부 링키지 — ODR 안전), vmath.

---

## 검증 규약 (모든 태스크 공통 — TDD 대체)

> 프로젝트 메모리 **`no_auto_tests`** (사용자 요청 시에만 단위테스트) 가 writing-plans 의 TDD 기본을 override 한다. 본 리팩토링은 **behavior-preserving 상수 추출**이므로 태스크별 검증 = **빌드 GREEN + 값 보존 확인**.

- **빌드**: `cmake --build --preset ninja --target _MyApp_` — 기대: exit 0 (마지막 줄 `Linking CXX executable apps/_MyApp_/_MyApp_`). sb7 `gl.h and gl3.h` `#warning` 1건은 정상(무시).
- **값 보존**: 각 상수 = 추출 전 리터럴과 **동일 값** (인벤토리 아닌 코드가 정본 — 본 플랜의 before 블록이 현재값).
- **커밋**: `git commit <정확한 경로들> -m "..."` **path-scoped** (인덱스 전체 커밋 금지 — 사용자 병렬 git 보호). 커밋 메시지 한국어 `[refactor] :` 스타일. **`Co-Authored-By` 미사용** (메모리).
- **새 Constants.h 는 header-only** → CMakeLists 수정 불필요(빌드 타겟 아님, include 경로는 src 루트로 이미 해소). configure 재실행 불필요.
- **include 규칙**: clangd Strict(MissingIncludes). 상수를 직접 쓰는 `.h`/`.cpp` 는 해당 `Constants.h` 를 **직접 include**.
- **가드레일**: 엔진 코어 `src/<module>/`·셰이더·`src/Text/` 미접근. 리소스 키/경로·구조적 상수(`PhysicsLayer` 비트·`360.0f` wrap)·싱글턴 비대상.

---

## ⚠ 선행 조건 (Task 6a 전 필수)

`PlayerBuilder.cpp` / `PlayerBuilder.h` / `main.cpp` 는 **미커밋 변경** 보유:
- 본 세션의 **DeadCode 제거**(mSprite/mSpriteSeq — PlayerBuilder.h 필드 + PlayerBuilder.cpp 루프 + main.cpp) — 빌드 GREEN, 커밋 대기.
- 사용자 WIP — `PlayerBuilder.cpp:86` `reg.FindSound("shoot") //!` (+ :87-88 `//!`).

Task 6a 가 `PlayerBuilder.cpp` 를 또 수정하므로, **Task 6a 착수 전 위 미커밋 분을 정리**(DeadCode 커밋 + `"shoot"` WIP 사용자 결정)해야 path-scoped 커밋이 섞이지 않는다. → Task 6a 가 가장 마지막 Player 단계인 이유.

---

## Task 1: apps/_MyApp_/src/Physics/Constants.h

**Files:**
- Create: `apps/_MyApp_/src/Physics/Constants.h`
- Modify: `apps/_MyApp_/src/Physics/PhysicsImpulse.h`

> 참고: `kDurationSec` 는 이미 `mActiveTimer(kDurationSec)` 로 단일 참조 — 중복 아님(spec C4 는 리네임으로 흡수). `PhysicsLayer.h` 는 구조적이라 **불가침**.

- [ ] **Step 1: apps/_MyApp_/src/Physics/Constants.h 생성**

```cpp
#ifndef _TOPDOWNSHOOTER_PHYSICS_CONSTANTS__
#define _TOPDOWNSHOOTER_PHYSICS_CONSTANTS__

namespace TopdownShooter::Physics
{
	// Impulse(Player Dash / Enemy Knockback) 속도 버스트 튜닝.
	constexpr float IMPULSE_FORCE    = 2.5f; // 버스트 힘 (Stat DashForce base; 7.5->2.5 1/3 튜닝)
	constexpr float IMPULSE_COOLDOWN = 0.8f; // 재발동 쿨다운 (초)
	constexpr float IMPULSE_DURATION = 0.3f; // active 버스트 창 (초)
} // namespace TopdownShooter::Physics

#endif //_TOPDOWNSHOOTER_PHYSICS_CONSTANTS__
```

- [ ] **Step 2: PhysicsImpulse.h — include 추가 + 3곳 참조 + kDurationSec 제거**

include 블록(`#include "apps/_MyApp_/src/Physics/PhysicsComponent.h"` 아래)에 추가:
```cpp
#include "apps/_MyApp_/src/Physics/Constants.h"
```

생성자 초기화 리스트 (현재):
```cpp
		    : mImpulseForce(2.5f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce), // 7.5->2.5 (1/3 — 넉백 세기 튜닝)
		      mCooldown(0.8f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed),
		      mActiveTimer(kDurationSec),
```
→ 변경:
```cpp
		    : mImpulseForce(IMPULSE_FORCE, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce),
		      mCooldown(IMPULSE_COOLDOWN, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed),
		      mActiveTimer(IMPULSE_DURATION),
```

private 멤버의 `kDurationSec` 선언 (현재):
```cpp
		static constexpr float kDurationSec = 0.3f;   // active 창 (plain — 맞는 enum 없음)
```
→ **줄 삭제** (IMPULSE_DURATION 으로 대체됨).

- [ ] **Step 3: 빌드 GREEN**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0, `Linking CXX executable apps/_MyApp_/_MyApp_`.

- [ ] **Step 4: 커밋**

```bash
git commit apps/_MyApp_/src/Physics/Constants.h apps/_MyApp_/src/Physics/PhysicsImpulse.h \
  -m "[refactor] : Physics 튜닝상수 중앙화 — Constants.h(IMPULSE_FORCE/COOLDOWN/DURATION) + PhysicsImpulse 참조"
```

---

## Task 2: apps/_MyApp_/src/HUD/Constants.h

**Files:**
- Create: `apps/_MyApp_/src/HUD/Constants.h`
- Modify: `apps/_MyApp_/src/HUD/HealthBarFactory.h`

- [ ] **Step 1: apps/_MyApp_/src/HUD/Constants.h 생성**

```cpp
#ifndef _TOPDOWNSHOOTER_HUD_CONSTANTS__
#define _TOPDOWNSHOOTER_HUD_CONSTANTS__

#include <vmath.h>

namespace TopdownShooter::HUD
{
	// 머리 위 분절형 체력바 외형/배치. (Bootstrap EntityPresentation 의 healthBarColor 기본도 FILL 참조 — C3.)
	const     vmath::vec4 HEALTHBAR_FILL_COLOR      = vmath::vec4(0.13f, 1.0f, 0.0f, 1.0f); // 채움(레퍼런스 녹색)
	const     vmath::vec4 HEALTHBAR_BG_COLOR        = vmath::vec4(0.0f, 0.0f, 0.0f, 0.55f); // 빈 트랙
	constexpr float       HEALTHBAR_SEGMENT_COUNT   = 5.0f;
	constexpr float       HEALTHBAR_SEGMENT_SPACING = 0.08f;
	constexpr float       HEALTHBAR_HEAD_OFFSET     = 0.5f;                     // cameraUp 머리 위 거리
	const     vmath::vec2 HEALTHBAR_SIZE            = vmath::vec2(1.2f, 0.18f); // 가로×세로
} // namespace TopdownShooter::HUD

#endif //_TOPDOWNSHOOTER_HUD_CONSTANTS__
```

- [ ] **Step 2: HealthBarFactory.h — include 추가 + HealthBarConfig 6 기본값 참조**

`#include <vmath.h>` 아래에 추가:
```cpp
#include "apps/_MyApp_/src/HUD/Constants.h"
```

`struct HealthBarConfig` 본문 (현재):
```cpp
		vmath::vec4 fillColor      = vmath::vec4(0.13f, 1.0f, 0.0f, 1.0f); // 채워진 조각 (레퍼런스 녹색)
		vmath::vec4 bgColor        = vmath::vec4(0.0f, 0.0f, 0.0f, 0.55f); // 빈 조각 트랙
		float       segmentCount   = 5.0f;
		float       segmentSpacing = 0.08f;
		float       headOffset     = 0.5f;                     // cameraUp 방향 머리 위 거리 (1.2->0.2, 1.0 하향)
		vmath::vec2 size           = vmath::vec2(1.2f, 0.18f); // 바 가로×세로
```
→ 변경:
```cpp
		vmath::vec4 fillColor      = HEALTHBAR_FILL_COLOR;      // 채워진 조각 (레퍼런스 녹색)
		vmath::vec4 bgColor        = HEALTHBAR_BG_COLOR;        // 빈 조각 트랙
		float       segmentCount   = HEALTHBAR_SEGMENT_COUNT;
		float       segmentSpacing = HEALTHBAR_SEGMENT_SPACING;
		float       headOffset     = HEALTHBAR_HEAD_OFFSET;     // cameraUp 방향 머리 위 거리
		vmath::vec2 size           = HEALTHBAR_SIZE;            // 바 가로×세로
```

- [ ] **Step 3: 빌드 GREEN**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0.

- [ ] **Step 4: 커밋**

```bash
git commit apps/_MyApp_/src/HUD/Constants.h apps/_MyApp_/src/HUD/HealthBarFactory.h \
  -m "[refactor] : HUD 튜닝상수 중앙화 — Constants.h(HEALTHBAR_*) + HealthBarConfig 기본값 참조"
```

---

## Task 3: apps/_MyApp_/src/Stage/Constants.h

**Files:**
- Create: `apps/_MyApp_/src/Stage/Constants.h`
- Modify: `apps/_MyApp_/src/Stage/WaveController.h`, `apps/_MyApp_/src/Stage/WaveController.cpp`

> WaveController 의 `kSpawnInterval`/`kMaxEnemies` class static 은 **제거**하고 `.cpp` 에서 `WAVE_*` 직접 사용(단일 소스). `WAVE_HP_BASE`(20) 와 `ENEMY_HP`(30, Task 6b) 는 **다른 개념** — 합치지 말 것.

- [ ] **Step 1: apps/_MyApp_/src/Stage/Constants.h 생성**

```cpp
#ifndef _TOPDOWNSHOOTER_STAGE_CONSTANTS__
#define _TOPDOWNSHOOTER_STAGE_CONSTANTS__

namespace TopdownShooter::Stage
{
	// 웨이브 스폰/난이도 곡선.
	constexpr float WAVE_SPAWN_INTERVAL = 3.0f; // 적 스폰 간격(초)
	constexpr int   WAVE_MAX_ENEMIES    = 5;    // 동시 생존 최대
	constexpr int   WAVE_HP_BASE        = 20;   // 적 HP = BASE + wave*PER_WAVE
	constexpr int   WAVE_HP_PER_WAVE    = 5;
	constexpr float WAVE_SPEED_BASE     = 1.5f; // 적 속도 = BASE + wave*PER_WAVE
	constexpr float WAVE_SPEED_PER_WAVE = 0.3f;
	constexpr int   WAVE_CONTACT_DAMAGE = 10;   // 접촉 데미지(전 웨이브 일정)
} // namespace TopdownShooter::Stage

#endif //_TOPDOWNSHOOTER_STAGE_CONSTANTS__
```

- [ ] **Step 2: WaveController.h — class static 2개 제거**

private 멤버 끝 (현재):
```cpp
		static constexpr float kSpawnInterval = 3.0f;
		static constexpr int   kMaxEnemies    = 5;
```
→ **두 줄 삭제**. (`.cpp` 가 `WAVE_*` 직접 참조.)

- [ ] **Step 3: WaveController.cpp — include 추가 + 곡선/게이트 참조**

`#include "apps/_MyApp_/src/Bootstrap/EnemyBuilder.h"` 아래에 추가:
```cpp
#include "apps/_MyApp_/src/Stage/Constants.h"
```

`SpawnEnemy()` 의 deps 세팅 (현재):
```cpp
		d.hp           = 20 + mWave * 5;
		d.speed        = 1.5f + static_cast<float>(mWave) * 0.3f;
		d.damage       = 10;
```
→ 변경:
```cpp
		d.hp           = WAVE_HP_BASE + mWave * WAVE_HP_PER_WAVE;
		d.speed        = WAVE_SPEED_BASE + static_cast<float>(mWave) * WAVE_SPEED_PER_WAVE;
		d.damage       = WAVE_CONTACT_DAMAGE;
```

`Update()` 의 스폰 게이트 (현재):
```cpp
		if (mSpawnTimer >= kSpawnInterval && LiveCount() < kMaxEnemies)
```
→ 변경:
```cpp
		if (mSpawnTimer >= WAVE_SPAWN_INTERVAL && LiveCount() < WAVE_MAX_ENEMIES)
```

- [ ] **Step 4: 빌드 GREEN**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0.

- [ ] **Step 5: 커밋**

```bash
git commit apps/_MyApp_/src/Stage/Constants.h apps/_MyApp_/src/Stage/WaveController.h apps/_MyApp_/src/Stage/WaveController.cpp \
  -m "[refactor] : Stage 튜닝상수 중앙화 — Constants.h(WAVE_*) + WaveController 곡선/게이트 참조"
```

---

## Task 4: apps/_MyApp_/src/Playable/Constants.h 확장

**Files:**
- Modify: `apps/_MyApp_/src/Playable/Constants.h` (기존 확장)
- Modify: `apps/_MyApp_/src/Playable/SpriteFxPlayable.h`

> `VIGNETTE_*` 는 여기서 **정의만** 하고 참조처(PlayerBuilder.cpp:116)는 Task 6a 에서 갱신(PlayerBuilder.cpp 수정을 한 태스크에 모음). 미사용 namespace-scope constexpr 는 빌드 무해.

- [ ] **Step 1: apps/_MyApp_/src/Playable/Constants.h 끝에 4 상수 추가**

`const EntityTextureConfig ENEMY_FRONT[3] = {...};` 블록 **아래**, namespace 닫는 `};` **위**에 삽입:
```cpp

	// ── 연출 지속시간 / PostFX 튜닝 (분해 Task6) ──
	constexpr float SPRITE_HIT_FLASH_DURATION = 0.18f; // 피격 hit-flash 표시(초)
	constexpr float SPRITE_DISSOLVE_DURATION  = 1.0f;  // 사망 dissolve 표시 기본(초; Player/Enemy override)
	constexpr float VIGNETTE_PEAK             = 0.45f; // 피격 비네팅 시작 강도(0.45->0)
	constexpr int   VIGNETTE_DURATION_MS      = 300;   // 피격 비네팅 tween 길이(ms)
```

- [ ] **Step 2: SpriteFxPlayable.h — include 추가 + 2 default-arg 참조**

`#include "playable/playable_base.h"` 아래에 추가:
```cpp
#include "apps/_MyApp_/src/Playable/Constants.h"
```

`SpriteHitFlashPlayable` ctor (현재):
```cpp
		explicit SpriteHitFlashPlayable(SJH::Scene::Actor *target, float durationSec = 0.18f);
```
→ 변경:
```cpp
		explicit SpriteHitFlashPlayable(SJH::Scene::Actor *target, float durationSec = SPRITE_HIT_FLASH_DURATION);
```

`SpriteDissolvePlayable` ctor (현재):
```cpp
		explicit SpriteDissolvePlayable(SJH::Scene::Actor *target, float durationSec = 1.0f);
```
→ 변경:
```cpp
		explicit SpriteDissolvePlayable(SJH::Scene::Actor *target, float durationSec = SPRITE_DISSOLVE_DURATION);
```

- [ ] **Step 3: 빌드 GREEN**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0.

- [ ] **Step 4: 커밋**

```bash
git commit apps/_MyApp_/src/Playable/Constants.h apps/_MyApp_/src/Playable/SpriteFxPlayable.h \
  -m "[refactor] : Playable 연출상수 추가 — Constants.h(SPRITE_*_DURATION/VIGNETTE_*) + SpriteFx 기본값 참조"
```

---

## Task 5: apps/_MyApp_/src/Bootstrap/Constants.h

**Files:**
- Create: `apps/_MyApp_/src/Bootstrap/Constants.h`
- Modify: `apps/_MyApp_/src/Bootstrap/EntityPresentation.h`, `apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp`

> **선행**: Task 2(HUD) 완료 필요 — C3(`HUD::HEALTHBAR_FILL_COLOR`) 참조. `EnemyDeps.healthBarColor`(적 빨강, 단일 사용)·트윈 진폭(0.85~1.15/±30)은 **스펙 외**라 미추출(인라인 유지).

- [ ] **Step 1: apps/_MyApp_/src/Bootstrap/Constants.h 생성**

```cpp
#ifndef _TOPDOWNSHOOTER_BOOTSTRAP_CONSTANTS__
#define _TOPDOWNSHOOTER_BOOTSTRAP_CONSTANTS__

namespace TopdownShooter::Bootstrap
{
	// EntityPresentation 사망 연출 길이(초) — Player 기본 / Enemy override.
	constexpr float PLAYER_DISSOLVE_SECONDS = 1.5f;
	constexpr float PLAYER_DEATH_DELAY      = 1.5f;
	constexpr float ENEMY_DISSOLVE_SECONDS  = 0.6f;
	constexpr float ENEMY_DEATH_DELAY       = 0.6f;
	// 적 상시 트윈 길이(ms 편도).
	constexpr int   ENEMY_SCALE_PULSE_MS    = 400;
	constexpr int   ENEMY_ROT_WOBBLE_MS     = 2000;
} // namespace TopdownShooter::Bootstrap

#endif //_TOPDOWNSHOOTER_BOOTSTRAP_CONSTANTS__
```

- [ ] **Step 2: EntityPresentation.h — include 2개 + 3 기본값 참조(C3 포함)**

`#include <vmath.h>` 아래에 추가:
```cpp
#include "apps/_MyApp_/src/Bootstrap/Constants.h"
#include "apps/_MyApp_/src/HUD/Constants.h"
```

`struct EntityPresentationConfig` 본문 (현재):
```cpp
		float       dissolveSeconds   = 1.5f;                                 // "death" SpriteDissolve 길이
		float       deathDelaySeconds = 1.5f;                                 // 사망 후 비활성 지연(=dissolve 가시화 창)
		vmath::vec4 healthBarColor    = vmath::vec4(0.13f, 1.0f, 0.0f, 1.0f); // 체력바 채움색(기본 녹색 = HealthBarConfig 기본과 동일)
```
→ 변경:
```cpp
		float       dissolveSeconds   = PLAYER_DISSOLVE_SECONDS;  // "death" SpriteDissolve 길이
		float       deathDelaySeconds = PLAYER_DEATH_DELAY;       // 사망 후 비활성 지연(=dissolve 가시화 창)
		vmath::vec4 healthBarColor    = HUD::HEALTHBAR_FILL_COLOR; // 체력바 채움색(녹색 — HUD 단일 소스, C3)
```

- [ ] **Step 3: EnemyBuilder.cpp — include 추가 + dissolve/delay + 트윈 길이 참조**

`#include "scene/actor.h"` 아래(또는 include 블록 적당한 위치)에 추가:
```cpp
#include "apps/_MyApp_/src/Bootstrap/Constants.h"
```

scaleTween 정의 (현재):
```cpp
			auto scaleTween = tweeny::from(0.85f).to(1.15f)
			                      .during(400)
			                      .via(tweeny::easing::sinusoidalInOut);
```
→ `.during(400)` 만 변경:
```cpp
			auto scaleTween = tweeny::from(0.85f).to(1.15f)
			                      .during(ENEMY_SCALE_PULSE_MS)
			                      .via(tweeny::easing::sinusoidalInOut);
```

rotTween 정의 (현재):
```cpp
			auto rotTween = tweeny::from(-30.0f).to(30.0f)
			                    .during(2000)
			                    .via(tweeny::easing::sinusoidalInOut);
```
→ `.during(2000)` 만 변경:
```cpp
			auto rotTween = tweeny::from(-30.0f).to(30.0f)
			                    .during(ENEMY_ROT_WOBBLE_MS)
			                    .via(tweeny::easing::sinusoidalInOut);
```

EntityPresentationConfig 세팅 (현재):
```cpp
			pres.dissolveSeconds   = 0.6f;
			pres.deathDelaySeconds = 0.6f;
```
→ 변경 (`pres.healthBarColor = deps.healthBarColor;` 는 유지):
```cpp
			pres.dissolveSeconds   = ENEMY_DISSOLVE_SECONDS;
			pres.deathDelaySeconds = ENEMY_DEATH_DELAY;
```

- [ ] **Step 4: 빌드 GREEN**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0.

- [ ] **Step 5: 커밋**

```bash
git commit apps/_MyApp_/src/Bootstrap/Constants.h apps/_MyApp_/src/Bootstrap/EntityPresentation.h apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp \
  -m "[refactor] : Bootstrap 연출상수 중앙화 — Constants.h(DISSOLVE/DELAY/트윈MS) + EntityPresentation·EnemyBuilder 참조(C3 녹색 HUD 단일소스)"
```

---

## Task 6a: apps/_MyApp_/src/Entity/Constants.h 생성 + Player 참조

**Files:**
- Create: `apps/_MyApp_/src/Entity/Constants.h`
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerActor.h`
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerHand.h`, `apps/_MyApp_/src/Entity/Player/PlayerHand.cpp`

> **이 태스크는 clean**(미커밋 충돌 없음) — Enemy/Bullet 상수도 이 파일에 함께 정의(Task 6b/6c 가 참조). **PlayerBuilder.cpp 참조는 Task 8(게이트)로 분리**. 6a 후 struct 기본값은 상수-backed 로 바뀌지만 PlayerBuilder 의 중복 override(3.0 등)가 Task 8 까지 잔존 — 값이 동일하므로 동작 보존(빌드 GREEN).

- [ ] **Step 1: apps/_MyApp_/src/Entity/Constants.h 생성 (Player+Hand+Enemy+Bullet 전체)**

```cpp
#ifndef _TOPDOWNSHOOTER_ENTITY_CONSTANTS__
#define _TOPDOWNSHOOTER_ENTITY_CONSTANTS__

namespace TopdownShooter::Entity
{
	// ── Player (PlayerActorConfig 기본값) ──
	constexpr int   PLAYER_HP             = 100;
	constexpr float PLAYER_MOVE_SPEED     = 3.0f; // C1: struct 기본(5.0)↔Builder(3.0) 불일치 -> 실값 3.0
	constexpr float PLAYER_LINEAR_DAMPING = 5.0f;
	constexpr int   PLAYER_WEAPON_DAMAGE  = 10;
	constexpr float PLAYER_SPRITE_FPS     = 8.0f; // C2: SpriteCfg::fps + PlayerBuilder kFps 통합

	// ── Hand (PlayerHands 배치) ──
	constexpr float HAND_SPREAD_DEG       = 25.0f; // forward(-Z) 기준 좌(+)/우(-) 벌림각
	constexpr float HAND_RADIUS           = 0.6f;  // forward 거리(player local)
	constexpr float HAND_Y_OFFSET         = 0.0f;
	constexpr float HAND_SCALE            = 0.25f; // 손 스프라이트 균등 Scale
	constexpr int   HAND_QUEUE_OFFSET     = 10;    // 몸통 레이어 위

	// ── Enemy (EnemyConfig/EnemyDeps 기본 + factory 물리) ──
	constexpr int   ENEMY_HP              = 30;
	constexpr float ENEMY_SPEED           = 2.0f;
	constexpr int   ENEMY_DAMAGE          = 10;
	constexpr float ENEMY_LINEAR_DAMPING  = 0.5f;
	constexpr float ENEMY_FRICTION        = 0.3f;
	constexpr float ENEMY_RADIUS          = 0.4f;
	constexpr float ENEMY_SPRITE_FPS      = 6.0f; // 2프레임 walk 애니 속도

	// ── Bullet (BulletConfig 기본 + 물리/시각 반지름) ──
	constexpr float BULLET_SPEED          = 15.0f;
	constexpr int   BULLET_DAMAGE         = 10;
	constexpr float BULLET_LIFETIME       = 3.0f;
	constexpr float BULLET_RADIUS         = 0.15f; // C5: 물리 CircleBody + 시각 Sphere mesh 동기
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_CONSTANTS__
```

- [ ] **Step 2: PlayerActor.h — include 추가 + 5 struct 기본값 참조**

`#include "apps/_MyApp_/src/Playable/Constants.h"` 아래에 추가:
```cpp
#include "apps/_MyApp_/src/Entity/Constants.h"
```

각 nested struct 기본값 (현재 → 변경):
```cpp
		struct LifeCfg     { int   hp = 100; };          // → int   hp = PLAYER_HP;
		struct MovementCfg { float speed = 5.0f; };      // → float speed = PLAYER_MOVE_SPEED;
```
PhysicsCfg 의 `float linearDamping = 5.0f;` → `float linearDamping = PLAYER_LINEAR_DAMPING;`
WeaponCfg 의 `int damage = 10;` → `int damage = PLAYER_WEAPON_DAMAGE;`
SpriteCfg 의 `float fps = 8.0f;` → `float fps = PLAYER_SPRITE_FPS;`

(나머지 PhysicsCfg 필드 density=1.0f/friction=0.3f/size/startPosition 등은 **상수 비대상 — 유지**.)

- [ ] **Step 3: PlayerHand.h — include 추가 + class static 5개 제거 + ctor 기본값 참조**

`#include "scene/actor.h"` 아래에 추가:
```cpp
#include "apps/_MyApp_/src/Entity/Constants.h"
```

`PlayerSingleHand` ctor 선언 (현재):
```cpp
		explicit PlayerSingleHand(float spreadDeg = 25.0f, float radius = 0.6f, float yOffset = 0.0f, float scale = 1.0f);
```
→ 변경 (scale 은 "무배율" 기본 1.0 유지 — HAND_SCALE 와 의미 다름):
```cpp
		explicit PlayerSingleHand(float spreadDeg = HAND_SPREAD_DEG, float radius = HAND_RADIUS, float yOffset = HAND_Y_OFFSET, float scale = 1.0f);
```

`PlayerHands` private 의 static constexpr 5줄 (현재) **삭제**:
```cpp
		// 양손 기본 배치 — forward(-Z) 기준 ±벌림각 + 거리 + 스프라이트 크기. (비주얼 튜닝 대상)
		static constexpr float kSpreadDeg       = 25.0f;
		static constexpr float kRadius          = 0.6f;
		static constexpr float kYOffset         = 0.0f;
		static constexpr float kHandScale       = 0.25f; // 손 스프라이트가 커서 축소 (Scale 만 — 궤도 무관)
		static constexpr int   kHandQueueOffset = 10; // 플레이어 몸통 레이어(DrawOrder 0~3) 위 (튜닝 대상)
```
(이로써 `private:` 섹션이 비면 `private:` 라벨도 함께 제거. `PlayerHands` 는 public 멤버만 남음.)

- [ ] **Step 4: PlayerHand.cpp — include 추가 + k상수 사용처를 HAND_* 로**

`#include "apps/_MyApp_/src/Entity/Player/PlayerHand.h"` 아래에 추가:
```cpp
#include "apps/_MyApp_/src/Entity/Constants.h"
```

`makeHand` 람다 내부 (현재):
```cpp
			hand->AddComponent<PlayerSingleHand>(spreadDeg, kRadius, kYOffset, kHandScale);
```
→ 변경:
```cpp
			hand->AddComponent<PlayerSingleHand>(spreadDeg, HAND_RADIUS, HAND_Y_OFFSET, HAND_SCALE);
```

`spr->QueueOffset` (현재):
```cpp
				spr->QueueOffset = kHandQueueOffset; // 플레이어 몸통 위 레이어 (튜닝 대상)
```
→ 변경:
```cpp
				spr->QueueOffset = HAND_QUEUE_OFFSET; // 플레이어 몸통 위 레이어
```

makeHand 호출 2줄 (현재):
```cpp
		makeHand("LeftHand", +kSpreadDeg);
		makeHand("RightHand", -kSpreadDeg);
```
→ 변경:
```cpp
		makeHand("LeftHand", +HAND_SPREAD_DEG);
		makeHand("RightHand", -HAND_SPREAD_DEG);
```

- [ ] **Step 5: 빌드 GREEN**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0. (PlayerActorConfig 기본값이 상수-backed 로 바뀌었지만 PlayerBuilder 의 override(3.0 등)가 잔존 -> 동작 동일.)

- [ ] **Step 6: 커밋 (clean — PlayerBuilder.cpp 미포함)**

```bash
git commit apps/_MyApp_/src/Entity/Constants.h apps/_MyApp_/src/Entity/Player/PlayerActor.h \
  apps/_MyApp_/src/Entity/Player/PlayerHand.h apps/_MyApp_/src/Entity/Player/PlayerHand.cpp \
  -m "[refactor] : apps/_MyApp_/src/Entity/Constants.h 신설 + Player struct/Hand 참조 — PLAYER_*/HAND_* 단일소스(C1 speed 기본·C2 fps), PlayerBuilder 갱신은 Task 8"
```

---

## Task 6b: Entity — Enemy 참조

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h`
- Modify: `apps/_MyApp_/src/Bootstrap/EnemyBuilder.h`

> `apps/_MyApp_/src/Entity/Constants.h` 는 Task 6a 에서 이미 생성됨. `EnemyConfig`(Entity) 와 `EnemyDeps`(Bootstrap) 둘 다 기본값 보유 — 둘 다 `Entity::ENEMY_*` 참조.

- [ ] **Step 1: EnemyFactory.h — include 추가 + EnemyConfig 3 기본값 + factory 물리 3값**

`#include "scene/actor.h"` 아래에 추가:
```cpp
#include "apps/_MyApp_/src/Entity/Constants.h"
```

`struct EnemyConfig` 기본값 (현재):
```cpp
		int   hp     = 30;
		float speed  = 2.0f;
		int   damage = 10;
```
→ 변경:
```cpp
		int   hp     = ENEMY_HP;
		float speed  = ENEMY_SPEED;
		int   damage = ENEMY_DAMAGE;
```

`CreateEnemyActor` 의 BodyConfig + CircleBody (현재):
```cpp
		bc.linearDamping = 0.5f;
		bc.density       = 1.0f;
		bc.friction      = 0.3f;
```
→ `linearDamping`·`friction` 만 변경 (density 유지):
```cpp
		bc.linearDamping = ENEMY_LINEAR_DAMPING;
		bc.density       = 1.0f;
		bc.friction      = ENEMY_FRICTION;
```
그리고 (현재):
```cpp
		auto* pb = actor->AddComponent<Physics::Components::CircleBody>(bc, 0.4f);
```
→ 변경:
```cpp
		auto* pb = actor->AddComponent<Physics::Components::CircleBody>(bc, ENEMY_RADIUS);
```

- [ ] **Step 2: EnemyBuilder.h — include 추가 + EnemyDeps 4 기본값 참조**

`#include <vmath.h>` 아래에 추가:
```cpp
#include "apps/_MyApp_/src/Entity/Constants.h"
```

`struct EnemyDeps` 기본값 (현재):
```cpp
		int                hp           = 30;
		float              speed        = 2.0f;
		int                damage       = 10;
```
→ 변경 (namespace Bootstrap → 한정자 필요):
```cpp
		int                hp           = Entity::ENEMY_HP;
		float              speed        = Entity::ENEMY_SPEED;
		int                damage       = Entity::ENEMY_DAMAGE;
```
그리고 (현재):
```cpp
		float              spriteFps    = 6.0f;        ///< 2프레임 walk 애니 속도
```
→ 변경 (`healthBarColor` 적 빨강은 스펙 외 — 유지):
```cpp
		float              spriteFps    = Entity::ENEMY_SPRITE_FPS; ///< 2프레임 walk 애니 속도
```

- [ ] **Step 3: 빌드 GREEN**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0.

- [ ] **Step 4: 커밋**

```bash
git commit apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h apps/_MyApp_/src/Bootstrap/EnemyBuilder.h \
  -m "[refactor] : Entity Enemy 튜닝상수 참조 — EnemyConfig/EnemyDeps 기본값 + factory 물리 = ENEMY_*"
```

---

## Task 6c: Entity — Bullet 참조

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Bullet/bullet_factory.h`
- Modify: `apps/_MyApp_/src/Entity/Components/WeaponComponents.cpp`

- [ ] **Step 1: bullet_factory.h — include 추가 + BulletConfig 3 기본값 + 반지름 2곳(C5)**

`#include "scene/actor.h"` 아래에 추가:
```cpp
#include "apps/_MyApp_/src/Entity/Constants.h"
```

`struct BulletConfig` 기본값 (현재):
```cpp
		float       speed    = 15.0f;
		int         damage   = 10;
		float       lifetime = 3.0f;
```
→ 변경:
```cpp
		float       speed    = BULLET_SPEED;
		int         damage   = BULLET_DAMAGE;
		float       lifetime = BULLET_LIFETIME;
```

물리 CircleBody 반지름 (현재):
```cpp
		actor->AddComponent<Physics::Components::CircleBody>(bc, 0.15f);
```
→ 변경:
```cpp
		actor->AddComponent<Physics::Components::CircleBody>(bc, BULLET_RADIUS);
```

시각 Sphere mesh 반지름 (현재):
```cpp
				SJH::MeshData data = SJH::Geometry::Sphere(0.0, kTwoPi, 16, 0.0, 1.0, 8, 0.15f);
```
→ 변경 (주석의 "반지름 0.15" 도 의미 보존 — C5 동기):
```cpp
				SJH::MeshData data = SJH::Geometry::Sphere(0.0, kTwoPi, 16, 0.0, 1.0, 8, BULLET_RADIUS);
```

- [ ] **Step 2: WeaponComponents.cpp — 중복 override 2줄 삭제**

`UseWeapon` 의 BulletConfig 세팅 (현재):
```cpp
		bc.world    = mWorld;
		bc.pos      = b2pos;
		bc.dir      = box2dForward; // 정규화 가정 (PlayerController 가 정규화 후 전달)
		bc.speed    = 15.0f;
		bc.damage   = static_cast<int>(Damage.GetValue());
		bc.lifetime = 3.0f;
```
→ `bc.speed`·`bc.lifetime` **삭제** (BulletConfig 기본값 BULLET_SPEED/BULLET_LIFETIME 로 충분 — A1 중복 제거; `bc.damage` 는 동적이라 유지):
```cpp
		bc.world    = mWorld;
		bc.pos      = b2pos;
		bc.dir      = box2dForward; // 정규화 가정 (PlayerController 가 정규화 후 전달)
		bc.damage   = static_cast<int>(Damage.GetValue());
```
(WeaponComponents.cpp 는 상수를 직접 안 쓰게 되므로 **include 추가 불필요**.)

- [ ] **Step 3: 빌드 GREEN**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0.

- [ ] **Step 4: 커밋**

```bash
git commit apps/_MyApp_/src/Entity/Bullet/bullet_factory.h apps/_MyApp_/src/Entity/Components/WeaponComponents.cpp \
  -m "[refactor] : Entity Bullet 튜닝상수 참조 — BulletConfig 기본값 + 반지름(C5 물리·시각 동기) = BULLET_*, Weapon 중복 override 삭제"
```

---

## Task 7: InputHandler — CameraController pitch clamp 명명

**Files:**
- Modify: `apps/_MyApp_/src/InputHandler/CameraController.cpp`

> 1값뿐이라 모듈 Constants.h 신설 안 함 — 파일상단 익명 ns `constexpr` 로 명명만 (spec §5.7).

- [ ] **Step 1: 파일상단 익명 ns 상수 추가**

include 블록 끝(`#include <vmath.h>` 아래), `namespace TopdownShooter::Controller` **위**에 삽입:
```cpp

namespace
{
	constexpr float kCameraPitchClampDeg = 89.0f; // gimbal lock 회피 pitch 상한(±)
}
```

- [ ] **Step 2: BindLookHandler 의 clamp 4줄 참조**

(현재):
```cpp
			if (mPitchDeg > 89.0f)
				mPitchDeg = 89.0f;
			if (mPitchDeg < -89.0f)
				mPitchDeg = -89.0f;
```
→ 변경:
```cpp
			if (mPitchDeg > kCameraPitchClampDeg)
				mPitchDeg = kCameraPitchClampDeg;
			if (mPitchDeg < -kCameraPitchClampDeg)
				mPitchDeg = -kCameraPitchClampDeg;
```

- [ ] **Step 3: 빌드 GREEN**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0.

- [ ] **Step 4: 커밋**

```bash
git commit apps/_MyApp_/src/InputHandler/CameraController.cpp \
  -m "[refactor] : CameraController pitch clamp 명명 — 89.0f -> kCameraPitchClampDeg(파일상단)"
```

---

## Task 8: PlayerBuilder.cpp 참조 (⚠ 게이트 — 미커밋 정리 후, 최후)

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp`

> ⚠ **선행 조건(문서 상단) 처리 후 착수.** PlayerBuilder.cpp 는 ① 본 세션 DeadCode 제거(미커밋) ② 사용자 `"shoot"` WIP 보유. 본 태스크 커밋 전 둘을 정리해야 path-scoped 커밋이 안 섞인다. **전제**: Task 1(IMPULSE 무관)·**Task 4(VIGNETTE_* 정의됨)·Task 6a(Entity 상수·PLAYER_SPRITE_FPS 정의됨)** 머지 완료.

- [ ] **Step 1: include 추가**

include 블록(`#include "apps/_MyApp_/src/Entity/Player/PlayerActor.h"` 아래)에 추가:
```cpp
#include "apps/_MyApp_/src/Entity/Constants.h"
```

- [ ] **Step 2: 중복 override 4줄 삭제**

`BuildPlayer` 의 pac 세팅 중 **4줄 삭제** (struct 기본값이 Task 6a 에서 상수-backed 동일값 — A1 중복 제거):
```cpp
		pac.life.hp = 100;                  // 삭제 (기본 PLAYER_HP=100)
		pac.movement.speed = 3.0f;          // 삭제 (기본 PLAYER_MOVE_SPEED=3.0; C1 해소)
		pac.physics.linearDamping = 5.0f;   // 삭제 (기본 PLAYER_LINEAR_DAMPING=5.0)
		pac.weapon.damage = 10;             // 삭제 (기본 PLAYER_WEAPON_DAMAGE=10)
```
(`pac.weapon.world = deps.physicsWorld;`·`pac.physics.world/size/startPosition/density/categoryBits/maskBits`·`pac.controller.*` 는 **유지** — dep/계산값 또는 상수 비대상.)

- [ ] **Step 3: kFps 참조 (C2)**

`BuildPlayerDirectionalGroups` 의 kFps (현재):
```cpp
			constexpr float kFps = 8.0f; // 애니(ColCount>1) 초당 프레임 (SpriteCfg 기본과 동일)
```
→ 변경:
```cpp
			constexpr float kFps = TopdownShooter::Entity::PLAYER_SPRITE_FPS; // 애니 초당 프레임 (단일 소스)
```

- [ ] **Step 4: VIGNETTE 참조**

`RegisterPlayerCombatPlayables` 의 "hit" 비네팅 tween (현재):
```cpp
				    tweeny::from(0.45f).to(0.0f).during(300).via(tweeny::easing::sinusoidalInOut)));
```
→ 변경 (Task 4 정의 참조; PlayerBuilder.cpp 는 apps/_MyApp_/src/Playable/Constants.h 이미 include):
```cpp
				    tweeny::from(TopdownShooter::Playable::VIGNETTE_PEAK).to(0.0f).during(TopdownShooter::Playable::VIGNETTE_DURATION_MS).via(tweeny::easing::sinusoidalInOut)));
```

- [ ] **Step 5: 빌드 GREEN**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: exit 0.

- [ ] **Step 6: 커밋 (⚠ 사용자와 PlayerBuilder.cpp 상태 확인 후)**

DeadCode 제거분/`"shoot"` WIP 정리 확인 후:
```bash
git commit apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp \
  -m "[refactor] : PlayerBuilder 튜닝상수 참조 — 중복 override 4 삭제(C1 speed) + kFps=PLAYER_SPRITE_FPS(C2) + VIGNETTE_* 참조"
```

---

## 완료 후 상태

- 신설 6: `apps/_MyApp_/src/Physics/Constants.h`·`apps/_MyApp_/src/HUD/Constants.h`·`apps/_MyApp_/src/Stage/Constants.h`·`apps/_MyApp_/src/Bootstrap/Constants.h`·`apps/_MyApp_/src/Entity/Constants.h` + `apps/_MyApp_/src/Playable/Constants.h` 확장.
- 참조 갱신: PhysicsImpulse·HealthBarFactory·WaveController·SpriteFxPlayable·EntityPresentation·EnemyBuilder·PlayerActor·PlayerHand·EnemyFactory·EnemyBuilder·bullet_factory·WeaponComponents·CameraController·PlayerBuilder.
- 해소: C1(speed 5↔3)·C2(kFps 중복)·C3(healthBarColor 녹색 단일)·C5(bullet radius 동기). (C4 는 리네임 흡수.)
- **10 커밋** (Task 1~5, 6a, 6b, 6c, 7, **8**). 각 빌드 GREEN. **Task 1~7(=1~5,6a,6b,6c,7) 은 clean — 즉시 실행 가능. Task 8(PlayerBuilder.cpp) 만 게이트.**
</content>
</invoke>
