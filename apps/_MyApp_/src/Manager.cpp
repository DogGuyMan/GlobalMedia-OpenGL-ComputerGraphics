/**
 * @file Manager.cpp
 * @brief @c TopdownShooter::Manager 구현 - 하위 시스템 라이프사이클 일괄 조율.
 *
 * @details
 *  ### 책임
 *  - Meyer's 싱글톤 인스턴스 제공(@c Get).
 *  - @c Init / @c Update / @c Shutdown 에서 하위 시스템들의 동일 단계를 정해진 순서로 호출.
 *
 *  ### 주의 - render loop 흐름 (main.cpp 기준)
 *  한 프레임의 시뮬레이션 단계 순서:
 *   1. @c Manager::Update(dt) - VFX 갱신 후 Physics @c Step (이 안에서 충돌 콜백 enqueue).
 *   2. (Physics Step 종료 직후) 게임 코드가 Sweep - Step 잠금 중 미뤄둔 body 변경/디스폰 일괄 처리.
 *   3. SceneRenderer 가 씬을 그린다.
 *  오디오는 본 @c Update 밖에서 render() 가 ungated 로 펌프(아래 @c Update 주석 참조).
 */
#include "Manager.h"

#include <spdlog/spdlog.h>

namespace TopdownShooter
{
	/// @copydoc Manager::Get
	Manager &Manager::Get()
	{
		static Manager instance;
		return instance;
	}

	/// @copydoc Manager::Init
	void Manager::Init()
	{
		mAudio.Init();
		mVFX.Init(/*maxSprites=*/8000);
		mPhysics.Init();
		mWorldText.Init();   // minogram BMFont 로드 (내부에서 ResourceRegistry::Get())
		spdlog::info("[Director] init OK (Audio + VFX + Physics + WorldText)");
	}

	/// @copydoc Manager::Update
	// 게임 시뮬레이션 tick — Stage FSM 의 CombatPlayState 에서만 호출(Title/Pause/GameOver freeze).
	// ※ 오디오(mAudio.Update)는 여기 없음 — 모든 State 에서 매 프레임 펌프해야 BGM 이 재생/전환되므로
	//   render() 가 ungated 로 mAudio.Update 를 직접 호출(FMOD Studio update = 비동기 명령 큐 처리).
	void Manager::Update(float dt)
	{
		mVFX.Update(dt);
		mPhysics.Step(dt);
	}

	/// @copydoc Manager::Shutdown
	void Manager::Shutdown()
	{
		// 역순 — Physics 가 다른 시스템 참조 없으므로 임의 순서 OK 이지만 명시.
		mPhysics.Shutdown();
		mVFX.Shutdown();
		mAudio.Shutdown();
		spdlog::info("[Director] shutdown OK");
	}
}
