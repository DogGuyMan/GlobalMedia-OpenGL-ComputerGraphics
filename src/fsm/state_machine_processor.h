#ifndef __SJH_FSM_STATE_MACHINE_PROCESSOR_H__
#define __SJH_FSM_STATE_MACHINE_PROCESSOR_H__

#include "scene/actor.h"   // SJH::Scene::Component
#include <cstdint>
#include <unordered_map>

namespace SJH::FSM
{
    /// @brief 비트 enum 기반 FSM Component 베이스. spec §6.0.2 정본.
    /// @tparam TState   `enum class : uint64_t` — 각 enumerator 가 1비트 (NONE=0 약속)
    /// @tparam TTransit `enum class : uint64_t` — 각 enumerator = *허용된 from 상태들의 bitwise OR*
    ///
    /// ### 사용 패턴
    /// @code
    /// enum class PlayerState : uint64_t {
    ///     NONE = 0, Idle = 1ull<<0, Move = 1ull<<1, Attack = 1ull<<2,
    /// };
    /// enum class PlayerTransit : uint64_t {
    ///     ToIdle = (uint64_t)PlayerState::Move | (uint64_t)PlayerState::Attack,
    ///     ToMove = (uint64_t)PlayerState::Idle,
    /// };
    /// class MyFSM : public SJH::FSM::StateMachineProcessor<PlayerState, PlayerTransit> {
    /// public:
    ///     MyFSM() {
    ///         Bind(PlayerTransit::ToIdle, PlayerState::Idle);
    ///         Bind(PlayerTransit::ToMove, PlayerState::Move);
    ///     }
    ///     void OnEnter(PlayerState s) override { ... }
    /// };
    /// @endcode
    template<typename TState, typename TTransit>
    class StateMachineProcessor : public SJH::Scene::Component
    {
    public:
        using StateU = uint64_t;

        StateMachineProcessor() = default;
        explicit StateMachineProcessor(TState startup) : current_(startup) {}

        /// @brief transit → target 매핑 등록. 생성자에서 모든 transit 일괄 등록 권장.
        void Bind(TTransit transit, TState target)
        {
            targetOf_[(StateU)transit] = target;
        }

        /// @brief transition 시도. allowed-from set 에 current 가 포함되면 OnExit→상태 교체→OnEnter 발화 후 true.
        /// @return false 시 (a) current==NONE, (b) from 비매치, (c) target 미등록 중 하나.
        bool TryTransit(TTransit transit)
        {
            const StateU allowedFrom = (StateU)transit;
            const StateU curr        = (StateU)current_;
            if (curr == 0) return false;
            if ((allowedFrom & curr) != curr) return false;
            auto it = targetOf_.find((StateU)transit);
            if (it == targetOf_.end()) return false;
            OnExit(current_);
            current_ = it->second;
            OnEnter(current_);
            return true;
        }

        /// @brief 매 프레임 OnUpdate 호출 — Scene::Actor::Update 재귀가 자동.
        void Tick(float dtSeconds) { OnUpdate(current_, dtSeconds); }

        TState State() const { return current_; }

        // === Component 베이스 (SJH::Scene::Component) 의 순수 가상 구현 ===
        void OnEnter() override {}                  // Actor 진입 시 1회
        void OnExit()  override {}                  // Actor 이탈 시 1회
        void Update(float dt) override { Tick(dt); } // 매 프레임 — Tick 위임

        // === FSM 가상함수 (파생 클래스가 override) ===
        virtual void OnEnter(TState /*s*/)              {}
        virtual void OnUpdate(TState /*s*/, float /*dt*/) {}
        virtual void OnExit(TState /*s*/)               {}

    private:
        TState current_ = TState::NONE;   // enum 에 NONE = 0 약속
        std::unordered_map<StateU, TState> targetOf_;
    };

}  // namespace SJH::FSM

#endif // __SJH_FSM_STATE_MACHINE_PROCESSOR_H__
