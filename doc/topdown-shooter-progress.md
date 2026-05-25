# Topdown Shooter — 마일스톤 진행 보고서

> **최종 갱신**: 2026-05-25 (M3 완료 반영)
> **브랜치**: `game/module/sprite`
> **관련 spec**: [`docs/superpowers/specs/2026-05-24-topdown-shooter-design.md`](../docs/superpowers/specs/2026-05-24-topdown-shooter-design.md) §부록 D
> **FSM 정본 spec** (Stage 4 진화): [`docs/superpowers/specs/2026-05-25-fsm-object-state-machine-design.md`](../docs/superpowers/specs/2026-05-25-fsm-object-state-machine-design.md)
> **M3 plan**: [`docs/superpowers/plans/2026-05-25-M3-physics-box2d.md`](../docs/superpowers/plans/2026-05-25-M3-physics-box2d.md)

## 진행 요약

| M | 범위 | 상태 | 진행도 |
|---|---|---|---|
| **M1** 빌드 + Sprite + 빌보드 1장 정적 | ✅ 완료 | 100% |
| **M2** Actor+Component+Input+FSM+follow | ✅ P2 정착 (FSM 코어는 Stage 4 진화) | ~90% |
| **M3** Box2D 물리 (Client 한정) | ✅ **완료 + spec 결정 진화** | 100% |
| **M3.5** Playable + sprite_sequence | 🟡 SpriteAnimator 경량 도입 | ~25% |
| **M4** PlayerStateMachine + 발사 + 적 | 🟡 도메인 선행 (Stat/Entity Components + Projectile 스켈레톤) | ~20% |
| **M5** Effekseer + FMOD + Tweeny Playable | ❌ 미시작 | 0% |
| **M6** 시퀀스 빌더 + 사운드 본격 | ❌ 미시작 | 0% |
| **M7** GameFSM + 보스 + 종료 | ❌ 미시작 | 0% |

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

## M3.5 — 경량 도입 (이번 세션)

원본 spec §1.6 의 `SJH::playable` + `SJH::sprite_sequence` 별도 모듈 + Playable 트리 (Composite) 패턴은 *미도입*. 단 *경량 대체* 로 `SpriteAnimator` (자동 wrap-around) 가 `src/sprite/` 안에 직접 도입.

| 항목 | 상태 |
|---|---|
| `SJH::playable` 모듈 (Playable / Sequence / Parallel / PlayerComponent / TickSystem) | ❌ 미시작 |
| `SJH::sprite_sequence` 모듈 (SpriteFrameClip + SpriteSequencePlayable) | ❌ 미시작 |
| `SpriteAnimator` (경량, src/sprite/ 내부) | ✅ `c4fd046` |
| Composite 트리 (Sequence + Parallel chaining) | ❌ — M5/M6 시점 |
| 단위 테스트 | ❌ |

**Trade-off**: SpriteAnimator 가 *단일 atlas loop/clamp* 만 — 시퀀스 chaining 필요해지면 M3.5 본격 도입. M5/M6 의 Effekseer/FMOD Playable 도입 시 함께 정착 권장.

---

## M4 — 도메인 선행 (이번 세션)

PlayerStateMachine + 발사 + 적 *본격 구현 미시작*. 단 *Entity 도메인* 의 선행 작업 완료.

### 완료 (도메인 선행)

| Commit | 영역 |
|---|---|
| `198a311` | Stat Modifier System |
| `5198096` | 스텟 데이터 연산자 |
| `5478421` | Movement Input/Logic 분리 |
| `2eb4150` | `Components.Interfaces.h` (ILivable/IDieable/IDamageable/IAttackable/IMovable) + Life/Movement/Weapon Component + `PlayerEntity` Facade + `PlayerActor` Pattern C factory |

### 남은 (본격 M4)

- ❌ `PlayerStateMachine` (`unordered_map<TState, unique_ptr<Playable>>` container — Stage 4 FSM 활용)
- ❌ Idle/Move/Attack/Die State 객체
- ❌ 마우스 클릭 발사 + Bullet Actor + b2 dynamic body + ray cast
- ❌ 적 1종 + 간단 AI
- ❌ Bullet spawn cost 측정 (chrono) → 풀 도입 여부 결정

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

## 다음 작업 — M4 본격 (PlayerStateMachine + 발사 + 적)

### 도메인 사전 완료 (재확인)

| Commit | 영역 |
|---|---|
| `198a311` | Stat Modifier System (Algebraic::Numeric::Stat) |
| `5198096` | 스텟 데이터 연산자 |
| `2eb4150` | Components.Interfaces.h (ILivable/IDieable/IDamageable/IAttackable/IMovable) + Life/Movement/Weapon Component + PlayerEntity Facade + PlayerActor Pattern C factory |
| **`def527c`** (M3) | PlayerActor.h 에 `PhysicsCfg` nested 추가 — physics.world / categoryBits / maskBits 주입 가능 |

### M4 본격 (남은 작업)

- ❌ `PlayerStateMachine` (`unordered_map<TState, unique_ptr<Playable>>` container — Stage 4 FSM 활용)
- ❌ Idle/Move/Attack/Die State 객체
- ❌ 마우스 클릭 발사 — `Carrier::Projectile.h` (이미 스켈레톤 존재: `IContactable` + `IDieable` 상속, OnEnter/Update 빈 구현, OnTriggerEnter 가드만) 활성화
- ❌ Bullet Actor factory — `b2_dynamicBody` + `Components::CircleBody` + `Filter::BulletPlayer` + ray cast
- ❌ 적 1종 + 간단 AI (b2_dynamicBody + `Filter::Enemy`)
- ❌ Bullet spawn cost 측정 (chrono) → 풀 도입 여부 결정

---

## 변경 기록

| 일자 | 변경 |
|---|---|
| 2026-05-25 | 초안 — M1+M2 완료 + M3.5/M4 선행 통합 진행 보고서 + M3 진입 예고 |
| 2026-05-25 | **M3 완료 반영** — 5 commit 산출 + spec 결정 #18 진화 회고 (PhysicsBodyComponent → Components::Physics + BoxBody/CircleBody) + Carrier::Projectile 스켈레톤 명시 → M4 진행도 15% → 20% |
