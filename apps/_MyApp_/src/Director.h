#ifndef _TOPDOWNSHOOTER_DIRECTOR_H__
#define _TOPDOWNSHOOTER_DIRECTOR_H__

#include "Audio/AudioSystem.h"
#include "Physics/physics_system.h"
#include "VFX/VFXSystem.h"

namespace TopdownShooter
{
	/// @brief Client-side Director — PhysicsSystem + AudioSystem + VFXSystem 집계 싱글톤.
	/// @details
	///   - **`SJH::Scene::Director` (engine) 와 무관** — namespace 분리, Cocos cc::Director 정통 *별도 인스턴스*.
	///   - main 의 startup 에서 Init, render 마다 Update(dt), shutdown 에서 Shutdown 호출.
	///   - 멤버는 값 보유 (싱글톤 자체 라이프타임 = 프로그램 종료까지).
	class Director
	{
	  public:
		static Director &Get();

		void Init();
		void Update(float dt);
		void Shutdown();

		Audio::AudioSystem      &Audio()   { return mAudio; }
		VFX::VFXSystem          &VFX()     { return mVFX; }
		Physics::PhysicsSystem  &Physics() { return mPhysics; }

		Director(const Director &)            = delete;
		Director &operator=(const Director &) = delete;
		Director(Director &&)                 = delete;
		Director &operator=(Director &&)      = delete;

	  private:
		Director()  = default;
		~Director() = default;

		Audio::AudioSystem      mAudio;
		VFX::VFXSystem          mVFX;
		Physics::PhysicsSystem  mPhysics;
	};
}

#endif // _TOPDOWNSHOOTER_DIRECTOR_H__
