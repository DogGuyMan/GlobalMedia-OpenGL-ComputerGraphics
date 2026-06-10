/**
 * @file StageState.Impl.h
 * @brief 4개 Stage FSM 구상 State 전체 구현 - TitleState / CombatPlayState / PauseState / GameOverState.
 *
 * @details
 *  ### 책임
 *  - @c BaseStageFsmState 를 상속한 4개 구상 State 의 OnEnter / OnUpdate / OnExit 구현.
 *  - @c GetCtx() 헬퍼로 Root Actor 에서 @c GameContextComponent 를 빠르게 조회.
 *  - 각 State 의 FMOD 상호작용(BGM_STATE 파라미터, Pause/Resume, Health LPF ramp)을 캡슐화.
 *
 *  ### 비-책임
 *  - [X] State 등록 / 전이 트리거 판정 - @c StageStateMachine / @c SJH::FSM::StateMachine 담당.
 *  - [X] 게임 로직 tick (Physics SyncToTransform, WaveController) - CombatPlayState::OnUpdate 가
 *    위임 호출하나, 로직 자체는 @c Manager / @c Director 가 담당.
 *
 *  ### 상태 전이 요약
 *  | State       | 진입 조건                  | 전이 조건                                    |
 *  |-------------|----------------------------|----------------------------------------------|
 *  | TitleState  | startup (initial)          | 빈 화면 마우스 클릭 -> CombatPlay            |
 *  | CombatPlay  | Title 클릭 / Pause resume  | PauseButton 클릭 -> Pause / Player 사망 -> GameOver |
 *  | Pause       | PauseButton 클릭           | 빈 화면 마우스 클릭 -> CombatPlay            |
 *  | GameOver    | Player 사망                | terminal (전이 없음)                         |
 *
 * @note 이 헤더는 구현 포함 헤더 (Impl.h) - 단일 translation unit 에서만 include 할 것.
 *       여러 TU 에서 include 하면 함수 중복 정의가 발생한다.
 */
#ifndef __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__
#define __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__

#include "Stage/Stage.h" // EStageStatus
#include "Stage/State/StageFSMState.h"
#include "Stage/State/StageStateMachine.h"
#include "Stage/Components/GameContextComponent.h"
#include "UI/StateOverlayLayer.h"
#include "Audio/FmodStudioPlayable.h"           // BGM_STATE 파라미터 / pause / stop
#include "Audio/Constants.h"                    // BUS_BGM / BUS_SFX (Pause 볼륨 슬라이더)
#include "Entity/Components/LifeComponents.h"   // Player HP -> FMOD "Health" 파라미터
#include "Tween/TweenPlayable.h"                // GameOver Health 0->1.0 ramp (tweeny 포함)
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
	/// @brief Root 에 부착된 @c GameContextComponent 를 조회하는 헬퍼.
	/// @details Root 직속 "GameContext" 자식 Actor 에서 @c GameContextComponent 를 꺼낸다.
	///          FindChild 는 비재귀 검색 - Root 직속 자식만 탐색한다.
	/// @param root 씬 루트 Actor.
	/// @return GameContextComponent 포인터, 없으면 nullptr.
	inline Components::GameContextComponent *GetCtx(SJH::Scene::Actor &root)
	{
		auto *ctxActor = root.FindChild("GameContext"); // 비재귀 - Root 직속
		return ctxActor ? ctxActor->GetComponent<Components::GameContextComponent>() : nullptr;
	}

	// -- TitleState ------------------------------------------------
	/**
	 * @brief 타이틀 화면 State - startup initial, 빈 화면 클릭 시 CombatPlay 로 전이.
	 * @details
	 *  - OnEnter: overlay 표시 (titleTex), blur PostFX ON, BGM Play + BGM_STATE=0 (Title 분위기).
	 *  - OnUpdate: 빈 화면 마우스 클릭(ImGui 위젯 제외) -> @c TryTransit(CombatPlay).
	 *  - OnExit:  overlay Hide, blur PostFX OFF.
	 */
	class TitleState : public BaseStageFsmState
	{
	  public:
		/// @brief TitleState 생성자 - StateFlag=Title, TransitFlag=CombatPlay.
		/// @param fsm 부모 StageStateMachine (역참조, non-owning).
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
				// [FMOD] Title 진입 = BGM 재생 시작 소유. Play(instance 생성) -> BGM_STATE=0(Title 분위기).
				if (ctx->bgmPlayable)
				{
					ctx->bgmPlayable->Play();
					ctx->bgmPlayable->SetParameter("BGM_STATE", 0.0f);
				}
			}
		}
		void OnUpdate(SJH::Scene::Actor & /*root*/, float /*dt*/) override
		{
			// 위젯(Pause 버튼/드롭다운) 위 클릭 제외 - 빈 화면 클릭만 시작 (이중처리 방지).
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

	// -- CombatPlayState -------------------------------------------
	/**
	 * @brief 전투 진행 State - WaveController/Director::Update tick 을 담당하는 게임 루프 허브.
	 * @details
	 *  - OnEnter: overlay Hide, BGM_STATE=1 (Combat 분위기), Pause->Resume 시 BGM SetPaused(false).
	 *  - OnUpdate: Manager::Update + Director::Update + Physics SyncToTransform 순 tick.
	 *              Player HP 비율 -> FMOD global "Health" 파라미터 실시간 송신.
	 *              Pause 전이는 PauseButtonLayer 가 구동(여기서 처리 안 함).
	 *              WaveController 가 Player 사망 감지 시 ApplyPending 경로로 GameOver 전이.
	 *  - OnExit:   없음.
	 *
	 *  TransitFlag = Pause | GameOver (비트 OR 복합 전이).
	 */
	class CombatPlayState : public BaseStageFsmState
	{
	  public:
		/// @brief CombatPlayState 생성자 - StateFlag=CombatPlay, TransitFlag=Pause|GameOver.
		/// @param fsm 부모 StageStateMachine (역참조, non-owning).
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
					ctx->bgmPlayable->SetPaused(false);                // Pause->Combat resume
				}
			}
			// WaveController 리셋 없음 - Pause Resume 시 상태 보존
		}
		void OnUpdate(SJH::Scene::Actor &root, float dt) override
		{
			// 일시정지는 좌상단 Pause 버튼(PauseButtonLayer->TogglePause)이 구동 - 여기선 입력 처리 없음.
			// 게임 로직 tick (render() 에서 이전 - Hybrid). 이 안 Director::Update ->
			// WaveController 가 Player 사망 시 GameOver 로 전이(이제 deferred - StateMachine::ApplyPending).
			auto &mgr = TopdownShooter::Manager::Get();
			mgr.Update(dt);
			SJH::Scene::Director::Get().Update(dt);
			mgr.Physics().SyncToTransform(SJH::Scene::Director::Get().Root());

			// [FMOD] Player HP 비율 -> global "Health" 파라미터 (BGM/믹서 자동화 입력).
			//   오디오 접근은 Manager 싱글톤이 아니라 ctx->audio 로 통일 (bgmPlayable 과 동일 경로).
			//   * 사망 프레임에 여기서 Health=0 을 써도 GameOver 전이는 *다음 Update* 의 ApplyPending 에서
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

	// -- PauseState ------------------------------------------------
	/**
	 * @brief 일시정지 State - 게임 로직 tick 없음 (D5 freeze), 볼륨 슬라이더 UI 표시.
	 * @details
	 *  - OnEnter: overlay 표시 (pauseTex), blur PostFX ON, BGM SetPaused(true),
	 *             볼륨 슬라이더 초기값 = 현재 bus 볼륨으로 초기화, mShowVolumeUI=true.
	 *  - OnUpdate: ImGui 볼륨 슬라이더(BGM/SFX) + 빈 화면 클릭 -> TryTransit(CombatPlay).
	 *              게임 Update 미호출 -> 물리/애니 freeze.
	 *  - OnExit:  overlay Hide, blur PostFX OFF, mShowVolumeUI=false.
	 *             BGM resume 은 CombatPlayState::OnEnter 가 단일 담당 (중복 회피).
	 */
	class PauseState : public BaseStageFsmState
	{
	  public:
		/// @brief PauseState 생성자 - StateFlag=Pause, TransitFlag=CombatPlay.
		/// @param fsm 부모 StageStateMachine (역참조, non-owning).
		explicit PauseState(StageStateMachine *fsm)
		    : BaseStageFsmState(fsm, EStageStatus::Pause, EStageStatus::CombatPlay)
		{
		}

		void OnEnter(SJH::Scene::Actor &root) override
		{
			if (auto *ctx = GetCtx(root))
			{
				if (ctx->overlay) ctx->overlay->Show(ctx->pauseTex);
				// [FMOD] BGM 일시정지 (instance 보존 - resume 은 CombatPlay::OnEnter).
				if (ctx->bgmPlayable) ctx->bgmPlayable->SetPaused(true);
				if (ctx->blurPass) ctx->blurPass->Enabled = true; // Title 동안 blur ON
				// 볼륨 슬라이더 UI 활성 - 현재 bus 볼륨으로 슬라이더 초기화 (Pause 동안만 표시).
				if (ctx->audio)
				{
					mBgmVolume = ctx->audio->GetBusVolume(Audio::BUS_BGM);
					mSfxVolume = ctx->audio->GetBusVolume(Audio::BUS_SFX);
				}
				mShowVolumeUI = true;
			}
		}
		void OnUpdate(SJH::Scene::Actor &root, float /*dt*/) override
		{
			// 볼륨 슬라이더 (Enter->활성/Exit->비활성) - BGM/SFX bus 볼륨만 조절. 변경 시 즉시 setVolume.
			if (mShowVolumeUI)
				if (auto *ctx = GetCtx(root); ctx && ctx->audio)
				{
					ImGui::Begin("Volume");
					if (ImGui::SliderFloat("BGM", &mBgmVolume, 0.0f, 1.0f, "%.2f"))
						ctx->audio->SetBusVolume(Audio::BUS_BGM, mBgmVolume);
					if (ImGui::SliderFloat("SFX", &mSfxVolume, 0.0f, 1.0f, "%.2f"))
						ctx->audio->SetBusVolume(Audio::BUS_SFX, mSfxVolume);
					ImGui::End();
				}

			// 화면 아무 곳(위젯 제외) 클릭 -> 재개. Pause 버튼/슬라이더 위 클릭은 위젯이 처리(WantCaptureMouse).
			if ((ImGui::IsMouseClicked(0, false) || ImGui::IsMouseClicked(1, false)) &&
			    !ImGui::GetIO().WantCaptureMouse)
				mFsm->TryTransit(EStageStatus::CombatPlay);
			// tick 없음 -> 게임 freeze (D5)
		}
		void OnExit(SJH::Scene::Actor &root) override
		{
			if (auto *ctx = GetCtx(root)){
				if (ctx->overlay) ctx->overlay->Hide();
				if (ctx->blurPass) ctx->blurPass->Enabled = false; // Title 동안 blur ON
			}
			mShowVolumeUI = false; // 볼륨 슬라이더 UI 비활성 (Pause 이탈)
			// [FMOD] BGM resume 은 CombatPlay::OnEnter 가 단일 담당 (여기선 안 함 - 중복 회피).
		}

	  private:
		bool  mShowVolumeUI = false;   ///< Pause Enter -> true / Exit -> false (슬라이더 표시 게이트).
		float mBgmVolume     = 1.0f;   ///< BGM Bus 볼륨 슬라이더 값 (OnEnter 에서 실제 bus 볼륨으로 초기화).
		float mSfxVolume     = 1.0f;   ///< SFX Bus 볼륨 슬라이더 값.
	};

	// -- GameOverState ---------------------------------------------
	/**
	 * @brief 게임 오버 Terminal State - 전이 없음, Health LPF 점진 해제 연출.
	 * @details
	 *  - OnEnter: overlay 표시 (gameOverTex), TweenPlayable 을 생성해 FMOD global "Health"
	 *             파라미터를 0 -> 1.0 으로 1초 quinticOut 으로 ramp (LPF 서서히 해제).
	 *  - OnUpdate: mHealthFade tick - IsFinished 후 1.0 고정, State 전이 없음.
	 *  - OnExit:   없음 (terminal).
	 *
	 *  TransitFlag = NONE (transit=0) - terminal.
	 */
	class GameOverState : public BaseStageFsmState
	{
	  public:
		/// @brief GameOverState 생성자 - StateFlag=GameOver, TransitFlag=NONE (terminal).
		/// @param fsm 부모 StageStateMachine (역참조, non-owning).
		explicit GameOverState(StageStateMachine *fsm)
		    : BaseStageFsmState(fsm, EStageStatus::GameOver, EStageStatus::NONE) // terminal (transit=0)
		{
		}

		void OnEnter(SJH::Scene::Actor &root) override
		{
			if (auto *ctx = GetCtx(root))
			{
				if (ctx->overlay) ctx->overlay->Show(ctx->gameOverTex);
				// [FMOD] GameOver 에서도 BGM 은 계속 재생 - Stop() 안 함(BGM_STATE 도 유지).
				//   HP=0 이라 Health=0(Low Pass Filter 최대) 상태 -> 즉시 1.0 으로 꺾지 않고
				//   1초에 걸쳐 0->1.0 으로 *점진 증가*시켜 LPF 를 서서히 해제한다(TweenPlayable).
				//   OnUpdate 가 매 프레임 tick(FSM Update 는 ungated - GameOver 에서도 호출됨).
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
			// terminal - State 전이는 없으나, Health 0->1.0 ramp 를 끝날 때까지 tick (끝나면 1.0 고정).
			if (mHealthFade && !mHealthFade->IsFinished())
				mHealthFade->Update(dt);
		}
		void OnExit(SJH::Scene::Actor &) override {}

	  private:
		std::unique_ptr<Tween::TweenPlayable<float>> mHealthFade; ///< GameOver Health 0->1.0 ramp (detached, 수동 tick).
	};
} // namespace TopdownShooter::Stage

#endif // __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__
