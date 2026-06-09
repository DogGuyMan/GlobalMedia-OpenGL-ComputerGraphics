/**
 * @file render_pipeline.h
 * @brief 렌더 파이프라인 구성 헬퍼 - PostFX 체인 빌드 + DefaultPipeline 셋업 유틸리티.
 *
 * @details
 *  ### 책임
 *  - @c SetupDefaultPipeline - passthrough 셰이더/Mesh/Material 을 ResourceRegistry 에 등록하고
 *    ScreenQuadStage 를 반환하는 *One-Shot 팩토리* (PostFX 없이 FBO -> backbuffer 합성).
 *  - @c BuildPostFXChain - 여러 PostFX 스테이지를 순서대로 체인 연결.
 *    각 스테이지의 OutputFB 가 다음 스테이지의 InputFB 로 연결되며,
 *    PassComponent 를 가진 PassActor 를 @c screenCamActor 의 자식으로 부착.
 *
 *  ### 비-책임
 *  - [X] 스테이지 실행/순서 제어 - Application 의 @c mStages 벡터 순회 책임.
 *  - [X] Framebuffer 소유 - 결과 @c PostFXChainResult::Framebuffers 는 *caller* 가 보관.
 *  - [X] RenderStage 의 추상 실행 - @ref IRenderStage / @ref RenderStage 참조.
 *
 * @note 두 함수 모두 *Pure Factory* - 내부 상태 없이 reg/sceneRenderer 에 부수효과만 위임.
 */
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
	/**
	 * @brief @c SetupDefaultPipeline 에 전달하는 리소스 식별자/경로 묶음.
	 * @details
	 *  기본값 그대로 사용하면 표준 passthrough 경로로 동작.
	 *  데모가 경로를 재정의해야 할 때 필드를 교체 후 전달.
	 */
	struct DefaultPipelineConfig
	{
		std::string PassthroughKey    = "screen_passthrough";   ///< ResourceRegistry Program 키.
		std::string PassthroughVS     = "./resources/shaders/passthrough.vs"; ///< passthrough 버텍스 셰이더 경로.
		std::string PassthroughFS     = "./resources/shaders/passthrough.fs"; ///< passthrough 프래그먼트 셰이더 경로.
		std::string ScreenQuadMeshKey = "mesh_screen_quad";     ///< ResourceRegistry Mesh 키.
		std::string BypassMatKey      = "mat_bypass_passthrough"; ///< ResourceRegistry Material 키.
	};

	/**
	 * @brief PostFX 사용 데모의 표준 파이프라인을 한 호출로 셋업.
	 * @details
	 *  수행 순서:
	 *  1. @c reg.CreateProgram(PassthroughKey, VS, FS) - passthrough 셰이더 등록.
	 *  2. @c reg.RegisterMesh(ScreenQuadMeshKey, Mesh::CreateScreenQuad()) - 화면 quad 등록.
	 *  3. @c ScreenQuadStage 생성 + @c SetSources({sceneFB}) 초기화.
	 *  4. @c reg.CreateSharedMaterial(BypassMatKey) + @c SetProgram - bypass Material 등록.
	 *  5. @c sceneRenderer.SetScreenQuadMesh / @c SetBypassMaterial 주입.
	 *
	 * @param reg           자원 등록 대상 ResourceRegistry.
	 * @param sceneRenderer 주입 대상 SceneRenderer.
	 * @param sceneFB       장면 렌더 FBO - nullptr 금지, caller 가 사전 생성해 전달.
	 * @param cfg           리소스 키/경로 묶음. 기본값 사용 권장.
	 * @return 생성된 @c ScreenQuadStage UPtr - caller 가 @c mStages.push_back 책임.
	 *         Program / Mesh / Material 등록 실패 시 spdlog::error + @c nullptr 반환.
	 */
	std::unique_ptr<ScreenQuadStage> SetupDefaultPipeline(
	    ResourceRegistry& reg,
	    SceneRenderer& sceneRenderer,
	    Framebuffer* sceneFB,
	    const DefaultPipelineConfig& cfg = {});

	/**
	 * @brief @c BuildPostFXChain 의 단일 PostFX 스테이지 설정.
	 * @details
	 *  @c Name 은 Program 키 겸 PassActor 이름으로 사용.
	 *  @c InitFloats 는 Material::Properties::Floats 에 복사 (D-6 data-driven, 예: @c gamma=1.0).
	 */
	struct PostFXStageConfig
	{
		std::string Name;     ///< 스테이지 이름 (Program 키 겸 PassActor 이름).
		std::string VertFile; ///< 버텍스 셰이더 경로.
		std::string FragFile; ///< 프래그먼트 셰이더 경로.
		std::unordered_map<std::string, float> InitFloats; ///< 초기 float uniform 값 맵 (예: @c {{"gamma", 1.0f}}).
	};

	/**
	 * @brief @c BuildPostFXChain 의 반환값 - FBO 소유 + PassComponent 비소유 핸들.
	 * @details
	 *  - @c Framebuffers - caller 가 멤버 변수로 보유해 생명주기 관리.
	 *  - @c PassComponents - Debug UI / 런타임 파라미터 갱신용 비소유 raw 포인터.
	 *    실제 소유는 @c screenCamActor 의 자식 PassActor.
	 */
	struct PostFXChainResult
	{
		std::vector<FramebufferUPtr>          Framebuffers;   ///< 각 스테이지 OutputFB 소유 (caller 보관 필수).
		std::vector<Scene::PassComponent*>    PassComponents; ///< Debug UI / 파라미터 접근용 비소유 포인터.
	};

	/**
	 * @brief PostFX 스테이지 체인을 빌드하고 결과 FBO/PassComponent 를 반환.
	 * @details
	 *  @c configs 순서대로 각 스테이지의 Program / Material / Framebuffer 를 생성하고,
	 *  PassComponent(@c prevFB -> @c fb)를 가진 PassActor 를 @c screenCamActor 의 자식으로 추가.
	 *  첫 스테이지의 InputFB = @p sceneFB, 이후 스테이지의 InputFB = 이전 스테이지의 OutputFB.
	 *
	 * @param reg             자원 등록 대상 ResourceRegistry.
	 * @param screenCamActor  PassActor 를 자식으로 받을 Screen 카메라 Actor (비소유, 수명 보장 필요).
	 * @param configs         각 PostFX 스테이지 설정 목록 (순서 = 체인 순서).
	 * @param sceneFB         체인 첫 스테이지의 입력 FBO (장면 렌더 결과).
	 * @param fbWidth         각 스테이지 OutputFB 가로 크기 (픽셀).
	 * @param fbHeight        각 스테이지 OutputFB 세로 크기 (픽셀).
	 * @return 생성된 FBO 소유 벡터 + PassComponent 비소유 벡터. 실패한 스테이지는 skip.
	 */
	PostFXChainResult BuildPostFXChain(
	    ResourceRegistry& reg,
	    Scene::Actor& screenCamActor,
	    const std::vector<PostFXStageConfig>& configs,
	    Framebuffer* sceneFB,
	    int fbWidth, int fbHeight);
}

#endif // __SJH_RENDER_PIPELINE_H__
