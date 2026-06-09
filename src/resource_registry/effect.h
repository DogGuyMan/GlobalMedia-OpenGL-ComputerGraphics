/**
 * @file effect.h
 * @brief @c Effekseer::EffectRef 의 캐시 엔트리 래퍼 - @c ResourceRegistry 가 @c unique_ptr 로 보유.
 *
 * @details
 *  ### 책임
 *  - @c Effekseer::EffectRef (내부 @c shared_ptr 류) 를 보유하고 논리 이름으로 캐시 검색 인터페이스 제공.
 *  - @c ResourceRegistry::CreateEffect 가 @c Effekseer::Effect::Create 로 얻은 @c EffectRef 를 인수받아 생성.
 *  - 소멸 시 @c EffectRef ref count 감소 -> 0 이 되면 Effekseer 가 자동 정리 (명시 release 불필요).
 *
 *  ### 비-책임
 *  - [X] 재생 제어 (@c manager->Play / @c StopEffect) - @c EffekseerPlayable 책임.
 *  - [X] @c Effekseer::ManagerRef 보유 - VFXSystem 이 owner.
 *
 * @note M5(2026-05-26) 신설. @c EffektRef 는 @c std::shared_ptr 유사 타입 -
 *       @c operator! / @c operator bool 미지원, nullptr 비교는 @c .Get()==nullptr 사용.
 *       (.efk 파일 경로는 @c char16_t* / @c u"..." 리터럴 - UTF-16 Effekseer 표준.)
 */
#ifndef __SJH_RESOURCE_REGISTRY_EFFECT_H__
#define __SJH_RESOURCE_REGISTRY_EFFECT_H__

#include "common/common.h"
#include <Effekseer.h>   // EffectRef = Effekseer::RefPtr<Effekseer::Effect> 정의 필요

namespace SJH
{
    CLASS_PTR(Effect)
    /**
     * @brief @c Effekseer::EffectRef 소유 래퍼 - 파티클 이펙트 에셋 캐시 엔트리.
     * @details 복사/이동 모두 금지 - @c ResourceRegistry 가 @c unique_ptr 로 단일 소유.
     *          @c EffectRef 자체가 @c shared_ptr 류이므로 소멸 시 자동 정리, @c dtor = default.
     */
    class Effect
    {
      public:
        /// @brief @p ref 를 인수받아 래퍼 생성. @p ref.Get() != nullptr 보장 (호출자 책임).
        explicit Effect(::Effekseer::EffectRef ref) : mRef(ref) {}

        /// @brief @c EffectRef refcount 감소 - 0 이 되면 Effekseer 자동 해제.
        ~Effect() = default;

        /// @brief 보유 @c EffectRef 반환 - @c manager->Play(ref, pos) 인자로 직접 전달 가능.
        ::Effekseer::EffectRef Ref() const { return mRef; }

        Effect(const Effect&)            = delete; ///< Effekseer EffectRef 단일 캐시 엔트리 - 복사 금지.
        Effect& operator=(const Effect&) = delete;
        Effect(Effect&&)                 = delete; ///< 이동도 금지 - ResourceRegistry unique_ptr 보유.
        Effect& operator=(Effect&&)      = delete;

      private:
        ::Effekseer::EffectRef mRef; ///< Effekseer 파티클 이펙트 에셋 핸들 (shared_ptr 류 - refcount 자동 관리).
    };
} // namespace SJH

#endif // __SJH_RESOURCE_REGISTRY_EFFECT_H__
