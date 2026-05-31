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
	struct SpriteFrameClip;
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
		/// @brief SpriteFrameClip 의 lifetime 저장소 — main 이 멤버로 소유 (SpriteSequencePlayable 가
		///        clip 을 *포인터로만* 보유하므로 빌더 로컬에 두면 댕글링. caller-owned 필수).
		SJH::SpriteSequence::SpriteFrameClip *clipStorage = nullptr;
	};

	/// @brief main 이 멤버로 보유할 산출 포인터.
	struct PlayerResult
	{
		SJH::Sprite::SpriteRenderer                  *Sprite      = nullptr;
		SJH::SpriteSequence::SpriteSequencePlayable  *SpriteSeq   = nullptr;
		SJH::Scene::Actor                            *SpriteActor = nullptr;
	};

	/// @brief 플레이어 액터 구성 — PlayerActorConfig(Life/Movement/Controller/Physics) +
	///        좌클릭(onFire) / G키(onDamage) Composite 콜백 + 스프라이트 아틀라스 시퀀스
	///        + WorldCamera ActorFolower follow-target wiring. 기존 main.cpp WramupPlayer 와 동일.
	/// @return atlas 로드 실패 시 SpriteActor=nullptr 인 빈 Result (기존 early-return 보존).
	PlayerResult BuildPlayer(const PlayerDeps &deps);
}

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_PLAYER_BUILDER_H__
