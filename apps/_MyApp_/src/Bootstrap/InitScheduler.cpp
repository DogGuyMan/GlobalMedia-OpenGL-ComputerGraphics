/**
 * @file InitScheduler.cpp
 * @brief InitScheduler 구현 -- 검증(중복/누락) + Kahn 위상정렬 직렬 실행(등록순 tiebreak + F-2).
 *
 * @details
 *  Kahn 위상정렬을 *등록 순서 우선* 으로 구현 (노드 ~40 이라 O(V^2) 무시) :
 *  매 스텝 등록 순서대로 첫 "모든 dep 완료" task 를 선택 -> 실행 -> 완료 표시.
 *  in-degree 0 후보가 없는데 미완 task 가 남으면 사이클 (Validate 1차 가드 + 방어적 재확인).
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
	InitTaskBuilder::InitTaskBuilder(InitScheduler &sched, std::string id)
	    : mSched(sched)
	{
		mTask.Id = std::move(id);
	}

	InitTaskBuilder &InitTaskBuilder::Needs(std::vector<std::string> deps)
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
	InitTaskBuilder InitScheduler::Task(std::string id)
	{
		return InitTaskBuilder(*this, std::move(id));
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
		std::unordered_set<std::string> ids;
		ids.reserve(mTasks.size());
		for (const auto &t : mTasks)
		{
			if (!ids.insert(t.Id).second)
			{
				spdlog::critical("[InitScheduler] 중복 task id '{}'", t.Id);
				std::abort();
			}
		}

		// 2. 누락 dep -- 선언 안 된 task 를 의존.
		for (const auto &t : mTasks)
			for (const auto &d : t.Deps)
				if (ids.find(d) == ids.end())
				{
					spdlog::critical("[InitScheduler] task '{}' 가 미선언 dep '{}' 의존", t.Id, d);
					std::abort();
				}
	}

	void InitScheduler::ExecuteInOrder()
	{
		const std::size_t n = mTasks.size();

		// id -> 등록 인덱스 (dep 완료 조회용). Validate 통과라 모든 dep 은 존재 보장.
		std::unordered_map<std::string, std::size_t> indexOf;
		indexOf.reserve(n);
		for (std::size_t i = 0; i < n; ++i)
			indexOf[mTasks[i].Id] = i;

		std::vector<bool> done(n, false);
		std::size_t       completed = 0;

		while (completed < n)
		{
			// 등록 순서대로 "모든 dep 완료" 인 첫 미완 task 선택 (결정성 tiebreak).
			std::size_t pick = n;
			for (std::size_t i = 0; i < n; ++i)
			{
				if (done[i])
					continue;
				bool depsReady = true;
				for (const auto &d : mTasks[i].Deps)
					if (!done[indexOf[d]])
					{
						depsReady = false;
						break;
					}
				if (depsReady)
				{
					pick = i;
					break;
				}
			}

			// 후보 없음 + 미완 잔존 = 사이클.
			if (pick == n)
			{
				spdlog::critical("[InitScheduler] 의존 사이클 -- 남은 {} task 실행 불가", n - completed);
				for (std::size_t i = 0; i < n; ++i)
					if (!done[i])
						spdlog::critical("  미완 노드: '{}'", mTasks[i].Id);
				std::abort();
			}

			// 실행 (F-2 fail-fast).
			const InitTask &task = mTasks[pick];
			spdlog::debug("[InitScheduler] run '{}'", task.Id);
			const bool ok = task.Run ? task.Run() : true;
			if (!ok)
			{
				spdlog::critical("[InitScheduler] task '{}' 실패로 init 중단 (F-2 fail-fast)", task.Id);
				std::abort();
			}

			done[pick] = true;
			++completed;
		}
	}
}
