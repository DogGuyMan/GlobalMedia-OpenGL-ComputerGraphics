> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

<!-- # M2 P1 Agent Prompt — SJH::fsm 코어 모듈 + 단위 테스트

> **사용법**: 이 파일의 `## Prompt` 섹션 아래 *전체 내용* 을 복사하여 새 Claude Code 세션 (또는 Agent tool 의 `prompt` 인자) 에 그대로 붙여넣으세요. self-contained 라 다른 컨텍스트 불요.
>
> **작업 디렉토리**: `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
> **브랜치**: `game/module/sprite`
> **예상 시간**: 15~25분 (헤더 작성 + 단위 테스트 5개 + 빌드 검증 + commit)

--- -->

## Prompt

You are implementing **M2 P1** of the topdown-shooter milestone — `SJH::fsm` 코어 모듈 신설 + 단위 테스트.

신규 `SJH::fsm` 코어 STATIC 라이브러리 (헤더-only INTERFACE) 추가. `StateMachineProcessor<TState, TTransit>` template — uint64_t 비트 enum 기반 FSM. Catch2 단위 테스트로 Bind / TryTransit (AND 판정) / OnEnter+OnExit 발화 순서 검증.

### 사전 정독 (코드 작성 전)

1. **`src/scene/actor.h:27-47`** — `SJH::Scene::Component` 베이스의 *순수 가상* 메서드 (OnEnter / OnExit / Update) 확인.
2. **`src/sprite/CMakeLists.txt`** — SJH 모듈의 STATIC + ALIAS 패턴 (참고만, fsm 은 INTERFACE).
3. **`<test>/test_uniform_atlas.cpp`** + **`test/CMakeLists.txt:222-243` 부근** — Catch2 v3 GL-free 단위 테스트 + 새 test executable 등록 패턴.

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

        /// @brief 매 프레임 OnUpdate 호출 — Scene::Actor::Update 재귀가 자동.
        void Tick(float dtSeconds) { OnUpdate(current_, dtSeconds); }

        TState State() const { return current_; }

        // === Component 베이스 (SJH::Scene::Component) 의 순수 가상 구현 ===
        void OnEnter() override {}                  // Actor 진입 시 1회
        void OnExit()  override {}                  // Actor 이탈 시 1회
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

헤더-only INTERFACE library:

```cmake
# SJH::fsm — 비트 enum 기반 FSM template (spec §6.0)
add_library(sjh_fsm INTERFACE)
add_library(SJH::fsm ALIAS sjh_fsm)

target_include_directories(sjh_fsm INTERFACE
    ${CMAKE_SOURCE_DIR}/src
)

# 헤더에 SJH::Scene::Component 노출 → INTERFACE dependency
target_link_libraries(sjh_fsm INTERFACE
    SJH::scene
)
```

> 본 저장소 *최초의 INTERFACE-only SJH 모듈*. 만약 빌드 중 INTERFACE 가 예상 외 동작 시 STATIC 으로 fallback 가능 (`.cpp` 1개 추가 — 빈 namespace 또는 explicit instantiation). 우선 INTERFACE 로 시도.

### Step 3: `src/CMakeLists.txt` 수정

기존 (Task 1 M1 의 `add_subdirectory(sprite)` 와 동일 패턴):
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
#include <vector>

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
        ToIdle   = (uint64_t)TestState::Move | (uint64_t)TestState::Attack,
        ToMove   = (uint64_t)TestState::Idle,
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
        REQUIRE(fsm.State() == TestState::Idle);
    }
}

TEST_CASE("StateMachineProcessor — OnEnter / OnExit 발화 순서", "[fsm]")
{
    SpyFSM fsm;

    REQUIRE(fsm.TryTransit(TestTransit::ToMove));
    REQUIRE(fsm.ExitLog.size() == 1);
    REQUIRE(fsm.ExitLog[0] == TestState::Idle);
    REQUIRE(fsm.EnterLog.size() == 1);
    REQUIRE(fsm.EnterLog[0] == TestState::Move);

    REQUIRE(fsm.TryTransit(TestTransit::ToAttack));
    REQUIRE(fsm.ExitLog.size() == 2);
    REQUIRE(fsm.ExitLog[1] == TestState::Move);
    REQUIRE(fsm.EnterLog.size() == 2);
    REQUIRE(fsm.EnterLog[1] == TestState::Attack);

    REQUIRE_FALSE(fsm.TryTransit(TestTransit::ToMove));   // Attack → Move 불가
    REQUIRE(fsm.ExitLog.size() == 2);
    REQUIRE(fsm.EnterLog.size() == 2);
}

TEST_CASE("StateMachineProcessor — NONE 상태에서 transit 실패", "[fsm]")
{
    SJH::FSM::StateMachineProcessor<TestState, TestTransit> fsm;   // default = NONE
    fsm.Bind(TestTransit::ToMove, TestState::Move);
    REQUIRE(fsm.State() == TestState::NONE);
    REQUIRE_FALSE(fsm.TryTransit(TestTransit::ToMove));
}

TEST_CASE("StateMachineProcessor — target 미등록 transit 실패", "[fsm]")
{
    SJH::FSM::StateMachineProcessor<TestState, TestTransit> fsm(TestState::Idle);
    REQUIRE_FALSE(fsm.TryTransit(TestTransit::ToMove));
    REQUIRE(fsm.State() == TestState::Idle);
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

기대: 5 TEST_CASE 모두 PASS. tweeny_demo / _MyApp_ 우산 데모 빌드 영향 없음.

회귀 가드:
```bash
cmake --build --preset ninja --target _MyApp_
cmake --build --preset ninja --target tweeny_demo
ctest --test-dir build_ninja -R "ComputeUVRect" -V   # M1 회귀 가드
```

### Step 7: Single commit

```bash
git add src/fsm/CMakeLists.txt <src>/fsm/state_machine_processor.h \
        src/CMakeLists.txt test/test_fsm.cpp test/CMakeLists.txt
git commit -m "feat(fsm): SJH::fsm 코어 모듈 신설 + StateMachineProcessor template

- src/fsm/ 신설 — INTERFACE library (header-only)
- StateMachineProcessor<TState, TTransit> template — spec §6.0.2 비트 enum 기반
- Bind(transit, target) + TryTransit(allowed-from AND 판정) + OnEnter/OnUpdate/OnExit 가상함수
- Scene::Component 베이스 Update 가 Tick 위임 → Actor::Update 재귀가 자동 발화
- SJH::engine 우산 (코어 13 → 14 모듈)
- test_fsm — Catch2 v3, 5 TEST_CASEs (init / AND 판정 / Enter+Exit 순서 / NONE 가드 / target 미등록 가드)

M2 P1."
```

## Project Conventions (필독)

- 한국어 주석 OK
- Header guard `__SJH_FSM_STATE_MACHINE_PROCESSOR_H__` (NOT `#pragma once`)
- Catch2 v3 (`<<catch2>/catch_test_macros.hpp>`)
- Korean commit messages OK
- `.claude/CLAUDE.md` 가 cwd 기반 자동 로드됨 — 본 저장소 빌드 명령 / 컨벤션 참조

## DO NOT touch

- `apps/_MyApp_/*` — 별도 subagent (P2) 담당
- `.claude/CLAUDE.md` — 별도 subagent (P3) 담당
- `src/sprite/*` / `src/scene/*` / 기타 코어 모듈

## Self-Review

- 5 TEST_CASE 모두 PASS?
- 헤더 guard `__SJH_FSM_STATE_MACHINE_PROCESSOR_H__` (NOT `#pragma once`)?
- INTERFACE library (`.cpp` 없음, INTERFACE include propagation)?
- 우산 link `SJH::fsm` 추가?
- 단일 commit, 메시지 형식 정확?

## Report

Status: DONE / DONE_WITH_CONCERNS / BLOCKED / NEEDS_CONTEXT
+ Files changed (5 files)
+ Test output (5 cases PASS, 회귀 가드 PASS)
+ Git commit SHA
+ Self-review findings

Work from: `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
