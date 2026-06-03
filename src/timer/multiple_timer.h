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
            assert(mTimers.find(name) == mTimers.end()
                   && "MultipleTimer::Register — duplicate key");
            auto result = mTimers.emplace(name, std::move(timer));
            return &result.first->second;
        }
        /// @brief 편의 오버로드 — baseTime만으로 생성·등록.
        Timer* Register(const std::string& name, float baseTime)
        {
            return Register(name, Timer(baseTime));
        }

        void Unregister(const std::string& name)   // 없으면 no-op
        {
            mTimers.erase(name);
        }

        Timer* Find(const std::string& name)        // 없으면 nullptr
        {
            auto it = mTimers.find(name);
            return (it == mTimers.end()) ? nullptr : &it->second;
        }

        bool Has(const std::string& name) const
        {
            return mTimers.find(name) != mTimers.end();
        }

        void        Clear()       { mTimers.clear(); }
        std::size_t Count() const { return mTimers.size(); }

        // === Component hook ===
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override
        {
            if (!IsEnabled()) return;
            for (auto& entry : mTimers)
                entry.second.Tick(dt);
        }

      private:
        std::unordered_map<std::string, Timer> mTimers;
    };
}

#endif // __SJH_TIMER_MULTIPLE_TIMER_H__
