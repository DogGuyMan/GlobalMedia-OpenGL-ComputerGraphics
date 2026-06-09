/**
 * @file state_machine.h
 * @brief FSM Aggregate Root - `StateMachine<TState, TOwner>`.
 *
 * @details
 *  ### 책임
 *  - 등록된 `IFsmState<TOwner>` 객체들의 lifetime 소유 (Aggregate Root).
 *  - current State 트래킹 + `TryTransit` / `ForceTransit` 전이 실행.
 *  - `Scene::Component` 상속 -> Actor 에 어태치 가능; `Update(dt)` 로 FSM 틱.
 *  - 지연(deferred) 전이: `TryTransit` 는 검증만 즉시 수행하고 실제 `OnExit`/`OnEnter` 는
 *    다음 `Update` 시작의 `ApplyPending()` 에서 일어난다 -> 재진입(re-entrancy) 제거.
 *
 *  ### 비-책임
 *  - [X] 전이 그래프 정보 보유 - 각 `IFsmState` 가 `GetTransitFlag()` 로 자가 노출.
 *  - [X] `TOwner` lifetime 관리 - 생성자에서 비소유 포인터(`mOwner`) 로만 보관.
 *
 *  ### 불변식 (Aggregate invariant)
 *  - **I1**: `curState in Registered(mStates) U {NONE}`
 *  - **I2**: `TryTransit` 성공 <=> `current.GetTransitFlag() & target_bit == target_bit` AND target 등록됨
 *  - **I3**: 전이 시 항상 `OnExit(cur)` -> `curState = target` -> `OnEnter(target)` 순서.
 *            적용 시점은 `Update` 경계의 `ApplyPending()` - `OnUpdate` 도중 재진입 없음.
 *
 * @note Stage 4 (TTransit template parameter 폐기) - 전이 그래프를 State 에 응집.
 *       정본 spec: `docs/superpowers/specs/2026-05-25-fsm-object-state-machine-design.md`
 */

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
	/**
	 * @brief FSM Aggregate Root - State 그래프 보유 + 전이 실행 + Component hook 위임.
	 *
	 * @details
	 *  `Scene::Component` 를 상속하므로 Actor 의 `AddComponent<StateMachine<...>>()` 로 추가하고,
	 *  `Update(dt)` 가 매 프레임 자동으로 호출된다.
	 *
	 *  ### 그래프 정보 응집 (DDD)
	 *  전이 가능 여부는 *각 State 객체 자체* 가 보유한다:
	 *  - `IFsmState::GetStateFlag()` = 내가 누구
	 *  - `IFsmState::GetTransitFlag()` = 내가 갈 수 있는 곳들의 비트 OR
	 *
	 *  StateMachine 은 current 트래킹 + Hook 디스패치만 담당.
	 *
	 *  ### TryTransit vs ForceTransit
	 *  - `TryTransit`  : 런타임 가드. 실패 = @c false 반환. 입력 핸들러 / 일반 게임 로직용.
	 *  - `ForceTransit`: 계약 위반 시 `std::abort()`. 시나리오/스크립트 절대 전이용.
	 *
	 *  ### owner_ 라이프타임 약속
	 *  `mOwner` 는 *non-owning* 백포인터. Actor/Component 패턴(Actor 가 `unique_ptr<Component>` 소유)
	 *  하에서 Component 는 Actor 보다 먼저 파괴되므로 자연스럽게 충족된다.
	 *
	 * @tparam TState  `enum class : uint64_t` - 각 enumerator 가 1비트 (`NONE = 0` 약속).
	 * @tparam TOwner  Actor 파생 클래스 - State hook 의 `owner` 인자 타입.
	 */
	template <typename TState, typename TOwner>
	class StateMachine : public SJH::Scene::Component
	{
	  public:
		/// @brief `StateU` - State ID / Transit 비트 마스크에 쓰이는 raw 정수 타입.
		using StateU = uint64_t;

		/**
		 * @brief 생성자 - owner 백포인터 + 초기 State 설정.
		 * @param owner   Component 가 어태치될 Actor 또는 동등 객체. *non-owning* 포인터로 보관.
		 * @param startup 초기 `curState`. 기본값 `NONE` - `NONE` 상태에서 `TryTransit`/`ForceTransit` 불가.
		 *                진입 직후 전이가 필요하면 `NONE` 이 아닌 시작 State 를 전달하거나,
		 *                `OnEnter()` 이후 `ForceTransit(initialState)` 를 호출한다.
		 */
		StateMachine(TOwner &owner, TState startup = TState::NONE)
		    : mOwner(&owner), curState(startup)
		{
		}

		/**
		 * @brief State Entity 등록 - Aggregate 가 `unique_ptr` 로 lifetime 소유.
		 * @details State 는 `GetStateFlag()` 로 자기 ID 를 노출하므로 별도 id 인자가 없다.
		 *          동일 ID 비트로 두 번 등록하면 silently overwrite (중복 방지는 호출 측 빌더 책임).
		 * @param state 등록할 State (nullptr 허용 안 됨 - assert 로 검증).
		 */
		void RegisterState(std::unique_ptr<IFsmState<TOwner>> state)
		{
			assert(state && "RegisterState: null state");
			const StateU id = state->GetStateFlag();
			mStates[id] = std::move(state);
		}

		/**
		 * @brief 런타임 가드 - @p target 으로 전이를 시도한다.
		 * @details `current.GetTransitFlag() & (uint64_t)target` 비트 검사로 허용 여부를 즉시 검증.
		 *          성공 시 @p target 을 pending 에 큐잉 (실제 `OnExit`/`OnEnter` 는 다음 `Update` 경계).
		 * @param target 전이 목적지 State.
		 * @return 전이가 허용되어 pending 에 등록됐으면 @c true, 아니면 @c false.
		 */
		bool TryTransit(TState target)
		{
			return TryTransitImpl((StateU)target);
		}

		/**
		 * @brief 계약 위반 abort - 전이 실패 시 `std::abort()`. 시나리오/스크립트 절대 전이용.
		 * @details 게임 로직에서 "반드시 전이돼야 한다" 는 불변식을 강제할 때 사용.
		 *          실패 가능성이 있는 일반 전이는 `TryTransit` 을 사용한다.
		 * @param target 전이 목적지 State.
		 */
		void ForceTransit(TState target)
		{
			if (!TryTransitImpl((StateU)target))
			{
				std::abort();
			}
		}

		/// @brief 현재 active State 를 반환한다.
		/// @return 현재 `TState` enumerator 값 (`NONE` 이면 아직 진입 State 없음).
		TState State() const
		{
			return curState;
		}

		// === Scene::Component 베이스 구현 ===

		/// @brief FSM Component 진입 - current State 가 등록돼 있으면 `IFsmState::OnEnter` 발화.
		/// @details Unity `OnStateMachineEnter` 와 유사. Actor 의 `OnEnter` 체인에서 자동 호출.
		void OnEnter() override
		{
			auto it = mStates.find((StateU)curState);
			if (it != mStates.end() && it->second)
				it->second->OnEnter(*mOwner);
		}

		/// @brief FSM Component 이탈 - current State `IFsmState::OnExit` 발화.
		/// @details Actor 의 `OnExit` 체인에서 자동 호출.
		void OnExit() override
		{
			auto it = mStates.find((StateU)curState);
			if (it != mStates.end() && it->second)
				it->second->OnExit(*mOwner);
		}

		/**
		 * @brief 매 프레임 - (1) 지난 tick 에 큐잉된 전이 적용 -> (2) current State `OnUpdate` 디스패치.
		 * @details `OnUpdate` 도중 `TryTransit` 가 호출되면 pending 에만 기록된다.
		 *          이번 `OnUpdate` 는 현재 State 로 끝까지 실행되고, 전이는 다음 `Update` 의 (1) 에서 일어난다
		 *          (재진입 제거 - Aggregate invariant I3 보존).
		 * @param dt 프레임 델타 시간 (초).
		 */
		void Update(float dt) override
		{
			ApplyPending();
			auto it = mStates.find((StateU)curState);
			if (it != mStates.end() && it->second)
				it->second->OnUpdate(*mOwner, dt);
		}

	  protected:
		/// @brief 전이 허용 여부 즉시 검증 + 성공 시 pending 큐잉. 실제 OnExit/OnEnter 는 ApplyPending 에서.
		bool TryTransitImpl(StateU targetBit)
		{
			const StateU curr = (StateU)curState;
			if (curr == 0)
				return false; // I1: NONE 에서는 전이 불가
			auto curIt = mStates.find(curr);
			// current state 미등록 - 계약 위반
			if (curIt == mStates.end() || !curIt->second)
				return false;
			// 현재 state 가 target 으로 갈 수 있는가? (검증은 *즉시* - ForceTransit 의 즉시 abort 의미 보존)
			if ((curIt->second->GetTransitFlag() & targetBit) != targetBit)
				return false;
			auto targetIt = mStates.find(targetBit);
			// target 미등록
			if (targetIt == mStates.end() || !targetIt->second)
				return false;
			// * 즉시 OnExit/OnEnter 하지 않는다 - pending 으로 큐잉. 같은 tick 내 복수 호출 시 마지막이 이김
			//   (모두 동일한 현재 state 기준으로 검증되므로 안전). 실제 전이는 ApplyPending()(Update 경계).
			mPendingState = (TState)targetBit;
			mHasPending   = true;
			return true;
		}

		/**
		 * @brief 큐잉된 전이를 안전한 경계(`Update` 시작)에서 원자적으로 적용.
		 * @details 순서: `OnExit(cur)` -> `curState = target` -> `OnEnter(target)`.
		 *          `OnEnter` 안에서 또 `TryTransit` 하면 그 전이는 다음 `Update` 의 `ApplyPending` 에서 처리
		 *          (tick 당 1 단계 전이만 일어남 - I3 보존).
		 */
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

		/// @brief FSM 을 보유한 Actor 의 비소유 포인터 (lifetime 은 Actor 가 보장).
		TOwner *mOwner;

		/// @brief 현재 active State 의 enumerator 값.
		TState curState = TState::NONE;

		/// @brief 등록된 State Entity 맵. key = `GetStateFlag()` 반환값.
		std::unordered_map<StateU, std::unique_ptr<IFsmState<TOwner>>> mStates;

		/// @brief 지연 전이 플래그 - `TryTransit` 성공 시 @c true, `ApplyPending` 처리 후 @c false.
		bool   mHasPending   = false;

		/// @brief 다음 `ApplyPending` 에서 적용될 전이 목적지 State.
		TState mPendingState = TState::NONE;
	};

} // namespace SJH::FSM

#endif // __SJH_FSM_STATE_MACHINE_H__
