#include "fsm/state_machine_processor.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

namespace
{
    // 테스트용 enum — Idle/Move/Attack 3 상태
    enum class TestState : uint64_t {
        NONE   = 0,
        Idle   = 1ull << 0,
        Move   = 1ull << 1,
        Attack = 1ull << 2,
    };
    enum class TestTransit : uint64_t {
        ToIdle   = (uint64_t)TestState::Move | (uint64_t)TestState::Attack,
        ToMove   = (uint64_t)TestState::Idle,
        ToAttack = (uint64_t)TestState::Idle | (uint64_t)TestState::Move,
    };

    /// @brief OnEnter / OnExit 발화 추적용 spy FSM
    class SpyFSM : public SJH::FSM::StateMachineProcessor<TestState, TestTransit>
    {
    public:
        SpyFSM() : SJH::FSM::StateMachineProcessor<TestState, TestTransit>(TestState::Idle)
        {
            Bind(TestTransit::ToIdle,   TestState::Idle);
            Bind(TestTransit::ToMove,   TestState::Move);
            Bind(TestTransit::ToAttack, TestState::Attack);
        }

        std::vector<TestState> EnterLog;
        std::vector<TestState> ExitLog;

        void OnEnter(TestState s) override { EnterLog.push_back(s); }
        void OnExit(TestState s)  override { ExitLog.push_back(s); }
    };
}

TEST_CASE("StateMachineProcessor — initial state via constructor", "[fsm]")
{
    SpyFSM fsm;
    REQUIRE(fsm.State() == TestState::Idle);
}

TEST_CASE("StateMachineProcessor — TryTransit AND 판정", "[fsm]")
{
    SpyFSM fsm;

    SECTION("Idle → Move 가능 (ToMove 의 from = Idle)") {
        REQUIRE(fsm.TryTransit(TestTransit::ToMove));
        REQUIRE(fsm.State() == TestState::Move);
    }
    SECTION("Idle → Attack 가능 (ToAttack 의 from = Idle | Move)") {
        REQUIRE(fsm.TryTransit(TestTransit::ToAttack));
        REQUIRE(fsm.State() == TestState::Attack);
    }
    SECTION("Idle → Idle 불가 (ToIdle 의 from = Move | Attack)") {
        REQUIRE_FALSE(fsm.TryTransit(TestTransit::ToIdle));
        REQUIRE(fsm.State() == TestState::Idle);
    }
}

TEST_CASE("StateMachineProcessor — OnEnter / OnExit 발화 순서", "[fsm]")
{
    SpyFSM fsm;

    REQUIRE(fsm.TryTransit(TestTransit::ToMove));
    REQUIRE(fsm.ExitLog.size() == 1);
    REQUIRE(fsm.ExitLog[0] == TestState::Idle);
    REQUIRE(fsm.EnterLog.size() == 1);
    REQUIRE(fsm.EnterLog[0] == TestState::Move);

    REQUIRE(fsm.TryTransit(TestTransit::ToAttack));
    REQUIRE(fsm.ExitLog.size() == 2);
    REQUIRE(fsm.ExitLog[1] == TestState::Move);
    REQUIRE(fsm.EnterLog.size() == 2);
    REQUIRE(fsm.EnterLog[1] == TestState::Attack);

    REQUIRE_FALSE(fsm.TryTransit(TestTransit::ToMove));   // Attack → Move 불가
    REQUIRE(fsm.ExitLog.size() == 2);
    REQUIRE(fsm.EnterLog.size() == 2);
}

TEST_CASE("StateMachineProcessor — NONE 상태에서 transit 실패", "[fsm]")
{
    SJH::FSM::StateMachineProcessor<TestState, TestTransit> fsm;   // default = NONE
    fsm.Bind(TestTransit::ToMove, TestState::Move);
    REQUIRE(fsm.State() == TestState::NONE);
    REQUIRE_FALSE(fsm.TryTransit(TestTransit::ToMove));
}

TEST_CASE("StateMachineProcessor — target 미등록 transit 실패", "[fsm]")
{
    SJH::FSM::StateMachineProcessor<TestState, TestTransit> fsm(TestState::Idle);
    REQUIRE_FALSE(fsm.TryTransit(TestTransit::ToMove));
    REQUIRE(fsm.State() == TestState::Idle);
}
