# M2 Implementation Plan — SJH::fsm 모듈 + Player WASD + Camera follow

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> ## 📊 진행 상황 (최종 갱신: 2026-05-25)
>
> ### 완료 ✅
>
> | Task | 산출 commit | 상태 | 비고 |
> |---|---|---|---|
> | **P1** SJH::fsm 코어 모듈 | `78dea2a` → `3bdf499` (4-stage 진화) | ✅ 구조 완료 | Stage 4 = `StateMachine<TState, TOwner>` + `IFsmState` 응집. **정본 spec: [`../specs/2026-05-25-fsm-object-state-machine-design.md`](../specs/2026-05-25-fsm-object-state-machine-design.md)** |
> | **P2** PlayerController + TargetFollowableCameraController + main.cpp | `c1012ea` + 이번 세션 cleanup | ⚠️ 대부분 완료 | Camera follow 정착 (별도 클래스), PlayerController 부착, 디버그 spdlog 제거 ✓ |
> | **P3** CLAUDE.md Active Target Management | (해당 commit) | ✅ | `_MyApp_` 활성 첫 줄, audio_demo 포함 6개 모두 활성 반영 |
>
> ### 잔여 ⚠️
>
> 1. **P2 의 Movement wiring 누락** — main.cpp 의 sprite Actor 셋업에서 `Movement` Component 부착 안 됨 + `PlayerController::SetMovableTarget(IMovable*)` 호출 안 됨 → Player 가 실제로 *런타임에 안 움직임* (DoForward 호출 시 nullptr 가드로 no-op). **사용자가 직접 실행 예정** (PlayerActor.h `CreatePlayerActor(cfg)` factory 활용).
> 2. **P1 단위 테스트 미작성** — Stage 3 에서 `test_fsm.cpp` 폐기. 사용자 명시 거부로 *작성 안 함*. 본 plan 의 *5 TEST_CASE 작성 요구* 가 무효화됨.
> 3. **FSM 실 사용처 0** — `StateMachine<TState, TOwner>` 헤더가 존재하나 `PlayerStateMachine` 등 사용처 없음. M2 P2 의 첫 인스턴스화로 실증 예정.
>
> ### 진화 history (P1 의 4 commit chain)
>
> | Stage | Commit | 변화 |
> |---|---|---|
> | Stage 1 | `78dea2a` | `StateMachineProcessor<TState, TTransit>` switch-on-enum (본 plan 의 P1 원본) |
> | Stage 2 | `16a6cdd` | `ObjectStateMachine` + `IFsmState<TOwner>` 도입 (4 엔진 정통 흡수) |
> | Stage 3 | `6f5346c` | rename + Stage 1 폐기 (`state_machine_processor.h` + `test_fsm.cpp` 제거) |
> | **Stage 4** | `3bdf499` | **`StateMachine<TState, TOwner>` + `GetTransitFlag()` 응집** (TTransit template parameter 제거) |
>
> ### 본 plan 의 P1 prompt 와 *현재 상태* 차이
>
> 본 plan 의 P1 prompt (line 67-418) 는 *Stage 1 원본* 만 명세. *진화한 정본 spec* 은 별도 ([`../specs/2026-05-25-fsm-object-state-machine-design.md`](../specs/2026-05-25-fsm-object-state-machine-design.md)) 로 분리. *P1 prompt 자체는 historical record 로 보존*.
>
> ### 사용자 추가 작업 (plan 범위 외)
>
> | Commit | 작업 |
> |---|---|
> | `5198096` | Stat Modifier System (Algebraic::Numeric::Stat) |
> | `5478421` | Movement Input/Logic 분리 (역할 분리 리팩토링) |
>
> ### 다음 작업 (사용자 직접 진행)
>
> A. **main.cpp Movement wiring** — `apps/_MyApp_/src/Entity/Player/PlayerActor.h` 의 `CreatePlayerActor(cfg)` factory 호출로 sprite Actor 셋업 교체. 효과: Player WASD 시각 검증 첫 가능.
>
> ---

> **For agentic workers:** 본 plan 의 3개 task (P1/P2/P3) 는 *완전 독립* — 동시 dispatch 가능 (충돌 영역 0). 각 task 의 prompt 블록을 *그대로 복사* 하여 fresh subagent 에게 전달. 각 task 후 spec compliance + code quality 2-stage review.

**Goal:** M2 마일스톤 — (a) `SJH::fsm` 코어 모듈 신설 + 단위 테스트 + FSMTickSystem 자유함수, (b) Player Actor 가 WASD 로 XZ 평면 이동 + Camera 가 Player follow, (c) M1 follow-up — CLAUDE.md `_MyApp_` 활성 상태 갱신.

**Architecture:**
- `SJH::FSM::StateMachineProcessor<TState, TTransit>` template (헤더-only, uint64_t 비트 기반, virtual OnEnter/OnUpdate/OnExit). spec §6.0.2 정본
- `apps/_MyApp_/src/InputHandler/PlayerController` (Component, sprite Actor 부착) → Transform.Translate XZ 이동
- `apps/_MyApp_/src/InputHandler/CameraController` follow 모드 — `SetFollowTarget(Actor*)` 후 매 Update 에서 target Translate + offset 추적
- gameplay 모델: Player free-move on XZ, Camera 가 Player 머리 위에서 일정 거리 follow + Mouse 로 yaw/pitch 추가 회전

**Tech Stack:** C++17, CMake 3.14+, OpenGL 4.1 Core, vmath (sb7), Catch2 v3, SJH::engine 우산 (M1 정착).

---

## 병렬 dispatch 전략 — 충돌 영역 0

| 파일 | P1 | P2 | P3 |
|---|---|---|---|
| `src/fsm/{CMakeLists.txt, state_machine_processor.h}` | ✏️ create | — | — |
| `src/CMakeLists.txt` (1 line + 우산) | ✏️ +2 lines | — | — |
| `test/test_fsm.cpp` | ✏️ create | — | — |
| `test/CMakeLists.txt` (1 block) | ✏️ +block | — | — |
| `apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}` | — | ✏️ create | — |
| `apps/_MyApp_/src/InputHandler/CameraController.{h,cpp}` | — | ✏️ modify | — |
| `apps/_MyApp_/src/InputHandler/CMakeLists.txt` | — | ✏️ +1 source | — |
| `apps/_MyApp_/main.cpp` | — | ✏️ modify | — |
| `.claude/CLAUDE.md` | — | — | ✏️ modify |

**겹침 0** — 3 subagent 동시 dispatch 안전.

---

## File Structure (Locked-in)

### Create
| 파일 | 책임 |
|---|---|
| `src/fsm/CMakeLists.txt` | `sjh_fsm` INTERFACE library (header-only) + `SJH::fsm` ALIAS |
| `<src>/fsm/state_machine_processor.h` | `StateMachineProcessor<TState, TTransit>` template + spec §6.0.2 의 비트 인코딩 패턴 |
| `test/test_fsm.cpp` | Catch2 v3 단위 테스트 — Bind / TryTransit (AND 판정 + target lookup) / OnEnter+OnExit 발화 순서 |
| `apps/_MyApp_/src/InputHandler/PlayerController.h` | Component 베이스 + Builder + WASD → Transform.Translate XZ |
| `apps/_MyApp_/src/InputHandler/PlayerController.cpp` | PlayerController 구현 |

### Modify
| 파일 | 변경 |
|---|---|
| `src/CMakeLists.txt` | `add_subdirectory(fsm)` + `SJH::engine` 우산 `target_link_libraries` 에 `SJH::fsm` 추가 |
| `test/CMakeLists.txt` | `test_fsm` executable 등록 + umbrella `tests` 의존에 추가 |
| `apps/_MyApp_/src/InputHandler/CameraController.{h,cpp}` | `SetFollowTarget(SJH::Scene::Actor*)` + follow mode (`Update` 가 target Transform.Translate 추적, Mouse 는 yaw/pitch 상대 회전 유지) |
| `apps/_MyApp_/src/InputHandler/CMakeLists.txt` | `PlayerController.cpp` 추가 |
| `apps/_MyApp_/main.cpp` | sprite Actor 에 `PlayerController` 부착, Camera 의 `CameraController.SetFollowTarget(spriteActor)` 셋업. **디버그 spdlog (line 157-161) 제거**. Camera 초기 Translate 는 sprite 머리 위로 (예: y=5, z=5 그대로) |
| `.claude/CLAUDE.md` | "Active Target Management" 섹션의 `_MyApp_` 을 *임시 비활성* → *활성* 으로 이동 + M1 완료 설명 1줄 |

---

## P1 — `SJH::fsm` 코어 모듈 + 단위 테스트

**Files (자가완결)**:
- Create: `src/fsm/CMakeLists.txt`
- Create: `<src>/fsm/state_machine_processor.h`
- Create: `test/test_fsm.cpp`
- Modify: `src/CMakeLists.txt` (2 line — `add_subdirectory(fsm)` + 우산 link)
- Modify: `test/CMakeLists.txt` (1 block — test_fsm 등록 + umbrella 추가)

### Subagent Prompt — P1

```
You are implementing **M2 P1** of the topdown-shooter milestone — `SJH::fsm` 코어 모듈 신설 + 단위 테스트.

## Task Description

신규 `SJH::fsm` 코어 STATIC 라이브러리 (헤더-only) 추가. `StateMachineProcessor<TState, TTransit>` template 만 — spec §6.0.2 의 비트 enum 기반 FSM. Catch2 단위 테스트로 Bind / TryTransit (AND 판정) / OnEnter+OnExit 발화 순서 검증.

### Step 1: `<src>/fsm/state_machine_processor.h` 작성

```cpp
#ifndef __SJH_FSM_STATE_MACHINE_PROCESSOR_H__
#define __SJH_FSM_STATE_MACHINE_PROCESSOR_H__

#include "scene/actor.h"   // SJH::Scene::Component
#include <cstdint>
#include <unordered_map>

namespace SJH::FSM
{
    /// @brief 비트 enum 기반 FSM Component 베이스. spec §6.0.2 정본.
    /// @tparam TState   `enum class : uint64_t` — 각 enumerator 가 1비트 (NONE=0 약속)
    /// @tparam TTransit `enum class : uint64_t` — 각 enumerator = *허용된 from 상태들의 bitwise OR*
    ///
    /// ### 사용 패턴
    /// @code
    /// enum class PlayerState : uint64_t {
    ///     NONE = 0, Idle = 1ull<<0, Move = 1ull<<1, Attack = 1ull<<2,
    /// };
    /// enum class PlayerTransit : uint64_t {
    ///     ToIdle = (uint64_t)PlayerState::Move | (uint64_t)PlayerState::Attack,
    ///     ToMove = (uint64_t)PlayerState::Idle,
    /// };
    /// class MyFSM : public SJH::FSM::StateMachineProcessor<PlayerState, PlayerTransit> {
    /// public:
    ///     MyFSM() {
    ///         Bind(PlayerTransit::ToIdle, PlayerState::Idle);
    ///         Bind(PlayerTransit::ToMove, PlayerState::Move);
    ///     }
    ///     void OnEnter(PlayerState s) override { ... }
    /// };
    /// @endcode
    template<typename TState, typename TTransit>
    class StateMachineProcessor : public SJH::Scene::Component
    {
    public:
        using StateU = uint64_t;

        StateMachineProcessor() = default;
        explicit StateMachineProcessor(TState startup) : current_(startup) {}

        /// @brief transit → target 매핑 등록. 생성자에서 모든 transit 일괄 등록 권장.
        void Bind(TTransit transit, TState target)
        {
            targetOf_[(StateU)transit] = target;
        }

        /// @brief transition 시도. allowed-from set 에 current 가 포함되면 OnExit→상태 교체→OnEnter 발화 후 true.
        /// @return false 시 (a) current==NONE, (b) from 비매치, (c) target 미등록 중 하나.
        bool TryTransit(TTransit transit)
        {
            const StateU allowedFrom = (StateU)transit;
            const StateU curr        = (StateU)current_;
            if (curr == 0) return false;
            if ((allowedFrom & curr) != curr) return false;
            auto it = targetOf_.find((StateU)transit);
            if (it == targetOf_.end()) return false;
            OnExit(current_);
            current_ = it->second;
            OnEnter(current_);
            return true;
        }

        /// @brief 매 프레임 OnUpdate 호출 — FSMTickSystem 이 자동.
        void Tick(float dtSeconds) { OnUpdate(current_, dtSeconds); }

        TState State() const { return current_; }

        // === Component 베이스 (SJH::Scene::Component) 의 순수 가상 구현 ===
        // 본 클래스는 추상이 아님 — 파생 클래스가 OnEnter/OnUpdate/OnExit override 안 해도
        // 빈 동작으로 작동 (즉 transit 만 추적). 단 통상은 OnEnter 만 override.
        void OnEnter() override {}                  // Scene::Component lifecycle (Actor 진입 시 1회)
        void OnExit()  override {}                  // Scene::Component lifecycle (Actor 이탈 시 1회)
        void Update(float dt) override { Tick(dt); } // 매 프레임 — Tick 위임

        // === FSM 가상함수 (파생 클래스가 override) ===
        virtual void OnEnter(TState /*s*/)              {}
        virtual void OnUpdate(TState /*s*/, float /*dt*/) {}
        virtual void OnExit(TState /*s*/)               {}

    private:
        TState current_ = TState::NONE;   // enum 에 NONE = 0 약속
        std::unordered_map<StateU, TState> targetOf_;
    };

}  // namespace SJH::FSM

#endif // __SJH_FSM_STATE_MACHINE_PROCESSOR_H__
```

### Step 2: `src/fsm/CMakeLists.txt`

헤더-only 라 INTERFACE library:

```cmake
# SJH::fsm — 비트 enum 기반 FSM template (spec §6.0)
add_library(sjh_fsm INTERFACE)
add_library(SJH::fsm ALIAS sjh_fsm)

target_include_directories(sjh_fsm INTERFACE
    ${CMAKE_SOURCE_DIR}/src
)

# 헤더에 SJH::Scene::Component 노출 → PUBLIC dependency
target_link_libraries(sjh_fsm INTERFACE
    SJH::scene
)
```

### Step 3: `src/CMakeLists.txt` 수정

Task 1 (M1) 의 `add_subdirectory(sprite)` 와 동일 패턴. `scene` 직후 또는 알파벳 순:

기존:
```cmake
add_subdirectory(scene)
add_subdirectory(sprite)    # ← 추가 (M1)
add_subdirectory(shader)
```

다음으로 변경:
```cmake
add_subdirectory(scene)
add_subdirectory(sprite)    # ← 추가 (M1)
add_subdirectory(fsm)       # ← 추가 (M2)
add_subdirectory(shader)
```

그리고 `target_link_libraries(sjhopengl_engine INTERFACE ...)` 의 `SJH::sprite` 다음 줄에 `SJH::fsm` 추가:

```cmake
target_link_libraries(sjhopengl_engine INTERFACE
    SJH::common
    SJH::buffer
    SJH::diagnostics
    SJH::input
    SJH::layout
    SJH::material
    SJH::object
    SJH::program
    SJH::render
    SJH::resource_registry
    SJH::scene
    SJH::shader
    SJH::sprite          # M1 — 13 모듈 (sprite 합류)
    SJH::fsm             # M2 — 14 모듈 (fsm 합류)
)
```

### Step 4: `test/test_fsm.cpp` 작성

```cpp
#include "<fsm>/state_machine_processor.h"
#include <<catch2>/catch_test_macros.hpp>
#include <cstdint>

namespace
{
    // 테스트용 enum — Idle/Move/Attack 3 상태
    enum class TestState : uint64_t {
        NONE   = 0,
        Idle   = 1ull << 0,
        Move   = 1ull << 1,
        Attack = 1ull << 2,
    };
    enum class TestTransit : uint64_t {
        // ToIdle 은 Move 또는 Attack 에서 가능
        ToIdle   = (uint64_t)TestState::Move | (uint64_t)TestState::Attack,
        // ToMove 는 Idle 에서만 가능
        ToMove   = (uint64_t)TestState::Idle,
        // ToAttack 은 Idle 또는 Move 에서 가능
        ToAttack = (uint64_t)TestState::Idle | (uint64_t)TestState::Move,
    };

    /// @brief OnEnter / OnExit 발화 추적용 spy FSM
    class SpyFSM : public SJH::FSM::StateMachineProcessor<TestState, TestTransit>
    {
    public:
        SpyFSM() : SJH::FSM::StateMachineProcessor<TestState, TestTransit>(TestState::Idle)
        {
            Bind(TestTransit::ToIdle,   TestState::Idle);
            Bind(TestTransit::ToMove,   TestState::Move);
            Bind(TestTransit::ToAttack, TestState::Attack);
        }

        std::vector<TestState> EnterLog;
        std::vector<TestState> ExitLog;

        void OnEnter(TestState s) override { EnterLog.push_back(s); }
        void OnExit(TestState s)  override { ExitLog.push_back(s); }
    };
}

TEST_CASE("StateMachineProcessor — initial state via constructor", "[fsm]")
{
    SpyFSM fsm;
    REQUIRE(fsm.State() == TestState::Idle);
}

TEST_CASE("StateMachineProcessor — TryTransit AND 판정", "[fsm]")
{
    SpyFSM fsm;

    SECTION("Idle → Move 가능 (ToMove 의 from = Idle)") {
        REQUIRE(fsm.TryTransit(TestTransit::ToMove));
        REQUIRE(fsm.State() == TestState::Move);
    }
    SECTION("Idle → Attack 가능 (ToAttack 의 from = Idle | Move)") {
        REQUIRE(fsm.TryTransit(TestTransit::ToAttack));
        REQUIRE(fsm.State() == TestState::Attack);
    }
    SECTION("Idle → Idle 불가 (ToIdle 의 from = Move | Attack)") {
        REQUIRE_FALSE(fsm.TryTransit(TestTransit::ToIdle));
        REQUIRE(fsm.State() == TestState::Idle);   // 상태 변경 없음
    }
}

TEST_CASE("StateMachineProcessor — OnEnter / OnExit 발화 순서", "[fsm]")
{
    SpyFSM fsm;

    // 초기 transit: Idle → Move
    REQUIRE(fsm.TryTransit(TestTransit::ToMove));
    REQUIRE(fsm.ExitLog.size() == 1);
    REQUIRE(fsm.ExitLog[0] == TestState::Idle);     // OnExit(이전 state)
    REQUIRE(fsm.EnterLog.size() == 1);
    REQUIRE(fsm.EnterLog[0] == TestState::Move);    // OnEnter(새 state)

    // 두 번째 transit: Move → Attack
    REQUIRE(fsm.TryTransit(TestTransit::ToAttack));
    REQUIRE(fsm.ExitLog.size() == 2);
    REQUIRE(fsm.ExitLog[1] == TestState::Move);
    REQUIRE(fsm.EnterLog.size() == 2);
    REQUIRE(fsm.EnterLog[1] == TestState::Attack);

    // 실패 transit: Attack → Move 불가 (ToMove 의 from = Idle only)
    REQUIRE_FALSE(fsm.TryTransit(TestTransit::ToMove));
    REQUIRE(fsm.ExitLog.size() == 2);   // 발화 없음
    REQUIRE(fsm.EnterLog.size() == 2);
}

TEST_CASE("StateMachineProcessor — NONE 상태에서 transit 실패", "[fsm]")
{
    SJH::FSM::StateMachineProcessor<TestState, TestTransit> fsm;   // default = NONE
    fsm.Bind(TestTransit::ToMove, TestState::Move);
    REQUIRE(fsm.State() == TestState::NONE);
    REQUIRE_FALSE(fsm.TryTransit(TestTransit::ToMove));   // current==NONE → 항상 false
}

TEST_CASE("StateMachineProcessor — target 미등록 transit 실패", "[fsm]")
{
    SJH::FSM::StateMachineProcessor<TestState, TestTransit> fsm(TestState::Idle);
    // Bind 호출 안 함 — targetOf_ 비어있음
    REQUIRE_FALSE(fsm.TryTransit(TestTransit::ToMove));
    REQUIRE(fsm.State() == TestState::Idle);   // 상태 유지
}
```

### Step 5: `test/CMakeLists.txt` 에 test_fsm 등록

기존 `test_uniform_atlas` (M1) 블록 (~line 235-243 부근) 직후에 추가:

```cmake
#  Phase M2 — SJH::fsm StateMachineProcessor (GL 불요, 헤더-only)
add_executable(test_fsm test_fsm.cpp)
target_link_libraries(test_fsm PRIVATE
    Catch2::Catch2WithMain
    SJH::fsm                # StateMachineProcessor template
)
target_compile_features(test_fsm PRIVATE cxx_std_17)
catch_discover_tests(test_fsm)
```

또한 같은 파일의 umbrella `tests` target (`add_custom_target(tests DEPENDS ...)`) 의존 목록에 `test_fsm` 추가 (test_uniform_atlas 가 추가된 같은 위치).

### Step 6: 빌드 + 테스트

```bash
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target test_fsm
ctest --test-dir build_ninja -R "StateMachineProcessor" -V
```

기대: 5 TEST_CASE 모두 PASS. SJH::engine 우산 사용 데모 (tweeny_demo / _MyApp_) 빌드도 영향 없음.

### Step 7: Single commit

```bash
git add src/fsm/CMakeLists.txt <src>/fsm/state_machine_processor.h \
        src/CMakeLists.txt test/test_fsm.cpp test/CMakeLists.txt
git commit -m "feat(fsm): SJH::fsm 코어 모듈 신설 + StateMachineProcessor template

- src/fsm/ 신설 — INTERFACE library (header-only)
- StateMachineProcessor<TState, TTransit> template — spec §6.0.2 비트 enum 기반
- Bind(transit, target) + TryTransit(allowed-from AND 판정) + OnEnter/OnUpdate/OnExit 가상함수
- SJH::engine 우산 (코어 13 → 14 모듈)
- test_fsm — Catch2 v3, 5 TEST_CASEs (init / AND 판정 / Enter+Exit 순서 / NONE 가드 / target 미등록 가드)

M2 P1."
```

## Context

**Repository:** `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
**Branch:** `game/module/sprite`

**Reference**:
- spec §6.0.2 (`doc/superpowers/specs/2026-05-24-topdown-shooter-design.md`) — `StateMachineProcessor` 정본
- `src/scene/actor.h:31-33` — `SJH::Scene::Component` 의 순수 가상 메서드 (OnEnter/OnExit/Update)
- `src/sprite/CMakeLists.txt` — INTERFACE 가 아닌 STATIC 패턴 (참고용. fsm 은 INTERFACE 가 정통 — 헤더-only)

**Conventions**:
- 한국어 주석 OK
- Header guard `__SJH_FSM_STATE_MACHINE_PROCESSOR_H__` (NOT `#pragma once`)
- Catch2 v3 (`<<catch2>/catch_test_macros.hpp>`)

**DO NOT touch**: `apps/_MyApp_/*`, `.claude/CLAUDE.md` — 별도 subagent (P2, P3) 가 담당.

## 보강 사항 — INTERFACE library 본 저장소 최초 시도

본 `SJH::fsm` 은 **본 저장소에서 최초의 INTERFACE-only SJH 모듈** (다른 코어 12+1 모듈은 모두 STATIC). 헤더-only 라 INTERFACE 가 정통 — 단 `cmake/Dependency.cmake` 의 `stb_extra` / `tweeny` INTERFACE 타겟 패턴 참조 가능.

만약 빌드 중 INTERFACE 가 *expected behavior 와 다름* (e.g., `target_link_libraries` 가 INTERFACE 에서 작동 안 함) — STATIC 으로 fallback 가능. STATIC 으로 가면 `.cpp` 1개 필요 (예: `state_machine_processor.cpp` 에 explicit template instantiation 또는 빈 namespace). 단 INTERFACE 가 우선 시도.

## Self-Review

- 5 TEST_CASE 모두 PASS?
- 헤더 guard / namespace / Component 베이스 상속?
- INTERFACE library (`.cpp` 없음, INTERFACE include propagation)?
- 우산 link `SJH::fsm` 추가?
- 단일 commit, 메시지 형식 정확?

## Report

Status: DONE / DONE_WITH_CONCERNS / BLOCKED / NEEDS_CONTEXT
+ Files changed (5 files)
+ Test output (5 cases PASS)
+ Git commit SHA

Work from: `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
```

---

## P2 — PlayerController + Camera follow + main.cpp 정리

**Files (자가완결)**:
- Create: `apps/_MyApp_/src/InputHandler/PlayerController.h`
- Create: `apps/_MyApp_/src/InputHandler/PlayerController.cpp`
- Modify: `apps/_MyApp_/src/InputHandler/CameraController.h` (follow mode 멤버 + setter)
- Modify: `apps/_MyApp_/src/InputHandler/CameraController.cpp` (follow mode Update)
- Modify: `apps/_MyApp_/src/InputHandler/CMakeLists.txt` (PlayerController.cpp 추가)
- Modify: `apps/_MyApp_/main.cpp` (sprite 에 PlayerController 부착 + Camera follow target 셋업 + 디버그 spdlog 제거)

### Subagent Prompt — P2

```
You are implementing **M2 P2** of the topdown-shooter milestone — Player Actor 가 WASD 로 XZ 이동, Camera 가 Player follow.

## Task Description

현 `apps/_MyApp_/main.cpp` 는 Camera Actor 에 `CameraController` 가 부착되어 *카메라 자체가 free-fly*. M2 spec (부록 D) 의도는 *Player 가 WASD 로 XZ 평면 이동, Camera follow* — 두 컨트롤러 분리:

1. **PlayerController** (신설) — sprite Actor 에 부착. WASD → Owner Transform.Translate XZ 이동
2. **CameraController** (수정) — follow target Actor 보유 + 매 Update 에서 target.Transform.Translate 추적 + Mouse yaw/pitch 는 추가 상대 회전

main.cpp 셋업: sprite Actor 에 PlayerController, Camera Actor 의 CameraController 에 `SetFollowTarget(spriteActor)`. 디버그 spdlog 제거.

### Step 1: 현 `apps/_MyApp_/src/InputHandler/CameraController.{h,cpp}` 정독

이미 정통 패턴 (Component 베이스 + Builder + WASD/Mouse). PlayerController 작성 시 *대칭 패턴* 따르기. CameraController 의 follow mode 도입 시 *기존 free-fly 모드 보존* — `mFollowTarget == nullptr` 이면 기존 free-fly, 있으면 follow.

### Step 2: `apps/_MyApp_/src/InputHandler/PlayerController.h` 작성

```cpp
#ifndef _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
#define _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__

#include "input/keyboard_input.h"
#include "scene/actor.h"
#include <vmath.h>

namespace TopdownShooter::Controller
{
    /// @brief Top-down 게임의 Player 이동 컨트롤러 — WASD → Owner Transform.Translate XZ 이동.
    /// @details
    ///   ### 동작
    ///   - 제어 대상은 *Component 의 owner Actor* 의 Transform (sprite Actor).
    ///   - WASD: +Z/-Z (W=앞, S=뒤, A/D 는 X)
    ///   - 카메라 회전과 독립 — 월드 축 기준 이동 (회전 적용 없음, 탑다운 컨벤션)
    class PlayerController : public SJH::Scene::Component
    {
    public:
        enum class Action : int
        {
            MoveForward = 1, // W
            MoveBack,        // S
            MoveLeft,        // A
            MoveRight,       // D
        };

        PlayerController() = default;
        PlayerController(const PlayerController&)            = delete;
        PlayerController& operator=(const PlayerController&) = delete;

        bool SetUp();

        /// @brief KeyboardInput 의존 주입. SetUp() 전 호출 필수.
        PlayerController& SetKeyboardInput(SJH::KeyboardInput<Action>* k);

        /// @brief 이동 속도 (월드 단위/프레임, default 0.05).
        PlayerController& SetMoveSpeed(float v);

        virtual void OnEnter() override;
        virtual void OnExit() override;
        virtual void Update(float dt) override;

    private:
        bool mIsInitialized = false;
        SJH::KeyboardInput<Action>* mKeyboardInput = nullptr;
        vmath::vec3 mMoveDelta = vmath::vec3(0.0f, 0.0f, 0.0f);   // 매 Update reset → held handler 누적
        float mMoveSpeed = 0.05f;

        void RegisterBindings();
        void UnregisterBindings();
    };
}

#endif // _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
```

### Step 3: `apps/_MyApp_/src/InputHandler/PlayerController.cpp` 작성

```cpp
#include "PlayerController.h"
#include <<spdlog>/spdlog.h>

namespace TopdownShooter::Controller
{
    PlayerController& PlayerController::SetKeyboardInput(SJH::KeyboardInput<Action>* k)
    {
        mKeyboardInput = k;
        return *this;
    }

    PlayerController& PlayerController::SetMoveSpeed(float v)
    {
        mMoveSpeed = v;
        return *this;
    }

    bool PlayerController::SetUp()
    {
        if (mIsInitialized) return true;
        if (!mKeyboardInput) {
            spdlog::error("[PlayerController] SetUp failed — KeyboardInput 미주입");
            return false;
        }
        RegisterBindings();
        mIsInitialized = true;
        return true;
    }

    void PlayerController::OnEnter() {}
    void PlayerController::OnExit()
    {
        if (mKeyboardInput) UnregisterBindings();
    }

    void PlayerController::Update(float /*dt*/)
    {
        if (!mIsInitialized) return;
        auto* owner = GetOwner();
        if (!owner) return;

        // mMoveDelta 가 KeyboardInput held handler 에서 매 프레임 누적됨.
        // 월드 축 기준 이동 (탑다운 — 카메라 회전 무관, X/Z 평면)
        auto& tr = owner->GetTransform();
        tr.Translate += mMoveDelta * mMoveSpeed;
        mMoveDelta = vmath::vec3(0.0f, 0.0f, 0.0f);   // 다음 frame 까지 reset
    }

    void PlayerController::RegisterBindings()
    {
        using K = SJH::KeyboardInput<Action>;
        // 월드 축: +X=오른쪽, -X=왼쪽, +Z=뒤(카메라 쪽), -Z=앞 — OpenGL forward = -Z
        mKeyboardInput->Bind(K::Held, GLFW_KEY_W, Action::MoveForward,
            [this](){ mMoveDelta[2] -= 1.0f; });   // 앞 = -Z
        mKeyboardInput->Bind(K::Held, GLFW_KEY_S, Action::MoveBack,
            [this](){ mMoveDelta[2] += 1.0f; });   // 뒤 = +Z
        mKeyboardInput->Bind(K::Held, GLFW_KEY_A, Action::MoveLeft,
            [this](){ mMoveDelta[0] -= 1.0f; });
        mKeyboardInput->Bind(K::Held, GLFW_KEY_D, Action::MoveRight,
            [this](){ mMoveDelta[0] += 1.0f; });
    }

    void PlayerController::UnregisterBindings()
    {
        // KeyboardInput 의 Unbind API 가 없으면 (없을 가능성) 빈 구현.
        // mIsInitialized = false 로 가드되니 OnExit 후 Update 호출 무해.
    }
}
```

> **주의**: `KeyboardInput<TAction>::Bind` 의 정확한 시그니처는 `src/input/keyboard_input.h` 확인 필요. CameraController.cpp 의 Bind 호출 패턴을 *그대로 모방*. 위 코드의 `K::Held`/`Bind` 시그니처가 실제와 다르면 CameraController 와 동일 형태로 정정.

### Step 4: `apps/_MyApp_/src/InputHandler/CameraController.{h,cpp}` 수정 — follow mode 추가

#### CameraController.h — `SetFollowTarget` 추가

기존 멤버 다음에 추가:

```cpp
public:
    /// @brief Follow target Actor 설정. nullptr 이면 기존 free-fly 모드 유지.
    /// @details Update 가 매 프레임 target.Transform.Translate + mFollowOffset 으로 카메라 위치 갱신.
    ///          Mouse yaw/pitch 는 그대로 작동 — target 머리 위에서 *상대 회전*.
    CameraController& SetFollowTarget(SJH::Scene::Actor* t);

    /// @brief Follow 시 target → camera offset (default = vec3(0, 5, 5)).
    CameraController& SetFollowOffset(vmath::vec3 offset);

private:
    SJH::Scene::Actor* mFollowTarget = nullptr;
    vmath::vec3 mFollowOffset = vmath::vec3(0.0f, 5.0f, 5.0f);
```

#### CameraController.cpp — `Update` 의 free-fly 분기 추가

기존 `Update(float dt)` 본문 시작 부분에 follow 분기 추가:

```cpp
void CameraController::Update(float dt)
{
    if (!mIsInitialized || !mCamera) return;
    auto* owner = mCamera->GetOwner();
    if (!owner) return;

    auto& tr = owner->GetTransform();

    // === Follow mode 분기 ===
    if (mFollowTarget) {
        // target Actor 의 Translate + mFollowOffset = camera position
        const auto& targetTr = mFollowTarget->GetTransform();
        tr.Translate = targetTr.Translate + mFollowOffset;
        // Mouse yaw/pitch 누적은 그대로 — target 머리 위에서 상대 회전
        tr.EulerRot = vmath::vec3(mPitchDeg, mYawDeg, 0.0f);
        // WASD 이동은 free-fly 모드에서만 의미. follow 시 mMoveDelta 무시 후 reset.
        mMoveDelta = vmath::vec3(0.0f, 0.0f, 0.0f);
        return;
    }

    // === Free-fly mode (기존 로직) ===
    // ... 기존 코드 그대로 ...
}
```

`SetFollowTarget` / `SetFollowOffset` 구현 추가:

```cpp
CameraController& CameraController::SetFollowTarget(SJH::Scene::Actor* t)
{
    mFollowTarget = t;
    return *this;
}

CameraController& CameraController::SetFollowOffset(vmath::vec3 offset)
{
    mFollowOffset = offset;
    return *this;
}
```

### Step 5: `apps/_MyApp_/src/InputHandler/CMakeLists.txt` 수정

기존:
```cmake
add_library(myapp_input_handler ...
    CameraController.cpp
)
add_library(MyApp::InputHandler ALIAS myapp_input_handler)
```

`PlayerController.cpp` 1줄 추가:
```cmake
add_library(myapp_input_handler ...
    CameraController.cpp
    PlayerController.cpp
)
```

### Step 6: `apps/_MyApp_/main.cpp` 수정

#### 6a. include 추가
```cpp
#include "apps/_MyApp_/src/InputHandler/PlayerController.h"
```

#### 6b. private 멤버 추가
```cpp
SJH::KeyboardInput<Controller::PlayerController::Action> mPlayerKeyboard;
```

> **주의**: `KeyboardInput<TAction>` 는 *Action enum 별 별도 인스턴스* 필요 (template instance 가 다름). 기존 `mKeyboard` 는 `Controller::CameraController::Action` 용 — Player 용 별도 인스턴스 필요.

#### 6c. `startup()` 의 sprite Actor 셋업 수정

기존 (line 113-117):
```cpp
auto spriteActor = std::make_unique<SJH::Scene::Actor>("PlayerSprite");
spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);
spriteActor->AddComponent<SJH::Scene::MeshRenderer>(mPlane.get(), mat);
mSpriteActor = dir.Root().AddChild(std::move(spriteActor));
```

다음으로 교체:
```cpp
auto spriteActor = std::make_unique<SJH::Scene::Actor>("PlayerSprite");
spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);
spriteActor->AddComponent<SJH::Scene::MeshRenderer>(mPlane.get(), mat);
spriteActor->AddComponent<Controller::PlayerController>()
    ->SetKeyboardInput(&mPlayerKeyboard)
    .SetMoveSpeed(0.05f)
    .SetUp();
mSpriteActor = dir.Root().AddChild(std::move(spriteActor));
```

#### 6d. `startup()` 의 Camera CameraController 셋업 수정

기존 (line 100-104):
```cpp
camActor->AddComponent<Controller::CameraController>()
    ->SetKeyboardInput(&mKeyboard)
    .SetMouseInput(&mMouse)
    .SetCamera(cam)
    .SetUp();
```

다음으로 교체 — sprite Actor 가 이미 만들어진 후 `SetFollowTarget` 호출이 필요한데 *순서 이슈*: Camera 가 먼저 셋업되고 sprite 가 나중. 해결: sprite Actor 셋업 *후* Camera follow target 설정. 즉:

```cpp
// === Camera Actor 셋업 (target 설정은 sprite 만든 후) ===
auto camActor = SJH::Scene::CreateCameraActor("MainCamera", 45.0f, aspect, 0.1f, 100.0f);
camActor->GetTransform().Translate = vmath::vec3(0.0f, 5.0f, 5.0f);
camActor->GetTransform().EulerRot = vmath::vec3(-45.0f, 0.0f, 0.0f);
auto* cam = camActor->GetComponent<SJH::Scene::Camera>();
auto* camCtrl = camActor->AddComponent<Controller::CameraController>();
camCtrl->SetKeyboardInput(&mKeyboard)
    .SetMouseInput(&mMouse)
    .SetCamera(cam)
    .SetUp();
cam->SetTargetFramebuffer(nullptr);

mCameraActor = dir.Root().AddChild(std::move(camActor));
mCamera = cam;
dir.SetActiveCamera(cam);

// === sprite Actor 셋업 ===
// ... (위 6c 코드)

// === Camera 에 sprite follow target 설정 ===
camCtrl->SetFollowTarget(mSpriteActor);
camCtrl->SetFollowOffset(vmath::vec3(0.0f, 5.0f, 5.0f));
```

#### 6e. `render()` 의 PollHeld 추가 — Player 키보드도 poll

기존 (line 137):
```cpp
mKeyboard.PollHeld(window);
```

다음으로 변경:
```cpp
mKeyboard.PollHeld(window);
mPlayerKeyboard.PollHeld(window);
```

#### 6f. `onKey` 의 디스패치 추가 + **디버그 spdlog 제거**

기존 (line 154-162):
```cpp
void onKey(int key, int action) override
{
    mKeyboard.Dispatch(key, action);
    spdlog::info("Pressed {} {} {}",
        mCameraActor->GetTransform().Translate[0],
        mCameraActor->GetTransform().Translate[1],
        mCameraActor->GetTransform().Translate[2]
    );
}
```

다음으로 교체 — 디버그 spdlog 제거 + Player keyboard 추가:
```cpp
void onKey(int key, int action) override
{
    mKeyboard.Dispatch(key, action);
    mPlayerKeyboard.Dispatch(key, action);
}
```

### Step 7: 빌드 + 시각 검증

```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

기대:
- 처음: sprite (TestPattern frame 0) 가 화면 중앙에 정면 표시 (M1 결과 그대로)
- **WASD 누르면 sprite 가 XZ 평면 이동**, Camera 가 sprite 머리 위 5/5 offset 으로 follow
- **마우스 우클릭 드래그**: yaw/pitch 회전 — sprite 머리 위에서 시점 회전
- 디버그 콘솔 spdlog 폭주 없음

### Step 8: Single commit

```bash
git add apps/_MyApp_/src/InputHandler/PlayerController.h \
        apps/_MyApp_/src/InputHandler/PlayerController.cpp \
        apps/_MyApp_/src/InputHandler/CameraController.h \
        apps/_MyApp_/src/InputHandler/CameraController.cpp \
        apps/_MyApp_/src/InputHandler/CMakeLists.txt \
        apps/_MyApp_/main.cpp
git commit -m "feat(_MyApp_): M2 Player WASD + Camera follow

- PlayerController Component 신설 — sprite Actor 부착, WASD → Transform.Translate XZ
- CameraController follow mode 추가 — SetFollowTarget(Actor*) + SetFollowOffset
  매 Update 에서 target Translate + offset 으로 카메라 위치 갱신. Mouse yaw/pitch
  는 상대 회전으로 유지.
- main.cpp: sprite Actor 에 PlayerController + Camera 에 SetFollowTarget(sprite)
- 디버그 spdlog (line 157-161) 제거
- mPlayerKeyboard 별도 인스턴스 — PlayerController::Action 용 (CameraController 와 분리)

M2 P2."
```

## Context

**Repository:** `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
**Branch:** `game/module/sprite`

**Reference (read-only)**:
- `apps/_MyApp_/src/InputHandler/CameraController.h/.cpp` — Component + Builder + WASD/Mouse 패턴 (대칭 모방)
- `src/input/keyboard_input.h` — `KeyboardInput<TAction>::Bind` 시그니처 확인
- `src/scene/actor.h:37` — `GetOwner()` 메서드 (Component 가 자기 Actor 접근)

**Spec/decision references**:
- spec 부록 D M2 (`doc/superpowers/specs/2026-05-24-topdown-shooter-design.md`) — gameplay 모델
- 결정 #18 — Box2D Component 는 Client 한정 (Physics 도입 시 M3)

**DO NOT touch**: `src/fsm/`, `src/scene/*`, `src/sprite/*`, `.claude/CLAUDE.md`, 기타 모듈. 별도 subagent (P1, P3) 가 담당.

## 보강 — 필독 사전 작업 + 가드

**필독 사전 작업 (코드 작성 *전*):**
1. **`src/input/keyboard_input.h` 전체 정독** — `KeyboardInput<TAction>::Bind` 의 정확한 시그니처 (`Held` enum 이름 / 콜백 signature) 확인. plan 의 `K::Held` 가정이 실제와 다르면 *그 시점에서 코드 정정* (escalate 가 아니라 *대칭 모방*).
2. **`apps/_MyApp_/src/InputHandler/CameraController.cpp` 의 RegisterBindings 함수 읽기** — Bind 호출 정확 패턴 모방. PlayerController 의 RegisterBindings 가 *같은 구조* 여야 함.

**Actor 비상속 컨벤션 (user memory `compound_actor_pattern.md`):**
> "PreBuilt 는 `src/scene/compound_actor.h` 의 free factory. **Actor 비상속** — 특수 속성은 Component 로만"

→ `PlayerController` 는 *반드시* `SJH::Scene::Component` 베이스 상속 (CameraController 와 동일 패턴). 절대 *`class Player : public Actor`* 같은 Actor 상속 클래스 만들지 말 것.

**KeyboardInput<TAction> 별도 인스턴스 컨벤션:**
`KeyboardInput<Controller::CameraController::Action>` 과 `KeyboardInput<Controller::PlayerController::Action>` 은 *서로 다른 template 인스턴스* — main.cpp 가 *두 별도 인스턴스* (`mKeyboard` + `mPlayerKeyboard`) 보유 필수. 단일 인스턴스 공유 시도 시 *Action enum type 충돌* 컴파일 에러.

## Self-Review

- PlayerController 가 KeyboardInput<TAction>::Bind 시그니처 정확 (CameraController 모방)?
- CameraController follow mode 가 *기존 free-fly mode 보존* (mFollowTarget == nullptr 일 때)?
- main.cpp 의 *sprite 먼저, Camera 의 SetFollowTarget 나중* 순서?
- mPlayerKeyboard (별도 인스턴스) PollHeld + Dispatch 매 프레임 호출?
- 디버그 spdlog 완전 제거?
- 빌드 PASS + 시각 검증 (사용자 실행으로 확인 예정)?

## Report

Status: DONE / DONE_WITH_CONCERNS / BLOCKED / NEEDS_CONTEXT
+ Files changed (6 files)
+ Build result
+ Self-review findings (특히 KeyboardInput Bind 시그니처가 plan 가정과 다를 시 어떻게 정정했는지)
+ Git commit SHA

Work from: `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
```

---

## P3 — `.claude/CLAUDE.md` Active Target Management 갱신

**Files (자가완결)**:
- Modify: `.claude/CLAUDE.md` (Active Target Management 섹션)

### Subagent Prompt — P3

```
You are implementing **M2 P3** of the topdown-shooter milestone — `.claude/CLAUDE.md` 의 "Active Target Management" 섹션을 M1 완료 상태에 맞춰 갱신.

## Task Description

`.claude/CLAUDE.md` 의 "Active Target Management — CRITICAL" 섹션 (보통 line 60-90 부근) 의 *활성/비활성 데모 목록* 을 현 실제 상태와 일치하도록 갱신.

### 현 실제 상태 (`apps/CMakeLists.txt` line 1-6)

```cmake
add_subdirectory(_MyApp_)        # ← M1 완료 후 활성화 (Director + SceneRenderer + 빌보드)
add_subdirectory(migrate_demo)
add_subdirectory(box2d_demo)
add_subdirectory(effekseer_demo)
add_subdirectory(tweeny_demo)
# add_subdirectory(audio_demo)
```

### CLAUDE.md 의 옛 (정정 대상) 표현

```
- **활성 (`apps/CMakeLists.txt` 주석 해제됨):**
  - `migrate_demo` — 진행 중 마이그레이션 워크스페이스 (working tree 다수 변경)
  - `box2d_demo` — Box2D v2.4.1 물리 데모. 내부 `common` STATIC + `demo1`/`demo2`/`demo3` 서브타겟 3종
  - `effekseer_demo` — Effekseer 1.7.3.0 파티클 데모. 현재 `demo1` 1종
  - `tweeny_demo` — Tweeny 헤더 온리 트위닝 데모. `step(int32_t ms)` vs `step(float ratio)` 오버로드 함정 주의
- **임시 비활성 (주석 처리):**
  - `_MyApp_` — 자유 작업 / `game_deps` 링크 점검용 임시
  - `audio_demo` — FMOD Studio + ImGui 파라미터 데모. ImGui 의존으로 임시 비활성
```

`_MyApp_` 항목을 *활성* 목록으로 옮기고 M1 완료 설명 추가.

### Step 1: `.claude/CLAUDE.md` 의 Active Target Management 섹션 정정

Edit tool 사용 — 정확한 old_string / new_string 매칭. `_MyApp_` 줄을 *활성* 의 맨 위로 이동 + 설명 갱신:

기존 (Edit old_string):
```
- **활성 (`apps/CMakeLists.txt` 주석 해제됨):**
  - `migrate_demo` — 진행 중 마이그레이션 워크스페이스 (working tree 다수 변경)
  - `box2d_demo` — Box2D v2.4.1 물리 데모. 내부 `common` STATIC + `demo1`/`demo2`/`demo3` 서브타겟 3종
  - `effekseer_demo` — Effekseer 1.7.3.0 파티클 데모. 현재 `demo1` 1종
  - `tweeny_demo` — Tweeny 헤더 온리 트위닝 데모. `step(int32_t ms)` vs `step(float ratio)` 오버로드 함정 주의
- **임시 비활성 (주석 처리):**
  - `_MyApp_` — 자유 작업 / `game_deps` 링크 점검용 임시
  - `audio_demo` — FMOD Studio + ImGui 파라미터 데모. ImGui 의존으로 임시 비활성
```

다음으로 교체 (Edit new_string):
```
- **활성 (`apps/CMakeLists.txt` 주석 해제됨):**
  - `_MyApp_` — 탑다운 슈터 게임 (M1 완료 2026-05-24). SJH::sprite atlas + Director + SceneRenderer + Material 패턴 + Player WASD + Camera follow (M2 진행 중). 정통 데모 — 다른 SJH::engine 사용 데모 작성 시 main.cpp 참조 우선순위
  - `migrate_demo` — 진행 중 마이그레이션 워크스페이스 (working tree 다수 변경)
  - `box2d_demo` — Box2D v2.4.1 물리 데모. 내부 `common` STATIC + `demo1`/`demo2`/`demo3` 서브타겟 3종
  - `effekseer_demo` — Effekseer 1.7.3.0 파티클 데모. 현재 `demo1` 1종
  - `tweeny_demo` — Tweeny 헤더 온리 트위닝 데모. `step(int32_t ms)` vs `step(float ratio)` 오버로드 함정 주의
- **임시 비활성 (주석 처리):**
  - `audio_demo` — FMOD Studio + ImGui 파라미터 데모. ImGui 의존으로 임시 비활성
```

### Step 2: Single commit

```bash
git add .claude/CLAUDE.md
git commit -m "docs(CLAUDE): _MyApp_ 활성 데모로 승격 (M1 완료)

- M1 commit chain (a023b27..16f1426) 으로 _MyApp_ 가 정통 데모로 등극
- SJH::sprite + Director + SceneRenderer + Material 패턴 정착
- M2 진행 중 (Player WASD + Camera follow)
- 활성 목록의 최우선 — SJH::engine 사용 데모 작성 시 main.cpp 참조 권장

M2 P3."
```

## Context

**Repository:** `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
**Branch:** `game/module/sprite`

**Reference**:
- `apps/CMakeLists.txt` (line 1-6) — 현 실제 활성 상태
- `.claude/CLAUDE.md` — 정정 대상

**DO NOT touch**: 다른 파일. P1/P2 가 진행 중인 영역과 *겹침 0*.

## Self-Review

- `.claude/CLAUDE.md` 한 파일만 수정?
- _MyApp_ 가 *활성* 목록의 첫 번째 (최근 정통 데모이자 가장 활발)?
- audio_demo 는 *비활성* 그대로 (단독으로)?
- 한국어 톤 유지?
- 단일 commit, 메시지 형식 정확?

## Report

Status: DONE
+ File changed (1 file)
+ Git commit SHA

Work from: `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
```

---

## Dispatch Order

1. **3 subagent 동시 dispatch** — `Agent` tool 의 multi-tool-use 블록 1개에 P1/P2/P3 prompt 모두 포함 (single message, multiple tool calls)
2. 모두 완료되면 *3 commits 순서 무관* (충돌 0)
3. 각 task 별 spec/quality review (3 × 2 = 6 review subagent — review 도 *병렬 가능* 단 implementer 완료 후)

## Review Pipeline (모든 implementer DONE 후)

- **P1 review**: spec (`StateMachineProcessor` template + 단위 테스트 적정성) + quality (heater-only INTERFACE 디자인 + Catch2 컨벤션)
- **P2 review**: spec (PlayerController + Camera follow + main.cpp 셋업 일관성) + quality (Component 베이스 컨벤션 + 디버그 코드 제거 확인)
- **P3 review**: spec (CLAUDE.md 표현 정확성) + quality (markdown 톤)

review 도 P1/P2/P3 동시 6 dispatch 가능 (read-only).

## Self-Review

**Spec coverage:**
- [x] 부록 D M2 (a) Player WASD XZ + Camera follow → P2
- [x] 부록 D M2 (b) `src/fsm/` 신규 + StateMachineProcessor template → P1
- [x] 부록 D M2 (c) `Scene::Root()` + `Component::Owner()` 확인 → spec §B.12 — 둘 다 존재 확인 ✓ (`scene.h:19`, `actor.h:37`)
- [x] 부록 D M2 (d) FSMTickSystem 자유함수 — *본 plan 에서는 P1 안의 `StateMachineProcessor` 가 `Update(float dt)` 를 통해 Tick 위임* 함. 별도 자유함수 불필요 — Component 가 자기 Tick — Scene 의 Actor::Update 재귀 안에 자연 통합
- [x] M2 산출 — `SJH::FSM` 단위 테스트 (transit AND 판정) PASS → P1
- [x] M1 follow-up — CLAUDE.md _MyApp_ 활성 → P3

**Placeholder scan:** 없음.

**Type consistency:**
- `StateMachineProcessor::StateU` = `uint64_t` (P1) — 모든 비트 마스크 일관
- `PlayerController::Action` enum vs `CameraController::Action` enum — 별도 `KeyboardInput<TAction>` 인스턴스 (mPlayerKeyboard vs mKeyboard)
- `SJH::Scene::Actor*` follow target (P2) — non-owning pointer 컨벤션

**겹침 0 확인:** P1/P2/P3 가 *서로 다른 파일 집합* 만 수정 — 위 표 참조.

---

## Execution Handoff

본 plan 의 P1/P2/P3 는 *완전 독립* — `superpowers:dispatching-parallel-agents` 스킬로 *3 subagent 동시 dispatch*. controller 가 single message 에 multiple Agent tool calls 포함.

각 task 의 Subagent Prompt 블록을 *그대로 복사* 하여 prompt 인자로 전달 — self-contained 라 추가 컨텍스트 불요.
