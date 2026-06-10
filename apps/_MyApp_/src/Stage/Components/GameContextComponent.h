/**
 * @file GameContextComponent.h
 * @brief Stage FSM State 가 공유하는 컨텍스트 데이터 컴포넌트 - Root Actor 에 부착.
 *
 * @details
 *  ### 책임
 *  - State (TitleState / CombatPlayState / PauseState / GameOverState) 가 공통으로
 *    참조하는 오버레이 텍스처, WaveController, AudioSystem, BGM Playable, PlayerLife 포인터를
 *    한 곳에 모아 Root Actor 에 부착한다.
 *  - State 는 @c GetCtx(root) 헬퍼로 이 컴포넌트에 접근한다.
 *
 *  ### 비-책임
 *  - [X] 소유권 관리 - 모든 멤버는 비소유 raw 포인터.
 *    소유권은 각자 (overlay = ImGuiLayerStack, audio/bgmPlayable = Manager / BgmActor 등).
 *  - [X] 게임 로직 tick - Update/OnEnter/OnExit 는 stub.
 *
 * @note 포인터만 보유하므로 전방 선언으로 충분 - 사용처(StageState.Impl.h)가 실제 헤더를 include.
 *       God Object 분리(DDD) 는 차기 리팩토링(spec D7) 예정.
 */
#ifndef __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__
#define __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__

#include "scene/actor.h"
#include "resource_registry/texture.h"

// 포인터만 보유 - 전방 선언으로 충분 (UI/imgui / WaveController 헤더 전이 의존 차단).
// 실제 멤버 역참조(overlay->Show / waveCtrl->WaveLevel)는 사용처(StageState.Impl.h)가 헤더 include.
namespace TopdownShooter::UI { class StateOverlayLayer; }
namespace TopdownShooter::Stage { class WaveController; }
namespace TopdownShooter::Audio { class AudioSystem; class FmodStudioPlayable; }
namespace TopdownShooter::Entity::Components { class Life; }
namespace SJH::Scene { class PassComponent; }

namespace TopdownShooter::Stage::Components
{
	/**
	 * @brief Stage FSM State 가 참조하는 공유 컨텍스트 Component - Root Actor 에 부착.
	 * @details
	 *  State 는 @c GetCtx(root) 헬퍼를 통해 이 컴포넌트에 접근한다
	 *  (@c root.FindChild("GameContext")->GetComponent<GameContextComponent>() 경로).
	 *
	 *  보유 항목:
	 *  - overlay / 텍스처 3종 - Title/Pause/GameOver 오버레이 UI.
	 *  - waveCtrl             - 웨이브 레벨 조회 + Player 사망 감시/GameOver 전이 주체.
	 *  - blurPass             - Title/Pause 동안 blur PostFX Enabled 토글.
	 *  - audio / bgmPlayable  - FMOD AudioSystem + BGM EventInstance (Manager 를 직접 쓰지 않음).
	 *  - playerLife           - CombatPlay 가 HP 비율을 FMOD "Health" 파라미터로 송신.
	 *
	 *  모든 멤버는 비소유 raw 포인터. 소유권은 각 원소유자가 보유.
	 */
	class GameContextComponent : public SJH::Scene::Component
	{
	  public:
		void OnEnter() override {}
		void OnExit() override {}
		void Update(float) override {}

		// 오버레이 (ImGuiLayerStack 이 소유 - 여기는 비소유 raw)
		UI::StateOverlayLayer *overlay     = nullptr; ///< 화면 오버레이 레이어 (비소유).
		const SJH::Texture    *titleTex    = nullptr; ///< Title 화면 배경 텍스처 (비소유).
		const SJH::Texture    *pauseTex    = nullptr; ///< Pause 화면 오버레이 텍스처 (비소유).
		const SJH::Texture    *gameOverTex = nullptr; ///< GameOver 화면 오버레이 텍스처 (비소유).

		// 웨이브 진행 조회(ctx->waveCtrl->WaveLevel()/AliveEnemyCount())
		// + Player 사망 감시/GameOver 전이 주체 (WaveController 가 구동)
		WaveController *waveCtrl = nullptr; ///< 웨이브 진행 조회 + Player 사망 감시/GameOver 전이 주체 (비소유).

		// Title 동안 blur PostFX ON (TitleState OnEnter/OnExit 에서 Enabled 토글)
		SJH::Scene::PassComponent *blurPass = nullptr; ///< Title/Pause 동안 blur PostFX Enabled 토글 (비소유).

		// FMOD 재생 레퍼런스 - State 는 Manager 싱글톤이 아니라 ctx 를 통해 오디오 접근.
		//   audio        = AudioSystem (global parameter "Health" 등 System 스코프 호출)
		//   bgmPlayable  = BGM EventInstance wrap (Play/BGM_STATE/pause/stop - handoff sec.2)
		// 둘 다 비소유 raw - Manager(audio) / BgmActor(bgmPlayable) 가 소유. startup 에서 주입.
		Audio::AudioSystem        *audio       = nullptr; ///< FMOD AudioSystem - global parameter 송신용 (비소유).
		Audio::FmodStudioPlayable *bgmPlayable = nullptr; ///< BGM EventInstance wrap (Play/BGM_STATE/pause/stop, 비소유).
		// Player Life - CombatPlay 가 HP 비율을 FMOD global "Health" 파라미터로 송신 (sec.2).
		Entity::Components::Life *playerLife = nullptr; ///< Player Life Component - HP 비율 -> FMOD "Health" 파라미터 (비소유).
	};
} // namespace TopdownShooter::Stage::Components

#endif // __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__
