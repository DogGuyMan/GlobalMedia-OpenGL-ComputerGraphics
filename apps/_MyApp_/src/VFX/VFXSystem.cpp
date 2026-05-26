#include "VFXSystem.h"

#include <Effekseer.h>
#include <EffekseerRendererGL.h>
#include <spdlog/spdlog.h>

#include <cstring>

namespace TopdownShooter::VFX
{
	void VFXSystem::Init(int maxSprites)
	{
		auto graphicsDevice = ::EffekseerRendererGL::CreateGraphicsDevice(
		    ::EffekseerRendererGL::OpenGLDeviceType::OpenGL3);
		mRenderer = ::EffekseerRendererGL::Renderer::Create(graphicsDevice, maxSprites);
		mManager  = ::Effekseer::Manager::Create(maxSprites);

		mManager->SetSpriteRenderer(mRenderer->CreateSpriteRenderer());
		mManager->SetRibbonRenderer(mRenderer->CreateRibbonRenderer());
		mManager->SetRingRenderer(mRenderer->CreateRingRenderer());
		mManager->SetTrackRenderer(mRenderer->CreateTrackRenderer());
		mManager->SetModelRenderer(mRenderer->CreateModelRenderer());

		mManager->SetTextureLoader(mRenderer->CreateTextureLoader());
		mManager->SetModelLoader(mRenderer->CreateModelLoader());
		mManager->SetMaterialLoader(mRenderer->CreateMaterialLoader());
		mManager->SetCurveLoader(::Effekseer::MakeRefPtr<::Effekseer::CurveLoader>());

		spdlog::info("[VFXSystem] init OK (max={})", maxSprites);
	}

	void VFXSystem::Update(float dt)
	{
		if (mManager.Get() == nullptr) return;
		// Effekseer 의 표준 deltaFrame 은 frame 단위 (60fps 기준). 초 dt 를 frame 으로 변환.
		mManager->Update(dt * 60.0f);
	}

	void VFXSystem::Draw(const float *viewMat, const float *projMat)
	{
		if (mManager.Get() == nullptr || mRenderer.Get() == nullptr) return;

		::Effekseer::Matrix44 view, proj;
		std::memcpy(view.Values, viewMat, sizeof(float) * 16);
		std::memcpy(proj.Values, projMat, sizeof(float) * 16);
		mRenderer->SetCameraMatrix(view);
		mRenderer->SetProjectionMatrix(proj);

		mRenderer->BeginRendering();
		mManager->Draw();
		mRenderer->EndRendering();
	}

	void VFXSystem::Shutdown()
	{
		mManager.Reset();
		mRenderer.Reset();
		spdlog::info("[VFXSystem] shutdown OK");
	}
}
