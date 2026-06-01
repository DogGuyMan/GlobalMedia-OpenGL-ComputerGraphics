#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_PLAYER_BUILDER_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_PLAYER_BUILDER_H__

#include "InputHandler/PlayerController.h" // Controller::PlayerController::Action (KeyboardInput 템플릿 인자)
#include "input/keyboard_input.h"
#include "input/mouse_input.h"

// fwd — 포인터만 노출.
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
	/// @brief BuildPlayer 입력 의존 — 비싱글턴만 (reg/dir/Manager 은 ::Get()).
	struct PlayerDeps
	{
		SJH::KeyboardInput<Controller::PlayerController::Action> *keyboard = nullptr;
		SJH::MouseInput     *mouse        = nullptr;
		b2World             *physicsWorld = nullptr;
		SJH::Scene::Camera  *worldCamera  = nullptr; ///< raycast 카메라 + ActorFolower follow-target wiring.
	};

	/// @brief main 이 멤버로 보유할 산출 포인터.
	struct PlayerResult
	{
		SJH::Sprite::SpriteRenderer                  *Sprite      = nullptr;
		SJH::SpriteSequence::SpriteSequencePlayable  *SpriteSeq   = nullptr;
		SJH::Scene::Actor                            *SpriteActor = nullptr;
	};

	/// @brief 플레이어 액터 구성 — PlayerActorConfig(Life/Movement/Controller/Physics/Weapon) +
	///        좌클릭(onFire) / G키(onDamage) Composite 콜백 + FRONT_MOVE 4-레이어 스프라이트(CreatePlayerActor)
	///        + WorldCamera ActorFolower follow-target wiring.
	/// @return 구성된 PlayerResult (SpriteActor = root 부착 플레이어; Sprite/SpriteSeq = 애니 레이어 또는 nullptr).
	PlayerResult BuildPlayer(const PlayerDeps &deps);
}

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_PLAYER_BUILDER_H__
