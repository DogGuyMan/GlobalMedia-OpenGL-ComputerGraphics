# HANDOFF — Slang refl.json → C++ std140 미러 build-time codegen

> **수신자:** 신규 Claude Code 세션/에이전트 (이 대화 컨텍스트 없음 가정). 자기완결.
> **작성:** 2026-06-21 · **상태:** 🟡 *미착수, 설계만.* 사용자 결정 = **Option C (codegen)** 채택, "당장 아님 — 별 핸드오프로".
> **상위:** Phase 3 의 D-DPP-1 (Dynamic Properties / 정적 블록 패킹 방식) 의 *정적 블록* 갈래. Phase 3 핸드오프 §9 참조: [`2026-06-21-slang-phase3-propertyblocksetter-removal-handoff.md`](./2026-06-21-slang-phase3-propertyblocksetter-removal-handoff.md).
> ⚠ **gitignore 로컬** (`doc/`). ⚠ **toolchain 유동:** 사용자가 Slang 파이프라인을 능동 리팩토링 중 — *모든 경로/스크립트 위치를 착수 시 재확인*.

---

## 0. TL;DR + 다음 액션

**문제:** `.slang` 의 std140 UBO 블록(struct)을 C++ 에서 패킹하려면 *손으로 쓴 C++ 미러 struct + static_assert* 가 필요하다 (현재 `light_ubo_uploader.cpp` 의 `LightBlockStd140` 등). `.slang ↔ C++` 가 **byte 단위로 수동 동기화** — 블록이 늘면(Phase 3) 중복·유지보수 폭증.

**해결 (Option C 확정):** **build-time codegen** — Python(`scripts/slang_compile.py` 파이프라인)이 `*.refl.json` 을 읽어 **C++ std140 미러 struct + static_assert 를 자동 생성**(`.h`). → 수동 동기화 0 + zero-runtime(직접 struct memcpy 유지) + 컴파일타임 안전(static_assert). A(수동)·B(런타임 reflection)의 장점 합.

**왜 지금 아님:** 현재 미러는 LightBlock *1개* — design-decision-discipline (추상화는 반복 2회+ 후). 단 Phase 3 에서 skybox/postfx/healthbar 등 다수 블록이 UBO 화되면 그때 값을 함. 본 핸드오프는 *그때 또는 prototyping 으로 LightBlock 에 먼저* 적용할 설계.

**다음 액션 (착수 시):**
1. `scripts/slang_compile.py` 에 `--codegen-out <header.h>` 모드 추가 (refl.json → C++ .h). §4 설계.
2. `cmake/Slang.cmake` 에 `sjh_codegen_slang(...)` 얇은 wrapper + `shaders_slang/CMakeLists.txt` 에서 phong 블록 codegen 호출.
3. `light_ubo_uploader.cpp` 의 수동 미러(§5)를 생성 헤더 include 로 교체. **육안/빌드 검증.**

---

## 1. State of the world (re-measured 2026-06-21 저녁)

- **브랜치:** `game/slang-phase2-ubo` · **HEAD:** `3228f71 [dev] : UBO 매핑함수 추가` (구 기록 `be9dcad` 에서 +4 커밋 드리프트 — 사용자 병렬 imgui/glm/cmake 작업).
- ⚠ **Gate Bᴳ 도달 (2026-06-21 저녁):** 전 셰이더 Slang/UBO화 완료. 정적 블록(MaterialBlock)은 이제 **다수** 존재 (transparent/healthbar/billboard_atlas/skybox/postfx 등) → 본 codegen 의 "추상화는 반복 2회+ 후" 조건 충족 시점 도래. 단 각 MaterialBlock 멤버 업로드는 현재 `3228f71` 의 **런타임 reflection (UpdateUniformMember, author 이름→offset)** = D-DPP-1(b) 로 처리 중 — codegen(C)은 *수동 미러 struct(LightBlock 류 직접 memcpy)* 에만 적용. 즉 codegen 의 1차 대상은 여전히 `light_ubo_uploader.cpp` 의 수동 미러 1개 (§5). 다른 MaterialBlock 들은 (b) 경로라 미러 없음.
- **`3228f71` (D-DPP-1(b) 실체):** `Program::UpdateUniformMember` + `mUniformMembers`(author 이름→{블록, std140 offset}) GL introspection + mesh_pass `UploadMaterialUboMembers`. **런타임 KEY postfx + 컴파일타임 KEY material 멤버를 동일 경로로 흡수** → §8 의 "(b) 런타임 reflection" 갈래가 이미 구현됨. codegen(C)과 공존 (정적 미러 vs 런타임 lookup, 상보).
- **Slang 파이프라인 스크립트:** `scripts/slang_compile.py` (← `cmake/` 에서 이주됨). `cmake/Slang.cmake` 가 `SJH_SLANG_COMPILE_SCRIPT` 변수(root `CMakeLists.txt` 정의)로 참조, `sjh_compile_slang`/`sjh_reflect_slang` 가 얇은 wrapper. 현재 모드: slangc 호출 + GLSL 410 post-process(varying 정규화 포함) + reflection(`--reflection-out`) + depfile. **codegen 은 여기에 모드 추가 또는 sibling.**
  - ⚠ 이 위치/이름은 사용자 병렬 리팩토링으로 *드리프트 중* — 착수 시 `grep SJH_SLANG_COMPILE_SCRIPT CMakeLists.txt cmake/Slang.cmake` 로 재확인.
- **refl.json 산출:** `build_ninja/apps/_MyApp_/.../*.refl.json` (slangc `-reflection-json`). 빌드 산출물(비커밋).
- **Phase 2.5 완료:** LightBlock UBO + `LightUboUploader` (수동 미러). [[slang-toolchain-conventions]].

---

## 2. 문제 정밀화 — 무엇이 중복인가 (grounded)

"광원" 은 3중 표현이나 **순수 중복은 `.slang struct ↔ C++ 미러` 하나**:

| 표현 | 위치 | 성격 |
|---|---|---|
| 컴포넌트 | `scene/light.h` `DirLight/…` | 게임 데이터 (degree/Distance) — *변환 전 원본, 중복 아님* |
| GPU struct (SSOT) | [`phong_lighting.slang:17-56`](../../apps/_MyApp_/shaders_slang/phong_lighting.slang#L17) `DirLightS/…/LightBlock` | std140 GPU 레이아웃 |
| **C++ 미러** | [`light_ubo_uploader.cpp:46-97`](../../src/render/light_ubo_uploader.cpp#L46) `*Std140` + static_assert 13 | **GPU struct 와 순수 중복 (codegen 대상)** |

`Update()`(:218-256) 가 컴포넌트→미러 *의미 변환*(`cosf(radians(cutoff))`, `GetAttenuationCoeff(Distance)`)을 한다 — **이 패킹 코드는 codegen 후에도 남는다** (생성 struct 의 필드에 대입). codegen 이 없애는 건 *struct 정의 + 패딩 + static_assert* 뿐.

---

## 3. Option 비교 (왜 C — 재확인)

| 옵션 | 수동동기화 | runtime | 컴파일타임안전 | 비고 |
|---|---|---|---|---|
| A 수동 미러 (현재) | ✗ 필요 | 0 | ✓ | 블록 1개엔 OK, 다수엔 폭증 |
| B 런타임 reflection lookup | 0 | lookup 비용 | ✗ (필드명 런타임) | **postfx 런타임 KEY 엔 필수** (D-DPP-1 b) |
| **C codegen (채택)** | **0** | **0** | **✓** | 정적 블록 최적. 빌드 codegen 인프라 비용 |

→ **정적 블록(LightBlock/MaterialBlock/…) = C, postfx 런타임 KEY = B** 가 자연 (D-DPP-1 하이브리드). 본 핸드오프 = **C 갈래.**

---

## 4. Codegen 설계 (핵심)

### 4.1 입력 = refl.json (실측 구조, 2026-06-21 phong.refl.json)
`parameters[]` 중 `type.kind == "constantBuffer"` 가 블록. 각 블록 `type.elementType.fields[]`:
- `name`, `binding.{offset, size}`, `type.kind` ∈ {`matrix`, `vector`, `scalar`, `struct`, `array`}.
- `vector`: `elementCount`(3/4) + `scalarType`. `matrix`: 4x4 (size 64). `scalar`: `scalarType`(`int32`/`float32`).
- `struct`: 중첩 `fields[]` (재귀). `array`: `elementCount`(count) + `uniformStride` + `elementType`(struct 면 재귀).

실측 예 (uLight = LightBlock):
```
dirLight    struct off=0  size=64        { direction vec3@0, ambient@16, diffuse@32, specular@48 }
pointLights array  off=64 size=1280      elem=struct count=16 stride=80  { position@0, attenuation@16, ambient@32, diffuse@48, specular@64 }
spotLights  array  off=1344 size=1792    elem=struct count=16 stride=112 { position@0, direction@16, cutoff(scalar)@28, outerCutoff@32, attenuation@48, ambient@64, diffuse@80, specular@96 }
viewPos     vector off=3136 size=12
dirLightEnabled/numPointLights/numSpotLights scalar off=3148/3152/3156 size=4
```
⚠ **`spotLights.cutoff @28`** = direction vec3(@16, 12B) 의 4번째 슬롯에 패킹 — std140 비대칭. **codegen 은 offset 을 *계산하지 말고 refl.json 에서 읽어야* 한다** (가정 금지).

### 4.2 출력 = C++ .h (offset-driven, typed 필드 + 패딩 + static_assert)
**offset-driven 패딩 알고리즘** (모든 std140 quirk 자동 처리):
- 각 struct 의 필드를 *offset 순서* 로 방출. 이전 필드 끝(E)과 현 필드 offset(O) 사이 gap = `char _padN[O-E];` 삽입. 마지막에 블록 size 까지 tail pad.
- 필드 타입 = §4.3 매핑. 중첩 struct 는 재귀 생성, array 는 `Elem[count]`.
- 각 필드 offset + struct size 를 `static_assert(offsetof(...)==O)` / `static_assert(sizeof(S)==size)` 로 검증 (생성).

생성 예 (개략):
```cpp
// AUTO-GENERATED from phong.slang reflection — DO NOT EDIT.
#ifndef __SJH_GEN_PHONG_BLOCKS_H__
#define __SJH_GEN_PHONG_BLOCKS_H__
#include <vmath.h>
#include <cstdint>
#include <cstddef>
namespace SJH::Generated {
struct LightBlock_SpotLightS {
    vmath::vec3 position;  char _p0[4];   // @0
    vmath::vec3 direction; float cutoff;   // @16, @28
    float outerCutoff;     char _p1[12];  // @32
    vmath::vec3 attenuation; char _p2[4]; // @48
    ...
};
static_assert(sizeof(LightBlock_SpotLightS)==112);
static_assert(offsetof(LightBlock_SpotLightS, cutoff)==28);
...
struct LightBlock { LightBlock_DirLightS dirLight; LightBlock_PointLightS pointLights[16]; ... };
static_assert(sizeof(LightBlock)==3168);
}
#endif
```

### 4.3 타입 매핑표 (codegen 내부)
| refl kind | 조건 | C++ 타입 |
|---|---|---|
| matrix | 4x4 (size 64) | `vmath::mat4` |
| vector | elementCount 4 | `vmath::vec4` |
| vector | elementCount 3 | `vmath::vec3` (12B — 뒤 gap 은 pad 가 처리) |
| vector | elementCount 2 | `vmath::vec2` |
| scalar | scalarType float32 | `float` |
| scalar | scalarType int32 | `std::int32_t` |
| struct | — | 생성된 중첩 struct |
| array | count N | `<Elem>[N]` |

> `static_assert(sizeof(vmath::vec3)==12)` 를 생성 헤더 상단에 1회 (mirror 전제). vmath::vec2/vec3/vec4/mat4 크기 = 8/12/16/64.

---

## 5. 교체 대상 — 현재 수동 미러 (grounded, 삭제/대체)

[`src/render/light_ubo_uploader.cpp`](../../src/render/light_ubo_uploader.cpp):
- `:46-81` 수동 struct 4종 (`DirLightStd140`/`PointLightStd140`/`SpotLightStd140`/`LightBlockStd140`) → **생성 헤더로 교체.**
- `:83-97` static_assert 13개 → **생성 헤더가 자동 방출.**
- `:217` `LightBlockStd140 block{};` → `Generated::LightBlock block{};` (+ 멤버 접근 경로가 동일하도록 생성 필드명을 .slang 필드명과 일치시킬 것 — `block.dirLight.direction`, `block.pointLights[i].position` 등).
- `:218-268` `Update()` 패킹 로직 = **유지** (의미 변환). 단 멤버명이 생성 struct 와 1:1 이어야 함.

→ codegen 은 *struct 정의만* 대체, 패킹/의미변환은 불변. include 1줄 추가 + struct 이름 교체.

---

## 6. 통합 (build wiring) — 착수 시 위치 재확인

1. **`scripts/slang_compile.py`**: `--codegen-out <out.h>` 인자 추가 (이미 `--reflection-out` 모드 존재 → 같은 refl 데이터 재사용). refl JSON → §4 알고리즘 → `.h` 방출. 네임스페이스/가드/주석은 프로젝트 컨벤션(ASCII, `__SJH_X_H__`).
2. **`cmake/Slang.cmake`**: `sjh_codegen_slang(OUT_VAR INPUT ENTRY STAGE OUT_HEADER)` 얇은 wrapper (sjh_reflect_slang 형태 복제, `--codegen-out`). DEPFILE 동일.
3. **`apps/_MyApp_/shaders_slang/CMakeLists.txt`**: phong 블록용 `sjh_codegen_slang(_PHONG_GEN ${PHONG_SLANG} fsMain fragment ${_slang_gen_dir}/phong_blocks.h)` + `_shaders` 타겟 DEPENDS 에 추가. 생성 헤더 위치를 `target_include_directories` 로 `light_ubo_uploader.cpp`(render 모듈) 가 보게 — ⚠ 생성물이 `apps/` 인데 소비자가 `src/render/` 라 **include 경로 노출 설계 필요** (아래 §7 결정).
4. **`src/render/light_ubo_uploader.cpp`**: `#include "<generated>/phong_blocks.h"` + §5 교체.

---

## 7. 수신자 결정 사항 (착수 전 사용자/설계 확정)

| # | 결정 | 옵션 |
|---|---|---|
| G1 | **생성 헤더 거주/노출** | 생성물이 `apps/_MyApp_/.../generated/` 인데 소비자 `light_ubo_uploader.cpp` 는 `src/render/` (엔진 코어). (a) 엔진이 app 산출 헤더 include = 레이어 역전 ✗ / (b) **LightBlock 미러를 *app 측* 으로 이동** (LightUboUploader 가 엔진인데 LightBlock 은 app phong.slang 산출 — 이 긴장 자체가 설계 질문) / (c) 생성 헤더를 `src/render/generated/` 로 방출. **권고 = 먼저 이 레이어 질문부터** (엔진의 LightUboUploader 가 app 셰이더 블록을 아는 게 맞나? — Phase 3 에서 LightBlock 거주지 재고와 연동). |
| G2 | **typed 필드 vs char 버퍼** | (a) typed(vec3+pad, 현재 스타일, 접근 편함) / (b) `char data[size]` + offset 상수(완전 offset-driven). **권고 = (a)** (Update 패킹이 멤버 접근). |
| G3 | **어느 블록 codegen** | LightBlock 만(현 소비자) / 전 블록(Frame/Draw/Material 도). **권고 = LightBlock 우선** (Frame/Draw 는 mesh_pass 가 `sizeof(mat4)` offset 직접 사용 — 미러 불요). |
| G4 | **scalarType 판별** | refl.json `scalarType` 필드로 int32/float32 구분 (cutoff=float vs dirLightEnabled=int). codegen 필수 처리. |

---

## 8. Phase 3 와의 관계
- 본 codegen = D-DPP-1 의 **정적 블록 갈래(C)**. postfx *런타임 KEY* 3사이트(HpGrayscale/Tween/render_pipeline)는 **별도 B(런타임 reflection)** 필요 — codegen 으로 안 풀림 (키가 컴파일타임에 없음).
- 착수 타이밍: (a) Phase 3 진입 후 다수 블록 생길 때 / (b) 지금 LightBlock 1개로 *prototype* (인프라 검증 후 Phase 3 재사용). **사용자 = "당장 아님"** → (a) 또는 D-DPP-1 spec 시점.

---

## 9. Guardrails & conflict matrix
- **커밋:** 사용자 직접. 자발 금지, 요청 시 path-scoped. `Co-Authored-By` 미사용. 한국어 메시지.
- **빌드:** `export PATH="$HOME/slang/bin:$PATH"` → `cmake --preset ninja` → `--target _MyApp_`. 검증 = 빌드 GREEN(static_assert 통과 = 레이아웃 정합) + 런타임 GL 로그 + 육안 (LightBlock 미러 교체 후 PCB 조명 동일).
- **코드 컨벤션:** 생성 헤더도 ASCII/한글 주석 + `__SJH_X_H__` 가드 + `// AUTO-GENERATED DO NOT EDIT` 배너.
- **★ toolchain 유동 (건드리기 전 재확인):**

| 파일 | 소유 | 비고 |
|---|---|---|
| `scripts/slang_compile.py` | **사용자 (능동 리팩토링)** | codegen 모드 *추가* — 기존 reflection/post-process 모드 보존. 착수 시 현 구조 재확인. |
| `cmake/Slang.cmake` / root `CMakeLists.txt`(`SJH_SLANG_COMPILE_SCRIPT`) | 사용자 | wrapper 추가만. |
| `apps/_MyApp_/shaders_slang/CMakeLists.txt` | 사용자 | codegen 호출 라인 추가. |
| `src/render/light_ubo_uploader.cpp` | 본 작업 | 미러 교체 (G1 레이어 결정 선행). |
| `cmake/slang_compile.py` (구) | **이미 이주됨** | `scripts/` 로 옮겨짐 — 옛 경로 참조 금지. |

---

## 10. Verification
- `static_assert` 가 *생성 헤더 안에서* 통과 = refl.json offset 과 C++ 정합 (현재 수동과 동일 안전망, 자동).
- 생성 struct 의 멤버 경로가 `Update()` 패킹 코드와 일치 (빌드 에러로 즉시 드러남).
- 런타임: PCB 조명이 교체 전후 *동일* (육안). varying 류 함정 없음 (struct 만 교체, 셰이더 불변).

## 11. Pointers
- **상위:** Phase 3 핸드오프 §9 D-DPP-1 [`2026-06-21-slang-phase3-propertyblocksetter-removal-handoff.md`](./2026-06-21-slang-phase3-propertyblocksetter-removal-handoff.md).
- **현재 수동 미러:** `src/render/light_ubo_uploader.cpp:46-97`.
- **toolchain:** [[slang-toolchain-conventions]] (slang_compile.py + depfile + overlay). [[slang-varying-name-mismatch-macos]].
- **refl.json 입력 실측:** 본 §4.1 (phong.refl.json 2026-06-21 dump).

## Change log
- 2026-06-21: 최초 작성. 사용자 Option C(codegen) 채택 → 별 핸드오프 요청. refl.json 입력 구조 + offset-driven 패딩 알고리즘 + 타입 매핑 + 교체 대상(light_ubo_uploader.cpp:46-97) + 통합 wiring + G1~G4 결정사항(특히 G1 엔진↔app 레이어 긴장) grounded. slang_compile.py 가 `scripts/` 로 이주됨 재측정.
- 2026-06-21(저녁): HEAD 드리프트(be9dcad→3228f71, +4커밋) 반영. **Gate Bᴳ 도달** — 정적 블록 다수화로 codegen 조건 충족. 단 MaterialBlock 멤버는 `3228f71`(D-DPP-1(b) 런타임 reflection `UpdateUniformMember`)로 처리 중이라 codegen 1차 대상은 여전히 `light_ubo_uploader.cpp` 수동 미러 1개. codegen(C, 정적 미러)과 reflection(b, 런타임 lookup)은 상보 공존.
