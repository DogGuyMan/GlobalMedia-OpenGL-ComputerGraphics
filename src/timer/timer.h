/**
 * @file timer.h
 * @brief 게임플레이 단발/경과 타이머 - `SJH::Timer::Timer`.
 *
 * @details
 *  ### 책임
 *  - 경과 시간(`mPassedTime`) 을 `[0, BASE_TIME]` 범위로 누적 (`Tick`).
 *  - 완료 여부(`IsTimesUp`), 진행률(`GetProgress`), 인터벌 발화(`PollInterval`) 조회.
 *  - Pause / Resume 으로 틱 중단/재개.
 *  - `Reset()` 으로 경과 시간 초기화 (가속/인터벌 설정값은 유지).
 *
 *  ### 비-책임
 *  - [X] 상태 전이 - 전이 판단은 호출자(FSM 등) 책임.
 *    `IsTimesUp()` / `IsBlocked()` 결과를 읽어 `StateMachine::TryTransit` 을 호출한다.
 *  - [X] 복수 타이머 관리 - `MultipleTimer` 가 담당.
 *  - [X] `Actor` / `Component` 라이프사이클 - `Timer` 는 값 클래스(Value Object).
 *    `MultipleTimer::Register` 로 위탁해 컴포넌트 체계에 통합할 것.
 *
 *  ### FSM 응용 패턴
 *  ```
 *  RunState::OnUpdate(owner, dt):
 *      timer.Tick(dt);
 *      if (timer.IsBlocked())        machine.TryTransit(Pause);
 *      else if (timer.IsTimesUp())   machine.TryTransit(Terminate);
 *      else if (timer.PollInterval()) owner.OnIntervalFire();  // 연속발사/DoT 틱
 *  ```
 *  `SJH::FSM::StateMachine` 사용처가 구현. C# Affector(Ready->Start->Run<->Pause->Terminate) 포팅.
 *
 * @note `const float BASE_TIME` 탓에 `Timer` 는 생성 후 대입 불가.
 *       `MultipleTimer::Register` 는 `emplace` + move 로 삽입하므로 대입 없이 컨테이너 구성 가능.
 */

#ifndef __SJH_TIMER_TIMER_H__
#define __SJH_TIMER_TIMER_H__

#include <cassert>

namespace SJH::Timer
{
	/**
	 * @brief 게임플레이 시간 누적기 - 단발/경과 타이머 값 객체.
	 *
	 * @details
	 *  경과 시간을 `[0, BASE_TIME]` 범위에서 매 프레임 `Tick(dt)` 으로 누적한다.
	 *  가속(`SetAcceleration`)과 인터벌 발화(`SetInterval` + `PollInterval`)를 지원한다.
	 *
	 *  생성 후 Fluent Builder 체이닝 예:
	 *  ```cpp
	 *  auto* t = timers.Register("attack", Timer(0.5f)
	 *                  .SetAcceleration(1.0f)
	 *                  .SetInterval(0.15f));
	 *  ```
	 */
	class Timer
	{
	  public:
		/**
		 * @brief 생성자 - 기준 시간(BASE_TIME) 설정.
		 * @param baseTime 타이머 완료까지의 기준 시간(초). 반드시 > 0.
		 */
		explicit Timer(float baseTime)
		    : BASE_TIME(baseTime)
		{
			assert(baseTime > 0.0f && "Timer: baseTime must be > 0");
		}

		// === Fluent Builder (생성 직후 체이닝) ===

		/**
		 * @brief 시간 누적 가속 배율 설정. 기본값 @c 1.0f (실시간).
		 * @details `Tick(dt)` 는 `dt * mAcceleration` 을 누적. 음수는 @c 0 으로 클램프.
		 *          @c 2.0f 이면 실시간의 2배 속도, @c 0.0f 이면 사실상 Pause 와 동일.
		 * @param amount 가속 배율 (음수 -> @c 0 으로 보정).
		 * @return `*this` (Fluent Builder 체이닝용).
		 */
		Timer &SetAcceleration(float amount)
		{
			mAcceleration = (amount < 0.0f) ? 0.0f : amount; // 음수 -> 0 (C# 동일)
			return *this;
		}

		/**
		 * @brief 반복 인터벌 시간 설정. `PollInterval()` 이 이 간격마다 @c true 를 반환.
		 * @details @p interval <= 0 이면 인터벌 비활성 (`PollInterval` 은 항상 @c false).
		 * @param interval 인터벌 간격(초). @c 0 이하이면 비활성.
		 * @return `*this` (Fluent Builder 체이닝용).
		 */
		Timer &SetInterval(float interval)
		{
			mIntervalTime = interval; // <=0 이면 PollInterval 항상 false (비활성)
			mIextInterval = interval;
			return *this;
		}

		// === 매 프레임 ===

		/**
		 * @brief 경과 시간을 `dt * acceleration` 만큼 누적한다.
		 * @details `IsBlocked()` 이면 즉시 반환. 누적값은 `[0, BASE_TIME]` 로 클램프.
		 * @param dt 프레임 델타 시간(초). 음수 dt 는 0 으로 보정.
		 */
		void Tick(float dt)
		{
			if (mBlocked) return;
			mPassedTime += dt * mAcceleration;
			if (mPassedTime < 0.0f)           mPassedTime = 0.0f;      // 음수 dt 가드
			else if (mPassedTime > BASE_TIME) mPassedTime = BASE_TIME; // [0,Base] clamp
		}

		// === 조회 ===

		/// @brief 진행률 `[0.0, 1.0]` 반환. `BASE_TIME > 0` 보장이므로 나누기 안전.
		float GetProgress() const { return mPassedTime / BASE_TIME; }

		/// @brief 경과 시간이 `BASE_TIME` 에 도달했으면 @c true.
		bool IsTimesUp() const { return mPassedTime >= BASE_TIME; }

		/// @brief 현재 경과 시간(초) 반환.
		float GetPassedTime() const { return mPassedTime; }

		/// @brief 기준 시간(초) 반환.
		float GetBaseTime() const { return BASE_TIME; }

		/**
		 * @brief 인터벌 경과 시 @c true 를 1회 반환하고 다음 인터벌 기준점을 전진.
		 * @details non-const - 내부 `mIextInterval` 을 갱신.
		 *          매 프레임 1회 폴링을 가정한다. 한 `Tick` 에서 여러 인터벌을 건너뛰어도
		 *          이번 프레임은 @c true 1회만 반환하고, 나머지는 이후 프레임에 1회씩 보고
		 *          (catch-up 드레인이 아님 - C# 동일 의미).
		 * @return 인터벌 시간이 경과했으면 @c true, 아니면 @c false. 인터벌 미설정 시 항상 @c false.
		 */
		bool PollInterval()
		{
			if (mIntervalTime <= 0.0f) return false;
			if (mPassedTime >= mIextInterval)
			{
				mIextInterval += mIntervalTime;
				return true;
			}
			return false;
		}

		// === Pause (C# Pause / Continue) ===

		/// @brief 타이머 틱을 중단한다. `Tick` 호출이 no-op 이 된다.
		void Pause() { mBlocked = true; }

		/// @brief 타이머 틱을 재개한다.
		void Resume() { mBlocked = false; }

		/// @brief 현재 Pause 상태이면 @c true.
		bool IsBlocked() const { return mBlocked; }

		// === Reset (C# ResetTimer - accel / interval 설정값 유지) ===

		/**
		 * @brief 경과 시간을 0 으로 초기화하고 Pause 를 해제한다.
		 * @details `SetAcceleration` / `SetInterval` 설정값은 유지된다.
		 *          인터벌 기준점(`mIextInterval`) 도 초기 인터벌 값으로 리셋.
		 */
		void Reset()
		{
			mPassedTime   = 0.0f;
			mBlocked      = false;
			mIextInterval = mIntervalTime;
		}

	  private:
		/// @brief 타이머 완료 기준 시간(초). 생성 후 변경 불가 (const).
		const float BASE_TIME;

		/// @brief 누적된 경과 시간(초). `[0, BASE_TIME]` 범위로 클램프.
		float mPassedTime   = 0.0f;

		/// @brief 시간 누적 가속 배율. 기본 @c 1.0f.
		float mAcceleration = 1.0f;

		/// @brief Pause 상태 플래그. @c true 이면 `Tick` no-op.
		bool mBlocked      = false;

		/// @brief 인터벌 간격(초). @c 0 이하이면 인터벌 비활성.
		float mIntervalTime = 0.0f;

		/// @brief 다음 인터벌 발화 기준점(초). `PollInterval` 성공마다 `mIntervalTime` 씩 전진.
		float mIextInterval = 0.0f;
	};
} // namespace SJH::Timer

#endif // __SJH_TIMER_TIMER_H__
