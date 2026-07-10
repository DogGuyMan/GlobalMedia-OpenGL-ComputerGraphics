> 🔴 **SUPERSEDED (2026-06-03) → 단일 진입점: [`2026-06-03-pb-decomposition-COMPLETE-resume-handoff.md`](2026-06-03-pb-decomposition-COMPLETE-resume-handoff.md)**
> Task 6 directional = **committed `a16eef0`**. **Task 5(Carrier)+Task 7(PB 철거) = 커밋 `f45211c "deadcode 제거"`** + 빌드 GREEN(오케스트레이터 재검증 exit0). → **PlayerBehavior 분해 Task 0~7 = 코드 완료 + 빌드 GREEN, god-component 제거 확정.** 남은 것 = GUI 검증 + cull fix(`pass.h`, `M` 미커밋) 단독 커밋. 아래 §2 Task표/§5 다음작업은 stale. 최신 상태는 후속 문서 참조.

# Handoff — PlayerBehavior 분해 *재개 컨텍스트* (2026-06-02, foundation+directional 진척 갱신)

> **이 문서 = 다음 세션의 단일 진입점.** PlayerBehavior god-component 분해(Task 0~7)를 subagent-driven 으로 실행 중. **Task 0~4 + 연출 foundation + hit-FX = 커밋 완료. Task 6 directional = working tree 구현 완료·미커밋(검증/커밋 대기). Task 5(Carrier) = 부분 착수. Task 7 = 미착수.**
> **작성**: 2026-06-02. 브랜치 `game/module/ingame/temp`. HEAD `6adaf02`.
> **이 문서가 대체(supersede)**: `doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-task4-done-resume-handoff.md`(HEAD d44a4a3 시점 — foundation "미커밋"·Task6 "미착수" 로 stale). 그 문서의 §2(Task4 상세)·§4(잠긴결정)·§7(가드레일)·§8(문서인덱스)는 여전히 유효 — *깊이* 가 필요하면 참조.
> ⚠ **이 repo 는 여러 에이전트가 병렬 커밋 중**(directional / World Text / VFX / Fog). HEAD·working tree 가 *수시로* 바뀐다(이 문서 작성 중에도 d44a4a3→8f276a9→6adaf02 로 6+ 커밋 전진). 시작 시 **반드시 `git log --oneline -12` + `git status --short` 실측** 후 아래 표를 보정.

---

## 0. 한 눈에 (TL;DR)

- **하는 일**: 죽은 god-component `PlayerBehavior` 를 `Entity/Components` 로 분해 — Dash→Impulse / i-frame+Die→Life / 방향스프라이트→(PlayableDirector 통합) / 데미지배달→Carrier / 엔진 GetComponent 인터페이스 조회.
- **진행**: **Task 0·1·2·3·4 커밋 ✅. 연출 foundation(PlayableDirector+PostFXRegistry+hit-FX SpriteFx/Dissolve/HpGrayscale) 커밋 ✅(`8f276a9`+`aa684f7`). Task 6 directional = working tree 구현 완료·미커밋 🟡. Task 5(Carrier) = Projectile base 커밋·흡수 미완(부분) 🟡. Task 7 미착수 ⬜.**
- **다음 행동**: ① **Task 6 directional 빌드+GUI 검증 → 커밋**(working tree 에 구현돼 있음 — 전용 핸드오프 `doc/handoffs/2026-06-02/2026-06-02-directional-facing-pose-resume-handoff.md`) → ② **Task 5(Carrier) 흡수 완성**(5a bullet→Projectile / 5b EnemyContactHandler→ContactCarrier + 둘 삭제) → ③ **Task 7**(PlayerBehavior/BulletSpawnPlayable 철거). (Task 3 GUI 검증도 여전히 공식 미기록 — 적 접촉 즉사 확인.)
- **정본**: spec `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md` + plan `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` (**둘 다 gitignore=로컬 전용**) + 설계 핸드오프 `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md`(추적).
- **카덴스**: Task별 게이트(서브에이전트 구현 no-commit → 오케스트레이터 diff 직접검증 → 사용자 승인 커밋). `no_auto_tests`→빌드+GUI 검증.

---

## 1. git 현실 (작성 시점 — 재측정 필수)

```
HEAD: 6adaf02 [dev] : World Text — clangd 정리
브랜치: game/module/ingame/temp
최근 커밋 체인(신→구):
  6adaf02 World Text — clangd 정리(sign-conversion 캐스트 + include)
  e3c27df World Text — SJH::engine 우산에 SJH::text 합류 (16→17 모듈)
  d173298 World Text — TextRenderer(글리프 child SpriteRenderer 조립)
  10825cc World Text — Core SJH::text 모듈 + BitmapFont(BMFont 로더)
  aa684f7 ★ PlayableDirector 연출 통합 (Player/Enemy 빌더 배선) + sprite/PostFX 엔진 지원  ← foundation 와이어링 커밋
  ab63672 Doxygen 문서 시스템 도입
  (그 아래) 8f276a9 [dev] : 연출 디렉터 (MyApp::Playable 신규 lib)  ← foundation Playable/ 소스 커밋
  (그 아래) d44a4a3 impulse(Task4) / 8b5fc45 rename / f1e1a23 Task0~3+Timer
```

**working tree 미커밋**:
```
내 분해 — Task 6 directional (구현 완료·미커밋, 검증/커밋 대기):
   M apps/_MyApp_/src/Playable/PlayableDirector.{h,cpp}   (DirGroup/RegisterGroup/SetFacing/SetPose/Apply)
   M apps/_MyApp_/src/Playable/Constants.h                (PLAYER_FACING_THRESHOLD 등 directional 상수)
   M apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp         (:76~ 8그룹 child 빌드 + RegisterGroup)
   M apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}(:193-212 RD5 push, QuantizeByThreshold)
병렬 에이전트 (NOT MINE, 미접근):
   ?? apps/_MyApp_/src/Text/        (World Text Client — 진행중)
   m extern/Catch2 (submodule, 잡음)
```
- **커밋된 분해/foundation = Task 0~4 + Playable/ 소스(8f276a9) + 연출 배선/엔진지원(aa684f7).**
- **⚠ 구 task4-done 문서 대비 변화**: foundation "미커밋"→**커밋됨**(8f276a9+aa684f7). Task 6 "미착수"→**working tree 구현 완료**. World Text(SJH::text 17번째 코어모듈) 신규 합류. Doxygen 시스템 도입.

---

## 2. Task 0~7 상태표 (재측정)

| Task | 내용 | 상태 | 커밋/검증 |
|---|---|---|---|
| **0** | 엔진 `GetComponent<T>` 인터페이스 조회(+`if constexpr(is_base_of<Component,T>)` 가드) | ✅ 커밋 | `78ca9ee`(+`1f6c23f`). RD5 sink 이 이걸로 동작(PlayerController.cpp:195 `GetComponent<IActorPresentation>`) |
| **1** | `IActorPresentation`+`IImpulsable`+`EFacing`/`EPose`/`Quantize4` | ✅ 커밋 | `82735ed` |
| **2** | `Life` 확장(i-frame+DoDie+sink) → Timer 마이그레이션 `optional<Timer>`(계약 보존) | ✅ 커밋 | `1f6c23f`→`f1e1a23` |
| **3** | `EnemyContactHandler`→`Life::DoDamaged`(IDamageable) repoint | ✅ 커밋 | **⚠ GUI 실행 검증 공식 미기록**(적 접촉 즉사 확인 필요) |
| **4** | `Impulse`(`Physics/PhysicsImpulse.h`, IImpulsable, DashForce/CoolDownSpeed Stat) 미배선 ship | ✅ 커밋 | `d44a4a3` |
| **foundation** | PlayableDirector + PostFXRegistry + hit-FX(SpriteHitFlash/Dissolve + PostFXTween + HpGrayscalePostFX) + Player/Enemy 빌더 배선 | ✅ 커밋 | `8f276a9`(소스) + `aa684f7`(배선/엔진지원). "fire"/"hit"/"death" key 등록 |
| **6** | directional 8그룹(4방향×2포즈) Visible 토글 — PlayableDirector 흡수 + RD5 push | 🟡 **working tree 구현 완료·미커밋** | PlayableDirector DirGroup/SetFacing(.h:39-58) + PlayerBuilder:76-121 8그룹 + PlayerController:193-212 RD5(QuantizeByThreshold 히스테리시스). **빌드+GUI 검증 + 커밋 대기**. 전용 핸드오프 ↓§5.1 |
| **5** | `Carrier` base+`Projectile`/`ContactCarrier` — Bullet/Enemy contact handler 흡수·삭제 | 🟡 **부분 착수** | `Spawns/Projectile.h`(`Spawn::Carrier::Projectile : Component, IContactable, IDieable`) 커밋됨. 단 `BulletContactHandler.{cpp,h}`·`bullet_factory.h`·`EnemyContactHandler.{cpp,h}` 잔존 → 흡수(5a/5b) 미완 |
| **7** | `PlayerBehavior.{h,cpp}`+`BulletSpawnPlayable`+`PlayerMovementComponents.h` 철거 | ⬜ 미착수 | `PlayerBehavior.{cpp,h}` 잔존(`BulletSpawnPlayable` 은 8f276a9 에서 삭제 fold됨) |

---

## 3. 잠긴 설계 결정 (재논의 금지) — 구 task4-done §4 그대로 유효
- α: `GetComponent<T>` `is_polymorphic_v` 완화(인터페이스 조회). fast-path `if constexpr(is_base_of_v<Component,T>)` 가드(concrete=static_cast / 인터페이스=slow dynamic_cast). 설계 핸드오프 §13.1 "static_cast 오용 없음" 은 사실오류로 정정됨.
- RD1~RD6: Life 단일변이점이 `IActorPresentation` sink 로 forward(Template-Method). 액터당 sink 1 / 베이스 직접호출 / silent no-op / blanket 서브클래스 거부. **RD5: PlayerController 단일작성자 facing/pose**(PlayerController.cpp:193-212 실현).
- C1: Carrier `GetComponent<IDamageable>->DoDamaged` + `GetComponent<IImpulsable>->DoImpulse`(=Task 4 `Impulse` 넉백 타겟). **IImpulsable 신규**.
- 확정 4: i-frame=plain float→(Timer optional<Timer>, 계약 동일) / Components::Movement 유지 / 공격윈도 0.15s / Carrier 얇은 베이스.
- **directional(Task6) 잠긴 결정**: 방향=8그룹 Visible 토글(flipX/rebind 금지) / PlayableDirector 흡수(별도 PlayerSpriteDirector 금지) / pose 2축{Idle,Move}만. 그룹 숨김=`SpriteRenderer.Visible`(SetActive 아님 — 렌더+tick 둘 다 끔). **구조 제약(라이브검증)**: 8그룹 빌드는 PlayerBuilder(bootstrap, Entity+Playable link), PlayerController sink 은 `Entity::IActorPresentation*` 인터페이스(InputHandler 가 Playable 미link). → 전용 핸드오프 §4.
- **Life 실체**: `optional<Timer> mInvincibleTimer/mDieTimer`, 공개 계약 보존 → Task 6 영향 0. 현재 i-frame/사망지연 nullopt(즉사); 플레이어 0.5s 주입은 후속.

---

## 4. 이번 세션이 한 일 (2026-06-02)
1. **Task 4(Impulse) 커밋 검증** — `d44a4a3` spec §5.4 정확 일치 확인(PhysicsImpulse.h PascalCase, physics 분기 부착, 미배선 ship).
2. **Task 6 directional 무손실 핸드오프 생성**(기존 에이전트 기록 소실 → 재인계): agent-prompt + resume(↓§5.1). 워크플로(7-reader gather→synth→적대 critic) + 라이브 코드 교차검증으로 **구조 정정 2건**(그룹 빌드 PlayerActor→PlayerBuilder 링크사이클 회피 / sink IActorPresentation 인터페이스). 그 핸드오프가 **실행되어 directional 이 working tree 에 구현됨**(§2 Task6).
3. **HEAD drift 추적**: d44a4a3→8f276a9(foundation 소스)→aa684f7(배선)→World Text 4커밋→6adaf02. 본 문서/메모리 재측정 반영.

---

## 5. 다음 즉시 작업

### 5.1 Task 6 directional — 검증 + 커밋 (working tree 에 구현돼 있음)
구현 완료(미커밋). **단일 진입점 핸드오프**: `doc/handoffs/2026-06-02/2026-06-02-directional-facing-pose-resume-handoff.md`(+ 붙여넣기 `...-directional-facing-pose-agent-prompt.md`). 남은 일 = 빌드+GUI 검증 후 커밋:
```bash
cmake --preset ninja && cmake --build --preset ninja --target _MyApp_   # exit 0
cd build_ninja/apps/_MyApp_ && ./_MyApp_   # WASD 방향 따라 Front/Back/Left/Right + Idle/Move 전환 육안
```
- 커밋 범위(승인 후, path-scoped): `git add apps/_MyApp_/src/Playable/PlayableDirector.{h,cpp} apps/_MyApp_/src/Playable/Constants.h apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}`. `?? src/Text/`(World Text) 함께 stage 금지.
- 부호 트랩: W=-Z(Front=z>0). 반대면 controller velXZ/aimXZ 부호 우회(Components.Interfaces.h 읽기전용). 구현체는 `QuantizeByThreshold`(히스테리시스) 사용 — Quantize4 의 개선판.

### 5.2 Task 5 (Carrier) — 흡수 완성 (부분 착수됨)
`Spawns/Projectile.h`(`Spawn::Carrier::Projectile`) base 는 커밋됨. 남은 일:
- **5a**: `bullet_factory.h` 의 `BulletContactHandler`→`Carrier::Projectile` 로 교체, BulletContactHandler 삭제. BulletLifetime(Timer 기반, ctor 동일) + Projectile self-despawn 공존.
- **5b**: `EnemyFactory.h` 의 `EnemyContactHandler`→`ContactCarrier` + 둘 삭제. ⚠ `EnemyBuilder.cpp` 는 다른 에이전트 IDE 오픈 — **EnemyFactory 만 surgical**.
- Carrier = `GetComponent<IDamageable>->DoDamaged`(Task0 조회) + 넉백 `GetComponent<IImpulsable>->DoImpulse`(Task4 Impulse). plan Task5 전문 추출 디스패치(plan 파일 읽히지 말 것).

### 5.3 Task 7 — PlayerBehavior 철거
directional+Carrier 후. `PlayerBehavior.{h,cpp}`+`PlayerMovementComponents.h` 삭제 + Entity CMake 정리(BulletSpawnPlayable 은 이미 8f276a9 에서 삭제됨).

---

## 6. 병렬 트랙 충돌 매트릭스 (재측정)
| 트랙 | 상태 | 내 분해와 |
|---|---|---|
| **연출 foundation (PlayableDirector/PostFX/hit-FX)** | ✅ 커밋(`8f276a9`+`aa684f7`) | Task 6 의 sink=PlayableDirector. directional 이 그 위에 구현됨 |
| **Task 6 directional** | 🟡 working tree 구현·미커밋 | 내 작업 — PlayableDirector.{h,cpp}/PlayerBuilder.cpp/PlayerController.{h,cpp}/Constants.h |
| **World Text (SJH::text 17번째 모듈)** | ✅ 커밋(`10825cc`~`6adaf02`) + `?? src/Text/` 진행중 | 분해 무관 — 미접근 |
| **EnemyBuilder + WaveController** | ✅ 커밋, 적 스폰 live. EnemyBuilder.cpp IDE 오픈 | EnemyFactory 무변경 → **Task 5b 충돌0**(EnemyFactory 만 surgical, EnemyBuilder 미접근) |
| **Fog/PostFX / VFX(.efk)** | 진행중 | Task 5~7 main.cpp 미접근 → 충돌0. `.efk` 로드 실패는 분해 무관 |

---

## 7. 가드레일 / 컨벤션 (carry-forward)
- **커밋 = 사용자 승인 후**(무단 금지). **path-scoped `git add <경로>`**(`-A`/`.` 금지 — World Text/VFX/Fog 미커밋 휩쓸지 말 것).
- **`Co-Authored-By` 트레일러 미사용**(프로젝트 컨벤션).
- **미접근 경계**: `main.cpp`(Fog/foundation 경합) / `EnemyBuilder.cpp`(다른 에이전트 IDE 오픈 — Task5b 는 EnemyFactory 만) / `?? src/Text/`(World Text) / `Timer 코어 src/timer/`(불가침) / 셰이더 `billboard_atlas.{vs,fs}` / 코어 `src/playable`·`src/sprite`·`src/render`.
- **잠긴 결정 재논의 금지**(§3). 주석 한국어. 헤더가드 `__XXX_H__`(#pragma once 금지). `long` 금지(고정폭). 경로 슬래시. windows.h 는 `#ifdef _WIN32`+`NOMINMAX`. ctor 에서 sink/FindPhysics 해소 금지→`OnEnter`/lazy.
- **rename(`8b5fc45` snake→PascalCase)**: include 전 실제 파일명 확인. `no_auto_tests`(검증=빌드+GUI).

## 8. 정본 문서 인덱스
| 문서 | 경로 | git | 역할 |
|---|---|---|---|
| 분해 spec | `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md` | gitignore(로컬) | §13 잠긴결정 + §4 분해테이블 + 시그니처 |
| 분해 plan | `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` | gitignore(로컬) | Task 0~7 완전코드+검증/커밋 step |
| 설계 핸드오프 | `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md` | 추적 | §13/§14 잠긴결정(§13.1 정정됨) |
| **Task6 directional 핸드오프** | `doc/handoffs/2026-06-02/2026-06-02-directional-facing-pose-{agent-prompt,resume-handoff}.md` | 추적 | directional 단일 진입점 + 붙여넣기 프롬프트 |
| foundation spec | `doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md` | 로컬 | PlayableDirector 재편 |
| 구 task4-done 핸드오프 | `doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-task4-done-resume-handoff.md` | 추적 | 🔴 본 문서가 대체(Task4 상세·§4·§7 깊이만 참조) |
| **이 문서** | `doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-foundation-done-resume-handoff.md` | 추적 | **재개 단일 진입점** |
> spec/plan 이 다른 머신에 없으면(gitignore) 설계 핸드오프 §13/§14 + 본 문서로 재구성.

## 9. 변경 기록
| 일자 | 변경 |
|---|---|
| 2026-06-02 | **본 문서 생성** — task4-done 핸드오프 대체. 재측정: HEAD `d44a4a3`→`6adaf02`(6+ 커밋). foundation(PlayableDirector/PostFX/hit-FX) 커밋 완료(`8f276a9`+`aa684f7`). **Task 6 directional = working tree 구현 완료·미커밋**(전용 핸드오프 실행됨, RD5+8그룹+QuantizeByThreshold). Task 5 Carrier 부분 착수(Projectile base 커밋, 흡수 미완). World Text(SJH::text 17모듈)·Doxygen 신규 병렬 합류. 다음=Task6 검증·커밋→Task5 흡수→Task7. |
