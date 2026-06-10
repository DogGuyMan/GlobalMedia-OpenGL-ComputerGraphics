/**
 * @file WorldTextSystem.cpp
 * @brief @c TopdownShooter::Text::WorldTextSystem 구현 - minogram BMFont 로드.
 *
 * @details
 *  ### 책임
 *  - @c SJH::ResourceRegistry 를 통해 minogram PNG atlas + BMFont XML 을 로드해 @c mFont 채움.
 *
 * @note resource_registry.h 가 gl3w.h 를 끌어오므로 본 .cpp 안에서만 include 한다
 *       (헤더는 GL-free 유지 - Effekseer gl3.h 충돌 회피).
 */
#include "Text/WorldTextSystem.h"

#include "text/bitmap_font.h"
#include "resource_registry/resource_registry.h"

namespace TopdownShooter::Text
{
    /// @copydoc WorldTextSystem::Init
    void WorldTextSystem::Init()
    {
        auto& reg = SJH::ResourceRegistry::Get();
        mFont = SJH::Text::BitmapFont::LoadFromBMFont(
            reg, "minogram_6x10",
            "resources/font/minogram_6x10.png",
            "resources/font/minogram_6x10.xml");
        mLoaded = mFont.IsValid();
    }
}
