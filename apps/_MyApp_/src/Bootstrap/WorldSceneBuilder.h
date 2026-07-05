/**
 * @file WorldSceneBuilder.h
 * @brief 3D 월드 씬 기반 요소(WorldCamera/DirLight/Skybox)를 한 번에 조립하는 Pure Factory 진입점.
 *
 * @details
 *  ### 책임
 *  - Perspective WorldCamera Actor 생성 + @c ActorFolower 부착 + @c SceneFB 연결.
 *  - DirLight Actor 생성 + Ambient/Diffuse/Specular 설정.
 *  - Matrix Skybox Actor 생성 (프로그램/텍스처/머티리얼/메시 조립) + @c SkyboxMat 반환.
 *  - 세 Actor 모두 @c Director::Root().AddChild 까지 수행 (Pure factory - caller 추가 wiring 불필요).
 *
 *  ### 비-책임
 *  - [X] 플레이어/적/UI/파티클 조립 - @c PlayerBuilder / EnemyBuilder 등 담당.
 *  - [X] ResourceRegistry / Director 생성 - 내부에서 @c ::Get() 싱글톤 접근.
 *
 *  ### 정통 매핑
 *  - Cocos2D @c GameScene::createScene / Unreal @c UWorld::SpawnActor(Camera/Light/Sky)
 *    에 해당하는 월드 기반 액터 공장.
 *
 * @note @c BuildWorldScene 호출 전 @c Director::Get() 과 @c ResourceRegistry::Get() 이
 *       유효하게 초기화되어 있어야 한다 (main.cpp Warmup 이후 호출).
 *       @c WorldSceneDeps::sceneFB 가 nullptr 이면 WorldCamera 의 RenderTarget 이 없어
 *       @c SceneRenderer 가 warn+skip 한다 (@c scene_renderer.h @note 참조).
 */
#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_WORLD_SCENE_BUILDER_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_WORLD_SCENE_BUILDER_H__

// fwd - 포인터/참조만 노출 (헤더 의존 격리).
namespace SJH
{
	class MouseInput;
	class RenderTexture;
	class Material;
	class IRenderable;
}
namespace SJH::Scene
{
	class Camera;
}

namespace TopdownShooter::Bootstrap
{
	/**
	 * @brief @c BuildWorldScene 에 전달하는 외부 의존 묶음 - 비싱글턴 자원만 포함.
	 * @details @c ResourceRegistry / @c Director 등 싱글톤은 내부에서 @c ::Get() 으로 접근하므로
	 *          여기에 포함하지 않는다. @p aspect 는 GLFW 의존 격리를 위해 main 이 직접 계산해 주입.
	 */
	struct WorldSceneDeps
	{
		float             aspect  = 0.0f;   ///< RenderTexture 폭/높이 비율 (main 이 GetFramebufferInfo 로 계산, GLFW 의존 격리).
		SJH::MouseInput  *mouse   = nullptr; ///< WorldCamera @c ActorFolower 에 주입할 마우스 입력.
		SJH::RenderTexture *sceneFB = nullptr; ///< WorldCamera @c SetTargetRenderTarget 대상 씬 RenderTexture.
	};

	/**
	 * @brief @c BuildWorldScene 이 main 에게 반환하는 산출 포인터 묶음.
	 * @details main 이 멤버로 보유하여 @c PlayerBuilder 주입 및 매 프레임 @c u_time 갱신 등에 활용한다.
	 */
	struct WorldSceneResult
	{
		SJH::Scene::Camera *WorldCamera    = nullptr; ///< 생성된 WorldCamera 포인터 (main -> mCamera, PlayerBuilder deps 주입).
		SJH::Material      *SkyboxMat      = nullptr; ///< Matrix Skybox 머티리얼 (main -> mSkyboxMat, render() 에서 매 프레임 u_time 갱신).
		SJH::IRenderable   *SkyboxRenderer = nullptr; ///< Matrix Skybox MeshRenderer(IRenderable) - SkyboxPass 가 그릴 대상.
	};

	/**
	 * @brief 3D 월드 씬 기반 요소 전체를 조립해 @c Director::Root() 에 추가하고 산출물을 반환.
	 * @details 조립 순서:
	 *  1. @c BuildWorldCamera : Perspective Camera Actor + @c ActorFolower + sceneFB 연결.
	 *  2. @c BuildLighting : DirLight Actor (Ambient/Diffuse/Specular 설정).
	 *  3. @c BuildSkybox : Matrix Skybox 프로그램/텍스처/머티리얼/메시 조립 + Actor 등록.
	 *  세 Actor 모두 내부에서 @c Root().AddChild 하므로 caller 는 추가 wiring 이 불필요하다.
	 * @param deps 비싱글턴 외부 의존 (@c WorldSceneDeps 참조).
	 * @return 생성된 @c WorldSceneResult (WorldCamera + SkyboxMat).
	 */
	WorldSceneResult BuildWorldScene(const WorldSceneDeps &deps);
}

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_WORLD_SCENE_BUILDER_H__
