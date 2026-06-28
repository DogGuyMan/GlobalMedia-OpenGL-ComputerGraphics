// SJH::FSM::StateMachine characterization - mock TState/TOwner/IFsmState 로 전이 계약 잠금.
// 핵심: TryTransit 은 큐잉만, Update 가 적용(OnExit->curState->OnEnter). spec D3/D8a.
#include <catch2/catch_test_macros.hpp>

#include "fsm/state_machine.h" // StateMachine + IFsmState (+ scene/actor.h 전이)

#include <cstdint>
#include <memory>

namespace
{
    // 1) TState: enum class : uint64_t, NONE=0 + 단일 비트 (StateMachine 요구사항)
    enum class MockState : uint64_t
    {
        NONE = 0,
        A = 1ull << 0,
        B = 1ull << 1,
        C = 1ull << 2,
    };

    // 2) TOwner: 스파이 카운터 보유 plain struct
    struct MockOwner
    {
        int enterA = 0, exitA = 0, updateA = 0;
        int enterB = 0, exitB = 0, updateB = 0;
    };

    // 3) 구체 IFsmState<MockOwner>: A 는 B 로만 전이 허용, B 는 A 로만
    class StateA : public SJH::FSM::IFsmState<MockOwner>
    {
      public:
        uint64_t GetStateFlag() const override { return (uint64_t)MockState::A; }
        uint64_t GetTransitFlag() const override { return (uint64_t)MockState::B; }
        void OnEnter(MockOwner &o) override { o.enterA++; }
        void OnUpdate(MockOwner &o, float) override { o.updateA++; }
        void OnExit(MockOwner &o) override { o.exitA++; }
    };
    class StateB : public SJH::FSM::IFsmState<MockOwner>
    {
      public:
        uint64_t GetStateFlag() const override { return (uint64_t)MockState::B; }
        uint64_t GetTransitFlag() const override { return (uint64_t)MockState::A; }
        void OnEnter(MockOwner &o) override { o.enterB++; }
        void OnUpdate(MockOwner &o, float) override { o.updateB++; }
        void OnExit(MockOwner &o) override { o.exitB++; }
    };

    using SM = SJH::FSM::StateMachine<MockState, MockOwner>;

    // SM 은 unique_ptr 멤버 보유(move-only) + Component 가 user-declared dtor 라 move 억제 가능.
    // 값 반환 함정을 피하려 참조로 등록한다.
    void RegisterAB(SM &sm)
    {
        sm.RegisterState(std::make_unique<StateA>());
        sm.RegisterState(std::make_unique<StateB>());
    }
}

TEST_CASE("FSM: NONE 에서는 전이 불가", "[fsm]")
{
    MockOwner owner;
    SM sm(owner, MockState::NONE);
    RegisterAB(sm);
    REQUIRE(sm.State() == MockState::NONE);
    REQUIRE_FALSE(sm.TryTransit(MockState::A)); // curr==0 -> false
    REQUIRE(sm.State() == MockState::NONE);
}

TEST_CASE("FSM: 유효 전이는 큐잉되고 Update 에서 적용", "[fsm]")
{
    MockOwner owner;
    SM sm(owner, MockState::A);
    RegisterAB(sm);
    REQUIRE(sm.State() == MockState::A);

    REQUIRE(sm.TryTransit(MockState::B)); // A.transit=B, B 등록 -> true (큐잉만)
    REQUIRE(sm.State() == MockState::A);  // 아직 적용 전
    REQUIRE(owner.exitA == 0);
    REQUIRE(owner.enterB == 0);

    sm.Update(0.016f); // ApplyPending: OnExit(A)->curState=B->OnEnter(B), 이어 OnUpdate(B)
    REQUIRE(sm.State() == MockState::B);
    REQUIRE(owner.exitA == 1);
    REQUIRE(owner.enterB == 1);
    REQUIRE(owner.updateB == 1);
}

TEST_CASE("FSM: 플래그 미허용 전이는 거부", "[fsm]")
{
    MockOwner owner;
    SM sm(owner, MockState::A);
    RegisterAB(sm);
    // A.GetTransitFlag()=B 뿐 -> C 로 전이 시 (B & C)=0 -> false
    REQUIRE_FALSE(sm.TryTransit(MockState::C));
    sm.Update(0.016f);
    REQUIRE(sm.State() == MockState::A); // 불변
}

TEST_CASE("FSM: 왕복 전이 A->B->A", "[fsm]")
{
    MockOwner owner;
    SM sm(owner, MockState::A);
    RegisterAB(sm);
    REQUIRE(sm.TryTransit(MockState::B));
    sm.Update(0.016f); // -> B
    REQUIRE(sm.State() == MockState::B);
    REQUIRE(sm.TryTransit(MockState::A)); // B.transit=A
    sm.Update(0.016f); // -> A
    REQUIRE(sm.State() == MockState::A);
    REQUIRE(owner.enterA == 1); // 복귀 시 OnEnter(A) 1회
    REQUIRE(owner.exitB == 1);
}

TEST_CASE("FSM: Update 는 현재 상태 OnUpdate 를 매번 호출", "[fsm]")
{
    MockOwner owner;
    SM sm(owner, MockState::A);
    RegisterAB(sm);
    sm.Update(0.016f); // pending 없음 -> OnUpdate(A)
    sm.Update(0.016f);
    REQUIRE(owner.updateA == 2);
}
