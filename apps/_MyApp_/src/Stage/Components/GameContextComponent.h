#ifndef __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__
#define __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__

#include "scene/actor.h"
#include "resource_registry/texture.h"

// 포인터만 보유 — 전방 선언으로 충분 (UI/imgui · WaveController 헤더 전이 의존 차단).
// 실제 멤버 역참조(overlay->Show / waveCtrl->WaveLevel)는 사용처(StageState.Impl.h)가 헤더 include.
namespace TopdownShooter::UI { class StateOverlayLayer; }
namespace TopdownShooter::Stage { class WaveController; }
namespace SJH::Scene { class PassComponent; }

namespace TopdownShooter::Stage::Components
{
	/// @brief Stage FSM State 가 참조하는 공유 컨텍스트 (Root Actor 부착).
	/// @details State 가 owner.FindChild("GameContext")->GetComponent 로 접근.
	///          Hybrid 범위 — overlay + 텍스처 3종 + WaveController(웨이브 레벨/생존 적 수 조회) 보유.
	///          Player 사망→GameOver 판정·전이는 WaveController 가 담당(여기서 안 함).
	///          God Object 분리(DDD)는 차기 리팩토링(spec D7).
	class GameContextComponent : public SJH::Scene::Component
	{
	  public:
		void OnEnter() override {}
		void OnExit() override {}
		void Update(float) override {}

		// 오버레이 (ImGuiLayerStack 이 소유 — 여기는 비소유 raw)
		UI::StateOverlayLayer *overlay     = nullptr;
		const SJH::Texture    *titleTex    = nullptr;
		const SJH::Texture    *pauseTex    = nullptr;
		const SJH::Texture    *gameOverTex = nullptr;

		// 웨이브 진행 조회(ctx->waveCtrl->WaveLevel()/AliveEnemyCount())
		// + Player 사망 감시·GameOver 전이 주체 (WaveController 가 구동)
		WaveController *waveCtrl = nullptr;

		// Title 동안 blur PostFX ON (TitleState OnEnter/OnExit 에서 Enabled 토글)
		SJH::Scene::PassComponent *blurPass = nullptr;
	};
} // namespace TopdownShooter::Stage::Components

#endif // __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__
