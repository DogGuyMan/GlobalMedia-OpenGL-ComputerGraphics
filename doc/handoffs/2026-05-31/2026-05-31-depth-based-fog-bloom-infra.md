# Handoff — Depth-based Fog + Emissive Bloom 인프라 SP

> **수신자**: 본 프로젝트의 다른 Claude Agent
> **목적**: PostFX 체인에 *진짜 depth-based fog* 와 *진짜 emissive bloom* 을 통합하려면 현재 *프로젝트 인프라* 에 어떤 *리팩토링* 이 선행되어야 하는지 정리. 셰이더 단순화 우회 안 함.
> **작성 시점**: 2026-05-31. `game/module/ingame/temp` 브랜치 HEAD = `d93bf87 [fix] : fog 방향 반전`.

---

## 0. 배경

`apps/_MyApp_/main.cpp` 의 PostFX 체인 7-stage (`blurring/gamma/invert/sharpening/sobel/fog/bloom`) 가 통합 완료된 상태. 단 **fog 와 bloom 은 *셰이더 단순화* 우회 적용 중**:

- `apps/_MyApp_/resources/shaders/postprocess/fog.fs` — `vUV.y * 10.0` 스크린 공간 근사 (depth 안 씀)
- `apps/_MyApp_/resources/shaders/postprocess/bloom.fs` — `uColorMap` = `uIntensityMap` = `uScene` self-glow threshold blur (emissive map 안 씀)

사용자가 **진짜 depth-based fog (camera-pixel 거리 기반)** + **진짜 emissive bloom (별도 emissive output)** 을 원함. 위 셰이더 단순화는 *시각적으로 잘못된 논리* — 화면 위쪽이 무조건 흐려지는 것은 fog 의 정의가 아니다.

---

## 1. 현재 인프라 한계 (선행 리팩토링 필요)

### 1.1 `SJH::Framebuffer` 의 depth 가 RenderBuffer 만 — *sampling 불가*

[`src/buffer/framebuffer.h`](../../src/buffer/framebuffer.h), [`framebuffer.cpp`](../../src/buffer/framebuffer.cpp):

```cpp
class Framebuffer : public RenderTarget {
private:
    uint32_t   mRBODepthStencilBuffer{0};  // ← RBO (GL_DEPTH24_STENCIL8)
    TexturePtr mColorAttachment;
    // ❌ mDepthAttachment 없음 — depth Texture 어태치먼트 모드 부재
};
```

`InitWithColorAttachment` 가 항상 *RBO* 로 depth 처리. 셰이더에서 `sampler2D uDepth` 로 sample 불가.

**필요 리팩토링**:
- `static FramebufferUPtr CreateWithDepthTexture(int w, int h)` factory 추가
- `mDepthAttachment` (TexturePtr) 멤버 + `GetDepthAttachment()` API
- `glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, ..., depthTex->GetTextureID(), 0)` 로 attach

### 1.2 `SJH::Texture::Create(w, h, format)` 의 3-arg overload — depth 텍스처 생성 불가

[`src/resource_registry/texture.cpp:114~124`](../../src/resource_registry/texture.cpp):

```cpp
void Texture::SetTextureFormat(int width, int height, uint32_t format) {
    glTexImage2D(GL_TEXTURE_2D, 0, mFormat /*internalFormat*/,
                 mWidth, mHeight, 0,
                 mFormat /*format — 같은 값 재사용 ←  문제 */,
                 GL_UNSIGNED_BYTE /*type — 고정 */, nullptr);
}
```

문제:
- `GL_DEPTH_COMPONENT24` 같은 *internal-only* enum 을 `format` 위치에 넣으면 `GL_INVALID_ENUM`. 정상은 `internalFormat=GL_DEPTH_COMPONENT24` + `format=GL_DEPTH_COMPONENT` (분리).
- `type` 도 `GL_UNSIGNED_BYTE` 하드코딩 — depth 는 보통 `GL_FLOAT` 또는 `GL_UNSIGNED_INT_24_8`.

**필요 리팩토링**:
- `Texture::Create(int w, int h, uint32_t internalFormat, uint32_t format, uint32_t type)` 5-arg overload 추가
- `Texture::SetTextureFormat(int w, int h, uint32_t internalFormat, uint32_t format, uint32_t type)` 5-arg overload 추가
- depth Texture 의 기본 `SetFilter(GL_NEAREST, GL_NEAREST)` + `SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE)` 정통

### 1.3 PassComponent 자동 binding 이 `uScene` 1개 sampler 만 — `uDepth` / `uInverseProjection` 미지원

[`src/render/mesh_pass_processor.cpp`](../../src/render/mesh_pass_processor.cpp) 의 ScreenQuad 분기 (대략 line 225~):

```cpp
// uScene = inputFB 의 color attachment — 자동 binding
cmd.passMaterial->Properties.Textures["uScene"] = {
    cmd.inputFB->GetColorAttachment().get(), 0};
```

오직 `uScene` (sampler unit 0) 만 자동. 추가 sampler (`uDepth` 등) 나 matrix uniform (`uInverseProjection`) 은 *호출자가 Material::Properties 에 직접 set* 해야.

**선택 1 — 자동화 확장** (mesh_pass_processor 수정):
- `PassComponent` 에 *optional 멤버* `Framebuffer* DepthSourceFB = nullptr` 추가
- ScreenQuad 분기에서 `if (cmd.depthSource) { ... ["uDepth"] = {depthSource->GetDepthAttachment().get(), 1}; }`

**선택 2 — main.cpp 호출자 책임 유지** (인프라 변경 최소):
- 셰이더에서 사용하는 추가 uniform 은 *호출자 (main.cpp / 별도 system) 가* fog material 에 직접 set
- 단점: 매 frame 갱신 필요 + sceneFB resize 시 재바인딩 필요

### 1.4 WorldCamera 의 `InverseProjection` 매트릭스 자동 주입 시스템 부재

depth-based fog 가 view-space 좌표 복원하려면:
```glsl
vec4 ndc = vec4(vUV*2 - 1, depth*2 - 1, 1);
vec4 viewPos = uInverseProjection * ndc;
viewPos /= viewPos.w;
float dist = length(viewPos.xyz);  // camera-pixel 유클리디안 거리
```

`uInverseProjection` = `inverse(WorldCamera->GetProjectionMatrix())`.

현재:
- `Material::Properties::Mat4s` map 은 있음 (typed)
- `PropertyBlockSetter` 가 자동 송신
- 그러나 **WorldCamera 의 projection 매트릭스 → 특정 PostFX material 의 Mat4s 로 자동 binding 인프라 없음**

**필요 리팩토링**:
- main.cpp 가 `render()` 매 frame `inverse(mCamera->GetProjectionMatrix())` 계산 + fog material 의 `Properties.Mat4s["uInverseProjection"]` 에 set
- 또는 *Camera matrix bridge* 시스템 — 특정 PostFX material 들이 *World camera matrix* 를 자동 받도록 표 (Filament `materialInstance.setParameter("invProjection", ...)` 정통)

### 1.5 SceneFB resize 시 dependent material 의 Texture 재바인딩 자동화 부재

[`apps/_MyApp_/main.cpp`](../../apps/_MyApp_/main.cpp) `render()` 의 resize 분기 (대략 line 215~):

```cpp
if (window 크기 변경) {
    mSceneFB = SJH::Framebuffer::Create(fbW, fbH);  // ← 새 인스턴스
    // ❌ fogMat 의 Properties.Textures["uDepth"] 갱신 누락 — 옛 sceneFB 의 depth 가리킴
}
```

새 sceneFB 생성 시 *dangling reference* — fog material 이 이전 sceneFB 의 depth 를 가리키게 됨.

**필요 리팩토링**:
- main.cpp 가 resize 시 fog material 의 `uDepth` Texture 재바인딩 (수동)
- 또는 *Framebuffer reference tracking* 시스템 (큰 변경)

### 1.6 (Bloom 용) `SJH::SceneRenderer` MRT (Multiple Render Targets) 미지원

진짜 emissive bloom 은:
- WorldCamera 가 *2 attachment* 렌더 — `gl_FragData[0]` = albedo color, `gl_FragData[1]` = emissive
- bloom 셰이더가 *emissive map 만* 추출하여 blur

현재 `SceneRenderer` + `Material` + `Framebuffer` 모두 *single color attachment* 가정. MRT 지원하려면:
- `Framebuffer` 의 `CreateWithMRT(w, h, attachmentCount)` 추가
- WorldMesh 셰이더가 `out vec4 oAlbedo`, `out vec4 oEmissive` 출력
- `Material::Properties` 에 *emissive 표현* 추가 (별도 색 또는 emissive map)
- SceneRenderer 가 `glDrawBuffers(2, {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1})` 호출

큰 인프라 변경 — 별도 SP 권장.

---

## 2. 권장 작업 분해

### Phase 1 — Depth-based fog (SP 단위)

**범위**: §1.1 + §1.2 + §1.4 + §1.5

| Task | 파일 | 변경 |
|---|---|---|
| T1 | `src/resource_registry/texture.{h,cpp}` | `Create(w, h, internalFormat, format, type)` 5-arg overload + `SetTextureFormat` 5-arg overload + depth filter/wrap 기본값 |
| T2 | `src/buffer/framebuffer.{h,cpp}` | `CreateWithDepthTexture(w, h)` factory + `mDepthAttachment` 멤버 + `GetDepthAttachment()` API + `InitWithSizeAndDepthTexture` 본구현 |
| T3 | `apps/_MyApp_/resources/shaders/postprocess/fog.fs` | 원본 depth-based 복원 (uDepth + uInverseProjection + glsl-fog 3 mode) |
| T4 | `apps/_MyApp_/main.cpp` | (a) `mSceneFB = Framebuffer::CreateWithDepthTexture(...)` 2곳 (startup + resize). (b) startup 끝에 fog material 의 `uDepth` Texture set. (c) resize 시 fog material `uDepth` 재바인딩. (d) `render()` 매 frame `uInverseProjection` = `inverse(mCamera->projection)` set. (e) 헬퍼 `RebindFogUniforms()` private method 권장. |
| T5 | 빌드 + 시각 검증 | depth 0 (가까운 픽셀) = 원본, depth 1 (먼 픽셀) = FogColor 누적 |

**작업 추정**: 5 commits, 1 세션. 본 핸드오프 작성자가 step-by-step 까지 도달한 상태에서 사용자 요청으로 롤백됨.

### Phase 2 — Emissive bloom (별도 SP)

**범위**: §1.6 + 추가 Material/SceneRenderer 변경

**작업 추정**: 큰 SP. Phase 1 종료 후 별도 brainstorming 권장.

---

## 3. 안티 패턴 (사용자가 명시 거부)

### 3.1 ❌ 셰이더 단순화 — `vUV.y` 기반 fog 근사

```glsl
float dist = vUV.y * 10.0;  // ← 화면 위쪽이 무조건 fog ≠ 진짜 fog
```

사용자 인용:
> "화면 위쪽이면 흐리게 라는 논리는 하면 안된다. 멀리있는것이 Fog 에 감싸도록 해야 하는거다."

`vUV.y` 또는 `length(vUV - 0.5)` 같은 *스크린 공간 근사* 는 *depth 가 아니라 화면 위치* — fog 의 정의 (camera-pixel 거리) 와 불일치. 사용자가 직접 지적함.

### 3.2 ❌ Bloom 의 `uColorMap` = `uIntensityMap` = `uScene`

현재 `bloom.fs` 의 *threshold-based bright-pass* 는 *대체* 일 뿐 — 진짜 emissive 가 아님. 명시적으로 emissive map 분리 필요.

### 3.3 ❌ Composition Root 가 inner 모듈에 들어감

`.agents/skills/clean-ddd-hexagonal/SKILL.md` line 113 위반. 새 `SJH::bootstrap` 모듈 신설 안 함 — 본 프로젝트의 *Composition Root 는 main.cpp* 만.

### 3.4 ❌ Premature module split

`render_pipeline` 같은 신규 STATIC 모듈 분리 시기상조 — 자유 함수 family 로 충분 ([architecture.md §1](../../.claude/architecture.md) YAGNI 원칙).

---

## 4. 빠른 시작 가이드

### 4.1 진입점 파일

| 우선순위 | 파일 | 역할 |
|---|---|---|
| 1 | [`apps/_MyApp_/main.cpp`](../../apps/_MyApp_/main.cpp) `startup()` line 125~165 | mSceneFB 생성 + PostFX 체인 빌드 + fog 초기값 set 위치 |
| 2 | [`apps/_MyApp_/main.cpp`](../../apps/_MyApp_/main.cpp) `render()` resize 분기 | sceneFB 재생성 + dependent material 갱신 필요 위치 |
| 3 | [`apps/_MyApp_/resources/shaders/postprocess/fog.fs`](../../apps/_MyApp_/resources/shaders/postprocess/fog.fs) | 현재 *단순화 셰이더* — 원본 depth-based 로 복원 대상 |
| 4 | [`src/buffer/framebuffer.h`](../../src/buffer/framebuffer.h) `Framebuffer` 클래스 | depth-Texture mode 추가 대상 |
| 5 | [`src/resource_registry/texture.{h,cpp}`](../../src/resource_registry/texture.h) | depth 텍스처 생성 5-arg overload 추가 대상 |
| 6 | [`src/render/mesh_pass_processor.cpp`](../../src/render/mesh_pass_processor.cpp) `ScreenQuad` 분기 line 225 부근 | `uScene` 자동 binding 패턴 — `uDepth` 확장 후보 |

### 4.2 참조 commit (rollback 직전 진행 상태)

`d93bf87` HEAD 기준 — 본 핸드오프 작성자의 *시작 step* 진척:
- ✅ `Texture` 5-arg overload 추가 (롤백됨, 단 코드는 다시 보지 말 것 — 본 한계 §1.2 의 단순 적용)
- ✅ `Framebuffer::CreateWithDepthTexture` + `GetDepthAttachment` (롤백됨)
- ✅ `fog.fs` depth-based 복원 (롤백됨)
- 🟡 main.cpp 의 `RebindFogUniforms()` 헬퍼 미완료 (사용자 중단 시점)
- ❌ resize 분기의 fog material 재바인딩 미적용

### 4.3 진입 흐름

1. brainstorming skill — 본 핸드오프 + spec 작성
2. writing-plans skill — Phase 1 의 T1~T5 plan 작성
3. subagent-driven-development — 각 Task implementer + spec + quality review
4. 시각 검증 — 카메라 이동 시 *먼 픽셀이 FogColor 누적* 확인

---

## 5. 검증 시나리오 (Phase 1 시각 회귀)

| # | 시나리오 | 기대 |
|---|---|---|
| V1 | fog ON + density=0.05 + mode=Exp2 | 화면의 *먼 픽셀* (depth=1 부근) 이 FogColor 누적 |
| V2 | fog OFF | 변경 전 PostFX 결과와 동일 |
| V3 | WASD 카메라 이동 | fog 패턴이 카메라 움직임에 따라 *3D 일관* 변화 (지면이 멀어질수록 흐려짐) |
| V4 | uFogColor ImGui ColorEdit3 | 즉시 시각 반영 |
| V5 | 창 리사이즈 (드래그) | sceneFB 재생성 후에도 fog 정상 동작 (dangling 없음) |
| V6 | bloom (현재 단순화) — 영향 0 | Phase 1 범위 외, fog 통합이 bloom 깨면 안 됨 |

---

## 6. 안 받은 결정

본 핸드오프 작성자는 Phase 1 의 T1~T4 의 *형태* 까지 step-by-step 도달했으나, 사용자가 *"너의 작업 롤백 후 핸드오프 프롬프트만 작성"* 명령으로 모든 변경 폐기. **다른 Claude Agent 는 이 핸드오프 기반으로 사용자에게 brainstorming 부터 재진입 + spec 작성 + 동의 후 구현**. 본 작성자가 도달한 코드는 *참조 불필요* — 위 §1 의 한계 + §2 의 권장 분해만 활용.

---

**핸드오프 끝**.
