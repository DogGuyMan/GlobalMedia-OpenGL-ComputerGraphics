#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_WORLD_SCENE_BUILDER_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_WORLD_SCENE_BUILDER_H__

// fwd — 포인터/참조만 노출 (헤더 의존 격리).
namespace SJH
{
	class MouseInput;
	class Framebuffer;
	class Material;
}
namespace SJH::Scene
{
	class Camera;
}

namespace TopdownShooter::Bootstrap
{
	/// @brief BuildWorldScene 입력 의존 — 비싱글턴만 (reg/dir 은 ::Get() 으로 내부 조회).
	struct WorldSceneDeps
	{
		float             aspect  = 0.0f;   ///< main 이 GetFramebufferInfo 로 계산해 주입 (GLFW 의존 격리).
		SJH::MouseInput  *mouse   = nullptr; ///< World camera ActorFolower 주입용.
		SJH::Framebuffer *sceneFB = nullptr; ///< World camera SetTargetRenderTarget 대상.
	};

	/// @brief main 이 멤버로 보유할 산출 포인터.
	struct WorldSceneResult
	{
		SJH::Scene::Camera *WorldCamera = nullptr; ///< main → mCamera.
		SJH::Material      *SkyboxMat   = nullptr; ///< main → mSkyboxMat (render() 가 매 프레임 u_time 갱신).
	};

	/// @brief 3D 월드 씬 구성 — WorldCamera(+ActorFolower) / DirLight / Matrix Skybox 를
	///        Director::Root 에 등록. 기존 main.cpp 의 CreateAndRegisterWorldCamera +
	///        WarmupLighting + WarmupSkybox 행위와 동일 (Pure factory — caller wiring 불필요,
	///        세 Actor 모두 내부에서 Root().AddChild).
	WorldSceneResult BuildWorldScene(const WorldSceneDeps &deps);
}

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_WORLD_SCENE_BUILDER_H__
