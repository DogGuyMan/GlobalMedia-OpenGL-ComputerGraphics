#ifndef __SJH_TIMER_TIMER_H__
#define __SJH_TIMER_TIMER_H__

#include <cassert>

namespace SJH::Timer
{
    /// @brief 게임플레이 시간 누적기 (C# TimerComposite 포팅).
    ///        상태 없는 시간 카운터 — 전이 판단은 호출자(FSM 등) 책임.
    ///
    /// ### FSM 응용 레퍼런스 (코드 아님 — SJH::fsm 사용처가 구현)
    /// C# Affector(Ready→Start→Run⇄Pause→Terminate)는 SJH::fsm::StateMachine 으로 구현한다.
    ///   RunState::OnUpdate(owner, dt):
    ///       timer.Tick(dt);
    ///       if (timer.IsBlocked())         machine.TryTransit(Pause);
    ///       else if (timer.IsTimesUp())    machine.TryTransit(Terminate);
    ///       else if (timer.PollInterval()) owner.OnIntervalFire();  // 연속발사/DoT 틱
    class Timer
    {
      public:
        explicit Timer(float baseTime)
            : baseTime_(baseTime)
        {
            assert(baseTime > 0.0f && "Timer: baseTime must be > 0");
        }

        // === Fluent Builder (생성 직후 체이닝) ===
        Timer& SetAcceleration(float amount)
        {
            acceleration_ = (amount < 0.0f) ? 0.0f : amount; // 음수 → 0 (C# 동일)
            return *this;
        }
        Timer& SetInterval(float interval)
        {
            intervalTime_ = interval;   // <=0 이면 PollInterval 항상 false (비활성)
            nextInterval_ = interval;
            return *this;
        }

        // === 매 프레임 ===
        void Tick(float dt)
        {
            if (blocked_) return;
            passedTime_ += dt * acceleration_;
            if (passedTime_ < 0.0f)            passedTime_ = 0.0f;       // 음수 dt 가드
            else if (passedTime_ > baseTime_)  passedTime_ = baseTime_;  // [0,Base] clamp
        }

        // === 조회 ===
        float GetProgress()   const { return passedTime_ / baseTime_; } // baseTime_>0 보장
        bool  IsTimesUp()     const { return passedTime_ >= baseTime_; }
        float GetPassedTime() const { return passedTime_; }
        float GetBaseTime()   const { return baseTime_; }

        /// @brief non-const — interval 경과 시 true 1회 + nextInterval 누적.
        ///        매 프레임 1회 폴링 가정 (한 Tick에 여러 interval 건너뛰어도 1회만 보고 — C# 동일).
        /// @note  누적 발사는 *합쳐지지 않고 프레임마다 이연*된다. 큰 프레임 히치나 acceleration>1 로
        ///        한 Tick 에 경계를 N개 넘어도 그 N회는 이후 N 프레임에 걸쳐 1회씩 보고된다
        ///        (catch-up 드레인 아님). "true 1회 = interval 1구간 경과" 로 가정하지 말 것.
        bool PollInterval()
        {
            if (intervalTime_ <= 0.0f) return false;
            if (passedTime_ >= nextInterval_)
            {
                nextInterval_ += intervalTime_;
                return true;
            }
            return false;
        }

        // === Pause (C# Puase/Continue) ===
        void Pause()  { blocked_ = true; }
        void Resume() { blocked_ = false; }
        bool IsBlocked() const { return blocked_; }

        // === Reset (C# ResetTimer — accel/interval 설정값은 유지) ===
        void Reset()
        {
            passedTime_   = 0.0f;
            blocked_      = false;
            nextInterval_ = intervalTime_;
        }

      private:
        const float baseTime_;            // readonly (C# BaseTime)
        float       passedTime_   = 0.0f;
        float       acceleration_ = 1.0f;
        bool        blocked_      = false;
        float       intervalTime_ = 0.0f; // <=0 이면 interval 비활성
        float       nextInterval_ = 0.0f;
    };
}

#endif // __SJH_TIMER_TIMER_H__
