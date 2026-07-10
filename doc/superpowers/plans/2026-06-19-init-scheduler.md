# InitScheduler — 데이터주도 topo-sort 초기화 아키텍처 Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `apps/_MyApp_`의 수동 위상정렬된 `startup()`(~40노드 의존 DAG)를 데이터주도 topo-sort 초기화(`InitScheduler`)로 전환하고, 동시에 `render -> resource_registry` 모듈 사이클을 절단(완전 DAG화)하며 전역 싱글톤 2건(PostFXRegistry 흡수 / Manager 개명)을 정리한다.

**Architecture:** 클라 `Bootstrap/`에 거주하는 제네릭 `InitScheduler`(`(id, deps, affinity, fn)` task 등록 -> 검증 -> Kahn 위상정렬 -> 직렬 실행, F-2 fail-fast)를 핵심으로, `EngineBootstrap` 템플릿메서드가 3개 클라 hook(`OnResourcesReady`/`OnSceneSetup`/`OnBeforeFirstFrame`)을 순차 호출하고 각 hook이 phase-local 스케줄러를 돌린다. 파이프라인 조립 함수는 신규 엔진 모듈 `src/render_bootstrap/`로 이주해 `render -> rr` include를 끊고, SceneRenderer의 program 목록 pull을 외부 push로 뒤집는다.

**Tech Stack:** C++17 (MSVC 크로스 — C++20 금지), CMake + Ninja, OpenGL 4.1 / GLSL 410, spdlog(로깅), sb7 application 프레임워크. 정본 spec = [`doc/superpowers/specs/2026-06-19-init-scheduler-design.md`](2026-06-19-init-scheduler-design.md) (D-1~D-8 + §11).

---

## 검증 철학 (이 프로젝트 한정 — 스킬 기본 TDD 대체)

이 프로젝트는 **사용자 가드레일 `no_auto_tests`**(단위 테스트는 *요청 시에만*)와 그래픽스 도메인 특성상, 각 task의 검증은 **빌드 GREEN + GUI 육안**이 정본이다. writing-plans 스킬의 red-green TDD를 강제하지 않는다.

- **빌드 검증(모든 task)**: `cmake --build --preset ninja --target _MyApp_` -> **에러 0**.
- **GUI 육안(런타임 변동 task: 3/5/6/7)**: 사용자가 `cd build_ninja/apps/_MyApp_ && ./_MyApp_` 실행해 라이팅/스프라이트/PostFX/스테이지/발사 정상 확인. (에이전트는 GL 컨텍스트가 없어 실행 불가 — 사용자 위탁.)
- **Task 1(InitScheduler)** 은 순수 CPU 로직이라 단위 테스트가 가치 있으나, `no_auto_tests`로 **기본 미작성**. 거동 검증은 Task 5 실제 그래프 통합 + 콘솔 `spdlog::debug` 실행 순서 로그로 갈음. (사용자가 원하면 별도 요청 시 Catch2 테스트 추가.)

## 가드레일 / 컨벤션 (필수 준수)

- **커밋 = 사용자 게이트**: 각 task의 커밋 step은 *제안*이다. 에이전트는 커밋하지 않고 사용자 승인/실행을 기다린다. **`git add -A` 절대 금지** (사용자가 같은 워킹트리에서 vcpkg-migration을 병렬 staging 중 — `M .gitmodules` / `A extern/vcpkg` / `?? vcpkg.json` 가 인덱스에 올라가 있음). 반드시 **path-scoped** `git commit <경로...> -m "..."`. **`Co-Authored-By` 트레일러 미사용.**
- **불가침**: `extern/sb7code` 수정 금지. stb `STB_IMAGE_IMPLEMENTATION` 단일 owner=`src/texture/image.cpp` (건드리지 말 것). FMOD `SJH_HAS_FMOD` 가드 보존. rr `game_deps` PUBLIC 불변.
- **코드 컨벤션**: 주석 한국어 + **ASCII/한글만** (특수문자 0, 화살표는 `->`만, 유니코드 화살표 금지). 헤더가드 `_TOPDOWNSHOOTER_<...>_H__` (클라) / `__SJH_<MODULE>_<NAME>_H__` (엔진), **`#pragma once` 금지**, `#endif` 주석은 가드 매크로와 일치. **Tab indent**. `long` 금지 -> 고정폭(`int32_t` 등). 멤버 `m`+PascalCase / 지역 camelCase / bool `mIs*` / 포인터 접근 `*Ptr`. Doxygen 주석 스타일은 주변 코드(`src/render`)와 일치.
- **C++17만**: designated initializer(`{.Id=}`), concepts, `<ranges>` 등 C++20 금지 -> fluent builder 사용(D-6).
- **vcpkg 병렬 주의**: 사용자가 `CMakePresets.json`을 vcpkg로 마이그레이션 중(IDE에 열려 있음). 각 task 빌드 전 `cmake --list-presets`로 `ninja` 프리셋 존재를 재확인하고, 빌드 명령이 바뀌었으면 사용자에게 확인. 내 신규 모듈 `src/render_bootstrap/`은 내부 STATIC이라 vcpkg(외부 의존 획득)와 직교 — 충돌 0.

## Pre-flight (구현 착수 직전 1회)

- [ ] `git status --short` + `git log --oneline -4` 재측정. HEAD가 `cadf80b`에서 진행했으면 변경 파일이 내 대상과 겹치는지 확인 (사용자 병렬 webeditor/vcpkg는 별개 파일).
- [ ] `cmake --list-presets` 로 `ninja` 프리셋 확인. 없으면(vcpkg 전환됨) 사용자에게 새 빌드 명령 확인.
- [ ] `cmake --preset ninja` 1회 configure (이후 task는 `--build`만).

---

## File Structure

**신규 생성:**
- `apps/_MyApp_/src/Bootstrap/InitScheduler.h` + `.cpp` — 제네릭 데이터주도 스케줄러 (Task 1).
- `apps/_MyApp_/src/Bootstrap/EngineBootstrap.h` — 템플릿메서드 부트 스켈레톤 + `IClientBootstrap` 3 hook (Task 4, 헤더 온리).
- `<src>/render_bootstrap/render_pipeline.h` + `.cpp` + `CMakeLists.txt` — `src/render/`에서 이주 (Task 2).

**수정:**
- `src/render/CMakeLists.txt` — `render_pipeline.cpp` 소스 제거 (Task 2).
- `src/CMakeLists.txt` — `add_subdirectory(render_bootstrap)` + 우산 `SJH::render_bootstrap` (19모듈) (Task 2).
- `apps/_MyApp_/main.cpp` — include 2곳 변경(Task 2), render 루프 push(Task 3), startup() 재구성(Task 4/5).
- `<apps>/_MyApp_/src/Playable/PostFXConstants.h:23` — include 변경 (Task 2).
- `src/render/scene_renderer.{h,cpp}` — `SetActivePrograms` setter + rr pull 제거 (Task 3).
- `apps/_MyApp_/src/Bootstrap/CMakeLists.txt` — `InitScheduler.cpp` 소스 추가 (Task 1).
- `apps/_MyApp_/src/Playable/PostFXTweenPlayable.cpp` + `HpGrayscalePostFX.cpp` + `main.cpp` — PostFXRegistry -> rr.FindSharedMaterial (Task 6).
- D-8 rename 대상 11파일 (Task 7).

**삭제(Task 6):** `apps/_MyApp_/src/Playable/PostFXRegistry.{h,cpp}` + Playable/CMakeLists.txt 소스 라인.

---

## Task 1: InitScheduler 제네릭 유틸 (신규, 클라 Bootstrap/)

데이터 모델 + fluent 빌더 + 검증 + Kahn 직렬 실행. 독립 빌드 단위 (거동 변화 없음 — 아직 미사용).

**Files:**
- Create: `apps/_MyApp_/src/Bootstrap/InitScheduler.h`
- Create: `apps/_MyApp_/src/Bootstrap/InitScheduler.cpp`
- Modify: `apps/_MyApp_/src/Bootstrap/CMakeLists.txt`

- [ ] **Step 1: 헤더 작성** — `apps/_MyApp_/src/Bootstrap/InitScheduler.h`

```cpp
/**
 * @file InitScheduler.h
 * @brief 데이터주도 초기화 스케줄러 -- (id, deps, affinity, fn) task 를 등록하면
 *        위상정렬(Kahn) 후 직렬 실행. startup() 의 수동 위상정렬을 선언적 그래프로 대체.
 *
 * @details
 *  ### 책임
 *  - @c Task(id) fluent 빌더로 init 단계를 (선행 의존 + affinity + 실행 람다) 로 선언.
 *  - @c RunAll : 검증(중복id/누락dep/사이클 하드에러) -> Kahn 위상정렬 -> 직렬 실행(F-2 fail-fast).
 *  ### 비-책임
 *  - [X] 병렬 실행 -- T2 직렬만. @c EAffinity 는 분류만 (미래 T3 게이트, GL 단일스레드 제약).
 *  - [X] 엔진 hook 타이밍 -- @c EngineBootstrap / 클라가 RunAll 호출 시점 결정.
 *  ### 결정성
 *  - 같은 위상 레벨이 여럿이면 *등록 순서* 로 tiebreak -> 실행 순서 재현 가능 (GL 상태 결정성).
 *
 * @note 재사용 demo 생기면 엔진 모듈 @c SJH::init 로 승격 가능 (현재 YAGNI -- 클라 거주).
 */
#ifndef _TOPDOWNSHOOTER_BOOTSTRAP_INITSCHEDULER_H__
#define _TOPDOWNSHOOTER_BOOTSTRAP_INITSCHEDULER_H__

#include <functional>
#include <string>
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
		std::string              Id;       ///< 고유 식별자 (topo.dot 노드명).
		std::vector<std::string> Deps;     ///< 선행 task id 목록 (topo.dot 간선).
		EAffinity                Affinity = EAffinity::Cpu; ///< 분류만 (T2 직렬).
		std::function<bool()>    Run;      ///< 실제 init. 성공=true, 실패=false(F-2 fail-fast).
	};

	class InitTaskBuilder; // fwd

	/// @brief task 그래프를 모아 검증 후 위상정렬 직렬 실행하는 스케줄러.
	class InitScheduler
	{
	  public:
		/// @brief @p id 로 새 task 등록 빌더 시작. @c .Needs(...).Gl().Does([]{...}) 체이닝.
		/// @param id 고유 task 식별자.
		/// @return 체이닝용 빌더 (rvalue 로 즉시 소비).
		InitTaskBuilder Task(std::string id);

		/// @brief 검증 -> Kahn 위상정렬 -> 직렬 실행. 그래프 오류/실행 실패 시 하드에러(abort).
		void RunAll();

	  private:
		friend class InitTaskBuilder;
		void AddTask(InitTask task);      ///< 빌더 Does() 가 확정 등록.
		void Validate() const;            ///< 중복id/누락dep -> abort (사이클은 ExecuteInOrder 에서).
		void ExecuteInOrder();            ///< Kahn 직렬(등록순 tiebreak) + 사이클 abort + F-2.

		std::vector<InitTask> mTasks;     ///< 등록 순서 보존 (tiebreak 결정성).
	};

	/// @brief InitScheduler 등록 fluent 빌더 -- task 를 값 보유, Does() 에서 commit (댕글링 방지).
	/// @details designated initializer(C++20) 미지원 환경 대응 -- fluent 가 프로젝트 정통(Tweeny/UniformAtlas).
	class InitTaskBuilder
	{
	  public:
		/// @brief @p sched 에 등록될 @p id task 빌드 시작.
		InitTaskBuilder(InitScheduler &sched, std::string id);

		/// @brief 선행 의존 task id 목록 지정.
		/// @param deps 먼저 완료되어야 하는 task id 들.
		InitTaskBuilder &Needs(std::vector<std::string> deps);
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

- [ ] **Step 2: 구현 작성** — `apps/_MyApp_/src/Bootstrap/InitScheduler.cpp`

```cpp
/**
 * @file InitScheduler.cpp
 * @brief InitScheduler 구현 -- 검증(중복/누락) + Kahn 위상정렬 직렬 실행(등록순 tiebreak + F-2).
 *
 * @details
 *  Kahn 위상정렬을 *등록 순서 우선* 으로 구현 (노드 ~40 이라 O(V^2) 무시) :
 *  매 스텝 등록 순서대로 첫 "모든 dep 완료" task 를 선택 -> 실행 -> 완료 표시.
 *  in-degree 0 후보가 없는데 미완 task 가 남으면 사이클 (Validate 1차 가드 + 방어적 재확인).
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
```

- [ ] **Step 3: CMake 소스 추가** — `apps/_MyApp_/src/Bootstrap/CMakeLists.txt` 의 `add_library(myapp_bootstrap STATIC ...)` 목록에 `InitScheduler.cpp` 추가.

기존:
```cmake
add_library(myapp_bootstrap STATIC
    WorldSceneBuilder.cpp
    PlayerBuilder.cpp
    AudioWarmup.cpp
    EnemyBuilder.cpp
    EntityPresentation.cpp   # Player/Enemy 공통 연출 클러스터 부착 헬퍼 (director+hit/death+deathDelay+healthbar)
)
```
변경 후:
```cmake
add_library(myapp_bootstrap STATIC
    WorldSceneBuilder.cpp
    PlayerBuilder.cpp
    AudioWarmup.cpp
    EnemyBuilder.cpp
    EntityPresentation.cpp   # Player/Enemy 공통 연출 클러스터 부착 헬퍼 (director+hit/death+deathDelay+healthbar)
    InitScheduler.cpp        # 데이터주도 topo-sort 초기화 (spec 2026-06-19)
)
```

`myapp_bootstrap` 은 이미 `spdlog` 를 PRIVATE link 하므로(현 CMakeLists 32행) `<<spdlog>/spdlog.h>` 해소됨 — 추가 link 불요.

- [ ] **Step 4: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 에러 0 (새 .cpp 컴파일됨, 아직 호출처 없음 -- 거동 불변).

- [ ] **Step 5: 커밋 (사용자 게이트 — 제안만)**

```bash
git commit apps/_MyApp_/src/Bootstrap/InitScheduler.h apps/_MyApp_/src/Bootstrap/InitScheduler.cpp apps/_MyApp_/src/Bootstrap/CMakeLists.txt -m "[refactor] : InitScheduler 데이터주도 topo-sort 초기화 유틸 추가"
```
사용자 승인 전 커밋 금지. `git add -A` 금지(병렬 vcpkg staging 보호).

---

## Task 2: render_bootstrap 모듈 이주 (D-2 — render->rr include 절단)

`render_pipeline.{h,cpp}`(+구조체 3종)를 `src/render/`에서 신규 엔진 모듈 `src/render_bootstrap/`로 이주. `render` 모듈은 더 이상 `resource_registry`를 include하지 않게 된다.

**Files:**
- Create: `<src>/render_bootstrap/render_pipeline.h` (= 기존 `<src>/render/render_pipeline.h` 내용 동일, 가드만 유지)
- Create: `<src>/render_bootstrap/render_pipeline.cpp` (= 기존 내용, include 1줄 변경)
- Create: `src/render_bootstrap/CMakeLists.txt`
- Modify: `src/render/CMakeLists.txt` (소스 1줄 제거 + PRIVATE link 정리)
- Modify: `src/CMakeLists.txt` (add_subdirectory + 우산)
- Modify: `apps/_MyApp_/main.cpp:50` (include)
- Modify: `<apps>/_MyApp_/src/Playable/PostFXConstants.h:23` (include)
- Delete: `<src>/render/render_pipeline.h` + `<src>/render/render_pipeline.cpp`

- [ ] **Step 1: 신규 모듈 디렉토리에 헤더 복사** — `<src>/render_bootstrap/render_pipeline.h`

기존 `<src>/render/render_pipeline.h` 전체를 그대로 복사한다. 헤더 가드 `__SJH_RENDER_PIPELINE_H__` 는 *변경하지 않는다* (모듈 디렉토리만 바뀌고 논리적 이름은 동일, 중복 정의 없음). 내용은 현재 파일과 1바이트도 다르지 않다 (forward decl + `SetupDefaultPipeline`/`BuildPostFXChain` + 구조체 `DefaultPipelineConfig`/`PostFXStageConfig`/`PostFXChainResult`). `#include "<buffer>/framebuffer.h"` 는 그대로 유효 (buffer 모듈 PUBLIC).

- [ ] **Step 2: 신규 모듈 디렉토리에 구현 복사 + include 1줄 변경** — `<src>/render_bootstrap/render_pipeline.cpp`

기존 `<src>/render/render_pipeline.cpp` 전체를 복사하되, **19행의 자기 헤더 include만 변경**:

기존:
```cpp
#include "<render>/render_pipeline.h"
```
변경:
```cpp
#include "<render_bootstrap>/render_pipeline.h"
```
나머지 include(`object/mesh.h`, `material/material.h`, `<render>/pass_component.h`, `<render>/scene_renderer.h`, `<render>/screen_quad_stage.h`, `resource_registry/resource_registry.h`, `scene/actor.h`, `scene/layer.h`, `<<spdlog>/spdlog.h>`)와 본문은 그대로.

- [ ] **Step 3: 신규 CMakeLists 작성** — `src/render_bootstrap/CMakeLists.txt`

```cmake
# render_bootstrap -- 파이프라인 조립 함수 (SetupDefaultPipeline / BuildPostFXChain).
# render core 밖 최상위 모듈 -- render 가 rr 을 include 하던 사이클을 끊는다 (spec D-2).
# render_pipeline 은 rr/render/object/material/scene 를 *구상 의존* 하지만, 자신은
# 아무도 역의존하지 않는 최상위라 신규 사이클이 생기지 않는다.
add_library(sjhopengl_render_bootstrap STATIC
    render_pipeline.cpp
)
add_library(SJH::render_bootstrap ALIAS sjhopengl_render_bootstrap)

target_include_directories(sjhopengl_render_bootstrap
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# render_pipeline.h(공개) 는 <buffer>/framebuffer.h 만 노출 -> SJH::buffer PUBLIC.
# 나머지(render/rr/object/material/scene) 는 .cpp 전용 -> PRIVATE.
# spdlog 는 SJH::resource_registry 의 PUBLIC game_deps 로 전파되어 .cpp 컴파일 시 해소.
target_link_libraries(sjhopengl_render_bootstrap
    PUBLIC  SJH::buffer
    PRIVATE SJH::render SJH::resource_registry SJH::object SJH::material SJH::scene
)

target_compile_features(sjhopengl_render_bootstrap PUBLIC cxx_std_17)
```

- [ ] **Step 4: render 모듈에서 소스 제거** — `src/render/CMakeLists.txt`

`add_library(sjhopengl_render STATIC ...)` 목록에서 `render_pipeline.cpp` 줄(11행)을 삭제한다.

기존:
```cmake
    camera_stage.cpp              # SP-RenderStage 완성 — 단일 Camera 를 IRenderStage 로 wrap
    render_pipeline.cpp           # SP-Warmup — Setup + BuildPostFXChain 자유 함수 family
    actor_factory.cpp             # 2026-06-11 E2 — Skybox/ScreenCamera 팩토리 (scene 에서 이주)
```
변경 후:
```cmake
    camera_stage.cpp              # SP-RenderStage 완성 — 단일 Camera 를 IRenderStage 로 wrap
    actor_factory.cpp             # 2026-06-11 E2 — Skybox/ScreenCamera 팩토리 (scene 에서 이주)
```

> ⚠ `src/render/CMakeLists.txt` 의 `target_link_libraries` 에서 `SJH::resource_registry` 는 **아직 제거하지 않는다**. `scene_renderer.cpp:123` 이 여전히 rr 을 쓰기 때문 — Task 3 에서 그 pull 을 제거한 뒤 함께 정리한다. (이 task 만으로 render 의 *컴파일* rr 의존이 사라지지 않음 — Task 2+3 합쳐야 완전 절단.)

- [ ] **Step 5: 우산 + add_subdirectory** — `src/CMakeLists.txt`

`add_subdirectory(render)` 다음 줄에 추가:
```cmake
add_subdirectory(render)
add_subdirectory(render_bootstrap)  # <- 추가 (D-2 — 파이프라인 조립, render->rr 절단)
```

우산 `target_link_libraries(sjhopengl_engine INTERFACE ...)` 의 `SJH::texture` 다음에 추가:
```cmake
    SJH::texture            # 18 모듈 — leaf GPU 자원 (2026-06-11 E3/E5/E6 사이클 해소)
    SJH::render_bootstrap   # 19 모듈 — 파이프라인 조립 (2026-06-19 render->rr 절단)
```

- [ ] **Step 6: 클라 include 2곳 변경**

`apps/_MyApp_/main.cpp:50`:
```cpp
#include "<render>/render_pipeline.h"
```
->
```cpp
#include "<render_bootstrap>/render_pipeline.h"
```

`<apps>/_MyApp_/src/Playable/PostFXConstants.h:23`:
```cpp
#include "<render>/render_pipeline.h" // SJH::Render::PostFXStageConfig
```
->
```cpp
#include "<render_bootstrap>/render_pipeline.h" // SJH::Render::PostFXStageConfig
```

> `apps/_MyApp_` 은 `SJH::engine` 우산을 link 하므로 새 모듈 include 경로가 자동 전파 — 클라 CMakeLists 수정 불요.

- [ ] **Step 7: 옛 파일 삭제**

```bash
git rm <src>/render/render_pipeline.h <src>/render/render_pipeline.cpp
```
(또는 파일시스템 삭제 후 커밋 시 반영. 신규 `src/render_bootstrap/` 사본이 대체.)

- [ ] **Step 8: 빌드 검증**

Run: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_`
(신규 디렉토리라 configure 재실행 필요.)
Expected: 에러 0. `SJH::render_bootstrap` STATIC 빌드 + `_MyApp_` link 성공.

- [ ] **Step 9: 커밋 (사용자 게이트 — 제안만)**

```bash
git commit src/render_bootstrap/ src/render/CMakeLists.txt <src>/render/render_pipeline.h <src>/render/render_pipeline.cpp src/CMakeLists.txt apps/_MyApp_/main.cpp <apps>/_MyApp_/src/Playable/PostFXConstants.h -m "[refactor] : 파이프라인 조립을 render_bootstrap 모듈로 이주 (render->rr include 절단)"
```
(삭제된 파일도 path-scoped 커밋에 포함하면 삭제가 기록됨.)

---

## Task 3: 지점① push (D-1 — SceneRenderer program 목록 pull 제거)

SceneRenderer가 `ResourceRegistry::Get().GetAllPrograms()`로 *pull*하던 것을, 외부가 매 프레임 *push*하도록 뒤집는다. 이 변경으로 `render -> rr` 의 마지막 코드 의존이 사라져 **3-사이클(render->rr->sprite->render) 완전 소멸**.

**Files:**
- Modify: `<src>/render/scene_renderer.h` (setter + 멤버 + Program fwd)
- Modify: `<src>/render/scene_renderer.cpp` (rr pull 제거 + 멤버 사용 + include 제거)
- Modify: `src/render/CMakeLists.txt` (rr PRIVATE link 제거)
- Modify: `apps/_MyApp_/main.cpp` (render 루프에서 push)

- [ ] **Step 1: 헤더에 setter + 멤버 추가** — `<src>/render/scene_renderer.h`

35행의 forward decl 에 `Program` 추가:
```cpp
namespace SJH::Scene { class Actor; class Camera; class PassComponent; }
namespace SJH { class RenderTarget; class Framebuffer; class Mesh; class Material; class Program; }
```

`#include <vector>` 를 상단 include 블록(31행 `#include <cstdint>` 근처)에 추가:
```cpp
#include <cstdint>
#include <vector>
#include <vmath.h>
```

`public:` 의 `RenderWithCamera` 선언 다음에 setter 추가:
```cpp
        /// @brief 명시된 단일 Camera 에 대해 1패스 렌더 수행.
        /// @details @c SceneContext::GetCameras() 자동 순회를 우회하는 외부 진입점 - @c CameraStage 가 위임 호출.
        ///          @c Camera::GetTargetRenderTarget() 이 nullptr 이면 warn 후 skip.
        /// @param cam 렌더 대상 Camera (RenderTarget 이 연결되어 있어야 함).
        void RenderWithCamera(Scene::Camera& cam);

        /// @brief 라이트 uniform 송신 대상 Program 집합을 외부에서 주입 (D-1 push).
        /// @details rr 을 직접 pull 하던 의존을 끊기 위해 외부(Composition Root)가 매 프레임
        ///          @c ResourceRegistry::GetAllPrograms() 스냅샷을 push. render -> rr 의존 제거.
        /// @param programs 활성 Program 포인터 스냅샷 (owner = caller, 본 클래스는 복사 보유).
        void SetActivePrograms(std::vector<Program*> programs) { mActivePrograms = std::move(programs); }
```

`private:` 멤버에 추가 (93행 `mLastSceneOutput` 근처):
```cpp
        MeshPassProcessor      mProcessor;          ///< DrawCommand 큐 보유/정렬/GL draw 발행.
        LightUniformDispatcher mDispatcher;          ///< 수집된 Light 를 모든 Program 에 일괄 uniform 송신.

        std::vector<Program*>  mActivePrograms;     ///< D-1 push -- 외부 주입 Program 집합 (rr pull 대체).
        const Framebuffer*     mLastSceneOutput = nullptr;  ///< 이번 프레임 마지막 PassComponent 출력 FB.
```

> `std::move` 를 헤더 inline setter 에서 쓰므로 `#include <utility>` 가 필요. scene_renderer.h 가 `<vmath.h>`/`<cstdint>` 만 include 하므로 `<utility>` 도 추가:
```cpp
#include <cstdint>
#include <utility>
#include <vector>
#include <vmath.h>
```

- [ ] **Step 2: 구현에서 rr pull 제거** — `<src>/render/scene_renderer.cpp`

123행의 pull 을 멤버 사용으로 교체:

기존(123-124행):
```cpp
		auto programs = ResourceRegistry::Get().GetAllPrograms();
		mDispatcher.Dispatch(programs, dir, points, spots, viewPos);
```
변경:
```cpp
		// D-1 push -- 외부가 SetActivePrograms 로 주입한 스냅샷 사용 (rr 직접 pull 제거).
		mDispatcher.Dispatch(mActivePrograms, dir, points, spots, viewPos);
```

32행의 rr include 제거:
```cpp
#include "resource_registry/resource_registry.h"
```
이 줄을 삭제한다. (scene_renderer.cpp 의 다른 rr 사용 없음 — 123행이 유일했음.)

- [ ] **Step 3: render 모듈 CMake 에서 rr PRIVATE link 제거** — `src/render/CMakeLists.txt`

이제 `render` 의 어느 TU 도 rr 을 include 하지 않으므로(render_pipeline 이주 + scene_renderer pull 제거 완료) PRIVATE link 에서 `SJH::resource_registry` 제거:

기존(29-31행):
```cmake
    PRIVATE SJH::diagnostics SJH::material SJH::object SJH::resource_registry
            SJH::texture  # 2026-06-11 E3 — property_block_setter.cpp 의 Texture (rr 에서 texture 하위추출)
```
변경:
```cmake
    PRIVATE SJH::diagnostics SJH::material SJH::object
            SJH::texture  # 2026-06-11 E3 — property_block_setter.cpp 의 Texture (rr 에서 texture 하위추출)
```

> ⚠ 검증: 제거 전 `grep -rn "resource_registry" src/render/` 로 render 모듈 내 잔여 rr include 가 0 인지 확인. 0 이 아니면(다른 TU 가 rr 사용) 그 줄을 먼저 처리. (현재 grounding 상 render_pipeline.cpp[이주됨] + scene_renderer.cpp[Step 2 제거] 둘뿐이라 0 이어야 함.)

- [ ] **Step 4: 클라 render 루프에서 push** — `apps/_MyApp_/main.cpp`

`render()` 의 stages 순회 직전(354행 `GetLastSceneOutput` 블록 근처)에 program push 추가. SceneRenderer가 카메라별로 `RenderWithCamera`를 돌기 전에 한 번 주입하면 된다:

기존(351-358행):
```cpp
			// ── stages 컬렉션 순회 — World -> Screen -> ScreenQuad ─────────────────
			// ScreenQuadStage 의 sources 는 *stages 순회 직전* 갱신 (지난 프레임 PassComponent 출력).
			{
				auto *out = Manager::Get().SceneRenderer().GetLastSceneOutput();
				mScreenQuadStagePtr->SetSources({out ? out : mSceneFB.get()}); // ! ??? 필요한 것 맞나?
			}
			for (auto &s : mStages)
				s->Render(*mDefaultTarget);
```
변경:
```cpp
			// ── stages 컬렉션 순회 — World -> Screen -> ScreenQuad ─────────────────
			// D-1 push -- 라이트 uniform 송신 대상 Program 집합을 SceneRenderer 에 주입
			// (SceneRenderer 가 rr 을 직접 pull 하던 의존 제거 -> render->rr 사이클 절단).
			Manager::Get().SceneRenderer().SetActivePrograms(SJH::ResourceRegistry::Get().GetAllPrograms());

			// ScreenQuadStage 의 sources 는 *stages 순회 직전* 갱신 (지난 프레임 PassComponent 출력).
			{
				auto *out = Manager::Get().SceneRenderer().GetLastSceneOutput();
				mScreenQuadStagePtr->SetSources({out ? out : mSceneFB.get()}); // ! ??? 필요한 것 맞나?
			}
			for (auto &s : mStages)
				s->Render(*mDefaultTarget);
```
(`main.cpp` 은 이미 54행에서 `resource_registry/resource_registry.h` include — 추가 include 불요.)

- [ ] **Step 5: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 에러 0. 이제 `render` 모듈은 rr 을 컴파일/링크 의존하지 않음 -> `render -> rr -> sprite -> render` 3-사이클 소멸 (완전 DAG).

- [ ] **Step 6: GUI 육안 검증 (사용자)**

`cd build_ninja/apps/_MyApp_ && ./_MyApp_` — 라이팅이 이전과 동일하게 렌더되는지 확인 (program 집합 push 가 pull 과 동일 스냅샷이므로 시각 변화 없어야 정상).

- [ ] **Step 7: 커밋 (사용자 게이트 — 제안만)**

```bash
git commit <src>/render/scene_renderer.h <src>/render/scene_renderer.cpp src/render/CMakeLists.txt apps/_MyApp_/main.cpp -m "[refactor] : SceneRenderer program 목록 pull -> push (render->rr 코드 의존 절단, 완전 DAG)"
```

---

## Task 4: 부트 하니스 — EngineBootstrap + IClientBootstrap (D-4 변동성 분리 seam)

저변동 엔진 스켈레톤이 3개 클라 hook 을 순차 호출하는 템플릿메서드. **헤더 온리** (구현 사소). 거주는 클라 `Bootstrap/` (spec §2 가 InitScheduler 에 적용한 동일 YAGNI — 재사용 시 `src/` 엔진 모듈로 승격).

> **§10 proportionality 노트(grounding 확정)**: sb7 `application::run()` 이 window/GL 컨텍스트를 startup() *이전에* 생성하므로, `EngineBootstrap` 의 엔진-고정 단계는 거의 비어 있다(현재 `InitDeviceContext` 한 번의 touch 수준). 그럼에도 이 클래스가 값을 갖는 이유는 **3 hook 의 phase 시퀀싱**이다 — 각 hook 이 phase-local `InitScheduler` 를 돌려 "자원 -> 씬 -> 첫프레임직전" 의 거친 순서를 강제하고 phase 내부는 topo-sort. 엔진-고정 본문이 얇은 건 학생 프로젝트(sb7 소유)에서 *예상된* 것이며 D-4 변동성 경계(저변동 엔진 / 고변동 클라)는 hook 구조로 보존된다.

**Files:**
- Create: `apps/_MyApp_/src/Bootstrap/EngineBootstrap.h`

- [ ] **Step 1: 헤더 작성** — `apps/_MyApp_/src/Bootstrap/EngineBootstrap.h`

```cpp
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
```

- [ ] **Step 2: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 에러 0 (헤더 온리, 아직 미사용 -- 거동 불변). 헤더가 컴파일 DB에 없으면 이 task 단독으론 컴파일되지 않을 수 있음 -- Task 5 에서 main.cpp 가 include 하면 검증됨. 단독 검증이 필요하면 임시로 main.cpp 에 `#include "apps/_MyApp_/src/Bootstrap/EngineBootstrap.h"` 만 추가해 빌드 후 되돌린다.

- [ ] **Step 3: 커밋 (사용자 게이트 — 제안만)**

```bash
git commit apps/_MyApp_/src/Bootstrap/EngineBootstrap.h -m "[refactor] : EngineBootstrap 변동성 분리 부트 스켈레톤 + IClientBootstrap hook"
```

---

## Task 5: startup() 마이그레이션 (InitScheduler + EngineBootstrap 통합)

`game_application` 이 `IClientBootstrap` 을 구현하고, startup() 본문을 3 hook 으로 분할한다. 각 hook 은 phase-local `InitScheduler` 에 **결합도 기준 7개 coarse task** 를 등록하고 `RunAll()`.

> **왜 40노드가 아니라 7 coarse task인가**: `mStages` 의 최종 순서는 `insert(begin)`/`insert(begin+1)` 같은 *명령형 위치 삽입* 으로 결정된다. 토포정렬이 이 task 들을 재배치하면 렌더 순서가 깨진다. 따라서 명령형 순서가 중요한 블록(스테이지 조립, FSM 와이어링, PostFX 체인+screenCam local)은 **한 task 안에 묶어** 내부 순서를 보존하고, task *사이* 의 진짜 교차 의존만 `Needs(...)` 로 선언한다 (spec §7 "점진 가능" + 누락 dep 위험 회피). phase 간 순서는 hook 시퀀스가 보장하므로 phase 경계를 넘는 dep 은 선언 불요.

**Files:**
- Modify: `apps/_MyApp_/main.cpp` (startup() 재구성 + include 2줄 + 멤버/메서드)

- [ ] **Step 1: include 추가** — `apps/_MyApp_/main.cpp` 상단 Bootstrap include 블록(18-20행)에 추가

```cpp
#include "apps/_MyApp_/src/Bootstrap/AudioWarmup.h"
#include "apps/_MyApp_/src/Bootstrap/EngineBootstrap.h"   // <- 추가 (부트 스켈레톤 + IClientBootstrap)
#include "apps/_MyApp_/src/Bootstrap/InitScheduler.h"     // <- 추가 (데이터주도 topo-sort)
#include "apps/_MyApp_/src/Bootstrap/PlayerBuilder.h"
#include "apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.h"
```

- [ ] **Step 2: 클래스 선언을 IClientBootstrap 구현으로 변경** — `apps/_MyApp_/main.cpp:77`

기존:
```cpp
	class game_application : public sb7::application
	{
```
변경:
```cpp
	class game_application : public sb7::application, public Bootstrap::IClientBootstrap
	{
```

- [ ] **Step 3: startup() 본문을 부트 호출로 교체** — `apps/_MyApp_/main.cpp:96-292`

현재의 `void startup() override { ... }` 전체 본문을, 멤버 프레임버퍼 정보 캐시 + EngineBootstrap 호출로 교체한다. (기존 본문은 Step 4 의 3 hook 으로 이사.)

```cpp
		void startup() override
		{
			// A1 -- GLFW window 정보 캐시 (hook 들이 공유).
			mFbInfo = SJH::GetFramebufferInfo(window);
			glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

			// 부트 시퀀스 -- 엔진 스켈레톤이 3 hook 을 순차 호출 (제어역전).
			// 각 hook 은 phase-local InitScheduler 로 자기 phase task 를 topo-sort 직렬 실행.
			Bootstrap::EngineBootstrap engine;
			engine.Boot(*this);
		}
```

- [ ] **Step 4: 3개 hook 구현 추가** — `apps/_MyApp_/main.cpp`, `startup()` 다음에 삽입

아래 3 메서드는 기존 startup() 의 코드 블록을 **그대로** task 람다 안으로 옮긴 것이다 (식별자/순서 보존). phase 경계는 topo.dot 레벨 군집을 따른다.

```cpp
		// ── phase 1: 자원/시스템 (topo L0~L2) ────────────────────────────────
		void OnResourcesReady() override
		{
			Bootstrap::InitScheduler sched;

			// T1 core -- 렌더 타깃 + 시스템 초기화 + 오디오 워밍업.
			sched.Task("core").Gl().Does([&] {
				mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(mFbInfo.Width, mFbInfo.Height);
				mSceneFB       = SJH::Framebuffer::CreateWithDepthTexture(mFbInfo.Width, mFbInfo.Height);

				TopdownShooter::Manager::Get().Init();
				Bootstrap::WarmupAudio(Manager::Get().Audio());
				return mSceneFB != nullptr;
			});

			sched.RunAll();
		}

		// ── phase 2: 씬/파이프라인 (topo L3~L5) ──────────────────────────────
		void OnSceneSetup() override
		{
			auto &reg     = SJH::ResourceRegistry::Get();
			auto &dir     = SJH::Scene::Director::Get();
			auto &manager = TopdownShooter::Manager::Get();
			auto &phys    = manager.Physics();
			auto &vfxs    = manager.VFX();

			Bootstrap::InitScheduler sched;

			// T2 screenPipeline -- DefaultPipeline + ScreenCamera + PostFX 체인 + fog/vignette + 레지스트리.
			//   (screenCamActorPtr / chain local 이 한 task 안에 묶여 댕글링 없음.)
			sched.Task("screenPipeline").Gl().Does([&] {
				SJH::Render::DefaultPipelineConfig pipelineCfg{
				    PASSTHOURH_PROGRAM_CONFIG.Name,
				    PASSTHOURH_PROGRAM_CONFIG.VertFile,
				    PASSTHOURH_PROGRAM_CONFIG.FragFile,
				};
				auto sqStage = SJH::Render::SetupDefaultPipeline(reg, manager.SceneRenderer(), mSceneFB.get(), pipelineCfg);
				if (!sqStage)
					return false;
				mScreenQuadStagePtr = sqStage.get();
				mStages.push_back(std::move(sqStage));

				auto screenCamActor    = SJH::Scene::CreateScreenCameraActor("ScreenCamera", mFbInfo.Aspect, mSceneFB.get());
				mScreenCamera          = screenCamActor->GetComponent<SJH::Scene::Camera>();
				auto *screenCamActorPtr = dir.Root().AddChild(std::move(screenCamActor));

				auto chain = SJH::Render::BuildPostFXChain(reg, *screenCamActorPtr, POSTFX_PROGRAM_CONFIGS, mSceneFB.get(), mFbInfo.Width, mFbInfo.Height);
				mPostFXFBs      = std::move(chain.Framebuffers);
				mPassComponents = std::move(chain.PassComponents);

				for (std::size_t i = 0; i < POSTFX_PROGRAM_CONFIGS.size() && i < mPassComponents.size(); ++i)
				{
					const auto &name = POSTFX_PROGRAM_CONFIGS[i].Name;
					if (mPassComponents[i] && (name == "invert" || name == "blurring" || name == "sobel"))
						mPassComponents[i]->Enabled = false;
				}

				if (auto *fogMat = FindFogMaterial())
				{
					fogMat->Properties.Vec3s["uFogColor"] = Playable::FOG_COLOR;
					fogMat->Properties.Ints["uFogMode"]   = Playable::FOG_MODE;
				}
				RebindFogUniforms();

				if (auto *gvMat = FindPassMaterial("grayscale_vignetting"))
					gvMat->Properties.Vec3s["uVignetteColor"] = Playable::VIGNETTE_COLOR;

				TopdownShooter::Playable::PostFXRegistry::Get().Register("grayscale_vignetting", FindPassMaterial("grayscale_vignetting"));
				return true;
			});

			// T3 world -- WorldScene(camera/light/skybox) + 스테이지 액터 + FxRoot + spawn 컨텍스트
			//             + muzzle 이펙트 + 플레이어 + 웨이브 컨트롤러.
			sched.Task("world").Gl().Does([&] {
				auto worldScene = Bootstrap::BuildWorldScene({mFbInfo.Aspect, &mMouse, mSceneFB.get()});
				mCamera    = worldScene.WorldCamera;
				mSkyboxMat = worldScene.SkyboxMat;

				dir.Root().AddChild(std::move(TopdownShooter::Stage::CreateStageActor({&phys.World(), &reg})));

				mFxRoot = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("FxRoot"));
				VFX::SetSpawnContext(mFxRoot, &vfxs);
				WorldText::SetSpawnContext(mFxRoot, manager.WorldText().GetFont());

				reg.CreateEffect(vfxs.GetManager(), VFX::MUZZLE_EFFECT.key, VFX::MUZZLE_EFFECT.path);

				auto player  = Bootstrap::BuildPlayer({&mKeyboard, &mMouse, &phys.World(), mCamera});
				mSpriteActor = player.SpriteActor;

				auto *waveSpawner = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("WaveSpawner"));
				waveSpawner->AddComponent<Stage::WaveController>(&phys.World(), waveSpawner, mSpriteActor, Stage::ARENA_HALF_EXTENT);
				return mCamera != nullptr && mSpriteActor != nullptr;
			});

			// T4 stages -- stages 컬렉션 명령형 조립 (worldCam/particle/screenCam 순서 보존).
			//   deps: screenPipeline(mScreenCamera+screenQuad) + world(mCamera+vfxs).
			sched.Task("stages").Needs({"screenPipeline", "world"}).Cpu().Does([&] {
				mStages.insert(mStages.begin(),
				    std::make_unique<SJH::CameraStage>(&Manager::Get().SceneRenderer(), mScreenCamera));
				mStages.insert(mStages.begin(),
				    std::make_unique<SJH::CameraStage>(&Manager::Get().SceneRenderer(), mCamera));
				mStages.insert(mStages.begin() + 1,
				    std::make_unique<TopdownShooter::VFX::ParticleStage>(&Manager::Get().VFX(), mCamera));
				return true;
			});

			// T5 vfxUi -- VFX 테스트 이펙트 로드 + 게임 UI(PostFX 디버그) + VFX 소환 레이어.
			//   deps: screenPipeline(mPassComponents 디버그 엔트리). vfxEntries 는 이 task local.
			sched.Task("vfxUi").Needs({"screenPipeline"}).Gl().Does([&] {
				std::vector<UI::VfxSpawnLayer::Entry> vfxEntries;
				for (const auto &v : VFX::TEST_EFFECTS)
				{
					if (auto *eff = reg.CreateEffect(vfxs.GetManager(), v.key, v.path))
					{
						vfxEntries.push_back({v.key, eff});
						std::string narrow;
						for (const char16_t *p = v.path; *p; ++p)
							narrow.push_back(static_cast<char>(*p));
						SJH::Diagnostics::EffekseerDiagnostics::CheckEffectTextures(narrow);
					}
					else
						spdlog::warn("[vfx-test] load failed: {}", v.key);
				}

				std::vector<UI::PassDebugEntry> debugEntries;
				for (std::size_t i = 0; i < POSTFX_PROGRAM_CONFIGS.size(); ++i)
					if (i < mPassComponents.size())
						debugEntries.push_back({POSTFX_PROGRAM_CONFIGS[i].Name, mPassComponents[i]});
				mImGuiCtx = UI::BuildGameUI({window, &reg, &mImGuiStack, std::move(debugEntries), &mGamma, [this] { TogglePause(); }});

				auto layer = std::make_unique<UI::VfxSpawnLayer>(std::move(vfxEntries));
				mVfxLayer  = layer.get();
				mImGuiStack.Push(std::move(layer));
				return mImGuiCtx != nullptr;
			});

			sched.RunAll();
		}

		// ── phase 3: 진입/FSM (topo L6~L9) ───────────────────────────────────
		void OnBeforeFirstFrame() override
		{
			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();

			Bootstrap::InitScheduler sched;

			// T6 enter -- Director.Enter (모든 Component OnEnter -- Camera/Light 자동 등록).
			sched.Task("enter").Gl().Does([&] {
				dir.Enter();
				return true;
			});

			// T7 fsm -- GameContext + 오버레이 텍스처 + Stage FSM 등록/와이어링/진입.
			//   deps: enter (Component OnEnter 후 FSM 진입).
			sched.Task("fsm").Needs({"enter"}).Gl().Does([&] {
				auto *ctxActor = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("GameContext"));
				mCtx           = ctxActor->AddComponent<Stage::Components::GameContextComponent>();

				mCtx->titleTex    = reg.CreateTexture("ui_title", SJH::Image::Load("ui_title", "resources/texture/Title.png").get());
				mCtx->pauseTex    = reg.CreateTexture("ui_pause", SJH::Image::Load("ui_pause", "resources/texture/Pause.png").get());
				mCtx->gameOverTex = reg.CreateTexture("ui_gameover", SJH::Image::Load("ui_gameover", "resources/texture/GameOver.png").get());

				for (std::size_t i = 0; i < POSTFX_PROGRAM_CONFIGS.size() && i < mPassComponents.size(); ++i)
					if (mPassComponents[i] && POSTFX_PROGRAM_CONFIGS[i].Name == "blurring")
						mCtx->blurPass = mPassComponents[i];

				{
					auto overlay  = std::make_unique<UI::StateOverlayLayer>();
					mCtx->overlay = overlay.get();
					mImGuiStack.Push(std::move(overlay));
				}

				mStageFsm = std::make_unique<Stage::StageStateMachine>(dir.Root());
				mStageFsm->RegisterState(std::make_unique<Stage::TitleState>(mStageFsm.get()));
				mStageFsm->RegisterState(std::make_unique<Stage::CombatPlayState>(mStageFsm.get()));
				mStageFsm->RegisterState(std::make_unique<Stage::PauseState>(mStageFsm.get()));
				mStageFsm->RegisterState(std::make_unique<Stage::GameOverState>(mStageFsm.get()));

				// WaveController 연결 -- phase 2 에서 만든 WaveSpawner 를 이름으로 조회 (local 승격 회피).
				if (auto *waveSpawner = dir.Root().FindChild("WaveSpawner"))
				{
					mCtx->waveCtrl = waveSpawner->GetComponent<Stage::WaveController>();
					if (mCtx->waveCtrl)
						mCtx->waveCtrl->SetStageStateMachine(mStageFsm.get());
				}

				mCtx->audio = &TopdownShooter::Manager::Get().Audio();
				if (auto *bgmActor = dir.Root().FindChild("BgmActor"))
					mCtx->bgmPlayable = bgmActor->GetComponent<Audio::FmodStudioPlayable>();
				if (mSpriteActor)
					mCtx->playerLife = mSpriteActor->GetComponent<Entity::Components::Life>();

				mStageFsm->OnEnter();
				return true;
			});

			sched.RunAll();
		}
```

- [ ] **Step 5: 멤버 추가** — `apps/_MyApp_/main.cpp` private 멤버 블록(436행 근처)에 프레임버퍼 정보 캐시 추가

`mDefaultTarget` 선언 앞에:
```cpp
		// ── 멤버 ────────────────────────────────────────────────────────────────────
		SJH::FramebufferInfo  mFbInfo{};       ///< startup 캐시 -- 3 hook 이 공유하는 window/fb 크기/비율.
		SJH::RenderTargetUPtr mDefaultTarget;
```

> ⚠ `SJH::FramebufferInfo` 의 정확한 타입명을 `common/window_helper.h`(이미 47행 include)에서 확인할 것. `GetFramebufferInfo(window)` 의 반환형이다 (기존 99행 `const auto fb = SJH::GetFramebufferInfo(window);` 에서 `fb.Width/Height/Aspect` 사용 확인됨). 타입명이 다르면 멤버 선언을 실제 반환형으로 맞춘다.

- [ ] **Step 6: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 에러 0.
흔한 실패: (a) `mFbInfo` 타입명 불일치 -> Step 5 ⚠ 확인. (b) hook 안에서 못 잡는 캡처 -> `[&]` 가 `this` 캡처하므로 멤버 접근 OK, phase-local `reg/dir/manager` 참조는 각 hook 상단에서 재취득. (c) `Stage::WaveController` 등 헤더는 이미 main.cpp 가 include 중.

- [ ] **Step 7: GUI 육안 검증 (사용자) — 가장 중요**

`cd build_ninja/apps/_MyApp_ && ./_MyApp_`. 콘솔에 `[InitScheduler] run 'core'/'screenPipeline'/'world'/'stages'/'vfxUi'/'enter'/'fsm'` 순서 로그 확인(debug 레벨). 화면: Title 오버레이 -> 게임 진입 시 라이팅/스프라이트/PostFX(fog/vignette)/스테이지/발사/적 웨이브/데미지 숫자 모두 이전과 동일. **stages 순서(worldCam->particle->screenCam->screenQuad)가 보존되어 PostFX 합성이 정상인지** 특히 확인.

- [ ] **Step 8: 커밋 (사용자 게이트 — 제안만)**

```bash
git commit apps/_MyApp_/main.cpp -m "[refactor] : startup() 을 EngineBootstrap 3-phase + InitScheduler topo-sort 로 마이그레이션"
```

---

## Task 6: D-7 PostFXRegistry 흡수 (ResourceRegistry 로 통합)

중복 별칭인 `PostFXRegistry` 싱글톤을 제거. pass Material 은 이미 rr 에 `mat_pass_<name>` 로 등록되어 있으므로, 연출 트랙이 `rr.FindSharedMaterial("mat_pass_"+passName)` 로 직접 조회한다.

> **정정(grounding)**: spec D-7 은 `rr.FindMaterial(...)` 이라 적었으나, 실제 rr 메서드는 **`FindSharedMaterial(const std::string&)`** (resource_registry.h:83). pass Material 은 `CreateSharedMaterial("mat_pass_"+name)` 으로 생성되므로(render_pipeline.cpp:96,105) 대칭 조회는 `FindSharedMaterial`.

**소비자 3곳(grep 확정):**
- `apps/_MyApp_/main.cpp:163` — `PostFXRegistry::Get().Register(...)` (등록 — 흡수 시 *제거*. rr 에 이미 등록되어 있으므로 별도 등록 불요).
- `apps/_MyApp_/src/Playable/PostFXTweenPlayable.cpp:42` — `PostFXRegistry::Get().Material(mPassName)`.
- `apps/_MyApp_/src/Playable/HpGrayscalePostFX.cpp:45` — `PostFXRegistry::Get().Material(mPassName)`.

**Files:**
- Modify: `apps/_MyApp_/src/Playable/PostFXTweenPlayable.cpp`
- Modify: `apps/_MyApp_/src/Playable/HpGrayscalePostFX.cpp`
- Modify: `apps/_MyApp_/main.cpp` (Register 호출 + include 제거)
- Modify: `apps/_MyApp_/src/Playable/CMakeLists.txt` (소스 제거)
- Delete: `<apps>/_MyApp_/src/Playable/PostFXRegistry.h` + `.cpp`

- [ ] **Step 1: PostFXTweenPlayable.cpp 조회 교체**

include 교체 (16행):
```cpp
#include "<Playable>/PostFXRegistry.h"
```
->
```cpp
#include "resource_registry/resource_registry.h"
```

조회 교체 (42행):
```cpp
		// pass Material 은 매 프레임 조회 - 없으면(미등록/파괴) 조용히 무시.
		if (auto *mat = PostFXRegistry::Get().Material(mPassName))
			mat->Properties.Floats[mUniformName] = value;
```
->
```cpp
		// pass Material 은 매 프레임 조회 - rr 의 mat_pass_<name> 공유본 직접 조회 (D-7 흡수).
		if (auto *mat = SJH::ResourceRegistry::Get().FindSharedMaterial("mat_pass_" + mPassName))
			mat->Properties.Floats[mUniformName] = value;
```

- [ ] **Step 2: HpGrayscalePostFX.cpp 조회 교체**

include 교체 (15행):
```cpp
#include "<Playable>/PostFXRegistry.h"
```
->
```cpp
#include "resource_registry/resource_registry.h"
```

조회 교체 (45행):
```cpp
		if (auto *mat = PostFXRegistry::Get().Material(mPassName))
			mat->Properties.Floats[mUniformName] = ratio;
```
->
```cpp
		// rr 의 mat_pass_<name> 공유본 직접 조회 (D-7 PostFXRegistry 흡수).
		if (auto *mat = SJH::ResourceRegistry::Get().FindSharedMaterial("mat_pass_" + mPassName))
			mat->Properties.Floats[mUniformName] = ratio;
```

> `MyApp::Playable` 은 이미 `SJH::resource_registry` 를 PRIVATE link (Playable/CMakeLists.txt) — include/link 추가 불요. `SJH::material` 도 이미 PUBLIC link (Material::Properties 접근 OK).

- [ ] **Step 3: main.cpp 에서 Register 제거 + include 제거**

163행 일대(161-163행)의 Register 블록 제거:
```cpp
			// 연출 foundation — PostFX pass Material 을 레지스트리에 등록 (hit-FX 트랙의 PostFXTweenPlayable 이
			// PostFXRegistry::Get().Material("grayscale_vignetting")->Properties 로 도달). pass material 유효 지점.
			TopdownShooter::Playable::PostFXRegistry::Get().Register("grayscale_vignetting", FindPassMaterial("grayscale_vignetting"));
```

> ⚠ 이 코드는 Task 5 에서 `OnSceneSetup` 의 `screenPipeline` task 람다 안으로 이미 옮겨졌다. **Task 5 적용 후라면 그 람다 안의 Register 줄을 제거**한다 (위 주석 2줄 + Register 1줄). rr 에 `mat_pass_grayscale_vignetting` 이 이미 `CreateSharedMaterial` 로 등록되어 있으므로 별도 등록 불필요.

27행의 include 제거:
```cpp
#include "<Playable>/PostFXRegistry.h"             // 연출 foundation — PostFX pass Material 레지스트리
```

- [ ] **Step 4: Playable CMakeLists 에서 소스 제거** — `apps/_MyApp_/src/Playable/CMakeLists.txt`

`add_library(myapp_playable STATIC ...)` 목록에서 `PostFXRegistry.cpp` 줄 제거:
```cmake
add_library(myapp_playable STATIC
    PlayableDirector.cpp
    PostFXRegistry.cpp          # <- 이 줄 제거
    PostFXTweenPlayable.cpp
    SpriteFxPlayable.cpp
    HpGrayscalePostFX.cpp
    SpriteLayerFactory.cpp   # 3-빌더 공유 sprite-layer 부착 헬퍼 (atlas + SpriteRenderer + 옵션 애니)
)
```

- [ ] **Step 5: PostFXRegistry 파일 삭제**

```bash
git rm <apps>/_MyApp_/src/Playable/PostFXRegistry.h <apps>/_MyApp_/src/Playable/PostFXRegistry.cpp
```

- [ ] **Step 6: 빌드 검증**

Run: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_`
(소스 목록 변경이라 configure 재실행.)
Expected: 에러 0. `PostFXRegistry` 미참조 확인: `grep -rn "PostFXRegistry" apps/_MyApp_` 결과 0.

- [ ] **Step 7: GUI 육안 검증 (사용자)**

`./_MyApp_` 실행. **HP 가 닳을 때 화면 무채색(grayscale_vignetting)** + **피격 시 빨강 비네팅 트윈** 이 정상 작동하는지 확인 (HpGrayscalePostFX + PostFXTweenPlayable 가 rr 직접 조회로 동일 Material 도달).

- [ ] **Step 8: 커밋 (사용자 게이트 — 제안만)**

```bash
git commit apps/_MyApp_/src/Playable/PostFXTweenPlayable.cpp apps/_MyApp_/src/Playable/HpGrayscalePostFX.cpp apps/_MyApp_/src/Playable/CMakeLists.txt <apps>/_MyApp_/src/Playable/PostFXRegistry.h <apps>/_MyApp_/src/Playable/PostFXRegistry.cpp apps/_MyApp_/main.cpp -m "[refactor] : PostFXRegistry 를 ResourceRegistry 로 흡수 (중복 전역 제거, 싱글톤 -1)"
```

---

## Task 7: D-8 Manager 개명 (Scene::Director 명칭 충돌 해소)

`Manager` 클래스를 서술적 이름으로 개명. **병합 아님** — 엔진 `Scene::Director`(씬) 와 다른 도메인/레이어(클라 시스템 허브)이며, "Director" 라는 문서상 혼동만 해소한다.

> ⚠ **새 이름 미확정 — 사용자 확정 후 실행.** 이 plan 은 **`GameSystems`** 를 권장명으로 사용한다 (이유: 이 클래스는 Audio/VFX/Physics/WorldText/SceneRenderer 5종 *시스템* 을 값으로 집계하는 허브 -> "게임 시스템들" 이 가장 서술적). 다른 이름(`GameApp` 등)을 택하면 **Task 7 전체에서 `GameSystems` 를 그 이름으로 일괄 치환**한다.

**범위(grep 확정):** `Manager::` 참조 11파일 / 136 토큰 / `TopdownShooter::Manager` 정규참조 17. 순수 기계적 rename.

**Files (rename 대상):**
- `<apps>/_MyApp_/src/Manager.h` -> `GameSystems.h` (+ 클래스/가드/주석)
- `<apps>/_MyApp_/src/Manager.cpp` -> `GameSystems.cpp`
- `apps/_MyApp_/src/CMakeLists.txt` (타겟 `myapp_manager`/`MyApp::Manager` + 소스명)
- 참조처: `apps/_MyApp_/main.cpp`, `apps/_MyApp_/src/Bootstrap/{AudioWarmup.h,EnemyBuilder.cpp,PlayerBuilder.cpp,CMakeLists.txt}`, `apps/_MyApp_/src/Spawns/{OneShotSweeper.h,SequenceContext.h}`, `apps/_MyApp_/src/Stage/{StageBuilder.cpp,apps/_MyApp_/src/Stage/State/StageState.Impl.h}`, `apps/_MyApp_/src/Text/WorldTextSystem.h`, `apps/_MyApp_/src/VFX/VFXSystem.cpp`

- [ ] **Step 1: 파일 rename**

```bash
git mv <apps>/_MyApp_/src/Manager.h apps/_MyApp_/src/GameSystems.h
git mv <apps>/_MyApp_/src/Manager.cpp apps/_MyApp_/src/GameSystems.cpp
```

- [ ] **Step 2: GameSystems.h 내부 갱신** — `apps/_MyApp_/src/GameSystems.h`

- 헤더 가드 `_TOPDOWNSHOOTER_DIRECTOR_H__` -> `_TOPDOWNSHOOTER_GAMESYSTEMS_H__` (열기 + `#endif` 주석 둘 다).
- 클래스명 `class Manager` -> `class GameSystems`, 생성자/소멸자 `Manager()` `~Manager()`, 복사/이동 delete 의 `Manager` -> `GameSystems`, `static Manager &Get()` -> `static GameSystems &Get()`.
- 주석의 "Client-side Director" / `@brief` 등 "Director" 표현을 "게임 시스템 허브(GameSystems)" 로 정리 (Doxygen `@file Manager.h` -> `@file GameSystems.h`).

- [ ] **Step 3: GameSystems.cpp 내부 갱신** — `apps/_MyApp_/src/GameSystems.cpp`

- `#include "Manager.h"` -> `#include "GameSystems.h"`.
- `Manager &Manager::Get()` -> `GameSystems &GameSystems::Get()`, `static Manager instance;` -> `static GameSystems instance;`.
- `void Manager::Init/Update/Shutdown` -> `GameSystems::`.
- 로그 문자열 `"[Director] init OK"` / `"[Director] shutdown OK"` -> `"[GameSystems] ..."` (기능 무관, 일관성).
- `@file Manager.cpp` -> `@file GameSystems.cpp`.

- [ ] **Step 4: CMake 타겟 rename** — `apps/_MyApp_/src/CMakeLists.txt`

```cmake
# M5 CL3 — 게임 시스템 허브 싱글톤 (Audio + VFX + Physics + WorldText + SceneRenderer 집계)
add_library(myapp_gamesystems STATIC
    GameSystems.cpp
)
add_library(MyApp::GameSystems ALIAS myapp_gamesystems)
target_include_directories(myapp_gamesystems
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(myapp_gamesystems
    PUBLIC
        MyApp::Audio
        MyApp::VFX
        MyApp::Physics
        MyApp::Text
    PRIVATE
        spdlog
)
target_compile_features(myapp_gamesystems PUBLIC cxx_std_17)
```
그리고 우산 `myapp_client INTERFACE` link 목록의 `MyApp::Manager` -> `MyApp::GameSystems`:
```cmake
    MyApp::GameSystems  # M5 CL3 — Audio+VFX+Physics+WorldText+SceneRenderer 집계 싱글톤
```

- [ ] **Step 5: Bootstrap CMake link rename** — `apps/_MyApp_/src/Bootstrap/CMakeLists.txt`

`MyApp::Manager       # Manager::Get()` -> `MyApp::GameSystems   # GameSystems::Get()`.

- [ ] **Step 6: 참조처 일괄 치환 — 코드 9파일**

다음 명령으로 잔여 참조를 모두 찾는다:
```bash
grep -rln "TopdownShooter::Manager\|\bManager::\|\"Manager.h\"\|#include \"Manager.h\"" apps/_MyApp_ --include='*.h' --include='*.cpp'
```
각 파일에서:
- `TopdownShooter::Manager::Get()` -> `TopdownShooter::GameSystems::Get()`
- `Manager::Get()` (네임스페이스 내 unqualified) -> `GameSystems::Get()`
- `#include "Manager.h"` -> `#include "GameSystems.h"`
- 변수/주석의 `Manager` 표현 정리

> ⚠ **`SJH::SceneRenderer` 의 `SceneRenderer()` 접근자, `WaveController`/`PhysicsSystem` 등 다른 "Manager" 무관 식별자를 오치환하지 말 것.** grep 은 `Manager` 토큰 136개를 보이지만 정규 대상은 `TopdownShooter::Manager` / `Manager::` (클래스) 뿐이다. Effekseer 의 `Manager`(예: `vfxs.GetManager()` -> Effekseer::Manager), Box2D 등 **서드파티 `Manager` 는 건드리지 않는다.** `vfxs.GetManager()` 는 VFXSystem 의 메서드라 무관 — 치환 금지. 반드시 파일별로 컨텍스트 확인 후 치환.

- [ ] **Step 7: 빌드 검증**

Run: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_`
Expected: 에러 0. `grep -rn "TopdownShooter::Manager\b\|class Manager\b" apps/_MyApp_` 결과 0 (서드파티 `Manager` 제외).

- [ ] **Step 8: GUI 육안 검증 (사용자)**

`./_MyApp_` — 게임 전체가 이전과 동일 작동 (순수 rename, 거동 0 변화).

- [ ] **Step 9: 커밋 (사용자 게이트 — 제안만)**

```bash
git commit apps/_MyApp_/src/GameSystems.h apps/_MyApp_/src/GameSystems.cpp <apps>/_MyApp_/src/Manager.h <apps>/_MyApp_/src/Manager.cpp apps/_MyApp_/src/CMakeLists.txt apps/_MyApp_/src/Bootstrap/CMakeLists.txt apps/_MyApp_/main.cpp apps/_MyApp_/src/Bootstrap/AudioWarmup.h apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp apps/_MyApp_/src/Spawns/OneShotSweeper.h apps/_MyApp_/src/Spawns/SequenceContext.h <apps>/_MyApp_/src/Stage/StageBuilder.cpp apps/_MyApp_/src/Stage/State/StageState.Impl.h apps/_MyApp_/src/Text/WorldTextSystem.h apps/_MyApp_/src/VFX/VFXSystem.cpp -m "[refactor] : Manager -> GameSystems 개명 (Scene::Director 명칭 충돌 해소)"
```

---

## 최종 검증 (전체 task 후)

- [ ] 완전 DAG 확인: `render` 모듈이 `rr` 을 include 하지 않음 — `grep -rn "resource_registry" src/render/` = 0.
- [ ] 전역 싱글톤 5 -> 4: `PostFXRegistry` 소멸 확인.
- [ ] 빌드 GREEN + GUI 전체 회귀 (라이팅/스프라이트/PostFX/스테이지/발사/적/데미지숫자/HP무채색/피격비네팅/Title-Pause-GameOver FSM).
- [ ] (선택) `doc/diagrams/2026-06-19-module-deps-current.dot` 의 `render->rr` 빨강 엣지 제거 + `render_bootstrap` 노드 추가해 그래프 갱신.

---

## Self-Review (spec 대비 — 작성자 체크)

**1. Spec coverage:**
- D-1 지점① push -> Task 3 ✅
- D-2 지점② render_bootstrap -> Task 2 ✅
- D-3 T2 직렬 -> Task 1 (`ExecuteInOrder` 직렬, `EAffinity` 분류만) ✅
- D-4 Template Method 변동성 분리 -> Task 4 (EngineBootstrap+IClientBootstrap) + Task 5 (3 hook) ✅
- D-5 검증->Kahn->F-2 -> Task 1 (`Validate`+`ExecuteInOrder` 사이클 abort + F-2) ✅
- D-6 fluent builder C++17 -> Task 1 (`InitTaskBuilder`) ✅
- D-7 PostFXRegistry 흡수 -> Task 6 ✅
- D-8 Manager 개명 -> Task 7 ✅
- spec §8 지점① per-frame -> Task 3 Step 4 (render 루프 매 프레임 push) ✅ (open item 확정: per-frame 채택)
- spec §10 EngineBootstrap proportionality -> Task 4 노트로 해소(hook phase 시퀀싱이 가치) ✅

**2. Placeholder scan:** "TBD"/"적절히"/추상 단계 없음. 유일한 미확정 = Task 7 클래스명 -> `GameSystems` 로 구체화 + 사용자 확정 게이트 명시. ✅

**3. Type consistency:** `InitScheduler::Task` -> `InitTaskBuilder` -> `.Needs/.Gl/.Cpu/.Does` -> `AddTask` -> `RunAll`(`Validate`+`ExecuteInOrder`) 전 task 일관. `SetActivePrograms(std::vector<Program*>)` setter 명 Task 3 헤더/구현/호출 일치. `FindSharedMaterial` rr 실 시그니처 일치(spec `FindMaterial` 정정). ✅

**열린 결정 1건(사용자 확인):** Task 7 의 개명 후보 `GameSystems` 확정 여부 — plan 은 권장명으로 진행, 다르면 일괄 치환.
