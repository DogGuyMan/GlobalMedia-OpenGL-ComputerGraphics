// 의존성 등록 검증용 임시 타겟 — Task 8 에서 비활성화한다.
#include <sb7.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#include <Effekseer/Effekseer.h>
#include <Effekseer/EffekseerRendererGL.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <box2d/box2d.h>
#include <mutex>
#include <tweeny/tweeny.h>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <vector>

class deptest_application : public sb7::application
{
	void startup() override
	{

		// Tweeny — 0->100 보간 트윈
		auto tween = tweeny::from(0).to(100).during(100);
		int mid = tween.step(50);

		// stb_rect_pack — 패킹 컨텍스트 초기화
		stbrp_context ctx;
		std::vector<stbrp_node> nodes(64);
		stbrp_init_target(&ctx, 256, 256, nodes.data(), static_cast<int>(nodes.size()));

		// Box2D — 중력 월드 + 동적 바디 1개
		b2World world(b2Vec2(0.0f, -10.0f));
		b2BodyDef bodyDef;
		bodyDef.type = b2_dynamicBody;
		bodyDef.position.Set(0.0f, 4.0f);
		b2Body *body = world.CreateBody(&bodyDef);
		world.Step(1.0f / 60.0f, 8, 3);
		std::printf("[deptest] box2d body y=%.3f\n", body->GetPosition().y);

		// assimp — Importer 인스턴스 생성 (링크 검증)
		Assimp::Importer assimpImporter;
		std::printf("[deptest] assimp importer ready (err='%s')\n",
					assimpImporter.GetErrorString());


		// spdlog — 컴파일 정적 라이브러리 링크 검증
		spdlog::info("[deptest] spdlog compiled-lib OK (v{}.{}.{})",
					 SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
	}

	void render(double currentTime) override
	{
		static const GLfloat green[] = {0.0f, 0.25f, 0.0f, 1.0f};
		glClearBufferfv(GL_COLOR, 0, green);

		// Effekseer — 매니저 + GL 렌더러 1회 생성 (심볼 링크 검증용)
		static std::once_flag efkOnce;
		std::call_once(efkOnce, [] {
			auto manager = Effekseer::Manager::Create(8000);
			auto renderer = EffekseerRendererGL::Renderer::Create(
				8000, EffekseerRendererGL::OpenGLDeviceType::OpenGL3);
			std::printf("[deptest] effekseer manager=%p renderer=%p\n",
					static_cast<void *>(manager.Get()),
					static_cast<void *>(renderer.Get()));
		});
	}
};

DECLARE_MAIN(deptest_application);
