#ifndef _TOPDOWNSHOOTER_VFX_EFFEKSEERPLAYABLE_H__
#define _TOPDOWNSHOOTER_VFX_EFFEKSEERPLAYABLE_H__

#include "playable/playable_base.h"
#include "resource_registry/effect.h"   // SJH::Effect
#include <Effekseer.h>
#include <vmath.h>

namespace TopdownShooter::VFX
{
	/// @brief 3D 위치 추적 정책 (Phase 4 결정).
	enum class TrackPolicy
	{
		Static,        // OnPlay 시 1회 SetLocation (단발 muzzle/폭발)
		FollowOwner    // OnUpdate 매 frame Actor.Transform.Translate 추적 (오라/이펙트)
	};

	/// @brief Effekseer Effect spawn + lifecycle 관리 leaf Playable (M5 T2).
	class EffekseerPlayable : public SJH::Playable::PlayableBase
	{
	  public:
		EffekseerPlayable(::Effekseer::ManagerRef manager,
		                  SJH::Effect            *effect,
		                  const vmath::vec3      &spawnPos = vmath::vec3(0.0f),
		                  TrackPolicy             track    = TrackPolicy::Static);
		~EffekseerPlayable() override;

	  protected:
		void OnPlay() override;
		void OnStop() override;
		void OnUpdate(float dt) override;

	  private:
		::Effekseer::ManagerRef mManager;
		SJH::Effect            *mEffect   = nullptr;
		::Effekseer::Handle     mHandle   = -1;   // -1 = invalid
		vmath::vec3             mSpawnPos{0.0f};
		TrackPolicy             mTrack    = TrackPolicy::Static;
	};
}

#endif // _TOPDOWNSHOOTER_VFX_EFFEKSEERPLAYABLE_H__
