/**
 * @file PlayerBuilder.h
 * @brief 플레이어 액터 + 컴포넌트 클러스터를 조립하는 Pure Factory 진입점.
 *
 * @details
 *  ### 책임
 *  - @c PlayerDeps (keyboard/mouse/physicsWorld/worldCamera) 를 받아 PlayerActor 를 구성.
 *  - Life/Movement/Controller/Physics/Weapon 컴포넌트 조립 및 콜백 주입.
 *  - 방향별 스프라이트 8그룹(4방향 x 2포즈) 자식 Actor + @c PlayableDirector 등록.
 *  - 발밑 그림자/피격 원 데칼 + VFX/오디오 seam 람다 주입.
 *  - @c WorldCamera 의 @c ActorFolower follow-target 을 플레이어로 연결.
 *
 *  ### 비-책임
 *  - [X] 적/파티클/UI/스테이지 구성 - @c WorldSceneBuilder / EnemyBuilder 등 담당.
 *  - [X] ResourceRegistry / Director 생성 - 내부에서 @c ::Get() 싱글톤 접근.
 *
 *  ### 정통 매핑
 *  - Cocos2D @c GameScene::createPlayer / Unreal @c AGameMode::SpawnDefaultPawnAtTransform
 *    에 해당하는 플레이어 조립 공장.
 *
 * @note @c BuildPlayer 호출 전 @c Director::Get() 과 @c ResourceRegistry::Get() 이
 *       유효하게 초기화되어 있어야 한다 (main.cpp Warmup 이후 호출).
 */
#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_PLAYER_BUILDER_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_PLAYER_BUILDER_H__

#include "InputHandler/PlayerController.h" // Controller::PlayerController::Action (KeyboardInput 템플릿 인자)
#include "input/keyboard_input.h"
#include "input/mouse_input.h"

// fwd - 포인터만 노출.
class b2World;
namespace SJH::Scene
{
	class Camera;
	class Actor;
}
namespace SJH::Sprite
{
	class SpriteRenderer;
}
namespace SJH::SpriteSequence
{
	class SpriteSequencePlayable;
}

namespace TopdownShooter::Bootstrap
{
	/**
	 * @brief @c BuildPlayer 에 전달하는 외부 의존 묶음 - 비싱글턴 자원만 포함.
	 * @details @c ResourceRegistry / @c Director / @c GameSystems 등 싱글톤은
	 *          @c BuildPlayer 내부에서 @c ::Get() 으로 직접 조회하므로 여기에 포함하지 않는다.
	 */
	struct PlayerDeps
	{
		SJH::KeyboardInput<Controller::PlayerController::Action> *keyboard = nullptr; ///< 플레이어 키보드 입력.
		SJH::MouseInput     *mouse        = nullptr;    ///< 플레이어 마우스 입력 (좌클릭 발사/우클릭 조준).
		b2World             *physicsWorld = nullptr;    ///< Box2D 물리 월드 (PlayerBody + Weapon Bullet 생성).
		SJH::Scene::Camera  *worldCamera  = nullptr;    ///< 마우스->Ground 레이캐스트 카메라 + @c ActorFolower follow-target wiring.
	};

	/**
	 * @brief @c BuildPlayer 가 main 에게 반환하는 산출 포인터 묶음.
	 * @details main 이 멤버로 보유하여 다른 시스템(@c WaveController, @c GroundClick 등)에 주입한다.
	 */
	struct PlayerResult
	{
		SJH::Scene::Actor *SpriteActor = nullptr; ///< root 에 부착된 플레이어 Actor (WaveController 타깃 + GroundClick 주입용).
	};

	/**
	 * @brief 플레이어 액터 전체를 조립해 @c Director::Root() 에 AddChild 하고 산출물을 반환.
	 * @details 조립 순서:
	 *  1. @c PlayerActorConfig 구성 (Life/Movement/Controller/Physics/Weapon).
	 *  2. @c CreatePlayerActor 호출 -> spriteActor 생성.
	 *  3. renderActor / aimPivot 자식 Actor 추가.
	 *  4. @c AttachEntityPresentation (공통 연출 클러스터 - "death"/체력바/HitFlash).
	 *  5. @c BuildPlayerDirectionalGroups (4방향 x 2포즈 스프라이트 8그룹 + @c RegisterGroup).
	 *  6. @c HpGrayscalePostFX 컴포넌트 부착 (체력 감소 -> 화면 무채색).
	 *  7. @c RegisterPlayerCombatPlayables ("fire"/"dash"/"hit" 연출 Director 등록).
	 *  8. 입력 콜백 주입 (fire/damage -> director.Play, facing pivot, physicsWorld).
	 *  9. VFX seam 람다 주입 (Life/PlayerEntity/Weapon onXxxFx).
	 *  10. @c AttachGroundDecals (발밑 그림자 + 피격 원).
	 *  11. @c Root().AddChild -> enter.
	 *  12. @c PlayerHands 컴포넌트 부착 (손 child Actor 2개, aimPivot 궤도).
	 *  13. @c ActorFolower follow-target 을 SpriteActor 로 wiring.
	 *  14. @c RefreshDirectional (초기 가시성 확정 - Front/Idle 외 SetActive false).
	 * @param deps 비싱글턴 외부 의존 (@c PlayerDeps 참조).
	 * @return 구성된 @c PlayerResult (SpriteActor = root 부착 플레이어).
	 */
	PlayerResult BuildPlayer(const PlayerDeps &deps);
}

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_PLAYER_BUILDER_H__
