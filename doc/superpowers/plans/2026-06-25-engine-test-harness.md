# 엔진 테스트 하네스 (Phase 1 Track A + crossver) Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 현 HEAD(`7949a65`)를 oracle 로, GL 없는 순수 로직 5모듈(common·timer·object·sprite·fsm)에 Catch2 v3 characterization 단위 테스트 + cross-version differential 하네스를 세워 리팩토링 회귀 안전망을 만든다.

**Architecture:** 폐기된 `094362d` 구조 계승 — `test_smoke/`(배선 first-green) + `test/`(모듈별 `test_<module>` exe, 각 `catch_discover_tests`) + `<scripts>/crossver_verify.sh`(git worktree 로 두 커밋 동일 테스트 실행·diff). 루트 `CMakeLists.txt` 의 `ENABLE_TESTING`+`EXISTS` 가드가 이미 배선돼 있어 디렉토리 생성만으로 작동. 테스트는 **공개 헤더(계약)에만** 바인딩(contract-anchoring).

**Tech Stack:** C++17 · Catch2 v3.13.0 (vcpkg `Catch2::Catch2WithMain`) · CMake/Ninja · glm · git worktree · bash.

> **정본 spec:** `doc/superpowers/specs/2026-06-25-engine-test-harness-design.md` (D1~D10). **이 plan + spec 둘 다 `doc/` = .gitignore 로컬** (커밋 안 됨, 프로젝트 정책).

---

## ⚠ 이 plan 의 테스트 사이클 = characterization (red-first TDD 아님)

기존 코드(Timer/Transform/…)는 **이미 존재**한다. 우리는 그 *현재 동작*을 잠근다(D4 oracle). 그래서 사이클은:

**테스트 작성(D8 기대값) → 빌드 → 실행(현 동작이면 PASS) → 커밋**

- 실행이 **PASS** = 현 동작을 정확히 포착(정상).
- 실행이 **FAIL** = 둘 중 하나. (i) 내 기대값 도출 오류 → 수정. (ii) 현 코드가 내 손계산 계약과 불일치 = **현 코드 버그 후보** → 사람 판단(버그 기록 vs 내 계산 오류). **절대 통과시키려 기대값을 적당히 맞추지 말 것.**
- 메모리 `no_auto_tests`("TDD red-green 강제 안 함")와 정합 — red-first 강제 아님.

**D8 기대값 출처:** (a) 손계산 가능한 순수 수학 = 명세에서 도출한 값 assert / (b) 불투명 회귀값 = 구조적 property assert. FP 비교: `WithinRel(eps)` 또는 `WithinAbs(eps)`(정확 이진분수는 `Approx`).

---

## ★ HANDOFF-UNIT 지도 (메모리 `plan-handoff-boundary-markup`)

| Task | unit | green | commit | ctx |
|---|---|---|---|---|
| Task 0 | U0 배선 first-green | smoke ctest GREEN | `test_smoke/` | 小 |
| Task 1 | U1 test_timer | GREEN | `test/test_timer.cpp test/CMakeLists.txt` | 中 |
| Task 2 | U2 test_transform + test_light | GREEN | `test/test_transform.cpp test/test_light.cpp test/CMakeLists.txt` | 中 |
| — | **☆ 보조 절단** (컨텍스트 빠듯하면 Task 2 끝에서) | | | |
| Task 3 | U3 test_sprite_uvrect | GREEN | `test/test_sprite_uvrect.cpp test/CMakeLists.txt` | 小中 |
| Task 4 | U4 test_fsm + tests 우산 | GREEN | `test/test_fsm.cpp test/CMakeLists.txt` | 中 |
| Task 5 | U5 crossver capstone | 두 커밋 등가 | `<scripts>/crossver_verify.sh` | 中 |
| Task 6 | **★ HANDOFF #1** (Phase 1 종료) | — | (handoff doc) | 小 |

**커밋 규율(메모리 `user-parallel-git`):** 반드시 **path-scoped** `git commit <경로>` (사용자 병렬 staging 보호). `git add -A` 금지. **빌드/실행은 사용자가 직접** 하는 게 관례지만, 이 plan 의 verify step 은 명령을 명시하니 실행자(에이전트/세션)가 돌려 GREEN 확인 후 커밋. Co-Authored-By 트레일러 미사용(프로젝트 관례).

---

## File Structure

| 파일 | 책임 | 생성/수정 |
|---|---|---|
| `test_smoke/CMakeLists.txt` | smoke exe 등록 | 생성 |
| `test/smoke/smoke.cpp` | 배선 카나리아 1~3 case (SJH::common) | 생성 |
| `test/CMakeLists.txt` | 모듈별 exe + `catch_discover_tests` + `tests` 우산 | 생성(Task1) → 증분 수정(Task2~4) |
| `test/test_timer.cpp` | `SJH::Timer::Timer` characterization | 생성 |
| `test/test_transform.cpp` | `SJH::Transform` characterization (vmath→glm 수리) | 생성 |
| `test/test_light.cpp` | `SJH::Light` POD + `GetAttenuationCoeff` (object 한정) | 생성 |
| `test/test_sprite_uvrect.cpp` | `SJH::Sprite::ComputeUVRect` (094362d 거의 verbatim) | 생성 |
| `test/test_fsm.cpp` | `SJH::FSM::StateMachine` mock 전이 | 생성 |
| `<scripts>/crossver_verify.sh` | 두 커밋 동일 테스트 실행·diff | 생성 |

루트 `CMakeLists.txt` **무수정** (ENABLE_TESTING/EXISTS 가드 기존).

검증 공통 명령:
```bash
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target <타겟>
ctest --test-dir build_ninja -R <정규식> --output-on-failure
```

---

## Task 0 (U0): 배선 first-green — `test_smoke`

**Files:**
- Create: `test/smoke/smoke.cpp`
- Create: `test_smoke/CMakeLists.txt`

- [ ] **Step 1: smoke.cpp 작성** — `SJH::common` 의 header-only 순수 함수만 (배선 증명 목적; Deg2Rad=M_PI 기반, FixedTime=1/60).

`test/smoke/smoke.cpp`:
```cpp
// smoke — 테스트 배선(Catch2+vcpkg+CTest+catch_discover_tests) 카나리아.
// 로직 검증이 아니라 "테스트 환경 자체가 작동한다"를 증명한다. (spec D5)
#include <<catch2>/catch_test_macros.hpp>
#include <<catch2>/matchers/catch_matchers_floating_point.hpp>

#include "common/common.h" // SJH::Deg2Rad / Rad2Deg / FixedTime

using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("smoke: 테스트 배선 작동", "[smoke]")
{
    // Deg2Rad(180) = 180 * M_PI / 180 = M_PI
    REQUIRE_THAT(SJH::Deg2Rad(180.0f), WithinRel(3.14159274f, 1e-5f));
    // Rad2Deg 는 Deg2Rad 의 역
    REQUIRE_THAT(SJH::Rad2Deg(SJH::Deg2Rad(90.0f)), WithinRel(90.0f, 1e-5f));
    // FixedTime = 1/60
    REQUIRE_THAT(SJH::FixedTime(), WithinAbs(1.0f / 60.0f, 1e-7f));
}
```

- [ ] **Step 2: test_smoke/CMakeLists.txt 작성**

`test_smoke/CMakeLists.txt`:
```cmake
# 배선 first-green. 루트 CMakeLists 의 ENABLE_TESTING 블록이 find_package(Catch2 3)+
# enable_testing() 후 EXISTS 가드로 이 디렉토리를 add_subdirectory 한다.
add_executable(smoke smoke.cpp)
target_link_libraries(smoke PRIVATE Catch2::Catch2WithMain SJH::common)
target_compile_features(smoke PRIVATE cxx_std_17)

include(Catch) # vcpkg Catch2Config 가 Catch.cmake 를 CMAKE_MODULE_PATH 에 등록
catch_discover_tests(smoke)
```

- [ ] **Step 3: 구성 + 빌드**

Run:
```bash
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target smoke
```
Expected: 구성에서 `find_package(Catch2 3)` 성공, smoke 링크 성공. 에러 0.

- [ ] **Step 4: 실행 — GREEN 확인**

Run: `ctest --test-dir build_ninja -R "smoke" --output-on-failure`
Expected: `100% tests passed, 0 tests failed` (smoke 케이스 1개 PASS). 이게 GREEN 이면 배선 전체(Catch2 link·discover·ctest) 증명 완료.
- 만약 `include(Catch)` 실패 → 루트가 `find_package(Catch2 3)` 를 먼저 했는지 확인(ENABLE_TESTING=ON 필요).

- [ ] **Step 5: 커밋**

```bash
git add test/smoke/smoke.cpp test_smoke/CMakeLists.txt
git commit test/smoke/smoke.cpp test_smoke/CMakeLists.txt -m "[test] smoke 배선 first-green (Catch2+CTest 경로 증명)"
```

---

## Task 1 (U1): `test_timer` — `SJH::Timer::Timer` characterization

**Files:**
- Create: `test/test_timer.cpp`
- Create: `test/CMakeLists.txt` (이 Task 에서 신규 — Task2~4 가 증분 추가)

검증 대상 동작(VERBATIM 공식 from `src/timer/timer.h`): ctor `Timer(float baseTime)`(>0); `Tick`은 `mPassedTime += dt*mAcceleration` 후 `[0, BASE_TIME]` clamp; `GetProgress = mPassedTime/BASE_TIME`; `IsTimesUp = mPassedTime>=BASE_TIME`; `SetAcceleration(neg)→0`; `Pause/Resume`(blocked 시 Tick no-op); `SetInterval(x)` → mIntervalTime=mIextInterval=x; `PollInterval`은 `mPassedTime>=mIextInterval` 면 true + threshold += interval(catch-up 없음, 호출당 1회); `Reset` → passed=0, blocked=false, iext=mIntervalTime.

- [ ] **Step 1: test_timer.cpp 작성** (값은 0.5/1.0/2.0 등 이진분수만 사용 → 정확)

`test/test_timer.cpp`:
```cpp
// SJH::Timer::Timer characterization — 현 HEAD 동작 잠금 (spec D3/D8a).
#include <<catch2>/catch_test_macros.hpp>
#include <<catch2>/matchers/catch_matchers_floating_point.hpp>

#include "timer/timer.h"

using Catch::Matchers::WithinAbs;
using SJH::Timer::Timer;

TEST_CASE("Timer: 기본 누적과 진행도", "[timer]")
{
    Timer t(2.0f);
    REQUIRE_THAT(t.GetBaseTime(), WithinAbs(2.0f, 1e-7f));
    REQUIRE_THAT(t.GetProgress(), WithinAbs(0.0f, 1e-7f));
    REQUIRE_FALSE(t.IsTimesUp());

    t.Tick(0.5f); // accel 기본 1.0 → mPassedTime = 0.5
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(0.5f, 1e-7f));
    REQUIRE_THAT(t.GetProgress(), WithinAbs(0.25f, 1e-7f)); // 0.5/2.0
    REQUIRE_FALSE(t.IsTimesUp());
}

TEST_CASE("Timer: 가속", "[timer]")
{
    Timer t(2.0f);
    t.SetAcceleration(2.0f);
    t.Tick(0.5f); // 0.5 * 2 = 1.0
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(1.0f, 1e-7f));
    REQUIRE_THAT(t.GetProgress(), WithinAbs(0.5f, 1e-7f));
}

TEST_CASE("Timer: 음수 가속은 0 으로 클램프", "[timer]")
{
    Timer t(2.0f);
    t.SetAcceleration(-3.0f); // → 0
    t.Tick(1.0f);             // 1.0 * 0 = 0
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(0.0f, 1e-7f));
}

TEST_CASE("Timer: BASE_TIME 상한 클램프 + IsTimesUp", "[timer]")
{
    Timer t(2.0f);
    t.Tick(10.0f); // 10 > 2 → clamp 2.0
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(2.0f, 1e-7f));
    REQUIRE_THAT(t.GetProgress(), WithinAbs(1.0f, 1e-7f));
    REQUIRE(t.IsTimesUp());
}

TEST_CASE("Timer: 음수 dt 하한 가드", "[timer]")
{
    Timer t(2.0f);
    t.Tick(-5.0f); // mPassedTime = -5 → 0 으로 가드
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(0.0f, 1e-7f));
}

TEST_CASE("Timer: Pause/Resume", "[timer]")
{
    Timer t(2.0f);
    t.Pause();
    REQUIRE(t.IsBlocked());
    t.Tick(1.0f); // blocked → no-op
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(0.0f, 1e-7f));
    t.Resume();
    REQUIRE_FALSE(t.IsBlocked());
    t.Tick(1.0f);
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(1.0f, 1e-7f));
}

TEST_CASE("Timer: PollInterval 은 호출당 1회 true, 임계 전진", "[timer]")
{
    Timer t(10.0f);
    t.SetInterval(0.5f); // 0.5/1.0/1.5 모두 정확 이진분수

    t.Tick(0.5f);                  // passed = 0.5
    REQUIRE(t.PollInterval());     // 0.5 >= 0.5 → true, iext → 1.0
    REQUIRE_FALSE(t.PollInterval()); // 0.5 >= 1.0 → false (catch-up 없음)

    t.Tick(0.5f);                  // passed = 1.0
    REQUIRE(t.PollInterval());     // 1.0 >= 1.0 → true, iext → 1.5
    REQUIRE_FALSE(t.PollInterval());
}

TEST_CASE("Timer: 인터벌 미설정이면 PollInterval 항상 false", "[timer]")
{
    Timer t(10.0f);
    t.Tick(5.0f);
    REQUIRE_FALSE(t.PollInterval()); // mIntervalTime <= 0
}

TEST_CASE("Timer: Reset", "[timer]")
{
    Timer t(10.0f);
    t.SetInterval(0.5f);
    t.Tick(0.5f);
    t.Tick(0.5f); // passed = 1.0
    t.Reset();
    REQUIRE_THAT(t.GetPassedTime(), WithinAbs(0.0f, 1e-7f));
    REQUIRE_FALSE(t.IsBlocked());
    // Reset 후 인터벌 임계가 초기값으로 복원 → 다시 0.5 에서 첫 true
    t.Tick(0.5f);
    REQUIRE(t.PollInterval());
}
```

- [ ] **Step 2: test/CMakeLists.txt 작성 (신규 — timer 1개 + tests 우산 시드)**

`test/CMakeLists.txt`:
```cmake
# 모듈별 characterization 단위 테스트. 각 exe 는 공개 헤더(계약)에만 바인딩(contract-anchoring).
# 루트 CMakeLists 의 ENABLE_TESTING 블록이 find_package(Catch2 3)+enable_testing() 후
# EXISTS 가드로 이 디렉토리를 add_subdirectory 한다.
include(Catch) # vcpkg Catch2Config → catch_discover_tests 제공

# --- 헬퍼: test_<name>.cpp + 모듈 link + discover 를 한 줄로 ---
function(sjh_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain ${ARGN})
    target_compile_features(${name} PRIVATE cxx_std_17)
    catch_discover_tests(${name})
endfunction()

sjh_add_test(test_timer SJH::timer)

# 우산 타겟 — `cmake --build --preset ninja --target tests` 로 전체 일괄 빌드.
# (Task 2~4 에서 DEPENDS 에 신규 타겟 추가)
add_custom_target(tests DEPENDS test_timer)
```

- [ ] **Step 3: 빌드**

Run: `cmake --preset ninja -DENABLE_TESTING=ON && cmake --build --preset ninja --target test_timer`
Expected: 링크 성공(에러 0). `SJH::timer`(INTERFACE→SJH::scene STATIC) 전이 의존 자동 해소.

- [ ] **Step 4: 실행 — GREEN 확인**

Run: `ctest --test-dir build_ninja -R "Timer" --output-on-failure`
Expected: 모든 Timer 케이스 PASS. FAIL 시: 기대값 도출 vs 현 코드 불일치 — §"characterization 사이클" 규칙대로 판단(통과시키려 값 맞추지 말 것).

- [ ] **Step 5: 커밋**

```bash
git add test/test_timer.cpp test/CMakeLists.txt
git commit test/test_timer.cpp test/CMakeLists.txt -m "[test] test_timer characterization (Timer 누적/클램프/PollInterval/Reset)"
```

---

## Task 2 (U2): `test_transform` + `test_light` — `SJH::object` (094362d 복원+수리)

**Files:**
- Create: `test/test_transform.cpp` (094362d 복원 — vmath→glm 수리)
- Create: `test/test_light.cpp` (094362d 복원 — object/light.h 한정, vmath→glm)
- Modify: `test/CMakeLists.txt` (2개 타겟 + 우산 DEPENDS 추가)

**핵심 drift(spec §4 복원 절차):** 옛 테스트는 `vmath` 사용. HEAD 는 glm. `Transform::GetRotationMatrix` = `Rz*Ry*Rx` 각 `glm::radians(EulerRot[i])`(EulerRot 은 **degree**). `GetForward()` = **-col2**. `Light` POD + `GetAttenuationCoeff` 는 무변경.

- [ ] **Step 1: test_transform.cpp 작성** (기대 행렬은 impl 합성을 glm 으로 미러)

`test/test_transform.cpp`:
```cpp
// SJH::Transform characterization — vmath→glm 수리본 (094362d 복원). spec D8a.
// 합성: GetLocalMatrix = T * R * S, R = Rz*Ry*Rx (각 glm::radians(EulerRot[i]), EulerRot=degree).
#include <<catch2>/catch_test_macros.hpp>
#include <<catch2>/matchers/catch_matchers_floating_point.hpp>

#include "object/transform.h"
#include <<glm>/gtc/matrix_transform.hpp>

using Catch::Matchers::WithinAbs;

// glm mat4 근사 비교 (column-major, [col][row])
static void RequireMatNear(const glm::mat4 &a, const glm::mat4 &b, float eps = 1e-5f)
{
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            REQUIRE_THAT(a[c][r], WithinAbs(b[c][r], eps));
}

static void RequireVec3Near(const glm::vec3 &a, const glm::vec3 &b, float eps = 1e-5f)
{
    REQUIRE_THAT(a.x, WithinAbs(b.x, eps));
    REQUIRE_THAT(a.y, WithinAbs(b.y, eps));
    REQUIRE_THAT(a.z, WithinAbs(b.z, eps));
}

TEST_CASE("Transform: 기본값은 단위행렬", "[transform]")
{
    SJH::Transform t;
    RequireMatNear(t.GetLocalMatrix(), glm::mat4(1.0f));
}

TEST_CASE("Transform: 이동만", "[transform]")
{
    SJH::Transform t;
    t.Translate = glm::vec3(1.0f, 2.0f, 3.0f);
    RequireMatNear(t.GetLocalMatrix(),
                   glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f)));
}

TEST_CASE("Transform: 스케일만", "[transform]")
{
    SJH::Transform t;
    t.Scale = glm::vec3(2.0f, 3.0f, 4.0f);
    RequireMatNear(t.GetLocalMatrix(),
                   glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 3.0f, 4.0f)));
}

TEST_CASE("Transform: 기본 방향 벡터", "[transform]")
{
    SJH::Transform t;
    RequireVec3Near(t.GetRight(), glm::vec3(1.0f, 0.0f, 0.0f));
    RequireVec3Near(t.GetUp(), glm::vec3(0.0f, 1.0f, 0.0f));
    RequireVec3Near(t.GetForward(), glm::vec3(0.0f, 0.0f, -1.0f)); // -col2
}

TEST_CASE("Transform: Y축 90도 회전 방향", "[transform]")
{
    SJH::Transform t;
    t.EulerRot = glm::vec3(0.0f, 90.0f, 0.0f); // degree
    // Y+90: col0(Right)=(0,0,-1), col2=(1,0,0) → Forward=-col2=(-1,0,0)
    RequireVec3Near(t.GetRight(), glm::vec3(0.0f, 0.0f, -1.0f));
    RequireVec3Near(t.GetForward(), glm::vec3(-1.0f, 0.0f, 0.0f));
    RequireVec3Near(t.GetUp(), glm::vec3(0.0f, 1.0f, 0.0f)); // 불변
}

TEST_CASE("Transform: TRS 합성 순서 (T*Rz*S)", "[transform]")
{
    SJH::Transform t;
    t.Translate = glm::vec3(1.0f, 0.0f, 0.0f);
    t.EulerRot = glm::vec3(0.0f, 0.0f, 30.0f); // Z 30도
    t.Scale = glm::vec3(2.0f, 2.0f, 2.0f);

    // impl 미러: T * (Rz*Ry*Rx) * S, 여기선 Rx=Ry=0
    glm::mat4 expected =
        glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 2.0f));
    RequireMatNear(t.GetLocalMatrix(), expected);
}
```

- [ ] **Step 2: test_light.cpp 작성** (object/light.h 한정 — Light POD + GetAttenuationCoeff 구조적 property)

`test/test_light.cpp`:
```cpp
// SJH::Light(object/light.h) POD 기본값 + GetAttenuationCoeff 구조 property characterization.
// scene caster 라이트(DirLight/PointLight/SpotLight)는 SJH::scene 소속 → 별도 테스트로 defer
// (contract-anchoring: 이 exe 는 SJH::object 만 link). spec D3/D8.
#include <<catch2>/catch_test_macros.hpp>
#include <<catch2>/matchers/catch_matchers_floating_point.hpp>

#include "object/light.h"
#include <cmath> // std::isfinite

using Catch::Matchers::WithinAbs;

TEST_CASE("Light: 기본값", "[light]")
{
    SJH::Light l;
    REQUIRE_THAT(l.Pos.x, WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(l.Pos.y, WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(l.Pos.z, WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(l.Ambient.x, WithinAbs(0.1f, 1e-6f));
    REQUIRE_THAT(l.Diffuse.x, WithinAbs(0.5f, 1e-6f));
    REQUIRE_THAT(l.Specular.x, WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("Light: GetAttenuationCoeff 구조 property", "[light]")
{
    // 반환 = vec3(Kc=1, max(Kl,0), max(Kq^2,0)). 정확 산술 대신 계약 property 검증(D8a).
    glm::vec3 c = SJH::GetAttenuationCoeff(32.0f);
    REQUIRE_THAT(c.x, WithinAbs(1.0f, 1e-6f)); // Kc 상수 1
    REQUIRE(c.y > 0.0f);                       // Kl > 0
    REQUIRE(c.z >= 0.0f);                      // Kq^2 >= 0
}

TEST_CASE("Light: 감쇠는 거리 증가에 단조 감소(Kl)", "[light]")
{
    float near = SJH::GetAttenuationCoeff(8.0f).y;
    float far = SJH::GetAttenuationCoeff(64.0f).y;
    REQUIRE(far < near); // 멀수록 Kl 작음
}

TEST_CASE("Light: 큰 거리에서 유한값(NaN/Inf 없음)", "[light]")
{
    glm::vec3 c = SJH::GetAttenuationCoeff(1000.0f);
    REQUIRE(std::isfinite(c.x));
    REQUIRE(std::isfinite(c.y));
    REQUIRE(std::isfinite(c.z));
}
```

- [ ] **Step 3: test/CMakeLists.txt 수정 — 2개 타겟 + 우산 추가**

`test/CMakeLists.txt` 의 `sjh_add_test(test_timer SJH::timer)` 줄 **아래**에 추가:
```cmake
sjh_add_test(test_transform SJH::object)
sjh_add_test(test_light SJH::object)
```
그리고 `add_custom_target(tests DEPENDS test_timer)` 를 다음으로 **교체**:
```cmake
add_custom_target(tests DEPENDS test_timer test_transform test_light)
```

- [ ] **Step 4: 빌드 + 실행**

Run:
```bash
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target test_transform --target test_light
ctest --test-dir build_ninja -R "transform|light" --output-on-failure
```
Expected: transform/light 케이스 전부 PASS. (`-R` 정규식이 `[transform]`/`[light]` 태그가 아니라 테스트명에 매칭 — 케이스명에 "Transform"/"Light" 포함되어 잡힘.)

- [ ] **Step 5: 커밋**

```bash
git add test/test_transform.cpp test/test_light.cpp test/CMakeLists.txt
git commit test/test_transform.cpp test/test_light.cpp test/CMakeLists.txt -m "[test] test_transform/test_light characterization (094362d 복원, vmath->glm 수리)"
```

> **☆ 보조 handoff 지점:** 여기서 컨텍스트가 빠듯하면 절단. 복원+수리 패턴이 2모듈에 증명됨 → Task 3~4 는 동일 패턴 반복. 절단 시 `lossless-handoff` 스킬(Artifact B)로 간이 resume 작성.

---

## Task 3 (U3): `test_sprite_uvrect` — `ComputeUVRect` (094362d 거의 verbatim)

**Files:**
- Create: `test/test_sprite_uvrect.cpp`
- Modify: `test/CMakeLists.txt`

**Drift: 없음.** `ComputeUVRect(frameIdx,cols,tileSize,atlasW,atlasH)` 무변경. 반환 `(uMin,vMin,uSize,vSize)`, `col=idx%cols`, `row=idx/cols`, V-flip 없음, guard `cols<=0||atlasW<=0||atlasH<=0`→zero.

- [ ] **Step 1: test_sprite_uvrect.cpp 작성** (모두 정확 이진분수 → `Approx`)

`test/test_sprite_uvrect.cpp`:
```cpp
// SJH::Sprite::ComputeUVRect characterization (094362d test_uniform_atlas 복원, drift 없음).
// 반환 vec4 = (uMin, vMin, uSize, vSize). spec D8a.
#include <<catch2>/catch_test_macros.hpp>
#include <<catch2>/catch_approx.hpp>

#include "sprite/uniform_atlas.h"

using SJH::Sprite::ComputeUVRect;
using Catch::Approx;

TEST_CASE("ComputeUVRect: 4x4 / 256 / 64px 정사각", "[sprite][uvrect]")
{
    SECTION("frame 0 = 좌상단")
    {
        glm::vec4 uv = ComputeUVRect(0, 4, 64, 256, 256);
        REQUIRE(uv.x == Approx(0.0f));  // uMin
        REQUIRE(uv.y == Approx(0.0f));  // vMin
        REQUIRE(uv.z == Approx(0.25f)); // uSize 64/256
        REQUIRE(uv.w == Approx(0.25f)); // vSize
    }
    SECTION("frame 5 = (col1,row1)")
    {
        glm::vec4 uv = ComputeUVRect(5, 4, 64, 256, 256);
        REQUIRE(uv.x == Approx(0.25f)); // 1*64/256
        REQUIRE(uv.y == Approx(0.25f));
        REQUIRE(uv.z == Approx(0.25f));
        REQUIRE(uv.w == Approx(0.25f));
    }
    SECTION("frame 15 = (col3,row3)")
    {
        glm::vec4 uv = ComputeUVRect(15, 4, 64, 256, 256);
        REQUIRE(uv.x == Approx(0.75f)); // 3*64/256
        REQUIRE(uv.y == Approx(0.75f));
    }
}

TEST_CASE("ComputeUVRect: 8x4 비정사각 (6-arg)", "[sprite][uvrect]")
{
    SECTION("frame 0")
    {
        glm::vec4 uv = ComputeUVRect(0, 8, 64, 64, 512, 256);
        REQUIRE(uv.x == Approx(0.0f));
        REQUIRE(uv.y == Approx(0.0f));
        REQUIRE(uv.z == Approx(0.125f)); // 64/512
        REQUIRE(uv.w == Approx(0.25f));  // 64/256
    }
    SECTION("frame 8 = (col0,row1)")
    {
        glm::vec4 uv = ComputeUVRect(8, 8, 64, 64, 512, 256);
        REQUIRE(uv.x == Approx(0.0f));
        REQUIRE(uv.y == Approx(0.25f)); // 1*64/256
        REQUIRE(uv.z == Approx(0.125f));
        REQUIRE(uv.w == Approx(0.25f));
    }
}

TEST_CASE("ComputeUVRect: 무효 입력은 zero rect", "[sprite][uvrect]")
{
    REQUIRE(ComputeUVRect(0, 0, 64, 256, 256) == glm::vec4(0.0f));   // cols=0
    REQUIRE(ComputeUVRect(0, 4, 64, 0, 256) == glm::vec4(0.0f));     // atlasW=0
}
```

- [ ] **Step 2: test/CMakeLists.txt 수정**

`sjh_add_test(test_light SJH::object)` 아래에 추가:
```cmake
sjh_add_test(test_sprite_uvrect SJH::sprite)
```
우산 교체:
```cmake
add_custom_target(tests DEPENDS test_timer test_transform test_light test_sprite_uvrect)
```

- [ ] **Step 3: 빌드 + 실행**

Run:
```bash
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target test_sprite_uvrect
ctest --test-dir build_ninja -R "ComputeUVRect" --output-on-failure
```
Expected: 전부 PASS.

- [ ] **Step 4: 커밋**

```bash
git add test/test_sprite_uvrect.cpp test/CMakeLists.txt
git commit test/test_sprite_uvrect.cpp test/CMakeLists.txt -m "[test] test_sprite_uvrect characterization (ComputeUVRect, 094362d 복원)"
```

---

## Task 4 (U4): `test_fsm` — `StateMachine` mock 전이 + 우산 완성

**Files:**
- Create: `test/test_fsm.cpp`
- Modify: `test/CMakeLists.txt`

**핵심 계약(VERBATIM from `state_machine.h`):** `TState`=`enum class : uint64_t`(`NONE=0`+단일비트). `StateMachine(owner, startup=NONE)`. `TryTransit`은 **큐잉만**(`mPendingState`/`mHasPending`) — 게이트 `curr!=0 && 현재등록 && (현재.GetTransitFlag()&targetBit)==targetBit && 타겟등록`. **`Update(dt)` 가 `ApplyPending`(OnExit(from)→curState=to→OnEnter(to)) 후 `OnUpdate(cur)`**. 전이는 다음 `Update` 후에야 반영. `ForceTransit` 실패 시 `abort()` → **abort 분기 테스트 금지**.

- [ ] **Step 1: test_fsm.cpp 작성** (mock enum/owner/state + 전이 시나리오)

`test/test_fsm.cpp`:
```cpp
// SJH::FSM::StateMachine characterization — mock TState/TOwner/IFsmState 로 전이 계약 잠금.
// 핵심: TryTransit 은 큐잉만, Update 가 적용(OnExit→curState→OnEnter). spec D3/D8a.
#include <<catch2>/catch_test_macros.hpp>

#include "fsm/state_machine.h" // StateMachine + IFsmState (+ scene/actor.h 전이)

namespace
{
    // 1) TState: enum class : uint64_t, NONE=0 + 단일 비트 (StateMachine 요구사항)
    enum class MockState : uint64_t
    {
        NONE = 0,
        A = 1ull << 0,
        B = 1ull << 1,
        C = 1ull << 2,
    };

    // 2) TOwner: 스파이 카운터 보유 plain struct
    struct MockOwner
    {
        int enterA = 0, exitA = 0, updateA = 0;
        int enterB = 0, exitB = 0, updateB = 0;
    };

    // 3) 구체 IFsmState<MockOwner>: A 는 B 로만 전이 허용, B 는 A 로만
    class StateA : public SJH::FSM::IFsmState<MockOwner>
    {
      public:
        uint64_t GetStateFlag() const override { return (uint64_t)MockState::A; }
        uint64_t GetTransitFlag() const override { return (uint64_t)MockState::B; }
        void OnEnter(MockOwner &o) override { o.enterA++; }
        void OnUpdate(MockOwner &o, float) override { o.updateA++; }
        void OnExit(MockOwner &o) override { o.exitA++; }
    };
    class StateB : public SJH::FSM::IFsmState<MockOwner>
    {
      public:
        uint64_t GetStateFlag() const override { return (uint64_t)MockState::B; }
        uint64_t GetTransitFlag() const override { return (uint64_t)MockState::A; }
        void OnEnter(MockOwner &o) override { o.enterB++; }
        void OnUpdate(MockOwner &o, float) override { o.updateB++; }
        void OnExit(MockOwner &o) override { o.exitB++; }
    };

    using SM = SJH::FSM::StateMachine<MockState, MockOwner>;

    // SM 은 unique_ptr 멤버 보유(move-only) + Component 가 user-declared dtor 라 move 억제 가능.
    // 값 반환 함정을 피하려 참조로 등록한다.
    void RegisterAB(SM &sm)
    {
        sm.RegisterState(std::make_unique<StateA>());
        sm.RegisterState(std::make_unique<StateB>());
    }
}

TEST_CASE("FSM: NONE 에서는 전이 불가", "[fsm]")
{
    MockOwner owner;
    SM sm(owner, MockState::NONE);
    RegisterAB(sm);
    REQUIRE(sm.State() == MockState::NONE);
    REQUIRE_FALSE(sm.TryTransit(MockState::A)); // curr==0 → false
    REQUIRE(sm.State() == MockState::NONE);
}

TEST_CASE("FSM: 유효 전이는 큐잉되고 Update 에서 적용", "[fsm]")
{
    MockOwner owner;
    SM sm(owner, MockState::A);
    RegisterAB(sm); // startup A
    REQUIRE(sm.State() == MockState::A);

    REQUIRE(sm.TryTransit(MockState::B)); // A.transit=B, B 등록 → true (큐잉만)
    REQUIRE(sm.State() == MockState::A);  // 아직 적용 전
    REQUIRE(owner.exitA == 0);
    REQUIRE(owner.enterB == 0);

    sm.Update(0.016f); // ApplyPending: OnExit(A)→curState=B→OnEnter(B), 이어 OnUpdate(B)
    REQUIRE(sm.State() == MockState::B);
    REQUIRE(owner.exitA == 1);
    REQUIRE(owner.enterB == 1);
    REQUIRE(owner.updateB == 1);
}

TEST_CASE("FSM: 플래그 미허용 전이는 거부", "[fsm]")
{
    MockOwner owner;
    SM sm(owner, MockState::A);
    RegisterAB(sm);
    // A.GetTransitFlag()=B 뿐 → C 로 전이 시 (B & C)=0 → false
    REQUIRE_FALSE(sm.TryTransit(MockState::C));
    sm.Update(0.016f);
    REQUIRE(sm.State() == MockState::A); // 불변
}

TEST_CASE("FSM: 왕복 전이 A->B->A", "[fsm]")
{
    MockOwner owner;
    SM sm(owner, MockState::A);
    RegisterAB(sm);
    REQUIRE(sm.TryTransit(MockState::B));
    sm.Update(0.016f); // → B
    REQUIRE(sm.State() == MockState::B);
    REQUIRE(sm.TryTransit(MockState::A)); // B.transit=A
    sm.Update(0.016f); // → A
    REQUIRE(sm.State() == MockState::A);
    REQUIRE(owner.enterA == 1); // 복귀 시 OnEnter(A) 1회
    REQUIRE(owner.exitB == 1);
}

TEST_CASE("FSM: Update 는 현재 상태 OnUpdate 를 매번 호출", "[fsm]")
{
    MockOwner owner;
    SM sm(owner, MockState::A);
    RegisterAB(sm);
    sm.Update(0.016f); // pending 없음 → OnUpdate(A)
    sm.Update(0.016f);
    REQUIRE(owner.updateA == 2);
}
```

> ⚠ **현 코드 미검증 영역 주의:** `StateMachine` 은 production 사용 0(CLAUDE.md). 이 테스트가 **최초 인스턴스화/컴파일**일 수 있어 빌드 자체가 계약 검증이다. 컴파일 실패(예: Component pure-virtual 누락, 템플릿 오류)는 그 자체로 finding — §"characterization 사이클" 규칙대로 보고(임의 수정으로 덮지 말 것).

- [ ] **Step 2: test/CMakeLists.txt 수정 — fsm 추가 + 우산 완성**

`sjh_add_test(test_sprite_uvrect SJH::sprite)` 아래 추가:
```cmake
sjh_add_test(test_fsm SJH::fsm)
```
우산 최종형으로 교체:
```cmake
add_custom_target(tests DEPENDS test_timer test_transform test_light test_sprite_uvrect test_fsm)
```

- [ ] **Step 3: 빌드 + 실행 (전체 우산)**

Run:
```bash
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target tests
ctest --test-dir build_ninja --output-on-failure
```
Expected: smoke + 5 모듈 테스트 전체 PASS. `ctest --test-dir build_ninja -N` 로 등록 케이스 전수 나열 확인.

- [ ] **Step 4: 커밋**

```bash
git add test/test_fsm.cpp test/CMakeLists.txt
git commit test/test_fsm.cpp test/CMakeLists.txt -m "[test] test_fsm characterization (StateMachine mock 전이; 최초 인스턴스화 검증)"
```

---

## Task 5 (U5): cross-version differential 하네스 `crossver_verify.sh`

**Files:**
- Create: `<scripts>/crossver_verify.sh`

**메커니즘(spec §3.5):** 두 커밋을 git worktree 로 펼치고, 현 `test/`+`test_smoke/` 를 각 worktree 에 overlay, 각자 빌드+ctest 후 결과(테스트명+pass/fail, 타이밍 제거 정규화) diff. `315269a` 가 root CMakeLists 미변경이라 overlay 만으로 기존 가드가 집어듦.

> **day-0 선결:** `315269a` worktree 빌드가 GREEN 이어야 함(사용자 보장). 실패 시 fallback = base/cand 둘 다 HEAD 로 줘서 self-regression(결정성 카나리아)만 돌림.

- [ ] **Step 1: crossver_verify.sh 작성**

`<scripts>/crossver_verify.sh`:
```bash
#!/usr/bin/env bash
# cross-version differential: 두 커밋에서 동일 test/ 를 실행해 결과 등가성 검증.
# 사용법: sh <scripts>/crossver_verify.sh [baseRef] [candRef]
#   기본 baseRef=7949a65 (안정 oracle), candRef=315269a (리팩토링 완료본)
# 종료코드 0 = 등가, 1 = 차이(잠재 회귀), 2 = 빌드/환경 실패.
set -u

BASE_REF="${1:-7949a65}"
CAND_REF="${2:-315269a}"

REPO_ROOT="$(git rev-parse --show-toplevel)"
WORK_DIR="$(mktemp -d)"
trap 'cd "$REPO_ROOT"; git worktree remove --force "$WORK_DIR/base" 2>/dev/null; \
      git worktree remove --force "$WORK_DIR/cand" 2>/dev/null; rm -rf "$WORK_DIR"' EXIT

# 현 HEAD 의 테스트 소스(아직 미커밋일 수 있으니 working tree 에서 복사)
TEST_SRC="$REPO_ROOT/test"
SMOKE_SRC="$REPO_ROOT/test_smoke"

run_ref() {
    ref="$1"; label="$2"; wt="$WORK_DIR/$label"
    echo "=== [$label] worktree @ $ref ==="
    git -C "$REPO_ROOT" worktree add --detach "$wt" "$ref" >/dev/null || return 2
    # 현 테스트 소스를 overlay (대상 커밋엔 test/ 가 없음; root CMakeLists 의 EXISTS 가드가 집어듦)
    rm -rf "$wt/test" "$wt/test_smoke"
    cp -R "$TEST_SRC" "$wt/test"
    cp -R "$SMOKE_SRC" "$wt/test_smoke"
    (
        cd "$wt" || exit 2
        cmake --preset ninja -DENABLE_TESTING=ON >/dev/null 2>&1 || exit 2
        cmake --build --preset ninja --target tests >/dev/null 2>&1 || exit 2
        # 타이밍/번호 제거 정규화 → 테스트명 + Passed/Failed 만 정렬
        ctest --test-dir build_ninja 2>/dev/null \
          | grep -E 'Test +#[0-9]+:' \
          | sed -E 's/^ *Test +#[0-9]+: +//; s/ +\.+ +/ /; s/ +[0-9.]+ sec$//' \
          | sort
    ) > "$WORK_DIR/$label.result"
    return $?
}

run_ref "$BASE_REF" "base"; rc_base=$?
run_ref "$CAND_REF" "cand"; rc_cand=$?

if [ "$rc_base" -ne 0 ] || [ "$rc_cand" -ne 0 ]; then
    echo "FAIL: 빌드/환경 실패 (base rc=$rc_base, cand rc=$rc_cand). 315269a 빌드 GREEN 인지 확인." >&2
    exit 2
fi

echo "=== diff (base=$BASE_REF vs cand=$CAND_REF) ==="
if diff -u "$WORK_DIR/base.result" "$WORK_DIR/cand.result"; then
    echo "EQUIVALENT: 두 커밋의 테스트 결과 동일 ($(wc -l < "$WORK_DIR/base.result") 케이스)."
    exit 0
else
    echo "DIVERGENT: 결과 차이 = 리팩토링이 공개 동작을 바꿈(잠재 회귀)." >&2
    exit 1
fi
```

- [ ] **Step 2: 실행 권한 + day-0 선결 확인**

Run: `chmod +x <scripts>/crossver_verify.sh`
그리고 `315269a` 빌드 GREEN 사전 확인(사용자 직접 빌드 관례):
```bash
git worktree add --detach /tmp/wt-315 315269a && cd /tmp/wt-315 && cmake --preset ninja -DENABLE_TESTING=ON && cmake --build --preset ninja --target _MyApp_
# 확인 후: cd - ; git worktree remove --force /tmp/wt-315
```
Expected: 315269a 가 빌드됨. (안 되면 fallback: Step 3 를 `sh <scripts>/crossver_verify.sh 7949a65 7949a65` 로 self-regression 만.)

- [ ] **Step 3: 실행 — 등가성 확인**

Run: `sh <scripts>/crossver_verify.sh 7949a65 315269a`
Expected: `EQUIVALENT: 두 커밋의 테스트 결과 동일 (N 케이스).` 종료코드 0.
- `DIVERGENT` (코드 1) → 315269a 의 dead-code 제거가 공개 동작을 바꿈 = 실제 회귀 발견(가치 있는 결과). 사람 보고.
- `FAIL` (코드 2) → 빌드/환경. day-0 선결 재확인.

- [ ] **Step 4: 커밋**

```bash
git add <scripts>/crossver_verify.sh
git commit <scripts>/crossver_verify.sh -m "[test] crossver_verify.sh — 두 커밋 동일 테스트 differential 등가 검증"
```

---

## Task 6 (★ HANDOFF #1): Phase 1 종료 무손실 인계

**Files:**
- Create: `doc/handoffs/2026-06-25/2026-06-25-test-harness-p1-resume-handoff.md` (⚠ doc/ gitignore 로컬 — same-machine 세션 연속성용)

- [ ] **Step 1: 전체 GREEN 재확인**

Run: `cmake --preset ninja -DENABLE_TESTING=ON && cmake --build --preset ninja --target tests && ctest --test-dir build_ninja --output-on-failure`
Expected: smoke + 5 모듈 전부 PASS. + `sh <scripts>/crossver_verify.sh` 등가(또는 발견한 divergence 기록).

- [ ] **Step 2: `lossless-handoff` 스킬로 Artifact B 작성** (spec D10)

`lossless-handoff` 스킬(Artifact B = resume/context doc)을 호출해 작성. 반드시 포함:
- **Step 0 컨벤션**: 빌드(`cmake --preset ninja -DENABLE_TESTING=ON` …), 커밋(path-scoped, Co-Authored-By 미사용), 테스트(characterization, no red-first).
- **grounding 재측정**: 작성 시점 `git log --oneline -8` / `git status` / 각 test 파일 `file:line`.
- **State 표**: Task 0~5 완료 SHA, ctest 케이스 수, crossver 결과(EQUIVALENT/DIVERGENT).
- **Locked 결정**: spec D1~D10 요약 + 정본 경로 `doc/superpowers/specs/2026-06-25-engine-test-harness-design.md`.
- **다음 = Phase 2 Track B 별도 plan** (성격 전환: GL fixture+FBO+glReadPixels+FLIP, ⚙backend-volatile §11 — 착수 시 GL vs Metal/Vulkan 재결정).
- **guardrails**: doc/ gitignore 로컬(cross-machine 시 tracked dir), 사용자 병렬 git.

- [ ] **Step 3: 메모리 진입점 갱신**

`lossless-handoff` hygiene(원칙 5): 새 메모리 `engine-test-harness-effort`(project type) 작성 — 진입점=이 handoff 경로, 상태=Phase 1 완료/Phase 2 대기. MEMORY.md 인덱스 1줄 추가. ([[backend-abstraction-direction]] 와 상호 링크.)

---

## Self-Review (작성자 체크)

**1. Spec coverage:** D1(순서·Track A)→Task0~5 / D3(5타겟)→Task0~4 / D4(oracle·characterization)→사이클 섹션 / D5(구조 C)→Task0+1 / D6(contract-anchoring·crossver)→link 규칙+Task5 / D7→Task5 / D8(하이브리드 a/b)→각 테스트 / D10(lossless-handoff)→Task6. **누락 없음.** (D2 Mull·D9 backend·Track B 골든 = spec OUT, 별도 plan — 의도된 범위 밖.)

**2. Placeholder scan:** 모든 step 에 실제 코드/명령/기대값. "TBD/적절히/유사하게" 없음. 기대값 전부 손계산(이진분수) 또는 구조 property.

**3. Type consistency:** `sjh_add_test(name, module)` 헬퍼 Task1 정의 → Task2~4 동일 사용. `MockState`/`MockOwner`/`StateA`/`StateB`/`SM` Task4 내 자기완결. 링크 타겟명(`SJH::common/timer/object/sprite/fsm`) Dependency 실측과 일치. Catch2 헤더 경로 v3.13.0 실측 일치.

**잔여 리스크(실행자 인지):** (i) `test_fsm` = StateMachine 최초 인스턴스화 → 컴파일 실패 가능(보고 대상). (ii) `test_light` scene caster 라이트 defer(object 한정). (iii) 315269a 빌드 day-0 선결.
