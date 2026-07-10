# ImGui 통합 — Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ocornut/imgui master 브랜치를 `extern/imgui` 서브모듈로 추가하고, GLFW + OpenGL3 backend 와 함께 `SJH::imgui` STATIC 라이브러리로 인라인 컴파일하여 `project_deps` 에 합류시킨다. 신규 `apps/imguitest/` 챕터로 macOS + Windows MSVC 양쪽에서 smoke test 통과.

**Architecture:** `extern/imgui/` 의 소스 7개(코어 5 + backend 2)를 `src/imgui/CMakeLists.txt` 에서 `target_sources` 로 직접 컴파일하여 `SJH::imgui` 정적 라이브러리 생성. `cmake/Dependency.cmake` 의 `project_deps` INTERFACE 타겟에 `src/imgui/CMakeLists.txt` 끝에서 `target_link_libraries(... INTERFACE SJH::imgui)` 로 사후 합류. ImGui 의 GLSL 셰이더는 `"#version 410 core"` 사용 — `imguitest` 가 `init()` override 로 GL 4.1 Core 컨텍스트를 강제.

**Tech Stack:** ocornut/imgui (master 브랜치), CMake (Ninja/MSVC), GLFW3, OpenGL 4.1 Core (macOS 한계 + wine 호환), sb7 의 gl3w 로더.

**참고 스펙:** [doc/superpowers/specs/2026-05-20-imgui-integration-design.md](../specs/2026-05-20-imgui-integration-design.md)

**테스트 정책:** 이 프로젝트는 단위 테스트 프레임워크가 없다 (CLAUDE.md). 각 태스크의 검증은 **빌드 통과 + smoke test 실행** 으로 한다. ImGui 의 정상 동작 확인은 `ImGui::ShowDemoWindow()` 가 표시되고 마우스 입력에 반응하는지 시각적으로 확인한다.

---

### Task 1: `extern/imgui` 서브모듈 추가 + 안정 태그 핀

**Files:**
- Modify: `.gitmodules`
- Create: `extern/imgui/` (git submodule)

- [ ] **Step 1: 현재 작업 디렉토리에서 ImGui 최신 안정 태그 조회**

Run:
```bash
git ls-remote --tags https://github.com/ocornut/imgui.git | grep -oE 'refs/tags/v[0-9]+\.[0-9]+(\.[0-9]+)?$' | sort -V | tail -5
```

Expected: 최신 5개 태그가 표시됨 (예: `v1.91.5`, `v1.91.6`, `v1.91.7`, `v1.91.8`, `v1.91.9`). **마지막 줄의 태그를 `<IMGUI_TAG>` 로 사용한다.** 이후 step 에서 이 값을 그대로 사용.

- [ ] **Step 2: `.gitmodules` 에 imgui 항목 추가**

기존 `.gitmodules` 끝에 다음 블록을 append (수동 편집 — 다른 항목 보존):

```
[submodule "extern/imgui"]
	path = extern/imgui
	url = https://github.com/ocornut/imgui.git
	branch = master
```

- [ ] **Step 3: 서브모듈 추가 + 안정 태그 체크아웃**

Run (Step 1 에서 확인한 `<IMGUI_TAG>` 로 치환):
```bash
git submodule add -b master https://github.com/ocornut/imgui.git extern/imgui
cd extern/imgui
git fetch --tags
git checkout <IMGUI_TAG>
cd ../..
```

Expected: `extern/imgui/` 디렉토리에 imgui 소스가 받아지고, 지정한 태그의 커밋에 detached HEAD 로 위치.

- [ ] **Step 4: imgui 핵심 파일 존재 확인**

Run:
```bash
ls <extern>/imgui/imgui.cpp <extern>/imgui/imgui_draw.cpp <extern>/imgui/imgui_tables.cpp <extern>/imgui/imgui_widgets.cpp <extern>/imgui/imgui_demo.cpp <extern>/imgui/backends/imgui_impl_glfw.cpp <extern>/imgui/backends/imgui_impl_opengl3.cpp
```

Expected: 7개 파일 모두 존재 — `ls: cannot access` 에러가 하나도 없어야 함.

- [ ] **Step 5: 커밋**

```bash
git add .gitmodules extern/imgui
git commit -m "$(cat <<'EOF'
[deps] : ImGui 서브모듈 추가 (extern/imgui, master @ <IMGUI_TAG>)

ocornut/imgui 의 안정 태그 <IMGUI_TAG> 를 extern/imgui 에 핀.
이후 src/imgui/ 에서 핵심 5 + backend 2 파일을 직접 컴파일한다.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

`<IMGUI_TAG>` 는 실제 태그(예: `v1.91.9`)로 치환.

---

### Task 2: `SJH::imgui` STATIC 라이브러리 빌드 (project_deps 합류 전)

`src/diagnostics/` 와 동일한 모듈 패턴으로 신규 모듈을 만든다. 이 태스크에서는 **라이브러리 자체의 빌드만 검증** — 아직 `project_deps` 합류는 하지 않는다. 합류는 Task 3 에서 분리.

**Files:**
- Create: `src/imgui/CMakeLists.txt`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: `src/imgui/CMakeLists.txt` 작성**

```cmake
# SJH::imgui — ocornut/imgui 의 소스를 인라인 컴파일하는 STATIC 라이브러리.
# extern/imgui 에서 직접 컴파일하므로 별도 include/lib 산출물 없음 (의도적).

set(IMGUI_ROOT ${CMAKE_SOURCE_DIR}/extern/imgui)

if(NOT EXISTS ${IMGUI_ROOT}/imgui.cpp)
    message(FATAL_ERROR
        "extern/imgui/ 가 비어 있습니다. 'git submodule update --init --recursive' 를 실행하세요.")
endif()

add_library(SJH_imgui STATIC
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_tables.cpp
    ${IMGUI_ROOT}/imgui_widgets.cpp
    ${IMGUI_ROOT}/imgui_demo.cpp
    ${IMGUI_ROOT}/backends/imgui_impl_glfw.cpp
    ${IMGUI_ROOT}/backends/imgui_impl_opengl3.cpp)
add_library(SJH::imgui ALIAS SJH_imgui)

# SYSTEM include — 소비자의 Debug -Werror 영향 차단
target_include_directories(SJH_imgui SYSTEM PUBLIC
    ${IMGUI_ROOT}
    ${IMGUI_ROOT}/backends)

# Backend 가 glfw/GL 심볼을 직접 호출 — PUBLIC 으로 소비자에게도 전파
target_link_libraries(SJH_imgui PUBLIC glfw3 ${OPENGL_LIBRARIES})

# sb7 의 gl3w 를 재사용 — ImGui 의 내장 GL 로더 비활성화
target_compile_definitions(SJH_imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# ImGui 자체 컴파일 단위만 경고 침묵 — 소비자 -Werror 는 영향 없음
if(MSVC)
    target_compile_options(SJH_imgui PRIVATE /W0 /utf-8)
else()
    target_compile_options(SJH_imgui PRIVATE -w)
endif()
```

- [ ] **Step 2: `src/CMakeLists.txt` 에 `add_subdirectory(imgui)` 추가**

기존 파일 끝에 줄 추가:

```cmake
add_subdirectory(imgui)
```

(다른 `add_subdirectory(...)` 줄과 같은 위치, 알파벳 정렬 무관)

- [ ] **Step 3: 깨끗한 configure**

Run:
```bash
rm -rf build_ninja
cmake --preset ninja
```

Expected: `-- Configuring done`, `-- Generating done`, `-- Build files have been written to: .../build_ninja` — `FATAL_ERROR` 없이 통과. (extern/imgui 가 비어 있으면 Step 1 의 가드가 발동.)

- [ ] **Step 4: SJH_imgui 라이브러리만 빌드해서 컴파일 검증**

Run:
```bash
cmake --build --preset ninja --target SJH_imgui
```

Expected: 7개 imgui 소스 컴파일 후 `libSJH_imgui.a` 가 생성됨. 경고 침묵 정책(`-w`) 으로 ImGui 내부 경고 없음. macOS clang 의 `-Werror` 도 SYSTEM include 와 `-w` PRIVATE 조합으로 무영향.

확인:
```bash
ls build_ninja/src/imgui/libSJH_imgui.a
```

Expected: 파일이 존재하고 크기 > 0.

- [ ] **Step 5: 커밋**

```bash
git add src/imgui/CMakeLists.txt src/CMakeLists.txt
git commit -m "$(cat <<'EOF'
[build] : SJH::imgui STATIC 라이브러리 추가 (project_deps 합류 전)

src/imgui/CMakeLists.txt 에서 extern/imgui 의 핵심 5 + GLFW/OpenGL3 backend 2
파일을 인라인 컴파일. SYSTEM include + 자체 -w/W0 로 소비자 -Werror 격리.
IMGUI_IMPL_OPENGL_LOADER_CUSTOM 으로 sb7 의 gl3w 재사용.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: `SJH::imgui` 를 `project_deps` 에 합류

`INTERFACE` 타겟은 사후에도 `target_link_libraries` 로 의존성을 추가할 수 있다. `src/imgui/CMakeLists.txt` 끝에서 `project_deps` 에 `SJH::imgui` 를 합류시켜 모든 활성 챕터에 자동 노출.

**Files:**
- Modify: `src/imgui/CMakeLists.txt` (끝줄 추가)
- Modify: `cmake/Dependency.cmake` (주석만, 선택)

- [ ] **Step 1: `src/imgui/CMakeLists.txt` 끝에 `project_deps` 합류 줄 추가**

기존 파일의 마지막 줄(`endif()` for MSVC compile options) 다음에 append:

```cmake

# project_deps 에 합류 — INTERFACE 타겟이라 사후 추가 가능.
# 이로써 project_deps 를 링크하는 모든 챕터가 ImGui 를 자동으로 끌어옴.
target_link_libraries(project_deps INTERFACE SJH::imgui)
```

- [ ] **Step 2: `cmake/Dependency.cmake` 끝에 참조 주석 추가**

기존 파일의 마지막 줄(`target_include_directories(game_deps SYSTEM INTERFACE ...)`) 다음에 append:

```cmake

# ImGui: src/imgui/CMakeLists.txt 가 SJH::imgui STATIC 라이브러리 정의 후
#        project_deps 에 합류시킨다 (인라인 컴파일 — extern/imgui 직접 참조).
```

- [ ] **Step 3: 기존 챕터(`_MyApp_`, `_deptest_`) 가 여전히 빌드되는지 확인**

활성 챕터는 `apps/CMakeLists.txt` 의 `add_subdirectory(_MyApp_)` 한 줄. 재구성 없이 증분 빌드.

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected: `_MyApp_` 가 ImGui 심볼을 사용하지 않더라도 링크는 정상 통과. (ImGui 라이브러리는 정적이라 사용 안 하는 심볼은 dead-strip 됨.)

확인:
```bash
ls build_ninja/apps/_MyApp_/_MyApp_
```

Expected: 실행 파일이 존재.

- [ ] **Step 4: 커밋**

```bash
git add src/imgui/CMakeLists.txt cmake/Dependency.cmake
git commit -m "$(cat <<'EOF'
[build] : SJH::imgui 를 project_deps 에 합류

src/imgui/CMakeLists.txt 끝에서 target_link_libraries(project_deps INTERFACE
SJH::imgui) 호출. INTERFACE 타겟의 사후 의존성 추가 성질 활용.
cmake/Dependency.cmake 에는 위치 참조 주석만 추가.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: `apps/imguitest/` smoke test 챕터 작성

패턴 B(`main.cpp` 단일 파일). sb7 base 의 macOS 기본 GL 컨텍스트는 3.2 이지만 본 프로젝트 정책은 GL 4.1 Core 이므로 `init()` 을 override 하여 4.1 강제.

**Files:**
- Create: `apps/imguitest/CMakeLists.txt`
- Create: `<apps>/imguitest/main.cpp`
- Modify: `apps/CMakeLists.txt`

- [ ] **Step 1: `apps/imguitest/CMakeLists.txt` 작성**

기존 `apps/_MyApp_/CMakeLists.txt` 와 동일 패턴(콘솔 서브시스템 유지, game_deps 미링크):

```cmake
# 챕터 타겟 이름을 현재 CMakeLists.txt source 디렉토리 위치로 사용할 것
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# 실행파일 — ImGui Demo 출력 확인을 위해 콘솔 서브시스템 유지
add_executable(${CHAPTER_NAME} main.cpp)

# ImGui 는 project_deps 에 이미 합류 — game_deps 는 불필요
target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps)

# 인클루드 디렉토리 추가
target_include_directories(${CHAPTER_NAME} PRIVATE ${CMAKE_CURRENT_DIR})

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
    # WinMain 진입점 유지 + 콘솔 창도 같이 띄우기 (진단용)
    target_link_options(${CHAPTER_NAME} PRIVATE
        "/SUBSYSTEM:CONSOLE"
        "/ENTRY:WinMainCRTStartup")
endif()
```

- [ ] **Step 2: `<apps>/imguitest/main.cpp` 작성**

```cpp
// ImGui 통합 smoke test — ImGui::ShowDemoWindow() 가 정상 표시되는지 확인.
// 프로젝트 정책상 GL 4.1 Core / GLSL 410 강제 (macOS 한계 + wine 호환).
#include <sb7.h>

#include <imgui.h>
#include <<backends>/imgui_impl_glfw.h>
#include <<backends>/imgui_impl_opengl3.h>

#include <cstdio>

class imguitest_application : public sb7::application
{
    void init() override
    {
        sb7::application::init();
        // 프로젝트 정책 — GL 4.1 Core / GLSL 410 강제.
        // sb7 base 의 macOS 기본값(3.2)을 4.1 로 강제 (Forward Compat + Core 는 base 가 이미 설정).
        info.majorVersion = 4;
        info.minorVersion = 1;
        std::snprintf(info.title, sizeof(info.title), "ImGui Integration Smoke Test");
    }

    void startup() override
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        // sb7::application 의 protected 멤버 GLFWwindow* window 를 그대로 사용.
        // install_callbacks=true 로 ImGui 가 GLFW 콜백을 가로채게 함 — sb7 의 키/마우스
        //   콜백은 imguitest 자체에서 사용하지 않으므로 충돌 없음.
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 410 core");
    }

    void render(double /*currentTime*/) override
    {
        static const GLfloat clearColor[] = {0.15f, 0.15f, 0.18f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, clearColor);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Demo window — ImGui 위젯/입력 통합 검증
        ImGui::ShowDemoWindow();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void shutdown() override
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};

DECLARE_MAIN(imguitest_application);
```

- [ ] **Step 3: `apps/CMakeLists.txt` 에서 활성 챕터 교체**

기존 파일을 다음으로 수정:

```cmake
# ______ 챕터별 실행 파일 등록 ______
# 한 번에 하나만 활성화한다. 새 게임/챕터 타겟을 추가하면 이 아래에 add_subdirectory 를 적고 주석 해제.

# _deptest_ — 게임/엔진 의존성(game_deps) 링크 검증용 임시 타겟.
# 의존성 점검이 필요할 때만 주석 해제: cmake --build --preset ninja --target _deptest_
# add_subdirectory(_deptest_)
# add_subdirectory(_MyApp_)

# imguitest — ImGui 통합 smoke test (SP3)
add_subdirectory(imguitest)
```

- [ ] **Step 4: Debug 빌드**

Run:
```bash
cmake --build --preset ninja --target imguitest
```

Expected: 컴파일 0 에러, 링크 통과. `imguitest` 실행 파일이 `build_ninja/apps/imguitest/` 에 생성.

확인:
```bash
ls -la build_ninja/apps/imguitest/imguitest
```

Expected: 실행 파일 존재, 실행 권한 있음.

- [ ] **Step 5: smoke test 실행 — Demo window 시각 확인**

Run:
```bash
cd build_ninja/apps/imguitest && ./imguitest
```

Expected:
- 800x600 윈도우가 뜸 (타이틀: "ImGui Integration Smoke Test")
- 어두운 회색 배경 위에 ImGui Demo window 표시됨
- 마우스로 Demo window 의 widget(슬라이더, 버튼, 콤보박스 등)을 조작하면 정상 반응
- ESC 키로 종료 — 콘솔에 에러 출력 없음

**확인 후 작업 디렉토리로 복귀:**
```bash
cd ../../..
```

- [ ] **Step 6: 커밋**

```bash
git add apps/imguitest apps/CMakeLists.txt
git commit -m "$(cat <<'EOF'
[feat] : imguitest 챕터 추가 — ImGui smoke test (SP3)

apps/imguitest/ 신규 챕터. init() override 에서 GL 4.1 Core 강제,
startup() 에서 ImGui+GLFW+OpenGL3 backend 초기화, render() 에서
ImGui::ShowDemoWindow() 출력. macOS Ninja Debug 빌드/실행 검증 완료.

apps/CMakeLists.txt 활성 챕터를 _MyApp_ → imguitest 로 교체.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: macOS Release 빌드 검증

Debug 에서 검증한 동작이 Release(narrowing 경고 활성화 + 최적화) 에서도 깨지지 않는지 확인.

**Files:** (수정 없음 — 검증 전용)

- [ ] **Step 1: Release configure + 빌드**

Run:
```bash
rm -rf build_ninja-release
cmake --preset ninja-release
cmake --build --preset ninja-release --target imguitest
```

Expected: 0 에러. Release 빌드의 `-Wall -Werror` (CXXStandard.cmake) 가 ImGui 의 narrowing 경고를 잡지 않음 (SYSTEM include + `-w` PRIVATE).

- [ ] **Step 2: Release smoke test 실행**

Run:
```bash
cd build_ninja-release/apps/imguitest && ./imguitest
```

Expected: Debug 와 동일한 ImGui Demo window 표시 + 입력 반응. ESC 로 종료.

**확인 후 작업 디렉토리로 복귀:**
```bash
cd ../../..
```

- [ ] **Step 3: (커밋 없음 — 검증만)**

이 태스크는 코드 변경 없이 검증만 수행. `git status` 가 깨끗한지 확인:

Run:
```bash
git status --short
```

Expected: imgui 통합과 무관한 기존 작업 트리 변경사항만 표시 (`<doc>/superpowers/plans/2026-05-20-sp1-...md`, `src/common/...` 등). imgui 관련 새 변경 없음.

---

### Task 6: MSVC CI 매트릭스에 VS 2019 추가 + CI 검증

spec 의 검증 요구는 **VS 2019 + VS 2022, Debug + Release 총 4 조합**. 현재 `.github/workflows/build-msvc.yml` 의 매트릭스는 `msvc-2022` 만 → VS 2019 (`msvc` preset) 추가 필요.

**Files:**
- Modify: `.github/workflows/build-msvc.yml` (matrix 확장)

- [ ] **Step 1: 현재 매트릭스 라인 확인**

Run:
```bash
sed -n '14,22p' .github/workflows/build-msvc.yml
```

Expected: `matrix.include` 가 `msvc-2022` Debug/Release 2 항목만 포함하는 것을 확인.

- [ ] **Step 2: `build-msvc.yml` 매트릭스에 VS 2019 2 항목 추가**

`matrix.include` 블록을 다음으로 교체 (기존 2 → 4 항목):

```yaml
        include:
          - configuration: Debug
            build_preset: msvc-2022
            preset: msvc-2022
          - configuration: Release
            build_preset: msvc-2022-release
            preset: msvc-2022
          - configuration: Debug
            build_preset: msvc
            preset: msvc
          - configuration: Release
            build_preset: msvc-release
            preset: msvc
```

그리고 workflow 의 `Configure` 단계를 매트릭스의 `preset` 변수로 일반화:

```yaml
      - name: Configure
        run: cmake --preset ${{ matrix.preset }}
```

(기존 `cmake --preset msvc-2022` 하드코딩 제거.)

`build-extern-libs` 단계들은 `matrix.configuration == 'Release'` 가드만 있으므로 4 항목으로 늘어도 정확히 2번 (각 preset 의 Release 1번씩) 실행됨. extern 산출물은 VS 2019/VS 2022 가 ABI 호환이므로 매트릭스 간 중복 빌드는 시간 낭비 — 캐시 또는 단일 빌드 후 공유가 이상적이지만 본 plan 범위 밖. 일단 4번 빌드 허용.

> **검증 결정:** extern 라이브러리 빌드가 4 매트릭스 × Release 가드 = 2번 발생하므로 동시성 비용은 받아들임. 추후 별도 spec 으로 캐시/공유 최적화.

- [ ] **Step 3: 변경된 workflow 가 YAML 으로 유효한지 로컬 확인**

Run:
```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/build-msvc.yml'))" && echo "YAML OK"
```

Expected: `YAML OK` 출력. 파싱 에러 없음.

- [ ] **Step 4: 커밋 + push**

```bash
git add .github/workflows/build-msvc.yml
git commit -m "$(cat <<'EOF'
[ci] : build-msvc.yml 매트릭스에 VS 2019 추가 (SP3)

spec 의 VS2019/VS2022 × Debug/Release 4 조합 검증을 위해
msvc preset (VS 2019) 2 항목을 매트릭스에 추가. Configure 단계는
matrix.preset 변수로 일반화.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"

git push -u origin HEAD
```

Expected: push 성공. `Build (MSVC)` workflow 가 PR 또는 push 트리거로 자동 실행되거나, Step 5 에서 수동 트리거.

- [ ] **Step 5: workflow 수동 실행 (PR/push 트리거가 미발동인 경우)**

Run:
```bash
gh workflow run build-msvc.yml --ref $(git branch --show-current)
```

Expected: `✓ Created workflow_dispatch event` 출력.

- [ ] **Step 6: workflow 완료 대기 + 결과 확인**

Run:
```bash
gh run watch
```

Expected: 4 매트릭스 (VS 2019 Debug/Release + VS 2022 Debug/Release) 모두 `✓ success`. 약 25~40분 소요.

실패 시 — `gh run view --log-failed` 로 로그 확인. 흔한 실패 원인:
- `<extern>/imgui/imgui_demo.cpp` 의 cp949 디코딩 에러 → `SJH_imgui` PRIVATE 옵션에 `/utf-8` 가 적용되었는지 재확인 (Task 2 Step 1).
- `IMGUI_IMPL_OPENGL_LOADER_CUSTOM` 미전파 → ImGui 의 내장 로더가 `<<GL>/gl.h>` 와 sb7 gl3w 충돌. 매크로가 `target_compile_definitions(SJH_imgui PUBLIC ...)` 로 PUBLIC 인지 재확인 (Task 2 Step 1).
- `_d` 접미사 Debug 라이브러리 누락 → extern lib 빌드의 Release 가드 때문에 Debug 빌드가 Release artifact 만 받았을 가능성. matrix `configuration: Debug` 항목이 Release artifact 단계도 거치는지 점검.

- [ ] **Step 7: (커밋 없음 — Step 4 에서 push 완료)**

PR 이 열려 있었다면 머지 진행은 사용자 결정.

---

### Task 7: ARM64 호스트 x64 크로스 빌드 검증 (선택, 사용자 로컬)

spec 의 검증 기준에 ARM64 호스트(예: Apple Silicon UTM Windows VM, ARM64 dev machine) 에서 `cmake --preset msvc-2022 -A x64` 강제 빌드가 포함됨. CI 의 `windows-latest` 는 x64 호스트라 자동 검증 불가 — **ARM64 Windows 환경에 접근 가능한 사용자만 수동 검증**.

`doc/handoffs/2026-05-10-window11-env-handoff.md` 의 UTM Windows 11 ARM64 환경이 있다면 다음 step 수행.

**Files:** (수정 없음 — 검증 전용)

- [ ] **Step 1: ARM64 호스트 가용성 확인**

ARM64 Windows 환경이 **없으면 본 Task 전체를 건너뛴다.** (CI 의 x64 호스트 빌드로 spec 의 "MSVC 빌드 통과" 충분 조건 만족.)

ARM64 환경이 있으면 SMB 공유 등으로 프로젝트 디렉토리에 접근 후 다음 진행.

- [ ] **Step 2: ARM64 호스트에서 x64 크로스 configure**

ARM64 Windows 머신의 `Developer PowerShell for VS 2022` 에서:

```powershell
cd \\path\to\GlobalMedia-OpenGL-ComputerGraphics
cmake --preset msvc-2022 -A x64
```

Expected: configure 성공. `-- Configuring done`. `lib/windows/` 의 x64 사전 빌드 lib 와 정합.

- [ ] **Step 3: imguitest 빌드 + 실행**

```powershell
cmake --build --preset msvc-2022 --target imguitest
cd build_msvc\apps\imguitest\Debug
.\imguitest.exe
```

Expected: ImGui Demo window 표시. ARM64 호스트에서 x64 에뮬레이션으로 정상 실행.

- [ ] **Step 4: (커밋 없음 — 검증만)**

검증 결과를 별도 메모(`doc/superpowers/specs/2026-05-20-imgui-integration-design.md` 의 검증 기록 단락 또는 PR 코멘트) 에 남기는 것은 사용자 재량.

---

### Task 8: 회고 + CLAUDE.md 의존성 단락 한 줄 추가 (선택)

CLAUDE.md 의 의존성 섹션에 ImGui 항목을 한 줄 추가하여 미래의 cold-start 협업자가 즉시 파악할 수 있게 한다.

**Files:**
- Modify: `.claude/CLAUDE.md`

- [ ] **Step 1: CLAUDE.md 의 "Extern 라이브러리 부트스트랩" 단락 직전의 의존성 단락 찾기**

Run:
```bash
grep -n "extern/box2d\|extern/Effekseer" /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/.claude/CLAUDE.md
```

Expected: extern 라이브러리 리스트가 있는 단락의 라인 번호 확인.

- [ ] **Step 2: CLAUDE.md 에 imgui 한 줄 추가**

해당 단락의 라이브러리 나열 줄(예: `extern/box2d (v2.4.1) / extern/Effekseer (1.7.3.0) / ...`) 끝에 추가:

```
/ extern/imgui (<IMGUI_TAG>)
```

그리고 ImGui 의 통합 방식이 다른 라이브러리와 다른 점을 짧게 명시 — "ImGui 는 사전 빌드 lib 가 아닌 src/imgui/ 에서 인라인 컴파일하며 project_deps 에 자동 합류."

- [ ] **Step 3: 커밋**

```bash
git add .claude/CLAUDE.md
git commit -m "$(cat <<'EOF'
[docs] : CLAUDE.md 에 ImGui 의존성 한 줄 추가 (SP3)

ImGui 가 다른 라이브러리와 달리 사전 빌드 lib 없이 src/imgui/ 에서
인라인 컴파일되어 project_deps 에 자동 합류함을 명시.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## 완료 기준 체크리스트

플랜 전체가 끝나면 아래 항목 모두 만족:

- [ ] `extern/imgui` 가 안정 태그(`<IMGUI_TAG>`) 에 핀되어 체크아웃됨
- [ ] `cmake --build --preset ninja --target imguitest` (macOS Debug) 성공
- [ ] `./imguitest` 실행 시 ImGui Demo window 표시 + 마우스 입력 반응
- [ ] `cmake --build --preset ninja-release --target imguitest` (macOS Release) 성공
- [ ] GitHub Actions `Build (MSVC)` 4 매트릭스 (VS2019/VS2022 × Debug/Release) 모두 통과
- [ ] ARM64 호스트 환경 보유 시 x64 크로스 빌드 검증 통과 (Task 7, 환경 없으면 건너뜀)
- [ ] `cmake --build --preset ninja --target _MyApp_` 회귀 없음 (Task 3 의 Step 3 에서 확인)
- [ ] `apps/CMakeLists.txt` 의 활성 챕터가 `imguitest` 한 줄만 (한 번에 하나 규칙 준수)
- [ ] `lib/{macos,windows}` 와 `include/` 무변경, `BuildExternLibs.{sh,bat}` 무변경
- [ ] CLAUDE.md 에 ImGui 한 줄 추가됨 (Task 8)
