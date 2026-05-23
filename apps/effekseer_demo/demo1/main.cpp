#include "sb7.h"

#include <Effekseer.h>
#include <EffekseerRendererGL.h>

#include "client.h"
#include "input/mouse_input.h"

#include "common/common.h"
#include "common/layer.h"
#include "scene/camera.h"
#include "scene/compound_actor.h"
#include "scene/scene.h"
#include <vmath.h>

#include <cstdint>
#include <cstring>

#ifdef __APPLE__
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#endif

// 수정: index swap 제거 (직접 memcpy 의미)
static Effekseer::Matrix44 ToEfkMat(const vmath::mat4 &m)
{
	Effekseer::Matrix44 r;
	for (int row = 0; row < 4; ++row)
		for (int col = 0; col < 4; ++col)
			r.Values[row][col] = m[row][col]; // index swap 제거
	return r;
}

// Effekseer 파티클 엔진 기본 시연.
// resources/ 는 POST_BUILD 에서 extern/Effekseer/Examples/Resources/ 로부터 복사.
// 실행: cd build_ninja/apps/effekseer_demo/efk_demo1 && ./efk_demo1
class EfkDemo1 : public sb7::application
{
  public:
	void init() override
	{
		sb7::application::init();
		info.majorVersion = 4;
		info.minorVersion = 1; // GLSL 410 정통 (memory: glsl_410_project_policy)

#ifdef __APPLE__
		char exePath[PATH_MAX] = {};
		uint32_t exeSize = static_cast<uint32_t>(sizeof(exePath));
		if (_NSGetExecutablePath(exePath, &exeSize) == 0)
		{
			char exePathCopy[PATH_MAX] = {};
			strncpy(exePathCopy, exePath, PATH_MAX - 1);
			chdir(dirname(exePathCopy));
		}
#endif
	}

	void startup() override
	{

		const float aspect = static_cast<float>(info.windowWidth) /
		                     static_cast<float>(info.windowHeight);
		auto camActor = SJH::Scene::CreateCameraActor("EfkCamera",
		                                              45.0f, aspect, 0.1f, 500.0f);
		camActor->SetLayer(SJH::LAYER_SCENE);
		camActor->GetTransform().Translate = vmath::vec3(0.0f, 2.5f, 8.0f);
		camActor->GetTransform().EulerRot = vmath::vec3(-20.0f, 0.0f, 0.0f);

		mSceneCam = camActor->GetComponent<SJH::Scene::Camera>();
		mSceneCameraActor = camActor.get();

		camActor->AddComponent<Controller::CameraController>()
		    ->SetKeyboardInput(&mKeyboard)
		    .SetMouseInput(&mMouse)
		    .SetCamera(mSceneCam)
		    .SetUp();

		auto &dir = SJH::Scene::Director::Get();
		dir.Root().AddChild(std::move(camActor));
		dir.SetActiveCamera(mSceneCam);
		dir.Enter();

		// Effekseer 매니저 생성 (최대 8000 인스턴스)
		mManager = ::Effekseer::Manager::Create(8000);

		// EffekseerRendererGL 그래픽스 디바이스 및 렌더러 생성
		auto graphicsDevice = ::EffekseerRendererGL::CreateGraphicsDevice(
		    ::EffekseerRendererGL::OpenGLDeviceType::OpenGL3);
		mRenderer = ::EffekseerRendererGL::Renderer::Create(graphicsDevice, 8000);

		// 서브 렌더러 모듈 등록 (Sprite / Ribbon / Ring / Track / Model)
		mManager->SetSpriteRenderer(mRenderer->CreateSpriteRenderer());
		mManager->SetRibbonRenderer(mRenderer->CreateRibbonRenderer());
		mManager->SetRingRenderer(mRenderer->CreateRingRenderer());
		mManager->SetTrackRenderer(mRenderer->CreateTrackRenderer());
		mManager->SetModelRenderer(mRenderer->CreateModelRenderer());

		// 리소스 로더 등록
		mManager->SetTextureLoader(mRenderer->CreateTextureLoader());
		mManager->SetModelLoader(mRenderer->CreateModelLoader());
		mManager->SetMaterialLoader(mRenderer->CreateMaterialLoader());
		mManager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());

		// 에펙트 에셋 로드 (char16_t 경로 — Effekseer EFK_CHAR 규약)
		mEffect = Effekseer::Effect::Create(mManager, u"resources/Laser01.efkefc");
		assert(mEffect.Get() != nullptr && "Laser01.efkefc 로드 실패 — resources/ 디렉토리 확인");
	}

	void render(double t) override
	{
		float dt = static_cast<float>(1.0 / 60.0);
		mKeyboard.PollHeld(window);
		SJH::Scene::Director::Get().Update(dt);

		mSceneCam->Aspect = static_cast<float>(info.windowWidth) /
		                    static_cast<float>(info.windowHeight);

		::Effekseer::Matrix44 efkProj;
		efkProj.PerspectiveFovRH(
		    SJH::Deg2Rad(mSceneCam->FovYDeg),
		    mSceneCam->Aspect,
		    mSceneCam->NearZ,
		    mSceneCam->FarZ);

		// View 는 SJH 에서 받아와 memcpy 변환
		auto efkView = ToEfkMat(mSceneCam->GetViewMatrix());
		// 120 프레임마다 에펙트 재생 & 직전 정지
		if (mFrame % 120 == 0)
			mHandle = mManager->Play(mEffect, 0.0f, 0.0f, 0.0f);
		if (mFrame % 120 == 119)
			mManager->StopEffect(mHandle);

		// 에펙트 위치 이동 (좌->우 슬라이딩)
		mManager->AddLocation(mHandle, ::Effekseer::Vector3D(0.2f, 0.0f, 0.0f));

		// 레이어 뷰어 위치 — LOD / 컬링 기준점
		::Effekseer::Manager::LayerParameter layer;
		layer.ViewerPosition = ::Effekseer::Vector3D(
		    mSceneCameraActor->GetTransform().Translate[0],
		    mSceneCameraActor->GetTransform().Translate[1],
		    mSceneCameraActor->GetTransform().Translate[2]);
		mManager->SetLayerParameter(0, layer);

		// 매니저 업데이트 (파티클 타임라인 전진)
		::Effekseer::Manager::UpdateParameter update;
		mManager->Update(update);

		glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Effekseer 렌더링 패스
		mRenderer->SetTime(static_cast<float>(mFrame) / 60.0f);
		mRenderer->SetProjectionMatrix(efkProj);
		mRenderer->SetCameraMatrix(efkView);

		mRenderer->BeginRendering();

		::Effekseer::Manager::DrawParameter draw;
		draw.ZNear = 0.0f;
		draw.ZFar = 1.0f;
		draw.ViewProjectionMatrix = mRenderer->GetCameraProjectionMatrix();
		mManager->Draw(draw);

		mRenderer->EndRendering();

		++mFrame;
	}

	void shutdown() override
	{
		SJH::Scene::Director::Get().Exit();
		mEffect.Reset();
		mRenderer.Reset();
		mManager.Reset();
	}

	void onKey(int key, int action) override
	{
		mKeyboard.Dispatch(key, action);
		spdlog::info("Transform {} {} {}",
		             mSceneCameraActor->GetTransform().Translate[0],
		             mSceneCameraActor->GetTransform().Translate[1],
		             mSceneCameraActor->GetTransform().Translate[2]);
	}

	void onMouseButton(int button, int action) override
	{
		double x = 0.0, y = 0.0;
		glfwGetCursorPos(window, &x, &y);
		mMouse.HandleButton(button, action, x, y);
	}

	void onMouseMove(int x, int y) override
	{
		mMouse.HandleMove(static_cast<double>(x), static_cast<double>(y));
	}

  private:
	SJH::Scene::Actor *mSceneCameraActor = nullptr;
	SJH::Scene::Camera *mSceneCam = nullptr;
	SJH::KeyboardInput<Controller::CameraController::Action> mKeyboard;
	SJH::MouseInput mMouse;

	::Effekseer::ManagerRef mManager;
	::EffekseerRendererGL::RendererRef mRenderer;
	::Effekseer::EffectRef mEffect;
	::Effekseer::Handle mHandle = 0;
	int32_t mFrame = 0;
};

DECLARE_MAIN(EfkDemo1)
