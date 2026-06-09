/**
 * @file multiple_timer.h
 * @brief 다중 트랙 타이머 컴포넌트 - `SJH::Timer::MultipleTimer`.
 *
 * @details
 *  ### 책임
 *  - 이름 키(`string`)로 복수의 `Timer` 를 소유/관리.
 *  - `Register` / `Unregister` 로 동적 추가/제거 - 반환된 `Timer*` 핸들로 조회/제어.
 *  - `Scene::Component` 상속 -> `Update(dt)` 에서 모든 등록 타이머를 일괄 `Tick`.
 *
 *  ### 비-책임
 *  - [X] 전이 판단 - 호출자(`BaseEntity` / FSM State) 가 `Timer*` 핸들로 `IsTimesUp` 등 조회.
 *  - [X] `Timer` lifetime 외부 보유 - 호출자는 `Timer*` 핸들만 가지며, 해당 키가 `Unregister` 되면
 *    포인터 무효화. Actor `OnExit` 에서 `Unregister` 또는 `Clear` 권장.
 *
 *  ### 등록 컨벤션 (BaseEntity 중앙화 패턴)
 *  `BaseEntity::OnEnter` 에서 논리적 이름으로 `Register`, `OnExit` 에서 `Unregister`.
 *  엔티티 컴포넌트들은 중앙에서 등록된 핸들(`player.attack` / `life.iframe` 등)을
 *  raw-float 분산 산술 대신 `Timer*` 포인터로 참조한다.
 *  ```
 *  OnEnter:  mAttackTimer = timers.Register("player.attack", Timer(0.5f));
 *  OnExit:   timers.Unregister("player.attack");
 *  Update:   if (mAttackTimer->IsTimesUp()) { ... }
 *  ```
 *
 * @note `unordered_map<string, Timer>` 노드는 포인터 안정적 - `Register` 가 반환한 `Timer*` 는
 *       해당 키 `Unregister` / `Clear` 전까지 유효.
 *       `Timer` 의 `const BASE_TIME` 탓에 대입은 불가하나, `emplace` + move 삽입은 무해.
 */

#ifndef __SJH_TIMER_MULTIPLE_TIMER_H__
#define __SJH_TIMER_MULTIPLE_TIMER_H__

#include "timer/timer.h"
#include "scene/actor.h" // SJH::Scene::Component

#include <cassert>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

namespace SJH::Timer
{
	/**
	 * @brief 다중 트랙 타이머 컴포넌트 - 이름 키로 `Timer` 를 소유/일괄 틱.
	 *
	 * @details
	 *  `Scene::Component` 를 상속하므로 Actor 에 `AddComponent<MultipleTimer>()` 로 어태치하면
	 *  `Update(dt)` 가 자동으로 모든 등록 타이머를 `Tick` 한다.
	 *
	 *  사용자는 `Register` 가 반환한 `Timer*` 핸들만 보관하고,
	 *  해당 `Timer` 의 조회/제어(`IsTimesUp`, `Pause`, `Reset` 등)는 핸들을 통해 수행한다.
	 */
	class MultipleTimer : public SJH::Scene::Component
	{
	  public:
		// === 동적 관리 (Register / Unregister) ===

		/**
		 * @brief `Timer` 를 컨테이너로 move 이관하고 핸들을 반환한다.
		 * @details 중복 키 등록은 허용하지 않는다 (assert 로 즉시 감지).
		 *          반환된 `Timer*` 는 이 키가 `Unregister` / `Clear` 되기 전까지 유효.
		 * @param name  타이머 식별 이름 (e.g. `"player.attack"`, `"life.iframe"`).
		 * @param timer 등록할 `Timer` 값 (move 이관). `SetAcceleration` / `SetInterval` 체이닝 후 전달 가능.
		 * @return 컨테이너 내부 `Timer` 의 비소유 포인터.
		 */
		Timer *Register(const std::string &name, Timer timer)
		{
			assert(mTimers.find(name) == mTimers.end()
			       && "MultipleTimer::Register - duplicate key");
			auto result = mTimers.emplace(name, std::move(timer));
			return &result.first->second;
		}

		/**
		 * @brief 편의 오버로드 - `baseTime` 만으로 `Timer` 를 생성/등록한다.
		 * @param name     타이머 식별 이름.
		 * @param baseTime 기준 시간(초). `Timer(baseTime)` 을 내부 생성해 위임.
		 * @return 컨테이너 내부 `Timer` 의 비소유 포인터.
		 */
		Timer *Register(const std::string &name, float baseTime)
		{
			return Register(name, Timer(baseTime));
		}

		/// @brief @p name 키의 타이머를 제거한다. 존재하지 않으면 no-op.
		/// @details 제거 후 기존 `Timer*` 핸들은 무효화됨 - 이후 역참조 금지.
		/// @param name 제거할 타이머 이름.
		void Unregister(const std::string &name) // 없으면 no-op
		{
			mTimers.erase(name);
		}

		/// @brief @p name 키의 `Timer*` 를 반환한다. 없으면 @c nullptr.
		/// @param name 조회할 타이머 이름.
		/// @return 내부 `Timer` 비소유 포인터. 없으면 @c nullptr.
		Timer *Find(const std::string &name)
		{
			auto it = mTimers.find(name);
			return (it == mTimers.end()) ? nullptr : &it->second;
		}

		/// @brief @p name 키의 타이머가 등록돼 있으면 @c true.
		/// @param name 확인할 타이머 이름.
		bool Has(const std::string &name) const
		{
			return mTimers.find(name) != mTimers.end();
		}

		/// @brief 등록된 모든 타이머를 제거한다. 이후 기존 `Timer*` 핸들 전부 무효화.
		void Clear() { mTimers.clear(); }

		/// @brief 현재 등록된 타이머 수를 반환한다.
		std::size_t Count() const { return mTimers.size(); }

		// === Component hook ===

		/// @brief Component 진입 - 현재 구현 없음 (호출자가 Register 로 진입 시 타이머 등록).
		void OnEnter() override {}

		/// @brief Component 이탈 - 현재 구현 없음 (호출자가 Unregister 또는 Clear 로 정리 권장).
		void OnExit() override {}

		/// @brief 매 프레임 - 활성화(`IsEnabled()`) 상태이면 모든 등록 타이머를 `Tick(dt)`.
		/// @param dt 프레임 델타 시간(초).
		void Update(float dt) override
		{
			if (!IsEnabled()) return;
			for (auto &entry : mTimers)
				entry.second.Tick(dt);
		}

	  private:
		/// @brief 이름 키 -> `Timer` 소유 맵. 노드 포인터 안정적 (`unordered_map` 보장).
		std::unordered_map<std::string, Timer> mTimers;
	};
} // namespace SJH::Timer

#endif // __SJH_TIMER_MULTIPLE_TIMER_H__
