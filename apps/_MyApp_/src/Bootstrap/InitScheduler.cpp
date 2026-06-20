/**
 * @file InitScheduler.cpp
 * @brief InitScheduler 구현 -- 검증(중복/미등록) + Kahn 위상정렬 직렬 실행(enum값 tiebreak + F-2).
 *
 * @details
 *  Kahn 위상정렬 (노드 ~7 이라 O(V^2) 무시) :
 *  매 스텝 "모든 dep 완료" task 중 @c EInitTask 값(우선순위) 최소를 선택 -> 실행 -> 완료 표시.
 *  ready 후보가 없는데 미완 task 가 남으면 사이클 (Validate 1차 가드 + 방어적 재확인).
 */
#include "Bootstrap/InitScheduler.h"

#include <spdlog/spdlog.h>

#include <cstdlib>        // std::abort
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace TopdownShooter::Bootstrap
{
	// ---- InitTaskBuilder ----------------------------------------------------
	InitTaskBuilder::InitTaskBuilder(InitScheduler &sched, EInitTask id)
	    : mSched(sched)
	{
		mTask.Id = id;
	}

	InitTaskBuilder &InitTaskBuilder::Needs(std::vector<EInitTask> deps)
	{
		mTask.Deps = std::move(deps);
		return *this;
	}

	InitTaskBuilder &InitTaskBuilder::Gl()
	{
		mTask.Affinity = EAffinity::Gl;
		return *this;
	}

	InitTaskBuilder &InitTaskBuilder::Cpu()
	{
		mTask.Affinity = EAffinity::Cpu;
		return *this;
	}

	void InitTaskBuilder::Does(std::function<bool()> run)
	{
		mTask.Run = std::move(run);
		mSched.AddTask(std::move(mTask));
	}

	// ---- InitScheduler ------------------------------------------------------
	InitTaskBuilder InitScheduler::Task(EInitTask id)
	{
		return InitTaskBuilder(*this, id);
	}

	void InitScheduler::AddTask(InitTask task)
	{
		mTasks.push_back(std::move(task));
	}

	void InitScheduler::RunAll()
	{
		Validate();
		ExecuteInOrder();
	}

	void InitScheduler::Validate() const
	{
		// 1. 중복 id.
		std::unordered_set<int> ids;
		ids.reserve(mTasks.size());
		for (const auto &t : mTasks)
		{
			if (!ids.insert(static_cast<int>(t.Id)).second)
			{
				spdlog::critical("[InitScheduler] 중복 task id '{}'", ToString(t.Id));
				std::abort();
			}
		}

		// 2. 미등록 dep -- 이 스케줄러에 등록 안 된 task 를 의존 (cross-phase 오선언 가드).
		for (const auto &t : mTasks)
			for (const auto &d : t.Deps)
				if (ids.find(static_cast<int>(d)) == ids.end())
				{
					spdlog::critical("[InitScheduler] task '{}' 가 미등록 dep '{}' 의존", ToString(t.Id), ToString(d));
					std::abort();
				}
	}

	void InitScheduler::ExecuteInOrder()
	{
		const std::size_t n = mTasks.size();

		// id -> 등록 인덱스 (dep 완료 조회용). Validate 통과라 모든 dep 은 존재 보장.
		std::unordered_map<int, std::size_t> indexOf;
		indexOf.reserve(n);
		for (std::size_t i = 0; i < n; ++i)
			indexOf[static_cast<int>(mTasks[i].Id)] = i;

		std::vector<bool> done(n, false);
		std::size_t       completed = 0;

		while (completed < n)
		{
			// "모든 dep 완료" 인 미완 task 중 EInitTask 값(우선순위) 최소 선택 (결정성 tiebreak).
			std::size_t pick = n;
			for (std::size_t i = 0; i < n; ++i)
			{
				if (done[i])
					continue;
				bool depsReady = true;
				for (const auto &d : mTasks[i].Deps)
					if (!done[indexOf[static_cast<int>(d)]])
					{
						depsReady = false;
						break;
					}
				if (!depsReady)
					continue;
				if (pick == n || static_cast<int>(mTasks[i].Id) < static_cast<int>(mTasks[pick].Id))
					pick = i;
			}

			// 후보 없음 + 미완 잔존 = 사이클.
			if (pick == n)
			{
				spdlog::critical("[InitScheduler] 의존 사이클 -- 남은 {} task 실행 불가", n - completed);
				for (std::size_t i = 0; i < n; ++i)
					if (!done[i])
						spdlog::critical("  미완 노드: '{}'", ToString(mTasks[i].Id));
				std::abort();
			}

			// 실행 (F-2 fail-fast).
			const InitTask &task = mTasks[pick];
			spdlog::debug("[InitScheduler] run '{}'", ToString(task.Id));
			const bool ok = task.Run ? task.Run() : true;
			if (!ok)
			{
				spdlog::critical("[InitScheduler] task '{}' 실패로 init 중단 (F-2 fail-fast)", ToString(task.Id));
				std::abort();
			}

			done[pick] = true;
			++completed;
		}
	}
}
