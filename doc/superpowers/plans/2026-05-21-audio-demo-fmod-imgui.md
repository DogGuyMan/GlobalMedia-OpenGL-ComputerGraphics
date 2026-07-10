# audio_demo — FMOD Studio + ImGui 파라미터 데모 — Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 신규 챕터 `apps/audio_demo/` 가 FMOD Studio 의 `Music.bank` 를 로드하고 그 안 이벤트(`event:/Music/Level 01`) 파라미터(`Intensity`, `Progression`) 를 ImGui 슬라이더/콤보로 변경하면 청각적으로 즉시 반영된다.

**Architecture:** sb7::application 상속 single `main.cpp` 패턴 — FMOD Studio System + 캐시된 EventInstance + ImGui v1.53 결합 backend. 파라미터는 `getParameterDescriptionByIndex` 로 메타데이터 런타임 발견 → 타입별 위젯 자동 매핑.

**Tech Stack:** FMOD Studio API 2.03 (Core + Studio), ImGui v1.53 (`imgui_impl_glfw_gl3`), sb7 base, GLFW 3.0.4, OpenGL 4.1 Core / GLSL 410, CMake (Ninja preset).

**Reference spec:** [doc/superpowers/specs/2026-05-21-audio-demo-fmod-imgui-design.md](../specs/2026-05-21-audio-demo-fmod-imgui-design.md)

---

## File Structure

**Create:**
- `apps/audio_demo/CMakeLists.txt`
- `<apps>/audio_demo/main.cpp`
- `apps/audio_demo/resources/banks/Master.bank` (copy)
- `apps/audio_demo/resources/banks/Master.strings.bank` (copy)
- `apps/audio_demo/resources/banks/Music.bank` (copy)

**Modify:**
- `apps/CMakeLists.txt` — add `add_subdirectory(audio_demo)` 라인
- `.gitignore` — `apps/*/resources/banks/` 패턴 추가

**Reference (read-only):**
- `<apps>/imguitest/main.cpp` — ImGui v1.53 init/shutdown 패턴
- `apps/imguitest/CMakeLists.txt` — chapter CMakeLists 패턴
- `doc/FMOD_Setup.md` §6 — POST_BUILD copy 패턴
- `cmake/Dependency.cmake` — fmod/fmodstudio 타겟 등록 위치

---

## Task 1: 챕터 스캐폴딩 — 최소 빌드 통과

빈 sb7::application 으로 디렉토리 + CMakeLists + apps 활성화 후 빌드까지 한 사이클을 먼저 닫는다. FMOD/ImGui 통합은 이후 단계에서.

**Files:**
- Create: `apps/audio_demo/CMakeLists.txt`
- Create: `<apps>/audio_demo/main.cpp`
- Modify: `apps/CMakeLists.txt`

- [ ] **Step 1: CMakeLists.txt 작성**

```cmake
# audio_demo — FMOD Studio + ImGui 파라미터 데모
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_executable(${CHAPTER_NAME} main.cpp)

# project_deps (sb7 + GLFW + GL + SJH::imgui) + game_deps (fmod + fmodstudio 등)
target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps game_deps)

target_include_directories(${CHAPTER_NAME} PRIVATE ${CMAKE_CURRENT_DIR})

# 챕터 리소스(`resources/banks/*.bank`) POST_BUILD copy
add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources)

# FMOD 동적 라이브러리 — 실행 파일 옆으로 (doc/FMOD_Setup.md §6)
if(TARGET fmod)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmod> $<TARGET_FILE_DIR:${CHAPTER_NAME}>)
endif()
if(TARGET fmodstudio)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmodstudio> $<TARGET_FILE_DIR:${CHAPTER_NAME}>)
endif()

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
    target_link_options(${CHAPTER_NAME} PRIVATE
        "/SUBSYSTEM:CONSOLE"
        "/ENTRY:WinMainCRTStartup")
endif()
```

- [ ] **Step 2: main.cpp 최소 스텁**

```cpp
// audio_demo — FMOD Studio + ImGui 파라미터 데모 (single-file chapter).
// Task 1: 빈 sb7::application 으로 빌드 파이프라인 검증.
#include <sb7.h>

class audio_demo_application : public sb7::application
{
    void init() override
    {
        sb7::application::init();
        info.majorVersion = 4;
        info.minorVersion = 1;
        std::snprintf(info.title, sizeof(info.title), "FMOD Studio + ImGui Audio Demo");
    }

    void render(double /*currentTime*/) override
    {
        static const GLfloat clearColor[] = {0.10f, 0.10f, 0.12f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, clearColor);
    }
};

DECLARE_MAIN(audio_demo_application);
```

- [ ] **Step 3: apps/CMakeLists.txt 활성화**

`apps/CMakeLists.txt` 끝부분에 한 줄 추가:

```cmake
# audio_demo — FMOD Studio + ImGui 파라미터 데모 (신규)
add_subdirectory(audio_demo)
```

- [ ] **Step 4: configure + 빌드**

```bash
cmake --preset ninja
cmake --build --preset ninja --target audio_demo
```

Expected: configure done, build PASS (`Linking CXX executable apps/audio_demo/audio_demo`).

- [ ] **Step 5: 빈 윈도우 실행 확인**

```bash
cd build_ninja/apps/audio_demo && ./audio_demo
```

Expected: 회색 창 하나 뜸. ESC 또는 창 닫기로 종료.

- [ ] **Step 6: commit**

```bash
git add apps/audio_demo/ apps/CMakeLists.txt
git commit -m "[feat] : audio_demo 챕터 스캐폴딩 (Task 1)"
```

---

## Task 2: .bank 파일 배치 + .gitignore

데모 bank 3개를 챕터 resources 로 복사하고, FMOD 라이선스 보호용 `.gitignore` 패턴 추가.

**Files:**
- Create (copy): `apps/audio_demo/resources/banks/Master.bank`
- Create (copy): `apps/audio_demo/resources/banks/Master.strings.bank`
- Create (copy): `apps/audio_demo/resources/banks/Music.bank`
- Modify: `.gitignore`

- [ ] **Step 1: banks/ 디렉토리 생성 + 파일 복사**

```bash
mkdir -p apps/audio_demo/resources/banks
SDK="resources/installer/macos/FMOD Programmers API/api/studio/examples/media"
cp "$SDK/Master.bank" "$SDK/Master.strings.bank" "$SDK/Music.bank" \
   apps/audio_demo/resources/banks/
```

- [ ] **Step 2: 복사 결과 확인**

```bash
ls -la apps/audio_demo/resources/banks/
```

Expected: 3개 파일 (Master.bank, Master.strings.bank, Music.bank) 표시.

- [ ] **Step 3: .gitignore 패턴 추가**

`.gitignore` 의 FMOD 관련 줄(`include/fmod/` 근처) 다음에 한 줄 추가:

```
# FMOD 샘플 bank — 라이선스상 재배포 제약 (doc/FMOD_Setup.md §7 와 동일 사유)
apps/*/resources/banks/
```

- [ ] **Step 4: gitignore 적용 검증**

```bash
git check-ignore -v apps/audio_demo/resources/banks/Master.bank
git status --short apps/audio_demo/resources/banks/
```

Expected: 첫 명령은 `.gitignore:<line>: apps/*/resources/banks/ ...` 로 매칭, 두 번째는 무출력 (untracked 로도 안 잡힘).

- [ ] **Step 5: POST_BUILD copy 동작 확인**

```bash
cmake --build --preset ninja --target audio_demo
ls build_ninja/apps/audio_demo/resources/banks/
```

Expected: 빌드 직후 build dir 안 resources/banks/ 에 3개 bank 복사됨.

- [ ] **Step 6: commit**

```bash
git add .gitignore
git commit -m "[chore] : audio_demo bank 파일용 .gitignore 패턴 추가 (Task 2)"
```

---

## Task 3: FMOD Studio System 초기화 / 종료

System 객체 create + initialize + release 라이프사이클. bank 로드는 아직 없음 — 시스템 자체가 깨끗하게 살고 죽는지부터.

**Files:**
- Modify: `<apps>/audio_demo/main.cpp`

- [ ] **Step 1: FMOD 헤더 + 에러 헬퍼 + System 멤버 추가**

`main.cpp` 를 다음과 같이 갱신:

```cpp
// audio_demo — FMOD Studio + ImGui 파라미터 데모 (single-file chapter).
// Task 3: FMOD Studio System 초기화/종료.
#include <sb7.h>

#include <fmod/fmod_studio.hpp>
#include <fmod/fmod_errors.h>

#include <cstdio>
#include <cstdlib>

namespace {
// 학습 챕터 — 첫 실패에서 즉시 종료 (graceful degradation 안 함).
void ck(FMOD_RESULT r, const char* where) {
    if (r != FMOD_OK) {
        std::fprintf(stderr, "[FMOD] %s: %s\n", where, FMOD_ErrorString(r));
        std::exit(1);
    }
}
} // anon

class audio_demo_application : public sb7::application
{
    FMOD::Studio::System* mSystem = nullptr;

    void init() override
    {
        sb7::application::init();
        info.majorVersion = 4;
        info.minorVersion = 1;
        std::snprintf(info.title, sizeof(info.title), "FMOD Studio + ImGui Audio Demo");
    }

    void startup() override
    {
        ck(FMOD::Studio::System::create(&mSystem),                              "System::create");
        ck(mSystem->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr),
                                                                                "System::initialize");
        std::fprintf(stderr, "[audio_demo] FMOD Studio system initialized.\n");
    }

    void render(double /*currentTime*/) override
    {
        if (mSystem) {
            ck(mSystem->update(), "System::update");
        }
        static const GLfloat clearColor[] = {0.10f, 0.10f, 0.12f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, clearColor);
    }

    void shutdown() override
    {
        if (mSystem) {
            ck(mSystem->release(), "System::release");
            mSystem = nullptr;
        }
    }
};

DECLARE_MAIN(audio_demo_application);
```

- [ ] **Step 2: 빌드**

```bash
cmake --build --preset ninja --target audio_demo
```

Expected: link 성공. `otool -L build_ninja/apps/audio_demo/audio_demo | grep fmod` 로 `@rpath/libfmod.dylib` + `@rpath/libfmodstudio.dylib` 둘 다 표시되면 의존성 정상.

- [ ] **Step 3: 실행 + 로그 확인**

```bash
cd build_ninja/apps/audio_demo && ./audio_demo
```

Expected:
- stderr 에 `[audio_demo] FMOD Studio system initialized.` 출력
- 회색 창 표시, 음악 X
- ESC 종료 시 crash 없음

- [ ] **Step 4: commit**

```bash
git add <apps>/audio_demo/main.cpp
git commit -m "[feat] : audio_demo FMOD Studio System init/shutdown (Task 3)"
```

---

## Task 4: Bank 로딩 (Master / Strings / Music)

3개 bank 를 로드/언로드. 이벤트나 재생은 아직 없음 — 파일 I/O 와 bank 핸들 라이프사이클만.

**Files:**
- Modify: `<apps>/audio_demo/main.cpp`

- [ ] **Step 1: Bank 멤버 + 로드/언로드 호출 추가**

`mSystem` 멤버 아래에 추가:

```cpp
    FMOD::Studio::Bank* mMasterBank  = nullptr;
    FMOD::Studio::Bank* mStringsBank = nullptr;
    FMOD::Studio::Bank* mMusicBank   = nullptr;
```

`startup()` 의 `initialize` 호출 다음에 추가:

```cpp
        ck(mSystem->loadBankFile("resources/banks/Master.bank",
                                  FMOD_STUDIO_LOAD_BANK_NORMAL, &mMasterBank),
                                                                                "loadBankFile Master");
        ck(mSystem->loadBankFile("resources/banks/Master.strings.bank",
                                  FMOD_STUDIO_LOAD_BANK_NORMAL, &mStringsBank),
                                                                                "loadBankFile Strings");
        ck(mSystem->loadBankFile("resources/banks/Music.bank",
                                  FMOD_STUDIO_LOAD_BANK_NORMAL, &mMusicBank),
                                                                                "loadBankFile Music");
        std::fprintf(stderr, "[audio_demo] Banks loaded (Master + Strings + Music).\n");
```

`shutdown()` 의 `release` 호출 **앞에** bank 해제 추가 (역순):

```cpp
        if (mMusicBank)   { mMusicBank->unload();   mMusicBank   = nullptr; }
        if (mStringsBank) { mStringsBank->unload(); mStringsBank = nullptr; }
        if (mMasterBank)  { mMasterBank->unload();  mMasterBank  = nullptr; }
```

- [ ] **Step 2: 빌드 + 실행**

```bash
cmake --build --preset ninja --target audio_demo
cd build_ninja/apps/audio_demo && ./audio_demo
```

Expected:
- stderr 에 `[audio_demo] Banks loaded (Master + Strings + Music).` 표시
- 종료 시 crash 없음 (unload 순서 정상)
- 만약 `loadBankFile` 가 `ERR_FILE_NOTFOUND` 면 → POST_BUILD copy 가 안 됐거나 cwd 가 잘못된 것. `ls resources/banks/` 로 확인.

- [ ] **Step 3: commit**

```bash
git add <apps>/audio_demo/main.cpp
git commit -m "[feat] : audio_demo Master/Strings/Music bank 로드 (Task 4)"
```

---

## Task 5: 이벤트 생성 + 재생 (음악 들림)

`event:/Music/Level 01` 의 EventDescription → EventInstance → start. 여기서부터 청각 확인 가능.

**Files:**
- Modify: `<apps>/audio_demo/main.cpp`

- [ ] **Step 1: 이벤트/인스턴스 멤버 + 생성 + 재생 코드 추가**

멤버에 추가:

```cpp
    FMOD::Studio::EventDescription* mEventDesc = nullptr;
    FMOD::Studio::EventInstance*    mInstance  = nullptr;
```

`startup()` 의 bank 로드 다음에 추가 (fallback 포함):

```cpp
        // 우선 하드코드 이벤트 시도 — 실패 시 Music.bank 안의 첫 이벤트로 fallback.
        FMOD_RESULT r = mSystem->getEvent("event:/Music/Level 01", &mEventDesc);
        if (r != FMOD_OK) {
            std::fprintf(stderr, "[audio_demo] event:/Music/Level 01 미존재, fallback...\n");
            int count = 0;
            ck(mMusicBank->getEventCount(&count), "Bank::getEventCount");
            if (count == 0) {
                std::fprintf(stderr, "[FMOD] Music.bank 에 이벤트가 없음.\n");
                std::exit(1);
            }
            std::vector<FMOD::Studio::EventDescription*> events(count);
            ck(mMusicBank->getEventList(events.data(), count, nullptr), "Bank::getEventList");
            mEventDesc = events[0];
        }

        ck(mEventDesc->createInstance(&mInstance), "EventDescription::createInstance");
        ck(mInstance->start(),                     "EventInstance::start");
        std::fprintf(stderr, "[audio_demo] Event playing.\n");
```

`std::vector` 사용하므로 파일 상단에 `#include <vector>` 추가.

`shutdown()` 의 bank unload **앞에** 인스턴스 해제 추가:

```cpp
        if (mInstance) {
            mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
            mInstance->release();
            mInstance  = nullptr;
            mEventDesc = nullptr;
        }
```

- [ ] **Step 2: 빌드**

```bash
cmake --build --preset ninja --target audio_demo
```

Expected: link 성공.

- [ ] **Step 3: 실행 — 음악 들림**

```bash
cd build_ninja/apps/audio_demo && ./audio_demo
```

Expected:
- stderr `[audio_demo] Event playing.` 출력
- **스피커에서 음악 들림** (Music/Level 01 의 기본 진행)
- 창은 여전히 회색 (UI 없음)
- 종료 시 fade-out 없이 즉시 정지 + crash 없음

- [ ] **Step 4: commit**

```bash
git add <apps>/audio_demo/main.cpp
git commit -m "[feat] : audio_demo Music event 자동 생성 + 재생 (Task 5)"
```

---

## Task 6: 파라미터 자동 발견 + 콘솔 덤프

EventDescription 의 파라미터 메타데이터를 런타임 조회해 ParamCache 로 캐싱. UI 는 다음 Task — 여기서는 stderr 덤프로 사전 검증.

**Files:**
- Modify: `<apps>/audio_demo/main.cpp`

- [ ] **Step 1: ParamCache 구조체 + 발견 루프 추가**

파일 상단(anon namespace 안) 에 추가:

```cpp
struct ParamCache {
    std::string name;
    FMOD_STUDIO_PARAMETER_ID id;
    enum Kind { Continuous, Labeled, Switch, DiscreteInt };
    Kind kind;
    float minValue;
    float maxValue;
    std::vector<std::string> labels;   // Labeled 일 때만
    float currentValue;
};
```

`#include <string>` 도 상단에 추가.

application 멤버에 추가:

```cpp
    std::vector<ParamCache> mParams;
```

`startup()` 의 `mInstance->start()` **다음에** (인스턴스 시작 후 메타데이터 발견 + 콘솔 덤프) 추가:

```cpp
        int paramCount = 0;
        ck(mEventDesc->getParameterDescriptionCount(&paramCount),
                                                                                "getParameterDescriptionCount");
        for (int i = 0; i < paramCount; ++i) {
            FMOD_STUDIO_PARAMETER_DESCRIPTION desc{};
            ck(mEventDesc->getParameterDescriptionByIndex(i, &desc),
                                                                                "getParameterDescriptionByIndex");

            ParamCache p;
            p.name         = desc.name;
            p.id           = desc.id;
            p.minValue     = desc.minimum;
            p.maxValue     = desc.maximum;
            p.currentValue = desc.defaultvalue;

            const bool labeled  = (desc.flags & FMOD_STUDIO_PARAMETER_LABELED) != 0;
            const bool discrete = (desc.flags & FMOD_STUDIO_PARAMETER_DISCRETE) != 0;

            if (labeled) {
                p.kind = ParamCache::Labeled;
                for (int v = (int)desc.minimum; v <= (int)desc.maximum; ++v) {
                    char buf[128]{};
                    int retrieved = 0;
                    mEventDesc->getParameterLabelByID(desc.id, v, buf, sizeof(buf), &retrieved);
                    p.labels.emplace_back(buf);
                }
            } else if (discrete) {
                p.kind = (desc.minimum == 0.0f && desc.maximum == 1.0f)
                            ? ParamCache::Switch
                            : ParamCache::DiscreteInt;
            } else {
                p.kind = ParamCache::Continuous;
            }

            const char* kindStr = "?";
            switch (p.kind) {
                case ParamCache::Continuous:  kindStr = "Continuous";  break;
                case ParamCache::Labeled:     kindStr = "Labeled";     break;
                case ParamCache::Switch:      kindStr = "Switch";      break;
                case ParamCache::DiscreteInt: kindStr = "DiscreteInt"; break;
            }
            std::fprintf(stderr, "[audio_demo] Param[%d] '%s' [%s] %.2f..%.2f default=%.2f\n",
                         i, p.name.c_str(), kindStr,
                         p.minValue, p.maxValue, p.currentValue);

            mParams.push_back(std::move(p));
        }
        std::fprintf(stderr, "[audio_demo] %d parameters discovered.\n", paramCount);
```

- [ ] **Step 2: 빌드 + 실행**

```bash
cmake --build --preset ninja --target audio_demo
cd build_ninja/apps/audio_demo && ./audio_demo
```

Expected stderr 에 다음과 유사 (정확한 이름/범위는 Music.bank 의존):

```
[audio_demo] Param[0] 'Intensity' [Continuous] 0.00..100.00 default=0.00
[audio_demo] Param[1] 'Progression' [Labeled] 0.00..3.00 default=0.00
[audio_demo] 2 parameters discovered.
```

(Labeled count 가 다르거나 다른 파라미터가 추가로 발견될 수 있음 — 자동 발견이라 어느 쪽이든 OK.)

- [ ] **Step 3: commit**

```bash
git add <apps>/audio_demo/main.cpp
git commit -m "[feat] : audio_demo 파라미터 자동 발견 + 콘솔 덤프 (Task 6)"
```

---

## Task 7: ImGui v1.53 통합 (init / shutdown / 빈 윈도우)

`imguitest` 의 통합 패턴 그대로 가져옴 — `imgui_impl_glfw_gl3` 결합 backend. 이 Task 에서는 ImGui 컨텍스트만 살리고 placeholder 윈도우만 띄움.

**Files:**
- Modify: `<apps>/audio_demo/main.cpp`

- [ ] **Step 1: ImGui 헤더 + 컨텍스트 멤버 + init/shutdown/render**

상단 include 에 추가:

```cpp
#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>
```

application 멤버에 추가:

```cpp
    ImGuiContext* mImGuiCtx = nullptr;
```

`startup()` 의 **맨 끝** (파라미터 발견 완료 후) 에 추가:

```cpp
        mImGuiCtx = ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfwGL3_Init(window, true);
```

`render()` 를 다음과 같이 교체:

```cpp
    void render(double /*currentTime*/) override
    {
        if (mSystem) {
            ck(mSystem->update(), "System::update");
        }

        static const GLfloat clearColor[] = {0.10f, 0.10f, 0.12f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, clearColor);

        ImGui_ImplGlfwGL3_NewFrame();
        ImGui::Begin("Audio Demo");
        ImGui::Text("Music event playing. Widgets next.");
        ImGui::End();
        ImGui::Render();
    }
```

`shutdown()` 의 **맨 앞** (FMOD 정리보다 먼저) 에 추가:

```cpp
        if (mImGuiCtx) {
            ImGui_ImplGlfwGL3_Shutdown();
            ImGui::DestroyContext(mImGuiCtx);
            mImGuiCtx = nullptr;
        }
```

- [ ] **Step 2: 빌드 + 실행**

```bash
cmake --build --preset ninja --target audio_demo
cd build_ninja/apps/audio_demo && ./audio_demo
```

Expected:
- ImGui 윈도우 "Audio Demo" 표시 + 텍스트 "Music event playing. Widgets next."
- 음악은 여전히 재생 중
- 마우스로 윈도우 이동/리사이즈 가능
- ESC/X 종료 시 crash 없음

- [ ] **Step 3: commit**

```bash
git add <apps>/audio_demo/main.cpp
git commit -m "[feat] : audio_demo ImGui v1.53 통합 + placeholder 윈도우 (Task 7)"
```

---

## Task 8: 파라미터 위젯 (슬라이더/콤보/체크박스)

`mParams` 순회하며 kind 별로 위젯 생성, 변경 시 `setParameterByID`.

**Files:**
- Modify: `<apps>/audio_demo/main.cpp`

- [ ] **Step 1: render() 의 ImGui 블록을 위젯 루프로 교체**

`ImGui::Begin("Audio Demo")` ~ `ImGui::End()` 블록을 다음으로 교체:

```cpp
        ImGui::Begin("Audio Demo");
        ImGui::Text("Parameters (%zu discovered)", mParams.size());
        ImGui::Separator();

        for (auto& p : mParams) {
            float v = p.currentValue;
            bool changed = false;
            switch (p.kind) {
                case ParamCache::Continuous:
                    changed = ImGui::SliderFloat(p.name.c_str(), &v, p.minValue, p.maxValue);
                    break;
                case ParamCache::Labeled: {
                    int idx = (int)v;
                    // labels 를 const char* 배열로 변환 (v1.53 Combo 시그니처 호환)
                    std::vector<const char*> labelPtrs;
                    labelPtrs.reserve(p.labels.size());
                    for (auto& s : p.labels) labelPtrs.push_back(s.c_str());
                    changed = ImGui::Combo(p.name.c_str(), &idx,
                                           labelPtrs.data(), (int)labelPtrs.size());
                    v = (float)idx;
                    break;
                }
                case ParamCache::Switch: {
                    bool b = v >= 0.5f;
                    changed = ImGui::Checkbox(p.name.c_str(), &b);
                    v = b ? 1.0f : 0.0f;
                    break;
                }
                case ParamCache::DiscreteInt: {
                    int iv = (int)v;
                    changed = ImGui::SliderInt(p.name.c_str(),
                                               &iv,
                                               (int)p.minValue, (int)p.maxValue);
                    v = (float)iv;
                    break;
                }
            }
            if (changed) {
                ck(mInstance->setParameterByID(p.id, v), "setParameterByID");
                p.currentValue = v;
            }
        }

        ImGui::End();
```

- [ ] **Step 2: 빌드 + 실행 + 청각 검증**

```bash
cmake --build --preset ninja --target audio_demo
cd build_ninja/apps/audio_demo && ./audio_demo
```

Expected:
- Audio Demo 윈도우에 발견된 모든 파라미터가 각자의 위젯으로 표시 (예: `Intensity` SliderFloat 0~100, `Progression` Combo)
- 슬라이더를 끌면 **음악의 톤/레이어가 즉시 변화** (청각적)
- Combo 변경 시 음악 진행 구간 전환됨
- crash 없음

- [ ] **Step 3: commit**

```bash
git add <apps>/audio_demo/main.cpp
git commit -m "[feat] : audio_demo 파라미터 위젯 자동 매핑 (Task 8)"
```

---

## Task 9: Transport (Play/Stop/Pause) + Master 볼륨

음악 정지/재시작/일시정지 + 마스터 버스 볼륨 제어.

**Files:**
- Modify: `<apps>/audio_demo/main.cpp`

- [ ] **Step 1: Master Bus 멤버 + 캐시**

application 멤버에 추가:

```cpp
    FMOD::Studio::Bus* mMasterBus    = nullptr;
    float              mMasterVolume = 1.0f;
    bool               mPaused       = false;
```

`startup()` 의 ImGui init **앞에** 추가:

```cpp
        ck(mSystem->getBus("bus:/", &mMasterBus), "System::getBus root");
```

- [ ] **Step 2: render() 의 위젯 루프 다음에 Transport UI 추가**

`ImGui::End();` **앞에** 추가 (Audio Demo 윈도우 안):

```cpp
        ImGui::Separator();
        ImGui::Text("Transport");
        if (ImGui::Button("Play")) {
            ck(mInstance->start(), "EventInstance::start");
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            ck(mInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT), "EventInstance::stop");
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Pause", &mPaused)) {
            ck(mInstance->setPaused(mPaused), "EventInstance::setPaused");
        }

        ImGui::Separator();
        if (ImGui::SliderFloat("Master Volume", &mMasterVolume, 0.0f, 1.0f, "%.2f")) {
            ck(mMasterBus->setVolume(mMasterVolume), "Bus::setVolume");
        }
```

- [ ] **Step 3: 빌드 + 실행 + 청각 검증**

```bash
cmake --build --preset ninja --target audio_demo
cd build_ninja/apps/audio_demo && ./audio_demo
```

Expected:
- Transport 행에 Play / Stop / Pause 컨트롤
- Stop 누르면 음악이 fade-out 으로 잦아듦
- Play 누르면 다시 시작 (Stop ALLOWFADEOUT 이후 새 인스턴스 start 가 아니라 기존 인스턴스 start — FMOD 가 자동 재시작 처리)
- Pause 토글 시 즉시 멈춤/재개
- Master Volume 슬라이더로 전체 볼륨 변화

- [ ] **Step 4: commit**

```bash
git add <apps>/audio_demo/main.cpp
git commit -m "[feat] : audio_demo Transport + Master 볼륨 (Task 9)"
```

---

## Task 10: 최종 검증 + 문서 갱신

빌드 산출물 / 의존성 확인 + CLAUDE.md 의 활성 챕터 리스트에 audio_demo 추가.

**Files:**
- Modify: `.claude/CLAUDE.md`

- [ ] **Step 1: 빌드 산출물 dylib 의존성 확인 (macOS)**

```bash
otool -L build_ninja/apps/audio_demo/audio_demo | grep -E "fmod|@rpath"
```

Expected:
```
@rpath/libfmod.dylib (...)
@rpath/libfmodstudio.dylib (...)
```

(dead_strip_dylibs 가 안 일어남 — 실제로 호출하니까.)

- [ ] **Step 2: 실행 파일 옆 dylib 존재 확인**

```bash
ls build_ninja/apps/audio_demo/ | grep -E "fmod|\.bank"
```

Expected:
```
libfmod.dylib
libfmodL.dylib
libfmodstudio.dylib
libfmodstudioL.dylib
resources
```

(resources 안에 banks/ 도 들어있음.)

- [ ] **Step 3: rpath baked 확인**

```bash
otool -l build_ninja/apps/audio_demo/audio_demo | grep -A2 LC_RPATH
```

Expected: `path @loader_path (offset 12)` 표시.

- [ ] **Step 4: CLAUDE.md 의 챕터 리스트에 audio_demo 추가**

`Active Target Management` 절의 챕터 리스트 ("현재 빌드 가능한 타겟 디렉토리" 아래) 에 한 줄 추가:

```
- `audio_demo` — FMOD Studio + ImGui 파라미터 데모 (Music.bank, event:/Music/Level 0X, Intensity/Progression 슬라이더+콤보 + Transport)
```

- [ ] **Step 5: 전체 스모크 — clean rebuild + 실행**

```bash
cmake --build --preset ninja --target audio_demo
cd build_ninja/apps/audio_demo && ./audio_demo
```

Expected:
- 음악 재생됨
- ImGui 윈도우 표시, 모든 위젯 동작
- 슬라이더/콤보/체크박스/볼륨/Play/Stop/Pause 청각적으로 정상 반영
- 종료 시 crash 없음, stderr 에 에러 없음

- [ ] **Step 6: commit**

```bash
git add .claude/CLAUDE.md
git commit -m "[docs] : audio_demo 챕터 등재 + 최종 검증 통과 (Task 10)"
```

---

## Self-Review

**Spec 커버리지 (spec 절 → task):**
- "파일 레이아웃" → Task 1 + Task 2
- "결정 사항" 표 → Task 1 (CMake), Task 3 (FMOD init), Task 7 (ImGui), Task 8 (위젯 매핑), Task 9 (Transport)
- "application 라이프사이클" → Task 3~9 누적
- "파라미터 자동 발견 알고리즘" → Task 6 + Task 8
- "CMakeLists.txt 통합" → Task 1
- ".bank 배치 절차" → Task 2
- "에러 처리" → Task 3 (ck 헬퍼)
- "검증 기준" → Task 10
- "미적용 (YAGNI 컷)" — 의도적으로 안 다룸 ✓

**Placeholder scan:** TBD/TODO/"add appropriate ..." 없음. 모든 step 에 실제 코드/명령 포함. ✓

**Type consistency:** `ParamCache::Kind` 의 4개 enum (Continuous/Labeled/Switch/DiscreteInt) 가 Task 6 정의 → Task 8 사용에서 동일하게 매칭. `setParameterByID` 통일 (ByIndex 미사용). `mMasterBus` Task 9 에서 정의 + 사용. ✓

**Gap:** 없음 — spec 의 모든 요구사항 → task 매핑 완료.
