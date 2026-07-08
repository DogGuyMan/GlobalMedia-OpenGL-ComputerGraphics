/**
 * @file WorldSceneBuilder.cpp
 * @brief @c BuildWorldScene 및 내부 분할 자유함수 구현 - 월드 씬 조립 세 단계 처리.
 *
 * @details
 *  ### 책임
 *  - @c BuildWorldCamera : 45도 FOV Perspective Camera + @c ActorFolower(pan/zoom) 조립.
 *    카메라 culling mask = Default | Player | Enemy | DebugDraw.
 *  - @c BuildLighting : 주 방향광(@c DirLight) 생성 + Ambient(0.3)/Diffuse(0.9,0.85)/Specular(0.5) 설정.
 *  - @c BuildSkybox : Matrix 스타일 스크롤 skybox - chars 텍스처(GL_REPEAT) + noise 텍스처(GL_REPEAT)
 *    + @c Pass::RenderQueue::Skybox 머티리얼 + Box 메시 + SkyboxActor 등록. 반환된 머티리얼 포인터로
 *    render() 가 매 프레임 @c u_time 을 갱신해 스크롤 애니메이션을 구동.
 *
 *  ### 비-책임
 *  - [X] 플레이어/적/UI 조립 - @c PlayerBuilder / EnemyBuilder 등 담당.
 *
 * @note @c gl3w.h 는 반드시 다른 GL 헤더보다 먼저 포함해야 한다.
 *       chars/noise 텍스처는 @c GL_REPEAT 이 필수 (기본 CLAMP_TO_EDGE 면 스크롤 아티팩트 발생).
 */
#include <GL/gl3w.h> // GL_REPEAT / GL_LINEAR (skybox 텍스처). 반드시 다른 GL 헤더보다 먼저.

#include "Bootstrap/WorldSceneBuilder.h"

#include "InputHandler/ActorFolower.h"

#include "material/material.h"
#include "material/pass.h"
#include "scene/light.h"
#include "object/mesh.h"
#include "texture/image.h"
#include "resource_registry/resource_registry.h"
#include "texture/texture.h"
#include "scene/actor.h"
#include "scene/camera.h"
#include "scene/compound_actor.h"
#include "Bootstrap/actor_factory.h"  // CreateSkyboxActor (2026-06-24 apps client 이주)
#include "render/mesh_renderer.h"  // SJH::Scene::MeshRenderer - BuildSkybox 반환 타입
#include "object/model.h"            // SJH::Model (BuildPcbModel: GetMaterialCount/GetMaterial)
#include "program/program.h"         // SJH::Program (PCB phong 프로그램)
#include "Bootstrap/model_spawner.h" // SJH::Scene::ModelSpawner::SpawnEntities (PCB RenderUnit 펼침)
#include "scene/scene.h"

// -- BuildStage (구 StageBuilder 흡수) 의존 --------------------------------------
#include "material/material_uniforms.h"          // SJH::Uniforms::Set* (벽 머티리얼)
#include "Stage/Constants.h"                      // ARENA_HALF_EXTENT / WALL_THICKNESS
#include "Stage/Stage.h"                          // EStageStatus
#include "Stage/Components/StageStateComponent.h" // Stage::Components::StageState
#include "Stage/Components/MaterialTimeComponent.h" // Stage::Components::MaterialTime
#include "Stage/Factories/wall_factory.h"         // Stage::Factories::CreateWallActor (inline)
#include "GameSystems.h"                          // GameSystems::Get().VFX() (Orbit VFX)
#include "VFX/EffekseerPlayable.h"                // VFX::EffekseerPlayable (Orbit loop)

#include <box2d/box2d.h>
#include <memory>
#include <glm/glm.hpp>

namespace TopdownShooter::Bootstrap
{
	namespace
	{
		// -- World Camera (Perspective) - 3D 월드 ----------------------------
		/// @brief Perspective WorldCamera Actor 를 생성하고 @c Director::Root() 에 추가한다.
		/// @details FOV 45도, near 0.1, far 1000, 초기 위치 (0, 3, 6), pitch -30도.
		///          culling mask = Default | Player | Enemy | DebugDraw.
		///          @c ActorFolower 를 부착해 pan/zoom 을 마우스로 제어하고, @p deps.sceneFB 를
		///          RenderTarget 으로 연결한다.
		/// @param deps @c WorldSceneDeps (aspect / mouse / sceneFB).
		/// @return 생성된 @c SJH::Scene::Camera 포인터 (Actor 는 @c Root() 가 소유).
		SJH::Scene::Camera *BuildWorldCamera(const WorldSceneDeps &deps)
		{
			auto &dir = SJH::Scene::Director::Get();

			auto worldCamActor = SJH::Scene::CreateCameraActor("WorldCamera", 45.0f, deps.aspect, 0.1f, 1000.0f);
			auto &worldCamTransform = worldCamActor->GetTransform();
			worldCamTransform.SetTransformWithVectors(
			                     glm::vec3(0.0f, 3.0f, 6.0f),
			                     glm::vec3(-30.0f, 0.0f, 0.0))
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

		// -- DirLight ---------------------------------------------------------
		/// @brief 주 방향광(@c DirLight) Actor 를 생성하고 @c Director::Root() 에 추가한다.
		/// @details 방향 (-0.4, -1.0, -0.5), Ambient 0.3, Diffuse 0.9/0.85, Specular 0.5.
		///          @c SceneRenderer 가 OnEnter 훅으로 @c SceneContext 에 자동 등록한다
		///          (@c scenecontext_auto_register 컨벤션).
		void BuildLighting()
		{
			auto &dir = SJH::Scene::Director::Get();

			auto lightActor = SJH::Scene::CreateDirLightActor("MainDirLight",
			    glm::vec3(-0.4f, -1.0f, -0.5f));
			auto *light = lightActor->GetComponent<SJH::DirLight>();
			light->Ambient  = glm::vec3(0.3f, 0.3f, 0.3f);
			light->Diffuse  = glm::vec3(0.9f, 0.9f, 0.85f);
			light->Specular = glm::vec3(0.5f, 0.5f, 0.5f);
			dir.Root().AddChild(std::move(lightActor));
		}

		// -- Matrix Skybox (프로그램 + 텍스처 + 머티리얼 + Actor) ------------
		/// @brief Matrix 스타일 스크롤 Skybox 를 조립하고 @c Director::Root() 에 추가한다.
		/// @details 조립 내용:
		///  - 프로그램 : matrix_skybox.vs / .fs.
		///  - chars 텍스처 : GL_REPEAT + GL_LINEAR (기본 CLAMP_TO_EDGE + MIPMAP_LINEAR 덮어씀).
		///  - noise 텍스처 : GL_REPEAT (NOISE_SCALE 8배 타일링 필수).
		///  - 머티리얼 : @c Pass::RenderQueue::Skybox (DepthFunc LEQUAL + CullFront + DepthWrite off).
		///  - 텍스처 유닛 분리 : chars -> unit 0, noise_tex -> unit 1 (같은 유닛이면 한 텍스처만 읽힘).
		///  - SkyboxActor 스케일 50.
		/// @return Skybox MeshRenderer(IRenderable) 포인터. SkyboxMat 은 mr->Material 로 도출. SkyboxPass 가 그릴 대상.
		SJH::Scene::MeshRenderer *BuildSkybox()
		{
			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();

			auto *skyboxProg = reg.CreateProgram(
			    "matrix_skybox",
			    "resources/shaders/matrix_skybox.vs",
			    "resources/shaders/matrix_skybox.fs");

			auto *charsTex = reg.CreateTexture("chars", SJH::Image::Load("chars", "resources/texture/characters.png").get());
			// 매트릭스 글자 스크롤 필수 - 셰이더의 char_uv.x 가 +noise+time 으로 1 을 넘어 순환한다.
			// CreateTexture 기본 wrap 은 GL_CLAMP_TO_EDGE(texture.cpp) 라 끝 열에 고착돼 세로 줄로
			// 보이므로, 글자 열이 순환하도록 REPEAT 로 덮어쓴다 (uniform_atlas Bind->SetWrap 선례).
			charsTex->Bind();
			charsTex->SetWrap(SJH::WrapMode::Repeat, SJH::WrapMode::Repeat);
			// 촘촘한 격자에서 글자칸이 작아지면 기본 MIPMAP_LINEAR(texture.cpp)가 LOD 를 올려
			// 글자를 회색으로 뭉갠다 -> mipmap 없는 GL_LINEAR 로 또렷하게 유지.
			charsTex->SetFilter(SJH::FilterMode::Linear, SJH::FilterMode::Linear);
			auto *noiseTex = reg.CreateTexture("noise_tex", SJH::Image::Load("noise_tex", "resources/texture/matrix_noise.png").get());
			// 셰이더가 noise 좌표를 NOISE_SCALE(=8)배로 키워 샘플 -> 1 을 넘는 좌표가 클램프되지
			// 않고 타일링되도록 REPEAT 필수 (CLAMP 면 가장자리 한 색으로 뭉개짐).
			noiseTex->Bind();
			noiseTex->SetWrap(SJH::WrapMode::Repeat, SJH::WrapMode::Repeat);

			auto *skyboxMat = reg.CreateSharedMaterial("mat_matrix_skybox");
			skyboxMat->SetProgram(skyboxProg);
			// Skybox Pass - DepthFunc LEQUAL(.xyww 트릭) + CullMode FRONT(박스 안쪽 면) +
			// DepthWrite off + queue 2500(Opaque 다음). pass.h 의 Kind::Skybox 가 전부 자동 도출.
			skyboxMat->SetPass(SJH::Pass::RenderQueue::Skybox);
			// 텍스처 유닛 분리 필수 - TextureBinding.Unit 이 둘 다 기본값 0 이면
			// 두 sampler 가 같은 유닛을 가리켜 한 텍스처만 읽힌다 (BindSamplers 가
			// binding.Unit 그대로 BindTexture + sampler int 송신).
			skyboxMat->Properties.Textures["chars"] = {charsTex, 0};
			skyboxMat->Properties.Textures["noise_tex"] = {noiseTex, 1};
			skyboxMat->Properties.Floats["u_time"] = 0.0f;

			auto *skyboxMesh = reg.RegisterMesh("mesh_skybox", SJH::Mesh::CreateBox());
			// actor 생성은 Pure factory. SkyboxPass 가 그릴 수 있게 MeshRenderer 를 반환(skyboxMat 는 mr->Material 로 도출).
			auto *skyboxActor = dir.Root().AddChild(SJH::Scene::CreateSkyboxActor("MatrixSkybox", skyboxMesh, skyboxMat, 50.0f));
			return skyboxActor->GetComponent<SJH::Scene::MeshRenderer>();
		}

		// -- PCB 장식 3D 모델 (Phong lit, 물리 무관) - StageBuilder 물리아레나에서 이관 --------
		/// @brief PCB 3D 모델을 Phong 알베도 셰이더로 조립해 @c Director::Root() 에 추가한다.
		/// @details 자원(모델/Phong program)은 idempotent(find-or-create) 등록. model.cpp 가 저장한
		///          material.albedo(Vec3) 를 MaterialBlock.baseColor(Vec4) 로 승격 - slang phong UBO 입력.
		///          @c ModelSpawner 로 모델의 RenderUnit 을 자식 Actor 로 펼친다. 물리 무관이라 환경 요소.
		void BuildPcbModel()
		{
			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();

			// 자원 등록 (idempotent - 이미 있으면 Find 재사용). 키는 continuity 위해 "stage_" 유지.
			SJH::Model *pcbModel = reg.FindModel("stage_pcb");
			if (!pcbModel)
				pcbModel = reg.CreateModel("stage_pcb", "resources/model/pcb.fbx");

			SJH::Program *pcbProg = reg.FindProgram("stage_phong_albedo");
			if (!pcbProg)
				pcbProg = reg.CreateProgram("stage_phong_albedo",
				                            "resources/shaders/phong.vs",
				                            "resources/shaders/phong.fs");

			// slang phong UBO 셰이더 주입 + albedo(Vec3) -> baseColor(Vec4) 승격 (idempotent).
			for (int i = 0; i < pcbModel->GetMaterialCount(); ++i)
			{
				if (SJH::Material *mat = pcbModel->GetMaterial(i))
				{
					if (mat->GetProgram() == nullptr)
						mat->SetProgram(pcbProg);
					const auto albedoIt = mat->Properties.Vec3s.find("material.albedo");
					const glm::vec3 albedo = (albedoIt != mat->Properties.Vec3s.end())
					                             ? albedoIt->second
					                             : glm::vec3(0.8f, 0.8f, 0.8f);
					mat->Properties.Vec4s["baseColor"] = glm::vec4(albedo[0], albedo[1], albedo[2], 1.0f);
				}
			}

			// PCB Actor - Transform + ModelSpawner 로 RenderUnit 자식 펼침. Root 직속(환경 요소).
			auto pcbActor = std::make_unique<SJH::Scene::Actor>("PcbActor");
			pcbActor->GetTransform().SetTransformWithVectors(
			    glm::vec3(0.0, -1.75, 0.0),
			    glm::vec3(90.0, 0.0, 0),
			    glm::vec3(0.75, 0.75, 0.75));
			SJH::Scene::ModelSpawner::SpawnEntities(*pcbActor, *pcbModel);
			dir.Root().AddChild(std::move(pcbActor));
		}

		// -- 물리 아레나 스테이지 (구 StageBuilder::CreateStageActor 흡수) ------------------
		//    벽 4개(물리 바디 + PoliceTape 반투명 시각) + StageState + Orbit 배경 VFX.
		//    자원 키는 "stage_" prefix 유지 (외부 참조 continuity + 골든 무회귀).

		// ResourceRegistry key 상수.
		constexpr const char *kPlaneKey = "stage_plane";           ///< Plane mesh 등록 key (벽 시각화).
		constexpr const char *kWallMatKey = "stage_wall";          ///< 반투명 벽 공유 Material key.
		constexpr const char *kTransparentProgKey = "stage_transparent"; ///< 반투명 벽 Program key.
		constexpr const char *kTransparentVS = "resources/shaders/transparent.vs"; ///< 반투명 벽 VS.
		constexpr const char *kTransparentFS = "resources/shaders/transparent.fs"; ///< 반투명 벽 FS.
		constexpr const char *kWallTexKey = "stage_police_tape";   ///< PoliceTape 텍스처 key.
		constexpr const char *kWallTexPath = "resources/texture/PoliceTape.png"; ///< PoliceTape 경로.

		/// @brief 벽 시각화용 Plane mesh 를 idempotent 하게 등록/조회.
		SJH::Mesh *EnsurePlane(SJH::ResourceRegistry &reg)
		{
			if (auto *existing = reg.FindMesh(kPlaneKey))
				return existing;
			return reg.RegisterMesh(kPlaneKey, SJH::Mesh::CreatePlane());
		}

		/// @brief 반투명 벽 unlit Program 을 idempotent 하게 등록/조회.
		SJH::Program *EnsureTransparentProgram(SJH::ResourceRegistry &reg)
		{
			if (auto *existing = reg.FindProgram(kTransparentProgKey))
				return existing;
			return reg.CreateProgram(kTransparentProgKey, kTransparentVS, kTransparentFS);
		}

		/// @brief PoliceTape 텍스처를 idempotent 하게 등록/조회. GL_REPEAT wrap 포함.
		SJH::Texture *EnsureWallTexture(SJH::ResourceRegistry &reg)
		{
			if (auto *existing = reg.FindTexture(kWallTexKey))
				return existing;
			auto *tex = reg.CreateTexture(kWallTexKey, SJH::Image::Load(kWallTexKey, kWallTexPath).get());
			// uvScale 타일링이 1 을 넘어도 끝에서 고착되지 않고 반복되도록 REPEAT.
			tex->Bind();
			tex->SetWrap(SJH::WrapMode::Repeat, SJH::WrapMode::Repeat);
			return tex;
		}

		/// @brief 반투명 벽 공유 Material 을 idempotent 하게 등록/조회.
		/// @details Transparent Pass(blend on, depthWrite off, cull off 양면) + emissive(unit 0) PoliceTape.
		///          uvScale 은 BuildStage 가 arena/wallH 로 설정(전 벽 공유), uScrollSpeed 는 여기 기본값.
		SJH::Material *EnsureWallMaterial(SJH::ResourceRegistry &reg)
		{
			if (auto *existing = reg.FindSharedMaterial(kWallMatKey))
				return existing;
			auto *mat = reg.CreateSharedMaterial(kWallMatKey);
			mat->SetProgram(EnsureTransparentProgram(reg));
			mat->SetPass(SJH::Pass::RenderQueue::Transparent);
			SJH::Uniforms::SetTexture(*mat, "emissive", EnsureWallTexture(reg), 0);
			SJH::Uniforms::SetVec4(*mat, "tintColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
			SJH::Uniforms::SetVec2(*mat, "uvScale", glm::vec2(1.0f, 1.0f)); // placeholder - BuildStage 재설정.
			SJH::Uniforms::SetFloat(*mat, "uScrollSpeed", 0.3f);
			return mat;
		}

		/// @brief 물리 아레나 스테이지(벽 4개 + StageState + Orbit VFX)를 조립해 @c Root() 에 추가.
		/// @param world 물리 벽 바디 생성용 b2World (비소유).
		/// @details 구 @c StageBuilder::CreateStageActor 와 동작 동일 - 값(arena/wallH/startStatus)은
		///          @c Stage::Constants 기본값(구 StageConfig 기본). 렌더 무회귀(골든 bit-동일) 대상.
		void BuildStage(b2World &world)
		{
			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();

			// 1) 공유 자원 등록 - idempotent.
			SJH::Mesh *plane = EnsurePlane(reg);
			SJH::Material *wallMat = EnsureWallMaterial(reg); // Transparent + PoliceTape(emissive)

			// 2) Stage Actor + StageState Component (기본 시작 상태 = Title).
			auto stage = std::make_unique<SJH::Scene::Actor>("MainStage");
			auto *stageState = stage->AddComponent<Stage::Components::StageState>();
			stageState->SetCurrent(Stage::EStageStatus::Title);

			// 3) 벽 4개 - arena 안쪽 둘레. PoliceTape 반투명 띠로 시각화.
			const float arena = Stage::ARENA_HALF_EXTENT;
			const float wallH = Stage::WALL_THICKNESS;

			// 벽 4개 모두 동일 타일링 -> 공유 wallMat 직접 사용 (인스턴스 clone 불필요).
			const float tile = wallH * 2.0f;
			SJH::Uniforms::SetVec2(*wallMat, "uvScale", glm::vec2(arena * 2.0f / tile, wallH * 2.0f / tile));
			// uTime 은 공유 wallMat 에 한 번만 구동 (MaterialTime 은 값 세팅이라 하나로 충분).
			stage->AddComponent<Stage::Components::MaterialTime>(wallMat);

			auto spawnWall = [&](const char *name, glm::vec2 center, float yRot) {
				const bool horizontal = (static_cast<int>(yRot) % 180) == 0;
				const glm::vec2 half = horizontal ? glm::vec2(arena, wallH) : glm::vec2(wallH, arena);

				auto actor = Stage::Factories::CreateWallActor(name, world, center, half);
				auto &tr = actor->GetTransform();
				tr.EulerRot = glm::vec3(0.0f, yRot, 0.0f);
				tr.Scale = glm::vec3(arena * 2.0f, 1.0f, 1.0f);

				actor->AddComponent<SJH::Scene::MeshRenderer>(plane, wallMat);
				stage->AddChild(std::move(actor));
			};
			spawnWall("WallTop", glm::vec2(0.0f, +arena), 180.0f);
			spawnWall("WallBottom", glm::vec2(0.0f, -arena), 0.0f);
			spawnWall("WallLeft", glm::vec2(-arena, 0.0f), 270.0f);
			spawnWall("WallRight", glm::vec2(+arena, 0.0f), 90.0f);

			// 4) Orbit 배경 VFX - 아레나 중심(0,0,0)에 orbital_background.efk 상시 루프.
			//    Effect 미등록(Warmup 전/실패)이면 no-op. isLoop 재-Play 로 무한 지속.
			if (SJH::Effect *orbitEffect = reg.FindEffect("orbital_background"))
			{
				auto  orbitActor = std::make_unique<SJH::Scene::Actor>("OrbitVfx");
				auto *pl         = orbitActor->AddComponent<VFX::EffekseerPlayable>(
				    GameSystems::Get().VFX().GetManager(), orbitEffect, glm::vec3(0.0f), VFX::TrackPolicy::Static);
				pl->SetIsLoop(true);
				pl->Play();
				stage->AddChild(std::move(orbitActor));
			}

			dir.Root().AddChild(std::move(stage));
		}
	} // namespace

	WorldSceneResult BuildWorldScene(const WorldSceneDeps &deps)
	{
		WorldSceneResult result;
		result.WorldCamera = BuildWorldCamera(deps);
		BuildLighting();

		auto *skyboxRenderer  = BuildSkybox();
		result.SkyboxRenderer = skyboxRenderer;
		result.SkyboxMat      = skyboxRenderer ? skyboxRenderer->Material : nullptr;

		BuildPcbModel(); // PCB 장식 모델 (물리 무관 - StageBuilder 에서 이관).

		if (deps.physicsWorld)
			BuildStage(*deps.physicsWorld); // 물리 아레나(벽4+StageState+Orbit VFX) - 구 StageBuilder 흡수.

		return result;
	}
}
