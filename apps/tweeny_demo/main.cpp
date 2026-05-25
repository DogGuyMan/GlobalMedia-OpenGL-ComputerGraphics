/**
 * @file main.cpp
 * @brief tweeny_demo — 11 easings ping-pong planes (Task 3: single plane sanity).
 *        Tweeny 도입 전 SJH API (Mesh + Program + Uniforms) 통합 동작 확인.
 */

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <sb7.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <tweeny/tweeny.h>
#include <utility>
#include <vector>
#include <vmath.h>

#include "Constants.h"
#include "Controller.PingPongTween.h"
#include "common/common.h"
#include "material/material.h"
#include "material/material_uniforms.h"
#include "object/mesh.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "render/mesh_renderer.h"
#include "render/render_stage.h"
#include "render/render_target.h"
#include "render/scene_renderer.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/compound_actor.h"
#include "scene/layer.h"
#include "scene/scene.h"

#include "client.h"

namespace
{
	struct EasingRow
	{
		vmath::vec4 color;
		SJH::Material *material = nullptr;
		SJH::Scene::Actor *actor = nullptr;
	};

} // namespace

class tweeny_demo_app : public sb7::application
{
  public:
	void init() override
	{
		sb7::application::init();
		info.majorVersion = 4;
		info.minorVersion = 1;
		info.windowWidth = 800;
		info.windowHeight = 600;

#ifdef _WIN32
		strncpy_s(info.title, sizeof(info.title), "tweeny_demo - 11 easings ping-pong", _TRUNCATE);
#else
		std::strncpy(info.title, "tweeny_demo - 11 easings ping-pong", sizeof(info.title) - 1);
		info.title[sizeof(info.title) - 1] = '\0';
#endif
	}

	void startup() override
	{
		mQuad = SJH::Mesh::CreateScreenQuad();
		mProgram = SJH::Program::CreateWithVSFS(
		    TweenyDemo::Constants::VS_PATH,
		    TweenyDemo::Constants::FS_PATH);

		std::mt19937 rng{42u};
		std::uniform_real_distribution<float> distColor(0.3f, 1.0f);

		int fbW = 0, fbH = 0;
		glfwGetFramebufferSize(window, &fbW, &fbH);
		onResize(fbW, fbH);

		auto makeRow = [&](const std::string &name, auto &&easing) {
			
			static constexpr float kTweenFromX = -0.85f;
			static constexpr float kTweenToX = 0.85f;
			static constexpr int kTweenDurationMs = 1000;

			auto &dir = SJH::Scene::Director::Get();
			auto actor = std::make_unique<SJH::Scene::Actor>(name);
			actor->AddComponent<TweenyDemo::Controller::PingPongTween>()
			    ->SetPingPong(kTweenFromX, kTweenToX, kTweenDurationMs, easing)
			    .SetAxis(0);

			EasingRow row;
			row.actor = actor.get();
			row.color = vmath::vec4(distColor(rng), distColor(rng), distColor(rng), 1.0f);
			dir.Root().AddChild(std::move(actor));

			return row;
		};
		
		static constexpr int kRowCount = 11;
		mRows.reserve(kRowCount);
		mRows.push_back(makeRow(TweenyDemo::Constants::NAME_LINEAR_TWEEN, tweeny::easing::linear));
		mRows.push_back(makeRow(TweenyDemo::Constants::NAME_QUADRATICINOUT_TWEEN, tweeny::easing::quadraticInOut));
		mRows.push_back(makeRow(TweenyDemo::Constants::NAME_CUBICINOUT_TWEEN, tweeny::easing::cubicInOut));
		mRows.push_back(makeRow(TweenyDemo::Constants::NAME_QUARTICINOUT_TWEEN, tweeny::easing::quarticInOut));
		mRows.push_back(makeRow(TweenyDemo::Constants::NAME_QUINTICINOUT_TWEEN, tweeny::easing::quinticInOut));
		mRows.push_back(makeRow(TweenyDemo::Constants::NAME_SINUSOIDALINOUT_TWEEN, tweeny::easing::sinusoidalInOut));
		mRows.push_back(makeRow(TweenyDemo::Constants::NAME_EXPONENTIALINOUT_TWEEN, tweeny::easing::exponentialInOut));
		mRows.push_back(makeRow(TweenyDemo::Constants::NAME_CIRCULARINOUT_TWEEN, tweeny::easing::circularInOut));
		mRows.push_back(makeRow(TweenyDemo::Constants::NAME_BOUNCEINOUT_TWEEN, tweeny::easing::bounceInOut));
		mRows.push_back(makeRow(TweenyDemo::Constants::NAME_ELASTICINOUT_TWEEN, tweeny::easing::elasticInOut));
		mRows.push_back(makeRow(TweenyDemo::Constants::NAME_BACKINOUT_TWEEN, tweeny::easing::backInOut));

		auto &reg = SJH::ResourceRegistry::Get();
		auto &dir = SJH::Scene::Director::Get();

		auto tsm = reg.CreateSharedMaterial(TweenyDemo::Constants::MATERIAL_TWEENY_TEMPLATE);
		tsm->SetProgram(mProgram.get());

		for (std::size_t i = 0; i < static_cast<std::size_t>(kRowCount); ++i)
		{
			const float kRowTopY = 0.9f;
			const float kRowBottomY = -0.9f;
			auto &row = mRows[i];

			const std::string key = std::string("tweeny_row_") + row.actor->GetName();
			row.material = reg.CreateMaterialInstanceFrom(key, tsm);
			SJH::Uniforms::SetVec4(*row.material, "baseColor", row.color);

			row.actor->AddComponent<SJH::Scene::MeshRenderer>(mQuad.get(), row.material);

			float t = (kRowCount == 1) ? 0.5f : static_cast<float>(i) / static_cast<float>(kRowCount - 1);
			auto &xform = row.actor->GetTransform();
			xform.Translate[1] = (1.0f - t) * kRowTopY + t * kRowBottomY;
			xform.Scale = vmath::vec3(0.05f, 0.035f, 1.0f);
		}

		// === SP5 — SceneCamera Actor 도입 (옛 Render(RT, I, I) 우회 청산) ===
		{
			int fbW2 = 0, fbH2 = 0;
			glfwGetFramebufferSize(window, &fbW2, &fbH2);
			const float aspect = static_cast<float>(fbW2) / static_cast<float>(fbH2);

			auto camActor = SJH::Scene::CreateCameraActor("SceneCamera",
			    /*fov*/45.0f, aspect, /*near*/0.1f, /*far*/100.0f);
			camActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 5.0f);
			auto* cam = camActor->GetComponent<SJH::Scene::Camera>();
			cam->SetCullingMask(SJH::Scene::Layer::Default);   // 명시 — UI 비트 제외
			cam->SetTargetFramebuffer(nullptr);                // backbuffer
			dir.Root().AddChild(std::move(camActor));
			dir.SetActiveCamera(cam);
		}

		mStages.push_back(&mRenderSys);
		dir.Enter();
	}

	void render(double currentTime) override
	{
		double dt = SJH::DeltaTime(currentTime);
		int32_t dtMs = static_cast<int32_t>(dt * 1000);
		if (dtMs < 0)
			dtMs = 0;

		SJH::Scene::Director::Get().Update((float)dt);
		for (auto* s : mStages) s->Render(*mDefaultTarget);
	}

	void shutdown() override
	{
		mProgram.reset();
		mQuad.reset();
	}

	void onResize(int logicalW, int logicalH) override
	{
		int w = 0, h = 0;
		glfwGetFramebufferSize(window, &w, &h);
		if (w <= 0 || h <= 0)
			return;

		sb7::application::onResize(w, h);
		glViewport(0, 0, w, h);
		mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(w, h);
		for (auto* s : mStages) s->OnResize(w, h);   // broadcast
	}

  private:
	SJH::SceneRenderer mRenderSys;
	std::vector<SJH::IRenderStage*> mStages;   // 호출 순서, non-owning
	SJH::MeshUPtr mQuad;
	SJH::ProgramUPtr mProgram;
	SJH::RenderTargetUPtr mDefaultTarget;

	std::vector<EasingRow> mRows;
};

DECLARE_MAIN(tweeny_demo_app)