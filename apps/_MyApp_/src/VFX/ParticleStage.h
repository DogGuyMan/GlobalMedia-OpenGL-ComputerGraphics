#ifndef _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__
#define _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__

#include "render/render_passable/render_passable.h"

namespace SJH::Scene { class Camera; }

namespace TopdownShooter::VFX
{
	class VFXSystem;

	/// @brief Effekseer 파티클을 WorldCamera 의 sceneFB 에 합성하는 역할군 Pass (WorldPass 위에 얹힘).
	/// @details PassIterator 의 [WorldPass, ParticlePass, ...] 사이 거주.
	///          sceneFB 는 worldCam->GetTargetRenderTarget() 으로 매 프레임 동적 도출
	///          (진실의 원천 단일화 - architecture.md sec.11.5). resize 자동 추적.
	///          GL state 는 Effekseer 내부 BeginRendering/EndRendering 이 책임.
	/// @note    Client 거주 - Engine 코어 (SJH::render) 가 game_deps PUBLIC 합류 강제 회피.
	///          파일명 ParticleStage.{h,cpp} 는 Phase 5 에서 ParticlePass.{h,cpp} 로 git mv 예정(클래스만 선행 개명).
	class ParticlePass : public SJH::IPassable
	{
	  public:
		/// @param vfx       VFXSystem (비소유). nullptr 시 Draw 호출 무시.
		/// @param worldCam  Effekseer view/proj 출처 + sceneFB 출처 (비소유). nullptr 시 무시.
		ParticlePass(VFXSystem* vfx, SJH::Scene::Camera* worldCam);

		/// @brief sceneFB(=worldCam->GetTargetRenderTarget()) bind + Effekseer Draw.
		/// @note  rec/before 파라미터는 사용하지 않음 - Camera 의 RT 사용 (WorldPass/SkyboxPass 와 동일 패턴).
		void Draw(SJH::DeviceContext& rec, const SJH::Texture* before) override;
		/// @brief 이 Pass 의 출력 - 파티클은 worldCam sceneFB 에 직접 합성, 텍스처 결과 없음.
		const SJH::Texture* GetPassResult() const override { return nullptr; }

	  private:
		VFXSystem*           mVFX      = nullptr;
		SJH::Scene::Camera*  mWorldCam = nullptr;
	};
}

#endif // _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__
