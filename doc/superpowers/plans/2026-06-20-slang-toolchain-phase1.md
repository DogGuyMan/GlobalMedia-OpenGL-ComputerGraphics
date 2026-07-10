# Slang 툴체인 (Phase 1) Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `slangc` 를 빌드타임 셰이더 컴파일러로 CMake 에 배선하여, `simple.slang` 단일 소스에서 macOS GL 4.1 호환 GLSL(`simple.vs`/`simple.fs`) + WGSL + 리플렉션 JSON 을 빌드마다 자동 생성한다.

**Architecture:** Tool-only — `find_program(slangc)` + `add_custom_command`. 생성된 GLSL 은 `#version 450`/`layout(binding)`/`layout(...) buffer;` 를 CMake `-P` post-process 스크립트로 410 호환화. 생성물은 build 디렉토리에만(비커밋), POST_BUILD 로 실행파일 옆 `resources/shaders/` 에 overlay. 엔진 C++ 는 이 플랜에서 **변경하지 않음**(UBO 소비는 Phase 2 별도 플랜).

**Tech Stack:** CMake 3.24+, Slang `slangc` v2026.11 (`~/slang/bin`), GLSL 410, WGSL.

**정본 spec:** `doc/superpowers/specs/2026-06-20-slang-shader-migration-design.md`

**검증 모델 (중요):** 이 프로젝트는 그래픽스 도메인 + `no_auto_tests` 규칙 — 단위 테스트를 새로 만들지 않는다. 각 태스크의 검증은 **빌드 GREEN + 생성물 내용 확인(grep)** 으로 한다. 인-앱 비주얼 렌더 검증은 엔진 UBO 소비가 필요하므로 Phase 2 로 연기.

**커밋 규칙 (memory: user-parallel-git-and-builds):** 항상 `git commit <경로>` partial 커밋 (인덱스 전체 커밋 금지 — 사용자 staged 작업 보존). 빌드는 사용자 또는 에이전트가 명시 실행.

---

## 파일 구조

**생성:**
- `cmake/Slang.cmake` — `find_program(SLANGC_EXECUTABLE)` + `sjh_compile_slang()` 헬퍼 함수. root `CMakeLists.txt` 에서 include.
- `<cmake>/SlangPostProcess410.cmake` — `cmake -P` 스크립트. 생성 GLSL 을 410 호환으로 치환(version/binding/buffer 3종).
- `apps/_MyApp_/shaders_slang/simple.slang` — `simple.vs`+`simple.fs` 등가 Slang 소스 (VS `vsMain` + FS `fsMain`).

**수정:**
- `CMakeLists.txt` (root) — `include(cmake/Slang.cmake)` 추가.
- `apps/_MyApp_/CMakeLists.txt` — `simple.slang` 컴파일 + 생성물 overlay 배선.

**불변(이 플랜에서 손대지 않음):** `src/` 엔진 코드, 기존 `apps/_MyApp_/resources/shaders/*.vs|*.fs`(런타임은 계속 기존 GLSL 사용), `vcpkg.json`(nlohmann-json 이미 존재).

---

## Task 1: `<cmake>/SlangPostProcess410.cmake` — 410 호환 post-process 스크립트

**Files:**
- Create: `<cmake>/SlangPostProcess410.cmake`

생성 GLSL(`#version 450` + Vulkan식 `layout(binding)` + SSBO `layout(...) buffer;`)을 desktop GL 4.1 호환으로 치환하는 독립 `cmake -P` 스크립트. `-DSLANG_IN=<raw.glsl> -DSLANG_OUT=<final>` 인자.

- [ ] **Step 1: 스크립트 작성**

```cmake
# <cmake>/SlangPostProcess410.cmake
# slangc 의 GLSL 출력을 macOS OpenGL 4.1 (GLSL 410) 호환으로 치환한다.
# 사용: cmake -DSLANG_IN=raw.glsl -DSLANG_OUT=final.fs -P <cmake>/SlangPostProcess410.cmake
#
# 치환 3종 (정본 spec 3.1):
#   1) #version 450            -> #version 410 core
#   2) layout(binding = N)     단독 라인 제거 (UBO/텍스처 explicit binding 은 GLSL 420+)
#   3) layout(row_major) buffer; 라인 제거 (SSBO 기본 레이아웃, GLSL 430+ — 410 거부)
# binding 은 셰이더가 아니라 C++ 가 glUniformBlockBinding 으로 묶는다 (Phase 2).

if(NOT DEFINED SLANG_IN OR NOT DEFINED SLANG_OUT)
    message(FATAL_ERROR "SlangPostProcess410: SLANG_IN / SLANG_OUT 인자 필요")
endif()

file(READ "${SLANG_IN}" _content)

# 1) #version 450 -> 410 core
string(REPLACE "#version 450" "#version 410 core" _content "${_content}")

# 2) layout(binding = N) 단독 라인 제거 (뒤따르는 개행까지)
string(REGEX REPLACE "layout\\(binding = [0-9]+\\)\n" "" _content "${_content}")

# 3) layout(row_major) buffer; / layout(column_major) buffer; 라인 제거
string(REGEX REPLACE "layout\\((row|column)_major\\) buffer;\n" "" _content "${_content}")

file(WRITE "${SLANG_OUT}" "${_content}")
message(STATUS "[slang:410] ${SLANG_OUT}")
```

- [ ] **Step 2: 스크립트 단독 동작 검증 (PoC 산출물로)**

Run:
```bash
mkdir -p /tmp/pp && printf '#version 450\nlayout(row_major) uniform;\nlayout(row_major) buffer;\nlayout(binding = 0)\nlayout(std140) uniform B { vec4 x; } b;\nvoid main(){}\n' > /tmp/pp/in.glsl
cmake -DSLANG_IN=/tmp/pp/in.glsl -DSLANG_OUT=/tmp/pp/out.fs -P <cmake>/SlangPostProcess410.cmake
cat /tmp/pp/out.fs
```
Expected 출력:
```
#version 410 core
layout(row_major) uniform;
layout(std140) uniform B { vec4 x; } b;
void main(){}
```
(`#version 410 core`, `layout(binding=0)` 제거됨, `buffer;` 라인 제거됨, `layout(row_major) uniform;` 은 보존.)

- [ ] **Step 3: 커밋**

```bash
git add <cmake>/SlangPostProcess410.cmake
git commit <cmake>/SlangPostProcess410.cmake -m "[build] : Slang GLSL 410 호환 post-process 스크립트"
```

---

## Task 2: `cmake/Slang.cmake` — slangc 탐색 + 컴파일 헬퍼

**Files:**
- Create: `cmake/Slang.cmake`

`slangc` 실행파일을 `SLANG_ROOT`/PATH 에서 찾고, `.slang` → 타깃별 산출물을 만드는 `sjh_compile_slang()` 함수 정의. GLSL 타깃은 post-process 를 자동 체이닝.

- [ ] **Step 1: 작성**

```cmake
# cmake/Slang.cmake
# Slang 을 tool-only 로 통합 (find_program + add_custom_command). 라이브러리 링크 안 함.
# 정본 spec: doc/superpowers/specs/2026-06-20-slang-shader-migration-design.md

find_program(SLANGC_EXECUTABLE
    NAMES slangc
    PATHS
        $ENV{SLANG_ROOT}/bin
        $ENV{HOME}/slang/bin
    DOC "Slang shader compiler (slangc)")

if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR
        "slangc not found. Slang 을 설치하고 SLANG_ROOT 를 설정하거나 slangc 를 PATH 에 추가하세요. "
        "참고: doc/handoff SLANG_TOOLCHAIN_HANDOFF.md")
endif()
message(STATUS "Found slangc: ${SLANGC_EXECUTABLE}")

# sjh_compile_slang(<out_var> <input.slang> <entry> <stage> <target> <profile> <out_file>)
#   - out_var  : 최종 산출 파일 경로가 담길 변수 (PARENT_SCOPE)
#   - target   : glsl | wgsl  (glsl 은 410 post-process 자동 적용)
#   - profile  : glsl_410 등 (wgsl 은 "" 전달)
#   - out_file : 최종 산출 파일 절대경로 (예: .../simple.fs)
function(sjh_compile_slang OUT_VAR INPUT ENTRY STAGE TARGET PROFILE OUT_FILE)
    set(_profile_arg "")
    if(NOT PROFILE STREQUAL "")
        set(_profile_arg -profile ${PROFILE})
    endif()

    if(TARGET STREQUAL "glsl")
        # 1단계: slangc -> raw .glsl, 2단계: post-process -> OUT_FILE
        set(_raw "${OUT_FILE}.raw")
        add_custom_command(
            OUTPUT  ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders"
            COMMAND ${SLANGC_EXECUTABLE} ${INPUT}
                    -target glsl ${_profile_arg}
                    -entry ${ENTRY} -stage ${STAGE}
                    -o ${_raw}
            COMMAND ${CMAKE_COMMAND}
                    -DSLANG_IN=${_raw} -DSLANG_OUT=${OUT_FILE}
                    -P ${CMAKE_SOURCE_DIR}/cmake/SlangPostProcess410.cmake
            DEPENDS ${INPUT}
            COMMENT "[slang:glsl410] ${INPUT} (${STAGE}/${ENTRY}) -> ${OUT_FILE}"
            VERBATIM)
    else()
        add_custom_command(
            OUTPUT  ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders"
            COMMAND ${SLANGC_EXECUTABLE} ${INPUT}
                    -target ${TARGET} ${_profile_arg}
                    -entry ${ENTRY} -stage ${STAGE}
                    -o ${OUT_FILE}
            DEPENDS ${INPUT}
            COMMENT "[slang:${TARGET}] ${INPUT} (${STAGE}/${ENTRY}) -> ${OUT_FILE}"
            VERBATIM)
    endif()

    set(${OUT_VAR} ${OUT_FILE} PARENT_SCOPE)
endfunction()

# sjh_reflect_slang(<out_var> <input.slang> <entry> <stage> <out_json>)
#   리플렉션 JSON 생성 (Phase 2 의 C++ 바인딩 메타). 코드 생성은 -o /dev/null, -reflection-json 만 수확.
function(sjh_reflect_slang OUT_VAR INPUT ENTRY STAGE OUT_JSON)
    add_custom_command(
        OUTPUT  ${OUT_JSON}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders"
        COMMAND ${SLANGC_EXECUTABLE} ${INPUT}
                -target glsl -profile glsl_410
                -entry ${ENTRY} -stage ${STAGE}
                -reflection-json ${OUT_JSON} -o ${OUT_JSON}.ignore.glsl
        DEPENDS ${INPUT}
        COMMENT "[slang:refl] ${INPUT} (${STAGE}/${ENTRY}) -> ${OUT_JSON}"
        VERBATIM)
    set(${OUT_VAR} ${OUT_JSON} PARENT_SCOPE)
endfunction()
```

- [ ] **Step 2: root CMakeLists.txt 에 include 추가**

`CMakeLists.txt` (root) 에서 `add_subdirectory(apps)` 또는 `add_subdirectory(src)` **이전** 줄에 추가. 정확 위치: `cmake/Dependency.cmake` include 직후.

Modify: `CMakeLists.txt` — 기존 `include(cmake/Dependency.cmake)` 라인 바로 다음에:
```cmake
include(cmake/Slang.cmake)   # Slang tool-only 셰이더 컴파일러 (find_program + sjh_compile_slang)
```

- [ ] **Step 3: configure 가 slangc 를 찾는지 검증**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
export PATH="$HOME/slang/bin:$PATH"
cmake --preset ninja 2>&1 | grep -iE "Found slangc|slangc not found"
```
Expected: `-- Found slangc: /Users/escatrgot/slang/bin/slangc`

- [ ] **Step 4: 커밋**

```bash
git add cmake/Slang.cmake CMakeLists.txt
git commit cmake/Slang.cmake CMakeLists.txt -m "[build] : Slang tool-only 통합 (find_program + sjh_compile_slang)"
```

---

## Task 3: `apps/_MyApp_/shaders_slang/simple.slang` — PoC Slang 소스

**Files:**
- Create: `apps/_MyApp_/shaders_slang/simple.slang`

기존 `simple.vs`(uModel/uView/uProj) + `simple.fs`(baseColor) 등가. 전역 상수는 `ConstantBuffer` 로 묶어 UBO 화 (spec 저작 규칙).

- [ ] **Step 1: 작성**

```hlsl
// apps/_MyApp_/shaders_slang/simple.slang
// 기존 simple.vs + simple.fs 등가 (라이팅 무관 단색 출력).
// 저작 규칙(spec 3.1): 전역 상수/행렬은 ConstantBuffer 로 묶어 UBO 화.
// 생성물: simple.vs (vsMain) / simple.fs (fsMain) + simple.refl.json

struct CameraBlock
{
    float4x4 uModel;
    float4x4 uView;
    float4x4 uProj;
    float4   baseColor;
};
ConstantBuffer<CameraBlock> uCamera;

struct VSIn
{
    float3 aPos : POSITION;   // location 0 — SJH::Vertex 레이아웃 일치
};

[shader("vertex")]
float4 vsMain(VSIn input) : SV_Position
{
    return mul(uCamera.uProj, mul(uCamera.uView, mul(uCamera.uModel, float4(input.aPos, 1.0))));
}

[shader("fragment")]
float4 fsMain() : SV_Target
{
    return uCamera.baseColor;
}
```

- [ ] **Step 2: slangc 로 3 타깃 수동 컴파일 검증**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
export PATH="$HOME/slang/bin:$PATH"
S=apps/_MyApp_/shaders_slang/simple.slang
slangc $S -target glsl -profile glsl_410 -stage vertex   -entry vsMain -o /tmp/simple.vs.raw && echo "[VS glsl OK]"
slangc $S -target glsl -profile glsl_410 -stage fragment -entry fsMain -o /tmp/simple.fs.raw && echo "[FS glsl OK]"
slangc $S -target wgsl -stage vertex   -entry vsMain -o /tmp/simple.vert.wgsl && echo "[VS wgsl OK]"
slangc $S -reflection-json /tmp/simple.refl.json -target glsl -profile glsl_410 -stage fragment -entry fsMain -o /tmp/ignore.glsl && echo "[refl OK]"
grep -E "uModel|uView|uProj|baseColor" /tmp/simple.refl.json | head
```
Expected: 4개 `[... OK]` + 리플렉션에 `uModel/uView/uProj/baseColor` 멤버 표시.

- [ ] **Step 3: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/simple.slang
git commit apps/_MyApp_/shaders_slang/simple.slang -m "[shader] : simple.slang — PoC Slang 소스 (simple.vs/fs 등가)"
```

---

## Task 4: `apps/_MyApp_/CMakeLists.txt` — simple.slang 빌드 배선

**Files:**
- Modify: `apps/_MyApp_/CMakeLists.txt` (POST_BUILD 리소스 복사 블록 `:60-66` 부근)

`simple.slang` → `simple.vs`/`simple.fs`/`simple.vert.wgsl`/`simple.frag.wgsl`/`simple.refl.json` 생성, 빌드 의존성으로 묶고, 생성물을 실행파일 옆 `resources/shaders/` 로 overlay 복사.

- [ ] **Step 1: 셰이더 생성 + 의존 + overlay 배선 추가**

`apps/_MyApp_/CMakeLists.txt` 의 마지막 `if(EXISTS ... /resources)` POST_BUILD 블록 **바로 위**에 삽입:

```cmake
# ______ Slang 셰이더 컴파일 (Phase 1 — simple.slang PoC) ______
set(SLANG_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders)
set(SIMPLE_SLANG  ${CMAKE_CURRENT_SOURCE_DIR}/shaders_slang/simple.slang)

sjh_compile_slang(_SIMPLE_VS   ${SIMPLE_SLANG} vsMain vertex   glsl glsl_410 ${SLANG_GEN_DIR}/simple.vs)
sjh_compile_slang(_SIMPLE_FS   ${SIMPLE_SLANG} fsMain fragment glsl glsl_410 ${SLANG_GEN_DIR}/simple.fs)
sjh_compile_slang(_SIMPLE_WVS  ${SIMPLE_SLANG} vsMain vertex   wgsl ""       ${SLANG_GEN_DIR}/simple.vert.wgsl)
sjh_compile_slang(_SIMPLE_WFS  ${SIMPLE_SLANG} fsMain fragment wgsl ""       ${SLANG_GEN_DIR}/simple.frag.wgsl)
sjh_reflect_slang(_SIMPLE_REFL ${SIMPLE_SLANG} fsMain fragment ${SLANG_GEN_DIR}/simple.refl.json)

add_custom_target(${CHAPTER_NAME}_shaders ALL
    DEPENDS ${_SIMPLE_VS} ${_SIMPLE_FS} ${_SIMPLE_WVS} ${_SIMPLE_WFS} ${_SIMPLE_REFL})
add_dependencies(${CHAPTER_NAME} ${CHAPTER_NAME}_shaders)
```

그리고 기존 POST_BUILD 리소스 복사 블록 **다음**(닫는 `endif()` 뒤)에 생성물 overlay 복사 추가:

```cmake
# 생성된 Slang 셰이더를 실행파일 옆 resources/shaders 로 overlay (소스 resources 복사 위에 덮어씀)
add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders
        $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources/shaders
    COMMENT "${CHAPTER_NAME} 생성 Slang 셰이더 overlay")
```

- [ ] **Step 2: 빌드 — 셰이더 생성 + 산출물 검증**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
export PATH="$HOME/slang/bin:$PATH"
cmake --preset ninja
cmake --build --preset ninja --target _MyApp__shaders 2>&1 | tail -10
echo "=== 생성된 simple.vs 첫 줄 (410 이어야) ==="
head -1 build_ninja/apps/_MyApp_/slang_generated/shaders/simple.vs
echo "=== layout(binding) 잔존? (0 이어야) ==="
grep -c "layout(binding" build_ninja/apps/_MyApp_/slang_generated/shaders/simple.vs build_ninja/apps/_MyApp_/slang_generated/shaders/simple.fs
echo "=== refl.json 멤버 ==="
grep -E "uModel|baseColor" <build_ninja>/apps/_MyApp_/slang_generated/shaders/simple.refl.json | head -3
```
Expected:
- `simple.vs` 첫 줄 = `#version 410 core`
- `layout(binding` 카운트 = `0` (양 파일)
- refl.json 에 `uModel`/`baseColor` 멤버 존재

- [ ] **Step 3: 전체 빌드 GREEN (엔진 무변경 — 기존 GLSL 로 런타임 동작 유지)**

Run:
```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5
echo "EXIT=$?"
echo "=== overlay 확인 ==="
ls build_ninja/apps/_MyApp_/_MyApp_ && head -1 build_ninja/apps/_MyApp_/resources/shaders/simple.vs
```
Expected: 빌드 성공(`EXIT=0`), 실행파일 존재, overlay 된 `resources/shaders/simple.vs` 첫 줄 = `#version 410 core`.
※ 런타임은 아직 기존 loose-uniform 경로 — 이 overlay 된 simple.vs(UBO) 를 **소비하지 않음**. 소비는 Phase 2. 빌드 GREEN 만 확인.

- [ ] **Step 4: 커밋**

```bash
git add apps/_MyApp_/CMakeLists.txt
git commit apps/_MyApp_/CMakeLists.txt -m "[build] : _MyApp_ 에 simple.slang Slang 컴파일 배선 (glsl410/wgsl/refl)"
```

---

## Task 5: 함정 가드 + 문서 포인터

**Files:**
- Modify: `apps/_MyApp_/CMakeLists.txt` (Apple Gatekeeper 가드 — 선택)

slangc 다운로드 바이너리의 macOS quarantine 으로 CI/타 머신에서 첫 실행 차단 가능. configure 단계 안내.

- [ ] **Step 1: Slang.cmake 에 quarantine 경고 추가**

`cmake/Slang.cmake` 의 `message(STATUS "Found slangc...")` 다음에:
```cmake
if(APPLE)
    # 다운로드 바이너리 quarantine 시 첫 실행이 Gatekeeper 에 막힐 수 있음 (spec 함정).
    execute_process(
        COMMAND xattr -dr com.apple.quarantine "${SLANGC_EXECUTABLE}"
        ERROR_QUIET RESULT_VARIABLE _xattr_rv)
endif()
```

- [ ] **Step 2: 빌드가 여전히 GREEN 인지 재확인**

Run:
```bash
cmake --preset ninja 2>&1 | grep -iE "Found slangc"
cmake --build --preset ninja --target _MyApp__shaders 2>&1 | tail -3
echo "EXIT=$?"
```
Expected: slangc 발견 + 셰이더 타깃 빌드 성공.

- [ ] **Step 3: 커밋**

```bash
git add cmake/Slang.cmake
git commit cmake/Slang.cmake -m "[build] : slangc macOS quarantine 자동 해제 (configure 가드)"
```

---

## 다음 플랜 (이 플랜 범위 밖 — 별도 작성)

- **Phase 2 — 엔진 UBO 소비 + R1 비주얼:** `nlohmann-json` 으로 `simple.refl.json` 파싱 → `UniformBlock` (UBO RAII + std140 staging + `glUniformBlockBinding`) → 렌더 패스가 slang program 에 `uModel/uView/uProj` 를 UBO 로 송신. **bullet_factory 의 simple program 을 생성 GLSL 로 교체해 인-앱 렌더 + R1(행렬 row_major 전치) 실측 확정.** 이 플랜의 산출물(생성 simple.vs/fs/refl.json)을 소비.
- **Phase 3 — 26종 일괄 이주:** Phase 2 에서 확정한 패턴으로 phong/postprocess(9)/billboard/skybox/healthbar/transparent 그룹별 이주. 공유 VS·라이팅 헬퍼는 Slang `import` 모듈로 단일화.

---

## Self-Review

**Spec coverage (Phase 1 한정):**
- spec §2 tool-only → Task 2 `find_program` ✓
- spec §3.1 post-process 3종 → Task 1 ✓ / 저작 규칙(combined Sampler2D, ConstantBuffer) → Task 3 (simple 은 샘플러 없음, ConstantBuffer 적용) ✓
- spec §3.2 reflection JSON → Task 4 `sjh_reflect_slang` ✓ (소비는 Phase 2)
- spec §4.1 디렉토리(shaders_slang/) → Task 3 ✓
- spec §4.2 cmake/Slang.cmake + SlangPostProcess410 → Task 1/2 ✓
- spec §5 Phase 1 = 이 플랜 전체 ✓. Phase 0 비주얼/R1 + Phase 2/3 → 다음 플랜 (명시)
- spec §7 R4 macOS 우선 → Task 5 quarantine 가드 ✓

**Placeholder scan:** 모든 코드 블록 실내용. TBD 없음. ✓

**Type/이름 일관성:** `sjh_compile_slang`(7인자: out/input/entry/stage/target/profile/out_file) Task 2 정의 ↔ Task 4 호출 일치. `sjh_reflect_slang`(5인자) 일치. `SLANG_IN/SLANG_OUT` Task1↔Task2 일치. `${CHAPTER_NAME}_shaders` 타깃명 일관. ✓

**갭:** 없음 (Phase 1 범위 내). R1·UBO·bulk 는 의도적 후속 플랜.
