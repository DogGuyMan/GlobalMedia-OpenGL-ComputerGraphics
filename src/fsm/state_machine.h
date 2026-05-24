#ifndef __SJH_FSM_STATE_MACHINE_H__
#define __SJH_FSM_STATE_MACHINE_H__

#include "fsm/fsm_state.h"
#include "scene/actor.h"
#include <cassert>
#include <cstdint>
#include <cstdlib> // std::abort
#include <memory>
#include <unordered_map>

namespace SJH::FSM
{
	/// @brief Aggregate Root — State Entity 들 + current 보호.
	/// @tparam TState `enum class : uint64_t` — 각 enumerator 가 1비트 (NONE=0 약속)
	/// @tparam TOwner Actor 파생 클래스 — State hook 의 owner 인자 타입
	///
	/// ### 그래프 정보 응집 (DDD)
	/// 전이 그래프 정보 (어디서 어디로 갈 수 있는지) 는 *각 State 객체 자체* 가 보유한다
	/// (`IFsmState::GetStateFlag()` = 내가 누구, `IFsmState::GetTransitFlag()` = 내가 갈 수 있는 곳들의 OR).
	/// StateMachine 은 *current 트래킹* + *Hook 디스패치* 만 책임진다.
	///
	/// ### 불변식 (Aggregate Root 책임)
	/// - I1: current ∈ Registered(states_) ∪ {NONE}
	/// - I2: TryTransit 성공 ⟺ (current.GetTransitFlag() & target_bit) == target_bit AND target 등록됨
	/// - I3: 전이 시 항상 OnExit(current) → current = target → OnEnter(current) 순서
	///
	/// ### 자가 검증
	/// `RegisterState(unique_ptr<IFsmState>)` 는 *id 매개변수 없음* — state 가 알아서 자기 ID 노출.
	/// 같은 ID 비트가 두 번 등록되면 silently overwrite (assert 권장 위치는 호출 측 빌더).
	///
	/// ### TryTransit vs ForceTransit
	/// - TryTransit: 런타임 가드. 실패 시 false. 입력 핸들러용.
	/// - ForceTransit: 계약 위반 검출. 실패 시 std::abort(). 시나리오 스크립트용.
	///
	/// ### owner_ 라이프타임 약속
	/// `owner_` 는 *non-owning* 백포인터. 호출자는 owner 의 lifetime 이 본 Component 의 lifetime 을
	/// 엄격히 outlive 함을 보장해야 한다. Actor/Component 패턴 (Actor 가 unique_ptr<Component> 소유)
	/// 하에서는 Component 가 Actor 보다 먼저 파괴되어 자연스럽게 충족.
	template <typename TState, typename TOwner>
	class StateMachine : public SJH::Scene::Component
	{
	  public:
		using StateU = uint64_t;

		/// @brief 생성자 — owner 백포인터 + 초기 state 설정.
		/// @param owner   Component 가 어태치되는 Actor 또는 동등 객체. *non-owning* 백포인터로 보관.
		/// @param startup 초기 current_. default = NONE — 이 경우 TryTransit/ForceTransit 진입 *불가*
		///                (I1: NONE 에서는 전이 차단). 시작 시 ForceTransit 으로 전이를 의도하면
		///                반드시 NONE 외의 값을 전달.
		StateMachine(TOwner &owner, TState startup = TState::NONE)
		    : owner_(&owner), current_(startup)
		{
		}

		/// @brief State Entity 등록 — Aggregate 가 lifetime 소유.
		/// @note state 가 자기 ID (`GetStateFlag()`) 를 노출하므로 id 매개변수 불요.
		///       동일 ID 비트로 두 번 호출 시 silently overwrite. 각 TState enumerator 가 *유일한* 비트여야 한다.
		void RegisterState(std::unique_ptr<IFsmState<TOwner>> state)
		{
			assert(state && "RegisterState: null state");
			const StateU id = state->GetStateFlag();
			states_[id] = std::move(state);
		}

		/// @brief 런타임 가드 — target 으로 전이 시도. 실패 = false.
		/// @details `current.GetTransitFlag() & target` 비트 검사로 전이 가능 여부 자가 검증.
		bool TryTransit(TState target)
		{
			return TryTransitImpl((StateU)target);
		}

		/// @brief 계약 위반 abort — 실패 = std::abort(). 시나리오/스크립트용.
		void ForceTransit(TState target)
		{
			if (!TryTransitImpl((StateU)target))
			{
				std::abort();
			}
		}

		TState State() const
		{
			return current_;
		}

		// === Scene::Component 베이스 구현 (FSM 자체 hook — Unity OnStateMachineEnter 정통 분리) ===

		/// @brief FSM 진입 — current 가 등록 state 면 그 OnEnter 발화.
		void OnEnter() override
		{
			auto it = states_.find((StateU)current_);
			if (it != states_.end() && it->second)
			{
				it->second->OnEnter(*owner_);
			}
		}

		/// @brief FSM 이탈 — current state OnExit.
		void OnExit() override
		{
			auto it = states_.find((StateU)current_);
			if (it != states_.end() && it->second)
			{
				it->second->OnExit(*owner_);
			}
		}

		/// @brief 매 프레임 — 현재 state 의 OnUpdate 위임 (Godot _state.physics_process 정통).
		void Update(float dt) override
		{
			auto it = states_.find((StateU)current_);
			if (it != states_.end() && it->second)
			{
				it->second->OnUpdate(*owner_, dt);
			}
		}

	  private:
		bool TryTransitImpl(StateU targetBit)
		{
			const StateU curr = (StateU)current_;
			if (curr == 0)
				return false; // I1: NONE 에서는 전이 불가
			auto curIt = states_.find(curr);
			if (curIt == states_.end() || !curIt->second)
				return false; // current state 미등록 — 계약 위반
			// I2: 현재 state 가 target 으로 갈 수 있는가? (자가 검증)
			if ((curIt->second->GetTransitFlag() & targetBit) != targetBit)
				return false;
			auto targetIt = states_.find(targetBit);
			if (targetIt == states_.end() || !targetIt->second)
				return false; // target 미등록
			// I3: OnExit → swap → OnEnter
			curIt->second->OnExit(*owner_);
			current_ = (TState)targetBit;
			targetIt->second->OnEnter(*owner_);
			return true;
		}

		TOwner *owner_;
		TState current_ = TState::NONE;
		std::unordered_map<StateU, std::unique_ptr<IFsmState<TOwner>>> states_;
	};

} // namespace SJH::FSM

#endif // __SJH_FSM_STATE_MACHINE_H__
