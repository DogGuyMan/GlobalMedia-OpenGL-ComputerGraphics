# PostFX 체인 Resize 견고화 — 설계 (depth-fog 디커플링 단독 SP)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **대상**: `apps/_MyApp_` PostFX 체인의 *창 리사이즈* 시 use-after-free + 중간 FB 스케일 불일치 결함 견고화.
> **작성 시점**: 2026-06-01. 브랜치 `game/module/ingame/temp`.
> **선행 핸드오프**: [`doc/handoffs/2026-05-31/2026-05-31-postfx-chain-resize-robustness.md`](../../../doc/handoffs/2026-05-31/2026-05-31-postfx-chain-resize-robustness.md)
> **결정**: 선택 A(in-place Resize) · depth-fog 디커플링 · RBO 모드 전용 · 최소 변경.

---

## 1. 배경 — 검증된 결함

`apps/_MyApp_/main.cpp` PostFX 체인은 **startup 1회 구성** 후 불변 가정 하에 매 프레임 순회된다. 리사이즈 분기는 `mSceneFB`만 새 인스턴스로 *교체*하므로, init 시점에 옛 `mSceneFB.get()`을 캡처한 raw 포인터가 dangling된다.

### 1.1 use-after-free 지점 (코드 검증 완료)

[`main.cpp:150`](../../../apps/_MyApp_/main.cpp#L150) — startup 시 `BuildPostFXChain(..., mSceneFB.get(), ...)`로 **첫 `PassComponent.InputFB` = 옛 `mSceneFB.get()`** 캡처. `InputFB`는 `const` 포인터([`pass_component.h:27`](../../../src/render/pass_component.h#L27))라 사후 재지정 불가.

[`main.cpp:223`](../../../apps/_MyApp_/main.cpp#L223) — 리사이즈 분기가 `mSceneFB = Framebuffer::Create(fbW, fbH)`로 **인스턴스 교체** → 옛 인스턴스 소멸. `mPostFXFBs`/`mPassComponents`는 미갱신.

다음 프레임 [`mesh_pass_processor.cpp:113-114`](../../../src/render/mesh_pass_processor.cpp#L113-L114)가 소멸된 Framebuffer를 역참조 → **use-after-free**.

추가로 `mPostFXFBs`(중간 FB)는 옛 크기 유지 → 전체화면 quad가 작은 FB로 렌더 후 업스케일 → 스케일 불일치/흐릿함.

> **발현 조건**: macOS retina는 startup 후 framebuffer 크기가 안정적이라 분기가 드물게 발동. **사용자가 창을 드래그 리사이즈**할 때만 노출.

### 1.2 핸드오프 검증 중 정정 — ScreenQuadStage는 dangling 아님

[`main.cpp:251`](../../../apps/_MyApp_/main.cpp#L251)이 매 프레임 `mScreenQuadStagePtr->SetSources({out ? out : mSceneFB.get()})`로 갱신하므로 ScreenQuadStage fallback은 자가 치유된다. **진짜 dangling은 첫 `PassComponent.InputFB` 한 곳뿐.** (핸드오프 §0.3이 정확.)

### 1.3 현재 인프라 실태

- `Framebuffer`([`framebuffer.h`](../../../src/buffer/framebuffer.h))는 `Create*` factory만 — in-place resize API 부재. depth는 **RBO 모드만**(`GL_DEPTH24_STENCIL8` 렌더버퍼). depth-texture 모드 미존재.
- `Texture`([`texture.h`](../../../src/resource_registry/texture.h))는 private `SetTextureFormat(w,h,fmt)`(= `glTexImage2D` 재할당) 보유 — 같은 핸들로 크기 재할당 가능.
- **depth-fog SP는 미구현** — spec(`2026-05-31-depth-based-fog-design.md`)만 존재. `RebindFogUniforms`/`CreateWithDepthTexture`/`GetDepthAttachment`/depth-texture 모드 코드 0. (단 color-only fog PostFX 패스 `uFogColor`/`uFogMode`는 [`main.cpp:159-164`](../../../apps/_MyApp_/main.cpp#L159)에 이미 존재 — depth 샘플링 안 함.)

---

## 2. 결정 (확정)

| # | 결정 | 근거 |
|---|---|---|
| D1 | **선택 A — in-place `Framebuffer::Resize`** | 포인터 안정 → PassComponent/PostFXDebug raw 참조 유지 → 재배선 0, const 보존. 선택 B(chain rebuild) 대비 변경 표면 최소. |
| D2 | **depth-fog 디커플링** | depth-fog 미구현(코드 0). 현재 RBO-only FB 기준으로 resize만 견고화. §3.x 연동은 후속 노트. |
| D3 | **RBO depth 모드 전용** | 현재 모든 `Create(w,h)` FB가 RBO 모드. depth-texture 모드는 미존재 → 추측 구현 금지(YAGNI). |
| D4 | **최소 변경** | redundant해지는 카메라 repoint([`main.cpp:224-227`](../../../apps/_MyApp_/main.cpp#L224-L227)) + 매 프레임 [`251`](../../../apps/_MyApp_/main.cpp#L251) `SetSources`는 **그대로 둠** — 회귀 위험 최소. |
| D5 | **리사이즈 단일 진입점 = 기존 `render()` 크기 비교 분기** | [`main.cpp:216`](../../../apps/_MyApp_/main.cpp#L216) 분기 유지. `onResize()`는 미관여(현행). 새 진입점 도입 안 함. |
| D6 | **디바운스 미도입** | in-place Resize는 핸들 재생성 0으로 가벼움. 측정 후 필요 시 후속(YAGNI). |

---

## 3. 작업 분해

### T1 — `Texture::Resize(int w, int h)` ([`texture.{h,cpp}`](../../../src/resource_registry/texture.h))

- 같은 `mTextureID` 유지하고 `SetTextureFormat(w, h, mFormat)` 재호출(= `glTexImage2D`로 새 크기 빈 스토리지 재할당).
- `mWidth`/`mHeight` 갱신. 포맷은 기존 `mFormat` 유지.
- **핵심**: 텍스처 핸들 불변 → `GetTextureID()`/`GetColorAttachment()` 포인터 안정 → 모든 sampler consumer가 재바인딩 없이 새 크기 인식.

### T2 — `Framebuffer::Resize(int w, int h)` ([`framebuffer.{h,cpp}`](../../../src/buffer/framebuffer.h))

```cpp
void Framebuffer::Resize(int w, int h) {
    if (mColorAttachment) mColorAttachment->Resize(w, h);   // 색상 어태치먼트 재할당 (T1)
    if (mRBODepthStencilBuffer) {                           // RBO depth/stencil 재할당
        glBindRenderbuffer(GL_RENDERBUFFER, mRBODepthStencilBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    // FBO 핸들·어태치먼트 결합 불변 (텍스처/RBO ID 동일) → 재attach 불필요.
    glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer);     // 상태 재검증
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        spdlog::error("Framebuffer::Resize 실패 {}x{}", w, h);
    BindToDefault();
}
```

- **RBO 모드 전제** (D3). 헤더 doc에 "RBO depth 모드 전용 — depth-texture 모드 도입 시 분기 확장 필요(depth-fog 후속)" 명시.
- **caveat**: `mColorAttachment`가 외부 공유 텍스처(`Create(texture)` 경로)면 공유 holder에 영향. 본 SP의 `mSceneFB`/`mPostFXFBs`는 전부 `Create(w,h)` 내부 소유라 무관. doc에 명시.

### T3 — `main.cpp` resize 분기 ([`main.cpp:216-228`](../../../apps/_MyApp_/main.cpp#L216))

```cpp
mSceneFB->Resize(fbW, fbH);                       // 교체 → in-place (포인터 안정)
for (auto& fb : mPostFXFBs)                        // 중간 FB 동기 리사이즈
    if (fb) fb->Resize(fbW, fbH);
// 카메라 repoint(224-227)는 D4(최소 변경)대로 유지 — redundant하나 무해(동일 포인터 재설정)
```

- `mSceneFB` 인스턴스 유지 → 첫 `PassComponent.InputFB` 영구 유효 → **use-after-free 해소**.
- `mPostFXFBs` 각 in-place Resize → 중간 FB 새 화면 크기 → 전 패스 정상.

**변경 표면 총합**: 신규 메서드 2개 + main.cpp 분기 3줄 교체. <PassComponent>/PostFXDebugLayer/render_pipeline.cpp **무변경**.

---

## 4. 검증 (시각 — 단위 테스트 자동 추가 안 함, MEMORY `no_auto_tests`)

| # | 시나리오 | 기대 |
|---|---|---|
| R1 | 창 드래그 리사이즈 (확대) | PostFX 전 패스(blur/gamma/invert/sharpen/sobel/fog/bloom) 정상, 크래시·dangling 없음 |
| R2 | 창 드래그 리사이즈 (축소) | 동일 — 스케일 불일치/흐릿함 없음 |
| R3 | 리사이즈 반복(드래그 흔들기) | FB/텍스처/RBO 핸들 누수 없음 (in-place라 핸들 재생성 0; `<shell>/CMakeExecute.sh debug _MyApp_ leaks`로 선택 확인) |
| R4 | PostFXDebug 토글 (리사이즈 후) | ImGui 패널이 유효 PassComponent 참조 (인스턴스 유지 → 재배선 불필요 확인) |
| R5 | 리사이즈 안 함 (정적) | 기존 동작과 동일 — 회귀 없음 |

**수용 기준**: Debug 빌드 무경고(`-Werror`) + R1~R5 시각 통과.

---

## 5. 안티패턴 (준수)

- ❌ 매 프레임 무조건 Resize — 크기 *변경 시*에만 (기존 분기 조건 유지).
- ❌ Composition Root를 inner 모듈로 이동 — resize 오케스트레이션은 main.cpp 책임, 코어 모듈엔 원자적 자원 연산(`Resize`)만. "PostFXChainManager" 신규 STATIC 모듈 분리 시기상조(YAGNI).
- ❌ const 무분별 제거 — 선택 A라 `PassComponent` const 유지(파이프라인 경계 불변 보존).
- ❌ depth-texture 모드 추측 구현 — 미존재. RBO 모드만. 헤더 doc에 후속 확장 지점 명시.

**커밋 정책**: 변경 파일만 명시 add (`texture.{h,cpp}`, `framebuffer.{h,cpp}`, `apps/_MyApp_/main.cpp`). working tree의 무관 변경(fog 셰이더/PlayerController/StageBuilder 등) 미포함.

---

## 6. 후속 노트 (범위 외)

depth-fog SP 도착 시:
- sceneFB가 depth-texture 모드(`CreateWithDepthTexture` 산출)로 바뀌면 `Framebuffer::Resize`에 **depth-texture 분기 추가**(`mDepthAttachment` 존재 검사로 color+depth 텍스처 둘 다 재할당).
- resize 경로에서 **`RebindFogUniforms()` 호출 필요** (depth attachment 텍스처가 새로 생기면 `uDepth`가 옛 텍스처 dangling). 순서: **Resize 후** Rebind.

---

**설계 끝**.
