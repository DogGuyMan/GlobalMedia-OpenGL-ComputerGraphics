// 의존성 등록 검증용 임시 타겟 — Task 8 에서 비활성화한다.
#include <sb7.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#include <Effekseer/Effekseer.h>
#include <Effekseer/EffekseerRendererGL.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <box2d/box2d.h>
#include <tweeny/tweeny.h>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <vector>

namespace TopdownShooter
{
	class game_application : public sb7::application
	{
		void startup() override
		{
		}

		void render(double currentTime) override
		{
		}
	};

} // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
