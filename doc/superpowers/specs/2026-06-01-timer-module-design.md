# SJH::timer 모듈 설계 — 게임플레이 타이머 (TimerComposite 포팅)

- **날짜**: 2026-06-01
- **대상**: `src/timer/` 신규 코어 모듈 (`SJH::timer`)
- **레퍼런스**: `src/timer/timer.h` 주석의 C# `TimerComposite` + `Affector` / `SJH::fsm` (`StateMachine<TState,TOwner>`, `IFsmState<TOwner>`) / `doc/EngineAPI.md`

## 1. 배경 & 목적

C# 원본(`Sophia.Composite.NewTimer.TimerComposite`)은 두 책임이 엮여 있었다:

1. **`TimerComposite`** — 순수 시간 누적기 (BaseTime/PassedTime clamp, Acceleration, Interval 서브타이머, Rewind 조건, Pause).
2. **`Affector`** — 그 타이머를 비트플래그 FSM(Ready→Start→Run⇄Pause→Terminate)으로 구동하는 추상 클래스.

본 프로젝트는 C#의 비트플래그 FSM 패턴(`GetCurrentBit`/`GetTransitionBit`)을 이미 `SJH::fsm`(`GetStateFlag`/`GetTransitFlag`)으로 포팅해 두었다. 따라서 **이번 작업의 범위는 ①(순수 타이머)만** C++로 포팅한다. ②(Affector 라이프사이클)는 사용처에서 기존 `SJH::fsm`으로 구현하며, 그 응용 패턴은 본 문서와 헤더 주석에 *레퍼런스로만* 기록한다.

`src/playable/IntervalPlayable`("N초 뒤 finished_")이 이미 있으나, 이는 연출 시퀀싱(IPlayable 합성)용이며 게임플레이 타이머(진행률/Interval 폴링/가속/일시정지 질의)와 책임이 다르다. 중복이 아니다.

## 2. 확정 결정 (브레인스토밍 4문)

| # | 결정 | 선택 | 근거 |
|---|---|---|---|
| 1 | 범위 | **순수 Timer만** | Affector FSM은 `SJH::fsm` 재사용. YAGNI |
| 2 | 위치/형태 | **core `SJH::timer` 모듈로 승격** | 범용 유틸. 다른 데모도 재사용 가능 |
| 3 | 서브 기능 | **Interval + Acceleration 포팅, Rewind 제외** | Rewind는 C#에서도 미연결 미완성 상태였음. YAGNI |
| 4 | 네이밍 | **`SJH::Timer::Timer`** | 짧고 호출부 명료 (`SJH::Timer::Timer`는 합법 C++) |
| 5 | MultipleTimer 소유 | **MultipleTimer가 Timer 소유** | 중앙 lifetime → dangling 위험 최소화 |
| 6 | MultipleTimer 위치 | **core `SJH::timer`** (`multiple_timer.h`) | Component 래퍼도 범용. `timer.h`는 dep-free 유지 |
| 7 | 컨테이너 키 | **`std::string` 직접** (`map<string,Timer>`) | 가독성·중복 진단. 해시 미사용 (사용자 정정 2026-06-01) |

## 3. 모듈 & 파일 레이아웃

`Timer`(순수 값)는 헤더온리지만, `MultipleTimer`(Component 래퍼)가 `SJH::Scene::Component`를 상속하므로 모듈은 **`SJH::scene`을 INTERFACE link** 한다 (fsm 선례와 동일). 단 `timer.h` 자체는 dep-free로 유지하고, scene 의존은 `multiple_timer.h`에만 격리한다.

```
src/timer/
├── CMakeLists.txt        # add_library(sjh_timer INTERFACE) + ALIAS SJH::timer + link SJH::scene
├── timer.h               # namespace SJH::Timer { class Timer; }  ← dep-free 순수 값
└── multiple_timer.h      # namespace SJH::Timer { class MultipleTimer; } ← Component, scene 의존
```

- `src/CMakeLists.txt` — `add_subdirectory(timer)` 추가 + 우산 `SJH::engine` INTERFACE의 `target_link_libraries`에 `SJH::timer` 합류 → 모든 데모가 한 줄 link로 자동 사용.
- CMake 의존: `target_include_directories(sjh_timer INTERFACE ${CMAKE_SOURCE_DIR}/src)` + `target_link_libraries(sjh_timer INTERFACE SJH::scene)` (multiple_timer.h가 `scene/actor.h` 노출 → PUBLIC/INTERFACE 전파).
- 기존 stub `src/timer/timer.h`는 **삭제** (core로 승격). 사용처는 `#include "timer/timer.h"` / `#include "timer/multiple_timer.h"`로 전환. (`apps/_MyApp_/src/Timer/` 디렉토리는 비게 되면 함께 제거.)

## 4. 클래스 API

```cpp
#ifndef __SJH_TIMER_TIMER_H__
#define __SJH_TIMER_TIMER_H__

#include <cassert>

namespace SJH::Timer
{
    /// @brief 게임플레이 시간 누적기 (C# TimerComposite 포팅).
    ///        상태 없는 시간 카운터 — 전이 판단은 호출자(FSM 등)의 책임.
    class Timer
    {
      public:
        explicit Timer(float baseTime);   // 총 길이. baseTime > 0 (assert)

        // === Fluent Builder (생성 직후 체이닝) ===
        Timer& SetAcceleration(float amount);  // amount<0 → 0 clamp. 기본 1.0
        Timer& SetInterval(float interval);    // interval<=0 → 비활성. 기본 비활성

        // === 매 프레임 ===
        void  Tick(float dt);                  // blocked면 무시. passed += dt*accel, [0,Base] clamp

        // === 조회 ===
        float GetProgress()   const;           // passed / base → [0,1]
        bool  IsTimesUp()     const;           // passed >= base
        bool  PollInterval();                  // ★non-const: interval 경과 시 true 1회 + nextInterval 누적
        float GetPassedTime() const;
        float GetBaseTime()   const;

        // === Pause ===
        void  Pause();                         // C# Puase → blocked=true
        void  Resume();                        // C# Continue → blocked=false
        bool  IsBlocked()     const;

        // === Reset ===
        void  Reset();                         // passed=0, blocked=false, nextInterval 초기화

      private:
        const float baseTime_;
        float       passedTime_   = 0.0f;
        float       acceleration_ = 1.0f;
        bool        blocked_      = false;
        float       intervalTime_ = 0.0f;      // <=0 이면 interval 비활성
        float       nextInterval_ = 0.0f;
    };
}

#endif // __SJH_TIMER_TIMER_H__
```

### C# 대비 매핑 / 변경점
- `FrameTick` → **`Tick`** (엔진 관용), `GetIsTimesUp` → `IsTimesUp`, `GetProgressAmount` → `GetProgress`, `Puase/Continue` → `Pause/Resume`.
- `GetIsActivateInterval()`(부수효과 있던 메서드) → **`PollInterval()`** 개명 — "호출 시 내부 상태가 바뀐다(non-const)"는 의도를 이름에 명시.
- C#의 별도 `IntervalTimerComposite` 클래스 → **인라인 필드**로 흡수 (서브타이머 하나뿐 → 별도 클래스 오버엔지니어링 회피, YAGNI). Reset 시 `nextInterval_` 재초기화 동일.
- `RewaindTimerComposite`(`std::function<bool>` 조건 루프) → **제외** (결정 #3).
- `AccelerationAmount` setter는 `SetAcceleration` 으로 유지. 음수 → 0 clamp(C# 동일).

## 4.5 MultipleTimer API (Component 래퍼)

Client의 임의 Actor에 붙는 Component. 여러 Component가 자신의 Timer를 한 곳(이 Component)에 등록하면, `MultipleTimer::Update`가 매 프레임 전부 Tick한다 (중앙 디스패치).

```cpp
#ifndef __SJH_TIMER_MULTIPLE_TIMER_H__
#define __SJH_TIMER_MULTIPLE_TIMER_H__

#include "timer/timer.h"
#include "scene/actor.h"   // SJH::Scene::Component

#include <string>
#include <unordered_map>

namespace SJH::Timer
{
    /// @brief Actor에 붙는 Component. Timer 컨테이너 소유 + 중앙 Tick 디스패치.
    ///        다른 Component는 Register로 timer를 위탁하고, 반환된 Timer* 핸들로 조회/제어.
    class MultipleTimer : public SJH::Scene::Component
    {
      public:
        // === 동적 관리 (Register / Unregister) ===
        /// @brief timer를 컨테이너에 이관(move)하고 핸들 반환. 중복 키 → assert (silent overwrite 금지).
        Timer* Register(const std::string& name, Timer timer);
        /// @brief 편의 오버로드 — baseTime만으로 생성·등록.
        Timer* Register(const std::string& name, float baseTime);
        void   Unregister(const std::string& name);   // 없으면 no-op
        Timer* Find(const std::string& name);          // 없으면 nullptr
        bool   Has(const std::string& name) const;
        void   Clear();
        std::size_t Count() const { return timers_.size(); }

        // === Component hook ===
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override
        {
            if (!IsEnabled()) return;
            for (auto& [name, t] : timers_)
                t.Tick(dt);
        }

      private:
        std::unordered_map<std::string, Timer> timers_;
    };
}

#endif // __SJH_TIMER_MULTIPLE_TIMER_H__
```

### 설계 노트
- **소유 모델**: `map<string, Timer>`에 **값으로 소유**. `Register`는 인자 Timer를 move하여 노드에 저장. `unordered_map` 노드는 포인터 안정적이므로 반환된 `Timer*`는 해당 키가 Unregister될 때까지 유효.
- **`Timer`의 `const baseTime_`**: 복사대입/이동대입은 불가하나, map 삽입은 *구성*(emplace/insert + move 생성)만 사용하므로 문제 없음. 재대입이 필요한 시나리오(같은 키 baseTime 변경)는 Unregister 후 재Register로 처리.
- **중복 키**: `Register` 중복 → `assert` (Actor::AddComponent의 explicit-side-effects 컨벤션과 동일). 의도적 교체는 Unregister 선행.
- **반복 안전성**: `Timer::Tick`은 콜백이 없어 tick 루프 도중 컨테이너가 변형되지 않는다. Register/Unregister는 다른 Component가 *자기* Update에서 호출 → MultipleTimer 루프 밖. 따라서 deferred-removal 불필요.
- **사용 패턴**: 등록측 Component는 OnEnter에서 `owner->GetComponent<MultipleTimer>()->Register("iframe", 0.5f)`로 핸들 확보, 이후 `timer->IsTimesUp()` / `PollInterval()` 폴링. (단일 호출 contract상 MultipleTimer가 먼저 AddComponent 되어 있어야 함 — 사용처 책임.)

## 5. 동작 의미론 (엣지 케이스)

- **생성자**: `assert(baseTime > 0)`. `GetProgress`의 0 나눗셈 방지. `baseTime_`은 `const`.
- **Tick(dt)**: `blocked_`면 즉시 return. 아니면 `passedTime_ += dt * acceleration_` 후 `[0, baseTime_]` clamp. 음수 dt도 clamp로 안전(passed가 0 미만으로 내려가지 않음).
- **PollInterval()**: `intervalTime_ <= 0`(비활성)이면 항상 false. 활성 시 `passedTime_ >= nextInterval_`이면 `nextInterval_ += intervalTime_` 후 true. **한 Tick에 여러 interval을 건너뛰어도 호출당 1회만 보고**(C# 원본 동작) — 매 프레임 1회 폴링 가정.
- **SetInterval(<=0)**: interval 비활성화. `nextInterval_`은 `intervalTime_`로 재설정.
- **GetProgress()**: `passedTime_ / baseTime_`. clamp 덕에 자연히 [0,1].
- **Reset()**: `passedTime_=0`, `blocked_=false`, `nextInterval_=intervalTime_`. acceleration/interval 설정값은 유지(C# `ResetTimer` 동일).

## 6. FSM 응용 레퍼런스 (코드 아님 — 헤더 주석 + 본 문서 기록)

Timer 자체는 FSM을 모른다. C# Affector(Ready→Start→Run⇄Pause→Terminate) 패턴은 **사용처**에서 기존 `SJH::fsm::StateMachine<TState, TOwner>` + `IFsmState<TOwner>`로 구현한다. RunState의 의사코드:

```
RunState::OnUpdate(owner, dt):
    timer.Tick(dt);
    if (timer.IsBlocked())          → machine.TryTransit(TimerState::Pause)
    else if (timer.IsTimesUp())     → machine.TryTransit(TimerState::Terminate)
    else if (timer.PollInterval())  → owner.OnIntervalFire();   // 연속발사/DoT 틱
```

→ **Timer = 상태 없는 시간 누적기**, **FSM = 그 시간을 읽어 전이 결정**. 책임 분리 명확. (이 매핑이 C#의 `AffectorRunState.Affect`와 1:1 대응.)

## 7. 검증 (no_auto_tests 준수)

- 단위 테스트 **자동 추가 안 함** (사용자 컨벤션). 검증 = `cmake --build --preset ninja --target _MyApp_` 빌드 성공 + `SJH::engine` 우산 link로 `#include "timer/timer.h"` 해석 확인.
- `_MyApp_`의 실제 사용처(공격 윈도 0.15s, i-frame 등) 적용은 **본 범위 밖** — 후속 작업으로 남김.

## 8. 작업 산출물 요약

| 파일 | 동작 |
|---|---|
| `src/timer/timer.h` | 신규 — `SJH::Timer::Timer` 헤더온리 정의 + FSM 응용 주석 (dep-free) |
| `src/timer/multiple_timer.h` | 신규 — `SJH::Timer::MultipleTimer` Component (`map<string,Timer>` 소유 + 중앙 Tick) |
| `src/timer/CMakeLists.txt` | 신규 — `SJH::timer` INTERFACE ALIAS + `link SJH::scene` |
| `src/CMakeLists.txt` | 수정 — `add_subdirectory(timer)` + 우산 `SJH::engine` link 합류 |
| `src/timer/timer.h` | 삭제 — core 승격 (디렉토리도 비면 제거) |
