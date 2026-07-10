# HANDOFF -- Slang Post-Phase 2 전략: PropertyBlockSetter 제거를 향한 점진 정통화

> **수신자:** 신규 Claude Code 세션/에이전트 (이 대화의 컨텍스트 없음 가정).
> **작성:** 2026-06-20 · **갱신:** 2026-06-21 (Phase 2.5 DONE 반영) · **상태:** 🟢 **Phase 2.5 완료 — 다음 = Phase 3 (§4 D-DPP).**
> **선행 핸드오프:** [`doc/handoffs/2026-06-20/2026-06-20-slang-phase2-engine-ubo-handoff.md`](./2026-06-20-slang-phase2-engine-ubo-handoff.md) -- 본 핸드오프는 그것의 *후속 전략*을 다룬다.
> 이 문서 하나로 후속 작업을 안전하게 시작 가능하도록 자기완결로 작성.

---

## 0. TL;DR + 다음 액션

`apps/_MyApp_` 의 Slang 마이그레이션은 Phase 1(빌드툴체인 머지) + Phase 2(엔진 UBO 인프라, 별 핸드오프) 진행 중. 본 핸드오프는 Phase 2 *완료 후* 의 두 가지 진행 경로를 정리한 *전략 결정 문서* 다.

**핵심 결론 (이 분석 세션의 산출):**
- **끝점은 빅뱅이 정통** -- 모든 셰이더 UBO + `PropertyBlockSetter`/`uniform_cache` 모듈 제거 = Unity/Unreal/Godot 정통.
- **경로는 점진** -- 지금 한 PR 에 다 넣으면 (B1) Dynamic Properties 패턴 폐기라는 *별도 결정 spec* 이 누락된 채 게임 로직 곳곳이 깨진다.
- **권고 진행:** Phase 2 (현 핸드오프) → **Phase 2.5** (phong + LightBlock 1셰이더 PoC) → **Phase 3** (잔여 + 결정 spec 기반 PropertyBlockSetter 제거).

**다음 액션 (사용자 pick 후):**
- (A) 점진 = 본 핸드오프 §3 Phase 2.5 진행.
- (B) 빅뱅 = 본 핸드오프 §4 의 *Dynamic Properties 폐기 decision spec* 부터 작성 (코드 진입 전).

**🟢 Phase 2.5 DONE (2026-06-21) — PICK-1=A / PICK-2=S5b 확정 + 구현 완료:**
- **phong UBO 화:** PCB 스테이지가 `phong.vs/fs` (albedo, LightBlock UBO) 사용. mesh_pass `useUbo` 분기가 PropertyBlockSetter 우회 (**S7 — phong callsite 절단**). 육안 ✅ ("딱 잘된다").
- **dispatcher O4 승격:** `LightUniformDispatcher` → `LightUboUploader` (공유 LightBlock UBO 소유 + `Update`/`BindTo` + `Program::DisownUniformBlock`). 자세히 §6 D-LUD.
- **라이팅 SSOT 모듈:** `phong_lighting.slang` (`import` + `ComputePhong()` 한 줄, 26항목×3 복제 제거).
- **🐛 버그 + 해결 (정본 [`doc/Slang버그리포트.md`](../Slang버그리포트.md)):** PCB 투명 = Slang stage별 varying 이름 불일치 ↔ macOS GL 이름매칭 → 런타임 GL 링크 실패 → program=null → skip. **`cmake/slang_compile.py` 가 `_slangVaryN` 정규화** (stage-aware). + overlay staleness fix (`_shaders` 매-빌드 COMMAND) + 툴체인 단일화 (slang_compile.py + depfile). 메모리 [[slang-varying-name-mismatch-macos]] / [[slang-toolchain-conventions]].
- **dead 정리:** `phong_tex.{vs,fs}` / `phong_albedo.fs` 제거 (PCB 가 phong.* 로 이전, 사용처 0 grep 확인).
- **잔여 PICK:** PICK-4 (셰이더 재고 매트릭스) = 여전히 미결 → Phase 3 진입점.

**다음 세션 = Phase 3** — §4 의 D-DPP-3(셰이더 재고) + **D-DPP-1 (Dynamic Properties 폐기 결정 spec, *코드 전 필수*)** 부터. 그 후 잔여 셰이더 UBO 화 → D-DPP-4 (PropertyBlockSetter/uniform_cache 모듈 제거).
> 🟢 **Phase 3 실행 정본 = [`doc/handoffs/2026-06-21/2026-06-21-slang-phase3-propertyblocksetter-removal-handoff.md`](./2026-06-21-slang-phase3-propertyblocksetter-removal-handoff.md)** (본 §4 를 grounded 실측으로 정밀화 — B1 blocker = 런타임 KEY 3사이트로 좁힘, 셰이더 전수 인벤토리, 라인번호 재측정). Phase 3 착수 시 *그 문서* 를 단일 진입점으로.

---

## 1. State of the world (2026-06-21 갱신)

- **Phase 1 (slangc 툴체인) 머지 완료** -- `game/main` 의 squash `90353f4 [build] : Slang GLSL 410 ...`.
- **Phase 2 (엔진 UBO 인프라) = 🟢 T1~T5 완료 (2026-06-21).** 브랜치 `game/slang-phase2-ubo` 에서 진행:
  - **T1** `simple.slang` 1→3 블록 (Frame/Draw/Material) ✅
  - **T2** `SJH::UniformBuffer` RAII (src/program/ 거주, 사이클 회피) ✅
  - **T3** `Program::UniformBlock` introspection + 소유 (`HasUniformBlocks`/`Find`/`Bind`/`Update`/`BuildUniformBlocks`) ✅
  - **T4** `MeshPassProcessor` `useUbo` 분기 (비전치 D13, loose 경로 공존) ✅
  - **T5** `bullet_factory.h:132-133` PoC 활성화 + R1 ✅ (마젠타 정상 위치/색상 = D13 비전치 정합 확정, T6 전치 분기 *불요*).
  - 사용자 보고 = "마젠타가 잘 보임" (2026-06-21).
- **분석 세션 산출 (2026-06-20):**
  - 의존 그래프 (2-depth) + 문제 시사 P1~P5 작성.
  - Unity/Unreal/Godot 3-way 정통성 비교 (Context7 인용).
  - PropertyBlockSetter 제거 빅뱅 안의 5대 비용 (B1~B5) 식별.
  - **(B1) Dynamic Properties 패턴이 *진짜 blocker*** -- 자세한 내용 §4.
- **부수 산출 (2026-06-21):**
  - Slang→GLSL 의 `layout(row_major) uniform;` 와 row-vector 곱셈 순서 짝 출력 = *수학적 자기일관성* 확인 (row-vector × row_major ≡ column-vector × column_major). `vmath::mat4` raw 바이트 송신 정합 확정.
  - `simple.refl.json` 의 std140 offset 메타 (`uView @0 size 64`, `uProj @64 size 64`) = T4 `UpdateUniformBlock(..., sizeof(mat4), 0/64)` 호출과 정확 일치 → **§4 D-DPP-1 옵션 (b) Slang reflection 런타임 lookup 의 정통성 근거 확보**.
  - 성능 회귀 진단 방법론 핸드오프 신설 → [`doc/handoffs/2026-06-21/2026-06-21-perf-regression-system-triage-handoff.md`](./2026-06-21-perf-regression-system-triage-handoff.md) (직접 관련 없음, 부수 학습).
- **사용자 최종 결정 미완:** "(A) 점진" vs "(B) 빅뱅" 둘 중 pick 대기.

---

## 2. Locked strategic decision (재논의 금지 -- 분석 세션 lock)

| # | 결정 | 근거 |
|---|---|---|
| S1 | **PropertyBlockSetter + uniform_cache 모듈의 *최종 제거*가 정통 끝점** | Unity URP/HDRP 가 5.x 에서 legacy property 폐기. Unreal `BEGIN_UNIFORM_BUFFER_STRUCT` / Godot RD `uniform_set_create` 모두 UBO 가 *기본 ABI*. loose `glUniform*` 은 GL 2.x 잔재. |
| S2 | **단 *한 PR* 에 그 끝점까지 모두 넣지 *않는다*** | (B1) Dynamic Properties 폐기는 별도 결정 spec 이 필요. spec 부재 상태로 코드만 진행 불가 (§4). |
| S3 | **R1(D13) 의 *N→N+1 확장 정통성* 은 PoC 1셰이더 확정 후 검증** | 빅뱅은 모든 셰이더 동시 R1 검증 → 실패 격리 불가. 점진은 simple(Phase 2) → phong(Phase 2.5) 두 단계로 row_major 정합 확장 정통성 확인. |
| S4 | **공존 정책 = Phase 2 의 `useUbo` 분기는 *임시* 가 아니라 *Phase 3 까지 의도된 상태*** | `mesh_pass_processor.cpp` 의 if/else 가 Phase 2.5 까지는 UBO/loose 공존 게이트. Phase 3 에서 *전체 셰이더 UBO 화 완료 후* else 가지 제거 + PropertyBlockSetter 제거. |

**(P4 cache miss 메커니즘 정정 -- Phase 2 핸드오프의 표현 보강, 🟢 2026-06-21 빌드 실측 확인):**
[`src/program/uniform_cache.cpp:39-51`](../../src/program/uniform_cache.cpp#L39-L51) 의 `Build` 는 `glGetActiveUniform` 으로 UBO 멤버를 *enumerate 한다*. 단 `glGetUniformLocation` 이 -1 을 반환해 `mEntries[name] = { Location=-1, Type }` 로 저장. PropertyBlockSetter 가 `glUniform*(loc=-1, ...)` 호출 시 GL 스펙상 silent no-op -- 이게 "자연 skip" 의 정확한 메커니즘. (Phase 2 핸드오프의 "active uniform 으로 노출되지 않으므로" 표현은 부정확하나, 결과적 행동은 동일 -- 코드 변경 불필요.) **2026-06-21 빌드 실측 = 마젠타 불릿 R1 ✅ 시점에 *비-UBO 셰이더 외관 동일* 확인 → 공존 메커니즘 정상 작동 검증.**

---

## 3. Phase 2.5 -- phong + LightBlock UBO 확장 PoC (옵션 A 가지)

### 3.1 목표
Phase 2 가 *simple/불릿 1 셰이더* 로 R1 비전치 정합을 확정한 직후, **phong 셰이더 1개** 를 .slang 변환해서 같은 D11~D13 패턴이 *N→N+1 확장* 에서도 정합인지 검증.

### 3.2 새로 도입할 결정 (Phase 2 lock 결정 외 추가)

| # | 결정 | 옵션 |
|---|---|---|
| S5 | **LightBlock UBO scope** | (a) per-program LightBlock (모든 phong 사용 program 마다 별도 UBO) -- 단순 / (b) per-frame *공유* LightBlock (단일 UBO 를 모든 program 에 같은 binding point 로 결속) -- 정통(Unity `UnityPerFrame`, Unreal `FViewUniformShaderParameters`). **권고 = (b)**, 단 Phase 2 의 `Program::UniformBlock` 자기기술 모델이 program-소유 가정이라 *공유 UBO 의 owner* 결정 필요 (LightUniformDispatcher 가 소유 후 모든 program 에 bind 만 위탁?). |
| S6 | **light 배열 std140 layout** | `MAX_POINT_LIGHTS=16` + `MAX_SPOT_LIGHTS=16` (memory 의 `shader_max_lights_16` 컨벤션 유지). std140 vec3 alignment 가 16B 이므로 light struct 정렬 검증 필요. |
| S7 | **PropertyBlockSetter 의 첫 callsite 제거 -- phong material 전용** | phong 의 Material PropertyBlock 접근을 *완전히* UBO 분기로 옮긴다. PropertyBlockSetter 호출은 phong 셰이더에서만 *우회*. 비-phong 셰이더(skybox/postfx/decal/sprite) 는 Phase 3 까지 PropertyBlockSetter 보존. |

### 3.3 Task 슬라이스 (페이스 가벼움 -- 1세션 목표)

| Task | 내용 | 검증 |
|---|---|---|
| P25-T1 | `apps/_MyApp_/shaders_slang/phong.slang` 작성 (FrameBlock/DrawBlock/MaterialBlock + LightBlock). LightBlock 은 `PointLight[16]` + `SpotLight[16]` + `DirectionalLight` + `viewPos`. | `cmake --build --target _MyApp__shaders` + grep `uniform block_LightBlock_0` |
| P25-T2 | (S5(b) 채택 시) LightBlock UBO 의 *공유 owner* 도입 -- 후보 위치: `SJH::LightUniformDispatcher` 가 `UniformBuffer` 한 개 소유 후 모든 program 에 `glBindBufferBase` 위탁. Phase 2 의 `Program::UniformBlock` 자기기술과의 *이중 소유* 충돌 회피 결정 필요. | 빌드 GREEN + 단일 LightBlock UBO 가 N program 에 동일 binding point 로 결속 (RenderDoc 또는 spdlog) |
| P25-T3 | `MeshPassProcessor` 의 `useUbo` 분기에 LightBlock 갱신 경로 추가. LightUniformDispatcher 의 기존 loose 송신은 비-UBO 셰이더용으로 *공존 보존* (S4). | phong 셰이더에서 light 정상 작동 (R1 확장 검증) |
| P25-T4 | phong material PropertyBlock 의 *컴파일타임 알려진 키* (예: shininess, specular) 를 UBO MaterialBlock 멤버로 흡수. *동적 키* (예: 런타임 `mat->Properties.Floats["uTime"]`) 가 phong 에 *없는지* 사전 audit. 있으면 Phase 3 spec 으로 위탁. | 빌드 GREEN + phong 외관 동일 |
| P25-T5 | R1 *N→N+1 확장* 육안 게이트 -- phong 렌더되는 액터(예: 적, 스테이지 벽) 가 정상 위치/조명. | 사용자 육안 보고 ✅/❌ |
| P25-T6 (조건부) | T5 가 ❌ 면 phong 셰이더에 한해 행렬 전치 분기. Phase 2 의 simple 셰이더가 비전치 ✅ 였는데 phong 이 ❌ 라면 **단 셰이더별 row_major 비대칭** -- Slang 출력 layout 키워드 셰이더별 검증 후 spec 분기. | 재빌드/재실행/육안 |

### 3.4 가드레일 (Phase 2 상속 + Phase 2.5 추가)
- Phase 2 핸드오프 §4 가드레일 전체 상속 (path-scoped commit, Co-Authored-By 미사용, 한국어 + Doxygen + ASCII, `__SJH_X_H__` 헤더가드, 명명 컨벤션, no_auto_tests, 사용자 직접 빌드).
- **추가 가드레일:**
  - phong 외 셰이더 (skybox/postfx/decal/sprite/bitmap_font/world_text) 는 *건드리지 않는다*. Phase 3 영역.
  - `LightUniformDispatcher` 의 loose 송신 경로는 *제거하지 않고 공존 보존* (S4).
  - `MaterialPropertyBlock` 의 `Properties.Floats/Vec3s/Vec4s/Mat4s` map 자체는 *건드리지 않는다*. phong UBO 멤버 추출만.
  - Phase 2.5 PR 은 *phong 1셰이더 + LightBlock* 범위 한정 -- 빅뱅 회피.
- **Phase 2 학습 추가 (2026-06-21 갱신):**
  - **fail-fast 정책**: UBO 생성 실패 (`UniformBuffer::Create` nullptr) 시 `std::fprintf(stderr) + std::abort()` 채택 (사용자 결정). `src/program` 모듈은 game_deps 미링크라 **spdlog 사용 불가** -- LightBlock 도입 시에도 동일 패턴 (stderr + abort).
  - **clangd false positive 사전 인지**: `gl3w.h` 의 strict include 정책이 `GLuint`/`GLint`/`GLenum`/`std::size_t` 등을 *missing/unused* 로 잘못 표시. 기존 `program.h`/`uniform_cache.h` 등 동일 패턴 -- *프로젝트 컨벤션상 무시*. 빌드 GREEN 으로 실 컴파일 정합 확인 (Phase 2 의 모든 진단 false positive 였음).

---

## 4. Phase 3 prerequisite -- Dynamic Properties 폐기 decision spec (옵션 B 또는 Phase 2.5 후속)

> 🟢 **이 §4 는 [`doc/handoffs/2026-06-21/2026-06-21-slang-phase3-propertyblocksetter-removal-handoff.md`](./2026-06-21-slang-phase3-propertyblocksetter-removal-handoff.md) 로 grounded 정밀화됨.** 아래 §4.1~4.3 은 *원본 분석* (B1 을 "전부 깨짐" 으로 과대 서술). 실측 교정: B1 의 진짜 blocker 는 **런타임 KEY 3사이트** (HpGrayscalePostFX/PostFXTweenPlayable/render_pipeline), 나머지는 컴파일타임 KEY 라 기계적 변환. Phase 3 착수는 *그 문서* 의 §5/§9/§10 을 따른다.

### 4.1 왜 spec 이 *코드 진입 전* 에 필요한가

[`apps/_MyApp_/src/Playable/HpGrayscalePostFX.cpp:46`](../../apps/_MyApp_/src/Playable/HpGrayscalePostFX.cpp#L46),
[`apps/_MyApp_/src/Playable/PostFXTweenPlayable.cpp:42`](../../apps/_MyApp_/src/Playable/PostFXTweenPlayable.cpp#L42),
[`apps/_MyApp_/main.cpp:174 / :351 / :357`](../../apps/_MyApp_/main.cpp) 등이 모두 **런타임 동적 키 추가** 에 의존:

```cpp
mat->Properties.Floats[mUniformName] = ratio;        // uniformName 이 런타임 string
mat->Properties.Floats["u_time"]     = currentTime;
mat->Properties.Vec3s["uFogColor"]   = FOG_COLOR;
mat->Properties.Mat4s["uInverseProj"] = camera->GetInverseProjectionMatrix();
```

UBO 멤버는 **.slang 에 *선언된 키만* 존재**. 빅뱅 = 위 코드가 *전부 컴파일 안 됨*. 특히 직전에 머지된 D-7 PostFXRegistry 흡수 (rr 의 `mat_pass_<name>` 공유본 직접 조회) 의 ratio 갱신 시그니처 *재설계 강제*.

### 4.2 spec 이 답해야 할 결정

| # | 결정 | 옵션 (택일) |
|---|---|---|
| D-DPP-1 | **PropertyBlock 의 동적 키 패턴 폐기 후속** | (a) 폐기 -- 모든 동적 uniform 을 .slang UBO 멤버로 *선언 강제*, game 코드는 setter 의 *컴파일타임 멤버 접근* 으로 전환 (`mat->LightBlock().uFogColor = ...`). (b) 유지 -- UBO 멤버를 *Slang reflection JSON* 의 컴파일타임 offset 테이블로 매핑, `UpdateUniformBlock(name, key->offset, ...)` 런타임 lookup. **(2026-06-21 실측 검증)** `simple.refl.json` 이 `uFrame.uView @offset 0 size 64` + `uFrame.uProj @offset 64 size 64` 형식으로 std140 offset 을 *정확히* 제공함 확인. T4 의 `UpdateUniformBlock("FrameBlock", &viewMat, sizeof(mat4), 0)` 호출과 1:1 정합 → 옵션 (b) 의 *런타임 lookup 인프라가 손에 있음* (refl.json 파싱 + offset 테이블 빌드만 추가하면 됨). (c) 하이브리드 -- 컴파일타임 알려진 키만 UBO, 런타임 토글(예: postfx 디버그) 만 SSBO 또는 push constant 시뮬. |
| D-DPP-2 | **LightBlock UBO scope 의 *최종*형** | Phase 2.5 의 S5 결정을 Phase 3 시점에서 *전 셰이더 확장* 검증. per-frame 공유 LightBlock 이 phong 외 셰이더(decal/skybox 라이팅 없음, postfx 라이팅 없음) 에서 *무비용 bind* 인지 확인. |
| D-DPP-3 | **셰이더 재고 매트릭스** | 모든 `apps/_MyApp_/resources/shaders/` + `src/` 의 .vert/.frag 를 3-축 분류: (1) .slang 재작성 가능 (sb7 origin 인지) (2) 행렬 사용 -- R1 검증 필요 여부 (3) 동적 Properties 키 의존도. 셰이더 N개의 *전체 인벤토리* 가 spec 부록. |
| D-DPP-4 | **PropertyBlockSetter / uniform_cache 모듈 제거 시점** | 모든 셰이더가 UBO 화 *완료 직후* 별 PR. *부분 제거* 금지 (회귀 위험). |

### 4.3 spec 산출 형식
`doc/superpowers/specs/2026-XX-XX-dynamic-properties-deprecation-design.md` 로 작성. gitignore 로컬 정책 따름. D-DPP-1~4 각각 *옵션 표 + 추천* 형식 (project 의 architecture-design-workflow 컨벤션).

---

## 5. Verified integration seams (file:line, 분석 세션 2026-06-20 + 빌드 실측 2026-06-21)

- **사이클 가드:** [`src/buffer/CMakeLists.txt:17`](../../src/buffer/CMakeLists.txt#L17) `SJH::buffer` PUBLIC `SJH::program` -- UniformBuffer 가 `src/program/` 거주 강제 (Phase 2 핸드오프 §2 정정 라인). 🟢 빌드 GREEN 로 정합 검증.
- **UBO active uniform 동작:** [`src/program/uniform_cache.cpp:39-51`](../../src/program/uniform_cache.cpp#L39-L51) `glGetActiveUniform` 루프 + [`:47`](../../src/program/uniform_cache.cpp#L47) `glGetUniformLocation` 이 UBO 멤버 → -1. mEntries 등록은 *되되* location=-1. 🟢 마젠타 PoC R1 ✅ 시점 비-UBO 셰이더 외관 동일로 자연 skip 메커니즘 검증.
- **Slang→GLSL ABI 실측 (2026-06-21, simple.{vs,fs}):**
  - `#version 410 core` + `layout(row_major) uniform;` 전역 강제 -- D13 row_major 가정 확정.
  - `layout(std140) uniform block_<X>_0 { ... }` 강제 -- 우회 불가 (loose uniform 보존 0).
  - `layout(binding=N)` 제거 (post-process 410 호환).
  - **수학적 자기일관성:** Slang 출력의 mul 변환이 row-vector × row_major 순으로 변환됨 (`pos * uModel * uView * uProj`). column-vector × column_major 와 *수학적 동등* → `vmath::mat4` (column-major) raw 바이트가 그대로 정합 (D13 비전치 ✅).
- **Phase 2.5 phong 변환 대상 셰이더:** `apps/_MyApp_/resources/shaders/phong.{vert,frag}` (현재 본 브랜치 존재 여부 직접 확인 필요 -- 분석 시점에 미검증).
- **LightUniformDispatcher 변경 대상:** [`src/render/light_uniform_dispatcher.cpp`](../../src/render/light_uniform_dispatcher.cpp) `Dispatch(programs, dir, points, spots, viewPos)` 시그니처. S5 결정에 따라 UniformBuffer 소유 추가 (per-frame 공유 시) 또는 시그니처 무변경 (per-program 시).
- **Dynamic Properties 의존 사이트 (분석 세션 식별):**
  - [`apps/_MyApp_/main.cpp:174`](../../apps/_MyApp_/main.cpp#L174) `uFogColor` -- fog material
  - [`apps/_MyApp_/main.cpp:351`](../../apps/_MyApp_/main.cpp#L351) `u_time` -- skybox material
  - [`apps/_MyApp_/main.cpp:357`](../../apps/_MyApp_/main.cpp#L357) `uInverseProj` -- fog inverse projection
  - [`apps/_MyApp_/src/Playable/HpGrayscalePostFX.cpp:46`](../../apps/_MyApp_/src/Playable/HpGrayscalePostFX.cpp#L46) -- HP→grayscale ratio
  - [`apps/_MyApp_/src/Playable/PostFXTweenPlayable.cpp:42`](../../apps/_MyApp_/src/Playable/PostFXTweenPlayable.cpp#L42) -- tween-driven uniform
  - 추가 사이트는 `grep -rn 'Properties.Floats\[' apps/ src/` 로 전수조사 (Phase 3 spec 작성 시).
- **Slang reflection JSON offset 메타 (D-DPP-1 옵션 b 정통성 근거):** `build_ninja/apps/_MyApp_/slang_generated/shaders/simple.refl.json` 이 `parameters[].type.elementType.fields[].binding.{offset, size}` 형식으로 std140 byte offset 제공. 예: `uView { offset: 0, size: 64 }`, `uProj { offset: 64, size: 64 }`. 옵션 (b) 런타임 lookup 의 *데이터 소스가 이미 존재*.

---

## 6. Pending user decisions (Phase 3 진입 전 사용자 결정 필요)

본 핸드오프는 *분석 + 옵션 제시까지만* 한다. 다음 결정은 사용자가 명시적으로 pick 해야 진행 가능 -- 에이전트가 단정 금지:

1. **PICK-1: (A) 점진 vs (B) 빅뱅** -- 🟢 **사용자 답변 = (A) 점진 (2026-06-21).**
2. **PICK-2 (옵션 A 시):** Phase 2.5 의 S5 = LightBlock UBO scope = 🟢 **사용자 답변 = (b) per-frame 공유 (2026-06-21).** owner = dispatcher.
3. **PICK-3 (옵션 B 시):** D-DPP-1 = Dynamic Properties 폐기 옵션 -- (A) pick 되어 *해당 없음* (Phase 3 시점 재개).
4. **PICK-4:** 셰이더 재고 매트릭스 작성 -- *미결* (Phase 2.5 끝나고로 잠정).

### D-LUD: LightUniformDispatcher 재검토 결정 (2026-06-21, 7-에이전트 워크플로우 `wf_25f028c9-02e`)
- **재검토 동기:** 사용자가 dispatcher 의 존재 의의 의심 (가독성 위해 분리한 기억). deletion-test 3렌즈 + Unity/Unreal/Godot Context7 벤치마크 + 대안 옵션표 수행.
- **판정:** "지금 삭제할 군더더기가 아니라 곧 깊어질 빈 그릇." 현재 = shallow-but-justified (호출처 N=1, 멤버 0 stateless / 단 D6 program->object 역의존 격리가 진짜 정당화 축). 인라인 회귀(O2) = 3렌즈 만장일치 반대.
- **3엔진 must-have (Context7):** "광원 데이터를 공유 버퍼에 1회 업로드" (Unity ForwardLights / Unreal FLightShaderParameters / Godot RenderingDevice uniform_set). -> 현재 SJH 의 loose-uniform N-program 순회는 정통과 *반대*, Phase 2.5 LightBlock UBO 가 정통.
- **🟢 결정 (사용자 2026-06-21):**
  - **D-LUD-1:** 헤더 주석 정정 = *지금 완료* ([`src/render/light_uniform_dispatcher.h`](../../src/render/light_uniform_dispatcher.h) 의 "4 엔진 정통" 오류 3건 교정 + 현재/미래 구분 + D6 동기 명시). 코드 불변.
  - **D-LUD-2:** Phase 2.5 P25-T2 에서 **O4 승격** -- dispatcher 를 `UniformBuffer` owner 로 stateful 전환 + **rename `LightUniformDispatcher` -> `LightUboUploader`** + `Dispatch` 를 `Update`(std140 패킹) / `BindTo`(programs 결속) **2메서드 분할**. -> §8 의 "Dispatch 시그니처 유지" 가드레일은 *본 결정으로 해제* (사용자 승인). loose 경로는 비-UBO 셰이더용 공존 보존 (S4).

각 PICK 마다 *현재 인용된 file:line 외 추가 사실 확인이 필요한지* 사용자에게 명시적으로 질의.

---

## 7. Guardrails & conventions (반드시 준수)

- **커밋:** path-scoped partial 만. `git add -A`/bare `git commit` *금지* (사용자 staged 작업 휩쓺). `Co-Authored-By` 트레일러 *미사용*. 메시지 한국어, prefix `[engine]`/`[shader]`/`[build]`/`[docs]`.
- **빌드/검증:** `export PATH="$HOME/slang/bin:$PATH"` 후 `cmake --preset ninja` + `cmake --build --preset ninja --target _MyApp_`. **테스트 자동작성 금지 (`no_auto_tests`)** -- 검증 = 빌드 GREEN + grep + 육안.
  - **ninja 타겟 명명 (2026-06-21 실측 정정):** `SJH::program` 등 CMake ALIAS 는 ninja 입장에선 unknown. 실 타겟명 = `sjhopengl_<module>` (예: `--target sjhopengl_program`). 셰이더만 = `--target _MyApp__shaders`. Phase 2 핸드오프 §6 의 `--target program` 표기는 ninja 에서 안 통함.
- **코드 컨벤션:** 주석 한국어 + Doxygen + ASCII/한글만 (특수문자 0). 헤더가드 `__SJH_X_H__` (`#pragma once` 미사용). 명명 멤버 `mPascalCase` / 지역 `camelCase` / 타입·함수 `PascalCase` / bool `mIs*` / 포인터 `*Ptr`, trailing underscore 미사용.
- **크로스플랫폼:** `long` 금지 (`int32_t`/`uint64_t`), 경로 슬래시, `windows.h` 는 `#ifdef _WIN32` 안에서만.
- **스코프 -- 건드리지 말 것:**
  - sb7code (`extern/sb7code/`) -- memory 의 `sb7code_immutable` 컨벤션.
  - cmake/Slang*.cmake (Phase 1 완성품).
  - Phase 2 가 만든 코드 (`SJH::UniformBuffer`, `Program::UniformBlock`, `useUbo` 분기) -- *덮어쓰지 말고 확장*.
  - 비-phong 셰이더 (Phase 2.5 한정).
- **R1 은 사용자만 확정 가능:** Phase 2.5-T5 의 육안 게이트. 에이전트가 임의로 "전치 맞다/아니다" 단정 금지.
- **PR 분리:** Phase 2.5 PR + Phase 3 PR + 모듈 제거 PR 은 *각각 별*. 합치지 말 것.

---

## 8. 충돌 매트릭스 (Phase 2.5 가정 -- 옵션 A pick 시)

| 파일 | 소유 | 비고 |
|---|---|---|
| `apps/_MyApp_/shaders_slang/phong.slang` | 신규 (Phase 2.5) | 충돌 없음 |
| `src/render/light_uniform_dispatcher.{h,cpp}` -> rename `light_ubo_uploader.{h,cpp}` | Phase 2.5 | **D-LUD-2:** UBO owner 승격 + rename `LightUboUploader` + `Dispatch`->`Update`/`BindTo` 2메서드 분할 (시그니처 동결 해제). loose 경로 공존 보존 (S4). 호출처 [`render_stage.impls.cpp:140`](../../src/render/render_stage/render_stage.impls.cpp#L140) 동반 수정. |
| `src/render/mesh_pass_processor.cpp` | Phase 2.5 | `useUbo` 분기에 LightBlock 갱신 *추가*, 기존 view/proj/model 분기 *유지*. |
| `apps/_MyApp_/CMakeLists.txt` | Phase 2.5 | `sjh_compile_slang(phong.slang ...)` 추가 라인. |
| `src/program/program.h` 등 Phase 2 산출물 | **수정 금지** | 확장만 -- API 변경 시 spec 별도. |
| `apps/_MyApp_/resources/shaders/phong.{vert,frag}` | overlay 덮어쓰기 | Slang 산출이 POST_BUILD 단계에서 덮음. 원본 GLSL 은 git 잔존 (롤백용). |
| `src/material/material_property_block.h` | **수정 금지** | Phase 3 의 D-DPP-1 결정 영역. |
| `PostFXRegistry` 흡수된 `mat_pass_<name>` 경로 | **수정 금지** | 직전 머지 PR 의 D-7 결정 lock. |

---

## 9. Self-review 체크리스트 (보고 전)

- [ ] Phase 2 R1 ✅ 가 *문서로 확인*되었나? (Phase 2 핸드오프의 §8 Report 산출 인용)
- [ ] 사용자 PICK-1 (A or B) 가 *명시적으로 받은 답*인가? (에이전트가 임의 단정 금지)
- [ ] (옵션 A 시) Phase 2.5 PR 의 *phong 셰이더 한정 범위* 가 지켜졌나?
- [ ] LightUniformDispatcher 의 *loose 송신 경로 공존 보존* 가 확인되었나?
- [ ] 비-phong 셰이더 (skybox/postfx/decal/sprite/bitmap_font/world_text) 가 *건드려지지 않았나*?
- [ ] Dynamic Properties 의존 사이트 (HpGrayscalePostFX / PostFXTweenPlayable / fog / skybox) 가 *건드려지지 않았나*?
- [ ] PropertyBlockSetter / uniform_cache *모듈 자체* 가 살아있나? (Phase 3 까지 보존)
- [ ] R1 N→N+1 확장 (Phase 2.5-T5) 가 *사용자 육안 보고*로 ✅/❌ 받았나?

---

## 10. Report 형식

DONE / DONE_WITH_CONCERNS / BLOCKED + 변경 파일 목록 + 각 커밋 SHA + 사용자 PICK 답변 인용 + R1 N→N+1 결과 (✅/❌, T6 수행 여부) + Phase 3 spec 작성 여부 + deferred 항목.

---

## 11. Pointers

- **Phase 2 구현 핸드오프 (선행):** [`doc/handoffs/2026-06-20/2026-06-20-slang-phase2-engine-ubo-handoff.md`](./2026-06-20-slang-phase2-engine-ubo-handoff.md).
- **Phase 2 실작업 결과 (2026-06-21 변경 파일 7개, brunch `game/slang-phase2-ubo`):**
  - T1: `apps/_MyApp_/shaders_slang/simple.slang` (M)
  - T2: `src/program/uniform_buffer.{h,cpp}` (A 신규) + `src/program/CMakeLists.txt` (M)
  - T3: `src/program/program.{h,cpp}` (M)
  - T4: `src/render/mesh_pass_processor.cpp` (M)
  - T5 PoC: `apps/_MyApp_/src/Entity/Bullet/bullet_factory.h:132-133` (주석 해제)
- **Phase 1 툴체인 핸드오프 (원작성자):** `~/Downloads/SLANG_TOOLCHAIN_HANDOFF.md`.
- **분석 세션 산출 (이 문서의 근거):** 2026-06-20 워크플로우 `wf_e3886478-896` -- 9 에이전트 / discovery 4 + analysis 1 + Context7 benchmark 3 + synthesis 1. 보고서 본문은 본 핸드오프 §0~§4 에 흡수.
- **성능 회귀 진단 방법론 (부수 산출, 2026-06-21):** [`doc/handoffs/2026-06-21/2026-06-21-perf-regression-system-triage-handoff.md`](./2026-06-21-perf-regression-system-triage-handoff.md). Phase 2.5 작업 중 사용자가 "느려졌다" 보고 시 5단계 진단 파이프라인. *자기 도구 self-induced leak 의심 1순위* 가 핵심 학습.
- **정통성 인용 (Context7):**
  - Unity URP/HDRP: `UnityPerFrame`/`UnityPerDraw`/`UnityPerMaterial` 3-tier 이름 규약 -- SRPBatcher-Materials.md, HDRP 10.x C# ConstantBuffer API.
  - Unreal 5.7: `BEGIN_SHADER_PARAMETER_STRUCT`/`BEGIN_UNIFORM_BUFFER_STRUCT` + RDG -- dev.epicgames.com/rdg, FViewShaderParameters API.
  - Godot 4: `RenderingDevice.uniform_set_create` + `uniform_buffer_create` -- godot-docs class_renderingdevice.md.
- **현재 작동 중 의존 패턴 (건드리지 말 것):** memory 의 `propertyblock_postfx_pattern` / D-7 PostFXRegistry 흡수 결정 / `shader_max_lights_16` 컨벤션.
- **사이클 가드 근거:** memory `dependency_cycles_survey` + `init-scheduler-effort` -- buffer→program PUBLIC 라인.

---

## Change log

- 2026-06-20: 최초 작성. Phase 2 (별 핸드오프) 진입 전 *분석 세션* 산출을 기반으로 *후속 전략 결정* 을 정리. 사용자 최종 PICK-1 미완 상태로 핸드오프 -- 다음 세션의 첫 응답은 *사용자에게 PICK 질의*.
- 2026-06-21: 갱신. Phase 2 T1~T5 R1 ✅ 완료 사실 반영 + 빌드 실측 보강:
  - §0 활성 시점 ✅ 확정 표시 (Phase 2 R1 = 마젠타 정상 → 비전치 lock 정통성 검증).
  - §1 Phase 2 진행 상태 = 미착수 → T1~T5 완료. 부수 산출 (수학적 자기일관성, refl.json offset 메타, 성능 회귀 핸드오프) 추가.
  - §2 P4 cache miss 메커니즘 실측 확인 표시.
  - §3.4 가드레일 추가: fail-fast 정책 (spdlog 불가 → `fprintf+abort`) + clangd false positive 사전 인지.
  - §4 D-DPP-1 옵션 (b) Slang reflection 런타임 lookup 의 정통성 근거 보강 (refl.json offset 메타 *실측 확인*).
  - §5 Verified seams 에 Slang→GLSL ABI 실측 + mul 변환 자기일관성 + refl.json offset 데이터 추가.
  - §7 가드레일 ninja 타겟 정정 (`sjhopengl_<module>` 정확명, ALIAS 함정 명시).
  - §11 Pointers 에 Phase 2 변경 파일 목록 + 성능 회귀 핸드오프 cross-link 추가.
- 2026-06-21(2차): PICK-1=(A)/PICK-2=(b) 사용자 확정. LightUniformDispatcher 재검토(워크플로우 `wf_25f028c9-02e`, 7 에이전트) 결과 §6 D-LUD 신설 -- 헤더 주석 정정(완료) + Phase 2.5 P25-T2 O4 승격(rename `LightUboUploader` + Dispatch 2메서드 분할, 시그니처 동결 해제). §8 충돌 매트릭스 dispatcher 라인 정합화.
- 2026-06-21(3차): **Phase 2.5 DONE.** 상태/§0 을 완료로 갱신. T1~T5(phong albedo UBO + S7 + StageBuilder 전환) + phong_lighting 모듈 단일화(`import`) 구현, 육안 ✅. **varying 이름 불일치 링크 버그 발견+해결** (slang_compile.py `_slangVaryN` 정규화, 정본 `doc/Slang버그리포트.md`) + overlay staleness fix + Slang 툴체인 단일화(slang_compile.py + depfile, 사용자 병렬 리팩토링). dead 셰이더(phong_tex/phong_albedo) 제거. 다음 = Phase 3 (D-DPP-3 셰이더 재고 + D-DPP-1 Dynamic Properties spec).
