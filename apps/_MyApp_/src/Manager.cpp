#include "Manager.h"

#include <spdlog/spdlog.h>

namespace TopdownShooter
{
	Manager &Manager::Get()
	{
		static Manager instance;
		return instance;
	}

	void Manager::Init()
	{
		mAudio.Init();
		mVFX.Init(/*maxSprites=*/8000);
		mPhysics.Init();
		mWorldText.Init();   // minogram BMFont 로드 (내부에서 ResourceRegistry::Get())
		spdlog::info("[Director] init OK (Audio + VFX + Physics + WorldText)");
	}

	// 게임 시뮬레이션 tick — Stage FSM 의 CombatPlayState 에서만 호출(Title/Pause/GameOver freeze).
	// ※ 오디오(mAudio.Update)는 여기 없음 — 모든 State 에서 매 프레임 펌프해야 BGM 이 재생/전환되므로
	//   render() 가 ungated 로 mAudio.Update 를 직접 호출(FMOD Studio update = 비동기 명령 큐 처리).
	void Manager::Update(float dt)
	{
		mVFX.Update(dt);
		mPhysics.Step(dt);
	}

	void Manager::Shutdown()
	{
		// 역순 — Physics 가 다른 시스템 참조 없으므로 임의 순서 OK 이지만 명시.
		mPhysics.Shutdown();
		mVFX.Shutdown();
		mAudio.Shutdown();
		spdlog::info("[Director] shutdown OK");
	}
}
