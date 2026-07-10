# Client 의존 사이클 C1~C4 해소 (DIP) — 구현 Plan

> **상태: Task 1~4 구현 완료(C4 커밋 `f98fc81` · C3/C1/C2 빌드 GREEN·미커밋) · Task 5(검증)+커밋 잔여.** 본 문서는 *선택지 + 결정 + 진행*을 보존한다 — 결정은 번복 가능하니 채택안과 대안을 함께 남긴다. **다음 진입점 = 아래 §0.5 진행 상태.**
> 상위 인덱스: `doc/superpowers/plans/2026-06-11-dependency-cycle-master-roadmap.md` (후보 ①②③④ 전체). 짝 문서: 엔진 spec = `doc/superpowers/specs/2026-06-11-engine-dependency-cycle-refactor-design.md` (D1~D8) · 엔진 plan = `doc/superpowers/plans/2026-06-11-engine-cycle-e1-e6-plan.md` · 사이클 전수조사 = `doc/handoffs/2026-06-11/2026-06-11-dependency-cycle-refactor-handoff.md`.
> 모든 `file:line` 은 **2026-06-11 라이브 코드 기준 grounded** — 착수 시 drift 재확인.

---

## 0. TL;DR

탑다운 슈터 클라이언트의 **4개 양방향 사이클**(`Entity ↔ Physics/InputHandler/Spawns/Playable`)을 **DIP(인터페이스 추출)** 로 끊는다. 인터페이스 시임(`Components.Interfaces.h`)을 `Entity/` 밖 의존-제로 위치(`Contracts/`)로 옮기고, 남는 구상 레그(`BaseEntity::Timers/IsImpulseActive`, `PlayerEntity::Dash/UseWeapon`, `PlayerHands::TriggerFire`, 구상 `Life`)를 인터페이스 경유로 전환한다. **목표 = 4상 mutual=0.**

---

## 0.5 진행 상태 (핸드오프 진입점 — 2026-06-11)

**어디까지 됐나:** **Task 1~4 구현 완료.** Task 1(시임 이동) **커밋 `f98fc81`**. Task 2(C3)·3(C1)·4(C2)는 **빌드 GREEN·미커밋** (working tree 6파일 변경: `apps/_MyApp_/src/Entity/Player/PlayerEntity.h`·`apps/_MyApp_/src/Entity/Player/PlayerHand.h`·`InputHandler/PlayerController.{h,cpp}`·`apps/_MyApp_/src/Physics/PhysicsImpulse.h`·`apps/_MyApp_/src/Spawns/Carrier.h`). 절단 증명: `grep '#include "Entity/"' {Physics,InputHandler,Spawns,Playable}` = const-noise(`UltimateLaser.cpp`→`apps/_MyApp_/src/Entity/Constants.h`, §6) 외 **0**.
**다음 행동:** **Task 5 (통합검증 + GUI 육안)** + 사용자 path-scoped 커밋(6파일). Task 2~4가 단일 의존-역전 묶음이라 한 커밋으로 묶어도 무방.

**Task 1 산출(받는 사람이 알아야 할 grounded 사실):**
- 신규 헤더 = `apps/_MyApp_/src/Contracts/EntityContracts.h` (가드 `_TOPDOWNSHOOTER_CONTRACTS_ENTITYCONTRACTS_H__`). **7 인터페이스 + EFacing/EPose/Quantize4 + 신규 4(`ITimerOwner`/`IImpulseState`/`IPlayerCommand`/`IFireTrigger`)** 전부 *선언* 됨.
- `BaseEntity` 이미 `: ... ITimerOwner, IImpulseState` 부착 + `override` 2개(`Timers()` 비-const, `IsImpulseActive()`). → **Task 3은 Physics 라우팅만**, BaseEntity 재부착 불요.
- **`IPlayerCommand`/`IFireTrigger`는 선언만 — `PlayerEntity`/`PlayerHands` 부착 + 라우팅은 Task 4.**
- 구 `apps/_MyApp_/src/Physics/Components.Interfaces.h` 삭제됨 (커밋 반영).

**⚠ 정정/함정 (plan 본문보다 우선):**
- **소비처 = 14파일 (include 3형태!)** — 본문의 "10 소비처"는 *과소집계*. 형태: ① 전체 `"apps/_MyApp_/src/Physics/Components.Interfaces.h"` ② 상대 `"apps/_MyApp_/src/Physics/Components.Interfaces.h"`(BaseEntity) ③ 동일디렉토리 `"Components.Interfaces.h"`(Life/Weapon/MovementComponents). 향후 grep은 **3형태 모두 + `apps/_MyApp_/src/Physics/Components.Interfaces.h`(별개 파일)는 제외**.
- **override 함정:** 신규 인터페이스를 *이미 같은 시그니처 메서드를 가진 클래스*에 부착하면 `-Werror=inconsistent-missing-override` 로 그 메서드에 `override` 필수 (Task 1의 BaseEntity 에서 실제 발생). → **Task 4에서 `PlayerEntity::Dash/UseWeapon` + `PlayerHands::TriggerFire` 부착 시 `override` 추가** 잊지 말 것. (const 오버로드는 인터페이스에 없으면 override 안 붙임 — 예: `Timers() const`.)
- CMake: 헤더온리라 소스목록 변경 불요 (검증됨). include 루트 = `src/`.

**검증 패턴(슬라이스마다):** `cmake --build --preset ninja --target _MyApp_` → 에러 0. 절단 grep: `grep -rn '#include "Entity/' apps/_MyApp_/src/{Physics,InputHandler,Spawns}` → const-noise(`apps/_MyApp_/src/Entity/Constants.h`) 외 0.
**가드레일:** 커밋 사용자 게이트(path-scoped) · 착수 직전 `git status`(Entity/ 병렬 편집) · `Co-Authored-By` 미사용.

---

## 1. Decision Log — 선택지 + 채택 (번복 대비 대안 보존)

> 형식: 채택안 **굵게**, 대안은 회수 가능하게 남김.

### D-1. 절단 범위 (어디까지 끊나)

| 옵션 | 내용 | 결과 |
|---|---|---|
| **D1-a ✅ 채택** | **완전 절단 (mutual=0)** | 4상 전부 끊음 |
| D1-b | 인터페이스 시임 + Life만 | C3·C4만 완전, C1·C2 thin 잔존 |
| D1-c | 헤더 이동만 | C4만 완전 |

**채택 = D1-a (완전 절단).** 근거: 엔진 spec 의 mutual=0 목표와 일관, 사이클을 *부분*만 끊으면 SCC 가 유지돼 컴파일 방화벽 효과가 안 남.
**번복 시:** 작업량을 줄이려면 D1-b(액션 인터페이스 생략)로 후퇴 가능 — C1·C2 가 thin mutual 로 남는 것을 수용.

### D-2. 인터페이스 시임 거처 + 네임스페이스

| 옵션 | 위치 | 네임스페이스 | 파급 |
|---|---|---|---|
| **D2-a ✅ 채택** | **신규 `apps/_MyApp_/src/Contracts/`** | **`TopdownShooter::Entity` 유지** | 소비처 10파일 `#include` 경로만 변경, `Entity::IMovable` 코드 무변경 |
| D2-b | `Contracts/` | 신규 `TopdownShooter::Contracts` | ~16파일 `Entity::I*`→`Contracts::I*` 전수 리네임 (churn 큼) |
| D2-c | `Entity/Interfaces/` 승격 | (무관) | Entity 하위라 사이클 안 끊김 — 목표 미달 |

**채택 = D2-a (Contracts/ + ns Entity 유지).** 근거: 목표는 *사이클 절단*이지 *네임스페이스 정리*가 아님 → 최소 표면. 엔진 D7(네임스페이스 불변, `SJH::Scene::MeshRenderer` 선례)과 일관. 병렬 편집 충돌 위험 최소.
**알려진 사이드 이펙트(수용):** 네임스페이스 `TopdownShooter::Entity` 가 `Entity/` + `Contracts/` 두 디렉토리에 걸침 (경미한 의미 불일치).
**번복 시:** 의미 일치를 원하면 D2-b — 단 활발히 편집 중인 클라 파일 다수를 건드리는 churn/충돌 비용 감수.

### D-3. C2(InputHandler) 플레이어 액션 레그 처리

> 정정 이력: 최초엔 "C2 = 인터페이스 2개면 끝"으로 과소 추정했으나, grounding 결과 InputHandler 가 구상 `PlayerEntity::Dash`(`PlayerController.cpp:144`)·`UseWeapon`(`:532`)·`PlayerHands::TriggerFire`(`:170`)를 직접 호출함을 확인 → 범위 정정.

| 옵션 | 내용 | 결과 |
|---|---|---|
| **D3-a ✅ 채택** | **C2도 액션 인터페이스화** (Dash/UseWeapon/TriggerFire) | 4상 전부 mutual=0 |
| D3-b | C1·C3·C4 완전 + C2 는 BaseEntity 레그만 | C2 가 액션 레그로 thin mutual 잔존 |
| D3-c | C4·C3만 | C1·C2 보류 |

**채택 = D3-a (C2 액션 인터페이스화 — 완전 4상 절단).**
**💭 설계상 트레이드오프 (code-design-review-lenses Lens④ I/D + design-decision-discipline):** 이 액션 인터페이스(`IPlayerCommand`/`IFireTrigger`)는 **단일 구현**(PlayerEntity/PlayerHands)뿐이라 다형성 이득이 없는 *순수 레이어링용 DIP* 다. 또한 대상이 **활발히 개발 중인 게임플레이 로직**(대시/발사)이라 변동성이 높다. "변동성≠다형성" 관점에선 신중할 지점 — 사용자가 *완전 절단*을 우선해 채택. **번복 시:** 게임플레이가 더 안정될 때까지 D3-b 로 후퇴(액션 레그 보류) 가능.

> **F3 (improve-codebase seam 렌즈, 2026-06-11 전수조사):** 신규 4 인터페이스(`ITimerOwner`/`IImpulseState`/`IPlayerCommand`/`IFireTrigger`)는 *single adapter* = **hypothetical seam** (스킬: 1 adapter=가설 seam, 2=real seam) → shallow(interface ≈ impl 복잡도). 반면 기존 7 인터페이스(`ILivable`/`IDamageable` 등 — *multi-impl* = real seam)는 deep. 완전절단(D3-a)은 *DAG·테스트성* 우선 결정이지 다형성 정당화가 아님 — 이 점 인지하고 진행. (대안 D3-b 는 이 shallow seam 4개를 안 만드는 대신 C1·C2 thin cycle 잔존.)

### D-4. 액션 인터페이스 분할 입도 (✅ 확정 2026-06-11: D4-a)

| 옵션 | 형태 |
|---|---|
| **D4-a (추천)** | `IPlayerCommand { Dash; UseWeapon; }` (PlayerEntity 1개 구현) + `IFireTrigger { TriggerFire; }` (PlayerHands 별도 클래스) |
| D4-b | ISP 엄격: `IDashable` / `IWeaponUser` / `IFireTrigger` 3분할 |

**✅ 확정 = D4-a** (2026-06-11 사용자 확정). Dash/UseWeapon 은 같은 클래스(PlayerEntity)가 구현하고 같은 소비처(InputHandler)가 호출 → 한 인터페이스 `IPlayerCommand` 로 묶음. TriggerFire 는 다른 클래스(PlayerHands)라 `IFireTrigger` 분리. **번복 시:** D4-b(ISP 3분할).

---

## 2. Before / After (사이클별)

> **절단 방향 (핵심):** 4사이클 모두 *X→Entity 레그*(Physics/InputHandler/Spawns/Playable 가 Entity 에 의존)만 끊는다. *Entity→X 레그*(Entity 가 자기 하위 컴포넌트를 소유·생성·조회 — 예: `PlayerActor.cpp:85 AddComponent<Controller::PlayerController>`, `PlayerHand.cpp:125 GetComponent<PlayerController>()`)는 **그대로 단방향 유지** = 정상 상위→하위 DAG 엣지. 2-노드 사이클은 한 방향만 끊으면 소멸하므로 mutual=0 달성에 충분.

| 사이클 | 현재 레그 (grounded) | 절단 후 |
|---|---|---|
| **C4 Entity↔Playable** | Playable→Entity: `IActorPresentation`/`ILivable` (순수 인터페이스, `HpGrayscalePostFX.h:24`/`PlayableDirector.h:28`) | 헤더 이동만 → **0** |
| **C3 Entity↔Spawns** | Spawns→Entity: `Carrier.h:23` 인터페이스 + `Carrier.h:24` 구상 `Life`(`:193,199` `Life>()->IsAlive()`) | Life→`ILivable` + LifeComponents.h include 제거 → **0** |
| **C1 Entity↔Physics** | Physics→Entity: `PhysicsMovement.h:27`/`PhysicsImpulse.h:28` 인터페이스 + `PhysicsImpulse.h:32` `BaseEntity::Timers()` + `:31` `PlayerEntity.h`(**죽은 include**) | `Timers()`→`ITimerOwner` + 죽은 include 제거 → **0** |
| **C2 Entity↔InputHandler** | InputHandler→Entity: 인터페이스 + `BaseEntity::Timers/IsImpulseActive` + 구상 `PlayerEntity::Dash`(`:144`)·`UseWeapon`(`:532`) + `PlayerHands::TriggerFire`(`:170`) | `ITimerOwner`/`IImpulseState` + 액션 인터페이스 → **0** |

---

## 3. 신규 인터페이스 (grounded 시그니처 기반)

모두 `apps/_MyApp_/src/Contracts/EntityContracts.h`, 네임스페이스 `TopdownShooter::Entity` (D2-a). `MultipleTimer` 는 전방선언으로 의존-제로 유지 (참조 반환은 전방선언으로 충분).

```cpp
namespace SJH::Timer { class MultipleTimer; }   // fwd — 의존-제로 유지

namespace TopdownShooter::Entity {
    // 기존 7개 (ILivable/IDieable/IDamageable/IAttackable/IMovable/IActorPresentation/IImpulsable)
    //   + EFacing/EPose + Quantize4  ← 그대로 이동

    /// C1/C2: 엔티티 중앙 타이머 컨테이너 접근 (BaseEntity 구현)
    class ITimerOwner {
      public: virtual ~ITimerOwner() = default;
        virtual SJH::Timer::MultipleTimer&       Timers()       = 0;
    };
    /// C2: 임펄스 활성 게이트 (BaseEntity::IsImpulseActive 구현)
    class IImpulseState {
      public: virtual ~IImpulseState() = default;
        virtual bool IsImpulseActive() const = 0;
    };
    /// C2 액션 (D4-a — PlayerEntity 구현)
    class IPlayerCommand {
      public: virtual ~IPlayerCommand() = default;
        virtual void Dash(vmath::vec2 dir)     = 0;   // PlayerEntity::Dash(:128)
        virtual void UseWeapon(vmath::vec2 aim) = 0;  // PlayerEntity::UseWeapon(:143)
    };
    /// C2 액션 (PlayerHands 구현)
    class IFireTrigger {
      public: virtual ~IFireTrigger() = default;
        virtual void TriggerFire() = 0;               // PlayerHands::TriggerFire(:121)
    };
}
```

> 구현체 부착: `BaseEntity : ... ITimerOwner, IImpulseState` / `PlayerEntity : ... IPlayerCommand` / `PlayerHands : ... IFireTrigger`. 기존 메서드 시그니처와 동일하므로 override 가 됨 (본문 무변경).
> 조회: 컴포넌트 시스템이 인터페이스 타입 조회를 지원(`GetComponent<IDamageable>()` 선례, `Carrier.h:80`) → `GetComponent<ITimerOwner>()` 등 동작.

---

## 4. 구현 Task (슬라이스별 — 각 슬라이스 끝에 `_MyApp_` 빌드 GREEN)

> 슬라이스 순서: 기반(시임 이동) → 잎 사이클부터. 각 슬라이스 독립 빌드 가능.

### Task 1 — 시임 이동 + 신규 인터페이스 (기반, C4 동시 해소) ✅ 완료 (커밋 `f98fc81`)

> 실제 결과: 소비처 **14파일**(3 include 형태, 본문 "10"은 과소 — §0.5 참조). BaseEntity 부착 시 `override` 2개 필요(함정). 신규 4 인터페이스 *선언* 완료, `IPlayerCommand`/`IFireTrigger` 부착은 Task 4. 빌드 GREEN + C4 grep 증명.
- **신규** `apps/_MyApp_/src/Contracts/EntityContracts.h` ← `apps/_MyApp_/src/Physics/Components.Interfaces.h` 내용 이동(7 인터페이스 + EFacing/EPose/Quantize4) + 신규 4 인터페이스(§3). 헤더가드 `__TOPDOWNSHOOTER_CONTRACTS_ENTITYCONTRACTS_H__`.
- **삭제** `apps/_MyApp_/src/Physics/Components.Interfaces.h` (또는 1줄 forwarding shim 후 Task 5에서 제거 — 택1).
- **include 경로 변경 (10 소비처):** `apps/_MyApp_/src/Entity/Player/PlayerEntity.h`, `apps/_MyApp_/src/HUD/HealthBarDriver.h`, `apps/_MyApp_/src/HUD/HealthBarFactory.cpp`, `apps/_MyApp_/src/InputHandler/PlayerController.h`, `apps/_MyApp_/src/Physics/PhysicsImpulse.h`, `apps/_MyApp_/src/Physics/PhysicsMovement.h`, `apps/_MyApp_/src/Playable/HpGrayscalePostFX.h`, `apps/_MyApp_/src/Playable/PlayableDirector.h`, `apps/_MyApp_/src/Spawns/Carrier.h`, `apps/_MyApp_/src/Spawns/UltimateLaser.cpp` → `#include "apps/_MyApp_/src/Contracts/EntityContracts.h"`.
- **CMake:** include 루트가 `apps/_MyApp_/src` 이므로 헤더-온리 이동은 등록 변경 **거의 불필요** (착수 시 `Entity/CMakeLists.txt`·`Stage/CMakeLists.txt` 의 `Components.Interfaces.h` 참조 2건 확인 — grep 에 잡힘).
- **수용기준:** 빌드 GREEN. **C4 사이클 해소** (Playable 이 더는 `Entity/` 를 include 안 함 — `HpGrayscalePostFX.h`/`PlayableDirector.h` 의 Entity include 가 Contracts 로 전환됨).

### Task 2 — C3: Spawns 구상 Life → ILivable ✅ 완료 (빌드 GREEN·미커밋)
- `apps/_MyApp_/src/Spawns/Carrier.h:193,199` `GetComponent<Entity::Components::Life>()->IsAlive()` → `GetComponent<Entity::ILivable>()->IsAlive()`.
- `apps/_MyApp_/src/Spawns/Carrier.h:24` `#include "apps/_MyApp_/src/Entity/Components/LifeComponents.h"` 제거.
- **수용기준:** 빌드 GREEN. **C3 해소** (Spawns 의 `Entity/` include 0 — 단, `apps/_MyApp_/src/Spawns/UltimateLaser.cpp:30` `apps/_MyApp_/src/Entity/Constants.h` 는 §6 out-of-scope).

### Task 3 — C1: Physics BaseEntity → ITimerOwner + 죽은 include ✅ 완료 (빌드 GREEN·미커밋)
> 실제 결과: `BaseEntity` 부착은 Task 1에서 이미 됨(재부착 불요). `PhysicsImpulse.h`에 **`#include "timer/multiple_timer.h"` 직접추가** — `apps/_MyApp_/src/Entity/BaseEntity.h` 제거로 `MultipleTimer` 완전형이 transitive 하게 사라지는 함정 보강(`Timers().Register/Unregister` 컴파일). `timer/timer.h`도 동반.
- `BaseEntity : ... ITimerOwner` 부착 (Timers() 가 override).
- `apps/_MyApp_/src/Physics/PhysicsImpulse.h:63,73` `GetComponent<Entity::BaseEntity>()->Timers()` → `GetComponent<Entity::ITimerOwner>()->Timers()`.
- `apps/_MyApp_/src/Physics/PhysicsImpulse.h:32` `#include "apps/_MyApp_/src/Entity/BaseEntity.h"` 제거, `:31` `#include "apps/_MyApp_/src/Entity/Player/PlayerEntity.h"` **제거(죽은 include)**.
- **수용기준:** 빌드 GREEN. **C1 해소** (Physics 의 `Entity/` include 0).

### Task 4 — C2: InputHandler BaseEntity + 액션 인터페이스화 ✅ 완료 (빌드 GREEN·미커밋)
> 실제 결과: `mEntity`(BaseEntity*)를 **`ITimerOwner* mTimerOwner` + `IImpulseState* mImpulseState` 두 인터페이스 포인터로 분리**(타이머 위탁 vs 임펄스 게이트 = 별개 관심사, 각자 캐시 지점 OnEnter/Update-lazy). `PlayerEntity:IPlayerCommand`·`PlayerHands:IFireTrigger` 부착 + `override` 3개(Dash/UseWeapon/TriggerFire). `PlayerHand.h`에 Contracts include 추가. `.cpp`에 `timer/timer.h`+`timer/multiple_timer.h` 직접추가(Task 3과 동일 함정 — `mAttackTimer->Tick` 완전형). PlayerController.h 중복 Contracts include 1개 + 죽은 fwd-decl 2개(BaseEntity/PlayerHands) 제거.
- `BaseEntity : ... IImpulseState` 부착 (IsImpulseActive override). `PlayerEntity : ... IPlayerCommand` (Dash/UseWeapon override). `PlayerHands : ... IFireTrigger` (TriggerFire override).
- `PlayerController.h:128` `BaseEntity* mEntity` → `IImpulseState* mImpulseState`(+ 타이머용 `ITimerOwner* mTimerOwner`). fwd-decl(`:40,41`) 조정.
- `PlayerController.cpp`: `:308,323` `mEntity->Timers()` → `mTimerOwner->Timers()`; `:351` `mEntity->IsImpulseActive()` → `mImpulseState->IsImpulseActive()`; `:144` `GetComponent<PlayerEntity>()->Dash` → `GetComponent<IPlayerCommand>()->Dash`; `:532` `UseWeapon` 동일; `:170` `GetComponent<PlayerHands>()->TriggerFire()` → `GetComponent<IFireTrigger>()`; cache 지점 `:305,346` 조정.
- include 제거: `:34` `PlayerEntity.h`, `:43` `BaseEntity.h`, `:44` `WeaponComponents.h`(미사용 확인 후), `:45` `PlayerHand.h`.
- **수용기준:** 빌드 GREEN. **C2 해소** (InputHandler 의 `Entity/` include 0, 단 §6 잔재 제외).

### Task 5 — 검증 + 정리
- 재-grep: `grep -rn '#include "Entity/' apps/_MyApp_/src/{Physics,InputHandler,Spawns,Playable}` → **const-noise 외 0** 확인 (§6).
- 시임 shim 잔재 제거. 빌드 GREEN + GUI 육안(게임플레이: 대시/발사/피격/사망 정상).
- (선택) `doxygen/pages/30-client-architecture.md` 의 ClientDeps 그래프에서 끊긴 mutual 4개 갱신.

---

## 5. 검증 방법

- 빌드: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_` → **error 0**. (사용자 직접 빌드 선호 — 수신자는 컴파일 검증만.)
- 무-FMOD 회귀 불필요(이 작업은 Audio 무관)이나 MSVC CI 는 그대로 통과해야 함.
- 사이클 절단 증명: 위 재-grep 0 (Task 5).
- 단위 테스트: **요청 시에만** (no_auto_tests).

## 6. Out of scope (의도적 — 노이즈/단방향)

- **const-only 잔재:** `apps/_MyApp_/src/Spawns/UltimateLaser.cpp:30` `#include "apps/_MyApp_/src/Entity/Constants.h"` (`ULTIMATE_*` 상수) — de-noising 규칙상 *노이즈*(const), 인스턴스 사이클 아님. 물리 include 는 남지만 사이클 판정엔 무해. 원하면 상수도 별도 이동(후속).
- **단방향 구상 Life 소비 (사이클 무관):** Bootstrap 3 + WaveController 3 곳이 구상 `Components::Life` 사용 — Entity→이들 역방향이 없어 사이클 아님. 건드리지 않음.
- **엔진 사이클(E1~E6):** 별도 spec(`...-engine-dependency-cycle-refactor-design.md`) — 본 plan 범위 아님.

## 7. Guardrails (가드레일)

- **구현 미착수** — 본 문서는 plan. 사용자 승인 후 슬라이스별 진행.
- 커밋: 사용자 path-scoped 게이트, `git add -A` 금지, **`Co-Authored-By` 미사용**. 빌드는 사용자가 직접.
- 주석 한국어 + Doxygen + ASCII/한글만. 명명 `mPascalCase`/`camelCase`/`*Ptr`/`mIs*`. 헤더가드 `__...__`(`#pragma once` 미사용).
- 병렬 편집 주의: 클라(`Entity/`·`Physics/`·`Spawns/`·`InputHandler/`·`Playable/`)는 사용자 활발 편집 영역 — 슬라이스 직전 `git status` 재확인.

## 8. Decision Log 요약

| ID | 결정 | 채택 | 대안(번복 시) |
|----|------|------|---------------|
| D-1 | 절단 범위 | **완전 절단 mutual=0** | D1-b 부분 / D1-c 헤더만 |
| D-2 | 시임 거처/ns | **Contracts/ + ns Entity 유지** | D2-b 새 ns |
| D-3 | C2 액션 레그 | **액션 인터페이스화 (완전 4상)** | D3-b 액션 보류 |
| D-4 | 액션 인터페이스 입도 | **D4-a 확정** (IPlayerCommand+IFireTrigger, 2026-06-11) | D4-b ISP 3분할 |
