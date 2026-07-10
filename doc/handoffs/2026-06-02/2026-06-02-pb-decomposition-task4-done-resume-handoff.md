# Handoff — PlayerBehavior 분해 *재개 컨텍스트* (2026-06-02, Task 4 완료 갱신)

> 🔴 **대체됨 (2026-06-02, HEAD 6adaf02)** → `doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-foundation-done-resume-handoff.md` 가 단일 진입점.
> 본 문서는 HEAD `d44a4a3` 시점이라 foundation "미커밋"·Task6 "미착수" 로 **stale**. 그 사이 foundation(`8f276a9`+`aa684f7`) 커밋 + Task6 directional working tree 구현됨. **§2(Task4 상세)·§4(잠긴결정)·§7(가드레일)·§8(문서인덱스) 깊이만 참조.**

> **(아래는 작성 당시 — d44a4a3 기준. 최신 상태는 위 신규 문서)**
> **이 문서 = 다음 세션의 단일 진입점.** PlayerBehavior god-component 분해(Task 0~7)를 subagent-driven 으로 실행 중. **Task 0~4 커밋 완료 / Task 5~7 미착수.** 다음 즉시 작업 = **Task 3 GUI 검증(미완) + Task 5(Carrier, 가장 무거움)**.
> **작성**: 2026-06-02. 브랜치 `game/module/ingame/temp`. HEAD `d44a4a3 [dev] : impulse`.
> **이 문서가 대체(supersede)**: `doc/handoffs/2026-06-02/2026-06-02-playerbehavior-decomposition-resume-handoff.md` (§2 Task 표가 Task 4=미착수로 stale — 본 문서 우선). 그 문서의 §4(Life reconcile)·§5(잠긴결정)·§8(충돌매트릭스 서술)·§9(알려진이슈)는 여전히 유효 — *깊이* 가 필요하면 참조.
> ⚠ **이 repo 는 여러 에이전트가 병렬 커밋 중**(PlayableDirector foundation / VFX·Effekseer / Fog·PostFX / EnemyBuilder). HEAD·working tree 가 수시로 바뀐다 — 시작 시 **반드시 `git log --oneline -10` + `git status --short` 실측**.

---

## 0. 한 눈에 (TL;DR)

- **하는 일**: 죽은 god-component `PlayerBehavior` 를 `Entity/Components` 로 분해 (Dash→Impulse / i-frame+Die→Life / 방향스프라이트→PlayerSpriteDirector(→PlayableDirector 통합) / 데미지배달→Carrier / 엔진 GetComponent 인터페이스 조회 허용).
- **진행**: **Task 0·1·2·3·4 커밋 완료**(HEAD `d44a4a3` 반영 검증). **Task 5·6·7 미착수.**
- **다음 행동**: ① **Task 3 GUI 검증**(여전히 미수행 — 적 스폰 live 라 가능: 적 접촉→플레이어 즉사) → ② **Task 5 (Carrier)** 구현. (Task 6 은 PlayableDirector foundation 선행 의존 — §6 매트릭스.)
- **정본**: spec `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md` + plan `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` (**둘 다 gitignore=로컬 전용 — `git check-ignore` 확인됨**) + 설계 핸드오프 `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md`(추적).
- **카덴스**: Task별 게이트(서브에이전트 구현 no-commit → 오케스트레이터 diff 직접검증 → 사용자 승인 커밋). `no_auto_tests`→빌드+GUI 검증. 구현 서브에이전트 sonnet.

---

## 1. git 현실 (작성 시점 — 재측정 필수)

```
HEAD: d44a4a3 [dev] : impulse        ← 이번 세션 Task 4 가 여기 커밋됨
브랜치: game/module/ingame/temp
최근 커밋 체인(신→구):
  d44a4a3 [dev] : impulse            (← Task 4: PhysicsImpulse.h + PlayerActor.cpp, TweenPlayable.h fold)
  8b5fc45 [dev] : rename class & file (← snake_case → PascalCase 헤더 rename: physics_movement.h→PhysicsMovement.h 등)
  b4e5eb8 / d82e8b9 effekseer update and postFX update
  f1e1a23 effekseer test + imgui pass + life Timer (← Task 0~3 fold + Timer 마이그레이션)
  b3cc9b4 effekseer 1.7 version
  53bc25e .efk 텍스처 검증 배선  /  d3b31a5 wave demo (EnemyBuilder + WaveController)
```

**`d44a4a3 [dev] : impulse` 내용** (Task 4 — 이번 세션):
```
 apps/_MyApp_/src/Entity/Player/PlayerActor.cpp |  5 ++   (include 1줄 + AddComponent<Impulse> 1줄)
 apps/_MyApp_/src/Physics/PhysicsImpulse.h      | 64 ++   (신규 header-only)
 apps/_MyApp_/src/Tween/TweenPlayable.h         | 47 +-   (★ 내 작업 아님 — 사용자가 같은 커밋에 fold)
```

**working tree 미커밋 (전부 *다른 에이전트* — 미접근, path-scoped 커밋 준수)**:
```
 M apps/_MyApp_/main.cpp                         (Fog/PostFX + foundation 경합)
 M apps/_MyApp_/src/Bootstrap/CMakeLists.txt     (EnemyBuilder/foundation)
 M apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp   (EnemyBuilder 트랙 — IDE 로 열려있음)
 M apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp  (foundation: director 부착 — 3자 공유 편집점)
 M apps/_MyApp_/src/CMakeLists.txt               (Playable subdir 추가 등)
 M shell/CMakeExecute.sh  /  M src/buffer/framebuffer.h  /  M src/material/pass.h  (PostFX/Fog 엔진)
?? apps/_MyApp_/src/Playable/PlayableDirector.{cpp,h}   (★ PlayableDirector foundation — 신규 진행중)
?? apps/_MyApp_/src/Playable/PostFXRegistry.{cpp,h}     (★ foundation — PostFX 레지스트리)
?? apps/_MyApp_/src/Playable/CMakeLists.txt             (★ foundation)
?? doc/handoffs/2026-06-02/2026-06-02-task4-impulse-agent-prompt.md (Task 4 디스패치 프롬프트)
```
- **내 분해 미커밋 = 없음**(Task 0~4 전부 HEAD).
- **⚠ 구 문서 대비 변화**: PlayableDirector foundation 이 "미착수"→**working tree 에서 진행 중**(미커밋). Task 6 은 이 foundation 위에서 재편됨(§6).

---

## 2. 이번 세션이 한 일 — Task 4 (Impulse) ✅ 커밋

Dash/Knockback 용 일회성 물리 속도버스트 컴포넌트 신설 + 플레이어 부착. **미배선 ship**(dash 입력 없음 → dormant, correct-by-construction).

**① `apps/_MyApp_/src/Physics/PhysicsImpulse.h` (신규, header-only)**
- 파일명 결정: 사용자 선택 **PascalCase**(형제 `PhysicsComponent/PhysicsMovement/PhysicsSystem.h` 와 정합 — `8b5fc45` rename 방향). 핸드오프 프롬프트의 `physics_impulse.h`(snake) 대신 `PhysicsImpulse.h`.
- `namespace TopdownShooter::Physics`, `class Impulse : public SJH::Scene::Component, public Entity::IImpulsable`.
- include 4종 = `PhysicsMovement.h` 와 동일(`Algebraic/Stat.h`·`Entity/Components/Components.Interfaces.h`·`Physics/PhysicsComponent.h`·`scene/actor.h`) + `<cmath>`.
- `OnEnter()`=`Components::FindPhysics(GetOwner())`(비소유) / `Update(dt)`=active·cooldown 타이머 감소.
- `DoImpulse(vmath::vec2 dir)` override = cd/active 게이트 → normalize → `SetLinearVelocity(b2Vec2(n[0]*force, -n[1]*force))`(XZ→XY, Z→-Y, spec §4.4) → 타이머 arm. `mImpulseForce`=Stat(DashForce, 7.5) / `mCooldown`=Stat(CoolDownSpeed, 0.8s) / `mDurationSec`=0.3f plain.
- `ApplyKnockback(vmath::vec2)` convenience(Monster Knockback) + `IsActive()`. **`IImpulsable` 재정의 없이 구현만**(Task 1 의 `Components.Interfaces.h:122` 이미 존재).

**② `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` (+5)**
- `#include "Physics/PhysicsImpulse.h"` (PlayerActor.h 아래).
- physics 분기 `auto *pm = AddComponent<Physics::PhysicsMovement>(...)` **직후**: `actor->AddComponent<Physics::Impulse>();` + 한국어 주석.
- **controller / Action::Dash / DoForward suppress 미접근** — 미배선 ship. (속도싸움 suppress 는 dash 입력 바인딩 시 후속.)

**검증**: `cmake --build --preset ninja --target _MyApp_` → exit 0, `[20/21] Linking CXX executable apps/_MyApp_/_MyApp_`. 게임플레이 변화 0(아무도 `DoImpulse` 호출 안 함). 빌드가 검증의 전부.
**참고(concern 아님)**: clangd 가 PlayerActor.cpp 의 include 를 "not used directly" 표시 — `AddComponent<Impulse>()` 템플릿 인자를 IWYU 가 못 세는 한계. include 는 실제 필요(Impulse 유일 정의처), `-Werror` 는 unused-include 미검사 → 빌드 무관.

---

## 3. Task 0~7 상태표

| Task | 내용 | 상태 | 커밋/검증 |
|---|---|---|---|
| **0** | 엔진 `GetComponent<T>` 게이트 완화 + 본문 `if constexpr(is_base_of<Component,T>)` 가드(설계갭 fix) | ✅ 커밋 | `78ca9ee`(+if-constexpr `1f6c23f` fold). HEAD✓ |
| **1** | `IActorPresentation`+`IImpulsable`+`EFacing`/`EPose`/`Quantize4` | ✅ 커밋 | `82735ed`. HEAD✓ |
| **2** | `Life` 확장(i-frame+DoDie+sink) — 이후 Timer 마이그레이션 `optional<Timer>` 재작성(**계약 보존**) | ✅ 커밋 | `1f6c23f`→`f1e1a23`. HEAD✓ |
| **3** | `EnemyContactHandler`→`Life::DoDamaged`(IDamageable) repoint(load-bearing) | ✅ 커밋 | HEAD✓. **⚠ GUI 실행 검증 여전히 미수행** |
| **4** | `Impulse`(`Physics/PhysicsImpulse.h` 신규, IImpulsable, DashForce/CoolDownSpeed Stat) + PlayerActor physics 분기 부착. **미배선 ship** | ✅ **커밋(이번 세션)** | **`d44a4a3`. HEAD✓** (§2) |
| **5** | `Carrier` base+`Projectile`/`ContactCarrier` — Bullet→Projectile / 적→ContactCarrier. BulletContactHandler/EnemyContactHandler 흡수·삭제. **가장 무거움(5a/5b)** | ⬜ 미착수 | `Entity/{Bullet,Enemy}` 존재, Carrier 없음 확인 |
| **6** | `PlayerSpriteDirector`(8그룹 Visible 토글 + IActorPresentation sink + FX Layer C) — **PlayableDirector foundation 위로 재편**(§6) | ⬜ 미착수(foundation 선행) | PlayerSpriteDirector 없음 |
| **7** | `PlayerBehavior.{h,cpp}`+`BulletSpawnPlayable.{h,cpp}`+`PlayerMovementComponents.h` 철거 + Entity CMake | ⬜ 미착수 | `PlayerBehavior.{cpp,h}` 잔존 확인 |

---

## 4. 잠긴 설계 결정 (재논의 금지) — Impulse 부분은 §2 로 실현됨

설계 핸드오프 §13 + 구 resume 핸드오프 §5 의 잠긴 결정 그대로 유효:
- α: `GetComponent<T>` 게이트 `is_polymorphic_v` 완화(인터페이스 조회, Unity 정통). **fast-path 는 `if constexpr(is_base_of_v<Component,T>)` 가드**(concrete=static_cast 핫패스 / 인터페이스=slow dynamic_cast). ※ 설계 핸드오프 §13.1 의 "static_cast 오용 없음" 은 사실오류로 정정됨.
- RD1~RD6: Life 단일변이점이 `IActorPresentation` sink 로 forward(Template-Method). 액터당 sink 1 / 베이스 직접호출 / silent no-op / blanket 서브클래스 거부(+PlayerMovement 삭제). RD5: PlayerController 단일작성자 facing/pose.
- C1: Carrier `GetComponent<IDamageable>->DoDamaged` + IImpulsable 넉백. **IImpulsable 신규(IMovable 오버로드 금지)** — Task 4 가 이 인터페이스를 처음 구현.
- **Impulse 파라미터**(§2 실현): DashForce 7.5 / CoolDownSpeed 0.8s Stat + plain duration 0.3s. 속도싸움 = controller 가 `IsActive()` 중 `DoForward` suppress(dash 바인딩 시 후속).
- 확정 4 디테일: i-frame=plain float→(Timer 마이그레이션 optional<Timer>, 계약 동일) / **Components::Movement 유지** / 공격윈도 0.15s / Carrier 얇은 베이스.

**현재 `Life` 실체**(구 문서 §4 — 여전히 유효): plain-float 가 `SJH::Timer` 로 재작성돼 `optional<Timer> mInvincibleTimer/mDieTimer`. 공개 계약(`SetIFrameSeconds`/`SetDeathDelaySeconds`/`SetOnDeathFx`/`IsInvincible`/sink `ReactDamaged`·`ReactDied`/`DoDamaged`/`DoDie`) 보존 → **Task 6 영향 0**. 현재 i-frame/사망지연 둘 다 nullopt(즉사=기존동작); 플레이어 0.5s 주입은 Task 6.

---

## 5. 다음 즉시 작업

### 5.1 Task 3 GUI 검증 (여전히 미완 — 적 스폰 live 라 가능)
WaveController 가 main.cpp 에 배선됨(`d3b31a5`/`f1e1a23`) → 적이 스폰·추적. Task 3(접촉 데미지) 런타임 검증:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
cmake --build --preset ninja --target _MyApp_      # exit 0
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
- **기대**: 적 접촉 → 플레이어 **즉시 데미지 → 사라짐**(SetActive(false)). i-frame 미주입(Task 6에서 0.5s)이라 즉사 = 정상(버그 아님). load-bearing 수정 + 인터페이스 조회(GetComponent<IDamageable>→Life) 런타임 증명.
- **혼동 주의**: Effekseer `.efk` 로드 실패로 일부 VFX 안 보일 수 있음 — 분해 무관.

### 5.2 Task 5 (Carrier, 빌드+GUI, 가장 무거움 5a/5b)
plan 의 Task 5 전문을 서브에이전트에 디스패치(plan 파일 읽히지 말 것 — 추출). 요지:
- `Spawns/Carrier.h` 신규: `CarrierBase`(얇은 배달) + `Projectile` + `ContactCarrier`.
- **5a**: bullet_factory `BulletContactHandler`→`Carrier::Projectile`. **BulletLifetime 은 이제 Timer 기반(ctor 동일=유지)** + Projectile self-despawn 공존(hit vs lifetime, 무충돌).
- **5b**: EnemyFactory `EnemyContactHandler`→`ContactCarrier` + 둘 삭제. **`EnemyBuilder` 는 `CreateEnemyActor` 위에 스프라이트만 얹으므로 EnemyFactory 변경에 영향 없음**(충돌0). ※ EnemyBuilder.cpp 는 다른 에이전트가 IDE 로 열어둠 — **EnemyFactory 쪽만 surgical, EnemyBuilder 미접근**.
- Carrier 는 `GetComponent<IDamageable>->DoDamaged`(Task 0 인터페이스 조회) + 넉백 시 `GetComponent<IImpulsable>->DoImpulse`(=Task 4 의 `Impulse`). **Task 4 가 넉백 타겟을 이미 제공.**

---

## 6. 병렬 트랙 충돌 매트릭스 (재측정 — foundation 진행중으로 갱신)

> **PlayableDirector foundation 재편**(사용자 철학: *게임로직 외 모든 연출은 PlayableDirector 경유*): Task 6 의 `PlayerSpriteDirector` 는 별도 director 가 아니라 **`PlayableDirector` 로 통합**(액터당 sink 1 = PlayableDirector). 순서: **① foundation(PlayableDirector + PostFXRegistry + onFire/onDamage 마이그레이션) — 현재 working tree 진행중 → ② 그 위 스프라이트 Playable(Task 6) ∥ PostFX·사운드 Playable 병렬.** 정본 spec `doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md`.

| 트랙 | 상태 | 내 분해와 |
|---|---|---|
| **PlayableDirector 연출 foundation** | 🟡 **진행중·미커밋**(`?? Playable/PlayableDirector.{cpp,h}`+`PostFXRegistry.{cpp,h}`+CMake). PlayerBuilder.cpp 에 director 부착 | **Task 6 의 sink = PlayableDirector. foundation 커밋 후 Task 6.** PlayerActor.cpp 공유(내 Impulse 줄은 커밋됨 → 충돌0, additive) |
| **EnemyBuilder + WaveController** | ✅ 커밋(`d3b31a5`+`f1e1a23`) + working tree 추가변경. 적 스폰 live | EnemyFactory 무변경 → **Task 5 와 충돌0** (Task 5 는 EnemyFactory만 만짐) |
| **Entity 피격/사망 FX (hit-FX)** | 🟡 Layer A(SpriteRenderer uniform `1a9196c`)+B(Life Timer) 커밋. PlayableDirector 경유로 재편(Vignette/Grayscale/Damaged Playable) | Life 계약 보존 → Task 6 영향0. **Layer C(sink 구동)=Task 6** |
| **Fog/PostFX + imgui pass** | 🟡 진행중·미커밋(`M main.cpp`/`framebuffer.h`/`pass.h`). **main.cpp 경합** | Task 5~7 은 **main.cpp 미접근** → 충돌0 |
| **Effekseer 1.7 / .efk / vfx** | 🟡 진행중(VFX 에이전트). `.efk` 로드 실패 잔존 | 분해 무관 (resources/vfx + VFXSystem) |

---

## 7. 가드레일 / 컨벤션 (carry-forward)
- **커밋 = 사용자 승인 후**(무단 금지). **path-scoped `git add <경로>`**(`-A`/`.` 금지 — foundation/VFX/Fog 미커밋 휩쓸지 말 것).
- **`Co-Authored-By` 트레일러 미사용**(프로젝트 컨벤션 — 시스템 기본값 무시).
- **미접근 경계**: `main.cpp`(Fog/foundation 경합) / `EnemyBuilder.cpp`(다른 에이전트 IDE 오픈, Task 5 는 EnemyFactory 만) / `Timer 코어 src/timer/`(불가침 Value Object) / 셰이더 `billboard_atlas.{vs,fs}` / working tree 의 모든 `?? Playable/*`·`M` 파일(타 에이전트).
- **잠긴 결정 재논의 금지**(§4). §13.1 만 사실오류 정정(설계의도 불변).
- `no_auto_tests`(단위테스트 금지, 검증=빌드+GUI). 주석 한국어. 헤더가드 `__XXX_H__`(#pragma once 금지). `long` 금지(고정폭 int32_t/uint64_t). 경로 슬래시. windows.h 는 `#ifdef _WIN32`+`NOMINMAX`.
- ctor 에서 sink/virtual/FindPhysics 해소 금지 → `OnEnter`. 액터당 IActorPresentation/Life 1구현.
- **rename 진행중**(snake→PascalCase, `8b5fc45`): include 전 **실제 파일명 확인**(`ls` / 형제 헤더의 include 복사).

## 8. 정본 문서 인덱스
| 문서 | 경로 | git | 역할 |
|---|---|---|---|
| 분해 spec | `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md` | **gitignore(로컬)** | §13 잠긴결정 + §4 분해테이블 + 시그니처 |
| 분해 plan | `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` | **gitignore(로컬)** | Task 0~7 완전코드+검증/커밋 step |
| 설계 핸드오프 | `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md` | 추적 | §13/§14 잠긴결정(§13.1 정정됨) |
| 구 resume 핸드오프 | `doc/handoffs/2026-06-02/2026-06-02-playerbehavior-decomposition-resume-handoff.md` | 추적 | 🔴 본 문서가 대체. §4/§5/§8/§9 *깊이* 만 참조 |
| Task 4 프롬프트 | `doc/handoffs/2026-06-02/2026-06-02-task4-impulse-agent-prompt.md` | working tree | Task 4 디스패치 원본(완료됨) |
| foundation spec | `doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md` | (확인要) | PlayableDirector 재편 — Task 6 선행 |
| **이 문서** | `doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-task4-done-resume-handoff.md` | 추적 | **재개 단일 진입점** |
> spec/plan 이 다른 머신/세션에 없으면(gitignore) 설계 핸드오프 §13/§14 + 본 문서로 재구성.

## 9. 변경 기록
| 일자 | 변경 |
|---|---|
| 2026-06-02 | **Task 4(Impulse) 구현·커밋(`d44a4a3`)** — `PhysicsImpulse.h`(PascalCase 결정) + PlayerActor 부착, 미배선 ship, 빌드 exit 0. Task 표 갱신(4=✅). git 재측정: HEAD `8b5fc45`→`d44a4a3`, PlayableDirector foundation 이 working tree 진행중으로 갱신(구 문서 "미착수"). 충돌매트릭스 재측정. 구 resume 핸드오프(2026-06-02) 대체. 다음=Task3 GUI검증→Task5(Carrier). |
