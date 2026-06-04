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
