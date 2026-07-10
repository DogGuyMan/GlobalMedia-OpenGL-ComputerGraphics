# HANDOFF — Slang Phase 3: PropertyBlockSetter + uniform_cache 모듈 제거 (정통 끝점)

> **수신자:** 신규 Claude Code 세션/에이전트 (이 대화의 컨텍스트 없음 가정). 본 문서 하나로 안전 시작 가능하도록 자기완결.
> **작성:** 2026-06-21 · **갱신:** 2026-06-21(저녁) · **상태:** 🟢 **Gate Bᴳ 도달 — 전 셰이더 Slang/UBO화 완료 (fog 포함, 육안 ✅).** Phase C(잔여 제거) eligible.
> ⚠ **2026-06-21 저녁 중대 교정 — §0.5 필독.** 끝점("PropertyBlockSetter+uniform_cache 모듈 *삭제*")이 GL410 제약상 부분 성립 불가. 실측 후 "**sampler-only 슬림**"으로 재정의.
> **선행(전략 정본):** [`doc/handoffs/2026-06-20/2026-06-20-slang-post-phase2-strategy-handoff.md`](./2026-06-20-slang-post-phase2-strategy-handoff.md) — 본 핸드오프는 그 §4 (Phase 3 prerequisite) 를 *grounded 실측으로 정밀화* 한 실행 문서.
> ⚠ **gitignore 로컬:** `doc/` 는 .gitignore 됨 (다른 머신에 안 따라감). 로컬 전용.

---

## 0. TL;DR + 다음 액션

**끝점(정통):** `PropertyBlockSetter` (src/render) + `uniform_cache` (src/program) **모듈 자체 삭제.** 모든 셰이더가 UBO ABI 면 loose `glUniform*` 경로 = GL 2.x 잔재라 불요. Unity URP 5.x legacy property 폐기 / Unreal `BEGIN_UNIFORM_BUFFER_STRUCT` / Godot `uniform_set_create` 정통 (전략 핸드오프 §2 S1).

**경로(PICK-1=A 점진 확정):** Phase 2(simple) ✅ → Phase 2.5(phong) ✅ → **Phase 3 = 잔여 셰이더 UBO 화 + 모듈 제거.**

**🚧 진짜 blocker (B1) = 단 3개의 *런타임 KEY* 사이트.** 나머지 동적 키는 전부 *컴파일타임 KEY* 라 기계적 변환 (§5 가 정밀 구분 — 구 핸드오프엔 없던 핵심). 그래서:

**다음 액션 (순서 고정):**
1. **D-DPP-3 셰이더 재고 매트릭스** 작성 (§7 표가 초안 — 검증만) — *분석, 코드 0*.
2. **D-DPP-1 Dynamic Properties 폐기 결정 spec** 작성 (§10 옵션표) — *런타임 KEY 3사이트를 무엇으로 대체할지 결정*. **코드 진입 전 필수.**
3. (spec 합의 후) 잔여 셰이더 UBO 화 → 마지막에 D-DPP-4 모듈 제거.

⚠ **에이전트 단독 결정 금지:** D-DPP-1 옵션 (a/b/c) 은 사용자 pick 사항. spec 은 옵션표+추천까지, 결정은 사용자.

---

## 0.5 ★★ 중대 교정 (2026-06-21 저녁, grounded 실측) — 끝점 재정의

Phase B 완료 후 실측하니 §0/§2(S1) 의 "**PropertyBlockSetter + uniform_cache 모듈 자체 삭제**" 끝점이 **GL 4.1 제약상 그대로는 성립 불가**.

**근본 원인:** GL410 은 **sampler 를 UBO 에 못 넣는다** → 모든 셰이더가 UBO여도 `uTex`/`uScene`/`uDepth`/`chars`/`noise_tex` 등 **sampler 는 영구히 loose `glUniform1i` + `glBindTexture`** 경로다. 현 mesh_pass 가 *UBO 셰이더라도* material 전환 시 `PropertyBlockSetter::Set` 을 호출하는 이유 ([`mesh_pass_processor.cpp:166`](../../src/render/mesh_pass_processor.cpp#L166) ScreenQuad · [`:223`](../../src/render/mesh_pass_processor.cpp#L223) WorldMesh) = **sampler 바인딩**. 게다가 `passthrough.{vs,fs}` 는 아직 **raw GLSL blit (sampler-only, 비-UBO)** — `screen_passthrough` ([Constants.h:169-171](../../apps/_MyApp_/src/Playable/Constants.h#L169)) 가 사용.

→ **PropertyBlockSetter 는 삭제 대상이 아니라 *sampler 바인더*로 영구 잔존**, `uniform_cache` 도 동반 잔존 (PropertyBlockSetter 가 cache 순회로 sampler 를 찾음).

### 교정된 Phase C 분류 (실측 기반)
| 항목 | 상태 | 처리 |
|---|---|---|
| mesh_pass WorldMesh `else` (loose 행렬 분기, [:207-212](../../src/render/mesh_pass_processor.cpp#L207)) | **DEAD** (전 WorldMesh 셰이더 UBO) | 제거 가능 |
| LightUboUploader `LooseDispatch`([:142-186](../../src/render/light_ubo_uploader.cpp#L142)) + `BindTo` sentinel([:290](../../src/render/light_ubo_uploader.cpp#L290)) | **DEAD** (loose `UNI_VIEW_POS` 소비자 0 — phong=UBO 멤버, fog=지역변수) | 제거 가능 |
| PropertyBlockSetter 의 값 dispatch (`GL_FLOAT`/`VEC*`/`MAT4`/`INT`/`BOOL` case, [property_block_setter.cpp:51-87](../../src/render/property_block_setter.cpp#L51)) | **사실상 no-op** (UBO 멤버는 location=-1 → loose write 무시; 실 값은 `UploadMaterialUboMembers` 가 UBO로) | sampler-only 로 슬림 |
| `GL_SAMPLER_2D`/`CUBE` case ([:88-99](../../src/render/property_block_setter.cpp#L88)) | **LIVE** (영구) | 잔존 (PropertyBlockSetter 의 유일 책임이 됨) |
| PropertyBlockSetter 모듈 / uniform_cache 모듈 | **잔존** (sampler 바인딩 필수) | 삭제 ✗ → 슬림/개명 |

### ★ 사용자 결정 필요 (D-DPP-5 — 끝점 형태)
| 옵션 | 형태 | 장 | 단 |
|---|---|---|---|
| **(A) sampler-only 슬림** ⭐ | PropertyBlockSetter 값 case 제거 → sampler 바인딩만 (`SamplerBinder` 개명 후보). uniform_cache 잔존(sampler schema). 죽은 loose 경로(mesh_pass else + LightUboUploader loose) 제거. | 최소 변경, 정직(GL410 현실), 즉시 | "모듈 삭제" 끝점 미달성 (현실상 불가) |
| (B) sampler 전용 schema + 모듈 삭제 | passthrough 도 Slang화 + uniform_cache 를 sampler-only 미니 schema 로 대체 → PropertyBlockSetter 삭제 | 끝점 형식 달성 | 큰 신규 작업, 이득 적음(sampler 바인딩은 여전히 필요) |
| (C) 지금은 죽은 것만 | mesh_pass else + LightUboUploader loose 만 제거, PropertyBlockSetter/uniform_cache 손대지 않음 | 가장 안전 | 값 dispatch no-op 잔재 |

**💭 추천 = (A).** GL410 에서 sampler loose 는 불가피 → PropertyBlockSetter 의 본질은 *sampler 바인더*. 삭제가 아니라 책임을 sampler 로 정직하게 좁히는 게 정통(Unity 도 texture 바인딩은 별 경로). (B) 는 sampler 바인딩이 어차피 남으므로 모듈만 옮길 뿐 이득이 적다. (C) 는 과도기 잔재 유지.

---

## 1. State of the world (re-measured 2026-06-21 저녁)

- **브랜치:** `game/slang-phase2-ubo` · **HEAD:** `3228f71 [dev] : UBO 매핑함수 추가` (구 기록 `be9dcad` 에서 **+4 커밋 드리프트** — 사용자 병렬 작업).
- **recent log:** `3228f71 UBO 매핑함수` / `2a2e659 imgui vcpkg화 + vmath->glm` / `ca138f9 blend-func 수정(healthbar+shadow)` / `9076706 CMake 모던화·스크립트 분리` / `be9dcad`.
- **`3228f71` 내용:** 본 세션의 Slice 0 (`Program::UpdateUniformMember` + `mUniformMembers` introspection + mesh_pass `UploadMaterialUboMembers`) 을 사용자가 커밋 = **D-DPP-1(b) 런타임 reflection lookup 의 실체** (author 이름 → std140 offset).
- **uncommitted (git status, 2026-06-21 저녁):**
  - `?? apps/_MyApp_/shaders_slang/*.slang` (12종: billboard_atlas/bloom/blurring/fog/gamma/grayscale_vignetting/healthbar/invert/matrix_skybox/sharpening/sobel/transparent) — **본 세션 Phase B 산출**, 미커밋.
  - `D apps/_MyApp_/resources/shaders/...` (구 GLSL 17종 삭제 — Slang overlay 로 대체) + `M shaders_slang/CMakeLists.txt` / `M Constants.h` (배선) — 본 세션.
  - `M src/render/mesh_pass_processor.cpp` / `M src/render_bootstrap/render_pipeline.cpp` / `M scripts/slang_compile.py` — 본 세션 (postfx UBO 지원 + nullptr 정합 + sampler/array normalize).
  - `D extern/imgui` — 사용자 (imgui vcpkg 전이, 2a2e659).
- **Gate Bᴳ 완료 사실:** simple/phong(2.5) + simple_texture/transparent/healthbar/matrix_skybox/billboard_atlas + postfx 8종(gamma/grayscale_vignetting/invert/blurring/sharpening/sobel/bloom/fog) 전부 Slang/UBO. **유일 비-UBO = passthrough.{vs,fs}** (sampler-only blit, §0.5). 육안 ✅ (fog mode 0/1/2 + mat4/int/vec3 멤버 업로드 검증 완료).

---

## 2. Locked decisions (재논의 금지 — 전략 핸드오프에서 carry)

| # | 결정 | 근거 |
|---|---|---|
| S1 | **끝점 = PropertyBlockSetter + uniform_cache 모듈 제거** | 3엔진 정통 (UBO=기본 ABI). |
| PICK-1 | **경로 = A 점진** (빅뱅 아님) | B1 (Dynamic Properties) 결정 spec 부재 시 코드만 진행 불가. |
| D-DPP-4 | **모듈 제거는 *전 셰이더 UBO화 완료 직후 별 PR*** | 부분 제거 = 회귀 위험. |
| (Phase 2.5) | LightBlock = per-frame 공유 UBO, `LightUboUploader` 소유 | [[slang-toolchain-conventions]] |

---

## 3. 끝점까지의 strangler 모델 (무엇이 죽는가)

```
현재 (Phase 2.5 후):
  UBO 셰이더 (simple/phong)  --> mesh_pass useUbo 분기 --> UpdateUniformBlock (UBO)
  loose 셰이더 (나머지 N개)   --> mesh_pass else 분기   --> PropertyBlockSetter::Set --> uniform_cache 조회 --> glUniform*

Phase 3 끝 (목표):
  모든 셰이더 UBO          --> mesh_pass (분기 없음)   --> UpdateUniformBlock
  [삭제] PropertyBlockSetter / uniform_cache / mesh_pass else / LightUboUploader loose 경로 / material_uniforms Set* 동적키
```

**죽는 코드 (전부 UBO화 완료 시):**
- `src/render/property_block_setter.{h,cpp}` (모듈)
- `src/program/uniform_cache.{h,cpp}` (모듈) — `Program::mUniformCache` 멤버 + `GetUniformCache`/`GetLocation`/`GetType` 도 동반 정리
- `src/render/mesh_pass_processor.cpp` 의 `else`(loose) 분기 (:195-196) + ScreenQuad PropertyBlockSetter (:126)
- `src/render/light_ubo_uploader.cpp` 의 loose 경로 (`LooseDispatch` :140-186, `BindTo` 의 sentinel 분기 :290-291)
- `src/material/material_uniforms.{h,cpp}` 의 동적키 Set* (정책에 따라 — D-DPP-1)

---

## 4. PropertyBlockSetter / uniform_cache — 제거 대상 실측 (grounded)

**모듈 파일:**
- [`src/render/property_block_setter.{h,cpp}`](../../src/render/property_block_setter.cpp) — `namespace PropertyBlockSetter` + `Set(rc, MaterialPropertyBlock, Program)`. cache outer iteration (active uniform schema) + type dispatch (GL_FLOAT/VEC*/MAT4/SAMPLER + GL_BOOL fall-through) + block typed-map lookup.
- [`src/program/uniform_cache.{h,cpp}`](../../src/program/uniform_cache.cpp) — `glGetActiveUniform` 으로 schema 빌드 + name→(location,type). `Program` 이 `mUniformCache` 로 소유.

**실 호출처 (단 2곳 — 나머지는 doc 주석):**
- [`src/render/mesh_pass_processor.cpp:126`](../../src/render/mesh_pass_processor.cpp#L126) — ScreenQuad(PassComponent) 경로: `PropertyBlockSetter::Set(rc, effectiveMat->Properties, *prog)`.
- [`src/render/mesh_pass_processor.cpp:196`](../../src/render/mesh_pass_processor.cpp#L196) — loose WorldMesh 경로 (`else` 분기, 비-UBO 셰이더): `PropertyBlockSetter::Set(rc, material->Properties, *program)`.

**uniform_cache 소비자 (제거 시 정리 대상):**
- `property_block_setter.cpp` (cache.Entries() 순회) — 같이 죽음.
- `program_uniforms.cpp` `GetLocation` — loose `Uniforms::SetMat4(prog, name, val)` 의 location lookup (mesh_pass else 분기에서 사용). loose 죽으면 같이.
- `src/diagnostics/uniform_diagnostics.h` — warn-once / 타입 불일치 진단 (cache 기반). 정리 필요.
- `Program::GetUniformCache/GetLocation/GetType` ([`program.h:96-104`](../../src/program/program.h#L96)) — UBO 셰이더는 location=-1 이라 무용. 제거.

---

## 5. ★ B1 blocker 정밀화 — 컴파일타임 KEY vs 런타임 KEY (구 핸드오프 비약 교정)

구 핸드오프 §4.1 은 "동적 키 추가 사이트가 전부 컴파일 안 됨" 으로 *과대* 서술. **실측하면 동적 키 사이트의 절대다수는 *컴파일타임에 알려진 문자열 키* 라 기계적으로 UBO 멤버화 가능**하다. *진짜* blocker 는 **런타임에 키가 결정되는 단 3 사이트** 뿐:

### 5-A. 런타임 KEY (★ D-DPP-1 의 진짜 대상 — 3 사이트)
| 사이트 | 코드 | 성격 |
|---|---|---|
| [`HpGrayscalePostFX.cpp:46-47`](../../apps/_MyApp_/src/Playable/HpGrayscalePostFX.cpp#L46) | `FindSharedMaterial("mat_pass_"+mPassName)` 후 `Properties.Floats[mUniformName] = ratio` | `mUniformName`/`mPassName` 둘 다 *인스턴스 런타임 string* (D-7 PostFXRegistry 흡수와 회로) |
| [`PostFXTweenPlayable.cpp:43`](../../apps/_MyApp_/src/Playable/PostFXTweenPlayable.cpp#L43) | `mat->Properties.Floats[mUniformName] = value` | 트윈 대상 uniform 이름이 런타임 구성 |
| [`render_pipeline.cpp:113-115`](../../src/render_bootstrap/render_pipeline.cpp#L113) | `for (name,value : def.InitFloats) Properties.Floats[name] = value` | PostFX 체인 정의(데이터주도)의 키 — D-6 |

→ 이 3곳은 "어떤 uniform 을 구동할지" 를 *런타임 문자열* 로 정한다. UBO 멤버는 컴파일타임 layout 이라 직격. **D-DPP-1 이 답해야 할 핵심.**

### 5-B. 컴파일타임 KEY (기계적 변환 — blocker 아님)
키가 author-time 상수 문자열 → 해당 .slang UBO 멤버로 *그냥 옮기면 됨*. 전수:
- **fog:** `main.cpp:160` `uFogColor`(Vec3) · `:161` `uFogMode`(Int) · `:349` `uInverseProj`(Mat4) · postprocess/fog.fs 가 소비
- **grayscale_vignetting:** `main.cpp:166` `uVignetteColor`(Vec3)
- **skybox:** `main.cpp:344` `u_time` (`UNI_SKYBOX_TIME`) · `WorldSceneBuilder.cpp:144-146` `chars`/`noise_tex`(Tex)+`u_time`
- **MaterialTime (벽):** `MaterialTimeComponent.h:46` `uTime`(Float)
- **healthbar:** `HealthBarFactory.cpp:67-72` `uColor`/`uBgColor`(Vec4)+`uSegmentCount`/`uSegmentSpacing`/`uHeadOffset`/`uFill`(Float) · `HealthBarDriver.cpp:29` `uFill`
- **shadow/hit (적/플레이어):** `EnemyBuilder.cpp:122-137` / `PlayerBuilder.cpp:211-226` `uTex`(Tex)+`baseColor`(Vec4)
- **이미 UBO 처리됨 (참고):** `baseColor` — bullet(`bullet_factory.h:128`, simple.slang) / phong(`StageBuilder.cpp:181`) 는 mesh_pass useUbo MaterialBlock 경로. `material.shininess`/`albedo`(`model.cpp:100,106`) 도 phong UBO 흡수.
- **screen 텍스처:** `mesh_pass_processor.cpp:122` `uScene` (PassComponent 입력 FB)

**동적키 API 기반:** [`src/material/material_uniforms.cpp:23-62`](../../src/material/material_uniforms.cpp#L23) `SJH::Uniforms::Set{Float,Int,Vec2,Vec3,Vec4,Mat4,Texture}` = `mat.Properties.<Map>[name]=v` 한 줄. 위 모든 사이트의 backing. D-DPP-1 (a) 채택 시 이 API 의 운명도 결정.

---

## 6. 셰이더 인벤토리 (D-DPP-3 — grounded 전수, lit/unlit 분류)

`apps/_MyApp_/resources/shaders/` 실측 (2026-06-21). **viewPos sentinel 보유 = LightBlock 필요** 기준 분류:

| 셰이더 | 종류 | lit? | 동적키 의존 | Phase 3 처리 |
|---|---|---|---|---|
| simple.{vs,fs} | (이미 slang) | no | baseColor→UBO | ✅ 완료 |
| phong.{vs,fs} | (이미 slang) | **yes** | albedo→UBO | ✅ 완료 (`import phong_lighting`) |
| matrix_skybox.{vs,fs} | skybox | no | `u_time`/chars/noise_tex | .slang + per-material UBO + 텍스처 |
| transparent.{vs,fs} | 반투명 벽 | no | uvScale/uScrollSpeed/tintColor/emissive | .slang + UBO + 텍스처 |
| healthbar.{vs,fs} | UI | no | uColor/uFill/uSegment*… (6+) | .slang + UBO |
| billboard_atlas.{vs,fs} | sprite | no | (atlas uniform) | .slang + UBO |
| simple_texture.{vs,fs} | 텍스처 quad | no | baseColor/tex | .slang + UBO |
| passthrough.{vs,fs} | blit | no | uScene | .slang + UBO (screen) |
| postprocess/postprocess.vs | postfx VS 공유 | no | — | .slang (공유 VS) |
| postprocess/{bloom,blurring,fog,gamma,grayscale_vignetting,invert,sharpening,sobel}.fs | postfx | fog 만 viewPos | **fog=viewPos**, gv=uVignetteColor, 등 + 런타임키(HP/tween) | .slang + UBO + **D-DPP-1 (런타임키)** |

**핵심 관찰:** *loose LIT 셰이더는 이제 0개* (phong 이 slang 으로 이전, phong_tex/phong_albedo 삭제됨). `fog.fs` 만 viewPos 보유하나 *postfx depth 재구성용* (point/spot 조명 미사용). → **LightUboUploader 의 loose 경로는 사실상 fog 의 viewPos 1개만 위해 살아있음** (§9).

---

## 7. LightUboUploader loose 경로 — 거의 dormant (§5 보강)

[`src/render/light_ubo_uploader.cpp`](../../src/render/light_ubo_uploader.cpp):
- `LooseDispatch` (:140-186) — sentinel(`UNI_VIEW_POS`) 보유 program 에 dirLight+pointLights[16]+spotLights[16]+viewPos loose 송신.
- `BindTo` (:290-291) — LightBlock 없는 lighting program 에 LooseDispatch.
- **현 유일 consumer = fog.fs** (viewPos 만 실제 사용, 광원 배열은 set 되나 무시). phong 이전으로 lit loose 0.
→ Phase 3 에서 fog 를 UBO 화하면 (viewPos 를 FrameBlock UBO 멤버로) **loose 경로 통째 삭제 가능.** D-DPP-2 (LightBlock scope) 보다 단순.

---

## 8. 의존 그래프 영향 (모듈 제거 시)

- `SJH::render` → `property_block_setter.cpp` 제거 (CMakeLists `src/render/CMakeLists.txt:5`).
- `SJH::program` → `uniform_cache.{h,cpp}` 제거 (`src/program/CMakeLists.txt`). `program.h` 의 `#include "program/uniform_cache.h"` + `mUniformCache` 멤버 + 3 getter 제거 → **`program.h` API 변경** (consumer 영향: material.h, material_uniforms.h, diagnostics).
- `SJH::material` → `material_uniforms.{h,cpp}` 의 동적키 Set* 운명은 D-DPP-1.
- ⚠ **program.h API 변경은 광범위 consumer** → D-DPP-4 PR 은 *전 셰이더 UBO화 완료 후* 단독으로 (부분 금지).

---

## 9. 결정 (D-DPP — 옵션표 + 추천). 사용자 pick 필수.

### D-DPP-1 ★ Dynamic Properties (런타임 KEY 3사이트) 폐기 방식
| 옵션 | 형태 | 장 | 단 |
|---|---|---|---|
| **(a) UBO 멤버 강제** | 런타임키 폐기 → 모든 uniform 을 .slang UBO 멤버로 선언, game 코드는 컴파일타임 멤버 접근 (`mat->Block().uX=…`) | ABI 단일, 정통 | PostFX 시스템(HpGrayscale/Tween/체인) 의 *런타임 구성* 패턴 재설계 강제 — D-7 PostFXRegistry 회로 영향 |
| **(b) refl.json offset 런타임 lookup** ⭐ | UBO 멤버를 Slang reflection JSON 의 컴파일타임 offset 테이블로 매핑, `UpdateUniformBlock(name, key→offset, …)` 런타임 lookup | **런타임키 패턴 보존** (PostFX 시스템 무변경에 가까움) | offset 테이블 빌드 인프라 추가 (refl.json 파싱) |
| (c) 하이브리드 | 컴타임 키만 UBO, 런타임 토글만 별도(SSBO/push 시뮬) | 점진 | 두 경로 유지 복잡 |

**💭 추천 = 하이브리드 (c) 정밀화:** **정적 블록 = build-time codegen, 런타임 KEY postfx = (b) 런타임 reflection.**
- **정적 블록 (LightBlock/MaterialBlock 등): codegen.** 사용자 채택 — refl.json → C++ std140 미러 struct+static_assert 자동 생성 (수동 동기화 0 + zero-runtime + 컴파일타임 안전). 전용 핸드오프 = [`2026-06-21-slang-std140-mirror-codegen-handoff.md`](./2026-06-21-slang-std140-mirror-codegen-handoff.md).
- **런타임 KEY (§5-A 3사이트): (b) 런타임 reflection lookup.** 키가 컴파일타임에 없어 codegen 불가. `phong.refl.json` 이 이미 `{offset, size}` 메타 제공 → name→offset 테이블 lookup 으로 §5-A 가 거의 무변경. (a) 는 D-7 회로까지 흔들어 위험.

### D-DPP-2 LightBlock UBO scope 최종형
per-frame 공유 LightBlock 이 비-phong 셰이더(skybox/postfx — 조명 무관)에서 *무비용 bind* 인지 확인. **단 §7 관찰:** lit loose 0 + fog 만 viewPos → **scope 확장 자체가 거의 무의미**. fog viewPos 를 FrameBlock 으로 옮기면 LightBlock 은 phong 전용 유지.

### D-DPP-3 셰이더 재고 매트릭스 — §6 표가 초안. *검증/확정* 만 하면 됨.

### D-DPP-4 모듈 제거 시점 — 전 셰이더 UBO화 *완료 직후* 별 PR (§8 program.h API 변경 광범위).

---

## 10. 권고 Task 순서 (점진)

1. **D-DPP-3 확정** (§6 표 검증) — 코드 0.
2. **D-DPP-1 spec 작성** — `doc/superpowers/specs/2026-XX-XX-dynamic-properties-deprecation-design.md` (gitignore 로컬). 옵션표+추천((b)) → **사용자 pick.**
3. **셰이더 UBO화 (셰이더당 1 슬라이스, R1 N→N+1 정합 확장):** 우선순위 = (단순) passthrough/simple_texture → transparent/healthbar/billboard → skybox → **postfx (D-DPP-1 적용, 런타임키 3사이트 포함)**. 각각 phong 패턴 (`.slang` + 필요시 `import phong_lighting` + mesh_pass useUbo) + 육안 게이트.
4. **fog viewPos → FrameBlock** 이전 후 **LightUboUploader loose 경로 삭제** (§7).
5. **D-DPP-4: 모듈 제거 별 PR** — property_block_setter + uniform_cache + mesh_pass else 분기 + program.h getter + (정책 시) material_uniforms 동적키.

---

## 11. Guardrails & conventions (반드시 준수)

- **커밋:** 사용자 *직접* 커밋 (이번 세션 패턴). 자발 커밋 금지 — 요청 시 path-scoped (`git commit <경로>`, `git add -A` 금지 = 사용자 staged/병렬 작업 휩쓺). `Co-Authored-By` 미사용. 메시지 한국어, prefix `[engine]`/`[shader]`/`[build]`/`[docs]`.
- **빌드/검증:** `export PATH="$HOME/slang/bin:$PATH"` → `cmake --preset ninja` → `cmake --build --preset ninja --target _MyApp_`. **테스트 자동작성 금지** (`no_auto_tests`). 검증 = 빌드 GREEN + grep + **런타임 GL 로그** + 육안.
  - ⚠ **셰이더 링크는 빌드 GREEN/glslangValidator 로 안 잡힌다** — 앱 기동 GL 로그 필수 ([[slang-varying-name-mismatch-macos]]). "투명/사라짐" = program=null 1순위.
- **코드 컨벤션:** 주석 한국어 + Doxygen + ASCII/한글만 (특수문자 0). 헤더가드 `__SJH_X_H__`. 명명 멤버 `mPascalCase`/지역 `camelCase`/타입·함수 `PascalCase`/bool `mIs*`/포인터 `*Ptr`.
- **R1 은 사용자만 확정** — 셰이더 UBO화마다 육안 게이트. 에이전트 단정 금지.
- **PR 분리:** 셰이더 UBO화 PR(들) + 모듈 제거 PR 각각 별.

---

## 12. 충돌 매트릭스 (★ 사용자 병렬 toolchain 작업 — 건드리지 말 것)

| 파일/영역 | 소유 | 비고 |
|---|---|---|
| `cmake/slang_compile.py` | **사용자 (병렬)** | Slang 파이프라인 단일 스크립트 (slangc+post-process+varying정규화+reflection+depfile). **수정 금지** — varying fix 보존됨 ([[slang-toolchain-conventions]]). |
| `cmake/Slang.cmake` | **사용자 (병렬)** | 얇은 wrapper 로 축소됨. |
| `apps/_MyApp_/shaders_slang/CMakeLists.txt` | **사용자 (병렬)** | 셰이더 컴파일+overlay 자기완결. **새 .slang 추가는 여기에** `sjh_compile_slang` 라인 추가. |
| `apps/_MyApp_/CMakeLists.txt` | 사용자 (병렬) | `add_subdirectory(shaders_slang)` 로 위임. |
| `src/render/mesh_pass_processor.cpp` | Phase 3 | useUbo 분기 확장 + 끝에서 else 제거. |
| `src/render/light_ubo_uploader.*` | Phase 3 | loose 경로 §7 삭제. |
| `src/program/uniform_cache.*` / `property_block_setter.*` | Phase 3 (D-DPP-4) | 최종 삭제. |
| `src/material/material_uniforms.*` | Phase 3 (D-DPP-1 종속) | 동적키 운명. |
| `extern/sb7code`, `cmake/CXXStandard.cmake` 등 | **불가침** | sb7code 수정 금지 (memory `sb7code_immutable`). |

**새 .slang 추가 패턴 (Phase 3):** `apps/_MyApp_/shaders_slang/<name>.slang` 작성 → `shaders_slang/CMakeLists.txt` 에 `sjh_compile_slang(_X_VS … vsMain vertex glsl glsl_410 …/<name>.vs)` + FS + `sjh_reflect_slang` + `add_custom_target` DEPENDS 에 추가. lit 이면 `import phong_lighting`. overlay 는 자동 (매-빌드 COMMAND).

---

## 13. Verified integration seams (file:line, 2026-06-21 실측)

- **mesh_pass UBO/loose 분기:** [`mesh_pass_processor.cpp:155-214`](../../src/render/mesh_pass_processor.cpp#L155) — `useUbo = program->HasUniformBlocks()` (:155). MaterialBlock baseColor (:184-190), PropertyBlockSetter 우회/loose 분기 (:191-196), ScreenQuad PropertyBlockSetter (:126).
- **refl.json offset 메타 (D-DPP-1(b) 근거):** `build_ninja/apps/_MyApp_/shaders_slang/generated/*.refl.json` 의 `parameters[].type.elementType.fields[].binding.{offset,size}` (예: phong.refl.json `uView @0 size64`/`uProj @64`/LightBlock `pointLights @64 stride80`/`spotLights @1344 stride112`). std140 byte offset 정확.
- **Program std140 UBO API (Phase 2):** [`program.h:115-167`](../../src/program/program.h#L115) `UniformBlock` struct + `HasUniformBlocks`/`FindUniformBlock`/`BindUniformBlocks`/`UpdateUniformBlock`/`DisownUniformBlock`.
- **동적키 Set* API:** [`material_uniforms.cpp:20-64`](../../src/material/material_uniforms.cpp#L20).

---

## 14. Self-review 체크리스트 (Phase 3 작업 보고 전)
- [ ] D-DPP-1 옵션 (a/b/c) 이 *사용자 명시 pick* 인가? (에이전트 단정 금지)
- [ ] 셰이더 UBO화마다 *런타임 GL 로그 + 육안* 으로 링크/렌더 확인했나? (빌드 GREEN ≠ 링크)
- [ ] 새 .slang 은 `shaders_slang/CMakeLists.txt` 에 배선했나? (사용자 toolchain 파일 — 추가만, 구조 변경 금지)
- [ ] `cmake/slang_compile.py` / `Slang.cmake` 를 *안 건드렸나*? (사용자 병렬)
- [ ] 모듈 제거(D-DPP-4)는 *전 셰이더 UBO화 완료 후* 단독 PR 인가? (부분 제거 금지)
- [ ] loose 경로 삭제 전 fog viewPos 가 다른 경로(FrameBlock)로 옮겨졌나?

---

## 15. Pointers + memories
- **전략 정본:** [`doc/handoffs/2026-06-20/2026-06-20-slang-post-phase2-strategy-handoff.md`](./2026-06-20-slang-post-phase2-strategy-handoff.md) (§4 = 본 핸드오프의 grounded 원본).
- **버그리포트:** [`doc/Slang버그리포트.md`](../Slang버그리포트.md) (varying 링크 함정 — Phase 3 셰이더마다 재현 위험).
- **메모리:** [[slang-varying-name-mismatch-macos]] / [[slang-toolchain-conventions]] (Phase 2.5 상태 + 툴체인).
- **spec 컨벤션:** `doc/superpowers/specs/YYYY-MM-DD-<topic>-design.md` (gitignore 로컬, 옵션표+추천+Decision Log).
- **현재 작동 의존 (건드리지 말 것):** memory `propertyblock_postfx_pattern` (단 Phase 3 가 이걸 폐기) / `shader_max_lights_16` / D-7 PostFXRegistry.

---

## Change log
- 2026-06-21: 최초 작성. 전략 핸드오프 §4 (Phase 3 prerequisite) 를 grounded 실측으로 정밀화 — **B1 blocker 를 "런타임 KEY 3사이트 vs 컴파일타임 KEY 다수" 로 정정** (구 핸드오프의 "전부 깨짐" 비약 교정), 셰이더 인벤토리 전수 + lit 분류, loose 경로 dormant 관찰, 사용자 병렬 toolchain 충돌 매트릭스, 라인번호 재측정 (main.cpp:174→160 등 드리프트).
- 2026-06-21(저녁3): **C-1+C-2 전체 완료 + 빌드/육안 GREEN.** C-2 = passthrough.slang(전 셰이더 Slang 완결). **후속 회귀버그 수정**: lit 암전 = UBO binding point 전역 충돌(`bindingPoint=blockIndex`→의미명 고정 슬롯 Frame0/Draw1/Material2/Light3). 불릿+lit 동시 암전 없음 확인, DIAG 로그 제거. 정본=[[ubo-binding-point-semantic-slots]] + doc/버그리포트.md. 잔여=stale 주석 정리(선택).
- 2026-06-21(저녁2): **D-DPP-5=(B) 채택** (사용자 pick, 추천이던 (A) 대신 모듈 삭제 강행). **C-1 구현 완료 + 빌드 GREEN + 육안 OK** — PropertyBlockSetter/uniform_cache 삭제, sampler 는 mesh_pass `BindSamplers`(live GetLocation) 흡수, Program::GetLocation live화, loose 경로(mesh_pass else + LightUboUploader LooseDispatch) 제거. **C-2(passthrough Slang화) 빌드 게이트 대기.** 정본 plan = [`doc/superpowers/plans/2026-06-21-phase-c-propertyblocksetter-uniformcache-removal.md`](../superpowers/plans/2026-06-21-phase-c-propertyblocksetter-uniformcache-removal.md). 메모리 [[phase-c-propertyblocksetter-uniformcache-removed]].
- 2026-06-21(저녁): **Gate Bᴳ 도달** (전 셰이더 Slang/UBO화 완료, fog 포함 육안 ✅). **§0.5 중대 교정 추가** — GL410 sampler-loose 제약상 PropertyBlockSetter/uniform_cache 모듈 *삭제* 끝점 성립 불가 → "sampler-only 슬림"(D-DPP-5 (A) 추천)으로 재정의. 죽은 것 실측 확정 (mesh_pass else 행렬 분기 + LightUboUploader LooseDispatch, 둘 다 소비자 0). HEAD 드리프트 be9dcad→3228f71(+4커밋, 3228f71=Slice 0 D-DPP-1(b) 실체). passthrough=유일 비-UBO(sampler-only). git status 재측정.
