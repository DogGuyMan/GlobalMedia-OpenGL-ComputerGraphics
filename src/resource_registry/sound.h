/**
 * @file sound.h
 * @brief @c FMOD::Sound* 의 소유권 RAII 래퍼 - @c ResourceRegistry 캐시 엔트리용.
 *
 * @details
 *  ### 책임
 *  - @c FMOD::Sound* 단일 소유 + 소멸 시 @c FMOD::Sound::release() 자동 호출.
 *  - @c ResourceRegistry::CreateSound 가 생성하고 @c mSounds 맵에 @c unique_ptr 로 보관.
 *  - 외부(@c FmodPlayable 등)는 @c Raw() 로 포인터만 조회 - 직접 @c release 금지.
 *
 *  ### 비-책임
 *  - [X] @c FMOD::System* 보유 - 외부 AudioSystem 이 owner, 본 클래스는 ptr 을 사용하지 않음.
 *  - [X] 재생 제어 (@c play / @c stop) - @c FmodPlayable 책임.
 *
 *  ### FMOD 옵셔널 컴파일 가드
 *  @c SJH_HAS_FMOD 미정의 환경에서는 @c sound.cpp 의 dtor 가 @c release 를 건너뜀.
 *  헤더는 FMOD-free - @c FMOD::Sound 는 전방선언만.
 *
 * @note M5(2026-05-26) 신설. @c ResourceRegistry::Clear() 시 @c mSounds.clear() 로 일괄 정리.
 */
#ifndef __SJH_RESOURCE_REGISTRY_SOUND_H__
#define __SJH_RESOURCE_REGISTRY_SOUND_H__

#include "common/common.h"

// fwd - FMOD 헤더는 sound.cpp 안에서만 (옵셔널 컴파일 가드 SJH_HAS_FMOD)
namespace FMOD { class Sound; }

namespace SJH
{
    CLASS_PTR(Sound)
    /**
     * @brief @c FMOD::Sound* 소유권 RAII 래퍼.
     * @details 복사/이동 모두 금지 - @c ResourceRegistry 가 @c unique_ptr 로 단일 소유.
     *          소멸 시 @c FMOD::Sound::release() 호출 (@c SJH_HAS_FMOD 가드 내).
     */
    class Sound
    {
      public:
        /// @brief @p raw 소유권을 인수받아 RAII 래퍼 생성.
        /// @param raw @c ResourceRegistry::CreateSound 가 @c sys->createSound 로 얻은 핸들. nullptr 비허용.
        explicit Sound(::FMOD::Sound* raw) : mRaw(raw) {}

        /// @brief @c FMOD::Sound::release() 호출 - @c SJH_HAS_FMOD 가드 내.
        ~Sound();

        /// @brief 보유 @c FMOD::Sound* 반환. 소유권 X - @c Sound 수명 동안만 유효.
        ::FMOD::Sound* Raw() const { return mRaw; }

        Sound(const Sound&)            = delete; ///< FMOD 핸들 단일 소유 - 복사 금지.
        Sound& operator=(const Sound&) = delete;
        Sound(Sound&&)                 = delete; ///< 이동도 금지 - ResourceRegistry unique_ptr 보유.
        Sound& operator=(Sound&&)      = delete;

      private:
        ::FMOD::Sound* mRaw = nullptr; ///< FMOD 오디오 핸들 - nullptr 이면 해제 생략.
    };
} // namespace SJH

#endif // __SJH_RESOURCE_REGISTRY_SOUND_H__
