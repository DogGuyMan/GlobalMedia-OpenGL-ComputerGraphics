# PostFX 체인 Resize 견고화 Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `apps/_MyApp_` PostFX 체인이 창 드래그 리사이즈 시 발생시키는 use-after-free(첫 `PassComponent.InputFB` dangling) + 중간 FB 스케일 불일치를 in-place Framebuffer Resize로 견고화한다.

**Architecture:** `Texture::Resize`(같은 핸들로 `glTexImage2D` 재할당)와 `Framebuffer::Resize`(color 텍스처 + RBO 재할당, FBO 핸들 불변)를 코어 모듈에 추가하고, `main.cpp` 리사이즈 분기에서 `mSceneFB` 인스턴스 *교체*를 *in-place Resize*로 전환한다. 포인터 안정성으로 PassComponent/PostFXDebug raw 참조가 유지되어 재배선이 불필요하다.

**Tech Stack:** OpenGL (gl3w), C++17, CMake/Ninja, spdlog. RBO depth 모드(`GL_DEPTH24_STENCIL8`) 전용.

**테스트 정책:** 본 프로젝트는 단위 테스트 자동 추가 안 함(MEMORY `no_auto_tests`). 각 Task의 검증 게이트 = **Debug 빌드 무경고(`-Werror`)** + 최종 **시각 검증**(spec §4 R1~R5). TDD failing-test 스텝 대신 빌드 게이트를 사용한다.

**커밋 정책:** spec §5 — 변경 파일만 명시 add. working tree의 무관 변경(`PlayerController.*`, `StageBuilder.cpp`, `mouse_input.*`, `transparent.*`, `MaterialTimeComponent.h` 등) **미포함**. `git commit -a` / `git add -A` 금지.

---

## File Structure

- `src/texture/texture.h` / `texture.cpp` — `void Resize(int w, int h)` public 메서드 추가. 책임: 같은 GL 핸들로 색상 텍스처 스토리지 재할당.
- `<src>/buffer/framebuffer.h` / `framebuffer.cpp` — `void Resize(int w, int h)` public 메서드 추가. 책임: color 어태치먼트(T1 위임) + RBO depth/stencil을 새 크기로 재할당, FBO 핸들·어태치먼트 결합 유지.
- `apps/_MyApp_/main.cpp` — 리사이즈 분기([216-228](../../apps/_MyApp_/main.cpp#L216))에서 `mSceneFB` 교체를 in-place Resize로 전환 + `mPostFXFBs` 동기 Resize.

변경 표면 총합: 신규 public 메서드 2개 + main.cpp 분기 3줄 교체. PassComponent / PostFXDebugLayer / render_pipeline.cpp **무변경**.

---

## Task 1: `Texture::Resize(int w, int h)`

**Files:**
- Modify: `src/texture/texture.h` (public 메서드 선언 추가)
- Modify: `src/texture/texture.cpp` (정의 추가)

**배경:** `Texture`는 이미 private `SetTextureFormat(int width, int height, uint32_t format)`(= `glBindTexture` + `glTexImage2D`로 빈 스토리지 할당)를 보유([texture.h:73](../../src/resource_registry/texture.h#L73)). 같은 `mTextureID`로 재호출하면 핸들 불변 + 크기만 재할당 → 모든 sampler consumer 포인터 안정.

- [ ] **Step 1: 헤더에 `Resize` 선언 추가**

`src/texture/texture.h` 의 public 섹션 — `SetWrap` 선언 바로 아래([texture.h:67](../../src/resource_registry/texture.h#L67) 다음 줄)에 추가:

```cpp
        /// @brief 같은 GL 핸들을 유지하고 색상 스토리지를 새 크기로 재할당 (FBO 어태치먼트 리사이즈용).
        /// @details 핸들(@c GetTextureID) 불변 → 이 텍스처를 sampler 로 참조하는 모든 consumer 가
        ///          재바인딩 없이 새 크기를 인식. 포맷은 기존 @c mFormat 유지.
        /// @param width  새 너비 (픽셀). @param height 새 높이 (픽셀).
        void Resize(int width, int height);
```

- [ ] **Step 2: 구현 추가**

`src/texture/texture.cpp` — `SetTextureFormat` 정의 근처(파일 내 적당한 public 메서드 그룹)에 추가:

```cpp
    void Texture::Resize(int width, int height)
    {
        // 같은 mTextureID 로 glTexImage2D 재호출 — 핸들 불변, 크기만 재할당.
        SetTextureFormat(width, height, mFormat);
        mWidth  = width;
        mHeight = height;
    }
```

> 주의: `SetTextureFormat` 가 내부에서 `glBindTexture(GL_TEXTURE_2D, mTextureID)` 후 `glTexImage2D` 하는지 확인. 만약 바인딩을 안 하면 `Resize` 앞에 `Bind();` 한 줄 추가. (구현 시 `texture.cpp` 의 `SetTextureFormat` 본문 확인 필수.)

- [ ] **Step 3: 빌드 게이트 — 모듈 컴파일 무경고**

Run:
```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -20
```
Expected: `resource_registry` 컴파일 통과, `Texture::Resize` 관련 경고/에러 없음. (이 시점엔 호출처가 없어 `-Wunused` 무관 — 메서드는 외부 링크라 경고 안 남.)

- [ ] **Step 4: 커밋**

```bash
git add src/texture/texture.h src/texture/texture.cpp
git commit -m "feat(resource_registry): Texture::Resize — 같은 핸들 in-place 스토리지 재할당

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: `Framebuffer::Resize(int w, int h)`

**Files:**
- Modify: `<src>/buffer/framebuffer.h` (public 메서드 선언 추가)
- Modify: `<src>/buffer/framebuffer.cpp` (정의 추가)

**배경:** `Framebuffer`는 `Create(w,h)` 시 내부 RGBA8 텍스처(`mColorAttachment`) + RBO depth/stencil(`mRBODepthStencilBuffer`)을 소유([framebuffer.cpp:60-90](../../src/buffer/framebuffer.cpp#L60))。color는 T1로 위임, RBO는 `glRenderbufferStorage` 재호출. FBO 핸들·어태치먼트 결합은 ID 불변이라 재attach 불필요.

- [ ] **Step 1: 헤더에 `Resize` 선언 추가**

`<src>/buffer/framebuffer.h` 의 "기존 API 보존" 섹션 — `GetColorAttachment` 선언 아래([framebuffer.h:76](../../src/buffer/framebuffer.h#L76) 다음)에 추가:

```cpp
        /// @brief FBO 핸들을 유지하고 color 어태치먼트 + RBO depth/stencil 을 새 크기로 재할당.
        /// @details ⚠ RBO depth 모드 전용 — color 어태치먼트(@c mColorAttachment)와 RBO(@c mRBODepthStencilBuffer)만
        ///          재할당한다. depth-texture 모드(@c mDepthAttachment) 도입 시 분기 확장 필요(depth-fog 후속).
        ///          ⚠ @c mColorAttachment 가 외부 공유 텍스처(@c Create(TexturePtr) 경로)면 공유 holder 에 영향 —
        ///          @c Create(int,int) 로 생성한 자족 FBO 에만 안전.
        /// @param width  새 너비 (픽셀). @param height 새 높이 (픽셀).
        void Resize(int width, int height);
```

- [ ] **Step 2: 구현 추가**

`<src>/buffer/framebuffer.cpp` — `Bind()` 정의 아래([framebuffer.cpp:46](../../src/buffer/framebuffer.cpp#L46) 다음)에 추가:

```cpp
    void Framebuffer::Resize(int width, int height)
    {
        // color 어태치먼트 — 같은 텍스처 핸들로 in-place 재할당 (Texture::Resize).
        if (mColorAttachment)
            mColorAttachment->Resize(width, height);

        // depth/stencil RBO — 같은 RBO 핸들로 스토리지 재할당.
        if (mRBODepthStencilBuffer)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, mRBODepthStencilBuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        // FBO 핸들·어태치먼트 결합 불변(텍스처/RBO ID 동일) → 재attach 불필요. 상태만 재검증.
        glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer);
        auto result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (result != GL_FRAMEBUFFER_COMPLETE)
            spdlog::error("Framebuffer::Resize 실패 {}x{}: {}", width, height, result);
        BindToDefault();
    }
```

> `spdlog` 는 `framebuffer.cpp` 가 이미 `#include <<spdlog>/spdlog.h>` 함([framebuffer.cpp:2](../../src/buffer/framebuffer.cpp#L2)) — 추가 include 불필요.

- [ ] **Step 3: 빌드 게이트 — 모듈 컴파일 무경고**

Run:
```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -20
```
Expected: `buffer` 모듈 컴파일 통과, `Framebuffer::Resize` 관련 경고/에러 없음.

- [ ] **Step 4: 커밋**

```bash
git add <src>/buffer/framebuffer.h <src>/buffer/framebuffer.cpp
git commit -m "feat(buffer): Framebuffer::Resize — color+RBO in-place 재할당 (RBO 모드 전용)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: `main.cpp` 리사이즈 분기 in-place 전환

**Files:**
- Modify: `apps/_MyApp_/main.cpp:216-228` (리사이즈 분기)

**배경:** 현재 분기는 `mSceneFB = Framebuffer::Create(fbW, fbH)`로 인스턴스 교체([main.cpp:223](../../apps/_MyApp_/main.cpp#L223)) → 첫 `PassComponent.InputFB`(init 시점 옛 포인터 캡처, const) dangling + `mPostFXFBs` 옛 크기. in-place Resize로 포인터 안정 확보.

- [ ] **Step 1: 리사이즈 분기의 `mSceneFB` 교체를 in-place Resize로 전환**

`apps/_MyApp_/main.cpp` — 현재 [223번 줄](../../apps/_MyApp_/main.cpp#L223):

```cpp
				mSceneFB = SJH::Framebuffer::Create(fbW, fbH);
```

를 다음으로 교체:

```cpp
				mSceneFB->Resize(fbW, fbH);                        // 교체 → in-place (포인터 안정: 첫 PassComponent.InputFB dangling 해소)
				for (auto &fb : mPostFXFBs)                        // 중간 FB 동기 리사이즈 (스케일 불일치 해소)
					if (fb)
						fb->Resize(fbW, fbH);
```

> 그 아래 카메라 repoint 두 줄([224-227](../../apps/_MyApp_/main.cpp#L224))은 spec D4(최소 변경)대로 **그대로 둔다** — `mSceneFB.get()`이 동일 포인터라 `SetTargetRenderTarget`는 사실상 no-op이나 무해. 삭제/수정 금지.
> ⚠ `mSceneFB`가 분기 진입 시 항상 non-null인지 확인: init([main.cpp:128](../../apps/_MyApp_/main.cpp#L128))에서 생성되고 render() 도달 전 `dir.Enter()` 완료 → non-null 보장. 방어가 필요하면 `if (mSceneFB) mSceneFB->Resize(...)` 로 감쌀 수 있으나 YAGNI.

- [ ] **Step 2: 빌드 게이트 — 전체 무경고 빌드**

Run:
```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -20
```
Expected: 링크까지 통과(`-Werror` 무경고). `_MyApp_` 실행 파일 생성.

- [ ] **Step 3: 시각 검증 (spec §4 R1~R5)**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
관찰 (직접 수행):
- **R1** 창을 드래그로 **확대** → PostFX 전 패스(blur/gamma/invert/sharpen/sobel/fog/bloom) 정상, 크래시·깨짐 없음.
- **R2** 창을 드래그로 **축소** → 스케일 불일치/흐릿함 없음.
- **R3** 드래그로 빠르게 흔들기 → 크래시·시각 깨짐 없음 (핸들 in-place라 누수 없음).
- **R4** 리사이즈 후 PostFXDebug(ImGui) 토글 → 패널 정상 동작, 각 패스 enable 토글 반영.
- **R5** 리사이즈 안 하고 정적 실행 → 기존과 동일, 회귀 없음.

Expected: 5종 모두 통과. (이전엔 드래그 리사이즈 시 첫 PostFX 패스가 freed sceneFB 역참조 → 깨짐/UB.)

- [ ] **Step 4 (선택): leaks 체크**

macOS 한정, R3 누수 우려 확인용:
```bash
sh <shell>/CMakeExecute.sh debug _MyApp_ leaks
```
Expected: 리사이즈 반복 후에도 FB/Texture/RBO 핸들 누수 없음.

- [ ] **Step 5: 커밋**

```bash
git add apps/_MyApp_/main.cpp
git commit -m "fix(_MyApp_): PostFX resize use-after-free — sceneFB 교체→in-place Resize + 중간 FB 동기 리사이즈

첫 PassComponent.InputFB(const, init 캡처)가 freed sceneFB 역참조하던 결함 해소.
mPostFXFBs 동기 Resize로 스케일 불일치도 해결. 카메라 repoint는 최소 변경대로 유지.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

> ⚠ `git add`에 `apps/_MyApp_/main.cpp`만 명시. working tree의 무관 변경 파일들 미포함(spec §5).

---

## Self-Review 결과

**1. Spec 커버리지:**
- spec T1 (Texture::Resize) → Task 1 ✅
- spec T2 (Framebuffer::Resize) → Task 2 ✅
- spec T3 (main.cpp 분기) → Task 3 ✅
- spec §4 검증 R1~R5 → Task 3 Step 3 ✅
- spec D4 (카메라 repoint 유지) → Task 3 Step 1 주석 ✅
- spec §5 커밋 정책 → 전 Task 커밋 스텝 + Task 3 Step 5 경고 ✅

**2. Placeholder 스캔:** TBD/TODO/"적절히 처리" 없음. 전 코드 스텝에 실제 코드 블록 존재.

**3. 타입 일관성:** `Texture::Resize(int,int)` / `Framebuffer::Resize(int,int)` 시그니처 Task 1·2 선언과 Task 2·3 호출 일치. `mColorAttachment`/`mRBODepthStencilBuffer`/`mTextureID`/`mFormat`/`mWidth`/`mHeight` 는 실제 멤버명(framebuffer.h/texture.h 확인됨).

**검증 의존 주의:** Task 1 Step 2의 `SetTextureFormat`이 내부에서 텍스처를 바인딩하는지 `texture.cpp` 본문 확인 필요(미바인딩이면 `Bind()` 선행). 구현 시 첫 확인 사항.
