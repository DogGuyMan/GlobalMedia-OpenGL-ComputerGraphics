#include "Text/WorldTextSystem.h"

#include "text/bitmap_font.h"
#include "resource_registry/resource_registry.h"

namespace TopdownShooter::Text
{
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
