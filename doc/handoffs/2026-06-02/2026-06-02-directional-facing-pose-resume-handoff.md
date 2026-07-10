> 🔴 **SUPERSEDED (2026-06-03) → 단일 진입점: [`2026-06-03-pb-decomposition-COMPLETE-resume-handoff.md`](2026-06-03-pb-decomposition-COMPLETE-resume-handoff.md)** (중간 경유 `...-directional-facing-cullfix-session-resume-handoff.md`)
> directional facing 은 **DONE + committed (`a16eef0 "Playable 4방향"`)**. PlayerBehavior 분해 전체(Task 0~7)도 **코드 완료 + 빌드 GREEN** (Task5/7 커밋 `f45211c`). 이 문서의 STEP 코드는 *구현 기준/리뷰 참조* 로만 유효. 남은 잔여 = GUI 검증 + cull fix(`pass.h`) 커밋 — 후속 문서 참조.

# Handoff — Directional Facing/Pose 스프라이트 연출 (분해 Task6 잔여 슬라이스) 재개 컨텍스트 (2026-06-02, 무손실)

> **이 문서 = 다음 세션의 단일 진입점.** 탑다운 슈터(_MyApp_) 플레이어의 4방향(Front/Back/Left/Right)×2포즈(Idle/Move) 스프라이트 전환을 구현하는 작업. 연출 foundation(PlayableDirector/PostFXRegistry/SpriteFxPlayable/HpGrayscalePostFX)은 구현·미커밋 완료, directional 슬라이스만 미착수.
> **붙여넣기용 프롬프트**: `doc/handoffs/2026-06-02/2026-06-02-directional-facing-pose-agent-prompt.md` (자기완결 — 새 에이전트가 그것만으로 시작).
> **작성**: 2026-06-02. 브랜치 `game/module/ingame/temp`. HEAD `6adaf02`.
> ✅ **이 핸드오프는 실행됨** — directional STEP1~3 이 working tree 에 구현됨(미커밋, 빌드/GUI 검증+커밋 대기). 아래 STEP 코드는 *구현 기준/리뷰 참조*. foundation(소스 `8f276a9` + 배선 `aa684f7`)은 커밋 완료. (HEAD 가 작성 중 d44a4a3→8f276a9→6adaf02 로 6+ 커밋 전진 — §1 재측정.)
> 분해 전체 상위 진입점: `doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-foundation-done-resume-handoff.md`.
> **대체**: 구 `doc/handoffs/2026-06-02/2026-06-02-task6-on-director-foundation-agent-prompt.md` 의 **directional 부분만** 대체/축소(HitBlink/Dissolve 는 완료 → 범위 밖). 그 문서 상단에 SUPERSEDED 배너 부착됨.
> ⚠ 이 repo 는 병렬 커밋 + rename(8b5fc45) 진행 中: 시작 시 `git log --oneline -6` + `git status --short` 로 실측하고, include/참조 전 grep 으로 실제 파일명·라인 재확인.

## 0. 한 눈에 (TL;DR)
- **하는 일**: 플레이어가 이동/조준 방향에 따라 4방향×2포즈 스프라이트로 전환. ① PlayableDirector 가 8그룹 보유 + SetFacing/SetPose 빈 훅이 그룹 Visible 토글, ② PlayerBuilder 가 8그룹 child 를 미리 빌드 + RegisterGroup(개별 PNG → 자기 UniformAtlas, atlas 패킹 불필요), ③ PlayerController(RD5 단일 작성자)가 매 프레임 velocity/aim → Quantize4 → SetFacing/SetPose push.
- **진행**: foundation(Playable/ 소스 `8f276a9` + 연출 배선/엔진지원 `aa684f7`) = ✅ 커밋 완료. **directional STEP1~3 = 🟡 working tree 구현 완료·미커밋**: PlayableDirector DirGroup/SetFacing/Apply(.h:39-58) + PlayerBuilder:76-121 8그룹 빌드/RegisterGroup + PlayerController:193-212 RD5 push(`QuantizeByThreshold` 히스테리시스). Constants.h 에 PLAYER_FACING_THRESHOLD.
- **다음 행동**: (1) **빌드 + GUI 검증**(`cmake --build --preset ninja --target _MyApp_` → `./_MyApp_` → WASD 방향 따라 Front/Back/Left/Right + Idle/Move 육안), (2) 사용자 승인 후 path-scoped 커밋: `git add apps/_MyApp_/src/Playable/PlayableDirector.{h,cpp} apps/_MyApp_/src/Playable/Constants.h apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}` (`?? src/Text/`(World Text) 함께 stage 금지). 아래 STEP 코드는 *구현 기준* — 현 working tree 와 대조해 검토.
- **정본**: `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md` §3.4/§5.5/§5.7, `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` Task6, `doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md` §1.5. (※ `doc/`(복수) 는 gitignore 가능 — 본 문서가 자기완결.)
- **카덴스/방식**: 단일 task subagent 위임(별도 프롬프트 = agent-prompt). per-task 커밋 게이트(사용자 승인 후). no_auto_tests → 빌드 + GUI 육안.

## 1. 상태 (작성 시점 — 재측정 필수)
```
HEAD: 8f276a9ef20c1c6e2c34ca81f3d52ca29591244c  8f276a9 [dev] : 연출 디렉터 (MyApp::Playable 신규 lib)
브랜치: game/module/ingame/temp
최근 커밋(신→구):
  8f276a9 [dev] : 연출 디렉터  — ★ foundation Playable/ 소스 13파일 커밋 + BulletSpawnPlayable.{cpp,h} 삭제 fold.
  d44a4a3 [dev] : impulse      — Task4 impulse (PhysicsImpulse). 본 작업 무관.
  8b5fc45 [dev] : rename       — snake→PascalCase. ⚠ include 전 실제 파일명 확인.
✅ 커밋됨 (8f276a9, Playable/ — 더는 안 만짐. 편집 시 M 됨):
   PlayableDirector.{h,cpp} (SetFacing/SetPose 빈 훅 47-48 — 내가 채움) / PostFXRegistry.{h,cpp}
   PostFXTweenPlayable.{h,cpp} / SpriteFxPlayable.{h,cpp} (HitBlink/Dissolve) / HpGrayscalePostFX.{h,cpp} (HP grayscale [A]) / CMakeLists.txt
미커밋 (M) — 빌드 와이어링 (내 작업 접점, working tree 엔 존재해 빌드는 됨. 깔끔한 분리 원하면 선행 커밋):
   M apps/_MyApp_/src/CMakeLists.txt              (add_subdirectory(Playable)+MyApp::Playable link)
   M apps/_MyApp_/src/Bootstrap/CMakeLists.txt    (MyApp::Playable link)
   M apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp (director 부착 :73 + HpGrayscale :75 + fire/hit/death Register — STEP2 에서 내가 추가편집)
   M apps/_MyApp_/main.cpp                         (PostFXRegistry::Register)
미커밋 — 병렬 에이전트 dirty (NOT MINE, 미접근):
   M apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp           (적 스폰 — IDE 오픈 中)
   M apps/_MyApp_/src/Entity/Player/PlayerHand.{cpp,h}     (손/무기 스프라이트 — 신규 dirty, 미접근)
   M apps/_MyApp_/src/Entity/{Bullet/bullet_factory.h, CMakeLists.txt, Enemy/EnemyFactory.h}
   M apps/_MyApp_/src/Stage/{CMakeLists.txt, Factories/pickup_factory.h, Factories/wall_factory.h}
   M src/buffer/framebuffer.h, src/material/pass.h, src/render/property_block_setter.cpp,
     src/sprite/sprite_component.cpp, apps/_MyApp_/resources/shaders/billboard_atlas.vs (FX/렌더 코어)
   M CMakeLists.txt(루트), cmake/Doxygen.cmake, doxygen/direction/*  (Doxygen 병렬 트랙 — 미접근)
   m extern/Catch2 (submodule), M shell/CMakeExecute.sh   (잡음 — 미접근)
   (BulletSpawnPlayable/Physics filter.h 는 8f276a9 에 fold됨 — 더는 dirty 아님.)
⚠ 위 enumeration 은 작성 시점 스냅샷(HEAD 가 작성 중 d44a4a3→8f276a9 로 움직임). 시작 시 git status --short + git log -3 로 재측정하고, 이 목록에 없는 항목은 전부 "미접근"으로 간주.
```

## 2. Task 상태표
| # | 내용 | 상태 | 커밋/검증 |
|---|---|---|---|
| 선행 | 연출 foundation Playable/ 소스 (PlayableDirector + PostFXRegistry + HpGrayscalePostFX + CMake) | ✅ **커밋됨 `8f276a9`** | 빌드 와이어링 4 tracked-M(src/Bootstrap CMakeLists·PlayerBuilder·main.cpp)만 미커밋 — working tree 엔 존재 |
| 선행 | hit-FX (PostFXTween + SpriteHitFlash + SpriteDissolve) | ✅ 커밋됨 `8f276a9` | PlayerBuilder.cpp "hit"(:126)/"death"(:133) Register (PlayerBuilder 는 아직 M) |
| 본작업 | STEP1 PlayableDirector DirGroup/RegisterGroup/SetFacing/SetPose/Apply | 🟡 구현·미커밋 | PlayableDirector.h:39-58 (DirGroup{layers[4]}/RegisterGroup/SetFacing/SetPose) + .cpp Apply. 설계대로 |
| 본작업 | STEP2 PlayerBuilder 8그룹 child 빌드 + RegisterGroup | 🟡 구현·미커밋 | PlayerBuilder.cpp:76-121 (directional 8그룹 child + RegisterGroup) — **PlayerBuilder 에서**(링크사이클 회피) |
| 본작업 | STEP3 PlayerController RD5 push (interface sink, flipX 핵 교체) | 🟡 구현·미커밋 | PlayerController.cpp:193-212 (mSink=GetComponent<IActorPresentation>, QuantizeByThreshold→SetFacing/SetPose) |
| 검증 | 빌드 exit 0 + GUI 방향/포즈 전환 육안 + 커밋 | ⬜ **남은 일** | working tree 미커밋 — §0 다음행동 |

## 3. 정본 문서 인덱스
| 문서 | 경로 | git | 역할 |
|---|---|---|---|
| 분해 spec | `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md` | doc/(gitignore 가능) | §3.4 RD5 / §5.5 DirGroup·Apply / §5.7 PlayerController 멤버 |
| 분해 plan | `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` | doc/(gitignore 가능) | Task6 원설계 코드(PlayerSpriteDirector 형태 — 재편으로 PlayableDirector 흡수) |
| foundation spec | `doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md` | doc/(gitignore 가능) | §1.5 [A]/[B]/[C] ownership — directional 루프=[A], 선택 훅=[C] |
| 본 핸드오프 + 프롬프트 | `doc/handoffs/2026-06-02/2026-06-02-directional-facing-pose-*.md` | tracked(doc/ 단수) | 단일 진입점 + 붙여넣기 프롬프트 |
| 구 directional 프롬프트 | `doc/handoffs/2026-06-02/2026-06-02-task6-on-director-foundation-agent-prompt.md` | untracked | 🔴 directional 부분 대체됨 — HitBlink/Dissolve 완료분만 의미(배너 부착) |
> ⚠ `doc/`(복수) 정본은 다른 머신에 없을 수 있음 — 본 문서 §8 verbatim + agent-prompt 가 자기완결(재구성 가능).

## 4. 잠긴 결정 (재논의 금지) + 발견된 정정
- **RD5 (잠김)**: PlayerController 가 facing/pose 의 *유일 작성자*. director(PlayableDirector)는 토글만 — velocity 를 절대 읽지 않는다(SetFacing/SetPose 안에서 Apply 만). 계산식: `facing = attacking ? Quantize4(aimXZ) : moving ? Quantize4(velXZ) : lastFacing`; `pose = (attacking||moving) ? Move : Idle`. moving = velXZ 제곱크기 > 0.001. attackWindow = 0.15s.
- **방향=그룹 Visible (잠김, 만장일치)**: 단일 SpriteRenderer 의 flipX/PlayClip 으로 방향 표현 금지. 8그룹 미리 빌드 + 활성 그룹만 Visible=true. swap-in-place(런타임 atlas rebind)는 *거부된* 구 설계.
- **directional = PlayableDirector 흡수 (잠김, 2026-06-02 재편)**: 별도 PlayerSpriteDirector 클래스 신설 금지. "게임로직 외 모든 연출은 PlayableDirector 경유" 철학. plan 의 PlayerSpriteDirector 원설계는 폐기 — 로직만 PlayableDirector 로 동형 이식.
- **pose 2축만 (잠김)**: {Idle, Move} 만(자산이 IDLE/MOVE 만). Attack/Hit/Die 스프라이트 포즈 추가 금지 — Attack 은 "Move pose + aim facing" 으로 표현.
- ⚠ **정정 A (링크 토폴로지 — 구조 결정)**: synth 초안은 8그룹 빌드를 PlayerActor.cpp 에서, sink 을 PlayableDirector 구체타입으로 받았으나 **둘 다 링크 사이클이라 컴파일 불가**. 실측: `myapp_playable` 가 `MyApp::Entity` PUBLIC link(Playable/CMakeLists.txt:23) → Entity→Playable 추가 시 순환. `myapp_input_handler` 는 `MyApp::Entity` 만 link, Playable 미link(InputHandler/CMakeLists.txt:32). **정정**: 그룹 빌드 = `myapp_bootstrap`(Entity+Playable 둘 다 link, Bootstrap/CMakeLists.txt:21,29) = **PlayerBuilder.cpp**; sink = `Entity::IActorPresentation*`(인터페이스, GetComponent 인터페이스 조회=분해 Task0). PlayerActor.{h,cpp} 는 미접근(이미 Playable/Constants.h 를 헤더로만 include 해 컴파일됨 — 그대로 둠).
- ⚠ **정정 B (가시성 토글 근거)**: 그룹 숨김에 SetActive 아닌 SpriteRenderer.Visible 을 쓰는 *이유* — SetActive(false)는 렌더(`scene_renderer.cpp:119 if(!actor.IsActive())`)+tick(`actor.cpp:115 if(!mActive)`)을 **둘 다** 끈다. 그러면 비활성 그룹의 걷기 애니(bPart 루프, [A] scene-Component)가 멈춰 재진입 시 프레임이 튄다. Visible 만 끄면 child 는 active 라 bPart 가 계속 tick → 매끈한 재진입. (`mesh_renderer.h:61` Visible + `scene_renderer.cpp:128` 게이트는 정확.)
- ⚠ **정정 C (foundation 파일 수 + death 컴포지트)**: Playable/ untracked = **11개**(9 아님 — `HpGrayscalePostFX.{h,cpp}` 포함, PlayerBuilder.cpp:12,75-76 이 #include/AddComponent). **death 컴포지트는 grayscale 없음** — 화면 grayscale 은 HpGrayscalePostFX 가 HP비율로 상시([A]) 구동(사망=HP0 시 자동 완전 무채색), death 컴포지트는 SpriteDissolve 단일(:133).
- ⚠ **정정 D (PlayerBuilder 라인 drift)**: 실측 = director 부착 :73 / Register("fire") :106 / Register("hit") :126 / Register("death") :133(단일 SpriteDissolvePlayable, Composite 아님) / 콜백 SetFireCallback·SetDamageCallback :140-141 / `pac.sprite.direction` :62. (synth/구 핸드오프의 :71/:96/:116/:128 은 stale — grep `director->Register(` 로 재확인 권장.)
- ⚠ **정정 E (기타 drift)**: SetFacing/SetPose = PlayableDirector.h **47-48**(구 51-52 stale). 이미 등록된 키 = **"fire"/"hit"/"death"**("damaged_test" 폐기). 구조체 = `EntityTextureConfig`(`PlayerTextureConfig` 아님), 필드순서 `{DrawOrder,TexturePath,RowCount,ColCount,Flip}`(RowCount 가 ColCount 앞), ColCount>1=애니. fps 기본 = **8.0f**. 애니 컴포넌트 NS = `SJH::SpriteSequence::SpriteSequencePlayable`(SpriteRenderer 는 `SJH::Sprite::SpriteRenderer`).

## 5. 잔여 작업 — 구현 포인터 (실측 file:line)
- **STEP1 — PlayableDirector**: `apps/_MyApp_/src/Playable/PlayableDirector.{h,cpp}`. public `struct DirGroup{ std::array<SJH::Sprite::SpriteRenderer*,4> layers; }` + `RegisterGroup(f,p,g)`(inline) + `RefreshDirectional(){Apply();}`. private `mGroups[4][2]`/`mFacing`/`mPose`/`Apply()`/`idx()`. SetFacing/SetPose(:47-48) 본문 = mFacing/mPose 변경(같으면 early-return) + Apply(). Apply = 활성 그룹만 `layer->Visible=true`, 나머지 false. ★ bPart 는 DirGroup 에 안 넣음(child scene-Component 가 [A] 로 계속 tick — director 가 Play/Stop 안 함).
- **STEP2 — PlayerBuilder (★ PlayerActor 아님)**: `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp`. (a) :62 `pac.sprite.direction` 주입 제거(PlayerActor 단일방향 블록 무력화), (b) director 부착(:73) 직후 8그룹(kPlayerGroups 로컬 테이블, PLAYER_*_IDLE/MOVE)을 spriteActor child 로 빌드: 각 레이어 find-or-create atlas → `AddChild` → `AddComponent<SJH::Sprite::SpriteRenderer>` (flipX=t.Flip, QueueOffset=t.DrawOrder, Visible=(Front&&Idle)) → ColCount>1 이면 `AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(spr,{0,ColCount,8.0f})`.SetIsLoop(true).Play() → `director->RegisterGroup`. 끝에 `director->RefreshDirectional()`. fire/hit/death Register(:106/:126/:133)·콜백(:140-141)·HpGrayscale(:75-76) 무변경(공유 편집점, surgical add only).
- **STEP3 — PlayerController**: `apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}`. h: `Entity::IActorPresentation* mSink`/`EFacing mLastFacing`/`mAttackWindowSec(0.15)`/`mAttackTimer` 멤버(Components.Interfaces.h 이미 include). cpp Update(): :171-177 flipX 핵을 RD5 push 로 교체 — `if(!mSink) mSink=owner->GetComponent<Entity::IActorPresentation>()`(인터페이스 조회), velXZ(mInputValue[0],[2]) 는 :161 리셋 *전* 확보, Quantize4 로 facing/pose 계산해 `mSink->SetFacing/SetPose`. EulerRot[1]=mAimAngleY(:171) 유지. 미사용 mCachedSprite(h:101)/forward(h:18)/sprite_component.h include 정리(-Werror).
- 검증: build-only 아님 — **GUI 육안 필수**(방향 전환 + idle/move 포즈). Quantize4 부호 반대면 controller velXZ/aimXZ 에서 부호 우회(읽기전용 Components.Interfaces.h 미수정) + 보고.

## 6. 병렬 트랙 충돌 매트릭스
| 트랙 | 상태 | 내 작업과 |
|---|---|---|
| 적 스폰 (EnemyBuilder.cpp / EnemyFactory.h / bullet_factory.h / Stage/*) | wip(미커밋, IDE 오픈) | 충돌 0 — 미접근. enemy 도 PlayableDirector 부착하나 그건 적 트랙 소유 |
| FX/렌더 코어 (src/{buffer,material,render,sprite}/*, billboard_atlas.vs) | wip(미커밋) | 충돌 0 — 미접근. directional 은 셰이더 안 건드림 |
| hit-FX (SpriteFxPlayable, PostFXTween, HpGrayscalePostFX) | ✅ 완료·미커밋 | directional 은 "hit"/"death" key 와 *다른 축*(facing 상태 vs 트리거 이벤트) → key 충돌 0 |
| Fog (main.cpp) / Doxygen (루트 CMakeLists, cmake/Doxygen.cmake) | wip | directional 은 둘 다 미접근 → 충돌 0 |

| 파일 | foundation | 내Task(directional) | hit-FX |
|---|---|---|---|
| `PlayerBuilder.cpp` | director 부착 :73 + HpGrayscale :75 + fire/hit/death Register | :62 direction 제거 + 8그룹 빌드/RegisterGroup(추가) | hit(:126)/death(:133) | → 3자 공유, surgical add only, foundation 먼저 커밋 |
| `PlayableDirector.{h,cpp}` | Register/Play/Update/ReactX | SetFacing/SetPose 본문 + DirGroup/RegisterGroup/Apply 추가 | (해당 없음) | → 기존 멤버 의미 불변, 추가만 |
| `PlayerActor.{h,cpp}` | (foundation 미접근) | **미접근**(Entity→Playable 사이클) | — | → 그룹 빌드는 PlayerBuilder 가 |
| `PlayerController.{h,cpp}` | — | RD5 push(interface sink) | — | → InputHandler 는 Playable 미link → IActorPresentation 인터페이스만 |
| `main.cpp` | PostFXRegistry::Register | 미접근 | 미접근 | → Fog 에이전트와 경합 가능(내 작업 무관) |

## 7. 가드레일 / 컨벤션
- **커밋 정책**: 사용자 승인 전 커밋 금지. `git add -A`/`commit -a` 금지 — path-scoped only. Co-Authored-By 트레일러 미사용.
- **선행 커밋 범위(승인 시)**: `git add apps/_MyApp_/src/Playable/ apps/_MyApp_/src/CMakeLists.txt apps/_MyApp_/src/Bootstrap/CMakeLists.txt apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp apps/_MyApp_/main.cpp` (foundation atomic — Playable/ 글롭이 11파일 전부 포함). BulletSpawnPlayable.{cpp,h} 삭제 + Physics/filter.h↔PhysicsLayer.h 는 소속 모호 → 사용자 확인 후 별도. 루트 CMakeLists/Doxygen 트랙은 stage 금지.
- **미접근(절대)**: src/playable/*, src/timer/*, *.fs/*.frag/*.vs 셰이더, src/buffer/framebuffer.h, src/material/pass.h, src/render/property_block_setter.cpp, src/sprite/sprite_component.cpp, **PlayerActor.{h,cpp}**, EnemyBuilder.cpp, Entity/Enemy/*, Entity/Bullet/*, Stage/*, Spawns/*, LifeComponents.h(읽기만), main.cpp(PostFX/Fog/카메라), 루트 CMakeLists.txt, cmake/Doxygen.cmake, doxygen/direction/*, shell/CMakeExecute.sh, extern/Catch2.
- **코드 컨벤션**: 주석 한국어. 헤더가드 `__XXX_H__`(#pragma once 금지 — 단 인접 파일은 `_TOPDOWNSHOOTER_..._`(Constants/PlayerActor) 와 `__TOPDOWNSHOOTER_..._H__`(PlayableDirector) 혼용, 신규는 후자 권장). long 금지(int32_t 등). 경로 슬래시. 네임스페이스 TopdownShooter::*(게임) / SJH::*(엔진), MyApp:: 는 CMake alias 한정.
- **검증**: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_` → exit 0(sb7 #warning 만). `cd build_ninja/apps/_MyApp_ && ./_MyApp_` → 무크래시 + facing/pose 육안. (링크 에러 = 구조 위반 = Entity→Playable / InputHandler→Playable 사이클.)

## 8. (loss 위험 시) 핵심 코드 verbatim — 미커밋 foundation 의 내 작업 접점
```cpp
// apps/_MyApp_/src/Playable/PlayableDirector.h:47-48 (채울 빈 훅 — 현재 no-op)
void SetFacing(Entity::EFacing /*facing*/) override {}
void SetPose(Entity::EPose /*pose*/) override {}
// 같은 파일 43-45 (무변경): ReactDamaged→Play("hit") / ReactDied(.cpp)→Play("death") / ReactAttack→Play("attack")
// 시그니처(무변경): Register(string, unique_ptr<SJH::Playable::PlayableBase>)->PlayableDirector& / Play/Stop/Has / Update(float)

// apps/_MyApp_/src/Entity/Components/Components.Interfaces.h:85-95 (읽기 전용 — 이미 정의)
enum class EFacing : int { Front = 0, Back, Left, Right };
enum class EPose   : int { Idle  = 0, Move };
inline EFacing Quantize4(vmath::vec2 v) {            // x>0=Right, z>0=Front (탑다운 W=-Z)
    if (std::abs(v[0]) > std::abs(v[1])) return v[0] > 0.0f ? EFacing::Right : EFacing::Left;
    return v[1] > 0.0f ? EFacing::Front : EFacing::Back;
}

// 링크 토폴로지 (구조 결정 근거 — 실측)
// Playable/CMakeLists.txt:23   myapp_playable PUBLIC -> MyApp::Entity   (Entity→Playable 추가 시 순환)
// Bootstrap/CMakeLists.txt:21,29  myapp_bootstrap -> MyApp::Entity + MyApp::Playable  (그룹 빌드 안전)
// InputHandler/CMakeLists.txt:32  myapp_input_handler -> MyApp::Entity (Playable 미link → IActorPresentation 인터페이스만)

// src/render/scene_renderer.cpp:119/128 + actor.cpp:115 (가시성 근거)
if (!actor.IsActive()) return;                          // :119 — SetActive(false)=렌더 컬링
if (!mActive) return;                                   // actor.cpp:115 — SetActive(false)=tick 중단
if (mr->IsEnabled() && mr->Visible && mr->Mesh && mr->Material)  // :128 — Visible 게이트 (그룹 숨김은 이걸로)

// apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp (현행 — 무변경 블록, grep 으로 라인 재확인)
auto *director = spriteActor->AddComponent<TopdownShooter::Playable::PlayableDirector>(); // :73 (AddChild 전)
director->Register("fire", ...);    // :106  ┐
director->Register("hit",  ...);    // :126  ├ 건드리지 마라
director->Register("death", std::make_unique<...SpriteDissolvePlayable>(...)); // :133 (단일) ┘
controller->SetFireCallback([director]{ director->Play("fire"); });          // :140
controller->SetDamageCallback([director]{ director->ReactDamaged(0); });     // :141 (=Play("hit"))
// :62  pac.sprite.direction = &TopdownShooter::Playable::PLAYER_FRONT_MOVE;  ← 제거(8그룹으로 대체)
// :75-76 spriteActor->AddComponent<HpGrayscalePostFX>("grayscale_vignetting","uGrayscaleAmount"); ← 무변경([A] HP 상시)

// apps/_MyApp_/src/InputHandler/PlayerController.cpp:171-177 (현행 — flipX 핵을 RD5 interface push 로 교체)
owner->GetTransform().EulerRot[1] = mAimAngleY;          // :171 유지(손 궤도)
mCachedSprite = owner->GetComponent<SJH::Sprite::SpriteRenderer>();          // :175 ← 제거
mCachedSprite->flipX = (mAimDirection[0] < 0.0f);        // :177 ← 제거(→ mSink->SetFacing/SetPose)
```

## 9. 알려진 이슈 (내 작업 무관, 혼동 방지)
- muzzle / distortion.efk 로드 [error] 1건: 별개 VFX 에이전트 영역(.efk 버전 트랩). directional 무관 — 회귀 아님. fire FX 안 보여도 정상.
- "fire" 슬롯: muzzle null 이면 가드로 Register 생략 → Play("fire") silent no-op. directional 무관.
- main.cpp Fog 병렬 작업 + 루트 CMakeLists/Doxygen 트랙이 동시 진행 — directional 은 셋 다 미접근이라 무관. 단 foundation 의 main.cpp PostFXRegistry 변경이 Fog 와 충돌 가능(foundation 커밋 후 즉시 알릴 것).
- 구 진입점 핸드오프(`...playerbehavior-decomposition-resume-handoff.md`, `...pb-decomposition-task4-done-resume-handoff.md`)는 PlayerBehavior 분해 *전체* 트랙용 — 본 directional 슬라이스의 진입점은 이 문서. 혼동 말 것.

## 10. 변경 기록 (append-only)
| 일자 | 변경 |
|---|---|
| 2026-06-02 | 본 핸드오프 생성(기존 에이전트 기록 소실 → 무손실 재인계). directional facing/pose 슬라이스 분리 캡처(HitBlink/Dissolve 완료분 제외). 워크플로 합성(7 readers gather → synth → 적대 critic) 후 8 gap(blocker2/major2/minor4) + 라이브 코드 교차검증 반영. **구조 정정 2건**: ① 그룹 빌드를 PlayerActor→PlayerBuilder(Entity→Playable 사이클 회피), ② sink 을 PlayableDirector 구체타입→`Entity::IActorPresentation*`(InputHandler 가 Playable 미link). SetActive vs Visible 근거 정정(둘 다 끔 → Visible 로 bPart 유지). PlayerBuilder 라인 drift 정정(director :73/fire :106/hit :126/death :133/콜백 :140-141). |
| 2026-06-02 (작성 직후) | ⚠ **HEAD drift 반영**: 작성 도중 `d44a4a3`→`8f276a9` 로 foundation Playable/ 소스 13파일이 커밋됨(+BulletSpawnPlayable 삭제 fold). §0/§1/§2 재측정: Playable/ = 커밋완료(편집 시 M), 선행 커밋 가드 = 빌드 와이어링 4 tracked-M(src·Bootstrap CMakeLists·PlayerBuilder·main.cpp)으로 축소(working tree 존재→빌드는 됨). 신규 병렬 dirty PlayerHand.{cpp,h} 추가. |
