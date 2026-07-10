# ImGui 통합 — 설계 스펙

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 작성일: 2026-05-20

## 목표

ocornut/imgui **master 브랜치**를 `extern/imgui` 서브모듈로 추가하고, GLFW + OpenGL3 backend 와 함께 내부 STATIC 라이브러리 `SJH::imgui` 로 컴파일하여 **`project_deps` 에 직접 합류**시킨다. 신규 `apps/imguitest/` 챕터로 smoke test 까지 한 번에 수행한다. macOS(Ninja) + Windows(MSVC, VS2019/VS2022, Debug/Release, ARM64 호스트 x64 크로스) **모든 조합에서 빌드·실행 통과**가 완료 기준.

## 배경 / 제약

- 다른 라이브러리(sb7/glfw3/Box2D/Effekseer/assimp/spdlog) 는 **사전 빌드 정적 산출물을 `lib/{macos,windows}` 에 체크인**하는 패턴이지만, ImGui 는 컴파일 단위가 7개(코어 5 + backend 2) 로 작고 빈번한 컴파일 옵션 조정이 자연스러워 **인라인 컴파일** 이 ImGui 커뮤니티 표준이다.
- 교수 제출용 — vcpkg 미사용, CMake 단독 완결. 서브모듈이 초기화되지 않은 환경에서도 빌드되어야 한다는 기존 원칙은 **이번 결정에서 의도적으로 깬다** — ImGui 는 `extern/imgui/` 의 소스를 직접 컴파일하므로 서브모듈 초기화가 **필수**가 된다. (`lib/` 에 산출물을 두지 않는 의도적 예외.)
- 이 프로젝트의 GL/GLSL 정책은 **GL 4.1 Core / GLSL 410** (`exercise7_win`·`exercise8_win` 의 wine 호환 패턴과 일치). sb7 base `init()` 의 macOS 기본값은 GL 3.2 이지만, 챕터의 `init()` override 에서 `info.majorVersion=4; info.minorVersion=1;` 로 강제해 macOS(GL 4.1 까지 지원) + Windows(GL 4.3 base, 4.1 컨텍스트 정상) + wine 모두 `"#version 410 core"` 보장.
- 기존 `project_deps` 에 ImGui 가 합류하므로, 모든 활성화된 챕터(`chapter1~9`, `exerciseN`, `prevmidterm` 등)가 자동으로 ImGui 헤더/라이브러리를 끌어온다. 사용자가 이 점을 인지하고 명시적으로 선택했다.

## 결정 사항

| 항목 | 결정 |
|---|---|
| 브랜치 | **master** (안정 브랜치). docking/multi-viewport 미사용 |
| 통합 방식 | **헤더+소스 인라인 컴파일** — `extern/imgui` 에서 `target_sources` 로 직접 끌어옴 |
| Backend | `imgui_impl_glfw` + `imgui_impl_opengl3` |
| GLSL 버전 | `"#version 410 core"` (프로젝트 정책 — `imguitest` 가 `init()` 에서 GL 4.1 Core 강제) |
| 노출 방식 | **`project_deps` 에 직접 포함** (모든 챕터 자동 링크) |
| 헤더 노출 경로 | `extern/imgui/` 와 `extern/imgui/backends/` (SYSTEM include) — **`include/imgui/` 로 복사하지 않음** |
| GL 로더 | sb7 의 gl3w 재사용 (`IMGUI_IMPL_OPENGL_LOADER_CUSTOM` 정의) |
| 검증 챕터 | 신규 `apps/imguitest/` (smoke test) |
| `lib/` 산출물 체크인 | **없음** — 다른 라이브러리들과 의도적으로 다름 |
| `BuildExternLibs.{sh,bat}` 변경 | **없음** |
| imconfig.h | 미수정 (기본값) |

## 서브모듈 — `.gitmodules`

```
[submodule "extern/imgui"]
    path = extern/imgui
    url = https://github.com/ocornut/imgui.git
    branch = master
```

체크아웃 후 명시적 태그(예: `v1.91.6`) 로 커밋을 핀해 재현성 확보. 태그는 plan 단계에서 등록 시점의 최신 안정 태그로 확정.

## 컴파일 단위 — `src/imgui/`

`src/diagnostics/` 와 동일한 패턴으로 신규 모듈을 추가한다.

### `src/imgui/CMakeLists.txt` (요약)

```cmake
set(IMGUI_ROOT ${CMAKE_SOURCE_DIR}/extern/imgui)

add_library(SJH_imgui STATIC
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_tables.cpp
    ${IMGUI_ROOT}/imgui_widgets.cpp
    ${IMGUI_ROOT}/imgui_demo.cpp
    ${IMGUI_ROOT}/backends/imgui_impl_glfw.cpp
    ${IMGUI_ROOT}/backends/imgui_impl_opengl3.cpp)
add_library(SJH::imgui ALIAS SJH_imgui)

# SYSTEM include — Debug -Werror 영향 차단
target_include_directories(SJH_imgui SYSTEM PUBLIC
    ${IMGUI_ROOT}
    ${IMGUI_ROOT}/backends)

# GLFW + GL 심볼 (backend 가 직접 호출)
target_link_libraries(SJH_imgui PUBLIC glfw3 ${OPENGL_LIBRARIES})

# sb7 의 gl3w 재사용 — ImGui 의 내장 로더 비활성화
target_compile_definitions(SJH_imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# ImGui 자체 컴파일 경고 침묵 (소비자 -Werror 는 영향 없음)
if(MSVC)
    target_compile_options(SJH_imgui PRIVATE /W0 /utf-8)
else()
    target_compile_options(SJH_imgui PRIVATE -w)
endif()

# project_deps 에 합류 — INTERFACE 타겟이라 사후 추가 가능
target_link_libraries(project_deps INTERFACE SJH::imgui)
```

### `src/CMakeLists.txt` 변경

```cmake
add_subdirectory(imgui)   # 신규 라인 추가 (위치: diagnostics 옆 어디든 무방)
```

### 디자인 메모: `project_deps` 합류를 `src/imgui/` 가 책임지는 이유

루트 `CMakeLists.txt` 평가 순서는:
1. `cmake/Dependency.cmake` (project_deps INTERFACE 타겟 정의)
2. `src/` (SJH::imgui 정의)
3. `apps/` (챕터들이 project_deps 링크)

`SJH::imgui` 타겟은 `src/imgui/CMakeLists.txt` 가 평가될 때 비로소 생기므로, `cmake/Dependency.cmake` 에서는 아직 참조 불가. CMake 의 `INTERFACE` 타겟은 **나중에도 `target_link_libraries(...)` 로 의존성을 추가할 수 있다**는 성질을 이용해, `src/imgui/CMakeLists.txt` 의 끝에서 `target_link_libraries(project_deps INTERFACE SJH::imgui)` 한 줄로 합류시킨다. 이 결과 `apps/` 단계에서 project_deps 를 링크하는 모든 챕터가 ImGui 를 자동으로 끌어온다.

## `cmake/Dependency.cmake` 변경

**없음.** ImGui 의 project_deps 합류는 `src/imgui/CMakeLists.txt` 가 책임진다. (다른 라이브러리들이 `Dependency.cmake` 에 모여 있는 패턴과의 일관성은 인라인 컴파일이라는 예외 특성 때문에 일부 양보.)

선택 — 가독성을 위해 `cmake/Dependency.cmake` 끝에 다음과 같은 주석만 추가:

```cmake
# ImGui: src/imgui/CMakeLists.txt 가 SJH::imgui STATIC 라이브러리 정의 후
#        project_deps 에 합류시킨다 (인라인 컴파일 — extern/imgui 직접 참조).
```

## smoke test — `apps/imguitest/`

패턴 B (`main.cpp` 단일 파일).

### `<apps>/imguitest/main.cpp` 구조

```cpp
#include "sb7.h"
#include "imgui.h"
#include "<backends>/imgui_impl_glfw.h"
#include "<backends>/imgui_impl_opengl3.h"

class my_application : public sb7::application
{
    void init() override
    {
        sb7::application::init();
        // 프로젝트 정책 — GL 4.1 Core / GLSL 410 강제 (macOS 4.1 한계 + wine 호환)
        info.majorVersion = 4;
        info.minorVersion = 1;
    }

    void startup() override
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        // sb7::application 의 protected 멤버 GLFWwindow* window 를 그대로 사용
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 410 core");
    }

    void render(double /*currentTime*/) override
    {
        glClearBufferfv(GL_COLOR, 0, /* 회색 */);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
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
DECLARE_MAIN(my_application);
```

`sb7::application` 은 `GLFWwindow* window;` 를 protected 멤버로 보유 (`include/sb7.h:293`) — 상속한 `my_application` 에서 추가 접근자 없이 그대로 사용 가능. `glfw::init()` / 컨텍스트 생성은 sb7 의 `run()` 흐름이 `startup()` 호출 이전에 이미 완료한다.

### `apps/imguitest/CMakeLists.txt`

기존 chapter4 등 패턴 B 의 CMakeLists 를 그대로 복사. 링크는 `target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps)` 만으로 충분 (project_deps 가 ImGui 를 이미 포함).

### `apps/CMakeLists.txt` 활성화

기존 활성 줄(`add_subdirectory(_MyApp_)`) 을 주석 처리하고 `add_subdirectory(imguitest)` 추가. "한 번에 하나만 활성화" 규칙 준수.

## MSVC 호환 — 강조 검증 항목

이번 spec 의 **완료 기준**이 MSVC 호환에 있으므로 plan 단계에서 명시적으로 검증한다.

1. **컴파일 옵션**: `SJH_imgui` 에 `/utf-8` 적용 — `imgui_demo.cpp` 의 한글/일본어/중국어 데모 UTF-8 리터럴이 cp949 환경에서도 컴파일 통과.
2. **경고 침묵 격리**: `/W0` 는 ImGui 컴파일 단위에만 적용 (`PRIVATE`). 소비자 챕터의 `-Werror`/Release 경고 활성화는 영향 없음.
3. **Debug 매크로 호환**: 루트 `CXXStandard.cmake` 가 MSVC Debug 에 적용하는 narrowing 침묵(`/wd4244 /wd4305 /wd4267`) 은 ImGui 쪽에서 추가 작업 불요.
4. **빌드 매트릭스 검증 대상**:
   - `cmake --preset msvc` + Debug/Release
   - `cmake --preset msvc-2022` + Debug/Release
   - ARM64 호스트 → `cmake --preset msvc-2022 -A x64` (사전 빌드 lib 가 x64)
   - macOS Ninja Debug/Release
5. **CI**: 기존 `.github/workflows/build-msvc.yml` 에 `imguitest` 타겟 추가. `build-extern-libs.yml` 은 미변경(ImGui 산출물 미배포).
6. **gl3w 충돌 차단**: `IMGUI_IMPL_OPENGL_LOADER_CUSTOM` 매크로가 PUBLIC 으로 전파 → backend 컴파일 시 ImGui 의 내장 로더 코드가 비활성. 소비자에서는 ImGui 헤더보다 먼저 `sb7.h` (gl3w 포함) 가 들어오는 순서를 권장 (실제 충돌 시 plan 단계에서 forced include 검토).

## 검증 기준 (Done means)

1. `git submodule update --init --recursive` 로 `extern/imgui/` 체크아웃됨.
2. macOS Ninja Debug/Release 둘 다:
   - `cmake --build --preset ninja --target imguitest` 성공
   - `cd build_ninja/apps/imguitest && ./imguitest` 실행 시 ImGui demo window 표시, 마우스/키보드 입력 반응
3. Windows MSVC VS2019/VS2022 Debug/Release 4 조합 모두 `imguitest` 빌드 성공.
4. 무작위 선택한 기존 챕터 1개(예: `_MyApp_`) 도 동일 환경에서 빌드 성공 — project_deps 에 ImGui 가 합류했어도 회귀 없음.
5. CI 의 `build-msvc.yml` 매트릭스가 `imguitest` 를 포함해 통과.

## 영향 범위

- **추가**: `extern/imgui/`, `.gitmodules`, `src/imgui/CMakeLists.txt`, `apps/imguitest/{main.cpp,CMakeLists.txt}`, (선택) `CLAUDE.md` 의 의존성 단락 한 줄.
- **수정**: `src/CMakeLists.txt` (`add_subdirectory(imgui)` 추가), `apps/CMakeLists.txt` (활성 타겟 교체), 선택 — `cmake/Dependency.cmake` 주석 추가, `.github/workflows/build-msvc.yml` (imguitest 타겟 추가).
- **무변경**: `lib/{macos,windows}/`, `include/`, `shell/BuildExternLibs.{sh,bat}`, `cmake/CXXStandard.cmake`, 기존 챕터 소스.

## YAGNI — 이번 spec 에서 의도적으로 제외

- freetype 기반 폰트 렌더링 (기본 ProggyClean 사용)
- 한글/CJK 폰트 atlas 추가
- ImGui ini 저장 경로 커스터마이즈 (기본 `imgui.ini` 사용)
- Multi-viewport (docking 브랜치 기능, 본 spec 은 master 브랜치라 해당 없음)
- ImPlot / ImGuizmo / ImGuiFileDialog 등 부가 라이브러리 — 후속 spec 으로 분리
- 기존 챕터의 ImGui 기반 디버그 UI 도입 — 후속 작업

## 후속 작업 (참고)

- spec 의 후속편: ImPlot/ImGuizmo 등 ImGui 생태계 라이브러리 통합
- 챕터별 디버그 UI 시리즈: chapter9 의 uniform 슬라이더, chapter8 의 라이팅 파라미터 패널 등
- 한글 폰트 atlas 통합 (freetype + NotoSansKR 등)
