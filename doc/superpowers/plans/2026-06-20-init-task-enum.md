# InitScheduler task-ID enum 화 Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `InitScheduler` 의 task 식별자를 stringly-typed 에서 `enum class EInitTask` 로 전환 — 매직스트링 제거 + dep 오타 컴파일 차단 + enum 값을 우선순위 tiebreak 로 사용. `.Needs(...)` 위상정렬은 보존.

**Architecture:** 신규 `InitTaskId.h`(enum + inline ToString)를 단일 task 카탈로그로 두고, `InitScheduler` 의 `InitTask.Id`/`Deps` 와 빌더/스케줄러 시그니처를 `EInitTask` 로 교체. `ExecuteInOrder` 의 tiebreak 를 등록순에서 `static_cast<int>(Id)`(enum 값) 최소로 변경. `main.cpp` 의 7 task 호출부를 enum 으로. 스케줄러는 클라 enum 에 결합(템플릿 제네릭 아님 — 엔진 승격 시 YAGNI).

**Tech Stack:** C++17 (MSVC 크로스 — C++20 금지), CMake + Ninja(현재 vcpkg 툴체인), spdlog. 정본 spec = [`doc/superpowers/specs/2026-06-20-init-task-enum-design.md`](2026-06-20-init-task-enum-design.md) (E-1~E-5).

---

## 검증 철학 (이 프로젝트 한정)

`no_auto_tests` 가드레일 + 그래픽스 도메인 -> 검증 = **빌드 GREEN**(에이전트) + **GUI 육안**(사용자). TDD red-green 미강제. 본 변경은 spec §6 에서 **거동 무변경이 증명됨**(현 7-task 그래프는 deps 가 전 task 를 직렬화 -> tiebreak 가 실제 발화 안 함 -> 실행 순서 100% 불변). 따라서 GUI 는 sanity 확인.

## 가드레일 / 컨벤션

- **커밋 = 사용자 게이트**: 각 task 커밋 step 은 *제안*. 에이전트는 커밋/`git add` 하지 않는다. `git add -A` 금지. ⚠ `main.cpp` 는 현재 init-scheduler 리팩토링 + orbital 수정 + 사용자의 `Playable::` 한정 drift 가 **모두 미커밋으로 intertwined** — 커밋 granularity 는 사용자가 관리. `Co-Authored-By` 미사용.
- **코드 컨벤션**: 주석 한국어 + ASCII/한글만 (화살표 `->` ASCII, 유니코드 화살표 금지). Tab indent. 헤더가드 `_TOPDOWNSHOOTER_<...>_H__` · `#pragma once` 금지 · `#endif` 주석 일치. `long` 금지. 멤버 `m`PascalCase / 지역 camelCase. Doxygen 스타일 주변 일치.
- **C++17만**: designated init·concepts·`<ranges>` 금지.
- **불가침**: `extern/sb7code` / stb owner / FMOD 가드 등 무관(미접근). CMake 변경 0.

## Pre-flight (구현 착수 직전 1회)

- [ ] `git status --short` + `git log --oneline -3` 재측정. HEAD 가 `9123dae` 계열인지, working tree 에 init-scheduler 미커밋분이 있는지 확인.
- [ ] `cmake --build --preset ninja --target _MyApp_` 로 **현재 baseline 이 GREEN 인지** 먼저 확인 (vcpkg 툴체인 정상 동작 전제). 실패 시 빌드 환경부터 사용자 확인.
- [ ] `grep -n 'sched.Task(' apps/_MyApp_/main.cpp` 로 7 호출부 라인 재확인 (drift 시 라인번호만 이동, 내용은 동일).

---

## File Structure

- **신규** `apps/_MyApp_/src/Bootstrap/InitTaskId.h` — `EInitTask` enum(값=우선순위) + inline `ToString`. task 카탈로그(데이터), 스케줄러 메커니즘과 분리.
- **수정** `apps/_MyApp_/src/Bootstrap/InitScheduler.h` — `InitTask.Id`/`Deps` + 빌더/스케줄러 시그니처를 `EInitTask` 로. `InitTaskId.h` include, `<string>` 제거.
- **수정** `apps/_MyApp_/src/Bootstrap/InitScheduler.cpp` — ctor/Needs/Task enum, Validate/ExecuteInOrder 를 int-cast 키 + `ToString` 진단 + **enum 값 tiebreak**.
- **수정** `apps/_MyApp_/main.cpp` — 7 `sched.Task("...")`/`.Needs({"..."})` 를 `Bootstrap::EInitTask::*` 로.
- **CMake**: 변경 없음 (InitTaskId.h 헤더온리, InitScheduler.cpp 기존 소스 유지).

---

## Task 1: InitTaskId.h — EInitTask 카탈로그 (신규)

독립 컴파일 단위. 아직 아무도 include 안 하므로 빌드 거동 불변.

**Files:**
- Create: `apps/_MyApp_/src/Bootstrap/InitTaskId.h`

- [ ] **Step 1: 헤더 작성** — `apps/_MyApp_/src/Bootstrap/InitTaskId.h` 에 정확히:

```cpp
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
```

- [ ] **Step 2: 문법 검증**

Run: `clang++ -std=c++17 -fsyntax-only apps/_MyApp_/src/Bootstrap/InitTaskId.h`
Expected: 에러 0 (`.h` 확장자 경고는 무해). g++ 도 가능. 헤더온리라 전체 빌드는 불변 — 굳이 빌드 안 해도 됨.

- [ ] **Step 3: 자체 점검**

- 헤더가드 `_TOPDOWNSHOOTER_BOOTSTRAP_INITTASKID_H__` + `#endif` 주석 일치, `#pragma once` 없음?
- ASCII+한글만, 유니코드 화살표 없음, Tab indent?
- enum 7값 순서 = Core/ScreenPipeline/VfxUi/World/Stages/Enter/Fsm (실행 순서)?

- [ ] **Step 4: 커밋 (사용자 게이트 — 제안만)**

```bash
git add apps/_MyApp_/src/Bootstrap/InitTaskId.h
git commit -m "[refactor] : InitTaskId enum 카탈로그 추가 (task-ID 우선순위)"
```
사용자 승인 전 커밋 금지.

---

## Task 2: InitScheduler + main.cpp 를 EInitTask 로 마이그레이션

`InitScheduler.h`/`.cpp` API 와 `main.cpp` 호출부는 함께 바뀌어야 컴파일된다 (한 task 로 묶음, 빌드는 마지막에 GREEN 확인).

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/InitScheduler.h` (전체 교체)
- Modify: `apps/_MyApp_/src/Bootstrap/InitScheduler.cpp` (전체 교체)
- Modify: `apps/_MyApp_/main.cpp` (7 호출부)

- [ ] **Step 1: `InitScheduler.h` 전체 교체** — 다음 내용으로:

```cpp
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

#include "apps/_MyApp_/src/Bootstrap/InitTaskId.h"   // EInitTask (task 식별자 = 우선순위)

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
```
(변경 요점: `#include "apps/_MyApp_/src/Bootstrap/InitTaskId.h"` 추가, `<string>` 제거, `Id`/`Deps`/`Task`/`Needs`/빌더 ctor 가 `EInitTask`, tiebreak 주석 갱신.)

- [ ] **Step 2: `InitScheduler.cpp` 전체 교체** — 다음 내용으로:

```cpp
/**
 * @file InitScheduler.cpp
 * @brief InitScheduler 구현 -- 검증(중복/미등록) + Kahn 위상정렬 직렬 실행(enum값 tiebreak + F-2).
 *
 * @details
 *  Kahn 위상정렬 (노드 ~7 이라 O(V^2) 무시) :
 *  매 스텝 "모든 dep 완료" task 중 @c EInitTask 값(우선순위) 최소를 선택 -> 실행 -> 완료 표시.
 *  ready 후보가 없는데 미완 task 가 남으면 사이클 (Validate 1차 가드 + 방어적 재확인).
 */
#include "apps/_MyApp_/src/Bootstrap/InitScheduler.h"

#include <<spdlog>/spdlog.h>

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
```
(변경 요점: ctor/Needs/Task 가 `EInitTask`, Validate/ExecuteInOrder 의 set/map 이 `int`(=`static_cast<int>(Id)`), 진단 로그가 `ToString(...)`, **tiebreak 가 `break`-on-first 에서 enum 값 최소 비교로**.)

- [ ] **Step 3: `main.cpp` 7 호출부 교체**

각 `sched.Task("...")` / `.Needs({"..."})` 를 `Bootstrap::EInitTask::*` 로. (main.cpp 는 `namespace TopdownShooter` 안이라 `Bootstrap::` 한정 필요. `InitScheduler.h` 가 `InitTaskId.h` 를 전파하므로 추가 include 불요.) 7개 정확히:

```cpp
// OnResourcesReady
sched.Task(Bootstrap::EInitTask::Core).Gl().Does([&] {
```
```cpp
// OnSceneSetup
sched.Task(Bootstrap::EInitTask::ScreenPipeline).Gl().Does([&] {
```
```cpp
sched.Task(Bootstrap::EInitTask::World).Needs({Bootstrap::EInitTask::VfxUi}).Gl().Does([&] {  // vfxUi 가 TEST_EFFECTS(orbital_background) 를 선행 로드 -> 스테이지 FindEffect 의존
```
```cpp
sched.Task(Bootstrap::EInitTask::Stages).Needs({Bootstrap::EInitTask::ScreenPipeline, Bootstrap::EInitTask::World}).Cpu().Does([&] {
```
```cpp
sched.Task(Bootstrap::EInitTask::VfxUi).Needs({Bootstrap::EInitTask::ScreenPipeline}).Gl().Does([&] {
```
```cpp
// OnBeforeFirstFrame
sched.Task(Bootstrap::EInitTask::Enter).Gl().Does([&] {
```
```cpp
sched.Task(Bootstrap::EInitTask::Fsm).Needs({Bootstrap::EInitTask::Enter}).Gl().Does([&] {
```
방법: 각 호출부의 문자열 부분만 교체 (interior substring 매칭 권장 — `"core"` -> `Bootstrap::EInitTask::Core`, `Task("world").Needs({"vfxUi"})` -> `Task(Bootstrap::EInitTask::World).Needs({Bootstrap::EInitTask::VfxUi})` 등). 람다 본문/주석/`.Gl()`/`.Cpu()`/`.Does()` 무변경. world 의 orbital 주석은 그대로 유지.

- [ ] **Step 4: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: **에러 0**. 흔한 실패: (a) `Bootstrap::` 한정 누락 -> `EInitTask 미정의` (main.cpp 는 TopdownShooter 네임스페이스라 `Bootstrap::` 필수). (b) `.Needs({...})` 안에 문자열 잔존 -> 타입 불일치. (c) `<string>` 제거 후 다른 곳에서 string 사용 -> 다시 추가(없어야 정상).
검증: `grep -n 'sched.Task("' apps/_MyApp_/main.cpp` = **0건** (모든 문자열 ID 소멸).

- [ ] **Step 5: GUI 육안 검증 (사용자)**

`cd build_ninja/apps/_MyApp_ && ./_MyApp_`. spec §6 상 실행 순서 100% 불변(tiebreak inert) — 전체 init 거동 + orbital 배경 + 라이팅/스프라이트/PostFX/FSM 모두 이전과 동일해야 정상. 콘솔 `[InitScheduler] run 'Core'/'ScreenPipeline'/...` 로그가 이제 enum 이름으로 출력.

- [ ] **Step 6: 커밋 (사용자 게이트 — 제안만)**

```bash
git add apps/_MyApp_/src/Bootstrap/InitScheduler.h apps/_MyApp_/src/Bootstrap/InitScheduler.cpp apps/_MyApp_/main.cpp
git commit -m "[refactor] : InitScheduler task-ID 를 EInitTask enum 으로 (타입안전 + 우선순위 tiebreak)"
```
⚠ `main.cpp` 는 사용자의 `Playable::` 한정 drift + init-scheduler 미커밋분과 intertwined — **사용자가 staging granularity 직접 관리**. 에이전트는 커밋 금지.

---

## Self-Review (spec 대비)

**1. Spec coverage:**
- E-1 우선순위=tiebreak(deps 유지) -> Task 2 Step 2 `ExecuteInOrder` enum값 tiebreak + `.Needs` 위상정렬 보존 ✅
- E-2 enum-결합(A) -> 모든 task 가 `EInitTask` 직접 사용(템플릿 없음) ✅
- E-3 scope=task-ID enum 만 -> InitScheduler + main.cpp 7 호출부만 ✅
- E-4 별도 `InitTaskId.h` -> Task 1 ✅
- E-5 enum 순서=실행 순서 -> Task 1 enum 선언(Core..Fsm) ✅
- §6 거동 무변경 -> Task 2 Step 5 (tiebreak inert, GUI sanity) ✅

**2. Placeholder scan:** "TBD"/추상 단계 없음. 전체 파일 코드 제공. ✅

**3. Type consistency:** `EInitTask` / `ToString` / `Task(EInitTask)` / `Needs(std::vector<EInitTask>)` / 빌더 ctor `(InitScheduler&, EInitTask)` / `static_cast<int>(Id)` tiebreak — Task 1·2 전반 일관. `InitTaskId.h` include 경로 `"apps/_MyApp_/src/Bootstrap/InitTaskId.h"` 일치. ✅
