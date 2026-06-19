/**
 * @file EngineBootstrap.h
 * @brief 변동성 분리 부트 스켈레톤 -- 저변동 엔진이 3개 클라 hook 을 순차 호출 (Template Method).
 *
 * @details
 *  ### 책임
 *  - @c IClientBootstrap : 클라가 구현하는 3 hook (자원 준비 / 씬 구성 / 첫 프레임 직전).
 *  - @c EngineBootstrap::Boot : 엔진 고정 단계(저변동) + hook 순차 호출 (제어역전 -- 엔진이 클라 역호출).
 *  ### 비-책임
 *  - [X] 각 hook 내부 task 순서 -- 클라가 phase-local @c InitScheduler 로 topo-sort.
 *  - [X] window/GL 컨텍스트 생성 -- sb7 @c application::run() 이 startup() 이전에 소유 (불가침).
 *
 *  ### 정통 매핑 (context7 검증)
 *  - Unreal @c ELoadingPhase / Unity @c RuntimeInitializeLoadType / Cocos @c applicationDidFinishLaunching :
 *    엔진이 고정 페이즈 시퀀스를 소유하고 클라 코드를 정해진 타이밍에 역호출.
 *
 * @note 거주 = 클라 Bootstrap/ (YAGNI -- 재사용 demo 생기면 src/ 엔진 모듈 승격, InitScheduler 와 동일 정책).
 *       엔진 고정 본문이 얇은 것은 sb7 이 window/GL 을 소유하기 때문 (정상) -- 가치는 hook phase 시퀀싱.
 */
#ifndef _TOPDOWNSHOOTER_BOOTSTRAP_ENGINEBOOTSTRAP_H__
#define _TOPDOWNSHOOTER_BOOTSTRAP_ENGINEBOOTSTRAP_H__

namespace TopdownShooter::Bootstrap
{
	/// @brief 클라가 구현하는 부트 hook 3종 (제어역전 seam -- 다형성용 아님, sb7 startup/render 와 동일 정당성).
	class IClientBootstrap
	{
	  public:
		virtual ~IClientBootstrap() = default;

		/// @brief 자원 준비 phase -- 프레임버퍼/타깃/시스템 초기화/에셋 로드.
		virtual void OnResourcesReady() = 0;
		/// @brief 씬 구성 phase -- 파이프라인/카메라/PostFX/스테이지/플레이어/UI.
		virtual void OnSceneSetup() = 0;
		/// @brief 첫 프레임 직전 phase -- Director.Enter / GameContext / FSM 진입.
		virtual void OnBeforeFirstFrame() = 0;
	};

	/// @brief 저변동 부트 스켈레톤 -- 엔진 고정 단계 + 클라 hook 순차 호출 (Template Method).
	class EngineBootstrap
	{
	  public:
		/// @brief 부트 시퀀스 실행 -- 엔진 고정 단계 후 클라 3 hook 을 정해진 순서로 역호출.
		/// @param client hook 을 구현한 클라 부트스트랩.
		void Boot(IClientBootstrap &client)
		{
			// 엔진 고정(저변동) 단계가 여기 위치한다. 현재 sb7 이 window/GL 을 소유하므로
			// 엔진 전용 init 은 거의 없다 -- 미래 엔진-레벨 고정 단계(전역 GL 상태 기본값 등)는
			// 아래 hook 들 *앞/사이* 에 직선 코드로 추가한다 (저변동이라 topo-sort 불요).

			client.OnResourcesReady();   // phase 1: 자원/시스템
			client.OnSceneSetup();       // phase 2: 씬/파이프라인
			client.OnBeforeFirstFrame(); // phase 3: 진입/FSM
		}
	};
}

#endif // _TOPDOWNSHOOTER_BOOTSTRAP_ENGINEBOOTSTRAP_H__
