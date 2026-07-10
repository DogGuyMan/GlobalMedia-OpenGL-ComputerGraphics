# 핸드오프 프롬프트 — `SJH::timer` 마이그레이션 (다른 Claude Agent 용)

> 이 문서 전체를 다음 에이전트에게 그대로 전달하면 된다. `_MyApp_`에 흩어진 ad-hoc 타이밍 코드(직접 `-= dt` / `+= dt` 누산기)를 새 `SJH::timer` 모듈로 마이그레이션하는 작업의 완전한 컨텍스트다.

---

## 0. 당신의 임무 (Mission)

`apps/_MyApp_` 안에서 손으로 굴리던 시간 누산기(countdown/countup float)를 방금 신설된 코어 모듈 **`SJH::timer`** (`Timer` + `MultipleTimer`)로 옮긴다. **단, 코드를 건드리기 전에 §5의 "치명적 함정"과 §6의 "병렬 경합"을 반드시 읽고, §8의 미결정 사항을 사람에게 먼저 확인하라.** 잘못 옮기면 "스폰 즉시 무적" 같은 게임플레이 버그가 조용히 생긴다.

이 프로젝트 컨벤션상 **단위 테스트를 자동으로 추가하지 않는다**(§7). 검증은 빌드 + 실행 관찰이다.

---

## 1. 이미 완성된 것 — `SJH::timer` 모듈 (당신이 만드는 게 아님, 사용 대상)

직전 세션에서 설계→스펙→플랜→구현→2단계 리뷰까지 끝낸 새 코어 모듈이다. 16번째 우산 모듈로 `SJH::engine`에 합류되어 있어, `SJH::engine`을 link하는 모든 데모(=`_MyApp_`)는 추가 CMake 없이 바로 `#include` 가능하다.

- **정본 스펙**: `doc/superpowers/specs/2026-06-01-timer-module-design.md` (로컬 전용, git 미추적)
- **구현 플랜**: `doc/superpowers/plans/2026-06-01-timer-module.md` (로컬 전용)
- **커밋**: `bb18ce6`(Timer) → `b60a17b`(MultipleTimer) → `c8dad08`(모듈 CMake) → `3f56cb2`(우산 합류) → `0fe4ea2`(구 stub 삭제) → `f38e1db`(PollInterval 주석 보강)

### 1.1 파일
| 파일 | 내용 |
|---|---|
| `src/timer/timer.h` | `SJH::Timer::Timer` — 순수 값 클래스 (GL/scene 비의존). 헤더온리 |
| `src/timer/multiple_timer.h` | `SJH::Timer::MultipleTimer : SJH::Scene::Component` — Timer 컨테이너 소유 + 중앙 Tick |
| `src/timer/CMakeLists.txt` | `SJH::timer` INTERFACE + `SJH::scene` link |

> 구 stub `apps/_MyApp_/src/Timer/timer.h` (빈 `TopdownShooter::Timer` + C# 주석)는 **이미 삭제**됐다. `apps/_MyApp_/src/Timer/` 디렉토리도 제거됨. 이제 `#include "timer/timer.h"` / `#include "timer/multiple_timer.h"`로 쓴다.

### 1.2 `SJH::Timer::Timer` API (전체)
```cpp
namespace SJH::Timer {
class Timer {
  explicit Timer(float baseTime);          // assert(baseTime > 0)
  Timer& SetAcceleration(float amount);    // 음수→0. 기본 1.0. 시간배율
  Timer& SetInterval(float interval);      // <=0이면 interval 비활성. 기본 비활성
  void  Tick(float dt);                    // blocked면 무시. passed += dt*accel, [0,base] clamp
  float GetProgress()   const;             // passed/base → [0,1]
  bool  IsTimesUp()     const;             // passed >= base   ← "다 됐는가"
  bool  PollInterval();                    // non-const: interval 경과 시 true 1회 + nextInterval 누적
  float GetPassedTime() const;
  float GetBaseTime()   const;
  void  Pause();  void Resume();  bool IsBlocked() const;
  void  Reset();                           // passed=0, blocked=false, nextInterval=interval (accel/interval 설정 유지)
};
}
```
**핵심 의미**: Timer는 **0에서 base로 올라가는 카운트업**이다. "다 됐다" = `IsTimesUp()`. 재시작 = `Reset()` (단, 매 프레임 `Tick(dt)`을 누군가 불러줘야 진행한다).

**PollInterval 계약(주의)**: 누적 발사는 합쳐지지 않고 프레임마다 이연된다. 한 Tick에 경계를 N개 넘어도 그 N회는 이후 N 프레임에 1회씩 보고. "true 1회 = 1구간 경과"로 가정 금지. 연속발사/DoT 틱용.

### 1.3 `SJH::Timer::MultipleTimer` API (전체)
```cpp
class MultipleTimer : public SJH::Scene::Component {
  Timer* Register(const std::string& name, Timer timer);  // 중복 키 → assert. move 이관 후 핸들 반환
  Timer* Register(const std::string& name, float baseTime);// 편의 오버로드
  void   Unregister(const std::string& name);              // 없으면 no-op
  Timer* Find(const std::string& name);                    // 없으면 nullptr
  bool   Has(const std::string& name) const;
  void   Clear();  std::size_t Count() const;
  void   Update(float dt) override;        // !IsEnabled()면 early-return, 아니면 전체 Tick
};
```
- `map<string,Timer>`에 **값으로 소유**. 노드 포인터 안정 → 반환된 `Timer*`는 그 키 Unregister까지 유효.
- 한 Actor에 `AddComponent<MultipleTimer>()` 하나 붙이고, 같은 Actor의 다른 Component들이 `GetOwner()->GetComponent<MultipleTimer>()->Register("iframe", 0.5f)`로 위탁 → `MultipleTimer::Update`가 매 프레임 전부 Tick. 등록측은 `Find("iframe")->IsTimesUp()`으로 폴링.

### 1.4 사용 패턴 두 가지 (어느 쪽이든 사람 결정 후)
- **(A) 컴포넌트 자가 보유**: 각 Component가 `SJH::Timer::Timer mTimer;` 필드를 직접 들고 자기 `Update`에서 `mTimer.Tick(dt)`. 가장 단순. 단일 타이머 컴포넌트에 적합 (예: `BulletLifetime`).
- **(B) MultipleTimer 중앙 관리**: 타이머가 여럿인 Actor(Player: iframe/dash/dashCooldown)에서 `MultipleTimer` 하나에 등록. 사용자가 MultipleTimer를 요청한 의도가 이 케이스다.

---

## 2. 마이그레이션 대상 인벤토리 (실측)

| # | 위치 | 현재 패턴 | 제안 매핑 | 안정성 |
|---|---|---|---|---|
| 1 | `Entity/Bullet/BulletLifetime.h:18-26` | `mElapsed += dt; if(mElapsed>=mLifetime) SetActive(false)` (카운트업) | `Timer(lifetime)` + `IsTimesUp()`. 패턴 (A) 자가 보유 | **안정** — 단독·자족, 최우선 |
| 2 | `Entity/Components/LifeComponents.h:19-20,50,62,79-82` | `mInvincibleTimer -= dt` (카운트다운), `>0`=무적, `DoDamaged`서 `=mIFrameSeconds` 재장전 | i-frame Timer. **§5.1 역전 함정 필수** | **안정** — Life는 load-bearing, 철거 안 됨 |
| 3 | `LifeComponents.h:27-29,64-69` | `mDeathTimer -= dt`; 0 되면 `SetActive(false)` (사망 연출 지연) | 사망 지연 Timer | 안정 (Life 내부) |
| 4 | `Stage/WaveController.cpp:77` | `mSpawnTimer += dt` 스폰 케이던스 | `Timer` + `SetInterval(period)` + `PollInterval()` | ⚠ **경합** — §6, 지금 다른 워커가 수정 중 |
| 5 | `Entity/Player/PlayerBehavior.{h,cpp}` (`mInvincibilityTimer`/`mDashTimer`/`mDashCooldownTimer`) | 카운트다운 3종 | (이론상) MultipleTimer 등록 | 🚫 **철거 예정** — §6, 건드리지 말 것 |
| 6 | `Stage/Components/MaterialTimeComponent.h:23-25` | `mElapsed += dt` → 셰이더 `uTime` | **마이그레이션 안 함** | 무한 자유 진행 클럭(baseTime 없음). Timer 부적합 — 그대로 둬라 |

---

## 3. 권장 진행 순서 (Phasing)

1. **Phase 1 — `BulletLifetime` (대상 #1).** 가장 단순·자족. 패턴 (A). 여기서 Timer 사용감을 검증하고 빌드·실행 확인.
2. **Phase 2 — `Life`의 i-frame + 사망지연 (대상 #2,#3).** §5.1 역전 함정을 해결한 방식으로. Life는 안정적이고 load-bearing.
3. **Phase 3 — (사람 승인 후) Player 다중 타이머 → `MultipleTimer` (대상 #5 관련).** **단 PlayerBehavior가 철거된 *이후*의 후속 컴포넌트**에 적용. PB 자체는 만지지 않는다 (§6).
4. **Phase 4 — (경합 해소 후) `WaveController` 스폰 케이던스 (대상 #4).** Interval/PollInterval 시연처. 다른 워커의 WaveController 작업이 끝났는지 먼저 확인.

각 Phase마다 빌드+실행 검증 후 **path-scoped 커밋**.

---

## 4. 사용 예시 (복붙용 스케치)

### 4.1 BulletLifetime (패턴 A 자가 보유)
```cpp
#include "timer/timer.h"
class BulletLifetime : public SJH::Scene::Component {
  public:
    explicit BulletLifetime(float lifetime) : mTimer(lifetime) {}
    void Update(float dt) override {
        mTimer.Tick(dt);
        if (mTimer.IsTimesUp() && GetOwner()) GetOwner()->SetActive(false);
    }
  private:
    SJH::Timer::Timer mTimer;   // const baseTime_ 보유 → 대입 불가하나 멤버로는 OK
};
```

### 4.2 i-frame (패턴 A, §5.1 역전 해결 — "armed 게이트")
```cpp
// Timer는 카운트업·생성즉시 not-finished → i-frame은 기본 INACTIVE여야 하므로 별도 게이트 필요.
SJH::Timer::Timer mIFrame{0.5f};   // 임의 base, 실제 base는 재장전 시 의미
bool  mIFrameArmed = false;

bool IsInvincible() const { return mIFrameArmed && !mIFrame.IsTimesUp(); }

void Update(float dt) override {
    if (mIFrameArmed) { mIFrame.Tick(dt); if (mIFrame.IsTimesUp()) mIFrameArmed = false; }
    ...
}
void DoDamaged(int dmg) override {
    if (IsInvincible()) return;
    ...
    mIFrame.Reset(); mIFrameArmed = true;   // arm
}
```
> ⚠ 위는 한 가지 해법일 뿐. §5.1과 §8의 결정에 따라 "armed 게이트" 대신 "생성 시 finished 상태" 같은 다른 방식을 사람이 고를 수 있다.

---

## 5. 치명적 함정 (코드 만지기 전 필독)

### 5.1 카운트업 vs 카운트다운 의미 **역전** — "스폰 즉시 무적" 버그
기존 i-frame/cooldown은 **카운트다운**(`-= dt`, 값`>0`=활성, 0=비활성)이고 **기본 0 = 비활성**이다. 반면 `SJH::Timer`는 **카운트업**이고 **갓 생성한 Timer는 passed=0 → `IsTimesUp()==false`**, 즉 그대로 매핑하면 "**아직 안 끝남=활성=무적**"으로 읽혀 **스폰하자마자 무적**이 된다. 반드시 다음 중 하나로 해결:
- (a) `armed` bool 게이트로 "장전 전엔 무적 아님" 보장 (§4.2 예시),
- (b) 생성 직후 `Tick(base)`로 끝난 상태로 시작,
- (c) Timer에 작은 보강 제안(예: `Finish()` 또는 `Timer(base, startFinished=true)`) — 단 코어 모듈 수정은 사람 승인 필요(§8).
**절대 카운트다운→카운트업을 무지성 1:1 치환하지 말 것.**

### 5.2 컴포넌트 Tick 순서 비결정성 (MultipleTimer 사용 시)
`Actor`의 컴포넌트는 `unordered_map`이라 `Update` 호출 순서가 비결정적이다. `MultipleTimer::Update`가 Tick하기 *전에* 다른 컴포넌트가 같은 프레임에 폴링하면 1프레임 지연이 생길 수 있다. 게임플레이 타이머엔 보통 무해하나, 프레임-정확이 필요하면 패턴 (A)(자가 보유·자가 Tick)가 더 안전하다. 이 트레이드오프를 인지하고 선택하라.

### 5.3 `MaterialTimeComponent`는 마이그레이션 대상 아님
무한 자유 진행 셰이더 클럭이라 baseTime 개념이 없다. `SJH::Timer`로 옮기지 마라.

---

## 6. 병렬 워커 경합 — 지금 이 브랜치는 혼자가 아니다

현재 브랜치 `game/module/ingame/temp`에서 **다른 Claude 에이전트들이 동시에** 작업 중이다 (PlayerBehavior 분해, EnemyBuilder, Effekseer 진단, WaveController). 증거: timer 커밋 위로 무관한 `b9ec4c4`(diagnostics) 등이 계속 쌓이고, working tree의 dirty 파일 목록이 수시로 바뀐다. 따라서:

- 🚫 **`PlayerBehavior.{h,cpp}`는 절대 건드리지 마라.** 메모리 `next_work_playable`에 따르면 PB는 god-component 분해로 **철거 예정**이다(`PB·BulletSpawnPlayable·PlayerMovement 철거`). PB의 dash/iframe 타이머는 분해 후 후속 컴포넌트로 이동하므로, **분해가 끝난 뒤** 그 컴포넌트를 대상으로 삼아라. 시작 전 `apps/_MyApp_/src/Entity/Player/` 상태와 `doc/handoff/`의 최신 분해 핸드오프를 확인.
- ⚠ **`WaveController.{cpp,h}`는 지금 수정 중**(working tree dirty). 손대기 전 `git status`로 누가 잡고 있는지 확인하고, 충돌나면 그 작업 완료 후로 미뤄라.
- 항상 **path-scoped 커밋**(`git add <구체 경로>`만, `git add -A`/`.` 금지). 무관한 dirty 파일을 휩쓸지 마라.
- 작업 전 `git pull --rebase` 또는 최신 HEAD 확인으로 다른 워커 커밋을 반영하라.

---

## 7. 컨벤션 & 검증

- **헤더 가드** `__..._H__` 형식 (`#pragma once` 금지). **주석 한국어**. `long` 금지(고정폭 타입). 경로 슬래시 `/`.
- **단위 테스트 자동 추가 금지** (프로젝트 컨벤션). 검증 = 아래.
- **빌드/실행 검증**:
  ```bash
  cmake --preset ninja                                  # 구성
  cmake --build --preset ninja --target _MyApp_         # 빌드
  cd build_ninja/apps/_MyApp_ && ./_MyApp_              # 실행 (리소스 상대경로 때문에 cd 필수)
  ```
  헤더 단독 점검: `c++ -std=c++17 -I src -I include -fsyntax-only <헤더>` (exit 0).
- **실행 관찰**로 회귀 확인: 총알이 수명대로 사라지는가, 피격 후 i-frame 동안 무적인가, **스폰 직후엔 무적이 아닌가**(§5.1), 스폰 케이던스가 동일한가.

---

## 8. 코드 만지기 전 사람에게 확인할 미결정 사항

1. **i-frame 역전(§5.1) 해법**: (a) armed 게이트 / (b) 생성 시 finished / (c) Timer 코어에 `Finish()`·`startFinished` 보강 중 무엇? (c)는 코어 모듈(`src/timer/`) 수정이라 별도 승인 필요.
2. **사용 패턴**: 어느 사이트를 (A) 자가 보유로, 어느 것을 (B) `MultipleTimer` 중앙 관리로? (사용자가 MultipleTimer를 명시 요청했으니 Player 다중 타이머는 (B) 유력하나, PB 철거 일정과 엮임.)
3. **마이그레이션 범위**: 이번에 어디까지(Phase 1~2만? Player/Wave까지?)? 경합 사이트(#4,#5)는 다른 워커 완료를 기다릴지.
4. **MultipleTimer를 어느 Actor에** 붙일지(PlayerActor? 각 Entity?) — 분해된 컴포넌트 구조 확정 후.

---

## 9. 참고 문서
- `doc/superpowers/specs/2026-06-01-timer-module-design.md` — Timer/MultipleTimer 정본 설계 (결정 7개)
- `doc/superpowers/plans/2026-06-01-timer-module.md` — 구현 플랜(완료)
- `.claude/CLAUDE.md` — 빌드/모듈/컨벤션. (단, 모듈 수가 "15개"로 적힌 곳들은 timer 합류로 이제 16개)
- `src/fsm/` — 헤더온리 INTERFACE 모듈 선례 (timer가 따른 패턴)
- 메모리 `next_work_playable` (PlayerBehavior 분해/철거 배경), `no_auto_tests`
