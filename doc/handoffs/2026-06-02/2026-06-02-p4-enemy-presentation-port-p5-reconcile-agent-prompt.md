# Handoff — P4 (적 IActorPresentation [C] 포트 통합) + P5 (PlayerBehavior 철거 조율)

> **수신자**: PlayableDirector 연출 foundation(P0~P2 완료) *위에서* 적 연출을 포트로 통일하려는 다음 Claude Agent. 분해 Task 5/6/7 트랙과 병행/후행.
> **목적**: 역할별 분리 규칙(아래) 하에서 **적도 `IActorPresentation` [C] 포트(PlayableDirector)** 를 갖게 해 player 와 대칭으로 만들고(P4), **PlayerBehavior god-component 철거**(P5=분해 Task 7)를 조율.
> **작성**: 2026-06-02. 브랜치 `game/module/ingame/temp`. 작성시 HEAD `d44a4a3 [dev] : impulse`.
> ⚠ **이 repo 는 다중 에이전트 병렬 커밋**(foundation / VFX·Effekseer / Fog·PostFX / EnemyBuilder / 분해 / hit-FX). **착수 즉시 `git log --oneline -10` + `git status --short` 실측**, 그리고 아래 `file:line` 을 **직접 재검증**(드리프트 가정 금지).
> **산출 경로 규칙**: 추적 문서는 `doc/`(단수). `doc/`(복수)는 gitignore.

---

```
[ROLE]
너는 C++17/CMake OpenGL 탑다운 슈터(/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics, 브랜치 game/module/ingame/temp)의 구현 에이전트다.
이번 작업: (P4) 적 엔티티에 IActorPresentation [C] sink(PlayableDirector)를 부착해 적 피격/사망 *엔티티 연출*(hit-flash/dissolve)을 player 와 동일한 포트로 통일. (P5) PlayerBehavior 철거 = 분해 Task 7 — 본 핸드오프는 *조율/축소된 범위*만 전달(주 소유는 분해 트랙).
철학(사용자 확정): "게임 로직(HP/damage/physics/AI/입력판정) 제외, 모든 연출/비주얼/사운드는 PlayableDirector 경유 Playable 로만." P0~P2 가 이 foundation 을 이미 정착시켰다(아래).

[절대 규칙 — 위반 시 작업 거부]
- 커밋/git add 금지(사용자 승인 후). path-scoped `git add <경로>` 만 — `-A`/`.` 금지(병렬 에이전트 dirty 휩쓸기 방지).
- `Co-Authored-By` 트레일러 **미사용**(프로젝트 컨벤션). 커밋 메시지 한국어.
- 주석 한국어. 헤더가드 `__XXX_H__`(#pragma once 금지). `long` 금지(int32_t/uint64_t 등 고정폭). 경로 슬래시. ctor 에서 sink/virtual 해소 금지 → OnEnter.
- 단위테스트 자동 추가 금지(no_auto_tests). 검증 = 빌드 exit0 + 실행(GUI) 육안.
- **액터당 IActorPresentation/Life 1구현**(RD2). 적 director 부착 시 적이 IActorPresentation 을 2개 갖지 않도록(현재 적은 0개 — 안전).

[검증된 사실 — P0~P2 완료 상태 (이번 세션, 미커밋, 커밋 미승인)]
- **역할별 분리 규칙(정통, spec §1.5)**: 연출 Playable 의 소유·tick·정리는 역할로 3분:
  · **[C] 트리거 이벤트 연출**(fire/hit/death/attack) → `PlayableDirector.Register(key)` + 중앙 tick, verb→`Play(key)`.
  · **[A] 엔티티 상시 루프**(스프라이트 애니/idle tween/BGM) → 영속 actor 의 scene-Component(`AddComponent`+`SetIsLoop(true).Play()`).
  · **[B] 월드점 fire-and-forget**(피격 spark/사망 폭발) → `Spawns/` 임시 actor+`AutoDespawnOnFinish`+`SweepFinishedChildren`, **도메인 seam(`Life::mOnDeathFx`/`onHitFx` delegate)이 트리거**.
  정본: `doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md` §1.5 (gitignore — 없으면 본 핸드오프로 충분).
- **PlayableDirector 는 순수 [C]**: `apps/_MyApp_/src/Playable/PlayableDirector.{h,cpp}` (`TopdownShooter::Playable`). `: SJH::Scene::Component, public Entity::IActorPresentation`. API: `Register(key, unique_ptr<PlayableBase>)` / `Play(key)`(=Stop()+Play(), 미등록=silent no-op) / `Stop(key)` / `Has(key)` + `Update`(슬롯별 `playing` 플래그로 Play 된 것만 tick — autoplay 방지). verb→`ReactDamaged→Play("hit")` / `ReactDied→Play("death")`(P2 에서 월드점 spawn 제거 — 순수 C) / `ReactAttack→Play("attack")`. `SetFacing/SetPose` 빈 훅(분해 Task6). **P2 에서 `SetSpawnContext`/`mCtx`/Spawns 의존 제거됨**(director 가 [B] 안 함). 라이브러리 = 신규 `MyApp::Playable`(STATIC), `MyApp::Client` 우산 합류.
- **BulletSpawnPlayable 삭제 완료(P1)**: `Entity/Player/BulletSpawnPlayable.{h,cpp}` + Entity CMake 엔트리 제거(게임로직-in-Playable 위반·dead code). → **분해 Task 7 범위 축소: 이제 `PlayerBehavior.{h,cpp}` + `PlayerMovementComponents.h` 만 삭제 대상**.
- **잠긴 설계와 정합(중요 — 오해 금지)**: 역할별 분리는 분해 §13.2 의 "spawn-at-point(onDeathFx/onHitFx delegate) 는 IActorPresentation sink 와 별개" 와 **일치**한다. → **delegate(`onDeathFx`/`onHitFx`)는 [B] 도메인 seam 이므로 *유지*. 절대 은퇴/제거하지 마라**(M6-Task9 가 이걸로 적 폭발/spark 를 트리거). EnemyDeathHandler→Life::SetOnDeathFx 흡수는 분해 **Step 7 OPTIONAL·사용자 승인 필수** — P4 범위 아님.

[검증된 사실 — 적 현 연출 seam (this-pass 재검증 필수)]
- 적은 **IActorPresentation [C] sink 가 0개** → `Life::mSink` 항상 null → 적의 `ReactDamaged`/`ReactDied` 무효(=hit-flash/dissolve 엔티티 연출 부재). player 만 PlayableDirector 부착(`PlayerBuilder.cpp`). ← **P4 가 닫을 비대칭 갭**.
- 적 [B] 월드점 FX 는 delegate 로: `EnemyConfig.onDeathFx`([EnemyFactory.h:25]) → `if(cfg.onDeathFx) AddComponent<EnemyDeathHandler>`([:55-56]) → `EnemyDeathHandler` self-poll([EnemyDeathHandler.cpp:6-16])이 `!Life::IsAlive()` 시 `mOnDeathFx(pos)`(=`Spawns::SpawnEnemyDeathFX`) + `SetActive(false)`. 피격 spark 는 총알측 `BulletContactHandler::SetOnHitFx`([BulletContactHandler.h:30-31]). **이 delegate 배선은 M6-Task9 핸드오프 소유** — `doc/handoffs/2026-06-02/2026-06-02-enemy-hit-death-fx-wiring_m6task9.md`.
- 적 부트스트랩 = `Bootstrap/EnemyBuilder.cpp::BuildEnemy(const EnemyDeps&)` — `CreateEnemyActor(cfg)`(EnemyFactory, 무변경) 위에 SpriteRenderer + SpriteSequencePlayable[A] + idle ParallelPlayable(scale∥rot)[A] 를 얹는다. **EnemyFactory.h 는 건드리지 않는다**(Task 5 소유). ← **P4 의 적 director 부착 지점은 여기(EnemyBuilder)** — EnemyFactory.h 미접근.
- `Life`([Entity/Components/LifeComponents.h]): `DoDamaged`→`mSink->ReactDamaged(dmg)`(line 98) / `DoDie`→`mSink->ReactDied(pos)`(line 112) **이미 호출**(구현체만 붙이면 됨). `SetIFrameSeconds`/`SetDeathDelaySeconds`/`SetOnDeathFx` 존재. **Life 수정 금지**(분해 소유).

========================================================================
[P4] 적 IActorPresentation [C] 포트 통합 (EnemyBuilder 에서 부착)
========================================================================
목표: 적도 player 처럼 `PlayableDirector`(또는 동등 IActorPresentation 구현)를 부착해 `ReactDamaged→Play("hit")` / `ReactDied→Play("death")` 가 **적 엔티티 연출**(SpriteRenderer hit-flash / dissolve)을 구동. [B] delegate 는 그대로 둔다([C] 와 [B] 공존 — 같은 사망 이벤트가 화면연출[C]+월드폭발[B] 둘 다 가능, 의도된 분리).

선행 의존: **분해 Task 6 가 player 의 hit-flash/dissolve Playable 패턴을 먼저 정착**시켜야 그걸 적에 미러링 가능(SpriteRenderer FX uniform enableHit/dissolveThreshold 는 commit `1a9196c` 로 이미 존재). → **P4 는 Task 6 이후**.

STEP P4-1: `EnemyBuilder.cpp::BuildEnemy` 에서 `enemy->AddComponent<TopdownShooter::Playable::PlayableDirector>()` (sprite/Life 부착 뒤, dir.Root().AddChild 전 — Life::OnEnter 가 sink 캐시하도록 부착이 enter 보다 먼저). PlayerBuilder.cpp 의 부착 패턴을 그대로 참조.
STEP P4-2: 적 "hit"/"death" [C] Playable 등록 — Task 6 이 player 용으로 만든 hit-flash/dissolve Playable(SpriteRenderer FX uniform 구동)을 적 SpriteRenderer 대상으로 `Register("hit"/"death", ...)`. (Task 6 의 Playable 클래스를 재사용; 신규 게임로직 금지.)
STEP P4-3: 빌드+실행 — 적을 쏘면 hit-flash, HP0 시 dissolve(+기존 [B] 폭발/사망음/despawn 유지). 회귀: 적 스폰/추적/기존 delegate FX 동작 불변.
- **CMake**: PlayableDirector 부착은 `MyApp::Bootstrap` 이 `MyApp::Playable` 를 link 해야 함(PlayerBuilder 가 이미 그러함 — Bootstrap/CMakeLists.txt 확인, 미링크면 PRIVATE 추가).
- **주의**: 적 director 가 [B] 를 트리거하지 않게(순수 C 유지) — 폭발/spark 는 delegate 소유(M6-Task9). director 는 화면/스프라이트 연출만.

========================================================================
[P5] PlayerBehavior 철거 = 분해 Task 7 (조율 — 주 소유는 분해 트랙)
========================================================================
- P5 의 실체 = 분해 **Task 7**. 단일 진입점 = `doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-task4-done-resume-handoff.md`(HEAD `d44a4a3`, Task 0~4 커밋 / 5·6·7 미착수). Task 7 원본 범위 = `PlayerBehavior.{h,cpp}` + `BulletSpawnPlayable.{h,cpp}` + `PlayerMovementComponents.h` 삭제 + Entity CMake + grep 0참조.
- **이번 세션 P1 이 `BulletSpawnPlayable.{h,cpp}` 를 이미 삭제** → **Task 7 잔여 = `PlayerBehavior.{h,cpp}` + `PlayerMovementComponents.h` + Entity CMake `PlayerBehavior.cpp` 줄 제거 + grep 0참조**. (BulletSpawnPlayable 줄은 P1 에서 이미 제거됨.)
- 선행: Task 7 은 Task 5(Carrier — EnemyContactHandler/BulletContactHandler 흡수)·Task 6(PlayerSpriteDirector→PlayableDirector 통합) 이후. PlayerBehavior 는 현재 dead(부착 0회)라 삭제는 안전하나, 분해 카덴스(서브에이전트 구현→오케스트레이터 검증→사용자 커밋) 준수.
- **P5 는 독립 실행하지 말고 분해 Task 7 에 합류**시켜라(중복/충돌 방지). 본 핸드오프는 "BulletSpawnPlayable 선삭제로 범위 축소" 사실만 분해 트랙에 전달.

========================================================================
[스코프 경계 + 충돌 매트릭스]
========================================================================
| 파일 | 소유 | P4/P5 행동 |
|---|---|---|
| `Bootstrap/EnemyBuilder.cpp` | EnemyBuilder 트랙(+이번 P4) | P4 director 부착 — 해당 트랙과 조율(공유 편집점) |
| `EnemyFactory.h` | **분해 Task 5** | **미접근**(P4 는 EnemyBuilder 에서만 부착) |
| `Entity/Components/LifeComponents.h` | 분해(Task 2/6) | **미접근**(sink 호출 이미 있음) |
| `BulletContactHandler`/`EnemyContactHandler` | 분해 Task 5(Carrier 흡수) | **미접근**(delegate 유지) |
| `PlayerBuilder.cpp` | foundation+Task6+hit-FX 3자 공유 | P4 부착 패턴 *참조만*; 편집은 surgical |
| `main.cpp` | Fog/PostFX/foundation 경합 | **미접근** |
| `src/timer/*`, 셰이더(billboard_atlas/grayscale_vignetting), `Spawns/` 코어 | 불가침 | **미접근** |
| `PlayerBehavior.{h,cpp}`/`PlayerMovementComponents.h` | 분해 Task 7 | P5 = Task 7 에 합류(독립 삭제 금지) |
- delegate(`onDeathFx`/`onHitFx`) **은퇴 금지**([B] 도메인 seam — M6-Task9 + 분해 Step7 승인). PlayerBehavior/FX 슬롯 **부활 금지**.

[검증]
- 빌드: `cmake --build --preset ninja --target _MyApp_` → exit 0(경고는 기존 sb7 gl3.h #warnings 만 허용).
- 실행: `cd build_ninja/apps/_MyApp_ && ./_MyApp_` → 적 스폰/추적 정상, 적 피격 시 hit-flash, 사망 시 dissolve + (기존)폭발/사망음/despawn. 크래시 0. 입력 전 자동재생(spurious FX) 없음.
- ⚠ Effekseer `.efk` 로드 실패(muzzle 등)는 별개 VFX 트랙 — 무시. 자산(spark/explosion/event) 미존재면 [B] 는 안전 no-op.

[Self-review]
- 적에 IActorPresentation 1개만(PlayableDirector) 부착? Life::mSink 가 그걸 잡나? [C] hit/death 만 등록(게임로직 0)?
- delegate(onDeathFx/onHitFx) 그대로 유지? EnemyFactory.h/Life/main.cpp/Timer/셰이더 미접근?
- P5 를 독립 삭제하지 않고 분해 Task 7 에 합류(BulletSpawnPlayable 선삭제 사실 전달)?
- 빌드 exit0 + 회귀0(기존 적 FX/스폰 불변)?

[보고]
DONE/DONE_WITH_CONCERNS/BLOCKED + 변경 파일별 요약 + 빌드 마지막 줄 + 실행(적 hit/death 연출 + 기존 delegate FX 회귀) + 분해 Task 5/6/7 조율 메모 + git status. **커밋 금지**(사용자 승인).
```

---

## 사용 메모 (오케스트레이터/사용자용)

- **P4 는 분해 Task 6 이후** 착수 권장(player hit-flash/dissolve Playable 패턴을 적에 미러링). Task 6 = PlayableDirector foundation(P0~P2 완료) 위에서 진행.
- **P5 는 분해 Task 7 그 자체** — 별도 에이전트로 빼기보다 분해 트랙(`doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-task4-done-resume-handoff.md`)에 "BulletSpawnPlayable 선삭제(P1)로 Task 7 범위 축소" 를 전달하고 거기서 처리하는 것이 충돌이 적다. 이 프롬프트의 P5 절은 그 조율 메모.
- **핵심 정정(이전 논의 대비)**: "delegate 은퇴 + 포트 단일화" 는 **틀린 방향**이었다. 잠긴 분해(§13.2)·역할별 분리 모두 [C] 포트와 [B] delegate 를 *의도적으로 분리*한다. P4 는 적에 [C] 를 *추가*할 뿐 [B] delegate 를 없애지 않는다.
- 관련 핸드오프: M6-Task9(적 [B] FX delegate 배선) `doc/handoffs/2026-06-02/2026-06-02-enemy-hit-death-fx-wiring_m6task9.md` / 분해 재개 `…task4-done-resume-handoff.md` / foundation DONE `…playable-director-foundation-DONE-resume-handoff.md` / Task6 프롬프트 `…task6-on-director-foundation-agent-prompt.md`.
- 권장 커밋 메시지(승인 시): `[refactor] : 적 IActorPresentation [C] 포트 통합(P4) — hit/death 엔티티 연출 director 경유` (Co-Authored-By 미사용).
