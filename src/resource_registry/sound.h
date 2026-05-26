#ifndef __SJH_RESOURCE_REGISTRY_SOUND_H__
#define __SJH_RESOURCE_REGISTRY_SOUND_H__

#include "common/common.h"

// fwd — FMOD 헤더는 .cpp 안에서만
namespace FMOD { class Sound; }

namespace SJH
{
    CLASS_PTR(Sound)
    /// @brief FMOD::Sound* RAII wrap — ResourceRegistry::CreateSound 가 캐시 entry 로 보유.
    /// @details
    ///   - 외부 (FmodPlayable) 는 Raw() 로 FMOD::Sound* 조회만. 직접 release 금지.
    ///   - dtor 가 FMOD::Sound::release() 호출 — ResourceRegistry::Clear() 시 일괄 정리.
    class Sound
    {
      public:
        explicit Sound(::FMOD::Sound* raw) : mRaw(raw) {}
        ~Sound();

        ::FMOD::Sound* Raw() const { return mRaw; }

        Sound(const Sound&)            = delete;
        Sound& operator=(const Sound&) = delete;
        Sound(Sound&&)                 = delete;
        Sound& operator=(Sound&&)      = delete;

      private:
        ::FMOD::Sound* mRaw = nullptr;
    };
}

#endif // __SJH_RESOURCE_REGISTRY_SOUND_H__
