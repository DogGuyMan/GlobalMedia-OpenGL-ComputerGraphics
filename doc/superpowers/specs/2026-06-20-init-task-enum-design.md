# InitScheduler task-ID enum 화 + 우선순위 tiebreak 설계 (2026-06-20)

> **상태**: 설계 승인 완료(brainstorm). 다음 = writing-plans.
> **선행**: [`2026-06-19-init-scheduler-design.md`](2026-06-19-init-scheduler-design.md) (InitScheduler T2 데이터주도 topo-sort). 본 문서는 그 위 *식별자 타입 개선*.
> **브랜치/HEAD**: `game/main` / `9123dae` 위 미커밋 (init-scheduler 구현 + orbital 수정 포함).

## 1. 배경 / 문제

`InitScheduler`(T2)는 task 를 *string Id* 로 식별한다. 구현 + orbital 회귀 수정 후 드러난 약점:

1. **매직 스트링 분산** — task-ID 리터럴(`"core"`/`"screenPipeline"`/`"world"`/`"vfxUi"`/`"stages"`/`"enter"`/`"fsm"`)이 `main.cpp` 3 hook 의 `sched.Task(...)` + `.Needs({...})` 에 산재. 단일 출처 없음.
2. **dep 오타 = 런타임 abort** — `.Needs({"vfxui"})`(소문자 오타)는 컴파일은 통과하고 `Validate()` 가 런타임에 "unknown dep" abort. 타입 시스템이 못 잡는 stringly-typed 결함.
3. **tiebreak 이 암묵적** — 같은 위상레벨 다수 task 의 실행 순서가 *등록(호출) 순서* 로 결정. 호출 순서를 바꾸면 거동이 바뀌는 *implicit* 결합. 우선순위가 코드에 명시되지 않음.

> orbital 버그(2026-06-20)는 *의미적* 순서 결함이었지만, 그 수정 과정에서 위 stringly-typed + implicit-tiebreak 약점이 부각됨.

## 2. 목표 / 비목표

**목표**
- task-ID 를 **`enum class EInitTask`** 로: 매직스트링 제거 + 단일 출처(카탈로그).
- **dep 오타를 컴파일 에러로** (런타임 abort 아님) — `Deps` 가 enum 타입.
- **enum 값 = 명시적 int 우선순위 tiebreak** — 등록순(암묵) 대체. 호출 순서와 무관하게 우선순위가 enum 정의 순서로 *명시*.
- 선언적 `.Needs(...)` 위상정렬은 **hard 제약으로 보존** (orbital 수정 방식 그대로).

**비목표 (이번 범위 밖)**
- **템플릿 제네릭화** (`InitScheduler<TId>`) — 엔진 `SJH::init` 승격 시점의 일(YAGNI). 현재 클라 거주라 enum-결합이 정직.
- **다른 매직스트링** (`"mat_pass_"` 접두사, FindChild 액터명 `"WaveSpawner"`/`"BgmActor"`/`"GameContext"`/`"FxRoot"`) — 별도 작업.
- **병렬/affinity 거동 변경** — `EAffinity` 분류만 유지(T2 직렬 불변).

## 3. 확정 결정 (brainstorm)

| ID | 결정 | 근거 |
|----|------|------|
| **E-1** | **우선순위 = tiebreak (deps 유지)** — enum 값이 *같은 위상레벨에서 누가 먼저*. `.Needs(...)` 위상정렬은 hard 제약으로 보존 | 선언적 의존(orbital 수정 방식) + 명시적 우선순위 둘 다. 절대순서(deps 대체)는 load-before-consume 를 수동 우선순위로 회귀 = 방금 없앤 버그류 재발이라 거부 |
| **E-2** | **enum-결합 (A)** — 스케줄러가 `EInitTask` 직접 사용. 템플릿 제네릭(B) 아님 | InitScheduler 는 `apps/_MyApp_/src/Bootstrap/` *클라 코드*, `EInitTask` 는 이 클라 task 목록 -> 결합 자연. string-제네릭은 의도된 재사용 API 아닌 우연. 제네릭화는 엔진 승격 시(spec 단계 정합). 최소 churn(.cpp 구조 유지) |
| **E-3** | **scope = task-ID enum 만** — 다른 매직스트링 미포함 | 사용자가 가리킨 범위(InitScheduler). 집중 변경 |
| **E-4** | **`EInitTask` 별도 헤더 `InitTaskId.h`** | "task 목록 데이터"(자주 변함)와 "스케줄러 메커니즘"(안정) 분리. task 추가/재정렬 = 이 파일만 |
| **E-5** | **enum 값 순서 = phase/load 실행 순서** | enum 값이 곧 tiebreak 우선순위라, 의도된 실행 순서로 선언하면 tiebreak 가 자연스러운 순서를 산출 + `.Needs` 가 hard 보강 |

## 4. 아키텍처

### 4.1 `InitTaskId.h` (신규) — task 카탈로그

```cpp
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

> enum 값은 명시 안 함(0,1,2,... 자동) — 순서가 곧 우선순위. `Count` sentinel 은 불필요(switch 가 전 case 처리, dense 가정 불요).

### 4.2 `InitScheduler.h` — 타입만 string -> enum

```cpp
#include "apps/_MyApp_/src/Bootstrap/InitTaskId.h"   // EInitTask

struct InitTask
{
	EInitTask                Id;        // string -> enum
	std::vector<EInitTask>   Deps;      // vector<string> -> vector<enum>
	EAffinity                Affinity = EAffinity::Cpu;
	std::function<bool()>    Run;
};

// 빌더/스케줄러 시그니처:
//   InitTaskBuilder Task(EInitTask id);
//   InitTaskBuilder &Needs(std::vector<EInitTask> deps);
//   (.Gl()/.Cpu()/.Does()/RunAll() 동일)
//   InitTaskBuilder ctor: (InitScheduler&, EInitTask)
//   멤버: InitTask mTask;  (Id 가 enum)
```
`<string>` include 는 더 이상 Id/Deps 용으로 불요(다른 사용 없으면 제거 — plan 에서 확인). `<functional>`/`<vector>` 유지.

### 4.3 `InitScheduler.cpp` — tiebreak 규칙 변경 (핵심 거동 변화)

- **tiebreak**: `ExecuteInOrder` 가 ready task 중 *등록 인덱스 최소* 대신 **`static_cast<int>(Id)` 최소**(enum 값) 선택. → 호출 순서 무관, 우선순위가 enum 정의 순서로 명시.
- **내부 자료구조**: `unordered_map<string,...>` / `unordered_set<string>` 를 enum 키(또는 `static_cast<int>` 인덱스)로 전환. enum 이 작은 집합이라 `std::vector<bool> done` + 선형 스캔(현행 O(V^2)) 유지로 충분 — id 비교만 enum 으로. (구체 자료구조는 plan.)
- **Validate**: ① 중복 id(같은 `EInitTask` 2회 등록) ② 미등록 dep(`.Needs` 의 enum 이 *이 phase 스케줄러*에 미등록 — cross-phase 오선언 가드) 유지. **"오타 dep 이름" 에러 클래스는 소멸**(컴파일타임 차단). 사이클 검출 유지.
- **진단 메시지**: 전부 `ToString(id)` 로 — "task 'World' depends on unregistered 'Enter'" 식 가독.

### 4.4 `main.cpp` 호출부 (7 task)

```cpp
// OnResourcesReady
sched.Task(EInitTask::Core).Gl().Does([&]{...});
// OnSceneSetup
sched.Task(EInitTask::ScreenPipeline).Gl().Does([&]{...});
sched.Task(EInitTask::VfxUi).Needs({EInitTask::ScreenPipeline}).Gl().Does([&]{...});
sched.Task(EInitTask::World).Needs({EInitTask::VfxUi}).Gl().Does([&]{...});
sched.Task(EInitTask::Stages).Needs({EInitTask::ScreenPipeline, EInitTask::World}).Cpu().Does([&]{...});
// OnBeforeFirstFrame
sched.Task(EInitTask::Enter).Gl().Does([&]{...});
sched.Task(EInitTask::Fsm).Needs({EInitTask::Enter}).Gl().Does([&]{...});
```
`main.cpp` 에 `#include "apps/_MyApp_/src/Bootstrap/InitTaskId.h"` 추가(또는 InitScheduler.h 가 전파하므로 불요 — plan 확인). 람다 본문/딴 로직 무변경.

## 5. 에러 처리 / 결정성

- **컴파일타임**: dep 오타 불가(enum). 미정의 task 참조 불가.
- **런타임 Validate(하드에러 유지)**: 중복 id / 미등록 dep / 사이클 → `ToString` 메시지 + `std::abort()`.
- **결정성**: tiebreak 가 enum 값(전역 안정 순서)이라 *호출 순서 무관* 재현 가능. 등록순 tiebreak 보다 *더* 결정적(호출부 재배치에 불변).
- **F-2 fail-fast** 불변.

## 6. 마이그레이션 / 영향

| 파일 | 변경 |
|------|------|
| `apps/_MyApp_/src/Bootstrap/InitTaskId.h` | **신규** (enum + ToString, 헤더온리) |
| `apps/_MyApp_/src/Bootstrap/InitScheduler.h` | `InitTask.Id/Deps` enum 화, 빌더/스케줄러 시그니처 enum, `InitTaskId.h` include |
| `apps/_MyApp_/src/Bootstrap/InitScheduler.cpp` | tiebreak enum 값, 내부 자료구조 enum 키, 진단 `ToString` |
| `apps/_MyApp_/main.cpp` | 7 `sched.Task(...)`/`.Needs({...})` 를 enum 으로 |
| CMake | **변경 없음** (InitTaskId.h 헤더온리, InitScheduler.cpp 유지) |

**검증**: 빌드 GREEN + GUI 육안(특히 orbital 배경 + 전체 init 동일). 거동은 동일해야 정상.

> **거동 무변경 보장(강)**: 현 7-task 그래프는 deps 가 전 task 를 *직렬화*해 어느 시점에도 ready task 가 2개 이상이 되지 않는다 -> **tiebreak(등록순이든 enum값이든) 가 실제로 발화하지 않음**. 따라서 tiebreak 규칙 변경은 현 그래프에서 *inert* 하고 실행 순서는 100% 불변(`Core -> ScreenPipeline -> VfxUi -> World -> Stages -> Enter -> Fsm`). enum-값 tiebreak 의 가치는 *미래* 에 dep 없는 독립 task 가 동시 ready 될 때의 결정적·명시적 우선순위(호출 순서 비의존)다.

## 7. Decision Log

| 날짜 | 결정 | 비고 |
|------|------|------|
| 2026-06-20 | E-1 우선순위=tiebreak(deps 유지) | 절대순서(deps 대체) 거부 — 선언적 의존 회귀 |
| 2026-06-20 | E-2 enum-결합(A) | 템플릿(B) 은 엔진 승격 시. 클라 거주라 결합 정직 |
| 2026-06-20 | E-3 scope=task-ID enum 만 | 다른 매직스트링 별도 |
| 2026-06-20 | E-4 별도 `InitTaskId.h` | task 목록 vs 메커니즘 분리 |
| 2026-06-20 | E-5 enum 순서=실행 순서 | tiebreak 가 자연 순서 산출 + .Needs hard 보강 |
