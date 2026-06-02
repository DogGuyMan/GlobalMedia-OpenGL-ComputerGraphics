#ifndef _TOPDOWNSHOOTER_ENTITY_BASE__
#define _TOPDOWNSHOOTER_ENTITY_BASE__

#include "Components/Components.Interfaces.h"   // ILivable/IDieable/IDamageable/IImpulsable
#include "scene/actor.h"
#include <string>

// fwd — 포인터 멤버만 보유(완전형은 .cpp 에서). Entity 헤더가 Playable/Physics 헤더를 안 끌어오게.
namespace TopdownShooter::Entity::Components { class Life; }
namespace TopdownShooter::Physics::Components { class Physics; }
namespace TopdownShooter::Physics { class Impulse; }
namespace TopdownShooter::Playable { class PlayableDirector; }

namespace TopdownShooter::Entity
{
	/// @brief 엔티티 공통 Accessor-facade (Player/Enemy 공유). 로직 0 — 형제 컴포넌트 비소유 캐시 + 위임/노출.
	class BaseEntity : public SJH::Scene::Component,
	                   public ILivable, public IDieable, public IDamageable, public IImpulsable
	{
	  protected:
		Components::Life*                    mLife     = nullptr;
		Physics::Components::Physics*        mPhysics  = nullptr;   // = entityRigidbody/Collider
		Playable::PlayableDirector*          mDirector = nullptr;   // 연출+Audio (named playable)
		Physics::Impulse*                    mImpulse  = nullptr;   // Player+Enemy 부착. null-guard=비엔티티 바디 대비

	  public:
		void OnEnter() override;   // 4캐시 (.cpp — GetComponent/FindPhysics 완전형 필요)
		void OnExit()  override;
		void Update(float /*dt*/) override {}   // 로직 없음 — 형제가 자기 Update 보유

		// ── Life 위임 (ILivable/IDieable/IDamageable) ──
		bool IsAlive()  const override;
		int  GetHp()    const override;
		int  GetMaxHp() const override;
		void DoDamaged(int damage) override;   // i-frame 은 Life 내부
		void DoDie()               override;
		// ── Impulse 위임 (IImpulsable) — 넉백/대시 ──
		void DoImpulse(vmath::vec2 dir) override;
		bool IsImpulseActive() const;          // 버스트 활성 창 = 이동 suppress 게이트
		// ── accessor ──
		Physics::Components::Physics* GetPhysics()  const { return mPhysics; }
		Playable::PlayableDirector*   GetDirector() const { return mDirector; } // 포인터 반환 = fwd OK
		void Play(const std::string& key);     // .cpp (director->Play 완전형 필요)
	};
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_BASE__
