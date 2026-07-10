# 모듈별 Constants.h 튜닝값 중앙화 — 설계 (Design Spec)

- **날짜**: 2026-06-03
- **대상**: `apps/_MyApp_/src/` (탑다운 슈터 게임 클라이언트)
- **상태**: 설계 확정 (사용자 승인 2026-06-03 "A1을 진행") → writing-plans 대기
- **저장 위치**: `doc/` 는 gitignored — 본 spec 은 로컬 전용 (기존 spec 들과 동일 컨벤션)

---

## 1. 배경 / 동기

게임 밸런스·연출 "필(feel)" 수치(hp·speed·damage·cooldown·spread·radius·fps·duration·color 등)가
클래스 헤더의 `static constexpr`, 생성자 default-argument, `.cpp` 로직/Builder 의 inline 리터럴로 **~13개 모듈에 흩어져** 있다 (사전 인벤토리 ~60 knob — 부록 A).

문제:
- 밸런싱하려면 어느 파일에 그 수치가 있는지 추적해야 함 (가시성 낮음).
- **불일치(divergence)** 가 이미 존재 — 예: `PlayerActorConfig::MovementCfg::speed` 기본값 `5.0f` 인데 `PlayerBuilder.cpp` 가 `3.0f` 로 덮어써서, struct 기본값 `5.0` 은 죽은 값이고 실제 속도는 `3.0`.
- 같은 의미의 수치가 두 곳에 복제 (예: `healthBarColor` 가 EntityPresentation 과 HealthBarFactory 에 동일 RGBA 로 중복).

목표: 각 top-level 모듈이 자기 튜닝 knob 을 **단일 소스 `Constants.h`** 로 보유하게 하여 가시성·단일 소스·불일치 제거를 달성한다. (이미 `apps/_MyApp_/src/Playable/Constants.h`, `apps/_MyApp_/src/Physics/PhysicsLayer.h` 가 선례.)

비목표 (YAGNI):
- 가변 전역 상태/싱글턴 점검·축소 (`Manager` 등) — 건드리지 않음.
- 자유 헬퍼 함수 재배치 — 익명 ns 헬퍼는 file-private 유지.

---

## 2. 확정된 결정 (사용자 승인)

| # | 축 | 결정 |
|---|---|---|
| D1 | 1차 목적 | **설정값 중앙화** (가변 전역/자유함수 비대상) |
| D2 | 타깃 모델 | **모듈별 `Constants.h` · 튜닝값 중심**. 리소스 키/셰이더·텍스처 경로는 `.cpp` 익명 ns 에 잔류 (locality 유지) |
| D3 | granularity | **top-level src 모듈 단위**. 단 knob 0개 모듈엔 빈 파일 안 만듦 (실보유 모듈만) |
| D4 | 포함 범위 | **Builder Config 세팅값도 포함** — 모든 밸런스 수치가 named constant. Builder/struct 가 상수 참조 |
| D5 | 부착점 | **A1** — `Constants.h` 가 단일 소스. **Config struct default-member-init 이 상수 참조** + Builder 중복 raw override 삭제 + divergence 소거 |

---

## 3. 스코프 & 경계

### In (대상)
- 게임 밸런스/필 knob: hp·speed·damage·cooldown·spread·radius·fps·duration·color·count·spacing·offset·force·damping·friction.

### Out (그대로 둠)
- **리소스 키/경로 문자열** — `kPlaneKey="stage_plane"`, 셰이더 `.vs/.fs`, 텍스처 `.png`, 모델 `.fbx`. (.cpp 익명 ns 잔류.)
- **구조적 상수** — `PhysicsLayer` enum 비트(`1ull<<n`)/마스크, `MAX_*_LIGHTS`, DrawOrder 인덱스, 각도 wrap `360.0f`, identity `0.0f/1.0f`.
- **가변 전역/싱글턴** — `Manager`, `PostFXRegistry`.
- **const-correctness** — `const` 파라미터/멤버함수.
- **엔진 코어 `src/<module>/`** — 불가침. 본 작업은 `apps/_MyApp_/src/` 한정.

### "일괄(D3)" 의 운영 해석
granularity 는 top-level 고정(Entity 하위 Player/Bullet/Enemy 는 `apps/_MyApp_/src/Entity/Constants.h` 로 roll-up). knob 0개 모듈(Audio·VFX·Spawns·Algebraic·UI·Tween)엔 **빈 Constants.h 를 만들지 않는다** — 실보유 6개 신설/확장 + InputHandler 예외 1.

---

## 4. 컨벤션 (모든 `Constants.h` 공통)

- **파일**: `<Module>/Constants.h`
- **네임스페이스**: 모듈 top-level (`TopdownShooter::Entity`, `::Stage`, `::Physics`, `::HUD`, `::Bootstrap`, 기존 `::Playable`)
- **헤더 가드**: `_TOPDOWNSHOOTER_<MODULE>_CONSTANTS__` (예: `_TOPDOWNSHOOTER_ENTITY_CONSTANTS__`) — `apps/_MyApp_/src/Playable/Constants.h` 의 `_TOPDOWNSHOOTER_PLAYABLE_CONSTANTS__` 양식 일치. `#pragma once` 미사용 (프로젝트 규칙).
- **이름**: `SCREAMING_SNAKE` + 도메인 prefix — `PLAYER_HP`, `ENEMY_SPEED`, `BULLET_RADIUS`, `HAND_SPREAD_DEG`, `WAVE_SPAWN_INTERVAL`. (`apps/_MyApp_/src/Playable/Constants.h` 의 `PLAYER_FRONT_IDLE` 스타일 계승.)
- **타입**: 스칼라(int/float) = `constexpr`. 비리터럴 타입(`vmath::vec4` 색·`vmath::vec2` 크기) = `const` (vmath 생성자가 constexpr 아님).
- **구획**: 큰 파일(Entity)은 내부 `// ── Player ──` / `// ── Hand ──` / `// ── Enemy ──` / `// ── Bullet ──` 주석 섹션.

### A1 부착 패턴 (D5)
```cpp
// apps/_MyApp_/src/Entity/Constants.h  — 단일 소스
namespace TopdownShooter::Entity {
    constexpr int   PLAYER_HP         = 100;
    constexpr float PLAYER_MOVE_SPEED = 3.0f;   // C1: 실값(3.0)으로 단일화
}

// apps/_MyApp_/src/Entity/Player/PlayerActor.h  — struct 기본값이 상수 참조
struct LifeCfg     { int   hp    = TopdownShooter::Entity::PLAYER_HP; };
struct MovementCfg { float speed = TopdownShooter::Entity::PLAYER_MOVE_SPEED; };

// apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp  — 중복 override 삭제
//   (구) pac.movement.speed = 3.0f;   ← 제거 (struct 기본값이 이미 PLAYER_MOVE_SPEED)
//   값이 기본과 다른 인스턴스에서만 명시 대입 유지
```

---

## 5. 모듈별 배치 맵

배치 규칙: **그 값을 소유한 Config struct / Component 의 top-level 모듈** 에 둔다 (현재 작성 위치 무관 — A1 의 단일 소스 원칙).

> ⚠ 아래 현재값은 인벤토리(부록 A) 기준. **실행(plan) 단계에서 각 현재 리터럴을 재확인**하여 정확값을 상수로 박는다 (인벤토리는 "맵", 실값은 코드가 정본).

### 5.1 `apps/_MyApp_/src/Entity/Constants.h` (신설) — `namespace TopdownShooter::Entity`
소유: `PlayerActorConfig`(apps/_MyApp_/src/Entity/Player/PlayerActor.h), `PlayerHands`(apps/_MyApp_/src/Entity/Player/PlayerHand.h), `EnemyConfig`(apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h), `BulletConfig`(apps/_MyApp_/src/Entity/Bullet/bullet_factory.h), `WeaponComponents.cpp`.

| 구획 | 상수 | 현재값 | 출처 |
|---|---|---|---|
| Player | `PLAYER_HP` | 100 | PlayerActorConfig::LifeCfg / PlayerBuilder |
| | `PLAYER_MOVE_SPEED` | 3.0 | **C1 divergence 해소** |
| | `PLAYER_LINEAR_DAMPING` | 5.0 | PhysicsCfg / PlayerBuilder |
| | `PLAYER_WEAPON_DAMAGE` | 10 | WeaponCfg / PlayerBuilder |
| | `PLAYER_SPRITE_FPS` | 8.0 | **C2: SpriteCfg::fps + PlayerBuilder kFps 통합** |
| Hand | `HAND_SPREAD_DEG` | 25.0 | PlayerHand.h kSpreadDeg |
| | `HAND_RADIUS` | 0.6 | kRadius |
| | `HAND_Y_OFFSET` | 0.0 | kYOffset |
| | `HAND_SCALE` | 0.25 | kHandScale |
| | `HAND_QUEUE_OFFSET` | 10 | kHandQueueOffset |
| Enemy | `ENEMY_HP` | 30 | EnemyConfig / EnemyDeps |
| | `ENEMY_SPEED` | 2.0 | EnemyConfig / EnemyDeps |
| | `ENEMY_DAMAGE` | 10 | EnemyConfig / EnemyDeps |
| | `ENEMY_LINEAR_DAMPING` | 0.5 | EnemyFactory.h inline |
| | `ENEMY_FRICTION` | 0.3 | EnemyFactory.h inline |
| | `ENEMY_RADIUS` | 0.4 | EnemyFactory.h inline |
| | `ENEMY_SPRITE_FPS` | 6.0 | EnemyDeps |
| Bullet | `BULLET_SPEED` | 15.0 | BulletConfig / WeaponComponents |
| | `BULLET_DAMAGE` | 10 | BulletConfig |
| | `BULLET_LIFETIME` | 3.0 | BulletConfig / WeaponComponents |
| | `BULLET_RADIUS` | 0.15 | **C5: CircleBody + Sphere mesh 통합** |

### 5.2 `apps/_MyApp_/src/Stage/Constants.h` (신설) — `namespace TopdownShooter::Stage`
소유: `WaveController`(Stage/WaveController.{h,cpp}).

| 상수 | 현재값 | 출처 |
|---|---|---|
| `WAVE_SPAWN_INTERVAL` | 3.0 | WaveController.h kSpawnInterval |
| `WAVE_MAX_ENEMIES` | 5 | kMaxEnemies |
| `WAVE_HP_BASE` | 20 | .cpp `20 + wave*5` |
| `WAVE_HP_PER_WAVE` | 5 | .cpp |
| `WAVE_SPEED_BASE` | 1.5 | .cpp `1.5 + wave*0.3` |
| `WAVE_SPEED_PER_WAVE` | 0.3 | .cpp |
| `WAVE_CONTACT_DAMAGE` | 10 | .cpp d.damage |

### 5.3 `apps/_MyApp_/src/Physics/Constants.h` (신설) — `namespace TopdownShooter::Physics`
소유: `Impulse`(apps/_MyApp_/src/Physics/PhysicsImpulse.h). **`PhysicsLayer.h` 는 별도 유지** (구조적).

| 상수 | 현재값 | 출처 |
|---|---|---|
| `IMPULSE_FORCE` | 2.5 | mImpulseForce base |
| `IMPULSE_COOLDOWN` | 0.8 | mCooldown base |
| `IMPULSE_DURATION` | 0.3 | **C4: kDurationSec + mActiveTimer base 통합** |

### 5.4 `apps/_MyApp_/src/Playable/Constants.h` (**기존 확장**) — `namespace TopdownShooter::Playable`
기존 `PLAYER_FACING_THRESHOLD`, `PLAYER_*_IDLE/MOVE`, `ENEMY_FRONT[3]` 유지. 추가:

| 상수 | 현재값 | 출처 |
|---|---|---|
| `SPRITE_HIT_FLASH_DURATION` | 0.18 | SpriteFxPlayable.h |
| `SPRITE_DISSOLVE_DURATION` | 1.0 | SpriteFxPlayable.h |
| `VIGNETTE_PEAK` | 0.45 | PlayerBuilder hit tween |
| `VIGNETTE_DURATION_MS` | 300 | PlayerBuilder hit tween |

### 5.5 `apps/_MyApp_/src/HUD/Constants.h` (신설) — `namespace TopdownShooter::HUD`
소유: `HealthBarConfig`(apps/_MyApp_/src/HUD/HealthBarFactory.h).

| 상수 | 현재값 | 출처 |
|---|---|---|
| `HEALTHBAR_FILL_COLOR` | (0.13,1.0,0,1.0) | **C3: 단일 소스 (Bootstrap 이 참조)** |
| `HEALTHBAR_BG_COLOR` | (0,0,0,0.55) | HealthBarFactory.h |
| `HEALTHBAR_SEGMENT_COUNT` | 5 | segmentCount |
| `HEALTHBAR_SEGMENT_SPACING` | 0.08 | segmentSpacing |
| `HEALTHBAR_HEAD_OFFSET` | 0.5 | headOffset |
| `HEALTHBAR_SIZE` | (1.2,0.18) | size |

### 5.6 `apps/_MyApp_/src/Bootstrap/Constants.h` (신설) — `namespace TopdownShooter::Bootstrap`
소유: `EntityPresentationConfig`(apps/_MyApp_/src/Bootstrap/EntityPresentation.h), `EnemyBuilder.cpp` 로컬 트윈.

| 상수 | 현재값 | 출처 |
|---|---|---|
| `PLAYER_DISSOLVE_SECONDS` | 1.5 | EntityPresentation 기본 |
| `PLAYER_DEATH_DELAY` | 1.5 | EntityPresentation 기본 |
| `ENEMY_DISSOLVE_SECONDS` | 0.6 | EnemyBuilder override |
| `ENEMY_DEATH_DELAY` | 0.6 | EnemyBuilder override |
| `ENEMY_SCALE_PULSE_MS` | 400 | EnemyBuilder scaleTween |
| `ENEMY_ROT_WOBBLE_MS` | 2000 | EnemyBuilder rotTween |

### 5.7 InputHandler — **예외 (Constants.h 안 만듦)**
`CameraController.cpp` 의 pitch clamp `89.0f` 단 1값. 별도 Constants.h 대신 **`CameraController.cpp` 파일상단 익명 ns 에 `constexpr float kCameraPitchClampDeg = 89.0f;`** 로 명명만 한다 (1값에 모듈 헤더 신설은 과함 — 사용자 승인).

---

## 6. 중앙화가 드러낸 정리거리 (cleanup)

이 리팩토링의 실이득. 단순 이동이 아니라 아래 불일치/중복을 **해소**한다.

| # | 불일치/중복 | 해소 |
|---|---|---|
| C1 | `PlayerActorConfig` speed 기본 `5.0` ↔ Builder `3.0` | 실값 `3.0` → `PLAYER_MOVE_SPEED`. struct 기본값이 상수 참조, Builder override 제거 |
| C2 | `PlayerBuilder.cpp kFps=8.0` ↔ `SpriteCfg::fps=8.0` 중복 | 단일 `PLAYER_SPRITE_FPS` |
| C3 | `EntityPresentation::healthBarColor` ↔ `HealthBarFactory::fillColor` 동일 RGBA | 단일 `HUD::HEALTHBAR_FILL_COLOR` (Bootstrap→HUD cross-module 참조) |
| C4 | `PhysicsImpulse kDurationSec=0.3` ↔ `mActiveTimer` base 0.3 | 단일 `IMPULSE_DURATION` |
| C5 | bullet radius `0.15` 물리(CircleBody) + 시각(Sphere mesh) 따로 | 단일 `BULLET_RADIUS` (히트박스·비주얼 동기 보장) |

**병합하지 않는 것**: `damage=10` 이 4곳(`PLAYER_WEAPON_DAMAGE`/`ENEMY_DAMAGE`/`BULLET_DAMAGE`/`WAVE_CONTACT_DAMAGE`)에 같은 값으로 존재하나 **개념이 다르므로 별도 상수 4개 유지**. (값이 같다고 합치면 밸런싱 시 한쪽만 바꾸려다 다 바뀜 — 안티패턴.)

---

## 7. 마이그레이션 & 커밋 전략

### 단위
1모듈 = (`Constants.h` 작성 → 참조처 수정 → **빌드 GREEN** → 1커밋). 빌드 GREEN 게이트를 모듈마다 통과.

### 순서 (저의존 → 고의존)
1. **Physics** (`Constants.h` 신설, PhysicsImpulse.h 참조) — 3 상수, 독립적
2. **HUD** (HealthBarFactory.h 참조) — 6 상수
3. **Stage** (WaveController.{h,cpp} 참조) — 7 상수
4. **Playable** (기존 Constants.h 확장, SpriteFxPlayable.h + PlayerBuilder hit tween 참조) — 4 상수
5. **Bootstrap** (EntityPresentation.h + EnemyBuilder.cpp 참조, C3 는 HUD 의존) — 6 상수
6. **Entity** (최대·최후 — PlayerActor.h struct 기본값 + EnemyFactory.h + bullet_factory.h + WeaponComponents.cpp + PlayerHand.h + PlayerBuilder override 삭제) — ~21 상수
7. **InputHandler** (CameraController.cpp 파일상단 constexpr) — 곁다리, 아무 때나

### 커밋 가드레일 (메모리 준수)
- **`git commit <경로>` path-scoped** (사용자 병렬 git 작업 보호 — 인덱스 전체 커밋 금지).
- ⚠ **`PlayerBuilder.cpp` 는 step 4(Playable VIGNETTE 추출) + step 6(Entity override 삭제·상수 참조) 양쪽에서 수정** → 사용자의 미커밋 `"shoot"` WIP(`FindSound("shoot") //!`)와 두 단계 모두 얽힘. 두 단계 커밋 직전 사용자와 상의 (DeadCode 제거 미커밋 분 + `"shoot"` WIP 와 겹침 — 함께 정리 가능). 가능하면 step 4·6 의 PlayerBuilder.cpp 변경을 사용자 WIP 정리 후 진행.
- 빌드는 사용자가 직접 / 또는 오케스트레이터 재검증. **no_auto_tests** (메모리) — 단위 테스트 자발 추가 금지.
- 엔진 코어/셰이더/`src/Text/` 미접근.

---

## 8. 실행 전 재확인 항목 (plan 단계 todo)

인벤토리 에이전트가 추론한 값 — plan/실행 시 코드로 정본 확인:
- `apps/_MyApp_/src/Bootstrap/EntityPresentation.h` 존재 + dissolve/delay/healthBarColor 기본값 (1.5/1.5/색).
- `apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h` `EnemyConfig` 기본값(hp=30·speed=2.0·damage=10) + factory inline(damping=0.5·friction=0.3·radius=0.4).
- `apps/_MyApp_/src/Entity/Bullet/bullet_factory.h` `BulletConfig` 기본값(speed=15·damage=10·lifetime=3) + radius=0.15 2곳.
- `apps/_MyApp_/src/HUD/HealthBarFactory.h` `HealthBarConfig` 6 기본값.
- `WaveController.cpp` wave 곡선 계수(20/5, 1.5/0.3).
- C1 divergence(struct 5.0 vs Builder 3.0) 실재 확인 후 3.0 채택.
- **각 모듈 실제 네임스페이스 확인** — `HUD`/`Bootstrap`/`Stage` 등이 본 spec 가정(`TopdownShooter::HUD` 등)과 일치하는지 헤더로 확인 (불일치 시 그 모듈 네임스페이스 채택).
- **cross-module include 가능 확인** — Constants.h 는 header-only `constexpr` 라 CMake link 변경 불필요. `#include "apps/_MyApp_/src/HUD/Constants.h"` 가 src 루트 include base 로 해소되는지(Bootstrap→HUD 참조 C3)만 확인. vmath 외 의존 없음 → include cycle 없음.

---

## 부록 A — 사전 인벤토리 요약

총 ~60 knob / 13 모듈. 분류:
- **B1 (named constexpr/static const)** ~30: PlayerHand, WaveController, PhysicsImpulse, Playable/Constants, HealthBar 기본.
- **B2 (ctor default-arg)** ~20: PlayerActorConfig, EnemyConfig, BulletConfig, HealthBarConfig, EnemyDeps.
- **B3 (inline literal)** ~10: WeaponComponents, EnemyBuilder tween, PlayerBuilder kFps, EntityPresentation override.
- Config-set ~45 / scattered ~15.

튜닝값 실보유 모듈: Stage(5)·PlayerBuilder(5)·EnemyBuilder(9)·PlayerHand(5)·HealthBar(6)·bullet_factory(5)·EnemyFactory(6)·PhysicsImpulse(3)·EntityPresentation(3)·SpriteFxPlayable(2)·CameraController(1)·PostFXTween(1)·Playable/Constants(1).
튜닝값 0 모듈: PhysicsLayer(구조적)·HpGrayscalePostFX(동적)·SpriteLayerFactory·EffekseerPlayable·FmodPlayable·PlayerController(입력)·Spawns·BaseEntity·Algebraic·UI.
