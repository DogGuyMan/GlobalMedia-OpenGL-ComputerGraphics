/**
 * @file InitTaskId.h
 * @brief InitScheduler task 카탈로그 -- @c EInitTask (식별자 = 우선순위) + 진단용 @c ToString.
 *
 * @details
 *  ### 책임
 *  - 클라 init task 의 *단일 식별자 출처* (매직스트링 대체). enum 값 = tiebreak 우선순위.
 *  ### 비-책임
 *  - [X] 스케줄링 로직 -- @c InitScheduler 담당. 본 파일은 식별자 데이터만.
 *
 * @note "task 목록 데이터"(자주 변함) 를 "스케줄러 메커니즘"(안정) 과 분리. task 추가/재정렬은 이 파일만.
 *       엔진 승격 시 스케줄러를 @c InitScheduler<TId> 로 템플릿화하면 본 enum 결합이 풀린다 (현재 YAGNI).
 */
#ifndef _TOPDOWNSHOOTER_BOOTSTRAP_INITTASKID_H__
#define _TOPDOWNSHOOTER_BOOTSTRAP_INITTASKID_H__

namespace TopdownShooter::Bootstrap
{
	/// @brief InitScheduler task 식별자 -- enum 값 = tiebreak 우선순위 (작을수록 먼저).
	/// @details 의도된 실행 순서로 선언. 같은 위상레벨 다수 task 는 이 값으로 결정적 정렬
	///          (등록 순서 대체). hard 순서는 InitScheduler 의 .Needs(...) 위상정렬이 담당.
	enum class EInitTask
	{
		Core,            ///< phase1: 렌더타깃 + 시스템 init + 오디오 워밍업.
		ScreenPipeline,  ///< phase2: DefaultPipeline + ScreenCamera + PostFX.
		VfxUi,           ///< phase2: VFX 이펙트(orbital 포함) 로드 + 게임 UI. World 보다 먼저.
		World,           ///< phase2: WorldScene + 스테이지 액터(orbital FindEffect) + 플레이어.
		Stages,          ///< phase2: stages 컬렉션 조립.
		Enter,           ///< phase3: Director.Enter.
		Fsm,             ///< phase3: GameContext + Stage FSM.
	};

	/// @brief 진단/로그용 이름 (C++17 enum reflection 부재 -> 수동 매핑). 미지 값은 "?".
	inline const char *ToString(EInitTask id)
	{
		switch (id)
		{
		case EInitTask::Core:           return "Core";
		case EInitTask::ScreenPipeline: return "ScreenPipeline";
		case EInitTask::VfxUi:          return "VfxUi";
		case EInitTask::World:          return "World";
		case EInitTask::Stages:         return "Stages";
		case EInitTask::Enter:          return "Enter";
		case EInitTask::Fsm:            return "Fsm";
		}
		return "?";
	}
}

#endif // _TOPDOWNSHOOTER_BOOTSTRAP_INITTASKID_H__
