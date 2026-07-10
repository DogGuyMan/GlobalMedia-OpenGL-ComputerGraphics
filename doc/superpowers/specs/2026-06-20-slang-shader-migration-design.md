# Slang 셰이더 마이그레이션 설계 (GLSL + WGSL 단일 소스)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 작성: 2026-06-20 · 대상: `apps/_MyApp_` 활성 셰이더 26종 · 상태: 설계 합의 (PoC 게이트 통과)

## 1. 목표

모든 셰이더를 **Slang 단일 소스(`.slang`)** 로 작성하고 `slangc` 로 빌드타임 트랜스파일한다.
하나의 소스에서 **(a) macOS GL 4.1 호환 GLSL** 과 **(b) WebGPU 용 WGSL** (+ 향후 Metal MSL) 을 동시에 생성하여
현재 desktop OpenGL 데모를 유지하면서 WebGPU/WebGL 포팅의 토대를 만든다.

핵심 사용자 결정 (2026-06-20):
- 저작은 Slang, 출력은 GLSL (+ WGSL). 데스크톱 GL 4.1 은 계속 동작.
- 빌드 통합은 **CMake 빌드타임 자동 컴파일**.
- 바인딩 모델은 **UBO 전면 도입** (loose `glUniform*` → uniform buffer).

## 2. 도구 역할 (Tool-only) — 확정

`slang` 은 **라이브러리(API 링크)가 아니라 빌드타임 컴파일러(tool)** 로만 쓴다.
`slangc` 를 `glslangValidator`/`dxc`/`protoc` 와 동급의 외부 컴파일러로 취급.

| | 채택 | 제외 |
|---|---|---|
| 대상 | `slangc` 실행파일 | `slang::slang` dylib 링크 |
| CMake | `find_program` + `add_custom_command` | `find_package` / `target_link_libraries` |
| 헤더 | 불필요 | `<slang.h>` |
| rpath/dylib 배포 | 불필요 | 필요 |

- 설치: GitHub Release zip `v2026.11` → `~/slang/bin/slangc` (PATH 등록, `SLANG_ROOT=$HOME/slang`).
- vcpkg 의존성 그래프 **바깥** (충돌 없음). 단일 `slangc` 바이너리에 GLSL/WGSL/Metal/SPIR-V 백엔드 내장 — 타깃은 다운로드가 아닌 `-target` 플래그로 결정.
- 함정: 버전 확인은 `slangc -v` (`--version` 미지원), macOS Gatekeeper `xattr -dr com.apple.quarantine ~/slang`, GL 4.1 상한이라 `glsl_410` 고정 (SPIR-V 경유 GL 4.6 불가).

## 3. PoC 실측 결과 (게이트 — 통과)

`simple.vs/fs` 등가 셰이더(`uModel/uView/uProj` + `aPos` + `baseColor`)를 `.slang` 으로 작성해 실측.

| 항목 | 결과 | 함의 |
|---|---|---|
| 전역 유니폼 이름 보존 | **불가** | Slang 은 전역/`ConstantBuffer` 를 항상 std140 **UBO 블록**으로 래핑 (`block_Globals_0`), 멤버명 맹글 (`uModel_0`). `glGetUniformLocation("uModel")` → -1. loose `SetMat4("uModel")` 전부 무력. |
| 출력 `#version` | **450 고정** | `-profile glsl_410` 줘도 450. 낮추는 플래그 없음. macOS GL 4.1 은 450 거부. |
| UBO `layout(binding=N)` | 항상 방출 | 블록 explicit binding 은 GLSL **420+** 전용 → 410 불허. |
| attribute location | **보존** ✓ | `layout(location=0) in vec3 input_aPos_0;`. VAO 는 location 인덱스 바인딩이라 이름 맹글 무관. |
| 행렬 | Slang 내부 처리 | GLSL=`row_major`, WGSL=`ColMajor` storage. `-matrix-layout-column-major` 줘도 GLSL 은 `row_major`+`vec*matrix` 유지(플래그 무효). **C++ UBO 업로드의 row_major 정합은 빌드 실측 필요(미해결 R1).** |
| 샘플러 | **combined 시 loose 보존** ✓ | combined `Sampler2D` 사용 시 `uniform sampler2D matDiffuse_0;` (UBO 아님, 410 합법, `glUniform1i` 로 unit). 이름만 맹글. ※ `Texture2D`+`SamplerState`(분리)는 Vulkan식 `sampler2D(tex,samp)` 생성자 방출 → **410 불허**. |
| 같은 소스 → WGSL | **정상** ✓ | `@binding(0) @group(0) var<uniform>` + `@location(0)`. combined Sampler2D 도 WGSL 에선 `texture_2d`+`sampler` 자동 분리. |

**결론:** "slang→glsl 드롭인" 은 기본 출력으론 불가. 그러나 **GL 4.1 도 UBO 를 지원**(uniform block 은 GLSL 140 코어)하므로,
막는 건 표기 몇 종 뿐이고 **얇은 post-process + 저작 규칙** 으로 해소된다. → 하나의 `.slang` 이 GL 4.1 GLSL + WGSL 둘 다 커버.

### 3.1 GL 4.1 호환 — post-process + 저작 규칙 (확정)
**post-process (생성 GLSL 치환 3종):**
1. `#version 450` → `#version 410 core`
2. `layout(binding = N)` 단독 라인 제거 (UBO 블록·텍스처 binding)
3. `layout(row_major) buffer;` 라인 제거 (SSBO 기본 레이아웃, GLSL 430+ — 410 거부)

→ 결과 GLSL 은 410 합법. UBO binding 은 셰이더가 아니라 **C++ 에서 `glUniformBlockBinding`** 로 묶는다.
구현은 크로스플랫폼 위해 **CMake `-P` 스크립트**(`SlangPostProcess410.cmake`) — `sed` 비의존.

**저작 규칙 (R3 실측 도출):**
- 텍스처는 반드시 combined **`Sampler2D`/`Sampler2DArray`/`SamplerCube`** 사용 (분리 `Texture2D`+`SamplerState` 금지) → GLSL `uniform sampler2D` (410 합법) + WGSL 정상.
- 전역 상수/행렬/벡터는 `ConstantBuffer<Block>` 로 묶어 UBO 화. 샘플러는 UBO 밖 loose.

### 3.2 리플렉션 기반 바인딩 — JSON 런타임 파싱 (확정)
맹글된 멤버명(`uModel_0`)·샘플러명(`matDiffuse_0`)에 의존하지 않는다.
**`slangc -reflection-json <path>`** 로 블록/멤버 offset·sampler binding 인덱스를 추출(실측: `g`→slot0, 멤버 offset 0/64/128, `matDiffuse`→slot1).
**vcpkg `nlohmann-json` 추가** → 엔진이 로드 시 `*.refl.json` 을 파싱해 std140 struct offset + sampler unit 을 자동 매핑 (사용자 결정 2026-06-20).

## 4. 아키텍처

### 4.1 디렉토리 / 데이터 흐름
```
apps/_MyApp_/shaders_slang/
    *.slang                         (program 당 1개: VS+FS, SSOT, git 커밋)
    <common>/postprocess_common.slang (공유 VS — import 모듈)
    <common>/lighting.slang           (공유 Phong 헬퍼 — import 모듈)
        │  slangc -target glsl  (+410 post-process)
        ├─► build_ninja/.../resources/shaders/*.vs|*.fs   (파생물, 비커밋)
        │  slangc -target wgsl
        ├─► build_ninja/.../resources/shaders/*.wgsl       (향후 WebGPU)
        +  slangc -reflection-json → *.refl.json           (C++ 바인딩 메타, 런타임 파싱)
```
- **program 당 1 `.slang`** (VS `vsMain` + FS `fsMain` 한 파일). 공유 VS(postprocess 9종)·Phong 헬퍼(중복 3파일)는 Slang **`import` 모듈**로 단일화 (사용자 결정 2026-06-20). Slang 모듈 시스템 실익.
- 생성 GLSL 은 build dir 에만, git 엔 `.slang` 만. 기존 POST_BUILD `copy_directory resources/` 와 공존(생성물은 이미 build dir 의 resources 하위로 방출).

### 4.2 CMake 모듈 `cmake/Slang.cmake`
- `find_program(SLANGC_EXECUTABLE NAMES slangc PATHS $ENV{SLANG_ROOT}/bin $ENV{HOME}/slang/bin)` — 미발견 시 FATAL_ERROR + 안내.
- `sjh_compile_slang(<out_var> <input.slang> <entry> <stage> <target> [profile])`:
  - `add_custom_command` 으로 `slangc … -o <build>/shaders/<target>/<name>_<stage>.<ext>`.
  - target=glsl 시 후속 `COMMAND ${CMAKE_COMMAND} -DIN=… -DOUT=… -P <cmake>/SlangPostProcess410.cmake` (version+binding 치환).
  - `DEPENDS <input>` 로 `.slang` 변경 시 자동 재컴파일.
- `add_custom_target(myapp_shaders ALL DEPENDS …)` + `add_dependencies(_MyApp_ myapp_shaders)`.

### 4.3 엔진 UBO 지원 (loose → UBO 이행)
현 바인딩 경계: `src/program/program_uniforms.cpp` 의 `Uniforms::SetMat4/Vec*/Float/Int` (이름 기반 `glUniform*`).
도입:
- `UniformBlock` 추상 — UBO 핸들 + std140 staging buffer + `glBindBufferBase`. 프로그램 링크 후 `glGetUniformBlockIndex` → `glUniformBlockBinding(prog, idx, point)`.
- 멤버 set 은 이름이 아니라 리플렉션 offset 으로 staging buffer 에 기록 후 `glBufferSubData`.
- 샘플러: 리플렉션 binding 순서대로 `glUniform1i(loc, unit)` (loc 은 맹글명 1회 조회 또는 리플렉션 location).
- 영향 지점: `LightUniformDispatcher`(라이트 struct 송신), `Material` property block, `SceneRenderer` 의 `uModel/uView/uProj` 송신. **가장 큰 변경 면적.**

## 5. 단계 (Phase)

- **Phase 0 — PoC 확장 (1 셰이더 쌍 풀 왕복):** `simple.slang` → 410 GLSL → **실제 `_MyApp_` 빌드/육안 검증** (UBO 업로드 + 행렬 R1 정합 확정). 여기 통과해야 일괄 이주 진입.
- **Phase 1 — CMake 배선:** `cmake/Slang.cmake`(`find_program` + `sjh_compile_slang`) + `<cmake>/SlangPostProcess410.cmake`(치환 3종) 작성, `_MyApp_` 에 `simple.slang` 만 연결. 일반 빌드가 slangc 호출 → 410 GLSL 생성 확인.
- **Phase 2 — 엔진 UBO 경로:** vcpkg `nlohmann-json` 추가, `UniformBlock` + `*.refl.json` 런타임 파싱 바인딩. 기존 loose 경로와 **공존**(셰이더별 점진 전환), 전환 완료 셰이더부터 UBO.
- **Phase 3 — 26종 일괄 이주:** 그룹별(simple / phong(tex·albedo·lighting) / postprocess 9종 / billboard / skybox / healthbar / transparent). 공유 VS·Phong 헬퍼는 Slang `import` 모듈로 단일화. 텍스처는 combined `Sampler2D` 규칙 적용.
- **(선택) Phase 4 — WGSL/Metal 타깃 활성화 + CI(build-msvc.yml) slangc 설치 + 죽은 `resources/shader/` 12종 + 미사용 `PATH_SHADER_*` 상수 정리.**

## 6. 범위

- **In:** `apps/_MyApp_/resources/shaders/` 활성 26종.
- **Out:** 루트 `resources/shader/` 12종 — `src/common/constants.h` 의 `PATH_SHADER_*` 로만 선언, 활성 코드 미참조 = 죽은 코드 (Phase 4 삭제 후보).

## 7. 미해결 리스크

- **R1 (행렬, 미해결):** GLSL `row_major` UBO 에 vmath mat4(GL 관례 column-major) 업로드 시 정합/전치 필요 여부 — Phase 0 빌드 실측으로 확정 (`-matrix-layout-column-major` 무효 확인됨, C++ 업로드 측 전치 또는 std140 매핑으로 대응).
- **R2 (샘플러, 해결):** combined `Sampler2D` → loose `uniform sampler2D` 보존(410 합법). 맹글명은 리플렉션으로 unit 매핑. phong(diffuse/specular)·billboard(atlas/dissolve) 다중 샘플러는 리플렉션 slot 순서로.
- **R3 (post-process 취약성, 부분):** 치환 3종으로 simple·복잡(16-light array+textureSize) 검증 완료. 그룹별 이주 중 새 450-전용 구문 방출 모니터(완화).
- **R4 (CI/타 머신, 범위조정):** slangc 가 PATH/`SLANG_ROOT` 의존 + 생성물 비커밋 → 빌드 머신마다 slangc 필요. **macOS 로컬 우선, CI(build-msvc.yml)·Windows slangc 설치는 Phase 4 로 연기** (사용자 결정 2026-06-20).

## 8. 결정 로그

| # | 결정 | 근거 |
|---|---|---|
| D1 | Slang = tool-only | 핸드오프 확정. rpath/링크/ABI 회피, 외부 컴파일러 취급. |
| D2 | 빌드타임 CMake codegen, 생성물 비커밋 | 단일 SSOT(.slang), 동기화 위험 0. vcpkg 전이 기조와 일관. |
| D3 | UBO 전면 도입 | Slang GLSL/WGSL 둘 다 UBO 강제 + WebGPU 최종목표 정방향. loose-uniform 보존 플래그 부재(실측). |
| D4 | GL 4.1 호환은 post-process(version+binding) | GL 4.1 도 UBO 지원, 막는 건 표기 2종뿐(실측). |
| D5 | 리플렉션 JSON 으로 바인딩 | 맹글 이름 의존 제거. |
| D6 | 26종만 In, 루트 12종 Out | 루트 셰이더 활성 코드 미참조(죽은 코드) 확인. |
| D7 | 리플렉션 소비 = nlohmann-json 런타임 파싱 | 자동 동기화·견고. vcpkg 의존성 1줄 추가 수용. |
| D8 | program 당 1 .slang + import 모듈 | 공유 VS 9중복·라이팅 3중복 단일화. Slang 모듈 실익. |
| D9 | 텍스처는 combined `Sampler2D` | 분리 Texture2D+SamplerState 는 410 불허(실측). |
| D10 | macOS 우선, CI/Windows Phase 4 연기 | 범위·위험 최소. 동작 확정 후 CI 배선. |
| D11 | Phase 2 UBO 블록 3분할 | per-frame(view/proj)+per-draw(model)+per-material(baseColor). 갱신 빈도별 분리, 엔진 정통. |
| D12 | UBO 식별 = Program GL introspection | `glGetActiveUniformBlock*`/`glGetActiveUniformsiv` 로 블록·offset 조회. **런타임 nlohmann-json 불용**(GL 이 offset 제공) — D7 은 이 경로에서 미사용(refl.json 은 빌드 산출물로만 유지). 블록명 `block_<Struct>_0` 정규화로 매핑. |
| D13 | R1 비전치 먼저 | transpose 없이 구현 후 육안 확인. 틀리면 std140 기록 시 전치 추가. |
