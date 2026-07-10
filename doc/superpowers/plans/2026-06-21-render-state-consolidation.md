# Render GL-State 통합 구현 Plan (Phase A~C, gate 강제)

> 정본 spec: [`doc/superpowers/specs/2026-06-21-render-state-ownership-consolidation-design.md`](../specs/2026-06-21-render-state-ownership-consolidation-design.md)
> ⚠ gitignore 로컬. 커밋은 사용자 직접 (path-scoped, `Co-Authored-By` 미사용, 메시지 한국어 prefix `[engine]`/`[build]`).

## Summary

GL pipeline-state(depth/blend/stencil/cull) 권위를 `DeviceContext` 단일로 통합(`ApplyPipelineState` + `InvalidateStateCache`), `PipelineStateSetter` 흡수·제거, 명령형 `SetDepthTest/SetBlend` 제거하고 모든 state 를 `Pass::PipelineState` 데이터로 통일(state-as-data). 이후 **게이트 걸린 하류 제거**(PropertyBlockSetter/uniform_cache/loose)까지 한 Plan 으로 추적.

**게이트 불변식 (절대):** Phase N 은 *Phase N-1 의 gate(빌드 GREEN + 육안) 통과 보장* 후에만 착수. Phase C 각 Task 의 acceptance 첫 줄 = precondition 확인.

```
Phase A (본 통합) ─Aᴳ→ Phase B (Slang Phase 3, 별 spec) ─Bᴳ→ Phase C (하류 제거, gate 강제)
```

---

## Phase A — Render GL-state 통합

### Task A1: `Pass::Kind::Screen` 추가 + blit state 정의 (결정 확정)
- **변경:** [`src/material/pass.h`](../../../src/material/pass.h)
  - `enum Kind` 에 `Screen` 추가 (queue 정수 = blit 전용, 예 5000 이상 — world queue 와 비충돌. Scene::Layer::Screen 과 대응).
  - `DefaultPipelineStateOf` 에 `case Screen`: depth test off, depth write off, cull off(=0 sentinel), blend off(=replace 기본).
  - "2+ 소스 alpha" 동적 부분은 호출처가 도출된 `PipelineState` 의 `BlendEnable` 1필드만 조정.
- **acceptance:** blit state 가 하드코딩 토글이 아닌 `Pass::DefaultPipelineStateOf(Kind::Screen)` 데이터로 표현. 빌드 GREEN. `Pass::Kind::Screen` grep 존재.
- **근거:** 모든 state 가 Kind→PipelineState 단일 경로(WorldMesh 와 동일 메커니즘). 사용자 결정 2026-06-21.

### Task A2: `DeviceContext` 권위 흡수 (data-driven)
- **변경:** [`device_context.{h,cpp}`](../../../src/render/device_context.h)
  - `+ void ApplyPipelineState(const Pass::PipelineState&)` — `pipeline_state_setter.cpp` 의 Stencil4/Depth3/Cull1/Blend2 dirty-apply 로직 이식.
  - `+ void InvalidateStateCache()` — `mStateInitialized=false`.
  - `+ (private) Pass::PipelineState mLast; bool mStateInitialized=false;`
  - `- SetDepthTest / SetBlend` **제거**.
  - `BeginFrame`: glClear 유지 + `InvalidateStateCache()` 후 **Opaque baseline**(`DefaultPipelineStateOf(Kind::Opaque)` = depth on/blend off) 을 `ApplyPipelineState`. ⚠ 현 BeginFrame 은 blend ON 기본 — Opaque(blend off)로 변경. 각 draw 가 자기 state 적용하므로 baseline 은 BeginFrame~첫 draw 사이만 영향(육안 확인 대상).
  - header `#include "material/pass.h"`.
- **acceptance:** `SetDepthTest`/`SetBlend` 심볼 0 (grep). 빌드는 A4/A5 배선 전이라 호출처 에러 가능 → A2~A5 한 묶음 커밋.
- **주의:** D-RS-5(ii). 명령형 토글 잔존 금지.

### Task A3: `PipelineStateSetter` 제거
- **변경:** [`pipeline_state_setter.{h,cpp}`](../../../src/render/pipeline_state_setter.cpp) 삭제. [`src/render/CMakeLists.txt:5`](../../../src/render/CMakeLists.txt#L5) 에서 줄 제거.
- **acceptance:** `PipelineStateSetter` 심볼 0 (grep, 주석 제외).

### Task A4: `mesh_pass_processor` 배선
- **변경:** [`mesh_pass_processor.cpp`](../../../src/render/mesh_pass_processor.cpp)
  - `PipelineStateSetter stateSetter;` 지역변수 + `#include` 제거.
  - Process 진입: `rc.InvalidateStateCache()`.
  - WorldMesh: `stateSetter.Set(passState)` → `rc.ApplyPipelineState(passState)`. 끝 `RestoreDefaults` 제거.
  - ScreenQuad(:119-120,134): `rc.SetDepthTest/SetBlend` → `rc.ApplyPipelineState(Pass::DefaultPipelineStateOf(effectiveMat->GetPass()))`.
- **acceptance:** 빌드 GREEN(A2~A5 묶음). Process 진입 invalidate 존재.

### Task A5: `render_stage` 배선
- **변경:** [`render_stage.impls.cpp`](../../../src/render/render_stage/render_stage.impls.cpp) 6사이트(105,224,251,257…) `rc.SetDepthTest/SetBlend` → `rc.ApplyPipelineState(Pass::PipelineState)`. world 시작=opaque 기본, ScreenQuadStage blit=blit state(첫 소스 replace / 2+ alpha).
- **acceptance:** `SetDepthTest`/`SetBlend` 호출 0 (전 코드 grep). 빌드 GREEN.

### Task A6: foreign 무효화 + CMake
- **변경:**
  - [`ParticleStage.cpp`](../../../apps/_MyApp_/src/VFX/ParticleStage.cpp) — Effekseer 렌더 후 `DeviceContext::Get().InvalidateStateCache()`. (Box2D debug draw 경로 동일.)
  - [`src/render/CMakeLists.txt`](../../../src/render/CMakeLists.txt) — `SJH::material` PRIVATE → PUBLIC.
- **acceptance:** 빌드 GREEN.

### ⛔ Gate Aᴳ (Phase A 완료 보장 — Phase B/C 착수 전제)
- [ ] 빌드 GREEN (`cmake --build --preset ninja --target _MyApp_`).
- [ ] grep: `SetDepthTest`/`SetBlend`/`PipelineStateSetter` 심볼 0.
- [ ] **육안 5체크** (런타임 GL 로그 + 눈): ①반투명 벽 알파 ②skybox depth(LEQUAL)/cull(FRONT) ③stencil(있으면) ④**Effekseer 파티클 후 다음 프레임 WorldMesh depth/blend 정상**(=무효화 규율) ⑤ScreenQuad PostFX blit.
- [ ] 사용자 확정 (R1 은 사용자만).

---

## Phase B — Slang Phase 3 (별 spec, 본 Plan 은 gate 만 참조)

> 본 Plan 은 Phase B 의 내부 Task 를 정의하지 않는다 — 정본 = D-DPP-1 spec [`2026-06-21-dynamic-properties-deprecation-design.md`](../specs/2026-06-21-dynamic-properties-deprecation-design.md). 여기서는 **Phase C 의 precondition gate** 로만 등록.

### ⛔ Gate Bᴳ (Phase C 착수 전제 — 보장 필수)
- [ ] D-DPP-1 사용자 pick + 구현 완료.
- [ ] **전 셰이더 UBO화 완료** — 비-UBO(loose glUniform*) 셰이더 0. (= PropertyBlockSetter 소비자 0의 필요조건.)
- [ ] **fog viewPos → FrameBlock 이전 완료** — LightUboUploader loose 경로 마지막 consumer 소멸.
- [ ] 각 셰이더 UBO화 육안 게이트 통과.

---

## Phase C — 하류 제거 (§7.1 명시 타겟, **게이트 스텁**)

> **상세화 정책 (사용자 결정 2026-06-21):** Phase C 는 Phase B(전체 Slang Phase 3) *뒤*라 file:line 이 드리프트하고 Slang D-DPP-4 와 중복된다. 따라서 **여기서는 범위 + precondition gate 만 박고, 세부 변경/라인은 Slang D-DPP-4 착수 시점에 확정**한다. (gate 는 이미 확정이라 적용 안전.)
> ⚠ **모든 Task 의 precondition = "Gate Bᴳ 통과 확인".** 미통과 시 착수 금지 (loose 소비자 잔존 = 회귀).

| Task | 제거 대상 (범위) | ⛔ precondition gate | 상세 정본 |
|---|---|---|---|
| **C1** | `PropertyBlockSetter` (+ mesh_pass loose else 분기 / ScreenQuad PBS callsite) | Bᴳ: 전 셰이더 UBO화 → loose glUniform* 소비자 0 (grep 검증) | Slang D-DPP-4 |
| **C2** | `uniform_cache` + `program.h` API(`mUniformCache`/`GetLocation`/`GetType`/`GetUniformCache`) + uniform_diagnostics 정리 | C1 완료. **program.h consumer 광범위 → 단독 PR** | Slang D-DPP-4 |
| **C3** | `light_ubo_uploader` loose 경로(`LooseDispatch`/`BindTo` sentinel). UBO owner 유지 | fog viewPos → FrameBlock 이전 완료 (⊂ Bᴳ) | Slang D-DPP-4 |

### Gate Cᴳ (전체 완료)
- [ ] PropertyBlockSetter / uniform_cache / loose 경로 전부 제거, 빌드 GREEN.
- [ ] 육안 전체 회귀 없음. 사용자 확정.

> Phase C 착수 직전(= Slang D-DPP-4 시점) 본 표를 file:line 까지 확장해 detail Task 로 승격할 것. 그 전엔 stub 유지.

---

## 커밋 분리
- Phase A: A1 / (A2~A5 묶음, 호출처 동시) / A6 — 또는 사용자 판단 path-scoped.
- Phase C: C1, **C2 단독 PR**(program.h 광범위), C3 각각.
