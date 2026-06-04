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
	/// - I3: 전이 시 항상 OnExit(current)  current = target  OnEnter(current) 순서.
	///       단 적용은 *재진입이 아니라* `Update` 경계의 `ApplyPending()` 에서 (deferred 전이).
	///       TryTransit 은 검증만 즉시 수행하고 OnExit/OnEnter 는 다음 Update 시작에 큐잉 적용 →
	///       어떤 State 의 OnUpdate 도중 전이가 요청돼도 그 OnUpdate 잔여 코드가 새 State 위에서 도는 일이 없다.
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
		    : mOwner(&owner), curState(startup)
		{
		}

		/// @brief State Entity 등록 — Aggregate 가 lifetime 소유.
		/// @note state 가 자기 ID (`GetStateFlag()`) 를 노출하므로 id 매개변수 불요.
		///       동일 ID 비트로 두 번 호출 시 silently overwrite. 각 TState enumerator 가 *유일한* 비트여야 한다.
		void RegisterState(std::unique_ptr<IFsmState<TOwner>> state)
		{
			assert(state && "RegisterState: null state");
			const StateU id = state->GetStateFlag();
			mStates[id] = std::move(state);
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
			return curState;
		}

		// === Scene::Component 베이스 구현 (FSM 자체 hook — Unity OnStateMachineEnter 정통 분리) ===

		/// @brief FSM 진입 — current 가 등록 state 면 그 OnEnter 발화.
		void OnEnter() override
		{
			auto it = mStates.find((StateU)curState);
			if (it != mStates.end() && it->second)
				it->second->OnEnter(*mOwner);
		}

		/// @brief FSM 이탈 — current state OnExit.
		void OnExit() override
		{
			auto it = mStates.find((StateU)curState);
			if (it != mStates.end() && it->second)
				it->second->OnExit(*mOwner);
		}

		/// @brief 매 프레임 — (1) 지난 tick 에 큐잉된 전이 적용 → (2) 현재 state OnUpdate 디스패치.
		/// @details OnUpdate 도중 TryTransit 가 호출되면 pending 에만 기록되어, 이번 OnUpdate 는 현재 state 로
		///          끝까지 실행되고 전이는 다음 Update 의 (1)에서 일어난다 (재진입 제거 — Aggregate invariant 보존).
		void Update(float dt) override
		{
			ApplyPending();
			auto it = mStates.find((StateU)curState);
			if (it != mStates.end() && it->second)
				it->second->OnUpdate(*mOwner, dt);
		}

	  protected:
		bool TryTransitImpl(StateU targetBit)
		{
			const StateU curr = (StateU)curState;
			if (curr == 0)
				return false; // I1: NONE 에서는 전이 불가
			auto curIt = mStates.find(curr);
			// current state 미등록 — 계약 위반
			if (curIt == mStates.end() || !curIt->second)
				return false;
			// 현재 state 가 target 으로 갈 수 있는가? (검증은 *즉시* — ForceTransit 의 즉시 abort 의미 보존)
			if ((curIt->second->GetTransitFlag() & targetBit) != targetBit)
				return false;
			auto targetIt = mStates.find(targetBit);
			// target 미등록
			if (targetIt == mStates.end() || !targetIt->second)
				return false;
			// ★ 즉시 OnExit/OnEnter 하지 않는다 — pending 으로 큐잉. 같은 tick 내 복수 호출 시 마지막이 이김
			//   (모두 동일한 현재 state 기준으로 검증되므로 안전). 실제 전이는 ApplyPending()(Update 경계).
			mPendingState = (TState)targetBit;
			mHasPending   = true;
			return true;
		}

		/// @brief 큐잉된 전이를 안전한 경계(Update 시작)에서 적용 — OnExit(cur) → curState=target → OnEnter(target).
		/// @details I3 보존: 전이는 항상 이 순서. 단 *재진입이 아닌* 통제된 지점에서만 일어난다.
		///          OnEnter 안에서 또 TryTransit 하면 그 전이는 다음 Update 의 ApplyPending 에서 처리(tick 당 1단계).
		void ApplyPending()
		{
			if (!mHasPending)
				return;
			mHasPending = false;
			const StateU from = (StateU)curState;
			const StateU to   = (StateU)mPendingState;
			auto fromIt = mStates.find(from);
			if (fromIt != mStates.end() && fromIt->second)
				fromIt->second->OnExit(*mOwner);
			curState = mPendingState;
			auto toIt = mStates.find(to);
			if (toIt != mStates.end() && toIt->second)
				toIt->second->OnEnter(*mOwner);
		}

		TOwner *mOwner;
		TState curState = TState::NONE;
		std::unordered_map<StateU, std::unique_ptr<IFsmState<TOwner>>> mStates;

		// 지연 전이 — TryTransit 은 검증만 즉시, 실제 OnExit/OnEnter 적용은 ApplyPending()(Update 경계).
		//   재진입 방지: OnUpdate 도중 전이가 요청돼도 이번 tick 의 OnUpdate 는 현재 State 로 끝까지 실행되고,
		//   전이는 다음 Update 의 ApplyPending 에서 원자적으로 일어난다 (Aggregate invariant 보존).
		bool   mHasPending   = false;
		TState mPendingState = TState::NONE;
	};

} // namespace SJH::FSM

#endif // __SJH_FSM_STATE_MACHINE_H__
