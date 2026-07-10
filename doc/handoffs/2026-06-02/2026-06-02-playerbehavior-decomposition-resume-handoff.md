# Handoff — PlayerBehavior 분해 *재개 컨텍스트* (2026-06-02, 무손실 이주)

> 🔴 **SUPERSEDED (2026-06-02) → 단일 진입점은 [`doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-task4-done-resume-handoff.md`](2026-06-02-pb-decomposition-task4-done-resume-handoff.md).** Task 4(Impulse)가 `d44a4a3` 로 커밋되어 본 문서 §2 Task 표(4=미착수)·§7(다음=Task4)·§8(foundation 미착수)이 stale. **§4(Life reconcile)·§5(잠긴결정)·§9(알려진이슈)는 여전히 유효** — *깊이* 가 필요할 때만 참조.

> **이 문서 = 다음 세션의 단일 진입점.** PlayerBehavior god-component 분해(Task 0~7)를 subagent-driven 으로 실행 중이며, **Task 0~3 커밋 완료 / Task 4~7 미착수**. 다음 즉시 작업 = **Task 3 GUI 검증 → Task 4(Impulse)**.
> **작성**: 2026-06-02. 브랜치 `game/module/ingame/temp`. HEAD `f1e1a23`.
> **선행 실행 핸드오프**(이 문서가 대체): `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition-execution.md` (§3 Task 표가 stale — 본 문서 우선).
> ⚠ **이 repo 는 여러 에이전트가 병렬 커밋 중**(VFX/Effekseer/Timer/Fog·PostFX/imgui). HEAD·working tree 가 수시로 바뀐다 — 시작 시 **반드시 `git log --oneline -10` + `git status --short` 실측**.

---

## 0. 한 눈에 (TL;DR)

- **하는 일**: 죽은 god-component `PlayerBehavior` 를 `Entity/Components` 로 분해 (Dash→Impulse / i-frame+Die→Life / 방향스프라이트→PlayerSpriteDirector / 데미지배달→Carrier / 엔진 GetComponent 인터페이스 조회 허용).
- **진행**: **Task 0·1·2·3 커밋 완료**(HEAD 반영 검증). **Task 4·5·6·7 미착수.**
- **다음 행동**: ① **Task 3 GUI 검증**(이제 적이 스폰되니 가능 — 적 접촉→플레이어 데미지) → ② **Task 4 (Impulse)** 재개.
- **정본**: spec `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md` + plan `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` (둘 다 **gitignore=로컬 전용**) + 설계 핸드오프 `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md`(추적).
- **카덴스**: Task별 게이트(서브에이전트 구현 no-commit → 오케스트레이터 diff 직접검증 → 사용자 승인 커밋). `no_auto_tests`→빌드+GUI 검증. 구현 서브에이전트 sonnet.

---

## 1. git 현실 (작성 시점 — 재측정 필수)

```
HEAD: f1e1a23 [dev] : effekseer test & imgui pass update + life componenet TImer
브랜치: game/module/ingame/temp
최근 커밋 체인(신→구):
  f1e1a23 effekseer test + imgui pass + life Timer (← 커밋 갭 닫은 mixed 커밋)
  b3cc9b4 effekseer 1.7 version
  53bc25e .efk 텍스처 검증 배선
  d3b31a5 wave demo (EnemyBuilder + WaveController 소스)
  32db244 EffekseerPlayable lifecycle 진단
  0e8836c / 79e8902 timer PollInterval 주석 / EffekseerDiagnostics
  0fe4ea2 / 3f56cb2 / c8dad08 / b60a17b / bb18ce6  ← SJH::timer 모듈 신설(16번째)
  1a9196c [dev] entity hit, dissolve (← FX Layer A: SpriteRenderer FX uniform)
  d4d37ef effekseer vfx / 9391c50 Effekseer resource
  1f6c23f Life + DoDie + sink (← 내 Task 2, 이후 f1e1a23 이 Timer 로 재작성)
  82735ed IActorPresentation+IImpulsable (Task 1) / 78ca9ee GetComponent 게이트(Task 0)
```
- **내 분해 미커밋 = 없음**(Task 0~3 전부 HEAD). working tree 의 미커밋은 **VFX 에이전트의 `resources/vfx/...` 뿐**(내 것 아님 — 미접근).
- **커밋 갭 닫힘**: 적스폰 배선(main.cpp+CMake)·Timer 마이그레이션(Life/BulletLifetime)이 `f1e1a23` 에 (effekseer/imgui 와 섞여) 커밋됨. 깔끔한 분리는 아니나 유실 없음.

---

## 2. Task 0~7 상태표

| Task | 내용 | 상태 | 커밋/검증 |
|---|---|---|---|
| **0** | 엔진 `GetComponent<T>` 게이트 `is_polymorphic` + **본문 `if constexpr(is_base_of<Component,T>)` 가드**(설계갭 fix, §5) + PhysicsComponent.h:73 주석 | ✅ 커밋 | `78ca9ee` (+ if-constexpr 는 `1f6c23f` 에 fold). HEAD 검증✓ |
| **1** | `IActorPresentation`+`IImpulsable`+`EFacing`/`EPose`/`Quantize4` | ✅ 커밋 | `82735ed`. HEAD 검증✓ |
| **2** | `Life` 확장 (i-frame+DoDie+sink) — **이후 Timer 마이그레이션이 `optional<Timer>` 로 재작성**(§4) | ✅ 커밋 | `1f6c23f`→`f1e1a23`. **계약 보존**. HEAD 검증✓ |
| **3** | `EnemyContactHandler` → `Life::DoDamaged`(IDamageable) repoint (load-bearing) | ✅ 커밋 | HEAD 검증✓. **⚠ GUI 실행 검증 미수행** — 다음 작업 |
| **4** | `Impulse`(`Physics/physics_impulse.h` 신규, IImpulsable, DashForce/CoolDownSpeed Stat) + CreatePlayerActor physics 분기 AddComponent. **미배선 ship** | ⬜ 미착수 | physics_impulse.h 없음(정상) |
| **5** | `Carrier` base+`Projectile`/`ContactCarrier` — Bullet→Projectile / 적→ContactCarrier. BulletContactHandler/EnemyContactHandler 흡수·삭제. **가장 무거움(5a/5b)** | ⬜ 미착수 | |
| **6** | `PlayerSpriteDirector`(8그룹 Visible 토글 + IActorPresentation sink **+ FX 구동 Layer C**) + PlayerActor/PlayerBuilder(`SetIFrameSeconds(0.5).SetDeathDelaySeconds(0.5)` 주입)/PlayerController(RD5+flipX삭제) | ⬜ 미착수 | PlayerSpriteDirector 없음(정상) |
| **7** | `PlayerBehavior.{h,cpp}`+`BulletSpawnPlayable.{h,cpp}`+`PlayerMovementComponents.h` 철거 + Entity CMake | ⬜ 미착수 | PlayerBehavior.h 아직 존재 |

---

## 3. 정본 문서 인덱스

| 문서 | 경로 | git | 역할 |
|---|---|---|---|
| 분해 spec | `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md` | **gitignore(로컬)** | §13 잠긴결정 + §4 분해테이블 + 실제시그니처. **§3.1 정정(if-constexpr) + §5.3 갱신(Life optional<Timer>) 반영됨** |
| 분해 plan | `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` | **gitignore(로컬)** | Task 0~7 완전코드+검증/커밋 step. Task 6 = FX fold + ENEMY/PLAYER 이름 갱신 반영 |
| 설계 핸드오프 | `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md` | 추적 | §13/§14 잠긴결정. §13.1 정정됨(static_cast 오용 → if-constexpr) |
| 실행 핸드오프(구) | `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition-execution.md` | 추적 | **§3 Task 표 stale — 본 문서가 대체** |
| **이 문서** | `doc/handoffs/2026-06-02/2026-06-02-playerbehavior-decomposition-resume-handoff.md` | 추적 | **재개 단일 진입점** |
> spec/plan 은 `doc/superpowers/`=gitignore. 다른 머신/세션에 없으면 설계 핸드오프 §13/§14 + 본 문서로 재구성.

---

## 4. 현재 `Life` 실체 (Task 2 ↔ Timer 마이그레이션 reconcile — **중요**)

내 Task 2 의 plain-float Life 가 직후 `SJH::Timer` 마이그레이션으로 재작성됨. **공개 계약은 보존 → Task 6 영향 0.**
- **현행 멤버**: `std::optional<SJH::Timer::Timer> mInvincibleTimer`(i-frame, nullopt=무적없음) + `mDieTimer`(사망지연, nullopt=즉시) + `mDeathFxFired`(one-shot) + `mOnDeathFx` + `IActorPresentation* mSink`. helper `ArmInactive(slot, s)`(s>0 → emplace(s)+Tick(s)로 *finished 장전*=스폰즉시무적 방지 / s<=0 → reset).
- **보존된 계약**(검증✓): `SetIFrameSeconds(float)` / `SetDeathDelaySeconds(float)` / `SetOnDeathFx` / `IsInvincible()`(=`mInvincibleTimer && !->IsTimesUp()`) / `DoDamaged`(i-frame게이트+`mSink->ReactDamaged`+arm+DoDie) / `DoDie`(one-shot+`mSink->ReactDied`+onDeathFx+지연/즉시 SetActive).
- **sink forwarding(RD1) 인택트**: `DoDamaged`→`ReactDamaged(dmg)`, `DoDie`→`ReactDied(pos)`. → Task 6 의 PlayerSpriteDirector 가 그대로 받음.
- **현재 i-frame/사망지연 둘 다 미설정(nullopt)** → 적·플레이어 모두 즉시사망=기존동작. **플레이어 0.5s 주입은 Task 6**(PlayerBuilder).
- 정본: 현재 `LifeComponents.h` + `doc/handoffs/2026-06-02/2026-06-02-timer-migration-followup-agent-prompt.md` §1.2/§2. **Timer 코어(`src/timer/timer.h`)는 불가침 Value Object — baseTime const·assert(>0). "부재=optional nullopt"(§2.2).**

---

## 5. 잠긴 설계 결정 (재논의 금지) + 발견된 설계갭 정정

**§13 잠긴 결정** (설계 핸드오프 §13):
- α: `GetComponent<T>` 게이트 `is_polymorphic_v` 완화 (인터페이스 조회, Unity 정통).
- RD1 OPT-1: Life 단일변이점이 `IActorPresentation` sink 로 forward (Template-Method). RD2 액터당 sink 1 / RD3 베이스 직접호출 / RD6 silent no-op / RD4 blanket 서브클래스 거부(+PlayerMovement 삭제).
- RD5: PlayerController 단일작성자 `facing=attack?quantize4(aim):move?quantize4(vel):last; pose=(attack‖move)?Move:Idle`.
- C1: Carrier `GetComponent<IDamageable>->DoDamaged` + IImpulsable 넉백. IImpulsable 신규(IMovable 오버로드 금지).
- Impulse=Physics/, DashForce(7.5)/CoolDownSpeed(0.8) Stat + plain duration(0.3). 속도싸움=controller 가 IsActive() 중 DoForward suppress(dash 바인딩 시).
- **확정 4 디테일**: i-frame=plain float→(Timer 마이그레이션으로 optional<Timer>, 계약동일) / **Components::Movement 유지(삭제 안 함)** / 공격윈도 0.15s / Carrier 얇은베이스.

**⚠ 발견·정정된 설계갭 (Task 2 중, §13.1 정정)**: "게이트만 완화하면 됨, static_cast 오용 없음" 은 **오류**. fast-path `static_cast<T*>(Component*)` 는 인터페이스 T 에서 컴파일 에러(무관 타입 변환). **fix = fast-path 를 `if constexpr (std::is_base_of_v<Component, T>)` 로 가드**(concrete=static_cast 핫패스, 인터페이스=slow-path dynamic_cast). HEAD 반영✓. 인터페이스 조회는 안 막힘(slow-path 가 if-constexpr 밖). 설계 핸드오프 §13.1 + 분해 spec §3.1 정정 완료.

---

## 6. 잔여 Task 4~7 — 구현 포인터 (plan 의 해당 Task 전문 추출해 서브에이전트 디스패치)

> 카덴스: full task text 제공(plan 파일 읽히지 말 것) + no-commit/no-test + 빌드검증 + DONE/CONCERNS/BLOCKED. 오케스트레이터가 diff 직접검증 → 사용자 커밋.

- **Task 4 (Impulse, 빌드-only)**: `Physics/physics_impulse.h` 신규(header-only, PhysicsMovement.h 패턴 — `class Impulse : Component, Entity::IImpulsable`, DoImpulse=cd/active게이트→SetLinearVelocity XZ→XY→타이머arm, FindPhysics OnEnter). CreatePlayerActor physics 분기에 `AddComponent<Physics::Impulse>()`. **미배선 ship**(dash 입력 없음). 병렬작업 무관·충돌0.
- **Task 5 (Carrier, 빌드+GUI, 가장 무거움 5a/5b)**: `Spawns/Carrier.h` 신규(CarrierBase 얇은배달+Projectile+ContactCarrier). 5a: bullet_factory `BulletContactHandler`→`Carrier::Projectile`(BulletLifetime ctor 불변=유지). 5b: EnemyFactory `EnemyContactHandler`→`ContactCarrier` + 둘 삭제. **주의: BulletLifetime 은 이제 Timer 기반(ctor 동일)** + Projectile self-despawn 과 공존(hit vs lifetime, 무충돌). **EnemyBuilder 는 CreateEnemyActor 위에 스프라이트만 얹으므로 EnemyFactory 변경에 영향받지 않음**(충돌0).
- **Task 6 (Director+RD5+FX, 빌드+GUI)**: `PlayerSpriteDirector.{h,cpp}` 신규(8그룹 DirGroup Visible 토글 + IActorPresentation sink). PlayerActor 8그룹 빌드(`PLAYER_{F/B/L/R}_{IDLE/MOVE}` from Constants.h, `EntityTextureConfig`). PlayerBuilder `kPlayerGroups` 8테이블 + `life->SetIFrameSeconds(0.5).SetDeathDelaySeconds(0.5)` 주입(계약 보존됨✓) + child-scan 은퇴. PlayerController RD5 계산→push + flipX 핵 삭제. **+ FX 구동(Layer C)**: ReactDamaged(enableHit 0.42s)/ReactDied(dissolveThreshold 0→1 over 0.5s)/OnEnter dissolve.png 로드 — **SpriteRenderer FX 필드(enableHit/enableDissolve/dissolveThreshold/dissolveTex)는 1a9196c 로 이미 존재**. FX spec `doc/superpowers/specs/2026-06-01-entity-hit-death-fx-design.md` §4.3. **⚠ main.cpp 미접근**(Task 6 은 PlayerActor/Builder/Controller 만). PLAYER_ prefix + EntityTextureConfig 이름 반영됨.
- **Task 7 (철거, 빌드+GUI)**: PlayerBehavior/BulletSpawnPlayable/PlayerMovementComponents 삭제 + Entity CMake + EnemyDeathHandler.h PB주석. grep 0참조.

---

## 7. 다음 즉시 작업 — Task 3 GUI 검증 (이제 가능)

WaveController 가 main.cpp 에 배선됨(`f1e1a23`/`d3b31a5`) → **적이 ENEMY_FRONT 스프라이트로 스폰**(3종, 2프레임 walk, 추적). 따라서 Task 3(접촉 데미지) 런타임 검증 가능:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
cmake --build --preset ninja --target _MyApp_      # exit 0
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
- **기대**: 적이 플레이어에 접촉 → 플레이어 **빠른 데미지 → 사라짐**(SetActive(false)). **i-frame 미주입(Task 6에서 0.5s)이라 즉사 = 정상**(버그 아님). 이전(버그)엔 접촉 no-op. = load-bearing 수정 + 인터페이스 조회(GetComponent<IDamageable>→Life) 런타임 증명.
- 적 스폰/스프라이트/추적이 보이는지도 동시 확인(EnemyBuilder/Wave 결과).
- **혼동 주의**: Effekseer `.efk` 로드 실패(§9)로 일부 VFX 안 보일 수 있음 — 내 Task 무관.

---

## 8. 병렬 feature 트랙 상태 (충돌 매트릭스)

> **⚠ 2026-06-02 재편 — PlayableDirector foundation**: 사용자가 *"게임로직 외 모든 연출은 반드시 PlayableDirector 경유"* 철학 확정. → **내 Task 6 의 `PlayerSpriteDirector` 는 `PlayableDirector` 로 통합·재편**(별도 director 아님, RD2 액터당 sink 1개 = PlayableDirector). 새 순서: **① 공유 foundation(PlayableDirector + PostFXRegistry + onFire/onDamage 마이그레이션) 먼저(별도 에이전트) → ② 그 위에 스프라이트 Playable(내 Task6: directional/HitBlink/Dissolve 등록) ∥ PostFX·사운드 Playable(hit-FX: Vignette/Grayscale/Damaged) 병렬.** 정본 spec `doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md` + 프롬프트 `doc/handoffs/2026-06-02/2026-06-02-playable-director-foundation-agent-prompt.md`. **공유 편집점=PlayerBuilder.cpp(3자). enemy/bullet delegate 제거는 Task5 조율.** (§6/§2 의 "PlayerSpriteDirector 직접 uniform 토글" 서술은 이 재편으로 *Playable-through-director* 로 대체 — Task6 메커니즘 변경, RD1~6 은 보존.)

| 트랙 | 상태 | 내 분해와 |
|---|---|---|
| **PlayableDirector 연출 foundation** | ⬜ 설계 완료, 미착수(별도 에이전트 예정) | **Task 6 의 sink = PlayableDirector(통합). foundation 먼저 → Task6 병렬.** spec `2026-06-02-playable-director-foundation-design.md` |
| **EnemyBuilder + WaveController 배선** | ✅ 커밋(`d3b31a5`+`f1e1a23`). 적 스폰 live | EnemyFactory 무변경 → Task 5 와 충돌0. spec `2026-06-01-enemy-builder-sprite-design.md` |
| **Entity 피격/사망 FX (= hit-FX 트랙)** | 🟡 Layer A(SpriteRenderer uniform) 커밋(`1a9196c`) + Layer B(Life 사망지연=Timer mDieTimer) 커밋(`f1e1a23`). **이제 PlayableDirector 경유로 재편** — Vignette/Grayscale/Damaged Playable 을 foundation 위 등록 | spec `2026-06-01-entity-hit-death-fx-design.md`(§4.2/4.3) + 재편 결정 `2026-06-02-playable-director-foundation-design.md`. (구 `2026-06-02-player-hit-fx-decision-response.md` 무효) |
| **SJH::Timer 마이그레이션** | ✅ 모듈+BulletLifetime+Life 커밋. WaveController 케이던스(#4)는 후속 | Life 계약 보존(§4) → Task 6 영향0 |
| **Effekseer 1.7 / 진단 / vfx 재배치** | 🟡 진행 중(VFX 에이전트). `.efk` 로드 실패 잔존(§9) | 내 분해 무관 (resources/vfx + VFXSystem) |
| **Fog/PostFX + imgui pass** | 🟡 진행 중. **main.cpp 경합** | 내 Task 4~7 은 **main.cpp 미접근**(단 PostFXRegistry 1줄은 foundation 에이전트) → 충돌0 |

---

## 9. 알려진 이슈 (내 분해 무관, 혼동 방지)
- **Effekseer `.efk` 로드 7종 실패**(muzzle/dust/hit/laser/orbital/slash/summon) — `Effect::Create` 실패. VFX 에이전트가 effekseer 1.7 + .efk 경로/텍스처(메모리 [[efk_texture_basepath_trap]]) 작업 중. 런타임 FX 안 보여도 내 Task 와 무관.
- **working tree 의 `resources/vfx/...` 대량 변경** = VFX 에이전트. **건드리지 말 것**(path-scoped 커밋).

---

## 10. 가드레일 / 컨벤션 / 경합
- **잠긴 결정 재논의 금지**(§5). §13.1 만 사실오류로 정정(설계의도 불변).
- **커밋 = 사용자 승인 후**(무단 금지). **path-scoped `git add <경로>`**(`-A`/`.` 금지 — VFX/Fog 휩쓸지 말 것).
- **main.cpp 미접근**(Fog 경합) / **EnemyFactory.h 는 Task 5 가 소유**(EnemyBuilder 안 건드림) / **Timer 코어 불가침**(`src/timer/`) / **셰이더 billboard_atlas.{vs,fs} 무수정**(완성).
- `no_auto_tests`(단위테스트 금지, 검증=빌드+GUI). 주석 한국어. 헤더가드 `__XXX_H__`(#pragma once 금지). long 금지(고정폭). 경로 슬래시. `Co-Authored-By` 트레일러 미사용(프로젝트 컨벤션).
- ctor 에서 sink/virtual 해소 금지 → OnEnter. 액터당 IActorPresentation/Life 1구현.

## 11. 실행 카덴스 (subagent-driven-development + 프로젝트 override)
서브에이전트 구현(no commit, no test, sonnet, full task text) → 오케스트레이터가 **보고 불신·diff 직접검증**(spec 준수) → (무거운 Task 5/6 은 별도 quality 리뷰) → diff 요약 제시 → 사용자 승인 → 커밋(EnemyContactHandler/Carrier 등 자기 파일만). GUI-verify Task=3/5/6/7(사용자 실행), 빌드-only=4.

## 12. 변경 기록
| 일자 | 변경 |
|---|---|
| 2026-06-02 | 재개 컨텍스트 무손실 이주 — Task 0~3 커밋 완료(HEAD f1e1a23) 확인, 병렬작업(Timer/EnemyBuilder+Wave/FX A·B/effekseer) reconcile, Life optional<Timer> 계약보존 확인, 커밋갭 닫힘, 다음=Task3 GUI검증→Task4. 구 실행핸드오프(2026-06-01) 대체. spec §5.3/FX §4.2 stale 정정 반영. |
