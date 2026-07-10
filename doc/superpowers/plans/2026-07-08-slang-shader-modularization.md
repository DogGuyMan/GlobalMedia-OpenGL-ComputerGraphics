# Slang 셰이더 모듈화 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> ⚠️ 이 문서는 `doc/` 하위 = **gitignored 로컬 전용**. 커밋 대상 아님.
> ⚠️ 기준 체크아웃 = `755faa7`. 롤백된 `d71e2ff`(reflog 생존)에 postfx_common·billboard_common 실증 코드 있음 — 본 플랜이 그 코드를 인용.

**Goal:** 18개 Slang 셰이더의 반복 코드를 5개 모듈(기존 `phong_lighting` 1 + 신규 4: `billboard_common` / `texture_atlas` / `postfx_common` / `image_processing`)로 추출해 SRP·DRY를 회복한다.

**Architecture:** Slang `module` + `public` 함수 + 소비 셰이더의 `import`. 모듈 파일은 CMake 등록 불필요 — `slang_compile.py`가 `-I <parent>`로 sibling `.slang`을 import 해석(phong_lighting 선례). entry point(`vsMain`/`fsMain`)는 각 파일 로컬 유지, 공유는 함수·struct뿐. GL state(RenderStateBlock)는 C++ SSoT — 셰이더는 프래그먼트/버텍스 로직만.

**Tech Stack:** Slang → GLSL 410 (macOS OpenGL 4.1 Core), `scripts/slang_compile.py` post-process 파이프라인, CMake/Ninja, `_MyApp_` 타겟.

---

## Context & Decision Log (우리 분석 + git 실측 근거)

| ID | 결정 | 근거 |
|----|------|------|
| D-1 | entry point(`vsMain`/`fsMain`)는 모듈로 옮기지 않고 각 파일 로컬 유지. 몸통 로직만 공유 함수화 | phong_lighting 선례. d71e2ff `postfx_common` 실증 |
| D-4 | billboard 상위 합성(worldPos 조립)은 모듈에 넣지 않음 — billboard_atlas=flip/roll vs healthbar=headOffset 으로 합성 순서 상이. 저수준 3함수만 | d71e2ff `billboard_common` 실증 |
| D-atlas-SSoT | `texture_atlas` 는 rect 를 grid 로부터 GPU 재도출하지 않음 — CPU `SJH::sprite::ComputeUVRect` 가 SSoT. 셰이더는 `min+localUV*size` 적용만 | uniform_atlas.cpp:33-34 + BitmapFont 도 같은 CPU 모델 의존 |
| D-toolchain | `postfx_common` 공유 VS 도입 시 Slang 이 구조체 brace-init(GLSL420) 을 내 macOS GL410 런타임 거부(program=null). `slang_compile.py` 에 `normalize_struct_initializers` 선행 추가 필수 | d71e2ff slang_compile.py +20줄 실증. `[[slang-glsl410-traps]]` 4번째 트랩 |
| D-spike | `image_processing` 은 sampler-as-param(`TexelSize(Sampler2D)`) + 배열-param(`Sample3x3 out c[9]`, `Dot9 float[9]`) 로 전례 없는 위험 — 첫 소비처(sobel)를 카나리로 런타임 검증 후 나머지 진행 | d71e2ff 가 image_processing 을 시도조차 안 함(커밋 메시지와 불일치) |
| D-scope | `simple`/`simple_texture`/`transparent` 는 완전 로컬 유지(FrameBlock/DrawBlock 공유는 사용자 스코프 제외). `matrix_skybox` 의 회전 VS·SphericalUV·rain FS, `fog` 의 fogFactor, `depth_debug` 의 linearize 는 단일사용 로컬 유지 | 사용자 확정 |

**핵심 위험(런타임에서만 발현):** macOS GL410 트랩 4종은 전부 *빌드 GREEN 이어도 런타임 program=null* 로만 나타난다(`[[slang-varying-name-mismatch-macos]]`). 따라서 **모든 태스크의 합격 기준 = 빌드 GREEN + 런타임 육안(program=null 로그 없음)**. 빌드만으로 합격 판정 금지.

---

## File Structure

**신규 생성 (4 모듈 파일 — CMake 등록 불필요):**
- `apps/_MyApp_/shaders_slang/modules/billboard_common.slang` — `ExtractCameraAxes` / `ExtractModelCenter` / `ExtractModelAxisScale`
- `apps/_MyApp_/shaders_slang/modules/texture_atlas.slang` — `AtlasSubUV` / `AtlasCell`
- `apps/_MyApp_/shaders_slang/modules/postfx_common.slang` — `VSIn` / `VSOut` / `ComputeScreenQuadVSOut`
- `apps/_MyApp_/shaders_slang/modules/image_processing.slang` — `Luminance` / `GaussianWeight` / `TexelSize` / `kOffset3x3` / `Sample3x3` / `Dot9Color` / `Dot9`

**수정 (툴체인 1 + 소비 셰이더 14):**
- `scripts/slang_compile.py` — `normalize_struct_initializers` 추가 (post-process 7번째)
- billboard_common 소비: `healthbar.slang`, `billboard_atlas.slang`
- texture_atlas 소비: `billboard_atlas.slang`, `matrix_skybox.slang`
- postfx_common 소비: `invert`, `passthrough`, `gamma`, `sharpening`, `grayscale_vignetting`, `blurring`, `depth_debug`, `bloom`, `sobel`, `fog` (10)
- image_processing 소비: `sharpening`, `grayscale_vignetting`, `blurring`, `bloom`, `sobel` (5)

**손대지 않음:** `simple`, `simple_texture`, `transparent`, `phong`, `phong_lighting`, `apps/_MyApp_/shaders_slang/CMakeLists.txt`, 모든 C++/src, 커밋.

---

## Verification Recipe (모든 태스크 공통 — 태스크마다 참조)

**빌드:**
```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -30
```
기대: 컴파일 에러 없이 종료(`ninja: ... Error` / `slangc 비-0 종료` 부재). clangd `gl3w.h not found` 는 거짓 진단 — 무시.

**트랩 사전점검(결정적, 생성 GLSL 대상):** 빌드 후 생성물에 정규화 안 된 GLSL420 brace-init 잔존 검사:
```bash
grep -rn "= {" build_ninja/apps/_MyApp_/shaders_slang/generated/ ; echo "exit=$?"
```
기대: **매치 0줄**(`exit=1`). 매치가 있으면 brace-init 이 새 나온 것 — `normalize_array_initializers`/`normalize_struct_initializers` 가 못 잡은 케이스이니 런타임 program=null 위험.

**★ 골든 이미지 게이트 (주 합격 게이트, OpenCV 5%):**
```bash
cmake --preset ninja -DENABLE_TESTING=ON              # 1회만 (테스트 활성화)
cmake --build --preset ninja --target tests           # _MyApp_ + golden_compare 빌드
ctest --test-dir build_ninja -R "golden" --output-on-failure
```
기대: `golden_capture`(=_MyApp_ 를 SJH_GOLDEN_CAPTURE=1 로 실행, 고정-dt 180프레임 후 3 PNG 캡처+자동종료) + `golden_full`/`golden_no_imgui`/`golden_skybox` 3 케이스 **PASS**.
- 판정: `diffFraction <= 0.05` (5% 픽셀 예산). 단 `kChannelDiffThreshold=0` 이라 **1 LSB 차이도 diff 픽셀** → 사실상 *비트동일* 요구. 우리 리팩터는 *연산 순서 보존 추출* 이라 비트동일이어야 정상.
- **FAIL 시**: `build_ninja/test/golden_artifacts/diff_<name>.png` 를 열어 원인 분류 →
  - **구조적 차이**(스프라이트 누락·색 뒤바뀜·검은 화면) = 리팩터 버그. 원인 추측 후 셰이더 재구성(program=null? varying/샘플러/brace-init 트랩? UV 계산 오류?).
  - **균일 ~1 LSB 노이즈**(전면 미세차) = Slang 이 추출로 FP 연산을 재결합(reassoc)한 것. 추출을 *원 연산 순서와 정확히 일치*하도록 재작성(예: 루프 순서/괄호 보존). 임계는 절대 올리지 말 것(게이트 무력화).

**골든 커버리지 맵 (어느 Task 가 골든으로 검증되나):**
| Task | 커버 골든 | 비고 |
|---|---|---|
| A2 healthbar | full / no_imgui | 체력바 가시 |
| A3, B2 billboard_atlas | full / no_imgui | 스프라이트·텍스트 글리프 |
| B3 matrix_skybox | skybox | 스카이박스 전용 골든 |
| C 전체(postfx_common VS) | full | 활성 fog/grayscale/vignette/present 가 *공유 VS* 를 경유 → VS 파손 시 골든 FAIL(전이 검증) |
| D1 grayscale(Luminance) | full | grayscale 활성 |
| **D2 sobel / D3 sharpening / D4 blurring·bloom** | **없음(Enabled=false)** | ⚠️ 골든 미커버 → 아래 런타임 토글 게이트로 대체 |

**런타임 토글 게이트 (골든 미커버 postfx 전용 — D2/D3/D4):**
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
기대: ImGui `PassDebugLayer`(F1) 로 해당 효과(sobel/sharpen/blur/bloom) **Enabled 토글 → 화면에 정상 렌더** + stderr 에 `program` null / 셰이더 컴파일·링크 에러 없음. 확인 후 창 닫기.

**트랩 사전점검(결정적, 모든 Task 공통):** 빌드 후 *실제 GL 컴파일 대상*(`.vs`/`.fs`)에 정규화 안 된 GLSL420 brace-init 잔존 검사:
```bash
grep -rn "= {" --include="*.vs" --include="*.fs" build_ninja/apps/_MyApp_/shaders_slang/generated/ ; echo "exit=$?"
```
기대: **매치 0줄**(`exit=1`). 매치 있으면 brace-init 이 새 나온 것 → 런타임 program=null 위험, 정규화 누락.
> ⚠️ `--include="*.vs" --include="*.fs"` 필수. 스코프 없이 `generated/` 전체를 훑으면 `.raw`(slangc 전처리 전 산출) / `.ignore.glsl`(reflection 더미 side-output)이 오탐을 낸다 — 이 둘은 GL 이 컴파일하지 않고 post-process 도 안 거치며, 기존 sobel/sharpening 의 *배열* brace-init 을 항상 담고 있다(Task 0 실측 발견).

> 실행 위치: 리소스 상대경로 때문에 반드시 `build_ninja/apps/_MyApp_` 에서 실행. 골든 캡처는 창을 잠깐 띄웠다가 180프레임 후 자동 종료.

---

## Task 0: 툴체인 — 구조체 brace-init 정규화 (postfx_common 선행 필수)

**Files:**
- Modify: `scripts/slang_compile.py` (함수 추가 + `post_process_glsl_410` 에 호출 1줄)

- [ ] **Step 1: `normalize_struct_initializers` 함수 추가**

`scripts/slang_compile.py` 의 `normalize_array_initializers` 함수 정의 **직후**(현재 111줄, `post_process_glsl_410` 정의 앞)에 삽입:

```python
def normalize_struct_initializers(text):
    """Slang 의 구조체 brace-초기화(GLSL 420)를 GLSL 구조체 생성자 호출로 변환.

    Slang 은 entry-point 로 조립된 struct 값을 일반 함수(예: import 된 공유 VS 헬퍼)에
    값 전달할 때 'VSIn_0 _S1 = { input_aPos_0, input_aUV_0 };' (brace-init, GLSL 420 =
    ARB_shading_language_420pack) 를 낸다. macOS OpenGL 4.1(GLSL 410) 코어는 brace-init
    미지원 -> 런타임 컴파일 거부 -> program=null (postfx_common 공유 VS 도입 시 최초 발현 -
    이전엔 어떤 셰이더도 entry-point 파라미터 전체를 다른 함수에 값 전달하지 않았음).
    구조체는 배열과 달리 'TypeName(원소, ...)' 생성자 호출이 GLSL 코어 전 버전에서 유효하므로
    'Type name = { a, b };' -> 'Type name = Type(a, b);' 로 치환.
    (배열 brace-init 은 normalize_array_initializers 가 먼저 처리 - 그 결과물엔 '[' 이 남아
    이 정규식(이름 뒤 공백만 허용)과 겹치지 않는다. 초기화 원소는 flat - 중첩 brace 없음 가정.)
    """
    struct_init_re = re.compile(
        r"\b([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*=\s*\{([^{}]*)\}\s*;")
    return struct_init_re.sub(r"\1 \2 = \1(\3);", text)
```

- [ ] **Step 2: `post_process_glsl_410` 에 호출 추가**

`scripts/slang_compile.py` 의 `post_process_glsl_410` 함수에서 `normalize_array_initializers` 호출(현재 127줄, 주석 `# 6)`) **바로 아래**에 추가:

```python
    # 7) 구조체 brace-init(GLSL 420) -> 구조체 생성자 호출(GLSL 410) (postfx_common 공유 VS 함정)
    text = normalize_struct_initializers(text)
```

- [ ] **Step 3: 회귀 없음 확인 (현 셰이더엔 no-op)**

현재 셰이더 중 struct-by-value 전달은 없으므로 이 변경은 no-op 이어야 한다. Verification Recipe 의 **빌드** 실행 → GREEN 확인. 트랩 사전점검 → 0줄. (런타임은 기존과 동일하므로 생략 가능.)

- [ ] **Step 4: 커밋 (경로 지정)**

```bash
git add scripts/slang_compile.py
git commit -m "[refactor] slang_compile: 구조체 brace-init GLSL410 정규화 추가 (postfx_common 공유 VS 선행)"
```

---

## Task 0.5: 베이스라인 생성 GLSL 캡처 (등가성 대조용)

**Files:** (읽기 전용 — 산출물 복사)

- [ ] **Step 1: 현재(모듈화 전) 생성 GLSL 을 베이스라인으로 복사**

```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5
cp -R build_ninja/apps/_MyApp_/shaders_slang/generated /private/tmp/claude-501/-Users-escatrgot-DevelopProjects-SSU-GlobalMedia-OpenGL-ComputerGraphics/9095d0a2-fd15-4e14-b044-fe3249a69b49/scratchpad/slang_baseline
```

- [ ] **Step 2: 확인**

```bash
ls /private/tmp/claude-501/-Users-escatrgot-DevelopProjects-SSU-GlobalMedia-OpenGL-ComputerGraphics/9095d0a2-fd15-4e14-b044-fe3249a69b49/scratchpad/slang_baseline/postprocess/ | head
```
기대: `invert.fs`, `sobel.fs` 등 존재. 이후 각 소비처 변환 후 `diff` 로 *생성 GLSL 이 기능적으로 등가*인지 대조(수학 연산 동일, 함수 인라인 형태만 변화 허용). 커밋 없음(scratchpad).

---

## Phase A: billboard_common (🟢 순수 행렬수학, 툴체인 무관)

### Task A1: billboard_common.slang 생성

**Files:**
- Create: `apps/_MyApp_/shaders_slang/modules/billboard_common.slang`

- [ ] **Step 1: 모듈 파일 작성** (d71e2ff 실증 코드 그대로)

```hlsl
// apps/_MyApp_/shaders_slang/modules/billboard_common.slang
// 빌보드 공통 저수준 추출 함수 - 카메라축 + 모델행렬 center/scale.
// 상위 합성(worldPos 조립)은 호출부마다 다르므로(billboard_atlas=flip/roll, healthbar=headOffset)
// 이 모듈에 넣지 않는다(D-4).
module billboard_common;

// view 의 회전만 추출 (orthonormal 가정 - transpose == inverse-rotation, w=0 방향이라 translation 무영향).
public void ExtractCameraAxes(float4x4 view, out float3 right, out float3 up)
{
    right = mul(transpose(view), float4(1.0, 0.0, 0.0, 0.0)).xyz;
    up    = mul(transpose(view), float4(0.0, 1.0, 0.0, 0.0)).xyz;
}

public float3 ExtractModelCenter(float4x4 model)
{
    return mul(model, float4(0.0, 0.0, 0.0, 1.0)).xyz;
}

// axisLocal = float4(1,0,0,0) 또는 float4(0,1,0,0) - 호출부가 축 지정.
public float ExtractModelAxisScale(float4x4 model, float4 axisLocal)
{
    return length(mul(model, axisLocal).xyz);
}
```

- [ ] **Step 2: 빌드** — 이 시점엔 소비처가 없어 컴파일에 미포함(import 하는 셰이더 없음). 다음 태스크에서 사용. Verification Recipe **빌드** 만 실행해 GREEN 확인(신규 파일이 sibling glob depfile 을 건드려도 무해).

- [ ] **Step 3: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/modules/billboard_common.slang
git commit -m "[refactor] shaders: billboard_common 모듈 추가 (카메라축/center/scale 추출)"
```

### Task A2: healthbar.slang 를 billboard_common 소비로 변환

**Files:**
- Modify: `apps/_MyApp_/shaders_slang/healthbar.slang` (VS 부분만)

- [ ] **Step 1: `import` 추가** — 파일 상단 헤더 주석 직후, `struct FrameBlock` 앞에 삽입:

```hlsl
import billboard_common;
```

- [ ] **Step 2: `vsMain` 몸통 교체** — 기존 `vsMain`(52-69줄)의 카메라축·center·scale 추출부를 공유 함수 호출로 교체. 교체 후 `vsMain` 전체:

```hlsl
[shader("vertex")]
VSOut vsMain(VSIn input)
{
    // 카메라 right/up (world) - billboard_common 공유 추출.
    float3 cameraRight, cameraUp;
    ExtractCameraAxes(uFrame.uView, cameraRight, cameraUp);

    // uModel 에서 center + size - billboard_common 공유 추출.
    float3 center = ExtractModelCenter(uDraw.uModel);  // Scale.x -> 바 가로폭, Scale.y -> 바 세로높이
    float  sx     = ExtractModelAxisScale(uDraw.uModel, float4(1.0, 0.0, 0.0, 0.0));
    float  sy     = ExtractModelAxisScale(uDraw.uModel, float4(0.0, 1.0, 0.0, 0.0));

    // 머리 위 앵커: 화면상 cameraUp 방향 오프셋 (카메라각 독립).
    float3 anchor   = center + cameraUp * uMaterial.uHeadOffset;
    float3 worldPos = anchor + cameraRight * input.aPos.x * sx + cameraUp * input.aPos.y * sy;

    VSOut output;
    output.uv  = input.aTexCoord;
    output.pos = mul(uFrame.uProj, mul(uFrame.uView, float4(worldPos, 1.0)));
    return output;
}
```

`struct VSIn` / `struct VSOut` / `MaterialBlock` 등 나머지는 그대로 둔다.

- [ ] **Step 3: 빌드 + 트랩점검** — Verification Recipe **빌드** GREEN + **트랩 사전점검** 0줄.

- [ ] **Step 4: 런타임 검증(합격 게이트)** — Verification Recipe **런타임**. 적/플레이어 머리 위 **체력바가 정상 렌더**(조각 분할·fill·카메라 대면)되는지 육안 확인. program=null 로그 없음.

- [ ] **Step 5: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/healthbar.slang
git commit -m "[refactor] healthbar: billboard_common 소비로 VS 추출부 대체"
```

### Task A3: billboard_atlas.slang 의 VS 를 billboard_common 소비로 변환

**Files:**
- Modify: `apps/_MyApp_/shaders_slang/billboard_atlas.slang` (VS 부분만 — atlas UV 는 Phase B)

- [ ] **Step 1: `import` 추가** — 파일 상단 헤더 주석 직후, `struct FrameBlock` 앞:

```hlsl
import billboard_common;
```

- [ ] **Step 2: `vsMain` 카메라축·center·scale 추출부 교체** — 기존 `vsMain`(52-76줄)에서 `cameraRight`/`cameraUp`/`center`/`sx`/`sy` 계산 5줄을 공유 함수로 교체. 교체 후 `vsMain` 전체(⚠️ `output.uv` 줄은 Phase B 에서 바꾸므로 **여기선 그대로 유지**):

```hlsl
[shader("vertex")]
VSOut vsMain(VSIn input)
{
    // 카메라 right/up (world) - billboard_common 공유 추출.
    float3 cameraRight, cameraUp;
    ExtractCameraAxes(uFrame.uView, cameraRight, cameraUp);

    // uModel 에서 center + size - billboard_common 공유 추출.
    float3 center = ExtractModelCenter(uDraw.uModel);
    float  sx     = ExtractModelAxisScale(uDraw.uModel, float4(1.0, 0.0, 0.0, 0.0));
    float  sy     = ExtractModelAxisScale(uDraw.uModel, float4(0.0, 1.0, 0.0, 0.0));

    // flip 선반영 후 roll(카메라 정면축) 2D 회전.
    float2 p  = float2(input.aPos.x * uMaterial.uFlipX, input.aPos.y);
    float  cr = cos(uMaterial.uRoll);
    float  sr = sin(uMaterial.uRoll);
    float2 rp = float2(p.x * cr - p.y * sr, p.x * sr + p.y * cr);

    float3 worldPos = center + cameraRight * rp.x * sx + cameraUp * rp.y * sy;

    VSOut output;
    output.uv  = uMaterial.uUvRect.xy + input.aTexCoord * uMaterial.uUvRect.zw;
    output.pos = mul(uFrame.uProj, mul(uFrame.uView, float4(worldPos, 1.0)));
    return output;
}
```

- [ ] **Step 3: 빌드 + 트랩점검** — GREEN + 0줄.

- [ ] **Step 4: 런타임 검증(합격 게이트)** — **런타임**. 플레이어/적 **스프라이트가 정상 렌더**(빌보드 대면·좌우반전·애니 프레임)되는지 육안 확인. SpriteRenderer 최다 호출 셰이더라 텍스트 글리프(TextRenderer)도 함께 확인.

- [ ] **Step 5: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/billboard_atlas.slang
git commit -m "[refactor] billboard_atlas: billboard_common 소비로 VS 추출부 대체"
```

---

## Phase B: texture_atlas (🟢 순수 float수학, 툴체인 무관)

### Task B1: texture_atlas.slang 생성

**Files:**
- Create: `apps/_MyApp_/shaders_slang/modules/texture_atlas.slang`

- [ ] **Step 1: 모듈 파일 작성**

```hlsl
// apps/_MyApp_/shaders_slang/modules/texture_atlas.slang
// 아틀라스 UV 리맵 공통 - sub-rect 적용 + 균일 그리드 셀 분해.
// ※ CPU SJH::sprite::ComputeUVRect 가 rect 의 SSoT - 이 모듈은 rect 를 grid 로부터 재도출하지 않는다(D-atlas-SSoT).
//   셰이더는 이미 계산된 rect 를 '적용'만 한다.
module texture_atlas;

// 선택된 sub-rect(min,size)로 로컬 [0,1] UV 를 아틀라스 좌표로 리맵.
// rect.xy = (uMin, vMin), rect.zw = (uSize, vSize). billboard_atlas 의 uUvRect 적용식과 동일.
public float2 AtlasSubUV(float2 localUV, float4 rect)
{
    return rect.xy + localUV * rect.zw;
}

// 균일 그리드 셀 분해 - uv*grid 의 floor(셀 인덱스) / frac(셀 내부 [0,1]).
public void AtlasCell(float2 uv, float2 grid, out float2 cellId, out float2 inCell)
{
    float2 g = uv * grid;
    cellId = floor(g);
    inCell = frac(g);
}
```

- [ ] **Step 2: 빌드** — Verification Recipe **빌드** GREEN(아직 소비처 없음).

- [ ] **Step 3: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/modules/texture_atlas.slang
git commit -m "[refactor] shaders: texture_atlas 모듈 추가 (AtlasSubUV/AtlasCell, apply-only)"
```

### Task B2: billboard_atlas.slang 의 UV 줄을 AtlasSubUV 로 변환

**Files:**
- Modify: `apps/_MyApp_/shaders_slang/billboard_atlas.slang` (import 1줄 + uv 1줄)

- [ ] **Step 1: `import` 추가** — 기존 `import billboard_common;`(Task A3에서 추가) 바로 아래에:

```hlsl
import texture_atlas;
```

- [ ] **Step 2: `output.uv` 줄 교체** — `vsMain` 안의

```hlsl
    output.uv  = uMaterial.uUvRect.xy + input.aTexCoord * uMaterial.uUvRect.zw;
```
를

```hlsl
    output.uv  = AtlasSubUV(input.aTexCoord, uMaterial.uUvRect);
```
로 교체.

- [ ] **Step 3: 빌드 + 트랩점검 + 런타임** — GREEN + 0줄 + 스프라이트 정상 렌더(Task A3 와 동일 육안 기준: 애니 프레임이 올바른 칸을 샘플).

- [ ] **Step 4: 베이스라인 대조(선택)** — `diff <(...) build_ninja/.../generated/billboard_atlas.vs` 로 `output.uv` 계산이 기능적으로 동일한지 확인(변수명·인라인 형태만 차이).

- [ ] **Step 5: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/billboard_atlas.slang
git commit -m "[refactor] billboard_atlas: atlas UV 를 texture_atlas::AtlasSubUV 로 대체"
```

### Task B3: matrix_skybox.slang 를 AtlasCell + AtlasSubUV 소비로 변환

**Files:**
- Modify: `apps/_MyApp_/shaders_slang/matrix_skybox.slang` (import 1줄 + FS 그리드/char_uv 부분)

- [ ] **Step 1: `import` 추가** — 파일 상단 헤더 주석 직후, `struct FrameBlock` 앞:

```hlsl
import texture_atlas;
```

- [ ] **Step 2: 그리드 셀 분해 교체** — `fsMain`(69-101줄) 안의

```hlsl
    // 글자 격자 셀 분해
    float2 grid_uv = base_uv * float2(COLS, ROWS);
    float2 cellId  = floor(grid_uv);
    float2 inCell  = frac(grid_uv);
```
를

```hlsl
    // 글자 격자 셀 분해 - texture_atlas 공유.
    float2 cellId, inCell;
    AtlasCell(base_uv, float2(COLS, ROWS), cellId, inCell);
```
로 교체.

- [ ] **Step 3: char_uv 를 AtlasSubUV 로 교체** — 이어지는

```hlsl
    // 아틀라스 정확 매핑 - d 번째 64px 글자칸.
    float  cellLeftPx = GAP + d * (CHAR_W + GAP);
    float2 char_uv = float2((cellLeftPx + inCell.x * CHAR_W) / ATLAS_W, inCell.y);
```
를 — gapped strip rect(min=cellLeftPx/ATLAS_W, 0 / size=CHAR_W/ATLAS_W, 1)를 도출해 적용:

```hlsl
    // 아틀라스 정확 매핑 - d 번째 64px 글자칸. pixel-space 에서 sub-rect 적용 후 /ATLAS_W (원 연산 순서 보존).
    float  cellLeftPx = GAP + d * (CHAR_W + GAP);
    float2 charPx     = AtlasSubUV(inCell, float4(cellLeftPx, 0.0, CHAR_W, 1.0));
    float2 char_uv    = float2(charPx.x / ATLAS_W, charPx.y);
```

⚠️ **비트동일 주의(실측 교훈, 2026-07-08)**: 순진한 버전 `charRect = float4(cellLeftPx/ATLAS_W, 0, CHAR_W/ATLAS_W, 1); AtlasSubUV(inCell, charRect)` 은 **FP 재결합으로 비트동일이 아니다**(golden_skybox diffPixels=244). 원본 `(cellLeftPx + inCell.x*CHAR_W)/ATLAS_W` 는 *합을 한 번 나눔*인데, 선-나눗셈은 반올림 단계가 추가돼 IEEE-754 상 달라진다. 위 코드처럼 **pixel-space 에서 mul→add 후 마지막에 /ATLAS_W** 로 원 연산 순서를 그대로 재현해야 diffPixels=0. (threshold=0 골든이 이 미세 차이를 잡아냄 - 게이트가 제대로 작동한 사례.)

- [ ] **Step 4: 빌드 + 트랩점검** — GREEN + 0줄. ⚠️ `AtlasCell` 의 `out float2 x2` 다중 out-param 이 GLSL410 에서 새 트랩(구조체 wrap 등)을 낼 수 있으니 **트랩점검 필수**.

- [ ] **Step 5: 런타임 검증(합격 게이트)** — **런타임**. Matrix 스카이박스의 **떨어지는 초록 글자 비**가 정상 렌더(글자칸 정확·스크롤·노이즈)되는지 육안 확인.

- [ ] **Step 6: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/matrix_skybox.slang
git commit -m "[refactor] matrix_skybox: 그리드/char UV 를 texture_atlas(AtlasCell/AtlasSubUV) 로 대체"
```

---

## Phase C: postfx_common (🟡 Task 0 선행 필수 — 구조체 brace-init)

### Task C1: postfx_common.slang 생성

**Files:**
- Create: `apps/_MyApp_/shaders_slang/modules/postfx_common.slang`

- [ ] **Step 1: 모듈 파일 작성** (d71e2ff 실증 코드 그대로)

```hlsl
// apps/_MyApp_/shaders_slang/modules/postfx_common.slang
// 풀스크린 NDC passthrough VS 공통 - entry point(vsMain)는 각 파일에 로컬 유지(D-1),
// 몸통 로직만 함수로 공유.
module postfx_common;

public struct VSIn
{
    [[vk::location(0)]] float3 aPos : POSITION;
    [[vk::location(2)]] float2 aUV  : TEXCOORD0;
};

public struct VSOut
{
    public float4 pos : SV_Position;
    public float2 uv  : TEXCOORD0;
};

public VSOut ComputeScreenQuadVSOut(VSIn input)
{
    VSOut output;
    output.pos = float4(input.aPos, 1.0);
    output.uv  = input.aUV;
    return output;
}
```

- [ ] **Step 2: 빌드** — GREEN(소비처 없음).

- [ ] **Step 3: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/modules/postfx_common.slang
git commit -m "[refactor] shaders: postfx_common 모듈 추가 (풀스크린 NDC VS)"
```

### Task C2: 카나리 — invert.slang 1개만 먼저 변환(구조체 brace-init 트랩 실증)

**Files:**
- Modify: `apps/_MyApp_/shaders_slang/invert.slang`

- [ ] **Step 1: 변환** — `invert.slang` 전체를 아래로 교체(로컬 `VSIn`/`VSOut`/`vsMain` 몸통 제거, import + 위임):

```hlsl
// apps/_MyApp_/shaders_slang/invert.slang
// postfx 색반전 - postfx_common 공유 VS + FS. uScene 샘플러만.
// 생성물: postprocess/invert.{vs,fs}.
import postfx_common;

Sampler2D uScene;

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    return ComputeScreenQuadVSOut(input);
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float4 pixel = uScene.Sample(input.uv);
    return float4(1.0 - pixel.rgb, 1.0);
}
```

- [ ] **Step 2: 빌드 + 트랩점검(핵심)** — **빌드** GREEN. **트랩 사전점검**:
```bash
grep -n "= {" build_ninja/apps/_MyApp_/shaders_slang/generated/postprocess/invert.vs ; echo "exit=$?"
```
기대: **0줄**(exit=1). Task 0 의 `normalize_struct_initializers` 가 `VSIn_0 _S1 = {...}` 를 생성자로 바꿔야 여기 매치가 없다. 매치가 있으면 Task 0 이 누락/오작동 — 진행 중단하고 Task 0 재점검.

- [ ] **Step 3: 런타임 검증(합격 게이트)** — **런타임**. ImGui PassDebugLayer 에서 invert 효과 on → **화면 색반전**이 정상 동작, program=null 로그 없음. Task 0 정규화가 런타임에서 실증되는 지점.

- [ ] **Step 4: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/invert.slang
git commit -m "[refactor] invert: postfx_common 공유 VS 소비 (구조체 brace-init 정규화 실증)"
```

### Task C3: 나머지 9개 postfx VS 를 postfx_common 소비로 변환

**Files (9):** `passthrough`, `gamma`, `sharpening`, `grayscale_vignetting`, `blurring`, `depth_debug`, `bloom`, `sobel`, `fog` (`.slang`)

이 9개는 모두 **동일한 로컬 VS**(`struct VSIn{aPos loc0, aUV loc2}` + `struct VSOut{pos, uv}` + `vsMain{output.pos=float4(aPos,1); output.uv=aUV; return}`)를 갖는다(Task C2 의 invert 와 바이트 동일). 각 파일에 대해 **동일 3연산** 수행:

**연산 (각 파일):**
1. 파일 상단 헤더 주석 직후에 `import postfx_common;` 추가.
2. 로컬 `struct VSIn { ... };` 와 `struct VSOut { ... };` **삭제**.
3. `vsMain` 몸통을 `return ComputeScreenQuadVSOut(input);` 한 줄로 교체(시그니처 `VSOut vsMain(VSIn input)` 유지).
4. **그 외 모든 것 유지** — `Sampler2D uScene`/`uDepth`, `struct MaterialBlock`+`ConstantBuffer`, `fsMain` 전체, 파일 고유 헬퍼(fog 의 `fogFactor*`, blurring/bloom 의 `GaussianWeight` 등)는 **손대지 않음**(image_processing 은 Phase D).

- [ ] **Step 1: passthrough.slang 변환** — 위 연산. (FS 는 `return float4(uScene.Sample(input.uv).rgb, 1.0);` 유지.)
- [ ] **Step 2: gamma.slang 변환** — 위 연산. `MaterialBlock{gamma}`+`ConstantBuffer` 유지.
- [ ] **Step 3: sharpening.slang 변환** — 위 연산. FS(3x3 laplacian) 유지.
- [ ] **Step 4: grayscale_vignetting.slang 변환** — 위 연산. `MaterialBlock`(3멤버) + FS 유지.
- [ ] **Step 5: blurring.slang 변환** — 위 연산. 로컬 `GaussianWeight` + FS 유지.
- [ ] **Step 6: depth_debug.slang 변환** — 위 연산. `uDepth` 샘플러 + `MaterialBlock`(3멤버) + FS 유지.
- [ ] **Step 7: bloom.slang 변환** — 위 연산. 로컬 `GaussianWeight` + `MaterialBlock`(3멤버) + FS 유지.
- [ ] **Step 8: sobel.slang 변환** — 위 연산. FS(sobel) 유지.
- [ ] **Step 9: fog.slang 변환** — 위 연산. `uDepth` + `MaterialBlock`(mat4+vec3+float×3+int) + `fogFactor*` 헬퍼 + FS 유지.

- [ ] **Step 10: 빌드 + 트랩점검(전체)** — **빌드** GREEN. **트랩 사전점검**(전체 generated) 0줄:
```bash
grep -rn "= {" build_ninja/apps/_MyApp_/shaders_slang/generated/ ; echo "exit=$?"
```

- [ ] **Step 11: 런타임 검증(합격 게이트)** — **런타임**. PassDebugLayer 로 각 효과 순회 토글: passthrough/gamma/grayscale+vignette/blur/sharpen/sobel/bloom/depth_debug/fog 가 **각각 정상 렌더**. 어느 하나라도 program=null → 해당 파일의 변환 재점검.

- [ ] **Step 12: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/passthrough.slang apps/_MyApp_/shaders_slang/gamma.slang apps/_MyApp_/shaders_slang/sharpening.slang apps/_MyApp_/shaders_slang/grayscale_vignetting.slang apps/_MyApp_/shaders_slang/blurring.slang apps/_MyApp_/shaders_slang/depth_debug.slang apps/_MyApp_/shaders_slang/bloom.slang apps/_MyApp_/shaders_slang/sobel.slang apps/_MyApp_/shaders_slang/fog.slang
git commit -m "[refactor] postfx 9종: postfx_common 공유 VS 소비 (로컬 NDC VS 제거)"
```

---

## Phase D: image_processing (🔴 스파이크 게이트 — sampler/배열 param)

### Task D1: image_processing.slang 생성 (pure 함수만 먼저)

**Files:**
- Create: `apps/_MyApp_/shaders_slang/modules/image_processing.slang`

먼저 **sampler/배열 param 없는 안전 함수만** 넣어 모듈 자체 컴파일을 검증한다.

- [ ] **Step 1: 안전 함수만으로 모듈 작성**

```hlsl
// apps/_MyApp_/shaders_slang/modules/image_processing.slang
// 이미지 처리 공통 - 휘도 / 가우시안 가중치 / 텍셀 크기 / 3x3 컨볼루션.
// ⚠️ sampler/배열 param 을 공유 함수로 넘기는 최초 사례 - macOS GLSL410 통과가 전제(스파이크 D2).
module image_processing;

// Rec.709 휘도.
public float Luminance(float3 rgb)
{
    return dot(rgb, float3(0.299, 0.587, 0.114));
}

// 가우시안 가중치 exp(-거리제곱 * falloff).
public float GaussianWeight(float2 offset, float falloff)
{
    return exp(-dot(offset, offset) * falloff);
}
```

- [ ] **Step 2: grayscale_vignetting 을 Luminance 소비로 변환** — `import image_processing;` 를 `import postfx_common;`(Phase C 에서 추가) 아래에 넣고, FS 의

```hlsl
    float  luminance = dot(finalColor, float3(0.299, 0.587, 0.114));
```
를
```hlsl
    float  luminance = Luminance(finalColor);
```
로 교체.

- [ ] **Step 3: 빌드 + 트랩점검 + 런타임** — GREEN + 0줄 + grayscale/vignette 정상. (pure 함수라 안전 — 여기까지는 위험 없음.)

- [ ] **Step 4: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/modules/image_processing.slang apps/_MyApp_/shaders_slang/grayscale_vignetting.slang
git commit -m "[refactor] image_processing 모듈(pure) + grayscale Luminance 소비"
```

### Task D2: 스파이크 — sobel 로 sampler/배열 param 런타임 검증 (게이트)

**Files:**
- Modify: `apps/_MyApp_/shaders_slang/modules/image_processing.slang` (위험 함수 추가)
- Modify: `apps/_MyApp_/shaders_slang/sobel.slang` (카나리 소비)

- [ ] **Step 1: image_processing 에 sampler/배열 함수 추가** — 모듈 끝에 추가:

```hlsl
// 샘플러 해상도의 역수 = 1 텍셀의 UV 크기.
public float2 TexelSize(Sampler2D s)
{
    uint w, h;
    s.GetDimensions(w, h);
    return 1.0 / float2(w, h);
}

// 3x3 이웃 단위 오프셋 (texel 곱은 호출부).
public static const float2 kOffset3x3[9] = {
    float2(-1.0,  1.0), float2(0.0,  1.0), float2(1.0,  1.0),
    float2(-1.0,  0.0), float2(0.0,  0.0), float2(1.0,  0.0),
    float2(-1.0, -1.0), float2(0.0, -1.0), float2(1.0, -1.0)
};

// 3x3 이웃 9색 샘플 (uv 중심, texel 간격).
public void Sample3x3(Sampler2D s, float2 uv, float2 texel, out float3 c[9])
{
    for (int i = 0; i < 9; ++i)
        c[i] = s.Sample(uv + kOffset3x3[i] * texel).rgb;
}

// 9스칼라 x 9커널 가중합 (휘도 컨볼루션 - sobel gx/gy).
public float Dot9(float l[9], float k[9])
{
    float sum = 0.0;
    for (int i = 0; i < 9; ++i) sum += l[i] * k[i];
    return sum;
}

// 9색 x 9커널 가중합 (색 컨볼루션 - sharpening).
public float3 Dot9Color(float3 c[9], float k[9])
{
    float3 sum = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < 9; ++i) sum += c[i] * k[i];
    return sum;
}
```

- [ ] **Step 2: sobel.slang FS 를 공유 함수 소비로 변환** — `import image_processing;` 를 `import postfx_common;` 아래에 추가하고, `fsMain` 전체를 교체:

```hlsl
[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float2 texel = TexelSize(uScene);

    float3 c[9];
    Sample3x3(uScene, input.uv, texel, c);

    float l[9];
    for (int i = 0; i < 9; ++i)
        l[i] = Luminance(c[i]);

    float sobelX[9] = {
        -1.0, 0.0, 1.0,
        -2.0, 0.0, 2.0,
        -1.0, 0.0, 1.0
    };
    float sobelY[9] = {
        -1.0, -2.0, -1.0,
         0.0,  0.0,  0.0,
         1.0,  2.0,  1.0
    };

    float gx = Dot9(l, sobelX);
    float gy = Dot9(l, sobelY);

    float edge = sqrt(gx * gx + gy * gy);
    edge = clamp(edge, 0.0, 1.0);
    const float threshold = 0.3;
    edge = step(threshold, edge);

    return float4(edge, edge, edge, 1.0);
}
```

- [ ] **Step 3: 빌드 + 트랩점검(집중)** — **빌드** GREEN. **트랩 사전점검** 전체 0줄. 특히 sobel.fs 생성물을 직접 열어 확인:
```bash
grep -n "= {\|_[0-9]\+ *(" build_ninja/apps/_MyApp_/shaders_slang/generated/postprocess/sobel.fs | head
```
`kOffset3x3`/`sobelX`/`sobelY` 배열이 `float[](...)` 생성자로 정규화됐는지, sampler param 이 올바른 GLSL(`sampler2D` 인자)로 나왔는지 육안 확인.

- [ ] **Step 4: 런타임 검증(★게이트★)** — **런타임**. PassDebugLayer 에서 **sobel 엣지 검출이 정상 렌더**(윤곽선 흑백) + program=null 로그 없음.
  - **PASS** → 스파이크 통과. sampler/배열 param 이 macOS GL410 을 통과함이 실증됨. Task D3 진행.
  - **FAIL(program=null)** → 생성 GLSL 에서 실패 원인(sampler param 형태 / 배열 out-param 구조체 wrap 등) 진단.
    - 원인이 새 정규화로 해결 가능하면 `slang_compile.py` 에 정규화 추가(Task 0 패턴) 후 재검증.
    - 해결 불가하면 **폴백**: `image_processing` 을 `color_utils`(Luminance/GaussianWeight — pure, 유지) + convolution(sampler/배열 — **철회**)로 축소하고, sharpening/sobel 은 `TexelSize`/`Sample3x3` 를 **로컬 유지**(Luminance 만 공유). 이 경우 Task D3~D4 의 sharpening/sobel 은 Luminance 만 소비로 재작성. 폴백 결정을 본 플랜 하단 "미해결" 에 기록.

- [ ] **Step 5: 커밋(PASS 시)**

```bash
git add apps/_MyApp_/shaders_slang/modules/image_processing.slang apps/_MyApp_/shaders_slang/sobel.slang
git commit -m "[refactor] image_processing: sampler/배열 param 함수 + sobel 소비 (GL410 스파이크 통과)"
```

### Task D3: sharpening 을 image_processing 소비로 변환 (스파이크 PASS 전제)

**Files:**
- Modify: `apps/_MyApp_/shaders_slang/sharpening.slang`

- [ ] **Step 1: `import` + FS 변환** — `import image_processing;` 추가 후 `fsMain` 전체 교체:

```hlsl
[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float2 texel = TexelSize(uScene);

    float3 c[9];
    Sample3x3(uScene, input.uv, texel, c);

    float laplacian[9] = {
        -1.0, -1.0, -1.0,
        -1.0,  9.0, -1.0,
        -1.0, -1.0, -1.0
    };

    float3 color = Dot9Color(c, laplacian);
    return float4(color, 1.0);
}
```

- [ ] **Step 2: 빌드 + 트랩점검 + 런타임** — GREEN + 0줄 + sharpening(라플라시안 샤프닝) 정상 렌더.

- [ ] **Step 3: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/sharpening.slang
git commit -m "[refactor] sharpening: image_processing(Sample3x3/Dot9Color) 소비"
```

### Task D4: blurring / bloom 을 GaussianWeight + TexelSize 소비로 변환

**Files:**
- Modify: `apps/_MyApp_/shaders_slang/blurring.slang`, `apps/_MyApp_/shaders_slang/bloom.slang`

- [ ] **Step 1: blurring 변환** — `import image_processing;` 추가, 로컬 `float GaussianWeight(...)` **함수 정의 삭제**(모듈 것 사용), `fsMain` 에서 텍셀 계산을 `float2 texel = TexelSize(uScene);` 로 교체(기존 `GetDimensions`+`1/dims` 3줄 대체). 15x15 이중 루프·`GaussianWeight(cell, falloff)` 호출·`weightSum` 정규화는 유지.

- [ ] **Step 2: bloom 변환** — `import image_processing;` 추가, 로컬 `float GaussianWeight(...)` 삭제, `fsMain` 텍셀 계산을 `TexelSize(uScene)` 로 교체, brightpass 휘도 `dot(samp, float3(0.299,0.587,0.114))` 를 `Luminance(samp)` 로 교체. 9x9 루프·`MaterialBlock`(threshold/spread/intensity)·additive 는 유지.

- [ ] **Step 3: 빌드 + 트랩점검 + 런타임** — GREEN + 0줄 + blur(가우시안 블러)·bloom(밝은부분 번짐) 정상 렌더.

- [ ] **Step 4: 커밋**

```bash
git add apps/_MyApp_/shaders_slang/blurring.slang apps/_MyApp_/shaders_slang/bloom.slang
git commit -m "[refactor] blurring/bloom: image_processing(GaussianWeight/TexelSize/Luminance) 소비"
```

---

## 최종 검증 (전체 회귀)

- [ ] **Step 1: 클린 빌드** — `rm -rf build_ninja && cmake --preset ninja && cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -20` → GREEN.
- [ ] **Step 2: 전체 트랩점검** — `grep -rn "= {" build_ninja/apps/_MyApp_/shaders_slang/generated/ ; echo "exit=$?"` → 0줄.
- [ ] **Step 3: 전체 런타임 육안** — `./_MyApp_` 실행 후: 스프라이트/텍스트/체력바/스카이박스 렌더 + PassDebugLayer 로 postfx 9종(invert/gamma/grayscale/blur/sharpen/sobel/bloom/depth_debug/fog) 순회 토글 전부 정상. program=null 로그 0.
- [ ] **Step 4: 손대지 않은 셰이더 확인** — `git status` 로 `simple`/`simple_texture`/`transparent`/`phong`/`phong_lighting`/`shaders_slang/CMakeLists.txt` 무변경 확인.

---

## Self-Review 결과

**1. 스코프 커버리지:** 5 모듈(기존 1 + 신규 4) 전부 태스크化. billboard_common(A) / texture_atlas(B) / postfx_common(C) / image_processing(D) + 툴체인(Task 0). 소비처 14개 전부 변환 태스크 존재(healthbar/billboard_atlas/matrix_skybox + postfx 10 + image proc 5, billboard_atlas·grayscale·sobel 는 복수 Phase 에서 각 1회씩).

**2. 플레이스홀더:** 모듈 4개 전문 + 툴체인 함수 전문 + 소비처 변환 코드(핵심 부분) 명시. postfx 9종은 동일연산이라 연산 명세 + 파일별 체크박스(코드 반복 대신 정확한 3연산 + 유지 대상 명시).

**3. 타입 일관성:** `VSIn`/`VSOut`/`ComputeScreenQuadVSOut`(postfx_common), `ExtractCameraAxes`/`ExtractModelCenter`/`ExtractModelAxisScale`(billboard_common), `AtlasSubUV`/`AtlasCell`(texture_atlas), `Luminance`/`GaussianWeight`/`TexelSize`/`kOffset3x3`/`Sample3x3`/`Dot9`/`Dot9Color`(image_processing) — 정의 태스크와 소비 태스크 시그니처 일치 확인.

## 미해결 / 리스크

- **Task D2 스파이크가 최대 리스크.** sampler-as-param + 배열 out-param 이 macOS GL410 을 통과 못 하면 폴백(image_processing 을 pure-only 로 축소, convolution 로컬 유지)이 발동. 이 경우 Task D3(sharpening)·D4 는 조정 필요 — 스파이크 결과에 따라 확정.
- **`static const float2 kOffset3x3[9]` 모듈 스코프 배열** 이 소비처 GLSL 에 어떻게 emit 되는지 미검증(기존 `normalize_array_initializers` 가 잡을 것으로 예상하나 모듈 스코프는 첫 사례). D2 트랩점검에서 확인.
- **`AtlasCell` 의 다중 out-param(B3)** 도 유사 미검증 리스크 — B3 트랩점검·런타임 필수.
- **육안 검증 의존:** 골든 이미지 자동 비교(SJH_GOLDEN_CAPTURE)는 별도 worktree(game/golden-capture, 미병합)라 본 플랜은 육안. 픽셀 회귀 자동화가 필요하면 그 worktree 병합 후 별도 게이트 추가.
- **d71e2ff 복원 대안:** postfx_common+billboard_common(Phase A·C)은 d71e2ff 에 실증 코드가 있어 `git checkout d71e2ff -- <파일>` cherry-pick 도 가능(reflog gc 전까지). 본 플랜은 재작성이나, 시간 절약 시 cherry-pick 후 검증만 수행 가능.
