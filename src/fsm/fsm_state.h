/**
 * @file fsm_state.h
 * @brief FSM State Entity 베이스 인터페이스 - `IFsmState<TOwner>`.
 *
 * @details
 *  ### 책임
 *  - FSM 의 *각 State* 가 구현해야 할 순수 가상 인터페이스 정의.
 *  - **자가 식별**: `GetStateFlag()` (내가 누구) + `GetTransitFlag()` (내가 갈 수 있는 State 비트 OR) 를
 *    State 객체 자신이 노출한다 - DDD Entity within Aggregate 패턴.
 *  - **Hook 위임**: `StateMachine` 이 `OnEnter` / `OnUpdate` / `OnExit` 를 @p TOwner 참조와 함께 호출,
 *    State 는 내부 로직만 구현.
 *
 *  ### 비-책임
 *  - [X] 전이(transit) 실행 - `StateMachine::TryTransit` / `ForceTransit` 의 책임.
 *  - [X] 전이 가능 여부 검증 - `StateMachine::TryTransitImpl` 이 `GetTransitFlag` 를 읽어 판단.
 *  - [X] `TOwner` lifetime 관리 - 호출자(StateMachine) 가 비소유 포인터로 전달.
 *
 * @note Stage 4 (TTransit template parameter 폐기) - 전이 그래프 정보를 각 State 객체에 응집.
 *       정본 spec: `docs/superpowers/specs/2026-05-25-fsm-object-state-machine-design.md`
 */

#ifndef __SJH_FSM_FSM_STATE_H__
#define __SJH_FSM_FSM_STATE_H__

#include <cstdint>
namespace SJH::FSM
{
	/**
	 * @brief FSM State Entity 순수 가상 인터페이스.
	 *
	 * @details
	 *  `StateMachine<TState, TOwner>` 의 Aggregate 안에서 *Entity within Aggregate* 역할.
	 *  각 구체 State 는 이 인터페이스를 상속해 5개 메서드를 구현한다.
	 *
	 *  ### 비트 ID 규약
	 *  `TState enum class : uint64_t` 의 각 enumerator 는 *1 비트만 차지* 해야 한다
	 *  (`NONE = 0`, `Idle = 1 << 0`, `Move = 1 << 1`, ...).
	 *  `GetTransitFlag()` 는 전이 가능한 State 비트들의 OR 를 반환한다.
	 *
	 * @tparam TOwner Hook 인자로 받을 owner 타입. 보통 `PlayerActor` 등 Actor 파생 클래스.
	 */
	template <typename TOwner>
	class IFsmState
	{
	  public:
		virtual ~IFsmState() = default;

		/// @brief 이 State 의 고유 ID 비트를 반환한다. (`TState` enumerator 의 uint64_t 캐스트 값)
		/// @return 이 State 가 나타내는 단일 비트 (e.g. `(uint64_t)PlayerState::Idle`).
		virtual uint64_t GetStateFlag() const = 0;

		/// @brief 이 State 에서 전이 가능한 State 비트들의 OR 를 반환한다.
		/// @details `StateMachine::TryTransitImpl` 이 이 값과 target 비트의 AND 로 허용 여부를 판단.
		///          모든 State 로 전이 가능하면 `~0ULL`, 전이 불가면 `0` 을 반환.
		/// @return 허용 전이 비트 마스크 (0 이면 어디로도 전이 불가).
		virtual uint64_t GetTransitFlag() const = 0;

		/// @brief 이 State 에 진입할 때 1회 호출된다.
		/// @param owner FSM 을 보유한 Actor 참조 (non-owning).
		virtual void OnEnter(TOwner &owner) = 0;

		/// @brief 이 State 가 active 인 동안 매 프레임 호출된다.
		/// @param owner FSM 을 보유한 Actor 참조 (non-owning).
		/// @param dt    프레임 델타 시간 (초).
		virtual void OnUpdate(TOwner &owner, float dt) = 0;

		/// @brief 이 State 에서 이탈할 때 1회 호출된다.
		/// @param owner FSM 을 보유한 Actor 참조 (non-owning).
		virtual void OnExit(TOwner &owner) = 0;
	};

} // namespace SJH::FSM

#endif // __SJH_FSM_FSM_STATE_H__
