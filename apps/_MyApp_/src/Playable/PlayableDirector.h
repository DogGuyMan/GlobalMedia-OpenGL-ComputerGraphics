#ifndef __TOPDOWNSHOOTER_PLAYABLE_PLAYABLE_DIRECTOR_H__
#define __TOPDOWNSHOOTER_PLAYABLE_PLAYABLE_DIRECTOR_H__

#include "Entity/Components/Components.Interfaces.h" // IActorPresentation / EFacing / EPose (sink)
#include "playable/playable_base.h"                  // SJH::Playable::PlayableBase (map 보유 + Component)
#include "scene/actor.h"                             // SJH::Scene::Component

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vmath.h>

namespace SJH::Sprite { class SpriteRenderer; } // DirGroup 핸들 (실타입 .cpp)

namespace TopdownShooter::Playable
{
	/// @brief 연출 디렉터 — named Playable 보유 + 중앙 tick (정본 패턴 = src/timer/multiple_timer.h).
	///        철학: 게임 로직(HP/damage/physics/AI/입력판정)은 별개. 모든 연출/비주얼/사운드는
	///        이 director 의 named Playable 로만 (verb->Play(key)).
	///        Component + IActorPresentation 다중상속:
	///          - Component          : 액터에 부착되어 scene tick 을 받아 보유 Playable 을 직접 ->Update.
	///          - IActorPresentation : Life 의 sink (Life::DoDamaged->ReactDamaged / DoDie->ReactDied).
	///        보유 Playable 은 AddComponent 하지 않고 director 가 직접 Update(dt) 한다
	///        (MultipleTimer 가 Timer 를 Tick 하듯). Play() 로 활성화된 슬롯만 tick — 등록 직후
	///        미재생 슬롯은 tick 제외(자동재생 방지). leaf/Composite Playable 자체는 src/playable + Client.
	class PlayableDirector : public SJH::Scene::Component, public Entity::IActorPresentation
	{
	  public:
		/// @brief @p key 로 Playable 등록 (소유 이관). 같은 키 재등록은 덮어쓰기. fluent.
		PlayableDirector &Register(const std::string &key, std::unique_ptr<SJH::Playable::PlayableBase> p);

		/// @brief 있으면 Stop()+Play() (첫 프레임부터 재생) + 슬롯 활성화. 없으면 silent no-op.
		void Play(const std::string &key);
		/// @brief 있으면 Stop() (리셋 후 정지) + 슬롯 비활성. 없으면 no-op.
		void Stop(const std::string &key);
		bool Has(const std::string &key) const;

		/// @brief (facing,pose) 한 그룹 = 그 방향/포즈 4-레이어 SpriteRenderer 핸들(애니 미보유).
		struct DirGroup { std::array<SJH::Sprite::SpriteRenderer *, 4> layers{}; };
		/// @brief 그룹 등록 (빌드 시 1회).
		void RegisterGroup(Entity::EFacing f, Entity::EPose p, const DirGroup &g) { mGroups[idx(f)][idx(p)] = g; }
		/// @brief AddChild(enter) 후 1회 — 초기 (Front,Idle) 만 활성.
		void RefreshDirectional() { Apply(); }

		// === Component hook ===
		void OnEnter() override {}
		void OnExit() override {}
		/// @brief 중앙 tick — Play() 로 활성화되고 미완료인 슬롯만 ->Update(dt).
		void Update(float dt) override;

		// === IActorPresentation — verb->Play(key) (미등록 키는 silent no-op) ===
		void ReactDamaged(int /*dmg*/) override { Play("hit"); }
		void ReactDied(vmath::vec3 pos) override; // Play("death") 만 (월드점 death FX[B]는 도메인 seam 이 트리거 — 역할별 분리 P2)
		void ReactAttack(vmath::vec2 /*aimDir*/) override { Play("attack"); }

		// SetFacing/SetPose — controller(RD5) 가 계산한 facing/pose 로 8그룹 가시성 토글.
		void SetFacing(Entity::EFacing f) override; // 본문 .cpp
		void SetPose(Entity::EPose p) override;     // 본문 .cpp

	  private:
		/// @brief 한 named Playable 슬롯. playing = Play() 로 활성화됨(미활성/완료 슬롯은 tick 제외).
		struct Slot
		{
			std::unique_ptr<SJH::Playable::PlayableBase> playable;
			bool                                         playing = false;
		};

		std::map<std::string, Slot> mPlayables;

		static int idx(Entity::EFacing f) { return static_cast<int>(f); }
		static int idx(Entity::EPose p) { return static_cast<int>(p); }
		void Apply();
		DirGroup        mGroups[4][2]{};
		Entity::EFacing mFacing = Entity::EFacing::Front;
		Entity::EPose   mPose   = Entity::EPose::Idle;
	};
}

#endif // __TOPDOWNSHOOTER_PLAYABLE_PLAYABLE_DIRECTOR_H__
