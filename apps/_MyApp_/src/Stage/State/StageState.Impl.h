#ifndef __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__
#define __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__

#include "Stage/Stage.h" // EStageStatus
#include "Stage/State/StageFSMState.h"
#include "Stage/State/StageStateMachine.h"
#include "Stage/Components/GameContextComponent.h"
#include "UI/StateOverlayLayer.h"
#include "Manager.h"          // TopdownShooter::Manager::Get()
#include "render/pass_component.h" // SJH::Scene::PassComponent::Enabled (Title blur 토글)
#include "scene/actor.h"
#include "scene/scene.h"      // SJH::Scene::Director
#include <cstdint>           // uint64_t (CombatPlay transit OR-composite)
#include <imgui.h>            // IsMouseClicked(frame-edge) / GetIO().WantCaptureMouse

namespace TopdownShooter::Stage
{
	/// @brief Root 에 부착된 GameContextComponent 조회 (GameContext = Root 직속 child).
	inline Components::GameContextComponent *GetCtx(SJH::Scene::Actor &root)
	{
		auto *ctxActor = root.FindChild("GameContext"); // 비재귀 — Root 직속
		return ctxActor ? ctxActor->GetComponent<Components::GameContextComponent>() : nullptr;
	}

	// ── TitleState ────────────────────────────────────────────────
	class TitleState : public BaseStageFsmState
	{
	  public:
		explicit TitleState(StageStateMachine *fsm)
		    : BaseStageFsmState(fsm, EStageStatus::Title, EStageStatus::CombatPlay)
		{
		}

		void OnEnter(SJH::Scene::Actor &root) override
		{
			if (auto *ctx = GetCtx(root))
			{
				if (ctx->overlay) ctx->overlay->Show(ctx->titleTex);
				if (ctx->blurPass) ctx->blurPass->Enabled = true; // Title 동안 blur ON
			}
			// [FMOD] BGM_STATE=0 — FMOD 에이전트 담당
		}
		void OnUpdate(SJH::Scene::Actor & /*root*/, float /*dt*/) override
		{
			// 위젯(Pause 버튼/드롭다운) 위 클릭 제외 — 빈 화면 클릭만 시작 (이중처리 방지).
			if ((ImGui::IsMouseClicked(0, false) || ImGui::IsMouseClicked(1, false)) &&
			    !ImGui::GetIO().WantCaptureMouse)
				mFsm->TryTransit(EStageStatus::CombatPlay);
		}
		void OnExit(SJH::Scene::Actor &root) override
		{
			if (auto *ctx = GetCtx(root))
			{
				if (ctx->overlay) ctx->overlay->Hide();
				if (ctx->blurPass) ctx->blurPass->Enabled = false; // Title 이탈 시 blur OFF
			}
		}
	};

	// ── CombatPlayState ───────────────────────────────────────────
	class CombatPlayState : public BaseStageFsmState
	{
	  public:
		explicit CombatPlayState(StageStateMachine *fsm)
		    : BaseStageFsmState(fsm, EStageStatus::CombatPlay,
		                        static_cast<EStageStatus>(
		                            static_cast<uint64_t>(EStageStatus::Pause) |
		                            static_cast<uint64_t>(EStageStatus::GameOver)))
		{
		}

		void OnEnter(SJH::Scene::Actor &root) override
		{
			if (auto *ctx = GetCtx(root))
				if (ctx->overlay) ctx->overlay->Hide();
			// [FMOD] BGM_STATE=1, resume — FMOD 에이전트 담당
			// WaveController 리셋 없음 — Pause Resume 시 상태 보존
		}
		void OnUpdate(SJH::Scene::Actor & /*root*/, float dt) override
		{
			// 일시정지는 좌상단 Pause 버튼(PauseButtonLayer→TogglePause)이 구동 — 여기선 입력 처리 없음.
			// 게임 로직 tick (render() 에서 이전 — Hybrid). 이 안 Director::Update →
			// WaveController 가 Player 사망 시 GameOver 로 중첩 전이(transit 안전성 검증 완료).
			auto &mgr = TopdownShooter::Manager::Get();
			mgr.Update(dt);
			SJH::Scene::Director::Get().Update(dt);
			mgr.Physics().SyncToTransform(SJH::Scene::Director::Get().Root());
			// [FMOD] Health 파라미터 — FMOD 에이전트 담당
		}
		void OnExit(SJH::Scene::Actor &) override {}
	};

	// ── PauseState ────────────────────────────────────────────────
	class PauseState : public BaseStageFsmState
	{
	  public:
		explicit PauseState(StageStateMachine *fsm)
		    : BaseStageFsmState(fsm, EStageStatus::Pause, EStageStatus::CombatPlay)
		{
		}

		void OnEnter(SJH::Scene::Actor &root) override
		{
			if (auto *ctx = GetCtx(root))
				if (ctx->overlay) ctx->overlay->Show(ctx->pauseTex);
			// [FMOD] BGM pause — FMOD 에이전트 담당
		}
		void OnUpdate(SJH::Scene::Actor & /*root*/, float /*dt*/) override
		{
			// 화면 아무 곳(위젯 제외) 클릭 → 재개. Pause 버튼 위 클릭은 버튼 토글이 재개 처리.
			if ((ImGui::IsMouseClicked(0, false) || ImGui::IsMouseClicked(1, false)) &&
			    !ImGui::GetIO().WantCaptureMouse)
				mFsm->TryTransit(EStageStatus::CombatPlay);
			// tick 없음 → 게임 freeze (D5)
		}
		void OnExit(SJH::Scene::Actor &root) override
		{
			if (auto *ctx = GetCtx(root))
				if (ctx->overlay) ctx->overlay->Hide();
			// [FMOD] BGM resume — FMOD 에이전트 담당
		}
	};

	// ── GameOverState ─────────────────────────────────────────────
	class GameOverState : public BaseStageFsmState
	{
	  public:
		explicit GameOverState(StageStateMachine *fsm)
		    : BaseStageFsmState(fsm, EStageStatus::GameOver, EStageStatus::NONE) // terminal (transit=0)
		{
		}

		void OnEnter(SJH::Scene::Actor &root) override
		{
			if (auto *ctx = GetCtx(root))
				if (ctx->overlay) ctx->overlay->Show(ctx->gameOverTex);
			// [FMOD] BGM stop — FMOD 에이전트 담당
		}
		void OnUpdate(SJH::Scene::Actor &, float) override {} // terminal — 전이 없음
		void OnExit(SJH::Scene::Actor &) override {}
	};
} // namespace TopdownShooter::Stage

#endif // __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__
