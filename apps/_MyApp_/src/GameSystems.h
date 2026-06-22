/**
 * @file GameSystems.h
 * @brief 게임 클라이언트의 중앙 허브 - 하위 시스템(Physics/Audio/VFX/WorldText) + SceneRenderer 를
 *        한 곳에 집계하는 게임 시스템 허브(GameSystems) 싱글톤.
 *
 * @details
 *  ### 책임
 *  - 게임 시스템 4종(@c PhysicsSystem / @c AudioSystem / @c VFXSystem / @c WorldTextSystem)과
 *    @c SJH::SceneRenderer 를 값으로 보유하고 단일 접근점 제공.
 *  - 시스템 라이프사이클 일괄 조율 - @c Init / @c Update / @c Shutdown.
 *  - 각 시스템에 대한 레퍼런스 접근자(@c Audio / @c VFX / @c Physics / @c SceneRenderer / @c WorldText) 노출.
 *
 *  ### 비-책임
 *  - [X] 엔진 씬 그래프 관리 - @c SJH::Scene::Director (engine) 가 별도 담당. 본 클래스와 namespace 분리.
 *  - [X] 게임 루프 자체 - @c main.cpp 의 sb7 render() 가 호출 타이밍을 결정.
 *
 *  ### 정통 매핑
 *  - Cocos2D `cc::Director` - 시스템/씬 조율 싱글톤. 단 본 클래스는 *별도 인스턴스* (엔진 Director 와 무관).
 *
 * @note 엔진의 @c SJH::Scene::Director 와 이름이 겹치지 않게 namespace(@c TopdownShooter) 로 분리되어 있다.
 */
#ifndef _TOPDOWNSHOOTER_GAMESYSTEMS_H__
#define _TOPDOWNSHOOTER_GAMESYSTEMS_H__

#include "Audio/AudioSystem.h"
#include "Physics/PhysicsSystem.h"
#include "VFX/VFXSystem.h"
#include "Text/WorldTextSystem.h"   // <- 추가
#include "render/render_passable/render_passable.impls.h"   // SceneRenderer

namespace TopdownShooter
{
	/**
	 * @brief 게임 시스템 허브(GameSystems) - PhysicsSystem + AudioSystem + VFXSystem + WorldTextSystem + SceneRenderer 집계 싱글톤.
	 * @details
	 *  - @b SJH::Scene::Director @b (engine) @b 와 @b 무관 - namespace 분리, Cocos cc::Director 정통 *별도 인스턴스*.
	 *  - main 의 startup 에서 @c Init, render 마다 @c Update(dt), shutdown 에서 @c Shutdown 호출.
	 *  - 멤버는 값 보유 (싱글톤 자체 라이프타임 = 프로그램 종료까지).
	 *  - 복사/이동 금지 - Meyer's 싱글톤.
	 */
	class GameSystems
	{
	  public:
		/// @brief Meyer's 싱글톤 접근. 최초 호출 시 인스턴스 생성.
		/// @return 프로세스 전역 단일 @c GameSystems 레퍼런스.
		static GameSystems &Get();

		/// @brief 보유한 하위 시스템 전체를 초기화. main 의 startup 에서 1회 호출.
		/// @details Audio -> VFX(maxSprites) -> Physics -> WorldText(BMFont 로드) 순.
		void Init();

		/// @brief 게임 시뮬레이션 tick - VFX 갱신 + Physics Step.
		/// @details Stage FSM 의 CombatPlayState 에서만 호출(Title/Pause/GameOver 는 freeze).
		///          오디오는 여기서 펌프하지 않음 - 모든 State 에서 매 프레임 갱신해야 BGM 이
		///          재생/전환되므로 render() 가 @c mAudio.Update 를 ungated 로 직접 호출.
		/// @param dt 직전 프레임 경과 시간(초).
		void Update(float dt);

		/// @brief 보유한 하위 시스템 전체를 해제. main 의 shutdown 에서 1회 호출.
		/// @details Physics -> VFX -> Audio 역순 해제.
		void Shutdown();

		/// @brief 오디오 시스템 레퍼런스 접근자.
		/// @return 보유 중인 @c Audio::AudioSystem 레퍼런스.
		Audio::AudioSystem      &Audio()   { return mAudio; }
		/// @brief VFX(파티클) 시스템 레퍼런스 접근자.
		/// @return 보유 중인 @c VFX::VFXSystem 레퍼런스.
		VFX::VFXSystem          &VFX()     { return mVFX; }
		/// @brief 물리 시스템 레퍼런스 접근자.
		/// @return 보유 중인 @c Physics::PhysicsSystem 레퍼런스.
		Physics::PhysicsSystem  &Physics() { return mPhysics; }
		/// @brief 씬 렌더러 레퍼런스 접근자.
		/// @deprecated [DEAD-PHASE5] SceneRenderer 미사용(3.5a: world->WorldPass, screen->PostFxPass). 호출처 0.
		///             이 accessor + @c mScenesRender 멤버는 *모든 Task 종료 후 Phase 5* 제거 후보.
		/// @return 보유 중인 @c SJH::SceneRenderer 레퍼런스.
		SJH::SceneRenderer      &SceneRenderer() { return mScenesRender; }
		/// @brief 월드 텍스트 시스템 레퍼런스 접근자.
		/// @return 보유 중인 @c Text::WorldTextSystem 레퍼런스.
		Text::WorldTextSystem   &WorldText() { return mWorldText; }   // <- 추가

		GameSystems(const GameSystems &)            = delete;
		GameSystems &operator=(const GameSystems &) = delete;
		GameSystems(GameSystems &&)                 = delete;
		GameSystems &operator=(GameSystems &&)      = delete;

	  private:
		GameSystems()  = default;
		~GameSystems() = default;

		SJH::SceneRenderer	mScenesRender;        ///< 씬 렌더러 - Camera 컬렉션 순회 + 1패스 렌더.
		Audio::AudioSystem      mAudio;            ///< 오디오 시스템 - FMOD Studio 래퍼.
		VFX::VFXSystem          mVFX;              ///< VFX 시스템 - Effekseer 파티클 매니저.
		Physics::PhysicsSystem  mPhysics;          ///< 물리 시스템 - Box2D world.
		Text::WorldTextSystem   mWorldText;   // <- 추가  ///< 월드 텍스트 시스템 - BMFont 보유.
	};
}

#endif // _TOPDOWNSHOOTER_GAMESYSTEMS_H__
