# 게임/엔진 라이브러리 의존성 등록 Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Box2D · Effekseer · EnTT · Tweeny · stb 5종을 `extern/` 서브모듈 + `lib/`·`include/` 사전 빌드 산출물로 등록하고 `cmake/Dependency.cmake` 의 `game_deps` 집계 타겟으로 노출한다.

**Architecture:** 기존 sb7/glfw3 패턴을 그대로 따른다 — 서브모듈은 버전 추적·재빌드 소스, 빌드는 체크인된 산출물에 의존. 컴파일 라이브러리(Box2D/Effekseer)는 `IMPORTED STATIC`, 헤더 온리(EnTT/Tweeny/stb)는 `INTERFACE` 타겟. 모두 신규 `game_deps` INTERFACE 타겟으로 집계. `project_deps` 는 무변경. 검증은 임시 타겟 `apps/_deptest_/` 에서 헤더 include + 심볼 링크 → 빌드 성공으로 한다.

**Tech Stack:** CMake 3.x, Ninja(macOS) / MSVC(Windows), Box2D v2.4.1(C++ API), Effekseer 1.7x, EnTT v3.x, Tweeny v3, stb.

**참고 스펙:** `doc/superpowers/specs/2026-05-19-game-engine-deps-registration-design.md`

**플랫폼 주의:** macOS 산출물(`*.a`)은 로컬에서 생성·검증한다. Windows 산출물(`*.lib`)은 CI(`build-msvc.yml`)에서 생성한다 — Task 6 참조. 따라서 Task 4·5 의 빌드 검증은 macOS 기준이다.

---

### Task 1: extern/ 서브모듈 5종 등록

**Files:**
- Modify: `.gitmodules`
- Create: `extern/box2d` `extern/Effekseer` `extern/entt` `extern/tweeny` `extern/stb` (서브모듈)

- [ ] **Step 1: Box2D 서브모듈 추가 후 v2.4.1 고정**

```bash
git submodule add https://github.com/erincatto/box2d.git extern/box2d
git -C extern/box2d checkout v2.4.1
```

- [ ] **Step 2: Effekseer 서브모듈 추가 후 최신 1.7x 태그 고정**

```bash
git submodule add https://github.com/effekseer/Effekseer.git extern/Effekseer
git -C extern/Effekseer fetch --tags
EFK_TAG=$(git -C extern/Effekseer tag -l '1.7*' | sort -V | tail -1)
echo "Effekseer 태그: $EFK_TAG"
git -C extern/Effekseer checkout "$EFK_TAG"
```

Expected: `Effekseer 태그: 1.70x` 형태 출력 (예: `1.70e`).

- [ ] **Step 3: EnTT 서브모듈 추가 후 최신 v3.x 태그 고정**

```bash
git submodule add https://github.com/skypjack/entt.git extern/entt
git -C extern/entt fetch --tags
ENTT_TAG=$(git -C extern/entt tag -l 'v3.*' | sort -V | tail -1)
echo "EnTT 태그: $ENTT_TAG"
git -C extern/entt checkout "$ENTT_TAG"
```

Expected: `EnTT 태그: v3.13.x` 형태 출력.

- [ ] **Step 4: Tweeny 서브모듈 추가 후 v3 고정**

```bash
git submodule add https://github.com/mobius3/tweeny.git extern/tweeny
git -C extern/tweeny checkout v3
```

- [ ] **Step 5: stb 서브모듈 추가 (릴리스 태그 없음 → master HEAD 고정)**

```bash
git submodule add https://github.com/nothings/stb.git extern/stb
git -C extern/stb rev-parse HEAD
```

Expected: 40자리 커밋 SHA 출력 — 이 SHA 가 고정 버전이 된다.

- [ ] **Step 6: 서브모듈 등록 확인**

Run: `git submodule status`
Expected: `extern/box2d` `extern/Effekseer` `extern/entt` `extern/tweeny` `extern/stb` `extern/sb7code` 6줄이 모두 출력되고, 각 줄 앞에 공백(체크아웃 완료) 또는 `+`(태그로 이동됨)가 붙는다.

- [ ] **Step 7: 커밋**

```bash
git add .gitmodules extern/box2d extern/Effekseer extern/entt extern/tweeny extern/stb
git commit -m "[deps] : extern 서브모듈 5종 등록 (box2d/Effekseer/entt/tweeny/stb)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: 검증용 임시 타겟 `apps/_deptest_/` 생성

이후 모든 Task 의 빌드 검증에 쓸 최소 sb7 애플리케이션. 패턴 B(단일 `main.cpp`).

**Files:**
- Create: `<apps>/_deptest_/main.cpp`
- Create: `apps/_deptest_/CMakeLists.txt`
- Modify: `apps/CMakeLists.txt`

- [ ] **Step 1: `<apps>/_deptest_/main.cpp` 작성 (최소 sb7 앱)**

```cpp
// 의존성 등록 검증용 임시 타겟 — Task 8 에서 비활성화한다.
#include <sb7.h>

class deptest_application : public sb7::application
{
    void render(double currentTime) override
    {
        static const GLfloat green[] = {0.0f, 0.25f, 0.0f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, green);
    }
};

DECLARE_MAIN(deptest_application);
```

- [ ] **Step 2: `apps/_deptest_/CMakeLists.txt` 작성 (project_deps 만 링크)**

`apps/chapter9/CMakeLists.txt` 와 동일 패턴.

```cmake
# 챕터 타겟 이름을 현재 CMakeLists.txt source 디렉토리 위치로 사용할 것
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# 실행파일
if(WIN32)
    add_executable(${CHAPTER_NAME} WIN32 main.cpp)
else()
    add_executable(${CHAPTER_NAME} main.cpp)
endif()

# 라이브러리 추가
target_link_libraries(${CHAPTER_NAME}
    PRIVATE
        project_deps
)

# 인클루드 디렉토리 추가
target_include_directories(${CHAPTER_NAME} PRIVATE ${CMAKE_CURRENT_DIR})

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
endif()
```

- [ ] **Step 3: `apps/CMakeLists.txt` 에 `_deptest_` 활성화**

`apps/CMakeLists.txt` 끝부분 `add_subdirectory(chapter9)` 줄을 주석 처리하고, 그 아래에 `_deptest_` 줄을 추가한다.

```cmake
# add_subdirectory(chapter9)
add_subdirectory(_deptest_)
```

- [ ] **Step 4: 빌드 검증 — 실패가 아닌 성공을 확인**

Run:
```bash
cmake --preset ninja
cmake --build --preset ninja --target _deptest_
```
Expected: PASS — `build_ninja/apps/_deptest_/_deptest_` 실행 파일 생성.

- [ ] **Step 5: 커밋**

```bash
git add <apps>/_deptest_/main.cpp apps/_deptest_/CMakeLists.txt apps/CMakeLists.txt
git commit -m "[deps] : 의존성 검증용 임시 타겟 _deptest_ 추가

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: 헤더 온리 3종 등록 + game_deps 골격

EnTT · Tweeny · stb 헤더를 `include/` 로 복사하고 `Dependency.cmake` 에 INTERFACE 타겟 + `game_deps` 를 만든다.

**Files:**
- Create: `<include>/entt/entt.hpp`
- Create: `include/tweeny/` (Tweeny 헤더 묶음)
- Create: `<include>/stb_rect_pack.h`
- Modify: `cmake/Dependency.cmake`
- Modify: `<apps>/_deptest_/main.cpp`
- Modify: `apps/_deptest_/CMakeLists.txt`

- [ ] **Step 1: 헤더 복사**

```bash
# EnTT single-include
mkdir -p include/entt
cp <extern>/entt/single_include/entt/entt.hpp <include>/entt/entt.hpp

# Tweeny 헤더 묶음 (tweeny.h 가 형제 헤더를 상대 include 하므로 디렉토리째 복사)
mkdir -p include/tweeny
cp extern/tweeny/include/* include/tweeny/

# stb_rect_pack (stb_image.h 는 이미 include/ 에 존재 → 복사하지 않음)
cp <extern>/stb/stb_rect_pack.h <include>/stb_rect_pack.h
```

- [ ] **Step 2: 복사 확인**

Run: `ls <include>/entt/entt.hpp <include>/tweeny/tweeny.h <include>/stb_rect_pack.h`
Expected: 세 경로 모두 존재로 출력 (에러 없음).

- [ ] **Step 3: `cmake/Dependency.cmake` 에 헤더 온리 INTERFACE 타겟 + game_deps 추가**

`cmake/Dependency.cmake` 맨 끝(57번째 줄 `endif()` 다음)에 아래 블록을 추가한다.

```cmake

# ====== 게임/엔진 라이브러리 (extern 서브모듈 → lib/include 사전 빌드) ======
# 헤더 온리 — 헤더는 이미 include/ 에 체크인. INTERFACE 타겟은 game_deps 멤버 표식.
add_library(entt INTERFACE)
add_library(tweeny INTERFACE)
add_library(stb_extra INTERFACE)

# game_deps — 게임/엔진 챕터가 project_deps 와 함께 링크하는 집계 타겟.
# Box2D / Effekseer 는 각각 Task 4 / Task 5 에서 이 줄에 추가된다.
add_library(game_deps INTERFACE)
target_link_libraries(game_deps INTERFACE
    entt tweeny stb_extra)
target_include_directories(game_deps INTERFACE "${CMAKE_SOURCE_DIR}/include")
```

- [ ] **Step 4: `<apps>/_deptest_/main.cpp` 에 헤더 온리 검증 코드 추가**

`main.cpp` 전체를 아래로 교체한다.

```cpp
// 의존성 등록 검증용 임시 타겟 — Task 8 에서 비활성화한다.
#include <sb7.h>

#include <<entt>/entt.hpp>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#include <<tweeny>/tweeny.h>

#include <cstdio>

class deptest_application : public sb7::application
{
    void startup() override
    {
        // EnTT — registry 에 엔티티 1개 생성
        entt::registry registry;
        auto entity = registry.create();
        registry.emplace<int>(entity, 42);

        // Tweeny — 0→100 보간 트윈
        auto tween = tweeny::from(0).to(100).during(100);
        int mid = tween.step(50);

        // stb_rect_pack — 패킹 컨텍스트 초기화
        stbrp_context ctx;
        std::vector<stbrp_node> nodes(64);
        stbrp_init_target(&ctx, 256, 256, nodes.data(), static_cast<int>(nodes.size()));

        std::printf("[deptest] entt=%d tweeny=%d stbrp 초기화 완료\n",
                    registry.get<int>(entity), mid);
    }

    void render(double currentTime) override
    {
        static const GLfloat green[] = {0.0f, 0.25f, 0.0f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, green);
    }
};

DECLARE_MAIN(deptest_application);
```

- [ ] **Step 5: `apps/_deptest_/CMakeLists.txt` 에 game_deps 링크 추가**

`target_link_libraries` 블록을 아래로 교체한다.

```cmake
# 라이브러리 추가
target_link_libraries(${CHAPTER_NAME}
    PRIVATE
        project_deps
        game_deps
)
```

- [ ] **Step 6: 빌드 검증**

Run:
```bash
cmake --preset ninja
cmake --build --preset ninja --target _deptest_
```
Expected: PASS — 컴파일·링크 성공. `<vector>` 누락 경고가 나오면 `main.cpp` 상단에 `#include <vector>` 추가.

- [ ] **Step 7: 커밋**

```bash
git add include/entt include/tweeny <include>/stb_rect_pack.h cmake/Dependency.cmake <apps>/_deptest_/main.cpp apps/_deptest_/CMakeLists.txt
git commit -m "[deps] : 헤더 온리 3종 등록 (entt/tweeny/stb_rect_pack) + game_deps 골격

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Box2D 빌드 + 등록 (macOS)

Box2D v2.4.1 을 Release/Debug 로 빌드해 `lib/macos/` 로 체크인하고 `Dependency.cmake` 에 IMPORTED 타겟으로 등록한다.

**Files:**
- Modify: `<shell>/BuildExternLibs.sh`
- Create: `lib/macos/libbox2d.a` `lib/macos/libbox2d_d.a`
- Create: `include/box2d/` (Box2D 공개 헤더)
- Modify: `cmake/Dependency.cmake`
- Modify: `<apps>/_deptest_/main.cpp`

- [ ] **Step 1: `<shell>/BuildExternLibs.sh` 에 Box2D 빌드 함수 추가**

`<shell>/BuildExternLibs.sh` 의 sb7 빌드 블록(`build_sb7 Debug "_d"` 줄과 그 다음 `echo` 줄)과 `# ====== 완료 ======` 주석 사이에 아래 블록을 삽입한다.

```bash
# ====== Box2D 빌드 (Release + Debug) ======
echo "[+] Box2D v2.4.1 빌드..."

BOX2D_DIR="$ROOT_DIR/extern/box2d"

build_box2d() {
    local BUILD_TYPE=$1   # Release or Debug
    local SUFFIX=$2       # "" or "_d"

    local BOX2D_BUILD="$BUILD_DIR/box2d_${BUILD_TYPE}"
    cmake -S "$BOX2D_DIR" -B "$BOX2D_BUILD" \
        -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DBOX2D_BUILD_TESTBED=OFF \
        -DBOX2D_BUILD_UNIT_TESTS=OFF \
        -DBOX2D_BUILD_DOCS=OFF

    cmake --build "$BOX2D_BUILD"

    local ARTIFACT
    ARTIFACT=$(find "$BOX2D_BUILD" -name 'libbox2d.a' | head -1)
    if [ -z "$ARTIFACT" ]; then
        echo "ERROR: libbox2d.a 를 찾을 수 없음" >&2
        exit 1
    fi
    cp "$ARTIFACT" "$LIB_DIR/libbox2d${SUFFIX}.a"
}

build_box2d Release ""
build_box2d Debug "_d"

# Box2D 공개 헤더 복사
cp -r "$BOX2D_DIR/include/box2d" "$INCLUDE_DIR/box2d"

echo "  -> Box2D 빌드 완료 (Release + Debug)"
```

- [ ] **Step 2: 빌드 스크립트 실행**

Run: `sh <shell>/BuildExternLibs.sh`
Expected: PASS — `build_extern/output/macos/libbox2d.a` `libbox2d_d.a` 와 `build_extern/output/include/box2d/` 생성. (sb7/glfw3 도 다시 빌드되지만 정상.)

- [ ] **Step 3: 산출물을 lib/include 로 복사**

```bash
cp build_extern/output/macos/libbox2d.a build_extern/output/macos/libbox2d_d.a lib/macos/
cp -r build_extern/output/include/box2d include/box2d
ls lib/macos/libbox2d.a lib/macos/libbox2d_d.a <include>/box2d/box2d.h
```
Expected: 세 경로 모두 존재.

- [ ] **Step 4: `cmake/Dependency.cmake` 에 box2d IMPORTED 타겟 등록**

Task 3 에서 추가한 `# 헤더 온리` 주석 줄 **앞**(즉 `# ====== 게임/엔진 라이브러리 ...` 줄 바로 다음)에 아래를 삽입한다.

```cmake
# Box2D v2.4.1 — C++ 정적 라이브러리
add_library(box2d STATIC IMPORTED)
if(WIN32)
    set_target_properties(box2d PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/box2d.lib
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/box2d_d.lib)
else()
    set_target_properties(box2d PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libbox2d.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libbox2d_d.a)
endif()

```

- [ ] **Step 5: `game_deps` 에 box2d 추가**

`cmake/Dependency.cmake` 의 `game_deps` 블록에서 `target_link_libraries` 를 아래로 교체한다.

```cmake
target_link_libraries(game_deps INTERFACE
    box2d
    entt tweeny stb_extra)
```

- [ ] **Step 6: `<apps>/_deptest_/main.cpp` 의 `startup()` 에 Box2D 검증 코드 추가**

`#include <<tweeny>/tweeny.h>` 줄 다음에 추가:

```cpp
#include <<box2d>/box2d.h>
```

`startup()` 의 `std::printf(...)` 줄 **앞**에 추가:

```cpp
        // Box2D — 중력 월드 + 동적 바디 1개
        b2World world(b2Vec2(0.0f, -10.0f));
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position.Set(0.0f, 4.0f);
        b2Body *body = world.CreateBody(&bodyDef);
        world.Step(1.0f / 60.0f, 8, 3);
        std::printf("[deptest] box2d body y=%.3f\n", body->GetPosition().y);
```

- [ ] **Step 7: 빌드 검증**

Run:
```bash
cmake --preset ninja
cmake --build --preset ninja --target _deptest_
```
Expected: PASS — `b2World`/`b2Body` 심볼이 `libbox2d_d.a` 에서 링크됨. `undefined reference to b2World::b2World` 가 나오면 lib 복사(Step 3) 또는 IMPORTED 경로(Step 4)를 재확인.

- [ ] **Step 8: 커밋**

```bash
git add <shell>/BuildExternLibs.sh lib/macos/libbox2d.a lib/macos/libbox2d_d.a include/box2d cmake/Dependency.cmake <apps>/_deptest_/main.cpp
git commit -m "[deps] : Box2D v2.4.1 등록 (macOS 사전 빌드 + IMPORTED 타겟)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Effekseer 빌드 + 등록 (macOS)

**최대 리스크 태스크.** Effekseer 는 `Effekseer` + `EffekseerRendererGL` 두 정적 라이브러리를 산출하고, 헤더 구조와 CMake 옵션명이 버전에 따라 다르다. Step 1 에서 먼저 구조를 확인한 뒤 진행한다.

**Files:**
- Modify: `<shell>/BuildExternLibs.sh`
- Create: `lib/macos/libEffekseer.a` `libEffekseer_d.a` `libEffekseerRendererGL.a` `libEffekseerRendererGL_d.a`
- Create: `include/Effekseer/` (Effekseer + EffekseerRendererGL 공개 헤더)
- Modify: `cmake/Dependency.cmake`
- Modify: `<apps>/_deptest_/main.cpp`

- [ ] **Step 1: Effekseer 저장소 구조·CMake 옵션 확인**

```bash
ls extern/Effekseer
grep -nE 'option\(BUILD_|option\(USE_' extern/Effekseer/CMakeLists.txt
find extern/Effekseer -maxdepth 4 -name 'Effekseer.h' -o -maxdepth 4 -name 'EffekseerRendererGL.h'
```
Expected: `BUILD_GL` / `BUILD_VIEWER` / `BUILD_EXAMPLES` 류 옵션 목록과 우산 헤더(`Effekseer.h`, `EffekseerRendererGL.h`) 경로 확인. 보통 헤더는 `extern/Effekseer/Dev/Cpp/Effekseer/` 및 `Dev/Cpp/EffekseerRendererGL/` 아래. 옵션명이 아래 Step 2 와 다르면 Step 2 의 `-D` 플래그를 확인된 이름으로 맞춘다.

- [ ] **Step 2: `<shell>/BuildExternLibs.sh` 에 Effekseer 빌드 함수 추가**

Task 4 에서 삽입한 Box2D 블록 **다음**, `# ====== 완료 ======` 주석 **앞**에 삽입한다. (`EFK_HEADER_SRC` 는 Step 1 에서 확인한 헤더 루트 — 보통 `Dev/Cpp`.)

```bash
# ====== Effekseer 빌드 (Release + Debug) ======
echo "[+] Effekseer 빌드..."

EFK_DIR="$ROOT_DIR/extern/Effekseer"
EFK_HEADER_SRC="$EFK_DIR/Dev/Cpp"   # Step 1 에서 확인한 헤더 루트

build_effekseer() {
    local BUILD_TYPE=$1   # Release or Debug
    local SUFFIX=$2       # "" or "_d"

    local EFK_BUILD="$BUILD_DIR/effekseer_${BUILD_TYPE}"
    cmake -S "$EFK_DIR" -B "$EFK_BUILD" \
        -G "Ninja" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DBUILD_GL=ON \
        -DBUILD_VIEWER=OFF \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_TESTS=OFF \
        -DBUILD_DX9=OFF -DBUILD_DX11=OFF -DBUILD_DX12=OFF \
        -DBUILD_VULKAN=OFF -DBUILD_METAL=OFF

    cmake --build "$EFK_BUILD" --target Effekseer EffekseerRendererGL

    local EFK_LIB EFK_GL_LIB
    EFK_LIB=$(find "$EFK_BUILD" -name 'libEffekseer.a' | head -1)
    EFK_GL_LIB=$(find "$EFK_BUILD" -name 'libEffekseerRendererGL.a' | head -1)
    if [ -z "$EFK_LIB" ] || [ -z "$EFK_GL_LIB" ]; then
        echo "ERROR: Effekseer 정적 라이브러리를 찾을 수 없음" >&2
        exit 1
    fi
    cp "$EFK_LIB"    "$LIB_DIR/libEffekseer${SUFFIX}.a"
    cp "$EFK_GL_LIB" "$LIB_DIR/libEffekseerRendererGL${SUFFIX}.a"
}

build_effekseer Release ""
build_effekseer Debug "_d"

# Effekseer 공개 헤더 복사 (우산 헤더 + 하위 디렉토리)
mkdir -p "$INCLUDE_DIR/Effekseer"
cp -r "$EFK_HEADER_SRC/Effekseer/"*           "$INCLUDE_DIR/Effekseer/"
cp -r "$EFK_HEADER_SRC/EffekseerRendererGL/"* "$INCLUDE_DIR/Effekseer/"

echo "  -> Effekseer 빌드 완료 (Release + Debug)"
```

- [ ] **Step 3: 빌드 스크립트 실행**

Run: `sh <shell>/BuildExternLibs.sh`
Expected: PASS — `build_extern/output/macos/` 에 `libEffekseer.a` `libEffekseer_d.a` `libEffekseerRendererGL.a` `libEffekseerRendererGL_d.a` 4개, `build_extern/output/include/Effekseer/` 에 `Effekseer.h`·`EffekseerRendererGL.h` 생성.

- [ ] **Step 4: 산출물을 lib/include 로 복사**

```bash
cp build_extern/output/macos/libEffekseer.a build_extern/output/macos/libEffekseer_d.a \
   build_extern/output/macos/libEffekseerRendererGL.a build_extern/output/macos/libEffekseerRendererGL_d.a \
   lib/macos/
cp -r build_extern/output/include/Effekseer include/Effekseer
ls lib/macos/libEffekseer.a lib/macos/libEffekseerRendererGL.a include/Effekseer/Effekseer.h include/Effekseer/EffekseerRendererGL.h
```
Expected: 모든 경로 존재.

- [ ] **Step 5: `cmake/Dependency.cmake` 에 Effekseer IMPORTED 타겟 등록**

`box2d` IMPORTED 블록(Task 4 Step 4) **다음**에 삽입한다.

```cmake
# Effekseer — 파티클 엔진 + OpenGL 렌더러
add_library(Effekseer STATIC IMPORTED)
add_library(EffekseerRendererGL STATIC IMPORTED)
if(WIN32)
    set_target_properties(Effekseer PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/Effekseer.lib
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/Effekseer_d.lib)
    set_target_properties(EffekseerRendererGL PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/EffekseerRendererGL.lib
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/EffekseerRendererGL_d.lib)
else()
    set_target_properties(Effekseer PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libEffekseer.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libEffekseer_d.a)
    set_target_properties(EffekseerRendererGL PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libEffekseerRendererGL.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libEffekseerRendererGL_d.a)
endif()
# EffekseerRendererGL 는 Effekseer 코어에 의존 — 링크 순서 보장
target_link_libraries(EffekseerRendererGL INTERFACE Effekseer)

```

- [ ] **Step 6: `game_deps` 에 Effekseer 추가**

`game_deps` 의 `target_link_libraries` 를 아래로 교체한다.

```cmake
target_link_libraries(game_deps INTERFACE
    box2d
    EffekseerRendererGL
    entt tweeny stb_extra)
```

- [ ] **Step 7: `<apps>/_deptest_/main.cpp` 의 `render()` 에 Effekseer 검증 코드 추가**

`#include <<box2d>/box2d.h>` 줄 다음에 추가:

```cpp
#include <Effekseer.h>
#include <EffekseerRendererGL.h>
#include <mutex>
```

`render()` 의 `glClearBufferfv(...)` 줄 **다음**에 추가 (GL 컨텍스트가 살아있는 render 시점에서 1회만 생성·해제):

```cpp
        static std::once_flag efkOnce;
        std::call_once(efkOnce, [] {
            auto manager = Effekseer::Manager::Create(8000);
            auto renderer = EffekseerRendererGL::Renderer::Create(
                8000, EffekseerRendererGL::OpenGLDeviceType::OpenGL3);
            std::printf("[deptest] effekseer manager=%p renderer=%p\n",
                        static_cast<void *>(manager.Get()),
                        static_cast<void *>(renderer.Get()));
        });
```

- [ ] **Step 8: 빌드 검증**

Run:
```bash
cmake --preset ninja
cmake --build --preset ninja --target _deptest_
```
Expected: PASS. `Effekseer::Manager::Create` / `EffekseerRendererGL::Renderer::Create` 심볼이 링크됨. 링크 에러 시: `OpenGLDeviceType` enum 명이 버전마다 다를 수 있으니 `include/Effekseer/EffekseerRendererGL.h` 에서 실제 enum 명을 확인해 맞춘다.

- [ ] **Step 9: 커밋**

```bash
git add <shell>/BuildExternLibs.sh lib/macos/libEffekseer.a lib/macos/libEffekseer_d.a lib/macos/libEffekseerRendererGL.a lib/macos/libEffekseerRendererGL_d.a include/Effekseer cmake/Dependency.cmake <apps>/_deptest_/main.cpp
git commit -m "[deps] : Effekseer 등록 (macOS 사전 빌드 + IMPORTED 타겟)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Windows 빌드 스크립트 + CI 확장

Windows 산출물(`*.lib`)은 이 머신(macOS)에서 만들 수 없다. Windows 빌드 스크립트와 CI 워크플로를 확장해 CI 가 산출물을 만들게 한다.

**Files:**
- Modify: `shell/BuildExternLibs.bat`
- Modify: `.github/workflows/build-extern-libs.yml`
- Modify: `.github/workflows/build-msvc.yml`

- [ ] **Step 1: 기존 CI 워크플로 내용 확인**

```bash
cat .github/workflows/build-extern-libs.yml
cat .github/workflows/build-msvc.yml
cat shell/BuildExternLibs.bat
```
Expected: 기존 sb7/glfw3 빌드 단계 구조 파악. 이후 Step 에서 동일 패턴으로 신규 라이브러리 단계를 추가한다.

- [ ] **Step 2: `shell/BuildExternLibs.bat` 에 Box2D·Effekseer 빌드 추가**

`BuildExternLibs.bat` 의 sb7 빌드 단계 다음, 완료 출력 앞에 Box2D·Effekseer 빌드 블록을 추가한다. `BuildExternLibs.sh` 의 `build_box2d` / `build_effekseer` 와 동일한 cmake 인자를 쓰되, 생성기는 기존 `.bat` 이 sb7 빌드에 쓰는 것과 동일하게 맞추고(MSVC 멀티컨피그면 `--config`), 산출물명은 `box2d.lib` / `box2d_d.lib` / `Effekseer.lib` / `Effekseer_d.lib` / `EffekseerRendererGL.lib` / `EffekseerRendererGL_d.lib`, 헤더는 `box2d` 및 `Effekseer` 디렉토리로 복사한다.

```bat
echo [+] Box2D v2.4.1 build...
cmake -S "%ROOT_DIR%\extern\box2d" -B "%BUILD_DIR%\box2d" ^
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    -DBOX2D_BUILD_TESTBED=OFF -DBOX2D_BUILD_UNIT_TESTS=OFF -DBOX2D_BUILD_DOCS=OFF
cmake --build "%BUILD_DIR%\box2d" --config Release
cmake --build "%BUILD_DIR%\box2d" --config Debug

echo [+] Effekseer build...
cmake -S "%ROOT_DIR%\extern\Effekseer" -B "%BUILD_DIR%\effekseer" ^
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    -DBUILD_GL=ON -DBUILD_VIEWER=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF ^
    -DBUILD_DX9=OFF -DBUILD_DX11=OFF -DBUILD_DX12=OFF -DBUILD_VULKAN=OFF
cmake --build "%BUILD_DIR%\effekseer" --config Release --target Effekseer EffekseerRendererGL
cmake --build "%BUILD_DIR%\effekseer" --config Debug   --target Effekseer EffekseerRendererGL
```

산출물 `.lib` 경로는 `for /r %%f in (box2d.lib) do ...` 또는 빌드 트리 내 `Release\`/`Debug\` 하위에서 찾아 `%LIB_DIR%` 로 `_d` 접미사를 붙여 복사한다 (sh 스크립트의 `find` 와 동일 의도).

- [ ] **Step 3: `.github/workflows/build-extern-libs.yml` 에 macOS 신규 라이브러리 단계 추가**

기존 sb7/glfw3 빌드 step 다음에, `extern/box2d` `extern/Effekseer` 서브모듈 체크아웃 + `BuildExternLibs.sh` 가 산출한 `libbox2d*.a` `libEffekseer*.a` `libEffekseerRendererGL*.a` 와 `include/box2d` `include/Effekseer` 를 업로드 아티팩트에 포함하는 step 을 추가한다. 서브모듈 체크아웃은 `actions/checkout` 의 `submodules: recursive` 로 처리.

- [ ] **Step 4: `.github/workflows/build-msvc.yml` 에 Windows 신규 라이브러리 빌드 단계 추가**

기존 sb7/glfw3 빌드 step 다음에 Step 2 의 `BuildExternLibs.bat` 호출(또는 동등한 cmake 단계)을 추가하고, 생성된 `*.lib` 6개와 헤더를 `actions/upload-artifact` 로 업로드한다. 이 아티팩트가 Windows 용 `lib/windows/` 체크인 소스가 된다.

- [ ] **Step 5: 워크플로 YAML 문법 검증**

Run:
```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/build-extern-libs.yml')); yaml.safe_load(open('.github/workflows/build-msvc.yml')); print('YAML OK')"
```
Expected: `YAML OK` 출력.

- [ ] **Step 6: 커밋**

```bash
git add shell/BuildExternLibs.bat .github/workflows/build-extern-libs.yml .github/workflows/build-msvc.yml
git commit -m "[ci] : Box2D/Effekseer Windows 빌드 스크립트 + CI 워크플로 확장

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

- [ ] **Step 7: (수동) Windows 산출물 체크인**

푸시 후 `build-msvc.yml` CI 실행이 끝나면 업로드된 아티팩트에서 `box2d.lib` `box2d_d.lib` `Effekseer.lib` `Effekseer_d.lib` `EffekseerRendererGL.lib` `EffekseerRendererGL_d.lib` 를 내려받아 `lib/windows/` 에 넣고 커밋한다. (이 머신에서는 검증 불가 — CI 그린 확인 후 진행.)

```bash
# CI 아티팩트 다운로드 후:
git add lib/windows/box2d.lib lib/windows/box2d_d.lib lib/windows/Effekseer.lib lib/windows/Effekseer_d.lib lib/windows/EffekseerRendererGL.lib lib/windows/EffekseerRendererGL_d.lib
git commit -m "[deps] : Box2D/Effekseer Windows 사전 빌드 산출물 체크인

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: FMOD 설치 가이드 문서

FMOD Core API 는 독점 SDK 라 서브모듈/등록 대상이 아니다. 설치 절차만 문서화한다.

**Files:**
- Create: `doc/api/FMOD_Setup.md`

- [ ] **Step 1: `doc/api/FMOD_Setup.md` 작성**

```markdown
# FMOD Core API 설치 가이드

FMOD Core API 는 독점 SDK 라 GitHub 서브모듈로 등록할 수 없다. 아래 절차로 수동 설치한다.
이 문서는 설치 방법만 다루며, 빌드 통합(`Dependency.cmake` 등록)은 별도 작업이다.

## 1. SDK 다운로드

- https://www.fmod.com 가입 후 로그인
- Download → FMOD Engine → 플랫폼별 패키지 내려받기
  - macOS: FMOD Engine (macOS)
  - Windows: FMOD Engine (Windows)
- 버전은 코드가 참조하는 API 버전(예: 2.03)에 맞춘다.

## 2. 헤더 배치

SDK 의 `api/core/inc/` 안의 헤더를 `include/fmod/` 로 복사한다.

```
include/fmod/fmod.h
include/fmod/fmod_common.h
include/fmod/fmod_errors.h
include/fmod/fmod_codec.h
include/fmod/fmod_dsp.h
include/fmod/fmod_output.h
```

코드에서는 `#include <fmod/fmod.h>` 로 포함한다.

## 3. 정적 vs 동적 라이브러리

FMOD Core 는 **동적 라이브러리만** 배포된다 (정적 링크 라이브러리는 제공되지 않음).
- 일반 빌드: `fmod` (최적화)
- 로깅 빌드: `fmodL` (FMOD_DEBUG 로그 — 개발 중 권장)

## 4. 라이브러리 배치

### macOS — SDK 의 `api/core/lib/`
```
lib/macos/libfmod.dylib
lib/macos/libfmodL.dylib
```

### Windows — SDK 의 `api/core/lib/x64/`
```
lib/windows/fmod_vc.lib    fmod.dll      (링크용 import lib + 런타임 DLL)
lib/windows/fmodL_vc.lib   fmodL.dll
```

## 5. Dependency.cmake 등록 (향후 작업 — 참고용 스텁)

```cmake
# ====== FMOD Core API (수동 설치 — doc/api/FMOD_Setup.md 참조) ======
if(EXISTS "${CMAKE_SOURCE_DIR}/include/fmod/fmod.h")
    add_library(fmod SHARED IMPORTED)
    if(WIN32)
        set_target_properties(fmod PROPERTIES
            IMPORTED_IMPLIB   ${LIB_DIR}/fmod_vc.lib
            IMPORTED_LOCATION ${LIB_DIR}/fmod.dll)
    else()
        set_target_properties(fmod PROPERTIES
            IMPORTED_LOCATION ${LIB_DIR}/libfmod.dylib)
    endif()
    target_link_libraries(game_deps INTERFACE fmod)
else()
    message(STATUS "FMOD 미설치 — 오디오 비활성. doc/api/FMOD_Setup.md 참조")
endif()
```

## 6. 동적 라이브러리 런타임 배치

`.dylib`/`.dll` 은 실행 파일과 같은 디렉토리에 있어야 한다.
챕터 `CMakeLists.txt` 의 POST_BUILD 단계에서 실행 파일 옆으로 복사한다.

```cmake
add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:fmod> $<TARGET_FILE_DIR:${CHAPTER_NAME}>)
```

## 7. 주의 — 라이선스 / .gitignore

- FMOD 라이브러리 파일(`*.dylib` / `*.dll`)은 프로젝트 `.gitignore` 에서 이미 무시된다.
- FMOD 라이선스상 SDK 재배포에 제약이 있으므로, 산출물을 공개 저장소에 커밋하지 말 것.
- 각 개발 환경에서 본 문서대로 개별 설치한다.
```

- [ ] **Step 2: 문서 존재 확인**

Run: `ls doc/api/FMOD_Setup.md`
Expected: 경로 출력.

- [ ] **Step 3: 커밋**

```bash
git add doc/api/FMOD_Setup.md
git commit -m "[doc] : FMOD Core API 설치 가이드 추가

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: CLAUDE.md 갱신 + 임시 타겟 정리

**Files:**
- Modify: `.claude/CLAUDE.md`
- Modify: `apps/CMakeLists.txt`

- [ ] **Step 1: `.claude/CLAUDE.md` — extern 서브모듈 목록 갱신**

`## Reference` 섹션의 `extern/sb7code/` 줄을 아래로 교체한다.

```markdown
- `extern/sb7code/` — SuperBible 7 원본 (Git 서브모듈, glfw3 + sb7 소스)
- `extern/box2d` (v2.4.1) / `extern/Effekseer` / `extern/entt` / `extern/tweeny` / `extern/stb` — 게임/엔진 라이브러리 서브모듈. 버전 추적·재빌드 소스용이며, 빌드는 `lib/`·`include/` 체크인 산출물에 의존. 재빌드는 `shell/BuildExternLibs.{sh,bat}`.
- `doc/api/FMOD_Setup.md` — FMOD Core API 수동 설치 가이드 (독점 SDK, 서브모듈 불가)
```

- [ ] **Step 2: `.claude/CLAUDE.md` — Dependency layer 섹션에 game_deps 설명 추가**

`### Dependency layer (cmake/Dependency.cmake)` 섹션의 마지막 줄(`- 헤더는 ${CMAKE_SOURCE_DIR}/include 에서 노출 ...`) 다음에 추가한다.

```markdown
- **`game_deps`** (INTERFACE) = Box2D + Effekseer + EffekseerRendererGL + EnTT + Tweeny + stb. 게임/엔진 챕터만 `project_deps` 와 함께 옵트인 링크 (`target_link_libraries(타겟 PRIVATE project_deps game_deps)`). 일반 챕터는 `project_deps` 만 링크해 물리/파티클 엔진을 끌어들이지 않는다.
```

- [ ] **Step 3: `apps/CMakeLists.txt` — `_deptest_` 비활성화, `chapter9` 복원**

Task 2 Step 3 에서 바꾼 두 줄을 원복한다.

```cmake
add_subdirectory(chapter9)
# add_subdirectory(_deptest_)
```

- [ ] **Step 4: 빌드 검증 — chapter9 정상 복원 확인**

Run:
```bash
cmake --preset ninja
cmake --build --preset ninja --target chapter9
```
Expected: PASS — `_deptest_` 비활성, chapter9 정상 빌드.

- [ ] **Step 5: 커밋**

```bash
git add .claude/CLAUDE.md apps/CMakeLists.txt
git commit -m "[doc] : CLAUDE.md 게임 라이브러리 반영 + _deptest_ 비활성화

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

> **참고:** `apps/_deptest_/` 디렉토리 자체는 향후 의존성 점검용으로 남겨둔다 (`apps/CMakeLists.txt` 에서 주석 처리로 비활성). 완전 삭제를 원하면 `git rm -r apps/_deptest_` 후 별도 커밋.

> **참고 — .gitignore:** `.claude/` 와 `doc/` 는 `.gitignore` 대상이다. `.claude/CLAUDE.md` 변경을 커밋하려면 `git add -f .claude/CLAUDE.md` 가 필요하다. 본 계획 문서·스펙 문서도 동일 (`doc/superpowers/`).

---

## 자기 검토 결과

**스펙 커버리지:** 서브모듈 등록(T1) · 헤더 온리(T3) · Box2D(T4) · Effekseer(T5) · Dependency.cmake/game_deps(T3·T4·T5) · 빌드 스크립트(T4·T5·T6) · CI(T6) · FMOD 가이드(T7) · CLAUDE.md(T8) — 스펙 전 항목이 태스크에 매핑됨. 누락 없음.

**플레이스홀더:** Effekseer 의 CMake 옵션명·헤더 경로는 버전 의존이라 T5 Step 1 에서 실제 확인 후 진행하도록 명시 — "TBD" 가 아니라 검증 가능한 실제 지시.

**타입 일관성:** 타겟명 `box2d` / `Effekseer` / `EffekseerRendererGL` / `entt` / `tweeny` / `stb_extra` / `game_deps` 가 T3~T8 전체에서 일관. `game_deps` 의 `target_link_libraries` 는 T3(3종)→T4(+box2d)→T5(+EffekseerRendererGL) 로 점증하며 최종 6멤버.
