#include "sound.h"
#include <fmod/fmod.hpp>

namespace SJH
{
    Sound::~Sound()
    {
        if (mRaw)
        {
            mRaw->release();
            mRaw = nullptr;
        }
    }
}
