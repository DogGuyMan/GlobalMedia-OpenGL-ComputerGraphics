# 다른 Claude Code Agent 용 프롬프트 — Task6: PlayableDirector 위 directional/HitBlink sprite 연출

> 🔴 **directional facing/pose 슬라이스는 대체됨 (2026-06-02)** → `doc/handoffs/2026-06-02/2026-06-02-directional-facing-pose-agent-prompt.md` (+ resume `...-directional-facing-pose-resume-handoff.md`) 사용.
> 이 문서의 HitBlink/Dissolve 슬라이스는 이미 완료(`SpriteFxPlayable`)됐고, directional 부분은 위 신규 핸드오프가 라이브 코드 정정(그룹 빌드=PlayerBuilder / sink=IActorPresentation 인터페이스 / foundation 11파일 등)을 반영해 대체한다. 본 문서는 히스토리 참고용.

> 아래 한 코드블록을 새 세션에 그대로 붙여 사용. 자기완결. **foundation은 이미 구현됨**(미커밋) — 너는 그 위에 sprite 연출을 *등록*만 한다.
> 전체 맥락(무손실): [`2026-06-02-playable-director-foundation-DONE-resume-handoff.md`](2026-06-02-playable-director-foundation-DONE-resume-handoff.md). Task6 자체 스펙: 메모리 `next_work_playable` + `doc/superpowers/.../2026-06-01-playerbehavior-decomposition*`.

---

```
[ROLE]
너는 C++17/CMake OpenGL 탑다운 슈터(/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics, 브랜치 game/module/ingame/temp)의 구현 에이전트다.
철학: 게임 로직(HP/damage/physics/AI/입력판정) 제외, 모든 연출/비주얼/사운드는 PlayableDirector 의 named Playable(verb→Play(key))로만.
직전 작업으로 **PlayableDirector + PostFXRegistry 연출 foundation 이 이미 구현됐다(미커밋)**. 너의 일 = 그 위에 *플레이어 방향/포즈/피격* sprite 연출을 등록하는 것(= 분해 Task6 의 연출 슬라이스). foundation 자체는 손대지 마라.

[절대 규칙]
- 커밋/git add 금지(사용자 승인 후). `git add -A` 금지 — path-scoped only(이 브랜치는 병렬 에이전트 多, 무관 dirty 쓸려 들어감). Co-Authored-By 미사용.
- 빌드+육안 검증만. 단위테스트 자동추가 금지(no_auto_tests).
- 주석 한국어. 헤더가드 __XXX_H__ (#pragma once 금지). long 금지(고정폭 int32_t 등). 경로 슬래시.
- 네임스페이스: 게임코드 TopdownShooter::* (MyApp:: 은 CMake alias 한정), 엔진 SJH::*.
- **수정 금지(코어/병렬소유)**: src/playable/*, src/timer/*, *.fs/*.frag 셰이더, apps/_MyApp_/src/Spawns/*, enemy_factory.h, EnemyDeathHandler, Carrier, LifeComponents.h, Components.Interfaces.h(IActorPresentation은 읽기만 — Life가 이미 sink 호출). main.cpp 의 PostFX/Fog/카메라 라인.
- **EnemyBuilder.cpp / shell/CMakeExecute.sh / src/buffer/framebuffer.h / src/material/pass.h 미접근**(병렬 에이전트 dirty).

[검증된 사실 — foundation 인프라 (직접 재확인 권장)]
- PlayableDirector (`apps/_MyApp_/src/Playable/PlayableDirector.h`, namespace TopdownShooter::Playable):
    class PlayableDirector : public SJH::Scene::Component, public TopdownShooter::Entity::IActorPresentation
    - Register(const std::string& key, std::unique_ptr<SJH::Playable::PlayableBase> p) -> PlayableDirector&  (fluent, 덮어쓰기)
    - Play(key) = 있으면 Stop()+Play()(첫 프레임부터), 없으면 silent no-op
    - Stop(key) / bool Has(key) const
    - void SetSpawnContext(const Spawns::SequenceContext&)
    - 중앙 tick: Update(dt) 가 Play 된(playing=true) 미완료 슬롯만 ->Update. **등록만 한 슬롯은 tick 안 됨**(autoplay 방지).
    - IActorPresentation 훅(verb→Play(key)): ReactDamaged(int)→Play("hit") / ReactDied(vec3)→Play("death")+ctx월드점 / ReactAttack(vec2)→Play("attack")
    - **너의 자리(현재 빈 훅)**: void SetFacing(Entity::EFacing) override {}   void SetPose(Entity::EPose) override {}   (PlayableDirector.h:51-52)
- 플레이어 루트 액터에 director 1개 부착됨(= 유일 IActorPresentation = Life::mSink). 부착/등록은 apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp:72~137 (CreatePlayerActor 직후, AddChild 전).
- 이미 등록된 키: "fire"(좌클릭 muzzle+shot+slash) / "damaged_test"(G키 shake+Damaged). **이 둘은 건드리지 마라.**
- EFacing{Front,Back,Left,Right} / EPose{Idle,Move} 는 Components.Interfaces.h. `Quantize4(vmath::vec2)->EFacing` 자유함수 존재(x>0=Right,z>0=Front, 탑다운 W=-Z).
- 누가 SetFacing/SetPose 를 호출하나? 현재 호출자 0 — 너가 호출 경로도 마련해야 함(예: PlayerController/PlayerBehavior 의 이동·조준 방향에서 director->SetFacing(Quantize4(dir)) / SetPose(speed>0?Move:Idle)). **호출자 추가 시 PlayerController.cpp 등은 surgical**(병렬 분해 트랙과 공유).
- sprite 연출 Playable 본체(directional 스프라이트 전환 / HitBlink / Dissolve)는 Task6 분해 스펙 소유 — SJH::Sprite::SpriteSequencePlayable(`src/sprite/`) + SJH::Playable Composite 조합으로 만든다. 신규 sprite Playable 클래스는 **apps/_MyApp_/src/Playable/ 또는 Entity/Player/** 에 두되 코어(src/) 금지.

========================================================================
[STEP 1] directional/pose 연출 Playable 등록
========================================================================
- Task6 스펙대로 방향별 sprite 전환(FRONT/BACK/LEFT/RIGHT MOVE·IDLE) Playable 을 만들고, PlayerBuilder 의 director 부착 블록(또는 전용 빌더)에서 director->Register("<key>", ...) 한다.
- SetFacing(EFacing f) 빈 훅을 채워 facing→해당 directional Playable 로 라우팅(예: Play("face_front") 등 — 키 명명은 네가 정하고 Register와 일치시켜라). SetPose 동일.
- HitBlink/Dissolve: "hit"/"death" 키에 sprite 측 연출을 *추가*하고 싶으면 — 주의: "hit"/"death" 는 **hit-FX 트랙(PostFX/사운드)과 공유**. Composite 로 합성하거나 별도 키로 분리할지 그쪽과 조율(아래 §조율). 충돌 피하려면 sprite 전용 키("hit_sprite" 등) 권장.

========================================================================
[STEP 2] 호출 경로 배선
========================================================================
- 이동/조준 방향이 바뀔 때 director->SetFacing/SetPose 가 호출되도록 PlayerController 또는 PlayerBehavior 에서 배선(이동 입력/velocity → Quantize4 → SetFacing; speed>0 → SetPose(Move)). director 핸들은 spriteActor->GetComponent<TopdownShooter::Playable::PlayableDirector>() 로 획득.
- 피격 sprite 연출은 Life 가 이미 mSink->ReactDamaged 를 호출하므로 ReactDamaged→Play("hit") 경로로 자동 발동(추가 호출자 불필요).

[스코프 경계]
- OWN: 신규 directional/HitBlink/Dissolve sprite Playable 클래스(코어 밖) + 그 Register + SetFacing/SetPose 훅 본문 + 호출 경로 배선.
- NEVER: foundation 파일 의미 변경("fire"/"damaged_test" 로직, Register/Play/Update/PostFXRegistry 시그니처), 코어/셰이더/Spawns/enemy/Life, main.cpp PostFX·Fog·카메라.
- 공유 편집점 PlayerBuilder.cpp / PlayerController.cpp — 추가만, surgical.

[조율 — 병렬 트랙]
- hit-FX 트랙이 "hit"/"death" 에 PostFX(Vignette/Grayscale via PostFXRegistry)+사운드 Composite 를 등록할 예정. 같은 키를 둘이 Register 하면 나중 것이 덮어쓴다 → sprite 는 별도 키로 등록하고 director 가 양쪽을 각각 Play 하거나, 한 Composite 로 합성하기로 합의. 합의 전엔 sprite 전용 키 사용.
- enemy/bullet 연출, onDeathFx/SetOnHitFx delegate 제거는 Task5/7 — 미접근.

[검증]
- cmake --preset ninja && cmake --build --preset ninja --target _MyApp_   (기대: exit 0; sb7 gl.h/gl3.h #warnings 만 — 그건 -Wno-error)
- cd build_ninja/apps/_MyApp_ && ./_MyApp_   (기대: 무크래시. [error] 1건=muzzle(distortion.efk) 로드 실패는 기존/별개 VFX 에이전트 — 무관)
- 육안: 이동 방향에 따라 플레이어 스프라이트 facing 전환 / 피격 시 HitBlink. (좌클릭 fire FX 는 muzzle.efk 깨져서 안 보이는 게 정상 — 회귀 아님.)

[Self-review]
- foundation 파일(Playable/*) 시그니처·로직 무변경? "fire"/"damaged_test" 무변경?
- 신규 sprite Playable 은 코어(src/) 밖? SetFacing/SetPose 훅 본문 + 호출 경로 둘 다 배선?
- "hit"/"death" 키를 hit-FX 트랙과 충돌 없이 다뤘나(별도 키 or 합의)?
- 코어/셰이더/Spawns/enemy/Life/main PostFX 무수정? path-scoped?

[보고]
DONE / DONE_WITH_CONCERNS / BLOCKED + 변경 파일별 요약 + 빌드 마지막 줄 + 실행(facing 전환/HitBlink 육안) + "hit"/"death" 키 조율 상태 + git status. 커밋 금지.
```

---

## 사용 메모 (오케스트레이터/사용자용)
- 이 프롬프트는 **foundation 이 먼저 커밋/존재**해야 의미 있다. foundation 미커밋 상태에서 같은 워킹트리에 돌리면 두 작업이 섞인다 → 먼저 foundation path-scoped 커밋 승인 권장.
- **공유 편집점 = PlayerBuilder.cpp / PlayerController.cpp** (foundation·Task6·hit-FX 3자). 순서: foundation(완료) → (Task6 ∥ hit-FX). "hit"/"death" 키는 두 트랙이 공유하므로 한쪽이 먼저 키 규약을 확정하거나 별도 키로 분리.
- hit-FX 트랙용 프롬프트는 동일 골격에 STEP만 교체(PostFXTweenPlayable 로 PostFXRegistry::Get().Material("grayscale_vignetting")->Properties.Floats[...] 트위닝 → "hit"/"death" Composite 등록)하면 된다.
- 권장 커밋 메시지(Task6 결과): `[feat] : 플레이어 directional/HitBlink sprite 연출 — PlayableDirector 등록`.
