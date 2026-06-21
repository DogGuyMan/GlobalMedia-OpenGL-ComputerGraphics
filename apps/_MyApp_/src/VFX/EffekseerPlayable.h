#ifndef _TOPDOWNSHOOTER_VFX_EFFEKSEERPLAYABLE_H__
#define _TOPDOWNSHOOTER_VFX_EFFEKSEERPLAYABLE_H__

#include "playable/playable_base.h"
#include "resource_registry/effect.h"   // SJH::Effect
#include <Effekseer.h>
#include <glm/glm.hpp>

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
		                  const glm::vec3      &spawnPos = glm::vec3(0.0f),
		                  TrackPolicy             track    = TrackPolicy::Static,
		                  float                   yawRad   = 0.0f);
		~EffekseerPlayable() override;

	  public:
		/// @brief 매 프레임 Y축 회전 속도(rad/sec). 0=비회전. OnUpdate 가 yaw = mYaw + mSpinxelapsed 적용 (궁극기 회전 레이저).
		void SetSpinRadPerSec(float v) { mSpinRadPerSec = v; }
		/// @brief 최대 지속(초). >0 이면 경과 시 StopEffect + finished (AutoDespawnOnFinish 트리거). 0=무제한.
		void SetMaxDurationSec(float v) { mMaxDurationSec = v; }

	  protected:
		void OnPlay() override;
		void OnStop() override;
		void OnUpdate(float dt) override;

	  private:
		// Play + (yaw!=0) SetRotation 공유 - OnPlay 와 루프 재생(OnUpdate)이 함께 호출.
		void StartHandle();

		::Effekseer::ManagerRef mManager;
		SJH::Effect            *mEffect         = nullptr;
		::Effekseer::Handle     mHandle         = -1;   // -1 = invalid
		glm::vec3             mSpawnPos{0.0f};
		TrackPolicy             mTrack          = TrackPolicy::Static;
		float                   mYaw            = 0.0f; // Y축 시작 회전(라디안). 0 = 회전 없음
		float                   mSpinRadPerSec  = 0.0f; // 매 프레임 회전 속도(rad/sec). 0 = 비회전
		float                   mMaxDurationSec = 0.0f; // 최대 지속(초). >0 이면 경과 시 자동 종료. 0=무제한
	};
}

#endif // _TOPDOWNSHOOTER_VFX_EFFEKSEERPLAYABLE_H__
