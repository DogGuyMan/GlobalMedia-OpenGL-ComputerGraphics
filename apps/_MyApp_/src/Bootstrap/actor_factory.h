/**
 * @file actor_factory.h
 * @brief render-결합 PreBuilt Actor 팩토리 - Skybox / ScreenCamera (Compound Actor 컨벤션).
 *
 * @details
 *  ### 거주지 (2026-06-11 E2 사이클 해소로 scene -> render 이주)
 *  @c CreateSkyboxActor 는 @c MeshRenderer(render) 조립, @c CreateScreenCameraActor 는
 *  @c RenderTexture(buffer) 인자를 받는다 - 둘 다 scene 보다 상위(render) 자원에 결합하므로
 *  render 모듈에 거주시켜 scene -> render 역의존을 끊었다. 네임스페이스는 @c SJH::Scene 유지(D7) -
 *  MeshRenderer 가 "render 파일 + Scene 네임스페이스" 인 기존 선례와 동일.
 *  (render 무관한 Camera/Light 팩토리는 @c scene/compound_actor.h 잔존.)
 *
 *  ### 공통 반환 계약
 *  반환된 @c unique_ptr 을 @c Director::Root().AddChild 에 넘겨야 씬 트리에 편입되고
 *  @c OnEnter 가 호출된다 (Pure factory - 씬 편입 책임 호출자).
 */

#ifndef __SJH_RENDER_ACTOR_FACTORY_H__
#define __SJH_RENDER_ACTOR_FACTORY_H__

#include <memory>
#include <string>

namespace SJH
{
	class RenderTexture;
	class Mesh;
	class Material;
}

namespace SJH::Scene
{
	class Actor;

	/// @brief PostFX 2-Camera 패턴의 Orthographic ScreenCamera Actor 생성.
	/// @details @c IsOrthographic=true, @c OrthoSize=1.0, @c NoClear=true,
	///          @c CullingMask(UI|Screen), @c SetTargetRenderTarget(sceneFB).
	///          내부에서 @c CreateCameraActor(compound_actor.h) 를 재사용.
	/// @param name     Actor 이름.
	/// @param aspect   화면 비율.
	/// @param sceneFB  WorldCamera 출력 FBO (비소유 - @c NoClear 합성 대상).
	/// @return 비편입 Actor @c unique_ptr - 호출자가 @c Director::Root().AddChild 책임.
	std::unique_ptr<Actor> CreateScreenCameraActor(
	    std::string name,
	    float aspect,
	    RenderTexture* sceneFB);

	/// @brief Skybox Actor 생성 - @c Mesh + 큰 @c scale + @c MeshRenderer.
	/// @details 카메라 추적은 *셰이더* 측(vert shader view matrix translation 제거)으로 자동 처리.
	/// @param name       Actor 이름.
	/// @param skyboxMesh 큐브맵 메시 (비소유).
	/// @param skyboxMat  스카이박스 머티리얼 (비소유).
	/// @param scale      스케일 (world unit). 카메라 far plane 보다 크게 설정 권장.
	/// @return 비편입 Actor @c unique_ptr.
	std::unique_ptr<Actor> CreateSkyboxActor(
	    std::string name,
	    Mesh* skyboxMesh,
	    Material* skyboxMat,
	    float scale = 50.0f);
} // namespace SJH::Scene

#endif // __SJH_RENDER_ACTOR_FACTORY_H__
