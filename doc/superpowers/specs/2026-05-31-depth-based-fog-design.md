# Depth-based Fog (Phase 1) — Design Spec

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **대상**: `apps/_MyApp_` (탑다운 슈터). 브랜치 `game/module/ingame/temp`.
> **출발**: [`doc/handoffs/2026-05-31/2026-05-31-depth-based-fog-bloom-infra.md`](../../../doc/handoffs/2026-05-31/2026-05-31-depth-based-fog-bloom-infra.md) Phase 1.
> **작성**: 2026-05-31. brainstorming 5 결정 확정 후 작성.
> **상태**: 설계 승인됨 → writing-plans 대기.

---

## 0. 목표 / 비목표

### 목표
- `fog.fs` 의 *스크린 공간 근사*(`vUV.y * 10.0`)를 **진짜 depth-based fog**(카메라-픽셀 유클리디안 거리)로 교체.
- 거리는 sceneFB 의 *실제 depth* 를 `uInverseProjection` 으로 view-space 복원하여 산출.
- 카메라(WASD) 이동 시 fog 가 3D 로 일관되게 변화 — 지면이 멀어질수록 FogColor 누적.

### 비목표 (Phase 1 범위 외)
- Emissive bloom (handoff Phase 2 — MRT 인프라). **본 spec 에서 안 다룸.**
- PostFX 체인 전체의 resize 견고화 (중간 FB 재생성 + `PassComponent.InputFB` const dangling 해소). **기존 잠재 이슈로 §6 에 기록만.**
- 단위 테스트 신규 추가 (프로젝트 정책 — 사용자 요청 시에만). 수용 기준 = 시각 검증(§5).

---

## 1. 확정 결정 (brainstorming)

| # | 결정 | 근거 |
|---|---|---|
| **D1** | `uDepth` 바인딩 = **main.cpp 호출자 책임** (`RebindFogUniforms()` 헬퍼). 코어 render 모듈/`PassComponent`/`DrawCommand` 불변. | 인프라 0 변경. fog 의 depth 의존을 Composition Root(main.cpp)에 응집. handoff §1.3 "선택 2". §3.4 premature module split 회피. |
| **D2** | resize = **fog `uDepth` 재바인딩(방어적) + Texture/Framebuffer Resize 의 depth-texture 모드 확장**. | PostFX 체인 dangling 은 별도 resize SP(2026-06-01, commits ebf9918/5046b06/102429d)가 **in-place `Framebuffer::Resize` 로 해소**. depth-fog 는 그 in-place Resize 를 depth-texture 모드로 확장해야 함(§6). in-place 라 depth 텍스처 객체가 유지되어 `RebindFogUniforms()` 는 사실상 no-op이나 방어적으로 호출. |
| **D3** | **`depth >= 0.9999`(skybox/배경) 은 fog 제외** — fog 미적용 후 early return. | 매트릭스 skybox(far plane, `.xyww` depth≈1.0)를 또렷이 유지. Unity linear-fog skybox toggle 정통. |
| **D4** | `uInverseProjection` = **기존 `Material::Properties.Mat4s` 자동 송신**. 신규 "camera matrix bridge" 시스템 안 만듦. | [`property_block_setter.cpp:69-71`](../../../src/render/property_block_setter.cpp) 이 이미 `Mat4s` → `Uniforms::SetMat4` 자동 송신. [`camera.h`](../../../src/scene/camera.h) `GetProjectionMatrix()` 존재. handoff §1.4 의 "인프라 없음" 은 **사실과 다름** — 인프라 이미 있음. |
| **D5** | sceneFB depth = **depth-stencil *텍스처*** (`GL_DEPTH24_STENCIL8` / `GL_DEPTH_STENCIL` / `GL_UNSIGNED_INT_24_8`, `GL_DEPTH_STENCIL_ATTACHMENT`). depth-only 아님. | stencil(Outline `StencilMaskWrite`/`OutlineVisible`/`OutlineXRay`, [`pass.h:41-48`](../../../src/material/pass.h)) 보존. RBO→텍스처 1줄 변경이 handoff §1.2 의 depth-only 신규 경로보다 작음. **Fallback**: GL 4.1 샘플링 문제 시 depth-only `GL_DEPTH_COMPONENT24`(stencil 포기). |

handoff 원안 대비 **갱신 사항** 2건:
1. §1.4 "WorldCamera InverseProjection 자동 주입 시스템 부재" → **존재함**(D4). main.cpp 가 매 프레임 `Mat4s` 에 set 하면 자동 송신.
2. §1.2 depth-only 텍스처 → **depth-stencil 텍스처로 대체**(D5) — stencil 보존.

---

## 2. 변경 분해 (Task)

코어 인프라 2 (T1·T2) + 셰이더 1 (T3) + 호출자 배선 1 (T4) + 검증 (T5).

### T1 — `SJH::Texture` depth 텍스처 생성 지원
파일: [`src/texture/texture.h`](../../../src/resource_registry/texture.h), [`texture.cpp`](../../../src/resource_registry/texture.cpp)

현재 한계: `SetTextureFormat` 이 `internalFormat`/`format` 을 같은 값으로 쓰고 `type` 을 `GL_UNSIGNED_BYTE` 하드코딩 → `GL_DEPTH24_STENCIL8` 같은 packed 포맷 생성 불가(`GL_INVALID_ENUM` 또는 잘못된 type).

변경:
- **추가** `static TextureUPtr Create(int w, int h, uint32_t internalFormat, uint32_t format, uint32_t type)` — 5-arg overload.
- **추가** `void SetTextureFormat(int w, int h, uint32_t internalFormat, uint32_t format, uint32_t type)` — 5-arg overload. `glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, nullptr)`.
- 기존 3-arg `Create`/`SetTextureFormat` **보존** — 내부적으로 `(format, format, GL_UNSIGNED_BYTE)` 로 5-arg 위임(중복 제거) 가능.
- depth 텍스처 권장 상태: `SetFilter(GL_NEAREST, GL_NEAREST)` + `SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE)`. 5-arg `Create` 안에서 depth/stencil internalFormat 일 때 NEAREST 기본 적용(또는 호출자 T2 가 명시 set).
- `mFormat` 멤버 의미: `internalFormat` 저장(기존과 동일 — `GetFormat()` 소비자 영향 없음).
- **⚠ resize SP 정합(2026-06-01)**: 기존 `Texture::Resize`([texture.cpp](../../../src/resource_registry/texture.cpp))가 `SetTextureFormat(w,h,mFormat)` 3-arg 경로라 depth-stencil 텍스처에 `format=GL_DEPTH24_STENCIL8`/`type=GL_UNSIGNED_BYTE` 를 넘겨 `GL_INVALID_ENUM`. → **Texture 가 `format`/`type` 도 멤버로 보관**(`mDataFormat`/`mDataType`)하고 5-arg `SetTextureFormat` 이 셋을 저장, `Texture::Resize` 가 저장된 triple 로 재할당하도록 확장. 3-arg 경로는 `(format, GL_UNSIGNED_BYTE)` 저장(기존 동작 동일).

수용: `Create(w, h, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8)` 가 GL 에러 없이 유효 텍스처 핸들 반환 + 그 텍스처 `Resize(w2,h2)` 가 GL 에러 없이 재할당.

### T2 — `SJH::Framebuffer` depth-텍스처 모드
파일: [`<src>/buffer/framebuffer.h`](../../../src/buffer/framebuffer.h), [`framebuffer.cpp`](../../../src/buffer/framebuffer.cpp)

현재 한계: depth 가 항상 RBO(`mRBODepthStencilBuffer`) → 셰이더 sample 불가.

변경:
- **추가** `static FramebufferUPtr CreateWithDepthTexture(int w, int h)` factory.
- **추가** `TexturePtr mDepthAttachment` 멤버 + `const TexturePtr GetDepthAttachment() const` API.
- **추가** private `bool InitWithSizeAndDepthTexture(int w, int h)`:
  1. 색 RGBA8 텍스처 = `Texture::Create(w, h, GL_RGBA)` (기존 3-arg).
  2. depth-stencil 텍스처 = `Texture::Create(w, h, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8)` (T1 5-arg) + NEAREST/CLAMP.
  3. `glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color->GetTextureID(), 0)`.
  4. `glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth->GetTextureID(), 0)`.
  5. `glCheckFramebufferStatus` == `GL_FRAMEBUFFER_COMPLETE` 검증.
- 기존 `Create(TexturePtr)` / `Create(int,int)`(RBO depth) / `~Framebuffer` **보존**. RBO 모드와 텍스처 모드 공존(`mRBODepthStencilBuffer`==0 이면 텍스처 모드).
- 소멸: `mDepthAttachment` 는 `shared_ptr` RAII 자동 해제 — `~Framebuffer` 에 추가 GL 호출 불필요.
- **⚠ resize SP 정합(2026-06-01)**: 기존 `Framebuffer::Resize`([framebuffer.cpp:48-68](../../../src/buffer/framebuffer.cpp))가 color+RBO 만 재할당(doc 주석이 "depth-texture 모드 도입 시 분기 확장 필요" 명시). → **`Resize` 에 `if (mDepthAttachment) mDepthAttachment->Resize(w,h)` 분기 추가**. `mDepthAttachment` 존재(텍스처 모드) 시 RBO 분기는 자연 skip(`mRBODepthStencilBuffer`==0).

수용: `CreateWithDepthTexture(w,h)` 가 `GL_FRAMEBUFFER_COMPLETE` FBO + `GetDepthAttachment()` 유효 텍스처 반환 + `Resize(w2,h2)` 후에도 color/depth 동일 크기 유지 → `GL_FRAMEBUFFER_COMPLETE`.

### T3 — `fog.fs` depth-based 복원
파일: [`apps/_MyApp_/resources/shaders/postprocess/fog.fs`](../../../apps/_MyApp_/resources/shaders/postprocess/fog.fs)

uniform 추가/유지:
```glsl
#version 410 core
in  vec2 vUV;
out vec4 fragColor;

uniform sampler2D uScene;            // 직전 PostFX 출력 (색)
uniform sampler2D uDepth;            // sceneFB depth-stencil 텍스처 — .r = 정규화 depth [0,1]
uniform mat4      uInverseProjection;// inverse(WorldCamera projection)

uniform vec3  uFogColor   = vec3(0.5, 0.6, 0.7);
uniform float uFogDensity = 0.05;
uniform float uFogStart   = 0.0;     // Linear 모드 — view 거리 단위
uniform float uFogEnd     = 50.0;    // Linear 모드 — view 거리 단위 (스크린%가 아니라 거리)
uniform int   uFogMode    = 2;       // 0=Linear, 1=Exp, 2=Exp2
```

본문:
```glsl
void main() {
    vec3  sceneColor = texture(uScene, vUV).rgb;
    float rawDepth   = texture(uDepth, vUV).r;

    // D3 — skybox/배경(far plane) 은 fog 제외.
    if (rawDepth >= 0.9999) { fragColor = vec4(sceneColor, 1.0); return; }

    // NDC → view-space 복원. clip-space w 복원 위해 perspective divide.
    vec4 ndc     = vec4(vUV * 2.0 - 1.0, rawDepth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInverseProjection * ndc;
    viewPos     /= viewPos.w;
    float dist   = length(viewPos.xyz);   // 카메라-픽셀 유클리디안 거리

    float fogAmount;
    if      (uFogMode == 0) fogAmount = fogFactorLinear(dist, uFogStart, uFogEnd);
    else if (uFogMode == 1) fogAmount = fogFactorExp(dist, uFogDensity);
    else                    fogAmount = fogFactorExp2(dist, uFogDensity);

    fragColor = vec4(mix(sceneColor, uFogColor, fogAmount), 1.0);
}
```
- glsl-fog 3 함수(`fogFactorLinear`/`Exp`/`Exp2`)는 **현재 코드 그대로 유지** — 입력만 스크린 Y → view 거리.
- `uFogStart`/`uFogEnd` 의 의미가 *스크린 비율 0~1* → *view 거리 단위* 로 바뀜. main.cpp 초기값(아래 §2 T4)도 거리 단위로 갱신.

### T4 — main.cpp 호출자 배선
파일: [`apps/_MyApp_/main.cpp`](../../../apps/_MyApp_/main.cpp)

(a) **sceneFB 생성 교체** (2곳): `SJH::Framebuffer::Create(fbW, fbH)` → `SJH::Framebuffer::CreateWithDepthTexture(fbW, fbH)` — `startup()` line ~124 + `render()` resize 분기 line ~217.

(b) **`RebindFogUniforms()` private 헬퍼 신설**:
```cpp
void RebindFogUniforms() {
    auto* fogMat = FindFogMaterial();   // mPassComponents 중 name=="fog" 의 mMaterial
    if (!fogMat || !mSceneFB || !mSceneFB->GetDepthAttachment()) return;
    fogMat->Properties.Textures["uDepth"] = {mSceneFB->GetDepthAttachment().get(), 1}; // unit 1 (uScene=0)
}
```
- fog material 탐색: 기존 startup 의 `for ... if (Name=="fog")` 루프와 동일 방식. 헬퍼로 응집 권장.

(c) **호출 시점**: `startup()` 의 fog 초기값 set 블록 직후 1회 + `render()` resize 분기에서 `mSceneFB = CreateWithDepthTexture(...)` 직후.

(d) **매 프레임 `uInverseProjection` set** — `render()` 안:
```cpp
if (auto* fogMat = FindFogMaterial(); fogMat && mCamera)
    fogMat->Properties.Mat4s["uInverseProjection"] =
        Mat4Inverse(mCamera->GetProjectionMatrix());  // 익명 ns 일반 4x4 역행렬
```
- **inverse 계산**: sb7 `vmath` 는 일반 역행렬을 제공하지 않고([`camera.h:125`](../../../src/scene/camera.h) 가 명시), `Camera::InverseAffine` 은 affine 전용(scale=1 가정)이라 perspective projection(비-affine, w≠1)에는 **부적합**. 따라서 main.cpp 익명 네임스페이스에 **일반 4×4 역행렬 자유 함수 `Mat4Inverse(const vmath::mat4&)`** 를 신설(cofactor/adjugate, ~40줄). D1(Composition Root=main.cpp, 코어 모듈 미성장) 일관. 현 시점 유일 consumer 이므로 main.cpp 로컬; 후속 재사용 시 math util 로 승격.
- **최적화 메모**: projection 은 resize(aspect)에만 변하므로 캐시 가능하나, 4×4 역행렬 1회/프레임 비용은 무시 가능 → 캐시 무효화 복잡도 회피 위해 매 프레임 계산 유지(YAGNI).

(e) fog 초기값 거리 단위화: startup 의 `uFogStart`/`uFogEnd`/`POSTFX_PROGRAM_CONFIGS` 의 fog InitFloats 를 거리 단위로(`uFogStart=0`, `uFogEnd=50` 등) 조정. `uFogMode=2`(Exp2) 유지.

### T5 — 빌드 + 시각 검증
- `cmake --build --preset ninja --target _MyApp_` 무경고(Debug `-Werror`).
- `cd build_ninja/apps/_MyApp_ && ./_MyApp_` 실행 후 §5 시나리오 확인.

---

## 3. 데이터 흐름

```
WorldCamera ──render──> mSceneFB { color: RGBA8 tex, depth: DEPTH24_STENCIL8 tex }
                              │
   color ─> blur ─> gamma ─> invert ─> sharpen ─> sobel ─> fog.uScene
   depth ───────────────────────────────────────────────> fog.uDepth (unit 1)

mCamera.GetProjectionMatrix() ──inverse──> fog.uInverseProjection (Properties.Mat4s 자동 송신)

fog.fs:
   rawDepth = texture(uDepth, vUV).r
   rawDepth >= 0.9999 ? passthrough(skybox)          // D3
                      : viewPos = invProj * ndc; dist = |viewPos|
                        mix(sceneColor, uFogColor, fogFactor(dist))
   └─> bloom ─> backbuffer
```

포인터 소유:
- `mSceneFB` (`FramebufferUPtr`) 가 color+depth 텍스처 공유 소유.
- fog material 의 `Textures["uDepth"]` 는 **비소유 raw**(`GetDepthAttachment().get()`) — mSceneFB 수명 안에서만 유효. **resize 시 재바인딩 필수**(D2).

---

## 4. 아키텍처 영향

- **코어 모듈 변경 = T1·T2 (Texture/Framebuffer) 추가 API 만.** 기존 시그니처 보존 — 다른 consumer(다른 데모/테스트) 회귀 없음.
- **`SJH::render` / `PassComponent` / `DrawCommand` / `mesh_pass_processor` 불변** (D1).
- **Composition Root = main.cpp 유지** (handoff §3.3) — 신규 `SJH::bootstrap` 모듈 없음.
- **신규 STATIC 모듈 없음** (handoff §3.4, architecture.md §1 YAGNI).

---

## 5. 시각 검증 시나리오 (수용 기준)

| # | 시나리오 | 기대 |
|---|---|---|
| V1 | fog ON + density=0.05 + Exp2 | 지면의 *먼 픽셀*(depth 큰 값)이 FogColor 누적, 가까운 픽셀은 원본 |
| V2 | fog OFF (PostFXDebug 토글) | fog 도입 전 PostFX 결과와 동일 |
| V3 | WASD 카메라 이동 | fog 패턴이 카메라 움직임에 *3D 일관* 변화 (지면 원근에 따라) |
| V4 | `uFogColor` ImGui ColorEdit3 | 즉시 시각 반영 |
| V5 | 창 리사이즈(드래그) | sceneFB 재생성 후 fog 정상(uDepth dangling 없음). **fog 한정** — 체인 전체 견고화는 범위 외(§6) |
| V6 | bloom(현재 단순화) 영향 0 | fog 통합이 bloom 깨지 않음 |
| V7 | skybox(매트릭스) | 고밀도에서도 skybox 또렷 유지 (D3) |

---

## 6. 알려진 제약 / 후속

- **PostFX 체인 resize dangling — 별도 resize SP 로 해소됨(2026-06-01)**: 핸드오프([`doc/handoffs/2026-05-31/2026-05-31-postfx-chain-resize-robustness.md`](../../../doc/handoffs/2026-05-31/2026-05-31-postfx-chain-resize-robustness.md)) 기반 별도 SP(commits ebf9918/5046b06/102429d)가 **선택 A(in-place `Framebuffer::Resize`)** 로 use-after-free 해결: resize 분기가 `mSceneFB` 교체 대신 `mSceneFB->Resize()` + `mPostFXFBs` 동기 Resize. 포인터 안정 → `PassComponent.InputFB`(const) dangling 해소, PassComponent/PostFXDebug raw 참조 재배선 0. **depth-fog 와의 통합 책임(본 SP)**: 그 in-place Resize 는 *RBO 모드 전용* 이므로 (1) `Texture::Resize` 가 format/type triple 로 동작(§2 T1), (2) `Framebuffer::Resize` 에 `mDepthAttachment` 분기 추가(§2 T2). resize 분기엔 `RebindFogUniforms()` 방어 호출(D2 — in-place 라 no-op이나 의미 보존).
- **GL 4.1 depth-stencil 샘플링 리스크 (D5)**: `GL_DEPTH24_STENCIL8` 텍스처를 `sampler2D` 로 읽으면 .r = 정규화 depth(GL 3.0+ packed depth-stencil 표준). macOS GL 4.1 core 검증 항목 — T5 빌드/실행 시 확인. 문제 시 fallback = depth-only `GL_DEPTH_COMPONENT24`(stencil 포기, Outline 미사용 현 시점 무해).
- **Phase 2 (emissive bloom)**: MRT(`Framebuffer::CreateWithMRT` + WorldMesh 셰이더 `oAlbedo`/`oEmissive` + SceneRenderer `glDrawBuffers` + Material emissive). handoff §1.6 + §2 Phase 2. 별도 SP.

---

**spec 끝.**
