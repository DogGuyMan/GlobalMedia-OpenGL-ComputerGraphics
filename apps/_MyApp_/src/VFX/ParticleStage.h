#ifndef _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__
#define _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__

#include "render/render_stage.h"

namespace SJH::Scene { class Camera; }

namespace TopdownShooter::VFX
{
	class VFXSystem;

	/// @brief Effekseer 파티클을 WorldCamera 의 sceneFB 에 합성하는 렌더 stage.
	/// @details stages 컬렉션의 [worldCam, screenCam] 사이 거주.
	///          sceneFB 는 worldCam->GetTargetRenderTarget() 으로 매 프레임 동적 도출
	///          (진실의 원천 단일화 - architecture.md sec.11.5). resize 자동 추적.
	///          GL state 는 Effekseer 내부 BeginRendering/EndRendering 이 책임.
	/// @note    Client 거주 - Engine 코어 (SJH::render) 가 game_deps PUBLIC 합류 강제 회피.
	class ParticleStage : public SJH::IRenderStage
	{
	  public:
		/// @param vfx       VFXSystem (비소유). nullptr 시 Render 호출 무시.
		/// @param worldCam  Effekseer view/proj 출처 + sceneFB 출처 (비소유). nullptr 시 무시.
		ParticleStage(VFXSystem* vfx, SJH::Scene::Camera* worldCam);

		/// @brief sceneFB(=worldCam->GetTargetRenderTarget()) bind + Effekseer Draw.
		/// @note  target 인자는 사용하지 않음 - Camera 의 RT 사용 (CameraStage 와 동일 패턴).
		void Render(SJH::RenderTarget& target) override;

	  private:
		VFXSystem*           mVFX      = nullptr;
		SJH::Scene::Camera*  mWorldCam = nullptr;
	};
}

#endif // _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__
