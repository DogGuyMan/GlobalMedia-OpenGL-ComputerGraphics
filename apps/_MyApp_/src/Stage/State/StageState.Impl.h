#ifndef __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__
#define __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__

#include "Stage/Stage.h" // EStageStatus
#include "Stage/State/StageFSMState.h"
#include "Stage/State/StageStateMachine.h"
#include "Stage/Components/GameContextComponent.h"
#include "UI/StateOverlayLayer.h"
#include "Audio/FmodStudioPlayable.h"           // BGM_STATE 파라미터 / pause / stop
#include "Entity/Components/LifeComponents.h"   // Player HP → FMOD "Health" 파라미터
#include "Tween/TweenPlayable.h"                // GameOver Health 0→1.0 ramp (tweeny 포함)
#include "Manager.h"          // TopdownShooter::Manager::Get()
#include <memory>             // std::unique_ptr / make_unique (GameOver Health tween)
#include "render/pass_component.h" // SJH::Scene::PassComponent::Enabled (Title blur 토글)
#include "scene/actor.h"
#include "scene/scene.h"      // SJH::Scene::Director
#include <cstdint>           // uint64_t (CombatPlay transit OR-composite)
#include <imgui.h>            // IsMouseClicked(frame-edge) / GetIO().WantCaptureMouse
#include <tweeny/easing.h>

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
				// [FMOD] Title 진입 = BGM 재생 시작 소유. Play(instance 생성) → BGM_STATE=0(Title 분위기).
				if (ctx->bgmPlayable)
				{
					ctx->bgmPlayable->Play();
					ctx->bgmPlayable->SetParameter("BGM_STATE", 0.0f);
				}
			}
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
			{
				if (ctx->overlay) ctx->overlay->Hide();
				// [FMOD] BGM_STATE=1 + Pause 복귀 시 resume (Title 최초 진입 시는 no-op).
				if (ctx->bgmPlayable)
				{
					ctx->bgmPlayable->SetParameter("BGM_STATE", 1.0f); // Combat 분위기
					ctx->bgmPlayable->SetPaused(false);                // Pause→Combat resume
				}
			}
			// WaveController 리셋 없음 — Pause Resume 시 상태 보존
		}
		void OnUpdate(SJH::Scene::Actor &root, float dt) override
		{
			// 일시정지는 좌상단 Pause 버튼(PauseButtonLayer→TogglePause)이 구동 — 여기선 입력 처리 없음.
			// 게임 로직 tick (render() 에서 이전 — Hybrid). 이 안 Director::Update →
			// WaveController 가 Player 사망 시 GameOver 로 전이(이제 deferred — StateMachine::ApplyPending).
			auto &mgr = TopdownShooter::Manager::Get();
			mgr.Update(dt);
			SJH::Scene::Director::Get().Update(dt);
			mgr.Physics().SyncToTransform(SJH::Scene::Director::Get().Root());

			// [FMOD] Player HP 비율 → global "Health" 파라미터 (BGM/믹서 자동화 입력).
			//   오디오 접근은 Manager 싱글톤이 아니라 ctx->audio 로 통일 (bgmPlayable 과 동일 경로).
			//   ※ 사망 프레임에 여기서 Health=0 을 써도 GameOver 전이는 *다음 Update* 의 ApplyPending 에서
			//     일어나 GameOverState::OnEnter 가 Health=1.0 을 세팅하고, 이후 CombatPlay.OnUpdate 는
			//     다시 호출되지 않으므로 1.0 이 덮이지 않는다(LPF 해제 유지). IsAlive 가드 불요.
			if (auto *ctx = GetCtx(root); ctx && ctx->audio && ctx->playerLife)
			{
				const int   maxHp = ctx->playerLife->GetMaxHp();
				const float ratio = maxHp > 0
				                        ? static_cast<float>(ctx->playerLife->GetHp()) / static_cast<float>(maxHp)
				                        : 0.0f;
				ctx->audio->SetGlobalParameter("Health", ratio);
			}
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
			{
				if (ctx->overlay) ctx->overlay->Show(ctx->pauseTex);
				// [FMOD] BGM 일시정지 (instance 보존 — resume 은 CombatPlay::OnEnter).
				if (ctx->bgmPlayable) ctx->bgmPlayable->SetPaused(true);
			}
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
			// [FMOD] BGM resume 은 CombatPlay::OnEnter 가 단일 담당 (여기선 안 함 — 중복 회피).
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
			{
				if (ctx->overlay) ctx->overlay->Show(ctx->gameOverTex);
				// [FMOD] GameOver 에서도 BGM 은 계속 재생 — Stop() 안 함(BGM_STATE 도 유지).
				//   HP=0 이라 Health=0(Low Pass Filter 최대) 상태 → 즉시 1.0 으로 꺾지 않고
				//   1초에 걸쳐 0→1.0 으로 *점진 증가*시켜 LPF 를 서서히 해제한다(TweenPlayable).
				//   OnUpdate 가 매 프레임 tick(FSM Update 는 ungated — GameOver 에서도 호출됨).
				if (auto *audio = ctx->audio)
				{
					auto tw = tweeny::from(0.0f)
							.to(1.0f)
							.during(1000)
							.via(tweeny::easing::quinticOut); // 1000ms = 1.0초 선형 ramp
					mHealthFade = std::make_unique<Tween::TweenPlayable<float>>(
					    std::move(tw), [audio](float v) { audio->SetGlobalParameter("Health", v); });
					mHealthFade->Play();
				}
			}
		}
		void OnUpdate(SJH::Scene::Actor &, float dt) override
		{
			// terminal — State 전이는 없으나, Health 0→1.0 ramp 를 끝날 때까지 tick (끝나면 1.0 고정).
			if (mHealthFade && !mHealthFade->IsFinished())
				mHealthFade->Update(dt);
		}
		void OnExit(SJH::Scene::Actor &) override {}

	  private:
		std::unique_ptr<Tween::TweenPlayable<float>> mHealthFade; // GameOver Health ramp (detached, 수동 tick)
	};
} // namespace TopdownShooter::Stage

#endif // __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__
