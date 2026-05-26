#include "Director.h"

#include <spdlog/spdlog.h>

namespace TopdownShooter
{
	Director &Director::Get()
	{
		static Director instance;
		return instance;
	}

	void Director::Init()
	{
		mAudio.Init();
		mVFX.Init(/*maxSprites=*/8000);
		mPhysics.Init();
		spdlog::info("[Director] init OK (Audio + VFX + Physics)");
	}

	void Director::Update(float dt)
	{
		mAudio.Update(dt);
		mVFX.Update(dt);
		mPhysics.Step(dt);
	}

	void Director::Shutdown()
	{
		// 역순 — Physics 가 다른 시스템 참조 없으므로 임의 순서 OK 이지만 명시.
		mPhysics.Shutdown();
		mVFX.Shutdown();
		mAudio.Shutdown();
		spdlog::info("[Director] shutdown OK");
	}
}
