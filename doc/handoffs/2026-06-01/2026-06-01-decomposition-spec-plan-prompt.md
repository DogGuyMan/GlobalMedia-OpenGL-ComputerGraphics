# PlayerBehavior 분해 — Spec+Plan 핸드오프 프롬프트 (다음 Claude Code용)

> 아래 코드블록을 다음 Claude Code 세션의 *첫 메시지*로 붙여넣어 사용. 설계는 이미 잠김(brainstorming 완료) → 이 task는 **Spec + Plan 직렬화**(구현 X).

```
[ROLE]
OpenGL Computer Graphics — C++17 / CMake 단독(macOS Ninja + Windows MSVC) — 의 탑다운 슈터 `apps/_MyApp_` 를 담당하는 게임플레이 아키텍처 Claude Agent.
역할 = **"PlayerBehavior god-component 분해" 의 Spec + Plan 수립** (brainstorming → spec → writing-plans 중 *brainstorming 은 이미 완료, 모든 설계 결정 잠김*. 따라서 본 task = spec + plan 직렬화).
**구현(코드 작성)은 본 task 범위 밖** — 사용자 승인 후 별도 세션.
주석·소통 한국어. superpowers 워크플로우(writing-plans)를 따른다.

[FIRST READ — 반드시 순서대로]
1. `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md`  ← **정본 설계. §13(확정 기구) + §14(빌드순서 7스텝) + §4(분해 테이블) = 잠긴 결정. 최우선 정독.**
2. `.claude/CLAUDE.md`                                        ← 아키텍처/빌드/컨벤션 정본
3. `.agents/skills/clean-ddd-hexagonal/SKILL.md`             ← 평가 렌즈 (ports/adapters, 의존 inward, anti-explosion)
4. `src/scene/actor.h`                                        ← Component / AddComponent(typeid 키+dup assert) / GetComponent(line 92·162 enable_if 게이트 = Step 0 완화 대상, 179-193 slow-path dynamic_cast)
5. `apps/_MyApp_/src/Entity/Components/{Components.Interfaces.h, LifeComponents.h, MovementComponents.h, WeaponComponents.{h,cpp}}`
6. `apps/_MyApp_/src/Entity/Player/{PlayerBehavior.{h,cpp}(분해 원본·dead), PlayerActor.{h,cpp}, PlayerMovementComponents.h(shadow버그·삭제), PlayerHand.{h,cpp}}`
7. `apps/_MyApp_/src/Bootstrap/PlayerBuilder.{h,cpp}` + `apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}`(UpdateAim=조준 단일권위, OnFirePressed)
8. `apps/_MyApp_/src/Physics/{PhysicsComponent.h(73 stale 주석), PhysicsMovement.h, ContactListener.cpp(27-28 ForEach+dynamic_cast 정통)}`
9. `apps/_MyApp_/src/Algebraic/{Stat.h, Algebraic.Common.h}`  ← ENumericStatType: **DashForce(12)·CoolDownSpeed(41)**·MaxHp·MoveSpeed·Tenacity 등 이미 존재
10. `apps/_MyApp_/src/Spawns/Projectile.h`                    ← `Carrier::Projectile` 스켈레톤(Component+IContactable+IDieable)
11. `apps/_MyApp_/src/Playable/Constants.h`                   ← 8방향 PlayerTextureConfig 벡터(IDLE/MOVE만, B-part만 ColCount>1)
12. `apps/_MyApp_/src/Entity/Enemy/{EnemyContactHandler.cpp(13-14 dead PB::Hit → 접촉뎀 no-op = load-bearing 수정), EnemyFactory.h, EnemyDeathHandler.h}`
13. (배경) `doc/handoffs/2026-06-01/2026-06-01-{player-wasd-4direction-sprite-switching, raycast-hand-vfx-handoff}.md`

[CURRENT STATE — 검증된 사실, 추정 아님]
- 브랜치 `game/module/ingame/temp`. 빌드 green (`cmake --build --preset ninja --target _MyApp_` exit 0).
- `PlayerBehavior` = **dead code** (`AddComponent<PlayerBehavior>` 0회). 그 단일아틀라스 `PlayClip(EPlayerClip)` 모델은 출시된 4-레이어 directional 스프라이트와 구조적 비호환.
- 플레이어 live 경로: **4-레이어 directional 스프라이트**(`CreatePlayerActor` PlayerActor.cpp:83-118, FRONT_MOVE 1방향만 PlayerBuilder 배선) + **raycast 조준**(`PlayerController::UpdateAim`) + **`Weapon::UseWeapon(box2dForward)` 발사** + `PlayerHands`. 이동 = `Physics::PhysicsMovement`(b2Body).
- M6 단발 FX 인프라(`Spawns/`: AudioInstance/VfxInstance/CombatSequences/AutoDespawnOnFinish/OneShotSweeper) 완료·커밋.
- `Actor::GetComponent<T>` 가 `enable_if<is_base_of_v<Component,T>>` 게이트 → **인터페이스 조회 컴파일 불가** (Step 0 에서 `is_polymorphic` 로 완화).
- `EnemyContactHandler` 가 dead `PlayerBehavior::Hit` 호출 → **적 접촉 데미지 현재 no-op** (Step 3 repoint 로 수정).
- **모든 설계 결정 잠김** (핸드오프 §13): 포트해소 α / RD1 OPT-1(IActorPresentation sink) / RD2~6 / Carrier C1~4 / facing 규칙. → *재논의 금지, 직렬화만*.

[TASK — PlayerBehavior 분해 Spec + Plan]
핸드오프 §13(확정 기구) + §4(분해 테이블) + §14(빌드순서)를 **구현 가능한 spec + plan 으로 직렬화**한다.
산출물 2종:
 1. design spec → `doc/superpowers/specs/2026-XX-XX-playerbehavior-decomposition-design.md`
 2. implementation plan → `doc/superpowers/plans/2026-XX-XX-playerbehavior-decomposition.md` (Task 분해 + 빌드/실행 검증 step + 사용자 commit blocking step)
 (구현 안 함 — plan 끝에 "다음 세션 구현 진입점" 명시)

[PHASE 분해 — 순서 준수]
Phase 0 [확인·일체화]: 핸드오프 §13/§14 정독 + 현 코드와 시그니처 정합 검증(드리프트 0). 잠긴 결정을 재진술해 사용자와 일체화. (재결정 X — 정합 확인만.)
Phase 1 [spec]: §13 결정 직렬화 + §4 분해 테이블 + clean-ddd 매핑 + 4-엔진 정통 매핑(GetComponent<Interface>=Unity, Carrier=hitbox/hurtbox 정통, Template-Method 연출 훅) + 명시적 비스코프. self-review(placeholder/일관성/시그니처 드리프트).
Phase 2 [plan]: superpowers:writing-plans 로 §14 빌드순서(Step 0~7)를 Task 로 분해. 각 Task = 파일 경로 + 코드 + **빌드 검증(`cmake --build --preset ninja --target _MyApp_`, exit 0)** + (해당 시)실행 시각 검증 + **사용자 commit 트리거 blocking step**. [[no_auto_tests]] — 단위 테스트 step 금지, 빌드+실행 검증으로 대체.

[수용 기준 — 모든 항목 만족]
- spec 이 §13 전 결정(포트α / RD1~6 / Carrier C1~4 / facing)을 커버 + 각 시그니처가 실제 헤더와 정합.
- plan Task 가 §14 Step 0~7 전부 다룸: (0)GetComponent 게이트 완화+PhysicsComponent.h:73 주석 (1)IActorPresentation+IImpulsable (2)Life 확장(i-frame+DoDie+sink) (3)EnemyContactHandler repoint=load-bearing (4)Impulse+속도싸움 (5)Carrier base+subtypes 흡수 (6)PlayerSpriteDirector 8그룹+RD5 (7)PlayerBehavior/BulletSpawnPlayable/PlayerMovement 삭제.
- 코드 0줄 수정 (문서 2종만).

[금지 사항 — 이미 잠긴 결정, 재논의 없이 따를 것 (핸드오프 §10/§13)]
- 결정 재논의 금지(RD1~6 / 포트α / Carrier / facing — §13 확정).
- functional(std::function) 이벤트 등록으로 OnXXX 연출 금지 → **Template-Method virtual + `IActorPresentation` sink**(IContactable 式 defaulted no-op, 스프라이트/FMOD 타입 0).
- **blanket per-entity 서브클래스 금지(RD4=나)** — 공유 Life/Move/Weapon 베이스 + Stat=factory config + 연출=sink. **`PlayerMovement` 삭제**.
- `IMovable` 오버로드 금지 → 신규 `IImpulsable`.
- `PlayerBehavior` 부활 금지. 단일아틀라스 `PlayClip` 으로 방향 표현 금지(4-레이어 `SpriteRenderer.Visible` 토글).
- 자산 없는 Attack/Hit/Die 스프라이트 상태 추가 금지(있으면 방향별 Idle/Move 재활용).
- pose 작성자 2명 / Impulse·PhysicsMovement `SetLinearVelocity` 충돌 방치 금지(controller 가 `Impulse::IsActive()` 중 `DoForward` suppress).
- 단위 테스트 자발 추가 금지([[no_auto_tests]]).
- `doc/`(gitignore)에 추적 문서 금지 — 핸드오프/추적은 `doc/`(단수).

[컨벤션 가드레일]
- Client 파일명 PascalCase / engine(`src/`) snake_case. 헤더가드 `__XXX_H__`/`_TOPDOWNSHOOTER_XXX__`, `#pragma once` 미사용.
- 크로스플랫폼: `long` 금지(고정폭), 경로 슬래시, `windows.h`는 `#ifdef _WIN32`.
- 자원 = `SJH::ResourceRegistry` 위탁. `Spawns`→`Entity` 단방향(순환 회피, std::function delegate 정통). FMOD dynamic → game_deps 타겟 POST_BUILD dll copy.
- ctor 에서 virtual OnXXX/sink 해소 금지 → `OnEnter`(post-AddComponent). 액터당 Life-family/IActorPresentation 정확히 1구현.

[체크인 시점 — 각 Phase 끝]
Phase 0 일체화 확인 후 / Phase 1 spec 초안 후 / Phase 2 plan 초안 후 — 사용자 보고 + 다음 진행 승인.

[진행 모드]
spec/plan 작성 시 Phase 나눠 단락별로 하나씩 질의([[spec_phase_by_phase_inquiry]]). 한 번에 큰 design 던지지 말 것.

[질문해야 할 시점 — 잠긴 것 말고 *진짜 열린* 디테일만]
- Step 4 속도싸움 방식: controller 가 `Impulse::IsActive()` 동안 `DoForward` suppress(권고) vs Impulse 가 `ApplyLinearImpulse`(가산).
- i-frame 저장: plain float vs `Stat(Tenacity)`.
- attack-window 길이(공격 중 MoveSprite 표시 지속 시간 — 홀드 vs N초).
- Carrier subtype 경계: `Projectile`(flying, lifetime despawn) vs `ContactCarrier`(적 둘러싼 sensor, persistent) 시그니처/공통 base.
- `Components::Movement` 삭제 확정 — `cfg.physics.world==nullptr`(비물리) 분기 실사용 액터가 정말 없는가.
- (선택, 승인 필수) `EnemyDeathHandler` 를 `Life::SetOnDeathFx` 로 통합해 player+enemy 단일 death 기구로 할지(§14 Step 7 후속).

[첫 행동]
FIRST READ 1번(핸드오프 §13/§14) 정독 → Phase 0: 잠긴 결정 재진술 + 현 코드 시그니처 정합 검증 결과를 사용자에게 보고하고 일체화 확인받은 뒤 Phase 1(spec) 착수.
```
