#ifndef __SJH_RENDER_PIPELINE_H__
#define __SJH_RENDER_PIPELINE_H__

#include "buffer/framebuffer.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SJH
{
	class ResourceRegistry;
	class SceneRenderer;
	class ScreenQuadStage;
}
namespace SJH::Scene
{
	class Actor;
	class PassComponent;
}

namespace SJH::Render
{
	/// @brief 기본 RenderPipeline 셋업의 식별자/경로 묶음.
	struct DefaultPipelineConfig
	{
		std::string PassthroughKey    = "screen_passthrough";
		std::string PassthroughVS     = "./resources/shaders/passthrough.vs";
		std::string PassthroughFS     = "./resources/shaders/passthrough.fs";
		std::string ScreenQuadMeshKey = "mesh_screen_quad";
		std::string BypassMatKey      = "mat_bypass_passthrough";
	};

	/// @brief PostFX 사용 데모의 표준 setup.
	/// @details passthrough Program 등록 + ScreenQuad Mesh 등록 + bypassMaterial 등록
	///          + SceneRenderer 에 SetScreenQuadMesh/SetBypassMaterial 주입 + ScreenQuadStage 생성.
	/// @param sceneFB nullptr 금지 — caller 가 Framebuffer::Create 로 사전 생성한 RT 전달.
	/// @return ScreenQuadStage UPtr — caller 가 mStages.push_back 책임 (Pure factory).
	///         Program / Mesh / Material 등록 실패 시 spdlog::error + nullptr 반환.
	std::unique_ptr<ScreenQuadStage> SetupDefaultPipeline(
	    ResourceRegistry& reg,
	    SceneRenderer& sceneRenderer,
	    Framebuffer* sceneFB,
	    const DefaultPipelineConfig& cfg = {});

	/// @brief PostFX 한 단계의 셰이더 + 초기 uniform 값.
	struct PostFXStageConfig
	{
		std::string Name;
		std::string VertFile;
		std::string FragFile;
		std::unordered_map<std::string, float> InitFloats; // D-6 data-driven (gamma=1.0 등)
	};

	/// @brief PostFX 체인 빌드 결과.
	struct PostFXChainResult
	{
		std::vector<FramebufferUPtr>          Framebuffers;   // owner — caller 가 멤버 vector 로 보유
		std::vector<Scene::PassComponent*>    PassComponents; // 비소유 raw — Debug UI 참조용
	};

	/// @brief PostFX 체인 빌드 — 각 stage 의 Program/Material/FB 생성
	///        + PassActor(Layer::Screen) + AddComponent<PassComponent>(prev, fb, mat)
	///        + screenCamActor->AddChild.
	/// @details 첫 stage 의 InputFB = sceneFB, 이후 stages 는 prev 단계의 OutputFB.
	///          configs 의 각 element 의 InitFloats 가 Material::Properties::Floats 에 복사.
	PostFXChainResult BuildPostFXChain(
	    ResourceRegistry& reg,
	    Scene::Actor& screenCamActor,
	    const std::vector<PostFXStageConfig>& configs,
	    Framebuffer* sceneFB,
	    int fbWidth, int fbHeight);
}

#endif // __SJH_RENDER_PIPELINE_H__
