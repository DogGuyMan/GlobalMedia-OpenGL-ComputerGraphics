/**
 * @file InitScheduler.h
 * @brief 데이터주도 초기화 스케줄러 -- (id, deps, affinity, fn) task 를 등록하면
 *        위상정렬(Kahn) 후 직렬 실행. startup() 의 수동 위상정렬을 선언적 그래프로 대체.
 *
 * @details
 *  ### 책임
 *  - @c Task(id) fluent 빌더로 init 단계를 (선행 의존 + affinity + 실행 람다) 로 선언.
 *  - @c RunAll : 검증(중복id/미등록dep/사이클 하드에러) -> Kahn 위상정렬 -> 직렬 실행(F-2 fail-fast).
 *  ### 비-책임
 *  - [X] 병렬 실행 -- T2 직렬만. @c EAffinity 는 분류만 (미래 T3 게이트, GL 단일스레드 제약).
 *  - [X] 엔진 hook 타이밍 -- @c EngineBootstrap / 클라가 RunAll 호출 시점 결정.
 *  ### 결정성
 *  - 같은 위상 레벨이 여럿이면 *@c EInitTask 값(우선순위)* 으로 tiebreak -> 재현 가능 (호출 순서 비의존).
 *
 * @note 재사용 demo 생기면 엔진 모듈 @c SJH::init 로 승격 가능 (현재 YAGNI -- 클라 거주, EInitTask 결합).
 */
#ifndef _TOPDOWNSHOOTER_BOOTSTRAP_INITSCHEDULER_H__
#define _TOPDOWNSHOOTER_BOOTSTRAP_INITSCHEDULER_H__

#include "Bootstrap/InitTaskId.h"   // EInitTask (task 식별자 = 우선순위)

#include <functional>
#include <vector>

namespace TopdownShooter::Bootstrap
{
	/// @brief task 실행 친화도 -- T2 에선 분류만(직렬). 미래 T3 병렬 분기용.
	enum class EAffinity
	{
		Cpu, ///< CPU 전용(audio/physics/decode) -- 미래 병렬 후보.
		Gl,  ///< GL 객체 생성 포함 -- 메인스레드 직렬 강제.
	};

	/// @brief 단일 초기화 단계 -- id / 선행의존 / affinity / 실행 람다.
	struct InitTask
	{
		EInitTask                Id;       ///< 식별자 = 우선순위 (EInitTask 값).
		std::vector<EInitTask>   Deps;     ///< 선행 task 목록 (위상정렬 간선).
		EAffinity                Affinity = EAffinity::Cpu; ///< 분류만 (T2 직렬).
		std::function<bool()>    Run;      ///< 실제 init. 성공=true, 실패=false(F-2 fail-fast).
	};

	class InitTaskBuilder; // fwd

	/// @brief task 그래프를 모아 검증 후 위상정렬 직렬 실행하는 스케줄러.
	class InitScheduler
	{
	  public:
		/// @brief @p id task 등록 빌더 시작. @c .Needs(...).Gl().Does([]{...}) 체이닝.
		/// @param id task 식별자(= 우선순위).
		/// @return 체이닝용 빌더 (rvalue 로 즉시 소비).
		InitTaskBuilder Task(EInitTask id);

		/// @brief 검증 -> Kahn 위상정렬 -> 직렬 실행. 그래프 오류/실행 실패 시 하드에러(abort).
		void RunAll();

	  private:
		friend class InitTaskBuilder;
		void AddTask(InitTask task);      ///< 빌더 Does() 가 확정 등록.
		void Validate() const;            ///< 중복id/미등록dep -> abort (사이클은 ExecuteInOrder 에서).
		void ExecuteInOrder();            ///< Kahn 직렬(enum값 tiebreak) + 사이클 abort + F-2.

		std::vector<InitTask> mTasks;     ///< 등록 순서 보존 (tiebreak 는 enum 값).
	};

	/// @brief InitScheduler 등록 fluent 빌더 -- task 를 값 보유, Does() 에서 commit (댕글링 방지).
	/// @details designated initializer(C++20) 미지원 환경 대응 -- fluent 가 프로젝트 정통(Tweeny/UniformAtlas).
	class InitTaskBuilder
	{
	  public:
		/// @brief @p sched 에 등록될 @p id task 빌드 시작.
		InitTaskBuilder(InitScheduler &sched, EInitTask id);

		/// @brief 선행 의존 task 목록 지정.
		/// @param deps 먼저 완료되어야 하는 task 들.
		InitTaskBuilder &Needs(std::vector<EInitTask> deps);
		/// @brief affinity = Gl (메인스레드 직렬 강제).
		InitTaskBuilder &Gl();
		/// @brief affinity = Cpu (기본, 미래 병렬 후보).
		InitTaskBuilder &Cpu();
		/// @brief 실행 람다 지정 + 스케줄러에 commit. 빌더 체인의 종단.
		/// @param run 성공 시 true 반환 (false 면 RunAll 이 fail-fast abort).
		void Does(std::function<bool()> run);

	  private:
		InitScheduler &mSched;
		InitTask        mTask;
	};
}

#endif // _TOPDOWNSHOOTER_BOOTSTRAP_INITSCHEDULER_H__
