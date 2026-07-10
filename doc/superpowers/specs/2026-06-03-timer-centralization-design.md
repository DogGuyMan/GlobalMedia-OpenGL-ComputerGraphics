# Timer 통일 — 중앙화(BaseEntity MultipleTimer) + Timer 객체화

- 날짜: 2026-06-03
- 대상: `apps/_MyApp_` (탑다운 슈터)
- 상태: 설계 승인됨 (사용자 2026-06-03)

## 1. 배경 / 문제

게임플레이 시간 로직이 컴포넌트마다 흩어져 있다.

- `Entity::Components::Life` — `std::optional<SJH::Timer::Timer> mInvincibleTimer, mDieTimer` 직접 보유 + 자기 `Update`에서 Tick.
- `Physics::Impulse` — `SJH::Timer::Timer mActiveTimer, mCooldownTimer` 직접 보유 + 자기 `Update`에서 Tick.
- `Controller::PlayerController` — `float mAttackTimer` (raw float 카운트다운) — `SJH::Timer::Timer` 미사용 분산 산술.
- `Stage::WaveController` — `float mSpawnTimer` (raw float 카운트업) — `SJH::Timer::Timer` 미사용 분산 산술.

신설된 중앙 컨테이너 [`src/timer/multiple_timer.h`](../../../src/timer/multiple_timer.h)
(`SJH::Timer::MultipleTimer` — `Register`(안정 `Timer*` 반환)/`Find`/`Unregister` + 일괄 tick `Update`)
가 아직 어디서도 쓰이지 않는다.

## 2. 목표

엔티티에 속한 timer는 **한 곳(BaseEntity)** 이 보유·구동하게 중앙화하고,
엔티티가 아닌 시스템의 raw-float timer는 **`SJH::Timer::Timer` 객체로 통일**해 분산 산술을 제거한다.

### 사용자 확정 사항
1. 엔티티 컴포넌트(`Life`/`Impulse`)는 Timer를 직접 보유하지 않는다 — `BaseEntity`만 `MultipleTimer`를 보유.
2. Timer의 Update(tick)는 `BaseEntity` 자기 자신이 구동한다 (Template-Method skeleton).
3. `PlayerController::mAttackTimer`는 `PlayerEntity`(= BaseEntity)에 속하는 timer이므로 **중앙화 대상**.
4. `WaveController::mSpawnTimer`는 엔티티가 아니므로 중앙화 대상이 **아니다** — `SJH::Timer::Timer` 객체를 **직접 보유**해 동작하게 리팩토링 (모든 timer 기능을 `Timer::Timer` 객체로 통일, 분산 산술 제거).

### 비목표 (YAGNI)
- `src/timer/{timer,multiple_timer}.h` 코어 수정 — "사용"만 한다.
- `BulletLifetime` 등 사용자가 지명하지 않은 timer류는 손대지 않는다.
- 단위 테스트 신규 작성 (요청 시에만).

## 3. 아키텍처 — 두 패턴

작업 대상이 성격에 따라 둘로 갈린다.

### 패턴 E — 엔티티 timer 중앙화
`BaseEntity`가 `MultipleTimer`를 **합성 멤버**로 단일 보유(Actor 컴포넌트로 등록하지 않음 —
`mOwner`=null, `OnEnter/OnExit` no-op override 그대로 유효). `BaseEntity::Update(dt)`가
유일한 tick 구동자.

소비 컴포넌트(Life/Impulse/PlayerController)는:
- `OnEnter`: 같은 Actor의 `GetComponent<BaseEntity>()->Timers()`에 자기 timer를 `Register`하고
  반환된 `Timer*` 핸들만 캐시(비소유). 필요 시 직후 arm-inactive(`Tick(base)`로 finished 시작).
- 런타임: `Timer*` 경유로 `Reset`/`IsTimesUp`/`GetProgress` 등 호출(Tick은 하지 않음).
- `OnExit`: `Unregister`(키) + 핸들 null (재부착 시 중복키 assert 방지, 대칭).

> 생명주기 안전성: `MultipleTimer`는 `BaseEntity`의 값 멤버라, 빌더의 `AddComponent` 시점에
> 이미 생성된다. Actor scene 진입 시 모든 컴포넌트는 map에 존재하므로 어떤 형제의 `OnEnter`에서도
> `GetComponent<BaseEntity>()`가 성공한다 (OnEnter 순서 무관). `PlayerEntity`/`EnemyEntity`는
> `BaseEntity` 파생이라 `GetComponent<BaseEntity>()`가 slow-path `dynamic_cast`로 매칭된다.

### 패턴 S — 시스템 timer 객체화
`WaveController`는 엔티티가 아닌 stage 시스템(BaseEntity 없음)이므로 `SJH::Timer::Timer`를
**직접 멤버로 보유**하고 자기 `Update`에서 tick한다. raw-float 산술을 Timer API로 치환한다.

## 4. 키 네이밍 (패턴 E)

키는 per-actor `MultipleTimer` 내부에서만 유효(엔티티마다 독립 컨테이너 — 전역 충돌 없음).

| 발급 컴포넌트 | 키 | base time 소스 | 등록 조건 |
|---|---|---|---|
| `Life` | `"life.iframe"` | i-frame 초 | 초 > 0 일 때만 |
| `Life` | `"life.die"` | 사망지연 초 | 초 > 0 일 때만 |
| `Impulse` | `"impulse.active"` | `IMPULSE_DURATION` (0.3) | 항상 |
| `Impulse` | `"impulse.cooldown"` | `mCooldown.GetValue()` (0.8) | 항상 |
| `PlayerController` | `"player.attack"` | `mAttackWindowSec` (0.15) | 항상 (Player만 부착) |

"없음(미설정)" 의미는 **등록 안 함** = `Timer*` nullptr 로 표현(기존 `optional` null-guard와 1:1).

## 5. 컴포넌트별 변경 명세

### 5.1 `apps/_MyApp_/src/Entity/BaseEntity.h`
- `#include "timer/multiple_timer.h"` 추가.
- 멤버 `SJH::Timer::MultipleTimer mTimers;` 추가(유일 보유).
- `Update(float dt)` override 본문: `mTimers.Update(dt);` (기존 빈 본문 → tick 구동).
- accessor `SJH::Timer::MultipleTimer& Timers() { return mTimers; }` 추가(형제 위탁 창구).
- `BaseEntity.cpp` 불변(Update가 헤더 인라인이면 cpp 무관).

### 5.2 `apps/_MyApp_/src/Entity/Components/LifeComponents.h`
- `#include "apps/_MyApp_/src/Entity/BaseEntity.h"` 추가, `#include <optional>` 제거 가능.
- 멤버 교체: `std::optional<Timer> mInvincibleTimer, mDieTimer`
  → `SJH::Timer::Timer* mInvincible = nullptr; SJH::Timer::Timer* mDie = nullptr;` (비소유 핸들).
- 설정값은 빌더 단계(OnEnter 전)에 들어오므로 **float로 보관**:
  `float mIFrameSeconds = 0.0f; float mDieDelaySeconds = 0.0f;`
- `SetIFrameSeconds(s)` / `SetDeathDelaySeconds(s)` / ctor(iframe) → 위 float만 세팅
  (기존 `ArmInactive(slot, s)` 헬퍼의 "arm-inactive" 의미는 OnEnter 등록 시점으로 이전).
- `OnEnter`: 기존 sink 캐시 + `auto* be = GetOwner()->GetComponent<BaseEntity>();`
  - `if (be && mIFrameSeconds > 0) { mInvincible = be->Timers().Register("life.iframe", mIFrameSeconds); mInvincible->Tick(mInvincible->GetBaseTime()); }`
  - `if (be && mDieDelaySeconds > 0) { mDie = be->Timers().Register("life.die", mDieDelaySeconds); mDie->Tick(mDie->GetBaseTime()); }`
- `OnExit`: `if (auto* be = GetOwner()->GetComponent<BaseEntity>()) { be->Timers().Unregister("life.iframe"); be->Timers().Unregister("life.die"); } mInvincible = mDie = nullptr;`
- `Update`: timer **Tick 제거**. 사망지연 만료 검사는 `if (mDie->IsTimesUp())` 로(BaseEntity가 tick). death 안전망 유지.
- `IsInvincible() = mInvincible && !mInvincible->IsTimesUp()`.
- `DoDamaged`: `if (mInvincible) mInvincible->Reset();` (없으면 no-op = 무적 없음).
- `DoDie`: `if (mDie) mDie->Reset(); else if (GetOwner()) GetOwner()->SetActive(false);`.

### 5.3 `apps/_MyApp_/src/Physics/PhysicsImpulse.h`
- `#include "apps/_MyApp_/src/Entity/BaseEntity.h"` 추가.
- 멤버 교체: `Timer mActiveTimer, mCooldownTimer`
  → `SJH::Timer::Timer* mActive = nullptr; SJH::Timer::Timer* mCool = nullptr;` (비소유).
  `Stat mImpulseForce, mCooldown`은 유지(쿨다운 base 소스).
- ctor: timer 멤버 초기화/arm-inactive Tick 제거(멤버 자체가 사라짐). Stat 초기화만 남김.
- `OnEnter`: 기존 `mBody = FindPhysics(GetOwner())` + BaseEntity 캐시 →
  `mActive = be->Timers().Register("impulse.active", IMPULSE_DURATION); mActive->Tick(base);`
  `mCool = be->Timers().Register("impulse.cooldown", mCooldown.GetValue()); mCool->Tick(base);`
- `OnExit`: `mBody = nullptr` + `Unregister("impulse.active"/"impulse.cooldown")` + 핸들 null.
- `Update`: Tick 본문 제거 → 빈 override.
- `DoImpulse`: 게이트/리셋을 `mCool`/`mActive` 핸들 경유로(`if (!mCool || !mCool->IsTimesUp() || IsActive()) return; … mActive->Reset(); mCool->Reset();`).
- `IsActive() = mActive && !mActive->IsTimesUp();`

### 5.4 `src/InputHandler/PlayerController.{h,cpp}`
- 멤버: `float mAttackTimer` 제거 → `SJH::Timer::Timer* mAttack = nullptr;`.
  `float mAttackWindowSec = 0.15f`은 Register 인자로 유지.
- `OnEnter`: `mIsInitialized` 체크 이후 `mEntity = GetOwner()->GetComponent<BaseEntity>();`
  + `if (mEntity) { mAttack = mEntity->Timers().Register("player.attack", mAttackWindowSec); mAttack->Tick(mAttack->GetBaseTime()); }`
  (Update의 lazy `mEntity` 캐시는 안전망으로 유지 — 무해).
- `OnExit`: `if (auto* be = GetOwner()->GetComponent<BaseEntity>()) be->Timers().Unregister("player.attack"); mAttack = nullptr;` (기존 UnregisterBindings 등은 유지).
- `Update`: `if (mAttackTimer > 0) mAttackTimer -= dt; attacking = (mAttackTimer > 0)`
  → `const bool attacking = (mAttack && !mAttack->IsTimesUp());` (Tick은 BaseEntity가).
- `OnFirePressed`: `mAttackTimer = mAttackWindowSec;` → `if (mAttack) mAttack->Reset();`.
- `timer/multiple_timer.h`는 `apps/_MyApp_/src/Entity/BaseEntity.h` 경유로 전파(필요 시 명시 include).

### 5.5 `src/Stage/WaveController.{h,cpp}` (패턴 S)
- `#include "timer/timer.h"` 추가.
- 멤버: `float mSpawnTimer = 0.0f` → `SJH::Timer::Timer mSpawnTimer;`.
- ctor 초기화 리스트에 `mSpawnTimer(WAVE_SPAWN_INTERVAL)` 추가(`WAVE_SPAWN_INTERVAL`은 .cpp의 `apps/_MyApp_/src/Stage/Constants.h`에서 가용).
- `Update`:
  - `mSpawnTimer = 0.0f` (웨이브 개시/전멸 리셋 2곳) → `mSpawnTimer.Reset();`
  - `mSpawnTimer += dt;` → `mSpawnTimer.Tick(dt);`
  - `if (mSpawnTimer >= WAVE_SPAWN_INTERVAL && LiveCount() < WAVE_MAX_ENEMIES) { mSpawnTimer = 0.0f; SpawnEnemy(); }`
    → `if (mSpawnTimer.IsTimesUp() && LiveCount() < WAVE_MAX_ENEMIES) { mSpawnTimer.Reset(); SpawnEnemy(); }`

## 6. 동작 동등성 / 회귀 위험

- **read/tick 분리(패턴 E)**: 엔티티 timer의 tick은 `BaseEntity::Update`, read는 소비 컴포넌트
  Update/이벤트에서 일어난다. Actor의 컴포넌트 순회 순서는 비결정적(`unordered_map`)이라
  사망지연 비활성화·attack 윈도 경계가 **최대 1프레임** 흔들릴 수 있다. i-frame/cooldown 게이트는
  self-correcting, die(0.5s)·attack(0.15s≈9프레임)은 무해.
- **arm-inactive 불변**: 등록 직후 `Tick(base)`로 finished 시작 → 평소 비활성, `Reset`으로 발동
  (기존과 동일 패턴). BaseEntity가 매 프레임 모든 timer를 tick해도 finished는 [0,base] clamp로 무해.
- **WaveController clamp(패턴 S)**: `Timer`는 passed를 [0,base] clamp → 용량(MAX) 대기 중에도
  `IsTimesUp` 유지 → 해제 즉시 1회 spawn+reset. 기존 float 누적과 관측 결과 동일.
- **재부착**: `OnExit`의 `Unregister`로 중복키 assert 방지(엔티티 timer).

## 7. 빌드 / 의존성

- `BaseEntity.h`가 `timer/multiple_timer.h`(값 멤버)를 include. `LifeComponents.h`/`PhysicsImpulse.h`가
  `apps/_MyApp_/src/Entity/BaseEntity.h`를 include. `BaseEntity.h`는 `Life`/`Impulse`를 **전방 선언만** 하므로 순환 없음.
- `_MyApp_`는 `SJH::engine` 우산(= `SJH::timer` 포함)을 link → 헤더 가용.
- 검증: `cmake --build --preset ninja --target _MyApp_` GREEN.

## 8. 가드레일

- `src/timer/*` 코어 미수정. 빌더에 `MultipleTimer` AddComponent 안 함.
- `EnemyBuilder.cpp`(IDE 오픈)·셰이더·`src/Text/` 미접근.
- 커밋은 `git commit <경로>` partial(사용자 병렬 staging 보호). 빌드는 사용자가 직접 또는 명시 요청 시.
- `no_auto_tests` — 단위 테스트는 요청 시에만.

## 9. 터치 파일 (7)

1. `apps/_MyApp_/src/Entity/BaseEntity.h`
2. `apps/_MyApp_/src/Entity/Components/LifeComponents.h`
3. `apps/_MyApp_/src/Physics/PhysicsImpulse.h`
4. `apps/_MyApp_/src/InputHandler/PlayerController.h`
5. `apps/_MyApp_/src/InputHandler/PlayerController.cpp`
6. `apps/_MyApp_/src/Stage/WaveController.h`
7. `apps/_MyApp_/src/Stage/WaveController.cpp`
