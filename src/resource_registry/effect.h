#ifndef __SJH_RESOURCE_REGISTRY_EFFECT_H__
#define __SJH_RESOURCE_REGISTRY_EFFECT_H__

#include "common/common.h"
#include <Effekseer.h>   // EffectRef = std::shared_ptr<Effect> 정의 필요

namespace SJH
{
    CLASS_PTR(Effect)
    /// @brief Effekseer::EffectRef wrap — ResourceRegistry::CreateEffect 가 캐시 entry 로 보유.
    /// @details
    ///   - EffectRef 는 shared_ptr 류 — dtor 자동 정리, 명시 release 불요.
    ///   - 외부 (EffekseerPlayable) 는 Ref() 로 EffectRef 조회. manager_->Play(ref, pos) 인자로 직접 전달 가능.
    class Effect
    {
      public:
        explicit Effect(::Effekseer::EffectRef ref) : mRef(ref) {}
        ~Effect() = default;

        ::Effekseer::EffectRef Ref() const { return mRef; }

        Effect(const Effect&)            = delete;
        Effect& operator=(const Effect&) = delete;
        Effect(Effect&&)                 = delete;
        Effect& operator=(Effect&&)      = delete;

      private:
        ::Effekseer::EffectRef mRef;
    };
}

#endif // __SJH_RESOURCE_REGISTRY_EFFECT_H__
