# SJH::timer 모듈 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** C# `TimerComposite`를 포팅한 헤더온리 `SJH::Timer::Timer`(순수 시간 누적기) + `SJH::Timer::MultipleTimer`(Timer 컨테이너를 소유·중앙 Tick하는 Scene::Component)를 core 모듈 `SJH::timer`로 신설한다.

**Architecture:** `timer.h`는 GL/scene 비의존 순수 값 클래스. `multiple_timer.h`는 `SJH::Scene::Component`를 상속해 `map<string, Timer>`를 값으로 소유하고 `Update(dt)`에서 전체 Tick. 모듈은 INTERFACE(헤더온리)이며 `multiple_timer.h`의 scene 의존 때문에 `SJH::scene`을 INTERFACE link. 우산 `SJH::engine`에 합류.

**Tech Stack:** C++17, CMake INTERFACE 라이브러리, `SJH::scene` (`Component`/`Actor`).

**검증 방침:** 프로젝트 컨벤션상 단위 테스트를 자동 추가하지 않는다 (사용자 결정). 검증 = ① 헤더 단독 `-fsyntax-only` 컴파일 통과, ② `_MyApp_` 타겟 빌드 성공(CMake wiring 무결). 각 Task는 명시적 `git add <경로>`로 무관한 working-tree 변경을 휩쓸지 않는다.

**스펙:** `doc/superpowers/specs/2026-06-01-timer-module-design.md`

---

### Task 1: 순수 Timer 헤더 작성

**Files:**
- Create: `src/timer/timer.h`

- [ ] **Step 1: `src/timer/timer.h` 작성**

```cpp
#ifndef __SJH_TIMER_TIMER_H__
#define __SJH_TIMER_TIMER_H__

#include <cassert>

namespace SJH::Timer
{
    /// @brief 게임플레이 시간 누적기 (C# TimerComposite 포팅).
    ///        상태 없는 시간 카운터 — 전이 판단은 호출자(FSM 등) 책임.
    ///
    /// ### FSM 응용 레퍼런스 (코드 아님 — SJH::fsm 사용처가 구현)
    /// C# Affector(Ready→Start→Run⇄Pause→Terminate)는 SJH::fsm::StateMachine 으로 구현한다.
    ///   RunState::OnUpdate(owner, dt):
    ///       timer.Tick(dt);
    ///       if (timer.IsBlocked())         machine.TryTransit(Pause);
    ///       else if (timer.IsTimesUp())    machine.TryTransit(Terminate);
    ///       else if (timer.PollInterval()) owner.OnIntervalFire();  // 연속발사/DoT 틱
    class Timer
    {
      public:
        explicit Timer(float baseTime)
            : baseTime_(baseTime)
        {
            assert(baseTime > 0.0f && "Timer: baseTime must be > 0");
        }

        // === Fluent Builder (생성 직후 체이닝) ===
        Timer& SetAcceleration(float amount)
        {
            acceleration_ = (amount < 0.0f) ? 0.0f : amount; // 음수 → 0 (C# 동일)
            return *this;
        }
        Timer& SetInterval(float interval)
        {
            intervalTime_ = interval;   // <=0 이면 PollInterval 항상 false (비활성)
            nextInterval_ = interval;
            return *this;
        }

        // === 매 프레임 ===
        void Tick(float dt)
        {
            if (blocked_) return;
            passedTime_ += dt * acceleration_;
            if (passedTime_ < 0.0f)            passedTime_ = 0.0f;       // 음수 dt 가드
            else if (passedTime_ > baseTime_)  passedTime_ = baseTime_;  // [0,Base] clamp
        }

        // === 조회 ===
        float GetProgress()   const { return passedTime_ / baseTime_; } // baseTime_>0 보장
        bool  IsTimesUp()     const { return passedTime_ >= baseTime_; }
        float GetPassedTime() const { return passedTime_; }
        float GetBaseTime()   const { return baseTime_; }

        /// @brief non-const — interval 경과 시 true 1회 + nextInterval 누적.
        ///        매 프레임 1회 폴링 가정 (한 Tick에 여러 interval 건너뛰어도 1회만 보고 — C# 동일).
        bool PollInterval()
        {
            if (intervalTime_ <= 0.0f) return false;
            if (passedTime_ >= nextInterval_)
            {
                nextInterval_ += intervalTime_;
                return true;
            }
            return false;
        }

        // === Pause (C# Puase/Continue) ===
        void Pause()  { blocked_ = true; }
        void Resume() { blocked_ = false; }
        bool IsBlocked() const { return blocked_; }

        // === Reset (C# ResetTimer — accel/interval 설정값은 유지) ===
        void Reset()
        {
            passedTime_   = 0.0f;
            blocked_      = false;
            nextInterval_ = intervalTime_;
        }

      private:
        const float baseTime_;            // readonly (C# BaseTime)
        float       passedTime_   = 0.0f;
        float       acceleration_ = 1.0f;
        bool        blocked_      = false;
        float       intervalTime_ = 0.0f; // <=0 이면 interval 비활성
        float       nextInterval_ = 0.0f;
    };
}

#endif // __SJH_TIMER_TIMER_H__
```

- [ ] **Step 2: 헤더 단독 컴파일 검증**

Run: `c++ -std=c++17 -I src -fsyntax-only src/timer/timer.h`
Expected: 출력 없음 (exit 0). 경고/에러 없으면 통과.

- [ ] **Step 3: 커밋**

```bash
git add src/timer/timer.h
git commit -m "feat(timer): SJH::Timer::Timer 순수 시간 누적기 (TimerComposite 포팅)"
```

---

### Task 2: MultipleTimer Component 헤더 작성

**Files:**
- Create: `src/timer/multiple_timer.h`

- [ ] **Step 1: `src/timer/multiple_timer.h` 작성**

```cpp
#ifndef __SJH_TIMER_MULTIPLE_TIMER_H__
#define __SJH_TIMER_MULTIPLE_TIMER_H__

#include "timer/timer.h"
#include "scene/actor.h"   // SJH::Scene::Component

#include <cassert>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

namespace SJH::Timer
{
    /// @brief Actor에 붙는 Component. Timer 컨테이너 소유 + 중앙 Tick 디스패치.
    ///        다른 Component는 Register로 timer를 위탁하고, 반환된 Timer* 핸들로 조회/제어한다.
    /// @note  map<string, Timer> 노드는 포인터 안정적 — 반환된 Timer*는 해당 키 Unregister까지 유효.
    ///        Timer의 const baseTime_ 탓에 *대입*은 불가하나 map 삽입은 *구성*(emplace+move)만 쓰므로 무해.
    class MultipleTimer : public SJH::Scene::Component
    {
      public:
        // === 동적 관리 (Register / Unregister) ===
        /// @brief timer를 컨테이너로 move 이관하고 핸들 반환. 중복 키 → assert (silent overwrite 금지).
        Timer* Register(const std::string& name, Timer timer)
        {
            assert(timers_.find(name) == timers_.end()
                   && "MultipleTimer::Register — duplicate key");
            auto result = timers_.emplace(name, std::move(timer));
            return &result.first->second;
        }
        /// @brief 편의 오버로드 — baseTime만으로 생성·등록.
        Timer* Register(const std::string& name, float baseTime)
        {
            return Register(name, Timer(baseTime));
        }

        void Unregister(const std::string& name)   // 없으면 no-op
        {
            timers_.erase(name);
        }

        Timer* Find(const std::string& name)        // 없으면 nullptr
        {
            auto it = timers_.find(name);
            return (it == timers_.end()) ? nullptr : &it->second;
        }

        bool Has(const std::string& name) const
        {
            return timers_.find(name) != timers_.end();
        }

        void        Clear()       { timers_.clear(); }
        std::size_t Count() const { return timers_.size(); }

        // === Component hook ===
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override
        {
            if (!IsEnabled()) return;
            for (auto& entry : timers_)
                entry.second.Tick(dt);
        }

      private:
        std::unordered_map<std::string, Timer> timers_;
    };
}

#endif // __SJH_TIMER_MULTIPLE_TIMER_H__
```

- [ ] **Step 2: 헤더 단독 컴파일 검증**

Run: `c++ -std=c++17 -I src -I include -fsyntax-only src/timer/multiple_timer.h`
Expected: 출력 없음 (exit 0). (`-I include`는 scene→object/transform.h→vmath.h 해석용.)
주의: 에러가 나면 actor.h가 추가 include path를 요구하는지 확인 — `src/scene/CMakeLists.txt`의 include dir 참조.

- [ ] **Step 3: 커밋**

```bash
git add src/timer/multiple_timer.h
git commit -m "feat(timer): SJH::Timer::MultipleTimer Component (Timer 컨테이너 소유+중앙 Tick)"
```

---

### Task 3: 모듈 CMakeLists 작성

**Files:**
- Create: `src/timer/CMakeLists.txt`

- [ ] **Step 1: `src/timer/CMakeLists.txt` 작성**

`src/fsm/CMakeLists.txt`와 동일 패턴(INTERFACE + ALIAS + scene link).

```cmake
# SJH::timer — 게임플레이 타이머 (header-only, spec doc/superpowers/specs/2026-06-01-timer-module-design.md)
add_library(sjh_timer INTERFACE)
add_library(SJH::timer ALIAS sjh_timer)

target_include_directories(sjh_timer INTERFACE
    ${CMAKE_SOURCE_DIR}/src
)

# multiple_timer.h 가 SJH::Scene::Component (scene/actor.h) 를 헤더에 노출 → scene INTERFACE 의존
target_link_libraries(sjh_timer INTERFACE
    SJH::scene
)
```

- [ ] **Step 2: 커밋**

```bash
git add src/timer/CMakeLists.txt
git commit -m "build(timer): SJH::timer INTERFACE 모듈 CMakeLists"
```

---

### Task 4: 우산 타겟에 SJH::timer 합류

**Files:**
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: `add_subdirectory(timer)` 추가**

`src/CMakeLists.txt`의 `add_subdirectory(playable)` (현재 15번째 줄) 바로 아래에 추가:

```cmake
add_subdirectory(playable)
add_subdirectory(timer)     # <- 추가 (SJH::timer, spec 2026-06-01)
```

- [ ] **Step 2: 우산 link 리스트에 `SJH::timer` 추가**

`target_link_libraries(sjhopengl_engine INTERFACE ...)` 블록의 `SJH::playable` 줄 아래에 추가:

```cmake
    SJH::playable           # M3.5 — 15 모듈 (playable 합류)
    SJH::timer              # 16 모듈 — 게임플레이 타이머 (spec 2026-06-01)
)
```

- [ ] **Step 3: CMake 재구성 + _MyApp_ 빌드 검증**

Run: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_`
Expected: 구성·빌드 성공(exit 0). `SJH::timer` 우산 합류로 link 그래프가 깨지지 않음을 확인.
(헤더온리라 실제 객체 코드는 사용처가 include할 때 생성됨 — 본 단계는 wiring 무결성 검증.)

- [ ] **Step 4: 커밋**

```bash
git add src/CMakeLists.txt
git commit -m "build(engine): 우산 SJH::engine 에 SJH::timer 합류 (16 모듈)"
```

---

### Task 5: 구 stub 제거

**Files:**
- Delete: `src/timer/timer.h`

- [ ] **Step 1: stub 참조 여부 확인**

Run: `grep -rn "TopdownShooter::Timer\|src/Timer/timer.h\|\"Timer/timer.h\"" apps/ src/ || echo "no references"`
Expected: `no references` (구 stub `TopdownShooter::Timer`는 빈 클래스라 사용처 없음 예상).
만약 참조가 나오면 해당 사용처를 `#include "timer/timer.h"` + `SJH::Timer::Timer`로 먼저 전환한 뒤 진행.

- [ ] **Step 2: stub 파일 + 빈 디렉토리 제거**

Run: `git rm -f src/timer/timer.h && rmdir apps/_MyApp_/src/Timer 2>/dev/null || true`
Expected: `timer.h` 삭제. (디렉토리가 비면 제거; 다른 파일이 있으면 rmdir 실패해도 무방.)

주의: working tree의 `apps/_MyApp_/src/Timer/` 는 `git status`상 untracked(`??`)로 표시된 적이 있으니, tracked가 아니면 `git rm` 대신 `rm -f src/timer/timer.h`. 두 경우 모두 파일이 사라지면 성공.

- [ ] **Step 3: 재빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 빌드 성공(exit 0). stub 제거로 깨지는 곳 없음.

- [ ] **Step 4: 커밋**

```bash
git add -A apps/_MyApp_/src/Timer
git commit -m "chore(timer): 구 _MyApp_ Timer stub 제거 (core SJH::timer 승격)"
```

---

## Self-Review

**스펙 커버리지:**
- §3 레이아웃 (timer.h / multiple_timer.h / CMakeLists + scene link) → Task 1·2·3 ✓
- §4 Timer API (생성자/builder/Tick/조회/Pause/Reset/PollInterval) → Task 1 전체 ✓
- §4.5 MultipleTimer API (Register×2/Unregister/Find/Has/Clear/Count/Update) → Task 2 전체 ✓
- §5 의미론 (assert, clamp, PollInterval 누적, Reset) → Task 1 코드에 반영 ✓
- §6 FSM 응용 레퍼런스 → Task 1 헤더 주석 ✓
- §7 검증 (no_auto_tests, build+fsyntax) → 각 Task verification step ✓
- §3 우산 합류 + stub 삭제 → Task 4·5 ✓

**플레이스홀더 스캔:** 없음 (모든 step에 실제 코드/명령/기대 출력 명시).

**타입 일관성:** `Timer` / `MultipleTimer` / `SJH::Timer` 네임스페이스, 메서드명(`Tick`/`PollInterval`/`Register`/`Unregister`/`Find`/`Has`/`Clear`/`Count`)이 스펙과 전 Task에서 동일. `SJH::timer`(CMake ALIAS, 소문자) vs `SJH::Timer`(네임스페이스, 대문자) 구분 일관.
