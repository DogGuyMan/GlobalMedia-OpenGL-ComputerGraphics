#ifndef __TOPDOWNSHOOTER_TEXT_WORLD_TEXT_SYSTEM_H__
#define __TOPDOWNSHOOTER_TEXT_WORLD_TEXT_SYSTEM_H__

#include "text/bitmap_font.h"

namespace TopdownShooter::Text
{
    /// @brief minogram BMFont 1개를 보유하는 얇은 시스템 (VFX/Audio 시스템 평행). Manager 보유.
    class WorldTextSystem
    {
      public:
        /// @note 인자 없이 내부에서 ResourceRegistry::Get() 사용 — 호출자(Manager.cpp)가
        ///       resource_registry.h(gl3w.h)를 끌어오지 않게 해 Effekseer gl3.h 와의 GL 헤더 충돌 회피.
        void Init();
        SJH::Text::BitmapFont* GetFont() { return mLoaded ? &mFont : nullptr; }

      private:
        SJH::Text::BitmapFont mFont;
        bool mLoaded = false;
    };
}

#endif // __TOPDOWNSHOOTER_TEXT_WORLD_TEXT_SYSTEM_H__
