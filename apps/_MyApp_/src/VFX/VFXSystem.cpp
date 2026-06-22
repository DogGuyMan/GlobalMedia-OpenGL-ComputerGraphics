/**
 * @file VFXSystem.cpp
 * @brief VFXSystem 구현 - Effekseer Manager/Renderer 부트스트랩, 매 프레임 Update/Draw, Shutdown.
 *
 * @details
 *  ### 구현 흐름
 *  1. @c Init - GL 그래픽스 디바이스 생성 -> Renderer::Create -> Manager::Create 순서 고정 레시피.
 *     5종 SubRenderer(Sprite/Ribbon/Ring/Track/Model) + 4종 Loader(Texture/Model/Material/Curve) 주입.
 *     SubRenderer 를 하나라도 빠뜨리면 해당 파티클 종류가 *조용히* 렌더 안 됨 - 단발 muzzle 만 쓸 거라도 5종 다 주입 권장.
 *  2. @c Update - @c manager->Update(dt * 60.0f). * deltaFrame 단위 함정:
 *     Effekseer Update 인자는 초(sec)가 아니라 60fps 기준 프레임 수. 그냥 dt 를 넘기면 파티클 60배 슬로우.
 *  3. @c Draw - SJH/vmath 행렬 포인터를 @c Effekseer::Matrix44 에 memcpy 후
 *     SetCameraMatrix -> SetProjectionMatrix -> BeginRendering -> Draw -> EndRendering.
 *     씬 렌더 후 호출해야 반투명 파티클이 위에 얹힘 (@c ParticlePass 가 순서 보장).
 *  4. @c Shutdown - Manager 먼저 Reset, Renderer 나중 순서 엄수 (Ref 타입이라 명시 delete 불요).
 *
 *  ### 비-책임
 *  - [X] 개별 이펙트 재생 lifecycle - @c EffekseerPlayable 담당.
 *  - [X] .efkefc 에셋 로드/캐시 - @c SJH::ResourceRegistry / @c SJH::Effect 담당.
 *  - [X] 렌더 stage 통합(bind/clear) - @c ParticlePass 담당.
 */
#include "VFXSystem.h"

#include <Effekseer.h>
#include <EffekseerRendererGL.h>
#include <spdlog/spdlog.h>

#include <cstring>

namespace TopdownShooter::VFX
{
	/// @brief Effekseer 엔진 부트스트랩.
	/// @details
	///  고정 레시피 - GL 디바이스 -> Renderer -> Manager 순서. 역순이면 Manager 가 Renderer
	///  SubRenderer 참조를 얻지 못해 파티클 렌더 무음 실패.
	///  @n CurveLoader 만 렌더러와 무관한 @c Effekseer::MakeRefPtr 로 직접 생성.
	/// @param maxSprites 동시 스프라이트/인스턴스 풀 상한.
	void VFXSystem::Init(int maxSprites)
	{
		// 1단계 - GL 그래픽스 디바이스 생성 (macOS/Core 3.3+ = OpenGL3 타입 고정).
		auto graphicsDevice = ::EffekseerRendererGL::CreateGraphicsDevice(
		    ::EffekseerRendererGL::OpenGLDeviceType::OpenGL3);
		// 2단계 - 렌더러/매니저 생성. maxSprites 는 두 곳 모두 동일 값을 넣는다.
		mRenderer = ::EffekseerRendererGL::Renderer::Create(graphicsDevice, maxSprites);
		mManager  = ::Effekseer::Manager::Create(maxSprites);

		// 3단계 - 5종 SubRenderer 주입. 빠뜨리면 해당 종류의 파티클이 조용히 안 그려진다.
		mManager->SetSpriteRenderer(mRenderer->CreateSpriteRenderer());
		mManager->SetRibbonRenderer(mRenderer->CreateRibbonRenderer());
		mManager->SetRingRenderer(mRenderer->CreateRingRenderer());
		mManager->SetTrackRenderer(mRenderer->CreateTrackRenderer());
		mManager->SetModelRenderer(mRenderer->CreateModelRenderer());

		// 4단계 - 4종 Loader 주입. CurveLoader 만 렌더러 무관 MakeRefPtr 생성.
		mManager->SetTextureLoader(mRenderer->CreateTextureLoader());
		mManager->SetModelLoader(mRenderer->CreateModelLoader());
		mManager->SetMaterialLoader(mRenderer->CreateMaterialLoader());
		mManager->SetCurveLoader(::Effekseer::MakeRefPtr<::Effekseer::CurveLoader>());

		spdlog::info("[VFXSystem] init OK (max={})", maxSprites);
	}

	/// @brief 파티클 시뮬레이션 한 스텝 전진.
	/// @details
	///  * deltaFrame 단위 함정 - @c manager->Update 인자는 초(sec)가 아니라
	///  60fps 기준 프레임 수. @p dt (초) 를 그대로 넘기면 파티클이 60배 슬로우.
	///  반드시 @c dt * 60.0f 로 변환해서 전달.
	/// @param dt 직전 프레임 경과 시간(초).
	void VFXSystem::Update(float dt)
	{
		if (mManager.Get() == nullptr) return;
		// Effekseer 의 표준 deltaFrame 은 frame 단위 (60fps 기준). 초 dt 를 frame 으로 변환.
		mManager->Update(dt * 60.0f);
	}

	/// @brief 한 프레임 파티클 렌더 패스.
	/// @details
	///  SJH/vmath @c mat4 포인터(float[16])를 @c Effekseer::Matrix44::Values 에 memcpy.
	///  BeginRendering/Draw/EndRendering 트리플렛은 생략 불가 - 순서 고정.
	///  @n * VAO/EBO 오염 주의 - BeginRendering/Draw 가 현재 바인딩된 VAO 의
	///  EBO 를 덮어쓸 수 있다. 호출 후 다른 패스에서 EBO 를 재핀해야 함.
	/// @param viewMat  16 float view 행렬 포인터 (컬럼 메이저, RH 기준).
	/// @param projMat  16 float projection 행렬 포인터.
	void VFXSystem::Draw(const float *viewMat, const float *projMat)
	{
		if (mManager.Get() == nullptr || mRenderer.Get() == nullptr) return;

		// SJH 행렬(float[16]) -> Effekseer::Matrix44 (Values[4][4]) memcpy 변환.
		::Effekseer::Matrix44 view, proj;
		std::memcpy(view.Values, viewMat, sizeof(float) * 16);
		std::memcpy(proj.Values, projMat, sizeof(float) * 16);
		mRenderer->SetCameraMatrix(view);
		mRenderer->SetProjectionMatrix(proj);

		// BeginRendering - Draw - EndRendering 트리플렛 (순서 고정).
		mRenderer->BeginRendering();
		mManager->Draw();
		mRenderer->EndRendering();
	}

	/// @brief Effekseer Manager/Renderer 해제.
	/// @details
	///  Manager 를 *먼저* Reset 해야 내부 인스턴스/렌더러 참조가 풀린다.
	///  역순이면 Renderer 가 Manager 참조를 들고 있는 채로 해제되어 dangling 위험.
	///  @n Ref 타입이므로 @c Reset() 만으로 정리 - 명시 @c delete 불요.
	void VFXSystem::Shutdown()
	{
		mManager.Reset();    // 매니저 먼저 (인스턴스/렌더러 참조 해제)
		mRenderer.Reset();   // 렌더러 나중
		spdlog::info("[VFXSystem] shutdown OK");
	}
}
