/**
 * @file sound.cpp
 * @brief @c Sound 소멸자 정의 - @c SJH_HAS_FMOD 가드 내에서 @c FMOD::Sound::release() 호출.
 *
 * @details
 *  FMOD 미설치(@c SJH_HAS_FMOD 미정의) 환경에서는 dtor 본문이 no-op - 링크 에러 없이 스텁 동작.
 *  @c fmod/fmod.hpp 는 이 TU 에서만 include - 헤더(@c sound.h)는 FMOD-free 전방선언만.
 */
#include "sound.h"
#ifdef SJH_HAS_FMOD
#include <fmod/fmod.hpp>
#endif

namespace SJH
{
    Sound::~Sound()
    {
#ifdef SJH_HAS_FMOD
        if (mRaw)
        {
            mRaw->release();
            mRaw = nullptr;
        }
#endif
    }
}
