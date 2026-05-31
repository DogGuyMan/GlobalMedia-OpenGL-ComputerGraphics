# Topdown Shooter — 마일스톤 진행 보고서

> **최종 갱신**: 2026-05-31 (M5 위임항목 완료 반영 + Render Pipeline 대거 정착 — Stage Builder / SceneContext / RenderStage / 2-Camera PassComponent / ImGui 분리 / UniversalRT / Director→Manager rename / main 분할 / ParticleStage)
> **브랜치**: `game/module/ingame/temp` (이전 `game/module/rendertarget` 에서 이동)
> **관련 spec**: [`docs/superpowers/specs/2026-05-24-topdown-shooter-design.md`](../docs/superpowers/specs/2026-05-24-topdown-shooter-design.md) §부록 D
> **M4 spec** (arch-correction 반영): [`docs/superpowers/specs/2026-05-26-m4-player-behavior-design.md`](../docs/superpowers/specs/2026-05-26-m4-player-behavior-design.md)
> **FSM 정본 spec** (Stage 4 진화): [`docs/superpowers/specs/2026-05-25-fsm-object-state-machine-design.md`](../docs/superpowers/specs/2026-05-25-fsm-object-state-machine-design.md)
> **IPlayable 정본 spec** (M3.5 신설): [`docs/superpowers/specs/2026-05-26-playable-component-interface-design.md`](../docs/superpowers/specs/2026-05-26-playable-component-interface-design.md)
> **M3 plan**: [`docs/superpowers/plans/2026-05-25-M3-physics-box2d.md`](../docs/superpowers/plans/2026-05-25-M3-physics-box2d.md)

## 진행 요약

| M | 범위 | 상태 | 진행도 |
|---|---|---|---|
| **M1** 빌드 + Sprite + 빌보드 1장 정적 | ✅ 완료 | 100% |
| **M2** Actor+Component+Input+FSM+follow | ✅ P2 정착 (FSM 코어는 Stage 4 진화) | ~90% |
| **M3** Box2D 물리 (Client 한정) | ✅ **완료 + spec 결정 진화** | 100% |
| **M3.5** Playable + sprite_sequence | ✅ **코어 모듈 정착** — `SJH::playable` 신설 + `sprite_sequence_playable` 통합 + main.cpp 마이그레이션 (SpriteAnimator 폐기). leaf Playable (Effekseer/FMOD) 은 M5 로 위임 | ~90% |
| **M4** PlayerBehavior + 발사 + 적 *(arch-correction: Player FSM 없음)* | 🟡 **본격 배선 완료** — `IntervalPlayable`/`AppendInterval` T1·T2(엔진) ✅ + `SpriteSequencePlayable` multi-clip + `PlayerBehavior`(flat Idle/Move/Attack/Hit/Dash/Die) + Bullet/Enemy 시스템 + main.cpp 완전 배선. **잔여**: 시각 검증 미수행. `PlayerStateMachine` → **arch-correction 폐기** | ~65% |
| **M5** Effekseer + FMOD + Tweeny Playable | ✅ **완료 (2026-05-26)** — Director (Client 싱글톤) 신설 + AudioSystem/VFXSystem + leaf 4종 (FmodStudio/Fmod/Effekseer/Tween) + Composite (Sequence+Parallel) 통합. Engine 변경: SJH::ResourceRegistry 에 Sound/Effect 추가 + game_deps PUBLIC link (spec §6.1) | 100% |
| **M6** 시퀀스 빌더 + 사운드 본격 | ❌ 미시작 | 0% |
| **M7** GameFSM + 보스 + 종료 | 🟡 `Stage/Stage.h` + `Stage/State/StageFSMState.h` 주석 stub | ~3% |
| **부수** Render Phase 1 — DeviceContext / Material 정비 | ✅ Observer 제거 + EagerBuild + Camera↔RenderTarget 의존 역전 + DrawCommand 통합 | ~70% |
| **부수** Stage Builder 리팩토링 | ✅ **완료** (`537a323`) — `CreateStageActor` Builder + `StageConfig` + `Stage/Factories/`(Physics→이동) + `StageStateComponent` + WaveController 합류. spec [`2026-05-26-stage-builder-refactor`](../docs/superpowers/specs/2026-05-26-stage-builder-refactor-design.md) | 100% |
| **부수** Render Pipeline SP 시리즈 (SceneContext / RenderStage / 2-Camera PassComponent / ImGui 분리 / UniversalRT) | ✅ **대거 정착** (2026-05-27~31, 아래 §Render Pipeline 정착 참조). ⚠ `SP-UniversalRenderTarget`(`eb85809`) 은 spec/plan 부재 | ~90% |
| **부수** Director→Manager rename + main.cpp 분할 + ParticleStage | 🟡 rename/분할 ✅. **ParticleStage 클래스 커밋(`592e99b`) 됐으나 mStages 미배선** (main.cpp 에서 `VFX().Draw()` 직접 호출 잔존) | ~80% |

---

## M1 — 완료 (2026-05-24)

**산출**:
- `apps/_MyApp_` 재활성, sb7::application 패턴
- `src/sprite/` 신규 모듈 — `uniform_atlas.{h,cpp}` + `sprite_component.h` POD
- `billboard_atlas.{vs,fs}` GLSL 410 + `Director + SceneRenderer + Material + MeshRenderer` 패턴 정착
- `TestPattern` atlas 128px tile 4×4 grid 로드 + frame 0 빌보드 1장 표시

**Commits**:
- `b590bb1 fix(_MyApp_): M1.5 빌보드 정면 + Mesh::CreatePlane + Scene::Camera 의존`
- `71567be refactor(_MyApp_): GLSL uniform 컨벤션 정정 (snake_case → camelCase) + uModel 흡수`
- `16f1426 refactor(_MyApp_): Director + SceneRenderer + Material 패턴 전환`

---

## M2 — 정착 완료 (2026-05-25)

### M2 P1 — `SJH::fsm` 코어 모듈 (Stage 4 진화)

원본 plan ([`docs/superpowers/plans/2026-05-24-M2-fsm-player-follow.md`](../docs/superpowers/plans/2026-05-24-M2-fsm-player-follow.md)) 의 P1 = `StateMachineProcessor<TState, TTransit>` switch-on-enum 베이스. 4-commit 진화 후 *근본적으로 다른 design* 으로 정착.

| Stage | Commit | 변화 |
|---|---|---|
| Stage 1 | `78dea2a` | `StateMachineProcessor` switch-on-enum (plan 원본) |
| Stage 2 | `16a6cdd` | `ObjectStateMachine` + `IFsmState<TOwner>` 도입 (4 엔진 정통 흡수 — Unity/Unreal/Godot/Cocos2d) |
| Stage 3 | `6f5346c` | rename + Stage 1 폐기 (`state_machine_processor.h` + `test_fsm.cpp` 제거) |
| **Stage 4** | `3bdf499` | **`StateMachine<TState, TOwner>` + `GetTransitFlag()` 그래프 응집** (TTransit 제거) |

**현재 정본**: `SJH::FSM::StateMachine<TState, TOwner>` (Aggregate Root) + `IFsmState<TOwner>` (Entity within Aggregate, `GetStateFlag`/`GetTransitFlag` self-identifying)
- 사용처: **0** (PlayerStateMachine 등 미도입 — M4 시점)

### M2 P2 — PlayerController + Camera follow + main.cpp 정착

| 구현 | Commit | 산출 |
|---|---|---|
| 초기 도입 | `c1012ea` | PlayerController.h/.cpp 신설 + TargetFollowableCameraController (별도 클래스 — plan 의 follow mode 추가 보다 깔끔) |
| Movement 추상화 | `2eb4150` | `SetMovableTarget(IMovable*)` (Movement 구체 의존 제거) + dt 전달 (fps-independent) + `+=` 누적 (동시 키 대각 이동) + 입력 부호 정정 (W=-Z) |
| IMovable 시그니처 | `2eb4150` | `DoForward(vec2 dir, float dt)` units/sec 단위 명시 |
| Movement Component | `2eb4150` | `normalize(dir) * (speed * dt)` + zero-vec 가드 |
| Sprite Animator | `c4fd046` | `src/sprite/sprite_animator.h` 자동 frame wrap (default fps = FrameCount) |
| UniformAtlas Builder | `c4fd046` | `LoadFromPNG(path).SetGrid(cols, rows)` 체이닝 분리 |
| Camera follow 활성 | `2eb4150` | `SetFollowTarget(mSpriteActor).SetFollowOffset({0,5,5})` |

**시각 검증 완료** (사용자 확인 2026-05-25):
- 부드러운 WASD 이동
- 동시 키 대각 이동
- 키 떼면 정지 (UB 없음)
- Frame 자동 교체 (좌하단 → 우 → 위 row-major)

### M2 P3 — CLAUDE.md Active Target Management

CLAUDE.md 의 `_MyApp_` 가 활성 첫 줄 + M2 P2 완료 명시. ✅

### M2 잔여

- ❌ FSM 단위 테스트 — 사용자 명시 거부 (`no_auto_tests` 메모리). 사용자 요청 시 재고.
- ❌ FSM 실제 사용처 — `PlayerStateMachine` 미도입 (M4 시점).

---

## M3.5 — 본격 정착 (2026-05-26)

**정본 spec**: [`docs/superpowers/specs/2026-05-26-playable-component-interface-design.md`](../docs/superpowers/specs/2026-05-26-playable-component-interface-design.md) (7 결정 + Tweeny/DOTween 정통 Builder)

spec §1.5/§1.6 원안에서 *7 결정 진화* 를 거쳐 최종 정착 — IPlayable pure interface + PlayableBase abstract (Component 다중 상속) + Composite Component (vector<unique_ptr<IPlayable>>) + Fluent Builder (Append/Insert/Join).

### 산출 commits (8건)

| Commit | 영역 |
|---|---|
| `b383639` | feat(playable): SJH::playable 모듈 신설 — IPlayable + PlayableBase |
| `76e1b03` | feat(playable): SequencePlayable + ParallelPlayable + fluent Builder |
| `2eb81d8` | fix(playable): composite_playable strict-include + sign-conversion |
| `59eefcb` | feat(sprite_sequence): SpriteFrameClip + SpriteSequencePlayable 신설 |
| `fa02c3b` | fix(sprite_sequence): strict-include 정리 |
| `ee9b6eb` | build(engine): SJH::engine 우산에 playable 합류 (14 → 15 모듈) — sprite_sequence 는 SJH::sprite 안에 통합 정착 |
| `620ba4c` | dev: Sprite Renderer (sprite 모듈 내 sprite_sequence 코드 통합) |
| `f23279b` | dev: sprite playable — **이번 세션 SpriteAnimator → SpriteSequencePlayable main.cpp 마이그레이션 + sprite_animator.h 삭제 + sprite/CMakeLists self-link 정리** |

### 본 세션 마이그레이션 (f23279b)

| 파일 | 변경 |
|---|---|
| `apps/_MyApp_/main.cpp` | include 교체, startup() 의 SpriteAnimator 4줄 → SpriteSequencePlayable 8줄 (SpriteFrameClip{0, atlas->FrameCount(), 4.0f} + SetIsLoop(true).Play()), 멤버 mAnimator → mSpriteSeq + mWholeAtlasClip |
| `src/sprite/CMakeLists.txt` | self-link `SJH::sprite` 제거 + `SJH::playable` PRIVATE → PUBLIC 승격 (헤더 전파) |
| `src/sprite/sprite_component.h` | docstring SpriteAnimator → SpriteSequencePlayable + 사용 예 갱신 |
| `src/sprite/sprite_animator.h` | **삭제** (spec §6.1 폐기 완료) |

빌드 검증: `cmake --build --preset ninja --target _MyApp_` → `[5/6] Linking CXX executable apps/_MyApp_/_MyApp_` 정상.

### M5 로 위임했던 항목 — ✅ M5 에서 완료 (2026-05-26)

> 아래는 M3.5 시점에 "M5 로 위임" 한 항목. **모두 M5 leaf-playables 작업에서 완료됨** — 본 섹션의 과거 ❌ 표기를 갱신 (2026-05-31). 플랜: [`docs/superpowers/plans/2026-05-26-m5-leaf-playables.md`](../docs/superpowers/plans/2026-05-26-m5-leaf-playables.md) (spec [`2026-05-26-m5-leaf-playables-design.md`](../docs/superpowers/specs/2026-05-26-m5-leaf-playables-design.md)).

- ✅ FmodPlayable leaf — `apps/_MyApp_/src/Audio/FmodPlayable.{h,cpp}` (M5 Task 6 rename + 활성화). FmodStudioPlayable 동반.
- ✅ EffekseerPlayable leaf — `apps/_MyApp_/src/VFX/EffekseerPlayable.{h,cpp}` (M5 Task 6 + TrackPolicy enum).
- ✅ Composite 트리 시각 검증 — M5 Task 15~16 Sequence/Parallel 시나리오 부착 (TweenPlayable leaf 도 동반 도입).
- ❌ 단위 테스트 — [[no_auto_tests]] 정책상 의도적 제외 (변경 없음 — M5 Task 5 는 *기존* 테스트 빌드 영향 검증만).

### Render Phase 1 (부수작업, 2026-05-25 ~ 2026-05-26)

M3 직후 ~ M3.5 사이 진행된 render 모듈 정비 — spec/마일스톤 외 사용자 부수 작업. *spec 외* 라 본 보고서 *기록만* (마일스톤 진행도 무관).

| Commit | 영역 |
|---|---|
| `fc4ad0a` | refactor(scene/render): Camera 의 RenderTarget 의존 역전 — Framebuffer 직접 의존 제거 |
| `73ec685` | refactor(render): DrawCommand 를 MeshRenderer 단일 의존으로 통합 — program/mesh/material/actor 직접 필드 제거 |
| `251277c` | chore(_MyApp_): 빌드 잡음 청소 — physics_movement.cpp empty 삭제 + duplicate library 경고 silencing |
| `7b95332` | docs(render): DeviceContext / Uniforms / Program 책임 경계 명시 (Phase 1) |
| `e95be90` | dev: remove observer program - material (Material 의 Program observer 패턴 제거) |
| `642040b` | fix(material): Program 참조 복구 + EagerBuild 도입 — Observer 제거 후속 |
| `6c6c243` | dev: material eager delete |

---

## M4 — 본격 배선 완료 (2026-05-26)

**arch-correction (D1)**: `PlayerStateMachine` 미도입 → `PlayerBehavior : PlayableBase` flat 메서드로 대체. `SJH::fsm` 은 Enemy/Stage 전용.  
**정본 spec**: [`docs/superpowers/specs/2026-05-26-m4-player-behavior-design.md`](../docs/superpowers/specs/2026-05-26-m4-player-behavior-design.md) (11 결정)

### 완료

| 작업 | 위치 | 비고 |
|---|---|---|
| **T1** `IntervalPlayable` | `src/playable/interval_playable.{h,cpp}` | ✅ 엔진 코어 leaf — N초 대기 후 `finished_=true`. `elapsed_` 누적은 `PlayableBase::Update` 담당 |
| **T2** `AppendInterval(float)` | `src/playable/composite_playable.{h,cpp}` | ✅ DOTween 정통 — `return Append(make_unique<IntervalPlayable>(s))`. `CMakeLists.txt` 에 `interval_playable.cpp` 추가 |
| **T3** `SpriteSequencePlayable` 다중 클립 | `src/sprite/sprite_sequence_playable.{h,cpp}` | `RegisterClip/PlayClip/RegisterOnClipEnter` API |
| **T4** `PlayerBehavior` Component | `apps/_MyApp_/src/Entity/Player/` | `PlayableBase` 상속, flat Idle/Move/Attack/Hit/Dash/Die |
| **T5** `BulletSpawnPlayable` | `apps/_MyApp_/src/Entity/` | leaf — OnPlay = bullet spawn + finished_=true |
| **T6** Bullet Actor + 시스템 | `apps/_MyApp_/src/Entity/` | `BulletLifetime` + `BulletContactHandler` + factory |
| **T7** Enemy Actor + `SimplePursueAI` | `apps/_MyApp_/src/Entity/Monster/` | FSM 없음, 단일 컴포넌트 추적 AI |
| **도메인 선행** Stat/Interfaces/Life/Movement/Weapon | `198a311` `5198096` `2eb4150` | M4 본격 전 완료 |

**D7 WaveController**: M4 제외 → M6/M7 위임. M4 `startup()` 에서 수동 enemy 1~2 spawn 으로 대체.

### 잔여

- ❌ 시각 검증 미수행 (WASD이동/공격/Dash/피격/적 접촉 확인)
- ❌ `PlayerStateMachine` → **arch-correction(D1) 으로 설계 폐기** (flat `PlayerBehavior` 가 대체)

---

## Render Pipeline 정착 (2026-05-27 ~ 2026-05-31) — 대거 미반영 작업

> 2026-05-26 ~ 2026-05-31 사이 *렌더 파이프라인 재설계* 가 집중 진행됐으나 본 진행 보고서에 누락되어 있었음. 아래는 commit 기준 역추적 정리. (브랜치도 이 기간에 `game/module/rendertarget` → `game/module/ingame/temp` 로 이동.)

### 정착 commit chain (신→구)

| Commit | 작업 | 대응 spec | 상태 |
|---|---|---|---|
| `592e99b` | **ParticleStage 신규** (Effekseer→sceneFB 합성 stage) | [`2026-05-27-particle-stage`](../docs/superpowers/specs/2026-05-27-particle-stage-design.md) | 🟡 **클래스만** — `apps/_MyApp_/src/VFX/ParticleStage.{h,cpp}` 커밋됨. **그러나 main.cpp `mStages` 미배선** — `VFX().Draw()` 가 stages 순회 *밖* 에서 직접 호출 잔존 → 파티클 PostFX 미적용 (spec 동기 미해소) |
| `0dc43c6` | 리소스 정리 | — | ✅ |
| `614b366` | main.cpp 분할 | — | ✅ |
| `2af7efb` `4ebd2dd` | **Client `Director` → `Manager` rename** | — (M5 spec §6.2 의 `TopdownShooter::Director` 를 `Manager` 로 변경) | ✅ — `apps/_MyApp_/src/Manager.{h,cpp}`. **주의: 헤더 가드(`_TOPDOWNSHOOTER_DIRECTOR_H__`)·주석·spec·메모리·EngineAPI 가 아직 `Director` 로 기재 — 용어 불일치** |
| `cd8af99` | RenderState 리팩토링 | — | ✅ |
| `7874127` | bypass postfx | [`2026-05-27-pass-component-2camera`](../docs/superpowers/specs/2026-05-27-pass-component-2camera-design.md) | ✅ disabled PassComponent bypass blit |
| `57f5779` | **2-Camera 분리** (World Perspective + Screen Ortho) | 동상 | ✅ (버그 회고: `doc/design/GammaStucked.md`) |
| `9e82e33` | **Post FX 통합** (PassComponent + ScreenQuadStage) | 동상 | ✅ — `src/render/pass_component.h`(header-only) + `screen_quad_stage.{h,cpp}` + `mesh_pass_processor` 분기. `PostFXPass`/`SetPostFXChain` 폐기 (메모리 [[pass_component_postfx_pattern]]) |
| `7bdd30a` | **Editor GUI & Game GUI 분리** | plan [`2026-05-27-imgui-layer-separation`](../docs/superpowers/plans/2026-05-27-imgui-layer-separation.md) | ✅ — `apps/_MyApp_/src/UI/` (PostFXDebugLayer 등) |
| `5f1541c` | **Render Stage 리팩토링** (CameraStage 정착) | [`2026-05-27-sp-renderstage-camera-stage`](../docs/superpowers/specs/2026-05-27-sp-renderstage-camera-stage-design.md) | ✅ — `src/render/camera_stage.{h,cpp}` + `render_stage.{h,cpp}` + `IRenderStage` mStages 패턴 |
| `2924263` | 테스트용 벙커 | — | ✅ |
| `eb85809` | **SP-UniversalRenderTarget Phase A+B** | ⚠ **spec/plan 부재** | ✅ 코드 정착. Camera RT non-null 강제 + ScreenQuadStage 흐름 (SP-SceneContext spec §12 의 후속 SP 후보였으나 spec 없이 구현됨) |
| `c0e9922` | 컨벤션 | — | ✅ |
| `537a323` | **스테이지 구성** (Stage Builder) | [`2026-05-26-stage-builder-refactor`](../docs/superpowers/specs/2026-05-26-stage-builder-refactor-design.md) | ✅ — `Stage/StageBuilder.{h,cpp}` + `StageConfig.h` + `Stage/Factories/{wall,pickup}_factory.h` (Physics/→이동) + `Stage/Components/StageStateComponent.h` + `WaveController.{h,cpp}` + `Stage/State/` stub 3종 |
| `7d5dd2e` `57e6580` | **SceneContext + ResourceRegistry::GetAllPrograms() + MAX_*_LIGHTS=16** | [`2026-05-26-sp-scenecontext`](../docs/superpowers/specs/2026-05-26-sp-scenecontext-design.md) | ✅ — 해당 spec §13 변경 기록에 구현 완료 명시됨 |

### 발견된 갱신/정합성 갭

1. **ParticleStage 미배선** — 클래스는 있으나 `mStages` 컬렉션에 insert 되지 않음. spec [`2026-05-27-particle-stage`](../docs/superpowers/specs/2026-05-27-particle-stage-design.md) §4.5 의 (b) insert + (c) 기존 `VFX().Draw()` 삭제가 *미수행*. **다음 작업 1순위**.
2. **`SP-UniversalRenderTarget` spec 부재** — `eb85809` 가 spec/plan 없이 구현됨. 사후 spec 작성 권장 (Camera RT non-null + ScreenQuadStage compositor 정통).
3. **`Director` → `Manager` 용어 불일치** — Client 싱글톤이 rename 됐으나 모든 spec / `MEMORY.md` / `EngineAPI.md` / `.claude/CLAUDE.md` + `Manager.h` 헤더 가드가 여전히 `Director` 로 기재. 문서 일괄 정정 필요.
4. **Entity 디렉토리 명** — spec(M4) 은 `Entity/Monster/` 였으나 실제는 `Entity/Enemy/`. spec/CLAUDE.md 표기와 코드 불일치.
5. **`Entity/State/` 부재** — arch-correction D1 로 `PlayerStateMachine` 폐기됐으므로 *의도된 부재* (정합).

---

## 사용자 부수 작업 (spec M 분류 외)

| Commit | 작업 | 매핑 |
|---|---|---|
| `198a311` | Stat Modifier System (Algebraic::Numeric::Stat) | M4 도메인 선행 |
| `5198096` | 스텟 데이터 연산자 | M4 도메인 선행 |
| `5478421` | Movement Input/Logic 분리 | M2 P2 정착 |
| `aa8512d` | `.gitignore` 업데이트 | 인프라 |
| `a109bfd` | `SJH::ChdirToExecutableDir` 추출 (5개 main.cpp 중복 제거) | 인프라 정착 |

---

## M3 — 완료 (2026-05-25)

**Plan**: [`docs/superpowers/plans/2026-05-25-M3-physics-box2d.md`](../docs/superpowers/plans/2026-05-25-M3-physics-box2d.md)

### 산출 (5 commits)

| Commit | 영역 |
|---|---|
| `def527c` | Box2D v2.4.1 통합 (Client 한정) + Unity isTrigger 패턴 — `MyApp::Physics` STATIC 라이브러리 신설 |
| `1a874f0` | `IContactable` / `IPhysicsContactListener` 중복 인터페이스 통합 (follow-up) |
| `3ef1dab` | `Physics::Filter` constexpr uint16 → `enum class PhysicsLayer : uint64_t` |
| `538b317` | `PhysicsBodyComponent` (단일) → `Components::Physics` abstract + `BoxBody/CircleBody` 구체화 |
| `9aaa787` | wall + pickup 시각화 — `simple.vs/fs` 단색 평면 MeshRenderer |

### 모듈 구성 (`apps/_MyApp_/src/Physics/`)

| 파일 | 책임 |
|---|---|
| `Components.Interfaces.h` | `IContactable` — Unity MonoBehaviour OnTriggerEnter/OnCollisionEnter 정통 (4 콜백 default empty) |
| `PhysicsComponent.h` | `Components::Physics` abstract base — b2Body 라이프사이클 + `SetBody/SetHeightOffset/SetSensor` + getters + `FindPhysics(Actor*)` polymorphic 헬퍼 |
| `PhysicsComponent.Imp.h` | `Components::BoxBody / CircleBody` concrete — 형태 태그 (instantiable) |
| `physics_system.{h,cpp}` | `PhysicsSystem` — `b2World` owner + `Init/Step/Shutdown/SyncToTransform` + `PhysicsContactListener` 설치 |
| `physics_movement.{h,cpp}` | `PhysicsMovement : IMovable` — `DoForward` 가 `SetLinearVelocity` 갱신 |
| `contact_listener.{h,cpp}` | `PhysicsContactListener : b2ContactListener` — `IsSensor()` 분기 → `IContactable` 디스패치 |
| `wall_factory.h` | `CreateWallActor` — b2_staticBody + box shape (Solid) + `BoxBody` |
| `pickup_factory.h` | `CreatePickupActor` — b2_staticBody + box shape (Sensor) + `PickupTriggerLogger` |
| `filter.h` | `enum class PhysicsLayer : uint64_t` (Player/Enemy/Wall/Pickup/BulletPlayer/BulletEnemy) + `operator\| / & / ~` + `ToBits()` Box2D 어댑터 + `PlayerMask/EnemyMask/WallMask` 조합 |

### spec 결정 진화 (회고)

spec §4 + 결정 #18 의 원본 의도와 실제 정착 사이의 차이:

| 항목 | spec 결정 #18 (원본) | 실제 정착 | 진화 이유 |
|---|---|---|---|
| body 컴포넌트 | `PhysicsBodyComponent` 단일 클래스 | `Components::Physics` abstract + `BoxBody/CircleBody` concrete | shape 타입을 컴파일타임에 구분 → M4 총알 (Circle) 도입 시 자연 확장 |
| 충돌 디스패치 인터페이스 | spec 미정의 | `IContactable` (4 콜백 default empty) | Unity MonoBehaviour 정통 — 선택적 override |
| Layer 필터 | uint16 constexpr (spec 부록 D `Filter::PLAYER`) | `enum class PhysicsLayer : uint64_t` + `ToBits()` 어댑터 | 강타입 (`operator\|` 보존), 미래 확장 여유 64-bit |
| Polymorphic body lookup | spec 미정의 | `FindPhysics(Actor*)` — `ForEachComponent` + `dynamic_cast<Physics*>` | `Actor::GetComponent<T>` 가 `type_index` 정확 매치라서 base 질의 불가 회피 |
| Engine core 변경 | spec 미정의 | `src/scene/actor.h` 에 `ForEachComponent<Fn>` template 추가 | contact dispatch 가 모든 component 순회 필요 |

### 검증 (2026-05-25 사용자 확인)

- ✅ Solid 충돌 — Player 가 4개 회색 벽에 부딪혀 정지
- ✅ Trigger 감지 — 노란 Pickup 영역 진입/이탈 시 `[Pickup] OnTriggerEnter/Exit — other='PlayerSprite'` 로그 (Enter/Exit 정확히 페어링)
- ✅ 시각화 — `simple.vs/fs` (MVP + baseColor) + `Mesh::CreatePlane` XZ 평면 + Transform.Scale 로 box 크기 매칭

### M3 미수행 (보류)

- ❌ 두 dynamic body 충돌 → 튕김 검증 — spec §4 검증 항목 #5. 적/총알 도입 (M4) 시점에 자연 검증.
- ❌ `CircleBody` 실사용 — concrete 클래스 정착만, 사용처는 M4 총알.
- ❌ `physics_movement.cpp` empty 파일 — 헤더 inline 만이라 `(no symbols)` 경고 매 빌드. 향후 정리.

---

## 다음 작업 — ParticleStage 배선 → M4 시각 검증 → 문서 정합 → M6

**현재 상태 (2026-05-31)**: M5 완료 + M4 본격 배선 완료 + Render Pipeline 대거 정착. Bullet/Enemy/WaveController/PlayerBehavior/multi-clip + 2-Camera PostFX + ImGui 분리 + Stage Builder 모두 구현됨.

### 즉시 할 것 0순위 — ParticleStage 배선 마무리

`apps/_MyApp_/main.cpp` 의 `mStages` 에 `ParticleStage` 를 worldCam stage 와 screenCam stage *사이* 에 insert + 기존 `VFX().Draw()` 직접 호출(line 222 부근) 삭제 (spec [`2026-05-27-particle-stage`](../docs/superpowers/specs/2026-05-27-particle-stage-design.md) §4.5). 현재 파티클이 PostFX 체인을 건너뛰는 상태 (gamma/sobel/invert 미적용).

### 즉시 할 것 — 시각 검증

```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

확인 항목:
- WASD 이동 → Move 클립 전환, 정지 → Idle 클립 복귀
- 마우스 좌클릭 → Attack 클립 + 총알 발사 (NDC 방향)
- Shift → Dash (WASD 방향 기반, 쿨타임 0.8s)
- 피격(적 접촉) → Hit 클립 → Idle 복귀
- 웨이브: 3초마다 적 최대 5마리, 전멸 시 Wave++ 로그

### M4 잔여 (선택)

| Task | 우선도 | 위치 |
|---|---|---|
| `PlayerStateMachine` (Stage 4 FSM + IPlayable 결합) | 선택 — flat PlayerBehavior 로 현재 동작 | `apps/_MyApp_/src/Entity/State/` |
| Bullet spawn cost (chrono) → 풀 도입 여부 | 낮음 | — |

### M6 — 시퀀스 빌더 + 사운드 본격

Attack Playable 체인에 Effekseer muzzle + FMOD Laser 연결. main.cpp 의 G 키 Parallel(TweenShake ∥ FmodStudio.Damaged) 패턴을 PlayerBehavior::Hit 에 연결.

### M7 — Stage FSM

`apps/_MyApp_/src/Stage/State/StageFSMState.h` stub (IFsmState<Stage> 시그니처) → WaveController 와 연결, 시작/진행/클리어/게임오버 상태 구현.

---

## 변경 기록

| 일자 | 변경 |
|---|---|
| 2026-05-25 | 초안 — M1+M2 완료 + M3.5/M4 선행 통합 진행 보고서 + M3 진입 예고 |
| 2026-05-25 | **M3 완료 반영** — 5 commit 산출 + spec 결정 #18 진화 회고 (PhysicsBodyComponent → Components::Physics + BoxBody/CircleBody) + Carrier::Projectile 스켈레톤 명시 → M4 진행도 15% → 20% |
| 2026-05-26 | **M3.5 본격 정착 반영** — IPlayable spec (7 결정) + SJH::playable 모듈 신설 + sprite_sequence 통합 + SpriteAnimator → SpriteSequencePlayable 마이그레이션 (8 commits 산출). M3.5 진행도 25% → 90%. **다음 작업 = M5 Effekseer/FMOD leaf Playable** 로 우선순위 전환. Render Phase 1 부수작업 (7 commits) 신설. M7 Stage FSM stub (3%) + M4 디렉토리 준비 (+5%) 추가 |
| 2026-05-26 | **M4 본격 배선 완료** — SpriteSequencePlayable multi-clip API (RegisterClip/PlayClip) + PlayerBehavior(Idle/Move/Attack/Hit/Dash/Die + 속도 기반 클립 자동 전환) + BulletSpawnPlayable(leaf + factory 델리게이트) + Bullet 시스템(BulletLifetime/ContactHandler/factory) + Enemy 시스템(SimplePursueAI/ContactHandler/factory) + WaveController(3s 스폰/웨이브++) + main.cpp 완전 배선(4-clip RegisterClip/PlayerBehavior Init/BulletSpawn/WaveController/Attack NDC dispatch/Shift Dash). 빌드 성공. M4 진행도 25% → 65%. **잔여**: PlayerStateMachine(선택) + 시각 검증 |
| 2026-05-26 | **M4 T1·T2 엔진 반영 + arch-correction 문서화** — `IntervalPlayable` (`src/playable/interval_playable.{h,cpp}`) + `SequencePlayable::AppendInterval` 신설. **arch-correction D1**: `PlayerStateMachine` 설계 폐기 (flat `PlayerBehavior` 대체) — EngineAPI.md §3.15 FSM 결합 패턴 갱신. M4 spec 링크 추가 (`2026-05-26-m4-player-behavior-design.md`). M4 상세 섹션 현행화 |
| 2026-05-31 | **M5 위임항목 완료 반영 + Render Pipeline 대거 정착 반영 (장기 미갱신 복구)** — (1) M3.5 "미수행 (M5 로 위임)" 섹션의 stale ❌ 3종(FmodPlayable/EffekseerPlayable/Composite 시각검증)을 M5 완료(✅)로 정정 — M5 plan `2026-05-26-m5-leaf-playables.md` Task 6/15~16 대응. (2) 2026-05-26~31 의 13 commit 역추적 §"Render Pipeline 정착" 신설: Stage Builder(`537a323`) / SceneContext+GetAllPrograms(`7d5dd2e`·`57e6580`) / SP-UniversalRenderTarget(`eb85809`, spec 부재) / RenderStage CameraStage(`5f1541c`) / ImGui 분리(`7bdd30a`) / 2-Camera PassComponent PostFX(`9e82e33`·`57f5779`·`7874127`·`cd8af99`) / Director→Manager rename(`2af7efb`) / main 분할(`614b366`) / ParticleStage(`592e99b`, 미배선). 진행 요약표 3행 추가 + 다음작업 0순위(ParticleStage 배선) 신설 + 브랜치명 `game/module/ingame/temp` 정정. **정합성 갭 5종 명시**. ⚠ 직전 멀티-Edit 지연으로 손상됐던 파일을 `git checkout HEAD` 로 복구 후 순차 재적용 |
