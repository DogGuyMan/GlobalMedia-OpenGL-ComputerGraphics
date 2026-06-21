/**
 * @file ParticleStage.cpp
 * @brief ParticleStage 구현 - Effekseer 파티클을 WorldCamera 의 sceneFB 에 합성하는 렌더 stage.
 *
 * @details
 *  ### 구현 흐름
 *  1. ctor - @c VFXSystem 포인터와 @c SJH::Scene::Camera 포인터를 비소유로 보관.
 *  2. @c Render - IRenderStage 인터페이스 구현.
 *     a. vfx/worldCam nullptr 가드 - nullptr 이면 warn + skip.
 *     b. worldCam->GetTargetRenderTarget() 으로 sceneFB 도출 (진실의 원천 단일화).
 *     c. DeviceContext::BindTarget(*rt) 로 sceneFB bind. NoClear - WorldCamera 가 이미 그린 결과 보존.
 *     d. VFXSystem::Draw(view, proj) 로 Effekseer BeginRendering/Draw/EndRendering 실행.
 *
 *  ### 렌더 순서 보장
 *  stages 컬렉션에서 [SceneRenderer] -> [ParticleStage] -> [ScreenQuadStage] 순서.
 *  SceneRenderer 가 먼저 씬을 sceneFB 에 그리고, ParticleStage 가 그 위에 파티클을 얹는다.
 *  반투명 파티클이 불투명 오브젝트보다 위에 올라오게 되는 정석 순서.
 *
 *  ### 비-책임
 *  - [X] GL state 관리(blend/depth) - Effekseer BeginRendering/EndRendering 내부 책임.
 *  - [X] sceneFB 소유/생성 - worldCam 이 소유. RT 포인터만 빌림.
 *  - [X] VFXSystem::Update(dt) - main update 루프 (Director) 가 담당.
 *
 *  ### gl3w include 순서 주의
 *  @c GL/gl3w.h 를 Effekseer 헤더보다 *반드시* 먼저 포함해야 GL 타입이 먼저 정의된다.
 *  (Effekseer 헤더가 GL 타입을 직접 참조하므로, 역순이면 undefined symbol 링크 에러.)
 */
#include <GL/gl3w.h>  // gl3w 반드시 최우선 - Effekseer 헤더보다 먼저 GL 타입 정의

#include "VFX/ParticleStage.h"

#include "VFX/VFXSystem.h"
#include "render/device_context.h"
#include "buffer/render_target.h"
#include "scene/camera.h"

#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

namespace TopdownShooter::VFX
{
	/// @brief 생성자.
	/// @param vfx       VFXSystem 포인터 (비소유). nullptr 시 @c Render 무시.
	/// @param worldCam  view/proj + sceneFB 출처 Camera 포인터 (비소유). nullptr 시 무시.
	ParticleStage::ParticleStage(VFXSystem* vfx, SJH::Scene::Camera* worldCam)
	    : mVFX(vfx), mWorldCam(worldCam)
	{
	}

	/// @brief sceneFB 에 파티클 합성.
	/// @details
	///  @p target 인자는 사용하지 않는다 - worldCam->GetTargetRenderTarget() 이 sceneFB 의
	///  진실의 원천 (architecture.md sec.11.5). resize 자동 추적 구현.
	///  @n * VAO/EBO 오염 주의 - VFXSystem::Draw (Effekseer BeginRendering/Draw) 호출 후
	///  현재 바인딩된 VAO 의 EBO 가 변경될 수 있다. 이후 패스에서 EBO 재핀 필요.
	/// @param target IRenderStage 인터페이스 인자. 미사용 - Camera RT 를 직접 사용.
	void ParticleStage::Render(SJH::RenderTarget& /*target*/)
	{
		if (!mVFX || !mWorldCam)
		{
			spdlog::warn("ParticleStage::Render - vfx/worldCam nullptr - skip.");
			return;
		}

		// 진실의 원천 단일화 - worldCam 의 RT 가 sceneFB (D-1).
		auto* rt = mWorldCam->GetTargetRenderTarget();
		if (!rt)
		{
			spdlog::warn("ParticleStage::Render - worldCam.GetTargetRenderTarget() nullptr - skip.");
			return;
		}

		// sceneFB bind (NoClear - WorldCamera 가 이미 그린 결과 보존).
		SJH::DeviceContext::Get().BindTarget(*rt);

		// Effekseer 자체 GL state setup + Draw (D-2 - state 명시 set 하지 않음).
		const glm::mat4 view = mWorldCam->GetViewMatrix();
		const glm::mat4 proj = mWorldCam->GetProjectionMatrix();
		mVFX->Draw(&view[0][0], &proj[0][0]);

		// D-RS-2 - Effekseer 가 depth/blend/cull 등 GL state 를 DeviceContext 캐시 뒤에서 바꾼다.
		// foreign 경계에서 캐시를 무효화해 다음 consumer(ScreenQuadStage / 다음 프레임 BeginFrame)가
		// stale 캐시로 glEnable 을 skip 하는 것을 막는다 (VAO-EBO 재핀과 같은 결).
		SJH::DeviceContext::Get().InvalidateStateCache();
	}
}
