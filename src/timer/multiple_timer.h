#ifndef __SJH_TIMER_MULTIPLE_TIMER_H__
#define __SJH_TIMER_MULTIPLE_TIMER_H__

#include "timer/timer.h"
#include "scene/actor.h"   // SJH::Scene::Component

#include <cassert>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

namespace SJH::Timer
{
    /// @brief Actor에 붙는 Component. Timer 컨테이너 소유 + 중앙 Tick 디스패치.
    ///        다른 Component는 Register로 timer를 위탁하고, 반환된 Timer* 핸들로 조회/제어한다.
    /// @note  map<string, Timer> 노드는 포인터 안정적 — 반환된 Timer*는 해당 키 Unregister까지 유효.
    ///        Timer의 const baseTime_ 탓에 *대입*은 불가하나 map 삽입은 *구성*(emplace+move)만 쓰므로 무해.
    class MultipleTimer : public SJH::Scene::Component
    {
      public:
        // === 동적 관리 (Register / Unregister) ===
        /// @brief timer를 컨테이너로 move 이관하고 핸들 반환. 중복 키 -> assert (silent overwrite 금지).
        Timer* Register(const std::string& name, Timer timer)
        {
            assert(timers_.find(name) == timers_.end()
                   && "MultipleTimer::Register — duplicate key");
            auto result = timers_.emplace(name, std::move(timer));
            return &result.first->second;
        }
        /// @brief 편의 오버로드 — baseTime만으로 생성·등록.
        Timer* Register(const std::string& name, float baseTime)
        {
            return Register(name, Timer(baseTime));
        }

        void Unregister(const std::string& name)   // 없으면 no-op
        {
            timers_.erase(name);
        }

        Timer* Find(const std::string& name)        // 없으면 nullptr
        {
            auto it = timers_.find(name);
            return (it == timers_.end()) ? nullptr : &it->second;
        }

        bool Has(const std::string& name) const
        {
            return timers_.find(name) != timers_.end();
        }

        void        Clear()       { timers_.clear(); }
        std::size_t Count() const { return timers_.size(); }

        // === Component hook ===
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override
        {
            if (!IsEnabled()) return;
            for (auto& entry : timers_)
                entry.second.Tick(dt);
        }

      private:
        std::unordered_map<std::string, Timer> timers_;
    };
}

#endif // __SJH_TIMER_MULTIPLE_TIMER_H__
