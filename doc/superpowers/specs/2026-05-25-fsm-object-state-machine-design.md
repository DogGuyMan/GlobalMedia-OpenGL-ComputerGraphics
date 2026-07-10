# SJH::FSM — State Machine 설계 스펙

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 작성일: 2026-05-25 (최종 갱신: 2026-05-25 — 4단계 진화 통합)
> 최종 commit: `3bdf499` (현재 design)
> 관련: M2 P2 (PlayerController/FSM, 미작업)

## 정본 — 현재 Design

전이 그래프 정보를 *각 State 객체* 안에 응집한 *Entity within Aggregate* 패턴.

### 시그니처

**`src/fsm/fsm_state.h`** — State Entity 베이스

```cpp
template <typename TOwner>
class IFsmState
{
public:
    virtual ~IFsmState() = default;
    virtual uint64_t GetStateFlag()   const = 0;   // 내가 누구인지 (e.g. PlayerState::Idle)
    virtual uint64_t GetTransitFlag() const = 0;   // 내가 갈 수 있는 곳들의 OR (e.g. Move|Attack)
    virtual void OnEnter(TOwner& owner)            = 0;
    virtual void OnUpdate(TOwner& owner, float dt) = 0;
    virtual void OnExit(TOwner& owner)             = 0;
};
```

**`src/fsm/state_machine.h`** — Aggregate Root

```cpp
template <typename TState, typename TOwner>
class StateMachine : public SJH::Scene::Component
{
public:
    using StateU = uint64_t;

    StateMachine(TOwner& owner, TState startup = TState::NONE);

    void RegisterState(std::unique_ptr<IFsmState<TOwner>> state);  // id 자가 노출
    bool TryTransit(TState target);                                // 런타임 가드
    void ForceTransit(TState target);                              // 실패 시 std::abort()
    TState State() const;

    // Component overrides — current state hook 으로 위임
    void OnEnter() override;
    void OnExit() override;
    void Update(float dt) override;

private:
    bool TryTransitImpl(StateU targetBit);
    TOwner* owner_;
    TState  current_;
    std::unordered_map<StateU, std::unique_ptr<IFsmState<TOwner>>> states_;
};
```

### 사용 패턴 (M2 P2 미리보기)

```cpp
enum class PlayerState : uint64_t {
    NONE   = 0,
    Idle   = 1ull << 0,
    Move   = 1ull << 1,
    Attack = 1ull << 2,
};

class IdleState : public SJH::FSM::IFsmState<PlayerActor>
{
    uint64_t GetStateFlag()   const override { return (uint64_t)PlayerState::Idle; }
    uint64_t GetTransitFlag() const override {
        return (uint64_t)PlayerState::Move | (uint64_t)PlayerState::Attack;
    }
    void OnEnter(PlayerActor& p) override { p.PlayAnim("idle"); }
    void OnUpdate(PlayerActor& p, float dt) override { /* poll input */ }
    void OnExit(PlayerActor& p) override {}
};
// MoveState / AttackState 도 동일 패턴

using PlayerFSM = SJH::FSM::StateMachine<PlayerState, PlayerActor>;
auto* fsm = playerActor->AddComponent<PlayerFSM>(*playerActor, PlayerState::Idle);
fsm->RegisterState(std::make_unique<IdleState>());
fsm->RegisterState(std::make_unique<MoveState>());
fsm->RegisterState(std::make_unique<AttackState>());
// RegisterTransit 호출 없음 — 그래프가 State 클래스 안에 응집

fsm->TryTransit(PlayerState::Move);   // current=Idle, IdleState.GetTransitFlag() 가 Move 포함 → 진입
```

## DDD 렌즈 매핑

본 도메인은 게임 엔진 알고리즘 — 풀-DDD 가 아닌 *전술적 패턴* 만 적용.

| DDD 개념 | FSM 매핑 |
|---|---|
| Ubiquitous Language | `State` = 식별자, `StateMachine` = Aggregate Root |
| Value Object | `TState` enum — 정수 비교, 불변 |
| **Aggregate Root** | `StateMachine` — current 트래킹 + Hook 디스패치 |
| **Entity within Aggregate** | `IFsmState<TOwner>` 파생 객체 — *self-identifying* (`GetStateFlag`) + *self-transitioning* (`GetTransitFlag`) |
| Domain Service | (없음 — 전이 로직이 Aggregate Root 의 단일 메서드로 충분) |
| Invariants (Aggregate Root 책임) | (I1) `current ∈ Registered ∪ {NONE}` · (I2) `TryTransit 성공 ⟺ current.GetTransitFlag() & target == target` · (I3) `OnExit → swap → OnEnter` 순서 |

**핵심 DDD 정합성**: 전이 그래프 = Aggregate 내부 *그래프 노드 정보* — 각 Entity 가 자기 *adjacency* 보유. Aggregate Root 가 그래프 데이터를 별도 보관하지 않음 (이전 design 의 `targetOf_` map 폐기).

**의도된 *DDD 정통 위반* 1건** (실용성 우선): `StateMachine` 이 `SJH::Scene::Component` 를 *직접 상속*. 순수 도메인이라면 어댑터 클래스가 Composition 으로 도메인 객체를 보유해야 하나, 본 프로젝트의 Component 컨벤션 (Actor 트리에 어태치 가능한 것은 Component 파생) 을 따르기 위해 직접 상속.

## 엔진 4개 레퍼런스 매핑 (context7 + WebSearch 2026-05-25)

| 엔진 | State 보관 | hook 시그니처 | Transit 결정 |
|---|---|---|---|
| **Unity** | `StateMachineBehaviour` 파생 클래스, Animator State 마다 | `OnStateEnter/Update/Exit(Animator, AnimatorStateInfo, int layer)` | Animator 그래프 + Transition Condition |
| **Unreal** | `UAnimStateNode` (애님 FSM), `UGameplayAbility` (게임플레이 FSM) | `StateEntered/Left` notify; `Activate/EndAbility/CanActivate` | Transition Rule Graph (Blueprint) |
| **Godot** | `State extends Node` (style-guide 정통) | `enter() / exit() / physics_process(delta)` | `transition_to(target_state_path)` |
| **Cocos2d-x** | 표준 없음 — 커뮤니티 GoF State Pattern | `Enter/Update/Exit(Context*)` | 직접 호출 |

### 4 엔진 공통 결론
1. **State 는 *반드시 별도 객체*** — 4/4 일치. switch-on-enum 패턴은 엔진 정통에 없음.
2. **owner/context 가 hook 인자로 전달** — 4/4 일치.
3. **비트 마스크 transit gate 는 어디에도 없음** — 모두 그래프 또는 path travel. → 본 프로젝트의 *비트 enum* 은 *코드 + 학습 가성비* 목적의 **차별점**.
4. **FSM 자체 hook ⊥ 개별 State hook 분리** — Unity 가 `OnStateMachineEnter` ↔ `OnStateEnter` 분리. 우리는 `Component::OnEnter()` ↔ `IFsmState::OnEnter(owner)` 분리.

### 본 design 의 추가 차별점 — *Self-Transitioning State*
4 엔진 중 *어느 곳에도* State 객체가 *자기 transit 규칙* 을 알지 않는다. 그래프는 항상 *외부* (Animator/StateNode/parent Node) 가 보유. 본 design 의 `IFsmState::GetTransitFlag()` 는 이 정통과 다른 *진화* — DDD 의 Aggregate 응집도를 우선시.

## 진화 History — 4 단계

본 design 은 한 번에 도달하지 않았다. 4 commit 의 점진 진화 — 각 단계의 *learning* 기록.

### Stage 1 — P1 (`78dea2a` `feat(fsm): SJH::fsm 코어 모듈 신설 + StateMachineProcessor template`)

**도입**: `StateMachineProcessor<TState, TTransit>` template — switch-on-enum 베이스, `OnEnter(TState s)/OnUpdate(TState s, float dt)/OnExit(TState s)` 가상 메서드를 Processor 자체가 override.

**Learning**: 첫 spec 작성 전 *spec 없이* 구현된 단계. 4 엔진 정통 분석 *전*. switch-on-enum 패턴은 Anemic Domain Model 경향 — State 별 데이터/행위가 한 클래스에 평탄화.

**결과**: 후속 단계에서 *폐기*. 알고리즘 (비트 AND 판정) 만 다음 단계로 흡수.

### Stage 2 — P1.5 (`16a6cdd` `feat(fsm): ObjectStateMachine + IFsmState 추가 (M2 P1.5)`)

**도입**: `IFsmState<TOwner>` + `ObjectStateMachine<TState, TTransit, TOwner>` — State 객체 + Aggregate Root 분리. 4 엔진 정통 (Unity/Unreal/Godot/Cocos2d) 흡수.

**Code Quality Reviewer 가 잡은 *진짜 design bug***: `current_` 가 `NONE` 으로 초기화 + `TryTransitImpl` 의 I1 가드가 NONE 에서 전이 차단 → spec 의 사용 예제 `ForceTransit(ToIdle)` 이 시작 시 `std::abort()`. → 생성자에 `TState startup` 인자 추가로 fix (state_machine_processor.h 의 P1 패턴 답습).

**Learning**: subagent-driven-development pipeline (implementer + spec reviewer + code quality reviewer) 가 *M2 P2 빌드 직전에 발견됐을* trap 을 *지금* 잡았다. 가치 입증.

### Stage 3 — Cleanup (`6f5346c` `refactor(fsm): 파일/클래스 명 정리 + P1 StateMachineProcessor 폐기`)

**변경**:
- `i_fsm_state.h` → `fsm_state.h` (헤더 가드 `__SJH_FSM_FSM_STATE_H__`)
- `object_state_machine.h` → `state_machine.h` (헤더 가드 `__SJH_FSM_STATE_MACHINE_H__`)
- 클래스 `ObjectStateMachine` → `StateMachine`
- `state_machine_processor.h` + `test_fsm.cpp` 폐기 (Stage 1 흔적 제거)

**Learning**: grep 결과 `StateMachineProcessor` 사용처 = `test_fsm.cpp` 자기 자신뿐. spec 의 "Timer 류 단순 FSM 공존" 정당화가 *방어적 정당화* (P1 commit 회귀 부담 회피) 였음을 인정. YAGNI 적용 + 사용자의 *공존 의문 제기* 가 이 정리를 촉발.

### Stage 4 — TTransit 제거 (`3bdf499` `refactor(fsm): TTransit 제거 + GetTransitFlag 기반 그래프 정보 응집`)

**변경**:
- IFsmState 에 `GetStateFlag()` / `GetTransitFlag()` *const* 추가 — State 가 그래프 노드 정보 자체 보유
- StateMachine 의 template 파라미터: `<TState, TTransit, TOwner>` → `<TState, TOwner>`
- `RegisterState(TState id, ...)` → `RegisterState(...)` — state 가 알아서 자기 id 노출 (이중 진실 해소)
- `RegisterTransit(t, target)` + `targetOf_` map *전체 제거*
- `TryTransitImpl` 단순화: `current.GetTransitFlag() & target` 단일 비트 검사

**Learning**: Code quality reviewer 가 Stage 2 에서 우려한 *이중 진실* (State 객체와 Aggregate map 양쪽에 enum 식별자 보유) 이 *RegisterTransit 호출* 에도 존재했음. 사용자가 그래프 응집의 가능성을 발견 — DDD 의 Entity within Aggregate 정통에 더 정확히 부합.

## P1 → 현재 비교 표

| 측면 | Stage 1 (P1) | Stage 4 (현재) |
|---|---|---|
| State 객체화 | ❌ (switch-on-enum) | ✅ (`IFsmState<TOwner>` 파생) |
| 그래프 정보 위치 | (없음 — TTransit enum 값이 from-bit OR) | **각 State 의 `GetTransitFlag()`** |
| Aggregate Root 책임 | switch-on-enum 디스패치 + 전이 검증 | current 트래킹 + Hook 디스패치만 |
| 빌더 호출 수 (per state) | 1 (`Bind`) | 1 (`RegisterState`) — 동일 |
| owner hook 전달 | ❌ | ✅ (`TOwner&`) |
| Template 파라미터 수 | 2 (`TState, TTransit`) | 2 (`TState, TOwner`) — TTransit 제거 |
| DDD 준수도 | ⭐⭐ (Anemic 경향) | ⭐⭐⭐⭐ (Entity within Aggregate) |
| 엔진 정통 일치 | 0/4 | 4/4 + 자체 진화 (Self-Transitioning State) |

## 거부된 대안 (검토 했으나 채택 안 함)

| 대안 | 출처 | 거부 이유 |
|---|---|---|
| Animator 그래프 / Transition Rule Graph | Unity / Unreal | Blueprint·에디터 의존. C++17 코어에 부적합. *비트 enum 코드* 가 등가 표현. |
| `transition_to(path)` | Godot | Node tree path 의존. Component 단위 모델에 부적합. `TryTransit(TState)` 가 등가. |
| `UGameplayAbility::CanActivate` (State self-veto 가드) | Unreal GAS | YAGNI. 필요해지면 `virtual bool CanEnter(TOwner&)` 비파괴 추가 가능. |
| Hierarchical FSM (sub-state machine) | Godot / Cocos | 사용처 (Player / Timer) 가 평탄 FSM 으로 충분. |
| Port interface (`IPlayerActions`) — 제안 C | (자체 brainstorm) | 다중 Actor 가 같은 FSM 재사용 시점에 *진화*. M2 P2 단계엔 YAGNI. |
| `StateMachineProcessor` switch-on-enum 공존 | Stage 1 보존 의도 | 사용처 0 + 엔진 정통 위배. Stage 3 에서 폐기. |

## 후속 작업

| 작업 | 우선순위 | 위치 |
|---|---|---|
| **M2 P2** PlayerController + PlayerFSM (본 design 첫 실제 인스턴스화) | 다음 세션 | `apps/_MyApp_/src/InputHandler/`, `apps/_MyApp_/main.cpp` |
| `IFsmState::CanEnter(TOwner&) → bool` 옵션 가드 | M2 P2 이후, 필요 시 | `src/fsm/fsm_state.h` 비파괴 확장 |
| HFSM (sub-state machine) | 별도 spec, 사용처 등장 시 | — |
| Port interface 추출 (다중 Actor FSM 재사용) | M2 P2 + N | `src/fsm/` 또는 별도 모듈 |
| 단위 테스트 (사용자 요청 시) | M2 P2 작업 후 사용자 판단 | `<test>/test_state_machine.cpp` |

## 변경 기록

| 일자 | 변경 | Commit |
|---|---|---|
| 2026-05-25 (초안) | M2 P1.5 design 작성 — ObjectStateMachine + IFsmState (Stage 2) | (spec 초안) |
| 2026-05-25 (1차 갱신) | 4단계 진화 통합 — 정본 + history + learning 기록 | `3bdf499` |
