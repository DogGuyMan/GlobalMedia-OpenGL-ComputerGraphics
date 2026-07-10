# tweeny_demo — 11 Easing Ping-Pong Planes — Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `apps/tweeny_demo/` 챕터를 신규 추가해 Tweeny 의 11개 대표 easing 함수를 11개 plane 의 좌↔우 ping-pong 으로 시연한다.

**Architecture:** 단일 GLSL 410 ortho 셰이더, NDC 좌표 직접 사용. `SJH::Mesh::CreateScreenQuad()` 한 개를 11행이 공유 — uniform (`uOffset`, `uScale`, `baseColor`) 만 갱신 + 11회 `glDrawElements`. 모든 동작은 `<apps>/tweeny_demo/main.cpp` 한 파일 안에 완결.

**Tech Stack:** C++17, sb7 base, GLFW, OpenGL 4.1 Core / GLSL 410, Tweeny (header-only via `game_deps`), `SJH::engine` (Mesh + Program + Shader + Uniforms).

**Spec:** [`doc/superpowers/specs/2026-05-24-tweeny-demo-ping-pong-easings.md`](../specs/2026-05-24-tweeny-demo-ping-pong-easings.md)

---

## File Structure

| 파일 | 책임 |
|---|---|
| `apps/tweeny_demo/CMakeLists.txt` (신규) | tweeny_demo 실행 파일 + 링크 (`project_deps + game_deps + SJH::engine`) + POST_BUILD 리소스 복사 + MSVC SUBSYSTEM 처리 |
| `<apps>/tweeny_demo/main.cpp` (재작성) | sb7::application 상속. init/startup/render/shutdown 4 메서드. 11개 `EasingRow` POD. ping-pong 토글 로직. |
| `apps/tweeny_demo/resources/shader/simple.vs` (신규) | NDC 직접 출력 — `gl_Position = vec4(aPos.xy * uScale + uOffset, 0, 1)` |
| `apps/tweeny_demo/resources/shader/simple.fs` (수정) | `#version 330 core` → `#version 410 core` 한 줄 패치만 |
| `apps/CMakeLists.txt` (수정) | `add_subdirectory(tweeny_demo)` 한 줄 추가 |

---

## Task 1: CMakeLists 신규 + apps 등록 (스켈레톤 빌드 통과)

**목표:** `tweeny_demo` 타겟이 빈 `main.cpp` 만으로 빌드 가능한 상태.

**Files:**
- Create: `apps/tweeny_demo/CMakeLists.txt`
- Modify: `apps/CMakeLists.txt` (한 줄 추가)
- Modify: `<apps>/tweeny_demo/main.cpp` (skeleton 정리 — 컴파일만 통과)

- [ ] **Step 1.1: `apps/tweeny_demo/CMakeLists.txt` 작성**

```cmake
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_executable(${CHAPTER_NAME} main.cpp)

target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    game_deps
    SJH::engine
)

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
    target_link_options(${CHAPTER_NAME} PRIVATE
        "/SUBSYSTEM:CONSOLE"
        "/ENTRY:WinMainCRTStartup")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources)
endif()
```

- [ ] **Step 1.2: `apps/CMakeLists.txt` 에 등록**

기존 파일:
```cmake
# add_subdirectory(_MyApp_)
add_subdirectory(migrate_demo)
add_subdirectory(box2d_demo)
add_subdirectory(effekseer_demo)
# add_subdirectory(audio_demo)
```

`add_subdirectory(effekseer_demo)` 뒤에 한 줄 추가 → 최종:
```cmake
# add_subdirectory(_MyApp_)
add_subdirectory(migrate_demo)
add_subdirectory(box2d_demo)
add_subdirectory(effekseer_demo)
add_subdirectory(tweeny_demo)
# add_subdirectory(audio_demo)
```

- [ ] **Step 1.3: `<apps>/tweeny_demo/main.cpp` 를 컴파일 가능한 최소 skeleton 으로 정리**

기존 main.cpp 의 미완성 `auto planeModel = SJH::Mes` 가 컴파일 실패 — 다음으로 전면 교체:

```cpp
/**
 * @file main.cpp
 * @brief tweeny_demo — 11 easings ping-pong planes (skeleton, Task 1).
 *        실제 컨텐츠는 Task 2 이후로 채워진다.
 */

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <sb7.h>

class tweeny_demo_app : public sb7::application
{
public:
    void init() override
    {
        sb7::application::init();
        info.majorVersion = 4;
        info.minorVersion = 1; // GLSL 410 (project policy)
        info.windowWidth  = 800;
        info.windowHeight = 600;
        // sb7::APPINFO::title 은 char[128] 배열 — 안전한 strncpy 로 복사.
#ifdef _WIN32
        strncpy_s(info.title, sizeof(info.title), "tweeny_demo - 11 easings ping-pong", _TRUNCATE);
#else
        std::strncpy(info.title, "tweeny_demo - 11 easings ping-pong", sizeof(info.title) - 1);
        info.title[sizeof(info.title) - 1] = '\0';
#endif
    }

    void startup() override   {}
    void render(double)       override { glClearColor(0.1f, 0.1f, 0.12f, 1.0f); glClear(GL_COLOR_BUFFER_BIT); }
    void shutdown()           override {}
};

DECLARE_MAIN(tweeny_demo_app)
```

> `#include <cstring>` 가 필요 — 다음 step.

- [ ] **Step 1.4: 누락 헤더 추가**

`main.cpp` 상단의 `#include <sb7.h>` 다음 줄에 추가:

```cpp
#include <cstring>
```

- [ ] **Step 1.5: 빌드**

```bash
cmake --preset ninja
cmake --build --preset ninja --target tweeny_demo
```

Expected: 컴파일/링크 무에러. `build_ninja/apps/tweeny_demo/tweeny_demo` 생성.

- [ ] **Step 1.6: 실행 — 빈 진청색 윈도우 확인**

```bash
cd build_ninja/apps/tweeny_demo && ./tweeny_demo
```

Expected: 800×600 윈도우, 다크블루 (`0.1, 0.1, 0.12`) 배경. ESC 또는 X 로 종료.

- [ ] **Step 1.7: Commit**

```bash
git add apps/tweeny_demo/CMakeLists.txt <apps>/tweeny_demo/main.cpp apps/CMakeLists.txt
git commit -m "[init] : tweeny_demo CMake skeleton + apps 등록"
```

---

## Task 2: 셰이더 파일 작성 + 리소스 복사 확인

**목표:** simple.vs / simple.fs 가 GLSL 410 으로 정합되고 POST_BUILD 로 실행 디렉토리에 복사된다.

**Files:**
- Create: `apps/tweeny_demo/resources/shader/simple.vs`
- Modify: `apps/tweeny_demo/resources/shader/simple.fs` (version 패치)

- [ ] **Step 2.1: `simple.vs` 작성**

```glsl
#version 410 core

layout(location = 0) in vec3 aPos;     // Vertex.position (vec3). xy 만 사용.
layout(location = 1) in vec3 aNormal;  // unused — Vertex 레이아웃 호환만
layout(location = 2) in vec2 aTexCoord;// unused

uniform vec2 uOffset; // (tweenX, rowY) NDC
uniform vec2 uScale;  // (halfWidth, halfHeight) NDC

void main()
{
    gl_Position = vec4(aPos.xy * uScale + uOffset, 0.0, 1.0);
}
```

- [ ] **Step 2.2: `simple.fs` 의 version 만 410 으로 패치**

기존:
```glsl
#version 330 core
```
→
```glsl
#version 410 core
```

나머지 (`uniform vec4 baseColor; out vec4 fragColor; void main()...`) 는 그대로 유지.

- [ ] **Step 2.3: 빌드 후 리소스 복사 확인**

```bash
cmake --build --preset ninja --target tweeny_demo
ls build_ninja/apps/tweeny_demo/resources/shader/
```

Expected: `simple.fs`, `simple.vs` 두 파일이 빌드 디렉토리에 복사돼 있음.

- [ ] **Step 2.4: Commit**

```bash
git add apps/tweeny_demo/resources/
git commit -m "[init] : tweeny_demo simple.vs/fs GLSL 410 셰이더"
```

---

## Task 3: 메시 + 프로그램 로드 + 단일 plane 렌더 (Sanity)

**목표:** Tweeny 도입 전에 SJH API (`Mesh::CreateScreenQuad`, `Program::CreateWithVSFS`, `Uniforms::SetVec2/SetVec4`) 가 한 번에 동작하는지 확인 — 화면 중앙에 작은 빨간 사각형 한 개.

**Files:**
- Modify: `<apps>/tweeny_demo/main.cpp` (startup/render/shutdown 채우기)

- [ ] **Step 3.1: 헤더 추가**

main.cpp 상단 (`#include <cstring>` 다음) 에 추가:

```cpp
#include "object/mesh.h"
#include "program/program.h"
#include "program/program_uniforms.h"

#include <vmath.h>
```

- [ ] **Step 3.2: 멤버 + startup() 구현**

class 선언 안에 멤버 추가:

```cpp
private:
    SJH::MeshUPtr    mQuad;
    SJH::ProgramUPtr mProgram;
```

`startup()` 구현:

```cpp
void startup() override
{
    mQuad = SJH::Mesh::CreateScreenQuad();
    mProgram = SJH::Program::CreateWithVSFS(
        "./resources/shader/simple.vs",
        "./resources/shader/simple.fs");
}
```

- [ ] **Step 3.3: render() 에 단일 plane 그리기**

`render(double)` 본문을 교체:

```cpp
void render(double /*currentTime*/) override
{
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(mProgram->GetProgramAddr());
    glBindVertexArray(mQuad->GetVAO());

    SJH::Uniforms::SetVec2(*mProgram, "uOffset",   vmath::vec2(0.0f, 0.0f));
    SJH::Uniforms::SetVec2(*mProgram, "uScale",    vmath::vec2(0.05f, 0.035f));
    SJH::Uniforms::SetVec4(*mProgram, "baseColor", vmath::vec4(1.0f, 0.2f, 0.2f, 1.0f));

    glDrawElements(mQuad->GetPrimitiveType(),
                   mQuad->GetIndexCount(),
                   GL_UNSIGNED_INT,
                   nullptr);
}
```

- [ ] **Step 3.4: shutdown() 명시 정리**

```cpp
void shutdown() override
{
    mProgram.reset();
    mQuad.reset();
}
```

- [ ] **Step 3.5: 빌드 + 실행**

```bash
cmake --build --preset ninja --target tweeny_demo
cd build_ninja/apps/tweeny_demo && ./tweeny_demo
```

Expected: 다크블루 배경 + 화면 중앙에 작은 빨간 사각형 (폭 화면의 ~10%, 높이 ~7%).

> 검은 화면이거나 빨간 사각형이 안 보이면: 콘솔의 셰이더 컴파일/link 로그를 확인. `glGetError()` 디버그 출력은 `SJH::diagnostics::GLDebug::CheckGLError` 추가 호출로 진단.

- [ ] **Step 3.6: Commit**

```bash
git add <apps>/tweeny_demo/main.cpp
git commit -m "[dev] : tweeny_demo single plane sanity (Mesh+Program+Uniforms)"
```

---

## Task 4: 11개 EasingRow POD + Tweeny 통합 (정적 — tween step X)

**목표:** 11개 EasingRow 가 생성되고 startup 직후 X 좌표를 *각자의 from 위치* (-0.85) 에 두고 11행 모두 정적 렌더. ping-pong 은 아직 없음.

**Files:**
- Modify: `<apps>/tweeny_demo/main.cpp`

- [ ] **Step 4.1: tweeny + 표준 라이브러리 헤더 추가**

main.cpp 의 include 블록에 추가:

```cpp
#include <tweeny.h>

#include <array>
#include <random>
#include <vector>
```

- [ ] **Step 4.2: EasingRow 구조체 정의**

class 선언 *밖*, 파일 상단의 namespace 영역에 추가 (또는 class 내부 nested struct 도 가능 — 본 plan 은 파일 상단 자유 namespace 선택):

```cpp
namespace
{
    struct EasingRow
    {
        const char*          name;
        tweeny::tween<float> tween;
        vmath::vec4          color;
        float                y;
        bool                 forward = true;
    };

    constexpr int   kRowCount       = 11;
    constexpr float kTweenFromX     = -0.85f;
    constexpr float kTweenToX       =  0.85f;
    constexpr int   kTweenDurationMs = 2000;
}
```

- [ ] **Step 4.3: 멤버 추가 — `std::vector<EasingRow> mRows;`**

class private 멤버:
```cpp
std::vector<EasingRow> mRows;
```

- [ ] **Step 4.4: startup() 에서 11개 EasingRow 생성**

`startup()` 의 mProgram 초기화 뒤에 추가:

```cpp
std::mt19937 rng{42u};
std::uniform_real_distribution<float> distColor(0.3f, 1.0f);

auto makeRow = [&](const char* name, auto&& easing) {
    EasingRow row;
    row.name  = name;
    row.tween = tweeny::from(kTweenFromX)
                    .to(kTweenToX)
                    .during(kTweenDurationMs)
                    .via(easing);
    row.color = vmath::vec4(distColor(rng), distColor(rng), distColor(rng), 1.0f);
    return row;
};

mRows.reserve(kRowCount);
mRows.push_back(makeRow("linear",          tweeny::easing::linear));
mRows.push_back(makeRow("quadraticInOut",  tweeny::easing::quadraticInOut));
mRows.push_back(makeRow("cubicInOut",      tweeny::easing::cubicInOut));
mRows.push_back(makeRow("quarticInOut",    tweeny::easing::quarticInOut));
mRows.push_back(makeRow("quinticInOut",    tweeny::easing::quinticInOut));
mRows.push_back(makeRow("sinusoidalInOut", tweeny::easing::sinusoidalInOut));
mRows.push_back(makeRow("exponentialInOut",tweeny::easing::exponentialInOut));
mRows.push_back(makeRow("circularInOut",   tweeny::easing::circularInOut));
mRows.push_back(makeRow("bounceInOut",     tweeny::easing::bounceInOut));
mRows.push_back(makeRow("elasticInOut",    tweeny::easing::elasticInOut));
mRows.push_back(makeRow("backInOut",       tweeny::easing::backInOut));

// 11행을 [+0.9 .. -0.9] NDC 에 균등 분포 — i=0 위, i=10 아래.
for (int i = 0; i < kRowCount; ++i) {
    float t = (kRowCount == 1) ? 0.5f : static_cast<float>(i) / (kRowCount - 1);
    mRows[i].y = 0.9f + (-0.9f - 0.9f) * t; // lerp(0.9, -0.9, t)
}
```

- [ ] **Step 4.5: render() 를 11회 draw 로 확장 — tween 호출은 *peek 값* 만, step 없음**

`render(double)` 본문 교체:

```cpp
void render(double /*currentTime*/) override
{
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(mProgram->GetProgramAddr());
    glBindVertexArray(mQuad->GetVAO());

    for (auto& row : mRows) {
        float x = row.tween.peek();   // 정적 peek — Task 5 에서 step 으로 교체

        SJH::Uniforms::SetVec2(*mProgram, "uOffset",   vmath::vec2(x, row.y));
        SJH::Uniforms::SetVec2(*mProgram, "uScale",    vmath::vec2(0.05f, 0.035f));
        SJH::Uniforms::SetVec4(*mProgram, "baseColor", row.color);

        glDrawElements(mQuad->GetPrimitiveType(),
                       mQuad->GetIndexCount(),
                       GL_UNSIGNED_INT,
                       nullptr);
    }
}
```

- [ ] **Step 4.6: 빌드 + 실행**

```bash
cmake --build --preset ninja --target tweeny_demo
cd build_ninja/apps/tweeny_demo && ./tweeny_demo
```

Expected: 11개 plane 이 화면 *왼쪽 끝* (x=-0.85) 에 세로 일렬로 분포. 각 plane 색이 서로 다름 (RNG seed=42 결정성). 움직이지 않음 — ping-pong 은 Task 5.

- [ ] **Step 4.7: Commit**

```bash
git add <apps>/tweeny_demo/main.cpp
git commit -m "[dev] : tweeny_demo 11 EasingRow POD + 정적 렌더"
```

---

## Task 5: Ping-pong 로직 — tween.step + forward 토글

**목표:** 11개 plane 이 각자의 easing 곡선으로 좌↔우 무한 ping-pong.

**Files:**
- Modify: `<apps>/tweeny_demo/main.cpp` (render 메서드만)

- [ ] **Step 5.1: render() 에 delta-time + step + 방향 토글 추가**

`render(double currentTime)` 본문 교체 (시그니처는 그대로):

```cpp
void render(double currentTime) override
{
    // delta-time (ms) — sb7 의 currentTime 은 초 단위 double.
    static double prevTime = currentTime;
    float dtMs = static_cast<float>((currentTime - prevTime) * 1000.0);
    prevTime = currentTime;
    if (dtMs < 0.0f) dtMs = 0.0f; // 첫 프레임 안전.

    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(mProgram->GetProgramAddr());
    glBindVertexArray(mQuad->GetVAO());

    for (auto& row : mRows) {
        float x = row.forward ? row.tween.step(dtMs) : row.tween.step(-dtMs);

        // 방향 토글 — 양 끝 도달 시 다음 프레임부터 반대 방향.
        if      ( row.forward && row.tween.progress() >= 1.0f) row.forward = false;
        else if (!row.forward && row.tween.progress() <= 0.0f) row.forward = true;

        SJH::Uniforms::SetVec2(*mProgram, "uOffset",   vmath::vec2(x, row.y));
        SJH::Uniforms::SetVec2(*mProgram, "uScale",    vmath::vec2(0.05f, 0.035f));
        SJH::Uniforms::SetVec4(*mProgram, "baseColor", row.color);

        glDrawElements(mQuad->GetPrimitiveType(),
                       mQuad->GetIndexCount(),
                       GL_UNSIGNED_INT,
                       nullptr);
    }
}
```

> 주의: tweeny 의 `step(int)` 시그니처 — `step` 는 정수 milliseconds 를 받는 오버로드와 float duration ratio 오버로드 둘 다 있을 수 있다. 본 plan 은 float ms 오버로드를 가정. 컴파일 에러 시 `static_cast<int32_t>(dtMs)` 로 정수 캐스트 (tweeny `step(int)` 가 표준 형식).

- [ ] **Step 5.2: 빌드 + 실행**

```bash
cmake --build --preset ninja --target tweeny_demo
cd build_ninja/apps/tweeny_demo && ./tweeny_demo
```

Expected:
- 11행 plane 이 각자 다른 곡선으로 좌→우 (2초) → 우→좌 (2초) 무한 반복.
- `linear` 행이 등속 (눈으로 비교 기준).
- `bounce*` 행이 양 끝에서 통통 튐 (가장 눈에 띔).
- `elastic*` 행이 양 끝에서 잠시 *오버슛 + 진동*.
- `back*` 행이 시작/끝 직전 *살짝 뒤로 갔다가* 진입.
- `sinusoidal*` / `quadratic*` 가 가장 매끈.

- [ ] **Step 5.3: 컴파일 에러 시 fallback — `step(int)` 캐스트**

만약 `tween.step(dtMs)` 가 *ambiguous overload* 또는 *no matching function* 에러를 내면 다음으로 교체:

```cpp
int32_t dtMsInt = static_cast<int32_t>(dtMs);
float x = row.forward ? row.tween.step(dtMsInt) : row.tween.step(-dtMsInt);
```

- [ ] **Step 5.4: Commit**

```bash
git add <apps>/tweeny_demo/main.cpp
git commit -m "[dev] : tweeny_demo ping-pong (step + forward 토글)"
```

---

## Task 6: 시각 검증 + 마무리

**목표:** 명시적 검증 단계로 spec 의 4가지 검증 기준을 모두 통과하고 최종 commit.

- [ ] **Step 6.1: 빌드 무에러 (-Werror 통과)**

```bash
cmake --build --preset ninja --target tweeny_demo 2>&1 | grep -iE "error|warning" | head -20
```

Expected: 출력 없음 (또는 `In file included from ...` 같은 외부 컨텍스트만).

- [ ] **Step 6.2: 시각 검증 — 11개 곡선 구분**

```bash
cd build_ninja/apps/tweeny_demo && ./tweeny_demo
```

체크리스트:
- [ ] 11행 plane 이 화면에 세로로 균등 분포.
- [ ] 모든 plane 이 ping-pong 운동 중 (정지된 행 없음).
- [ ] linear 행이 등속.
- [ ] bounce/elastic 행이 비-매끄러운 거동을 보임.
- [ ] 색상 11개 모두 서로 구분 가능.

- [ ] **Step 6.3: 결정성 — 동일 색상 재현**

종료 후 다시 실행 — RNG seed=42 고정으로 11색이 정확히 동일해야 함.

```bash
cd build_ninja/apps/tweeny_demo && ./tweeny_demo
# (5초 관찰 후 종료)
./tweeny_demo
# (다시 5초 관찰 — 11색 동일 확인)
```

- [ ] **Step 6.4: 정상 종료 — GL 에러 없음**

데모 실행 중 콘솔에 `GL_INVALID_*` 류 메시지가 출력되지 않는지 확인. (`SJH::diagnostics` 가 활성이면 KHR_debug 콜백이 stderr 로 출력 — macOS 는 3.3 한도라 no-op, Linux/Windows 에서는 활성.)

- [ ] **Step 6.5: 최종 Commit (검증 완료 표식)**

이전 task 들에서 이미 push 했다면 본 step 은 옵션. 누락 파일이나 빌드 산출물 외 미커밋 파일 없는지 확인:

```bash
git status
```

Expected: `nothing to commit, working tree clean`.

---

## Self-Review

**1. Spec coverage 체크:**

| Spec 결정 사항 | 구현 Task |
|---|---|
| 챕터 위치 / 패턴 B / single main.cpp | Task 1.3 |
| 구조 — `project_deps + game_deps + SJH::engine` | Task 1.1 |
| Easing 세트 11개 InOut 대표 | Task 4.4 |
| Ping-pong / forward 토글 / `step(±dtMs)` | Task 5.1 |
| Tween 범위 [-0.85, 0.85] × 2000ms | Task 4.2 (constants) + 4.4 (factory) |
| 2D Ortho, NDC, y `lerp(0.9, -0.9, i/10)` | Task 4.4 (row.y 분포) + Task 2.1 (vs) |
| Plane 크기 `uScale=(0.05, 0.035)` | Task 5.1 (uniform) |
| 색상 plane 당 1회 RGB 랜덤, `mt19937{42}` | Task 4.4 |
| Geometry — `Mesh::CreateScreenQuad()` 공유 | Task 3.2 |
| Draw 전략 — VAO 1회 bind + 11회 uniform + 11회 draw | Task 5.1 |
| 셰이더 simple.vs (신규) + simple.fs (410 패치) | Task 2.1 / 2.2 |
| Uniforms `uOffset / uScale / baseColor` | Task 2.1 (vs decl) + Task 5.1 (set) |
| GL 4.1 Core / `info.majorVersion=4, minorVersion=1` | Task 1.3 |
| `apps/CMakeLists.txt` 등록 | Task 1.2 |
| YAGNI 컷 (ImGui/Camera/Actor/PostFX 없음) | 모든 task 에서 미포함 (의도된 비포함) |

모든 spec 요구사항이 task 에 mapping 됨. 누락 없음.

**2. Placeholder scan:** 본 plan 에 "TBD", "TODO", "implement later" 없음. 모든 step 이 실제 코드 또는 명시적 명령을 포함.

**3. Type consistency 체크:**
- `EasingRow` 멤버 — `name / tween / color / y / forward` — Task 4.2 정의, Task 4.4 생성, Task 5.1 read. 일관.
- `mQuad / mProgram / mRows` — Task 3.2 / 4.3 정의, Task 3 / 4 / 5 일관 사용.
- `SJH::Uniforms::SetVec2 / SetVec4` — Task 3.3 / 4.5 / 5.1 동일 시그니처 (`const Program&, const char*, const vmath::vec*&`).
- `constexpr int kRowCount = 11` 와 11개의 `push_back` — Task 4.4 일치.

**4. PreMortem 추적:**
- `PlaneGeometry` 사용 가능성 → 실제로는 `Mesh::CreateScreenQuad()` 사용 (Task 3.2). spec 의 PreMortem #1 가정 해소.
- `SJH::Program::SetUniform(vec2)` 시그니처 미확인 → 실제로는 `Uniforms::SetVec2` 자유 함수 (Task 3.3). spec 의 PreMortem #2 가정 해소.
- tweeny `via` API → `tweeny::easing::cubicInOut` 등 namespace 접근 (Task 4.4). PreMortem #3 가정 일치.
- `step(int)` 오버로드 — Task 5.3 에서 fallback 안내 추가.
