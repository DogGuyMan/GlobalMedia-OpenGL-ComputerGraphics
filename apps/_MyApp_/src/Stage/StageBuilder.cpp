/**
 * @file StageBuilder.cpp
 * @brief @c CreateStageActor 구현 - 공유 자원 등록(idempotent) + Stage Actor 조립.
 *
 * @details
 *  ### 조립 순서
 *  1. 익명 네임스페이스 내 @c Ensure* 헬퍼로 공유 자원(Plane/Program/Texture/Material/Model)을
 *     @c ResourceRegistry 에 idempotent 등록.
 *  2. Stage @c Actor + @c StageState @c Component 생성.
 *  3. 벽 4개(상하좌우) - lambda @c spawnWall 로 @c wall_factory + @c MeshRenderer + @c MaterialTime 조합.
 *  4. PCB 모델 @c Actor 생성 - @c ModelSpawner::SpawnEntities 로 RenderUnit 자식 펼침.
 *  5. Orbit 배경 VFX - @c EffekseerPlayable loop 설정 후 Play.
 *
 *  ### 함정 / 계약
 *  - @c gl3w.h 는 반드시 최상단 include - EffekseerRendererGL(시스템 gl3.h) 와 충돌 회피.
 *  - 벽 4개는 공유 Material(@c wallMat, "stage_wall") 을 *직접* 사용 (4벽 uvScale 동일이라 인스턴스 불필요).
 *    공유 Material 수정 시 전 벽에 반영 - PassDebugLayer 가 이 성질로 벽 Pass 를 일괄 토글.
 *  - Orbit VFX 는 @c ResourceRegistry 에 @c "orbital_background" Effect 등록 여부에만 의존 - 없으면 no-op.
 */
#include <GL/gl3w.h> // 반드시 최상단 - GameSystems.h->VFXSystem.h->EffekseerRendererGL(시스템 gl3.h) <-> resource_registry.h->gl3w.h 충돌 회피.

#include "Stage/StageBuilder.h"
#include "Stage/Components/MaterialTimeComponent.h"
#include "Stage/Components/StageStateComponent.h"
#include "Stage/Factories/wall_factory.h"

#include "GameSystems.h"                // GameSystems::Get().VFX() (Orbit 배경 VFX)
#include "VFX/EffekseerPlayable.h"      // Orbit EffekseerPlayable (Static + loop)
#include "material/material.h"
#include "material/material_uniforms.h"
#include "object/mesh.h"
#include "program/program.h"
#include "render/mesh_renderer.h"
#include "texture/image.h"
#include "resource_registry/resource_registry.h"
#include "texture/texture.h"
#include "scene/actor.h"
#include "Bootstrap/model_spawner.h"

#include <assimp/defs.h>
#include <box2d/box2d.h>
#include <cassert>
#include <string>
#include <glm/glm.hpp>

namespace TopdownShooter::Stage
{
	/// @cond INTERNAL
	namespace
	{
		// -- ResourceRegistry key 상수 - "stage_" prefix 로 Stage 도메인 영역 명시 --

		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		constexpr const char *kPlaneKey = "stage_plane";            ///< Plane mesh 등록 key (공유, 벽 시각화용).
		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		constexpr const char *kWallMatKey = "stage_wall";           ///< 반투명 벽 공유 Material key.

		// 반투명(Transparent) 벽 - PoliceTape 를 emissive 로 출력하는 unlit 셰이더.
		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		constexpr const char *kTransparentProgKey = "stage_transparent";  ///< 반투명 벽 Program key.
		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		constexpr const char *kTransparentVS = "resources/shaders/transparent.vs"; ///< 반투명 벽 VS 경로.
		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		constexpr const char *kTransparentFS = "resources/shaders/transparent.fs"; ///< 반투명 벽 FS 경로.
		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		constexpr const char *kWallTexKey = "stage_police_tape";    ///< PoliceTape 텍스처 key.
		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		// constexpr const char *kWallTexPath = "resources/texture/PoliceTape.png"; ///< PoliceTape 텍스처 경로.
		constexpr const char *kWallTexPath = "resources/texture/bwgradation1216.png"; ///< PoliceTape 텍스처 경로.

		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		constexpr const char *kPcbKey = "stage_pcb";                ///< PCB 모델 key.
		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		constexpr const char *kPcbModelPath = "resources/model/pcb.fbx"; ///< PCB 모델 경로.
		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		constexpr const char *kPhongAlbedoProgKey = "stage_phong_albedo"; ///< PCB 용 Phong+알베도 Program key.
		// Phase 2.5 (S7) - slang phong UBO 셰이더로 전환 (구 phong_tex.vs / phong_albedo.fs loose 판 대체).
		//   phong.slang -> phong.{vs,fs} (LightBlock UBO + MaterialBlock.baseColor.rgb=albedo). 값은 UBO 경로(loose 없음).
		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		constexpr const char *kPhongAlbedoVS = "resources/shaders/phong.vs"; ///< slang phong VS (LightBlock UBO).
		// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
		constexpr const char *kPhongAlbedoFS = "resources/shaders/phong.fs"; ///< slang phong FS (albedo 기반).

		/// @brief PCB 모델용 Phong 알베도 Program 을 idempotent 하게 등록/조회.
		/// @param reg 자원 레지스트리.
		/// @return 등록된(또는 기존) @c SJH::Program 포인터.
		SJH::Program *EnsurePhongAlbedoProgram(SJH::ResourceRegistry &reg)
		{
			if (auto *existing = reg.FindProgram(kPhongAlbedoProgKey))
				return existing;
			return reg.CreateProgram(kPhongAlbedoProgKey, kPhongAlbedoVS, kPhongAlbedoFS);
		}

		/// @brief 벽 시각화용 Plane mesh 를 idempotent 하게 등록/조회.
		/// @param reg 자원 레지스트리.
		/// @return 등록된(또는 기존) @c SJH::Mesh 포인터.
		SJH::Mesh *EnsurePlane(SJH::ResourceRegistry &reg)
		{
			if (auto *existing = reg.FindMesh(kPlaneKey))
				return existing;
			auto planeUPtr = SJH::Mesh::CreatePlane();
			return reg.RegisterMesh(kPlaneKey, std::move(planeUPtr));
		}

		/// @brief 반투명 벽 unlit Program 을 idempotent 하게 등록/조회.
		/// @param reg 자원 레지스트리.
		/// @return 등록된(또는 기존) @c SJH::Program 포인터.
		SJH::Program *EnsureTransparentProgram(SJH::ResourceRegistry &reg)
		{
			if (auto *existing = reg.FindProgram(kTransparentProgKey))
				return existing;
			return reg.CreateProgram(kTransparentProgKey, kTransparentVS, kTransparentFS);
		}

		/// @brief PoliceTape 텍스처를 idempotent 하게 등록/조회. GL_REPEAT wrap 설정 포함.
		/// @param reg 자원 레지스트리.
		/// @return 등록된(또는 기존) @c SJH::Texture 포인터.
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
		/// @details
		///   Transparent Pass(blend on, depthWrite off, cull off 양면) + emissive sampler(unit 0) 에
		///   PoliceTape 바인딩. uvScale 은 CreateStageActor 가 arena/wallH 로 설정(전 벽 공유), uScrollSpeed 는 여기 기본값.
		/// @param reg 자원 레지스트리.
		/// @return 공유 @c SJH::Material 포인터.
		SJH::Material *EnsureWallMaterial(SJH::ResourceRegistry &reg)
		{
			if (auto *existing = reg.FindSharedMaterial(kWallMatKey))
				return existing;
			auto *mat = reg.CreateSharedMaterial(kWallMatKey);
			mat->SetProgram(EnsureTransparentProgram(reg));
			// Transparent Pass - blend on, depthWrite off, cull off(양면). pass.h 가 자동 도출.
			mat->SetPass(SJH::Pass::RenderQueue::Transparent);
			// emissive sampler(unit 0) 에 PoliceTape 텍스처 - 라이팅 무관 자체발광.
			SJH::Uniforms::SetTexture(*mat, "emissive", EnsureWallTexture(reg), 0);
			SJH::Uniforms::SetVec4(*mat, "tintColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
			// 기본 타일링(placeholder) - 실제 값은 CreateStageActor 가 arena/wallH 로 설정(전 벽 공유).
			SJH::Uniforms::SetVec2(*mat, "uvScale", glm::vec2(1.0f, 1.0f));
			// U 방향 시간 스크롤 속도(tile/sec) - VS 가 uTime 과 곱해 PoliceTape 가 흐름.
			SJH::Uniforms::SetFloat(*mat, "uScrollSpeed", 0.3f);
			return mat;
		}

		/// @brief PCB 3D 모델을 idempotent 하게 등록/조회.
		/// @param reg 자원 레지스트리.
		/// @return 등록된(또는 기존) @c SJH::Model 포인터.
		SJH::Model *EnsurePcbModel(SJH::ResourceRegistry &reg)
		{
			if (auto *existing = reg.FindModel(kPcbKey))
				return existing;
			return reg.CreateModel(kPcbKey, kPcbModelPath);
		}
	} // namespace
	/// @endcond

	std::unique_ptr<SJH::Scene::Actor> CreateStageActor(const StageConfig &cfg)
	{
		assert(cfg.world != nullptr && "StageConfig::world 가 nullptr - PhysicsSystem 미초기화 상태에서 호출됨");
		assert(cfg.registry != nullptr && "StageConfig::registry 가 nullptr");

		auto &reg = *cfg.registry;

		// 1) 공유 자원 등록 - idempotent (이미 있으면 Find 로 재사용)
		SJH::Mesh *plane = EnsurePlane(reg);
		SJH::Material *wallMat = EnsureWallMaterial(reg); // Transparent + PoliceTape(emissive)

		// PCB 모델 자원 등록 및 캐싱
		SJH::Model *pcbModel = EnsurePcbModel(reg);

		// PCB 머티리얼에 slang phong UBO 셰이더 주입 (Phase 2.5 S7).
		//   model.cpp 가 저장한 material.albedo(Vec3) 를 MaterialBlock.baseColor(Vec4) 로 승격 -
		//   phong.slang 이 baseColor.rgb 를 albedo 로 사용 (mesh_pass useUbo 분기가 UpdateUniformBlock).
		SJH::Program *pcbProg = EnsurePhongAlbedoProgram(reg);
		for (int i = 0; i < pcbModel->GetMaterialCount(); ++i)
		{
			if (SJH::Material *mat = pcbModel->GetMaterial(i))
			{
				if (mat->GetProgram() == nullptr)
					mat->SetProgram(pcbProg);
				// albedo(Vec3) -> baseColor(Vec4) 승격 (idempotent). UBO MaterialBlock 의 입력.
				const auto albedoIt = mat->Properties.Vec3s.find("material.albedo");
				const glm::vec3 albedo = (albedoIt != mat->Properties.Vec3s.end())
				                               ? albedoIt->second
				                               : glm::vec3(0.8f, 0.8f, 0.8f);
				mat->Properties.Vec4s["baseColor"] = glm::vec4(albedo[0], albedo[1], albedo[2], 1.0f);
			}
		}

		// 2) Stage Actor + StageState Component
		auto stage = std::make_unique<SJH::Scene::Actor>("MainStage");
		auto *stageState = stage->AddComponent<Components::StageState>();
		stageState->SetCurrent(cfg.startStatus);

		// 3) 벽 4개 - arena 안쪽 둘레. PoliceTape 를 깐 반투명 바닥 띠로 시각화.
		const float arena = cfg.arenaHalfExtent;
		const float wallH = cfg.wallThickness;
		// 벽 하나를 생성/배치하는 팩토리 - (name, center, yRot) 만으로 통합.
		//   half(물리 박스)는 yRot 에서 자동 도출: 0/180 -> 가로(arena,wallH), 90/270 -> 세로(wallH,arena).
		//   시각 quad 는 항상 (arena*2, 1, 1) 에 yRot 만큼 Y축 회전 -> 세로 PoliceTape 펜스.

		// 벽 4개 모두 동일 타일링 -> 공유 wallMat 직접 사용 (인스턴스 clone 불필요).
		//   과거: 벽별 MaterialInstance 로 uvScale 독립 override 했으나 4벽 값이 동일해 중복이었음.
		//   공유로 전환 -> PassDebugLayer 가 shared SetPass 로 전 벽을 한 번에 토글 가능.
		const float tile = wallH * 2.0f;
		SJH::Uniforms::SetVec2(*wallMat, "uvScale", glm::vec2(arena * 2.0f / tile, wallH * 2.0f / tile));
		// uTime 은 공유 wallMat 에 한 번만 구동 (MaterialTime 은 값 세팅이라 하나로 충분 - 4중 부착 불필요).
		stage->AddComponent<Components::MaterialTime>(wallMat);

		auto spawnWall = [&](const char *name, glm::vec2 center, float yRot) {
			const bool horizontal = (static_cast<int>(yRot) % 180) == 0;
			const glm::vec2 half = horizontal ? glm::vec2(arena, wallH) : glm::vec2(wallH, arena);

			auto actor = Factories::CreateWallActor(name, *cfg.world, center, half);
			auto &tr = actor->GetTransform();
			tr.EulerRot = glm::vec3(0.0f, yRot, 0.0f);
			tr.Scale = glm::vec3(arena * 2.0f, 1.0f, 1.0f);

			// 시각 Material = 공유 wallMat 직접 주입 (인스턴스 없음). shared SetPass 가 전 벽에 반영.
			actor->AddComponent<SJH::Scene::MeshRenderer>(plane, wallMat);
			stage->AddChild(std::move(actor));
		};
		spawnWall("WallTop", glm::vec2(0.0f, +arena), 180.0f);
		spawnWall("WallBottom", glm::vec2(0.0f, -arena), 0.0f);
		spawnWall("WallLeft", glm::vec2(-arena, 0.0f), 270.0f);
		spawnWall("WallRight", glm::vec2(+arena, 0.0f), 90.0f);

		// 4) PCB 모델 Actor 추가
		auto pcbActor = std::make_unique<SJH::Scene::Actor>("PcbActor");
		pcbActor->GetTransform().SetTransformWithVectors(
		    glm::vec3(0.0, -1.75, 0.0),
		    glm::vec3(90.0, 0.0, 0),
		    glm::vec3(0.75, 0.75, 0.75));

		// ModelSpawner 유틸리티를 사용해 모델의 모든 RenderUnit을 자식 Actor로 펼침
		SJH::Scene::ModelSpawner::SpawnEntities(*pcbActor, *pcbModel);
		stage->AddChild(std::move(pcbActor));

		// 5) Orbit 배경 VFX - 아레나 중심(0,0,0)에 orbital_background.efk 상시 루프 (회전은 .efk 내장).
		//    Effect 미등록(startup Warmup 전/실패)이면 no-op. EffekseerPlayable 의 isLoop 재-Play 로 무한 지속.
		if (SJH::Effect *orbitEffect = reg.FindEffect("orbital_background"))
		{
			auto  orbitActor = std::make_unique<SJH::Scene::Actor>("OrbitVfx");
			auto *pl         = orbitActor->AddComponent<VFX::EffekseerPlayable>(
                GameSystems::Get().VFX().GetManager(), orbitEffect, glm::vec3(0.0f), VFX::TrackPolicy::Static);
			pl->SetIsLoop(true);
			pl->Play();
			stage->AddChild(std::move(orbitActor));
		}

		return stage;
	}
} // namespace TopdownShooter::Stage
