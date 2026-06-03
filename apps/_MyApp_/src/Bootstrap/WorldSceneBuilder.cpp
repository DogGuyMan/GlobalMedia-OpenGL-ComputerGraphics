#include <GL/gl3w.h> // GL_REPEAT / GL_LINEAR (skybox 텍스처). 반드시 다른 GL 헤더보다 먼저.

#include "Bootstrap/WorldSceneBuilder.h"

#include "InputHandler/ActorFolower.h"

#include "material/material.h"
#include "material/pass.h"
#include "object/light.h"
#include "object/mesh.h"
#include "resource_registry/image.h"
#include "resource_registry/resource_registry.h"
#include "resource_registry/texture.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/compound_actor.h"
#include "scene/scene.h"

#include <vmath.h>

namespace TopdownShooter::Bootstrap
{
	namespace
	{
		// ── World Camera (Perspective) — 3D 월드 ────────────────────────────
		SJH::Scene::Camera *BuildWorldCamera(const WorldSceneDeps &deps)
		{
			auto &dir = SJH::Scene::Director::Get();

			auto worldCamActor = SJH::Scene::CreateCameraActor("WorldCamera", 45.0f, deps.aspect, 0.1f, 1000.0f);
			auto &worldCamTransform = worldCamActor->GetTransform();
			worldCamTransform.SetTransformWithVectors(
			                     vmath::vec3(0.0f, 3.0f, 6.0f),
			                     vmath::vec3(-30.0f, 0.0f, 0.0))
			    .PrintTransform();

			auto *camera = worldCamActor->GetComponent<SJH::Scene::Camera>();
			camera
			    ->SetCullingMask(SJH::Scene::Layer::Default |
			                     SJH::Scene::Layer::Player |
			                     SJH::Scene::Layer::Enemy |
			                     SJH::Scene::Layer::DebugDraw)
			    .SetTargetRenderTarget(deps.sceneFB);

			worldCamActor
			    ->AddComponent<Controller::ActorFolower>()
			    ->SetMouseInput(deps.mouse)
			    .SetCamera(camera)
			    .SetFollowOffset(worldCamTransform.Translate)
			    .SetUp();

			dir.Root().AddChild(std::move(worldCamActor));
			return camera;
		}

		// ── DirLight ─────────────────────────────────────────────────────────
		void BuildLighting()
		{
			auto &dir = SJH::Scene::Director::Get();

			auto lightActor = SJH::Scene::CreateDirLightActor("MainDirLight",
			    vmath::vec3(-0.4f, -1.0f, -0.5f));
			auto *light = lightActor->GetComponent<SJH::DirLight>();
			light->Ambient  = vmath::vec3(0.3f, 0.3f, 0.3f);
			light->Diffuse  = vmath::vec3(0.9f, 0.9f, 0.85f);
			light->Specular = vmath::vec3(0.5f, 0.5f, 0.5f);
			dir.Root().AddChild(std::move(lightActor));
		}

		// ── Matrix Skybox (프로그램 + 텍스처 + 머티리얼 + Actor) ────────────
		SJH::Material *BuildSkybox()
		{
			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();

			auto *skyboxProg = reg.CreateProgram(
			    "matrix_skybox",
			    "resources/shaders/matrix_skybox.vs",
			    "resources/shaders/matrix_skybox.fs");

			auto *charsTex = reg.CreateTexture("chars", SJH::Image::Load("chars", "resources/texture/characters.png").get());
			// 매트릭스 글자 스크롤 필수 — 셰이더의 char_uv.x 가 +noise+time 으로 1 을 넘어 순환한다.
			// CreateTexture 기본 wrap 은 GL_CLAMP_TO_EDGE(texture.cpp) 라 끝 열에 고착돼 세로 줄로
			// 보이므로, 글자 열이 순환하도록 REPEAT 로 덮어쓴다 (uniform_atlas Bind->SetWrap 선례).
			charsTex->Bind();
			charsTex->SetWrap(GL_REPEAT, GL_REPEAT);
			// 촘촘한 격자에서 글자칸이 작아지면 기본 MIPMAP_LINEAR(texture.cpp)가 LOD 를 올려
			// 글자를 회색으로 뭉갠다 -> mipmap 없는 GL_LINEAR 로 또렷하게 유지.
			charsTex->SetFilter(GL_LINEAR, GL_LINEAR);
			auto *noiseTex = reg.CreateTexture("noise_tex", SJH::Image::Load("noise_tex", "resources/texture/matrix_noise.png").get());
			// 셰이더가 noise 좌표를 NOISE_SCALE(=8)배로 키워 샘플 -> 1 을 넘는 좌표가 클램프되지
			// 않고 타일링되도록 REPEAT 필수 (CLAMP 면 가장자리 한 색으로 뭉개짐).
			noiseTex->Bind();
			noiseTex->SetWrap(GL_REPEAT, GL_REPEAT);

			auto *skyboxMat = reg.CreateSharedMaterial("mat_matrix_skybox");
			skyboxMat->SetProgram(skyboxProg);
			// Skybox Pass — DepthFunc LEQUAL(.xyww 트릭) + CullMode FRONT(박스 안쪽 면) +
			// DepthWrite off + queue 2500(Opaque 다음). pass.h 의 Kind::Skybox 가 전부 자동 도출.
			skyboxMat->SetPass(SJH::Pass::Kind::Skybox);
			// 텍스처 유닛 분리 필수 — TextureBinding.Unit 이 둘 다 기본값 0 이면
			// 두 sampler 가 같은 유닛을 가리켜 한 텍스처만 읽힌다 (PropertyBlockSetter 가
			// binding.Unit 그대로 BindTexture + sampler int 송신).
			skyboxMat->Properties.Textures["chars"] = {charsTex, 0};
			skyboxMat->Properties.Textures["noise_tex"] = {noiseTex, 1};
			skyboxMat->Properties.Floats["u_time"] = 0.0f;

			auto *skyboxMesh = reg.RegisterMesh("mesh_skybox", SJH::Mesh::CreateBox());
			// actor 생성은 Pure factory.
			dir.Root().AddChild(SJH::Scene::CreateSkyboxActor("MatrixSkybox", skyboxMesh, skyboxMat, 50.0f));
			return skyboxMat;
		}
	} // namespace

	WorldSceneResult BuildWorldScene(const WorldSceneDeps &deps)
	{
		WorldSceneResult result;
		result.WorldCamera = BuildWorldCamera(deps);
		BuildLighting();
		result.SkyboxMat = BuildSkybox();
		return result;
	}
}
