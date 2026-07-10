# Handoff — PostFX 체인 Resize 견고화 SP

> **수신자**: 본 프로젝트의 다른 Claude Agent
> **목적**: `apps/_MyApp_` 의 PostFX 체인이 *창 리사이즈* 시 **dangling pointer 역참조(use-after-free)** + 중간 FB 크기 불일치로 깨지는 *기존 결함* 을 견고화. depth-based fog SP(2026-05-31)의 spec §6 에서 *범위 외* 로 분리 기록된 항목의 정식 후속.
> **작성 시점**: 2026-05-31. 브랜치 `game/module/ingame/temp`.
> **선행 관계**: depth-based fog SP([`doc/superpowers/specs/2026-05-31-depth-based-fog-design.md`](../../doc/superpowers/specs/2026-05-31-depth-based-fog-design.md)) 와 *coordinate* — 그쪽 `RebindFogUniforms()` 가 본 SP 의 resize 경로 안에서 호출되어야 함(§3.1).

---

## 0. 배경 — 무엇이 깨지나

`apps/_MyApp_/main.cpp` 의 PostFX 체인은 **startup 에서 1회 구성**되고, 그 후 *불변 가정* 하에 매 프레임 순회된다. 그러나 창 리사이즈 분기는 `mSceneFB` *만* 재생성하므로, 체인의 나머지(중간 FB + PassComponent 포인터)가 옛 자원을 가리키게 된다.

### 0.1 구성 (startup)

[`main.cpp:150-152`](../../apps/_MyApp_/main.cpp#L150-L152):

```cpp
auto chain = SJH::Render::BuildPostFXChain(reg, *screenCamActorPtr, POSTFX_PROGRAM_CONFIGS, mSceneFB.get(), fb.Width, fb.Height);
mPostFXFBs      = std::move(chain.Framebuffers);   // 중간 FB N개 (startup 크기 고정)
mPassComponents = std::move(chain.PassComponents); // PassComponent* N개 (비소유 raw)
```

[`render_pipeline.cpp` `BuildPostFXChain`](../../src/render/render_pipeline.cpp#L64-L121) — `prevFB` 가 `sceneFB` 로 시작:

```cpp
Framebuffer* prevFB = sceneFB;
for (const auto& def : configs) {
    auto fb = Framebuffer::Create(fbWidth, fbHeight);   // 중간 FB — RBO depth, startup 크기 고정
    auto* pc = passActor->AddComponent<Scene::PassComponent>(prevFB, fbPtr, mat);
    prevFB = fbPtr;                                      // 체인: 첫 패스 InputFB = sceneFB
}
```

즉 **첫 PostFX 패스(blurring)의 `InputFB` = startup 시점 `mSceneFB.get()`**.

### 0.2 리사이즈 (render)

[`main.cpp:210-222`](../../apps/_MyApp_/main.cpp#L210-L222):

```cpp
if (!mDefaultTarget || mDefaultTarget->GetWidth() != fbW || mDefaultTarget->GetHeight() != fbH) {
    mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fbW, fbH);
    if (mCamera)       mCamera->Aspect = ...;
    if (mScreenCamera) mScreenCamera->Aspect = ...;
    mSceneFB = SJH::Framebuffer::Create(fbW, fbH);      // ← 새 인스턴스. 옛 mSceneFB 소멸.
    if (mCamera)       mCamera->SetTargetRenderTarget(mSceneFB.get());
    if (mScreenCamera) mScreenCamera->SetTargetRenderTarget(mSceneFB.get());
    // ❌ mPostFXFBs 재생성 없음 — 옛 크기 유지
    // ❌ mPassComponents 의 InputFB/OutputFB 재지정 없음 — 첫 패스 InputFB 가 옛 mSceneFB dangling
}
```

`onResize()` ([`main.cpp:356-369`](../../apps/_MyApp_/main.cpp#L356-L369)) 도 `mDefaultTarget` + aspect 만 갱신 — 체인 미관여.

### 0.3 결함의 심각도 — use-after-free 역참조 지점

`PassComponent.InputFB`/`OutputFB` 는 **const 포인터** ([`pass_component.h:27-28`](../../src/render/pass_component.h#L27-L28)) 라 사후 재지정 불가. 리사이즈 후 다음 프레임, [`mesh_pass_processor.cpp:113-114`](../../src/render/mesh_pass_processor.cpp#L113-L114) 가 **소멸된 Framebuffer 를 역참조**한다:

```cpp
effectiveMat->Properties.Textures["uScene"] = {
    cmd.inputFB->GetColorAttachment().get(), 0};   // ← cmd.inputFB = 첫 패스 옛 sceneFB (freed)
```

→ **use-after-free**. (운 좋으면 옛 메모리가 아직 유효해 잠깐 동작하나, 정의되지 않은 동작.)

추가로 중간 FB(`mPostFXFBs`)는 옛 크기 유지 → 전체화면 quad 가 작은 FB 로 렌더 후 최종 blit 에서 업스케일 → 흐릿/스케일 불일치.

> ⚠ **실제 발현 조건**: macOS retina 는 startup 후 framebuffer 크기가 안정적이라 리사이즈 분기가 *드물게* 발동. 사용자가 창을 *드래그 리사이즈* 할 때만 노출 — depth-fog SP 의 V5 시나리오가 이를 의도적으로 노출함.

---

## 1. 현재 인프라 한계 (선행 리팩토링 필요)

### 1.1 `PassComponent.InputFB`/`OutputFB` 가 `const` — 사후 재지정 불가

[`pass_component.h:27-28`](../../src/render/pass_component.h#L27-L28):

```cpp
SJH::Framebuffer *const InputFB;   ///< readonly — 이전 패스의 OutputFB 와 포인터 공유
SJH::Framebuffer *const OutputFB;  ///< writable — 다음 패스의 InputFB 와 포인터 공유
```

`const` 는 *파이프라인 경계 불변* 을 의도한 설계지만, FB 재생성 시 재지정을 막는다.

**필요 결정**: const 제거(setter 추가) vs 체인 전체 rebuild.

### 1.2 `SJH::Framebuffer` 에 in-place `Resize(w,h)` API 부재

[`framebuffer.h`](../../src/buffer/framebuffer.h) 는 `Create*` factory 만 — 기존 인스턴스의 attachment 를 새 크기로 재할당하는 API 없음. 현재는 **인스턴스 교체(새 unique_ptr)** 만 가능 → 그 인스턴스를 가리키는 모든 raw 포인터가 dangling.

**선택 A (in-place resize)**: `void Framebuffer::Resize(int w, int h)` 추가 — FBO 핸들 유지하고 color/depth attachment 만 새 크기로 재생성. 포인터 안정 → repoint 불필요. (depth-fog SP 의 `GetDepthAttachment()` 도 새 텍스처로 자동 갱신되므로 `RebindFogUniforms()` 는 여전히 필요 — attachment 텍스처 객체가 바뀌므로.)

**선택 B (chain rebuild)**: 리사이즈 시 옛 PassActor 들 제거 + `mPostFXFBs` 파기 + `BuildPostFXChain` 재호출. 단순하나 teardown 비용 + §1.3 의 의존 재배선 필요.

### 1.3 체인 rebuild 시 *외부 비소유 참조* 가 dangling (선택 B 채택 시)

`mPassComponents` 를 raw 로 참조하는 곳이 둘:
- [`main.cpp:339`](../../apps/_MyApp_/main.cpp#L339) `std::vector<PassComponent*> mPassComponents` (PostFXDebug 토글 대상)
- [`PostFXDebugLayer.h:14`](../../apps/_MyApp_/src/UI/PostFXDebugLayer.h#L14) `PassComponent *Component; ///< 비소유` — ImGui 패널이 보유

체인을 rebuild 하면 이 둘 다 **재배선** 해야 한다 (PostFXDebugLayer 재생성 또는 entry 갱신 API). 선택 A(in-place resize)는 PassComponent 인스턴스가 유지되므로 이 문제 자체가 없다 → **선택 A 가 의존 면에서 유리**.

### 1.4 리사이즈 트리거 위치 이원화 — `render()` 분기 + `onResize()`

크기 변경 감지가 [`render()` 의 `mDefaultTarget` 크기 비교](../../apps/_MyApp_/main.cpp#L210)와 [`onResize()` 콜백](../../apps/_MyApp_/main.cpp#L356) 두 곳에 분산. 견고화 로직을 *한 곳* 으로 모으는 것이 정합성에 유리(중복 재생성/디바운스 회피).

**필요 결정**: 리사이즈 처리 단일 진입점 + 매 프레임 크기 비교 유지 여부.

---

## 2. 권장 작업 분해

### 선행 권장 = **선택 A (in-place Framebuffer::Resize + 포인터 안정)**

이유: PassComponent/PostFXDebug 의 raw 참조가 *유지* 되어 §1.3 재배선 불필요. const 제거 불필요. 변경 표면 최소.

| Task | 파일 | 변경 |
|---|---|---|
| T1 | `src/buffer/framebuffer.{h,cpp}` | `void Resize(int w, int h)` — color attachment(+ depth-texture 모드면 depth attachment) 새 크기 재생성, FBO 핸들 유지, `glCheckFramebufferStatus` 재검증. RBO 모드/텍스처 모드 둘 다 지원. |
| T2 | `src/resource_registry/texture.{h,cpp}` | (필요 시) `void Texture::Resize(int w, int h)` 또는 attachment 재생성을 Framebuffer 가 새 Texture 로 교체. ※ depth-fog SP 의 5-arg `Create` 재사용 가능. |
| T3 | `apps/_MyApp_/main.cpp` | 리사이즈 분기에서 `mSceneFB` *교체 대신* `mSceneFB->Resize(w,h)` + `for (auto& fb : mPostFXFBs) fb->Resize(w,h)` + `RebindFogUniforms()`. 카메라 target 재지정은 포인터 안정이면 불필요(확인). |
| T4 | 빌드 + 시각 검증 | depth-fog SP 의 V5 통과 — 드래그 리사이즈 중/후 PostFX(blur/gamma/.../fog/bloom) 정상, dangling/스케일 불일치 없음. |

> **대안 = 선택 B (chain rebuild)** 를 택할 경우: T1 = resize 시 PassActor 제거 헬퍼 + `BuildPostFXChain` 재호출, T2 = PostFXDebugLayer entry 재배선 API(`SetEntries`) 또는 Layer 재push, T3 = main.cpp 통합. 본 핸드오프 작성자는 **선택 A 권장** (§1.3 재배선 회피).

### 디바운스 (선택)
드래그 리사이즈는 프레임마다 발동 → in-place Resize 가 가벼우면 무방. 무거우면(많은 FB) 마지막 변경 후 N프레임 안정 시에만 적용하는 디바운스 고려 — *YAGNI, 측정 후 결정*.

---

## 3. depth-based fog SP 와의 연동 (필수)

### 3.1 `RebindFogUniforms()` 를 resize 경로에 포함

depth-fog SP 는 fog material 의 `uDepth` 를 `mSceneFB->GetDepthAttachment()` 로 바인딩한다. **in-place Resize 든 교체든, depth attachment 텍스처 객체가 바뀌면 `uDepth` 가 옛 텍스처를 가리킨다** → 본 SP 의 resize 경로가 `RebindFogUniforms()` 를 *반드시* 호출해야 한다 (depth-fog SP 의 D2 와 동일 책임, 더 넓은 범위로 흡수).

- depth-fog SP 가 *이미 main.cpp 리사이즈 분기에 `RebindFogUniforms()` 를 넣어둠* (그 SP 의 Task 4 Step 5). 본 SP 는 그 호출을 *유지* 하면서 sceneFB 교체를 in-place Resize 로 바꾸기만 하면 됨 — 순서 주의: **Resize 후** Rebind.

### 3.2 sceneFB 가 depth-texture 모드 (depth-fog SP 의 D5)

depth-fog SP 후 `mSceneFB` 는 `CreateWithDepthTexture` 산출물(color + depth-stencil 텍스처). 본 SP 의 `Framebuffer::Resize` 는 **depth-texture 모드도 지원** 해야 한다 (color + depth 텍스처 둘 다 재생성). RBO 모드(`mPostFXFBs`)와 텍스처 모드(`mSceneFB`)를 `mRBODepthStencilBuffer`/`mDepthAttachment` 존재로 분기.

---

## 4. 안티 패턴 (준수)

### 4.1 ❌ 매 프레임 무조건 재생성
크기 *변경 시* 에만. 매 프레임 FB 재할당은 GPU 메모리 churn + 성능 폭락.

### 4.2 ❌ Composition Root 를 inner 모듈로 이동
[`.claude/architecture.md`](../../.claude/architecture.md) — resize 오케스트레이션은 main.cpp(Composition Root) 책임. `Framebuffer::Resize` 같은 *원자적 자원 연산* 만 코어 모듈에 추가. "PostFXChainManager" 류 신규 STATIC 모듈 분리 시기상조(YAGNI).

### 4.3 ❌ const 무분별 제거
선택 B(rebuild)를 피하고 선택 A(in-place)를 택하면 `PassComponent` const 유지 가능 — *파이프라인 경계 불변* 설계 의도 보존. const 제거는 선택 B 채택 시에만.

### 4.4 ❌ depth-fog `RebindFogUniforms()` 누락
§3.1 — resize 경로에서 빠뜨리면 fog 가 옛 depth 텍스처 dangling. depth-fog SP 회귀.

---

## 5. 빠른 시작 가이드

### 5.1 진입점 파일

| 우선순위 | 파일 | 역할 |
|---|---|---|
| 1 | [`src/buffer/framebuffer.{h,cpp}`](../../src/buffer/framebuffer.h) | `Resize(w,h)` 추가 대상 — RBO/텍스처 모드 둘 다 |
| 2 | [`apps/_MyApp_/main.cpp`](../../apps/_MyApp_/main.cpp) `render()` resize 분기(L210~) | `mSceneFB` + `mPostFXFBs` Resize + `RebindFogUniforms()` |
| 3 | [`src/render/render_pipeline.cpp`](../../src/render/render_pipeline.cpp) `BuildPostFXChain` | 선택 B 채택 시 rebuild 진입점 (선택 A 면 미변경) |
| 4 | [`src/render/pass_component.h`](../../src/render/pass_component.h) | const InputFB/OutputFB — 선택 B 시 setter 대상 |
| 5 | [`apps/_MyApp_/src/UI/PostFXDebugLayer.h`](../../apps/_MyApp_/src/UI/PostFXDebugLayer.h) | 선택 B 시 entry 재배선 대상 (선택 A 면 미변경) |

### 5.2 진입 흐름 (본 프로젝트 컨벤션)

1. **brainstorming skill** — 본 핸드오프 기반 사용자와 선택 A/B 확정 + spec 작성 (`doc/superpowers/specs/`, 로컬 전용)
2. **writing-plans skill** — Task plan (`doc/superpowers/plans/`, 로컬 전용)
3. **subagent-driven-development** — Task 별 구현 + 빌드 게이트
4. **시각 검증** — depth-fog SP 의 V5 (드래그 리사이즈) 직접 실행/관찰
5. 프로젝트 정책: **단위 테스트 자동 추가 안 함**(MEMORY `no_auto_tests`) — 빌드 무경고 + 시각 검증이 수용 기준. 커밋은 *해당 파일만* 명시 add (working tree 의 fog/기타 무관 변경 미포함).

---

## 6. 검증 시나리오

| # | 시나리오 | 기대 |
|---|---|---|
| R1 | 창 드래그 리사이즈 (확대) | PostFX 전체(blur/gamma/invert/sharpen/sobel/fog/bloom) 정상, dangling/크래시 없음 |
| R2 | 창 드래그 리사이즈 (축소) | 동일 — 스케일 불일치/흐릿함 없음 |
| R3 | 리사이즈 중 fog ON | fog 가 새 크기 depth 로 정상 (uDepth 재바인딩 확인) |
| R4 | 리사이즈 반복(드래그 흔들기) | 메모리 누수/FB 핸들 누수 없음 (선택 시 leaks 체크: `shell/CMakeExecute.sh debug _MyApp_ leaks`) |
| R5 | PostFXDebug 토글 (리사이즈 후) | ImGui 패널이 유효 PassComponent 참조 (선택 B 면 재배선 확인) |
| R6 | 리사이즈 안 함 (정적) | 기존 동작과 동일 — 회귀 없음 |

---

## 7. 안 받은 결정 (brainstorming 에서 사용자 확정 필요)

1. **선택 A(in-place Resize) vs 선택 B(chain rebuild)** — 본 작성자 *A 권장*(§1.3 재배선 회피), 최종은 사용자.
2. **리사이즈 단일 진입점** — `render()` 크기 비교 유지 vs `onResize()` 콜백 일원화 (§1.4).
3. **디바운스 도입 여부** — 측정 후 결정 (§2).
4. `Framebuffer::Resize` 의 RBO/텍스처 모드 분기 방식 (`mRBODepthStencilBuffer`/`mDepthAttachment` 존재 검사).

---

**핸드오프 끝**.
