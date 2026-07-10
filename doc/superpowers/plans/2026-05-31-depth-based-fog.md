# Depth-based Fog (Phase 1) Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `fog.fs` 의 스크린 공간 근사(`vUV.y*10`)를 sceneFB 의 실제 depth + `uInverseProjection` 기반 카메라-픽셀 거리 fog 로 교체.

**Architecture:** 코어 인프라 최소 추가(Texture 5-arg overload, Framebuffer depth-텍스처 모드) + fog.fs depth 복원 + main.cpp(Composition Root) 호출자 배선. `SJH::render`/`PassComponent`/`DrawCommand` 불변. depth 는 depth-stencil 텍스처로 sampling(stencil 보존).

**Tech Stack:** C++17, OpenGL 4.1 core / GLSL 410, sb7 vmath(column-major), CMake Ninja preset.

**Spec:** [`doc/superpowers/specs/2026-05-31-depth-based-fog-design.md`](../specs/2026-05-31-depth-based-fog-design.md) (D1~D5).

---

## ⚠ 프로젝트 컨벤션 (이 플랜의 테스트 정책)

- **단위 테스트 자동 추가 안 함** (MEMORY `no_auto_tests` — 사용자 요청 시에만). 본 SP 는 GL 시각 출력이 핵심이라 헤드리스 단위 테스트 부적합.
- **수용 기준 = 빌드 무경고(Debug `-Werror`) + 시각 검증(Task 5, V1~V7).** 각 코드 Task 의 "검증" 단계는 *빌드 성공* 이 게이트.
- **커밋 위생**: working tree 에 fog 와 무관한 변경(`AudioSystem`, `PlayerEntity.h`, `Spawns/`, `Playable/` 등) 다수 존재. 각 커밋은 **해당 Task 파일만** 명시적으로 `git add` — `git add -A`/`git commit -a` 금지.
- 빌드 명령(반복): `cmake --build --preset ninja --target _MyApp_`
- 실행(반복): `cd build_ninja/apps/_MyApp_ && ./_MyApp_` (리소스 상대경로 → cd 필수)

---

## File Structure

| 파일 | 책임 | Task |
|---|---|---|
| `src/texture/texture.h` / `.cpp` | depth/packed 포맷 텍스처 생성 (internalFormat/format/type 분리) | T1 |
| `<src>/buffer/framebuffer.h` / `.cpp` | depth-stencil 텍스처 attach FBO factory | T2 |
| `apps/_MyApp_/resources/shaders/postprocess/fog.fs` | depth → view 거리 fog | T3 |
| `apps/_MyApp_/main.cpp` | sceneFB depth-texture 교체 + fog uniform 배선 | T4 |

---

## Task 1: `SJH::Texture` — depth/packed 포맷 5-arg overload

**Files:**
- Modify: `src/texture/texture.h`
- Modify: `src/texture/texture.cpp`

**근거:** 현재 `SetTextureFormat` 은 internalFormat=format 동일 + type=`GL_UNSIGNED_BYTE` 하드코딩 → `GL_DEPTH24_STENCIL8` packed 포맷 생성 불가. 5-arg 로 셋을 분리.

- [ ] **Step 1: 헤더에 5-arg `Create` 선언 추가**

`src/texture/texture.h` — 기존 3-arg `Create` 선언( `static TextureUPtr Create(int width, int height, uint32_t format);` ) **바로 아래**에 추가:

```cpp
        /**
         * @brief 빈 GL 텍스처를 internalFormat/format/type 분리 지정으로 생성 — depth/packed 포맷용.
         * @param internalFormat GPU 저장 포맷 (@c GL_RGBA8, @c GL_DEPTH24_STENCIL8 등).
         * @param format         픽셀 데이터 채널 의미 (@c GL_RGBA, @c GL_DEPTH_STENCIL 등).
         * @param type           원소 타입 (@c GL_UNSIGNED_BYTE, @c GL_UNSIGNED_INT_24_8 등).
         * @return 생성된 텍스처 (@c unique_ptr). 실패 시 @c nullptr.
         * @note depth/packed 텍스처는 보간/mipmap 부적합 → 본 overload 는 @c GL_NEAREST 필터 + @c GL_CLAMP_TO_EDGE wrap 으로 생성.
         */
        static TextureUPtr Create(int width, int height,
                                  uint32_t internalFormat, uint32_t format, uint32_t type);
```

- [ ] **Step 2: 헤더에 5-arg `SetTextureFormat` 선언 + format/type 멤버 추가**

`src/texture/texture.h` — private 섹션의 기존 `void SetTextureFormat(int width, int height, uint32_t format);` **바로 아래**에 추가:

```cpp
        void SetTextureFormat(int width, int height,
                              uint32_t internalFormat, uint32_t format, uint32_t type); ///< 5-arg — internalFormat/format/type 분리.
```

`src/texture/texture.h` — 기존 `uint32_t mFormat{GL_RGBA};` 멤버 **바로 아래**에 추가 (resize SP 정합 — `Texture::Resize` 가 depth-stencil 텍스처도 정확히 재할당하도록 format/type 보관):

```cpp
        uint32_t mDataFormat{GL_RGBA};        ///< glTexImage2D 의 format 인자 (GL_DEPTH_STENCIL 등). Resize 재할당용.
        uint32_t mDataType{GL_UNSIGNED_BYTE}; ///< glTexImage2D 의 type 인자 (GL_UNSIGNED_INT_24_8 등). Resize 재할당용.
```

- [ ] **Step 3: cpp 에 5-arg `Create` 정의 추가**

`src/texture/texture.cpp` — 기존 3-arg `Texture::Create(...)` 정의( `return std::move(texture);` 로 끝나는 블록 ) **바로 아래**에 추가:

```cpp
	TextureUPtr Texture::Create(int width, int height,
	                            uint32_t internalFormat, uint32_t format, uint32_t type)
	{
		auto texture = TextureUPtr(new Texture());
		texture->CreateTexture();
		texture->SetTextureFormat(width, height, internalFormat, format, type);
		// depth/packed 텍스처 — CreateTexture 기본값(LINEAR_MIPMAP_LINEAR)은 mipmap 미생성 시
		// incomplete + depth 보간 부적합. NEAREST + CLAMP 로 덮어쓴다.
		texture->SetFilter(GL_NEAREST, GL_NEAREST);
		texture->SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
		return texture;
	}
```

- [ ] **Step 4: cpp 의 3-arg `SetTextureFormat` 을 5-arg 위임으로 리팩토링 + 5-arg 정의 추가 (DRY)**

`src/texture/texture.cpp` — 기존 3-arg `SetTextureFormat` 정의 전체:

```cpp
	void Texture::SetTextureFormat(int width, int height, uint32_t format)
	{
		mWidth = width;
		mHeight = height;
		mFormat = format;

		glTexImage2D(GL_TEXTURE_2D, 0, mFormat,
		             mWidth, mHeight, 0,
		             mFormat, GL_UNSIGNED_BYTE,
		             nullptr);
	}
```

을 아래로 **교체** (3-arg = 5-arg 위임, 동작 동일):

```cpp
	void Texture::SetTextureFormat(int width, int height, uint32_t format)
	{
		// 3-arg = 기존 동작 보존 — internalFormat=format, type=UNSIGNED_BYTE 로 5-arg 위임.
		SetTextureFormat(width, height, format, format, GL_UNSIGNED_BYTE);
	}

	void Texture::SetTextureFormat(int width, int height,
	                               uint32_t internalFormat, uint32_t format, uint32_t type)
	{
		mWidth      = width;
		mHeight     = height;
		mFormat     = internalFormat; // GetFormat() 의미 유지 — 저장 포맷(internal).
		mDataFormat = format;         // Resize 재할당용 — depth-stencil 은 GL_DEPTH_STENCIL.
		mDataType   = type;           // Resize 재할당용 — depth-stencil 은 GL_UNSIGNED_INT_24_8.

		glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat),
		             mWidth, mHeight, 0,
		             format, type, nullptr);
	}
```

- [ ] **Step 5: `Texture::Resize` 를 저장된 triple 로 재할당 (resize SP 정합)**

`src/texture/texture.cpp` — 기존 `Texture::Resize` 정의 전체:

```cpp
	void Texture::Resize(int width, int height)
	{
		// SetTextureFormat 은 바인딩하지 않으므로 먼저 본 텍스처를 GL_TEXTURE_2D 에 바인딩.
		Bind();
		// 같은 mTextureID 로 glTexImage2D 재호출 — 핸들 불변, 크기만 재할당.
		SetTextureFormat(width, height, mFormat);
		mWidth  = width;
		mHeight = height;
	}
```

을 아래로 교체 (3-arg → 저장된 5-arg triple — depth-stencil 텍스처도 정확히 재할당):

```cpp
	void Texture::Resize(int width, int height)
	{
		// SetTextureFormat 은 바인딩하지 않으므로 먼저 본 텍스처를 GL_TEXTURE_2D 에 바인딩.
		Bind();
		// 저장된 internalFormat/format/type triple 로 재할당 — depth-stencil(GL_DEPTH_STENCIL/
		// GL_UNSIGNED_INT_24_8) 도 GL_INVALID_ENUM 없이 정확. (mWidth/mHeight 는 5-arg 가 set.)
		SetTextureFormat(width, height, mFormat, mDataFormat, mDataType);
	}
```

- [ ] **Step 6: 빌드 검증 (컴파일 게이트)**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 링크까지 성공 (경고 0). 새 overload 가 아직 호출처 없으므로 *기존 동작 회귀 없음* + 기존 `Texture::Resize`(RGBA 경로) 동작 동일 확인이 목적.

- [ ] **Step 7: 커밋**

```bash
git add src/texture/texture.h src/texture/texture.cpp
git commit -m "feat(texture): internalFormat/format/type 분리 5-arg Create/SetTextureFormat — depth 텍스처 지원

3-arg 는 5-arg 위임으로 보존. depth/packed 텍스처는 NEAREST+CLAMP 기본.
format/type 멤버 보관 → Texture::Resize 가 depth-stencil 도 정확히 재할당(resize SP 정합).
depth-based fog (spec 2026-05-31) T1.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: `SJH::Framebuffer` — depth-stencil 텍스처 모드

**Files:**
- Modify: `<src>/buffer/framebuffer.h`
- Modify: `<src>/buffer/framebuffer.cpp`

**근거:** 현재 depth 는 RBO → sampling 불가. depth-stencil *텍스처* 를 `GL_DEPTH_STENCIL_ATTACHMENT` 로 붙여 sampling 가능 + stencil 보존 (D5).

- [ ] **Step 1: 헤더에 factory + getter + private 멤버 선언 추가**

`<src>/buffer/framebuffer.h` — 기존 `static FramebufferUPtr Create(int width, int height);` 선언 **바로 아래**에 추가:

```cpp
        /**
         * @brief 내부 RGBA8 색 텍스처 + depth-stencil *텍스처* 를 생성하는 factory.
         * @details depth 를 RBO 대신 텍스처로 attach → 셰이더가 @c sampler2D 로 .r=정규화 depth 읽기 가능.
         *          @c GL_DEPTH24_STENCIL8 사용 — depth 샘플링 + stencil(Outline) 동시 보존.
         * @return 성공 시 @c FramebufferUPtr, 실패 시 @c nullptr.
         */
        static FramebufferUPtr CreateWithDepthTexture(int width, int height);
```

`<src>/buffer/framebuffer.h` — 기존 `GetColorAttachment()` 정의 **바로 아래**에 추가:

```cpp
        /// @brief depth-stencil 텍스처 어태치먼트 반환 (텍스처 모드일 때만 non-null). depth-based fog 가 sampler 로 읽음.
        const TexturePtr GetDepthAttachment() const { return mDepthAttachment; }
```

`<src>/buffer/framebuffer.h` — private 섹션의 `bool InitWithSize(int width, int height);` **바로 아래**에 추가:

```cpp
        bool InitWithSizeAndDepthTexture(int width, int height);
```

`<src>/buffer/framebuffer.h` — private 멤버 `TexturePtr mColorAttachment;` **바로 아래**에 추가:

```cpp
        TexturePtr mDepthAttachment;         ///< depth-stencil 텍스처 (텍스처 모드). RBO 모드면 nullptr.
```

- [ ] **Step 2: cpp 에 factory + Init 정의 추가**

`<src>/buffer/framebuffer.cpp` — 기존 `Framebuffer::Create(int width, int height)` 정의 블록 **바로 아래**에 추가:

```cpp
    FramebufferUPtr Framebuffer::CreateWithDepthTexture(int width, int height)
    {
        auto framebuffer = FramebufferUPtr(new Framebuffer());
        if (!framebuffer->InitWithSizeAndDepthTexture(width, height))
            return nullptr;
        return framebuffer;
    }
```

`<src>/buffer/framebuffer.cpp` — 기존 `Framebuffer::InitWithSize(...)` 정의 블록 **바로 아래** (네임스페이스 닫기 `}` 직전)에 추가:

```cpp
    bool Framebuffer::InitWithSizeAndDepthTexture(int width, int height)
    {
        // 색 RGBA8 텍스처 (기존 3-arg) — GL_COLOR_ATTACHMENT0.
        auto colorU = Texture::Create(width, height, GL_RGBA);
        if (!colorU)
        {
            spdlog::error("Framebuffer::CreateWithDepthTexture: color 텍스처 생성 실패 — {}x{}", width, height);
            return false;
        }
        mColorAttachment = TexturePtr(std::move(colorU));

        // depth-stencil 텍스처 (T1 5-arg) — sampler2D 로 .r=depth, stencil 보존.
        auto depthU = Texture::Create(width, height,
                                      GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
        if (!depthU)
        {
            spdlog::error("Framebuffer::CreateWithDepthTexture: depth 텍스처 생성 실패 — {}x{}", width, height);
            return false;
        }
        mDepthAttachment = TexturePtr(std::move(depthU));

        glGenFramebuffers(1, &mFBOFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mFBOFramebuffer);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mColorAttachment->GetTextureID(), 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               mDepthAttachment->GetTextureID(), 0);

        auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            spdlog::error("Framebuffer::CreateWithDepthTexture: incomplete — {}", status);
            return false;
        }

        BindToDefault();
        return true;
    }
```

> 소멸자 변경 불필요 — `mDepthAttachment` 는 `shared_ptr` RAII 자동 해제, `mRBODepthStencilBuffer` 는 0 유지되어 `~Framebuffer` 의 RBO 해제 분기 자연 skip.

- [ ] **Step 3: `Framebuffer::Resize` 에 depth-texture 분기 추가 (resize SP 정합)**

기존 `Framebuffer::Resize`([framebuffer.cpp:48-68](../../../src/buffer/framebuffer.cpp))는 color + RBO 만 재할당(doc 주석이 "depth-texture 모드 도입 시 분기 확장 필요" 명시). `mDepthAttachment`(텍스처 모드) 분기 추가.

`<src>/buffer/framebuffer.cpp` — 기존 `Resize` 의 RBO 재할당 분기:

```cpp
        // depth/stencil RBO — 같은 RBO 핸들로 스토리지 재할당.
        if (mRBODepthStencilBuffer)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, mRBODepthStencilBuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
```

**바로 아래**에 추가 (텍스처 모드 — `mDepthAttachment` 존재 시 함께 재할당; RBO 분기와 상호배타: 텍스처 모드면 `mRBODepthStencilBuffer`==0):

```cpp
        // depth/stencil 텍스처 모드(CreateWithDepthTexture) — 같은 텍스처 핸들로 스토리지 재할당.
        // Texture::Resize 가 저장된 format/type triple(GL_DEPTH_STENCIL/GL_UNSIGNED_INT_24_8)로 정확히 재할당.
        if (mDepthAttachment)
            mDepthAttachment->Resize(width, height);
```

> `mDepthAttachment` 는 §Step1 에서 추가한 멤버. 텍스처 모드 FBO 는 color+depth 텍스처가 같은 크기로 재할당되어 `glCheckFramebufferStatus` 재검증(기존 Resize 말미)이 `GL_FRAMEBUFFER_COMPLETE` 유지.

- [ ] **Step 4: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 성공 (경고 0). 아직 `CreateWithDepthTexture` 호출처 없음 — 회귀 없음 + 기존 RBO Resize 경로 동작 동일.

- [ ] **Step 5: 커밋**

```bash
git add <src>/buffer/framebuffer.h <src>/buffer/framebuffer.cpp
git commit -m "feat(framebuffer): CreateWithDepthTexture + Resize depth-texture 분기 — depth sampling

RBO 대신 GL_DEPTH24_STENCIL8 텍스처로 depth sampling 가능(stencil 보존).
Resize 에 mDepthAttachment 분기 추가(resize SP 정합 — color/depth 동기 재할당).
기존 RBO Create/Resize 보존. depth-based fog (spec 2026-05-31) T2.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: `fog.fs` — depth-based view 거리 fog

**Files:**
- Modify (전체 교체): `apps/_MyApp_/resources/shaders/postprocess/fog.fs`

**근거:** `uDepth` + `uInverseProjection` 로 view-space 위치 복원 → 카메라-픽셀 거리로 fog. `depth>=0.9999`(skybox) 제외 (D3).

- [ ] **Step 1: fog.fs 전체 교체**

`apps/_MyApp_/resources/shaders/postprocess/fog.fs` 를 아래 내용으로 **완전히 교체**:

```glsl
#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene;            // 직전 PostFX 출력 (색).
uniform sampler2D uDepth;            // sceneFB depth-stencil 텍스처 — .r = 정규화 depth [0,1].
uniform mat4      uInverseProjection;// inverse(WorldCamera projection) — NDC→view 복원.

// Fog 파라미터 — uFogStart/uFogEnd 는 *view 거리 단위* (스크린 비율 아님).
uniform vec3  uFogColor   = vec3(0.5, 0.6, 0.7);
uniform float uFogDensity = 0.05;
uniform float uFogStart   = 0.0;   // Linear 모드 시작 거리.
uniform float uFogEnd     = 50.0;  // Linear 모드 fully-fogged 거리.
uniform int   uFogMode    = 2;     // 0=Linear, 1=Exp, 2=Exp2.

// ==========================================
// glsl-fog 레퍼런스 함수 (입력 dist = view-space 유클리디안 거리).
// https://github.com/hughsk/glsl-fog
// ==========================================

float fogFactorLinear(const float dist, const float start, const float end) {
    return 1.0 - clamp((end - dist) / (end - start), 0.0, 1.0);
}

float fogFactorExp(const float dist, const float density) {
    return 1.0 - clamp(exp(-density * dist), 0.0, 1.0);
}

float fogFactorExp2(const float dist, const float density) {
    const float LOG2 = -1.442695;
    float d = density * dist;
    return 1.0 - clamp(exp2(d * d * LOG2), 0.0, 1.0);
}

void main()
{
    vec3  sceneColor = texture(uScene, vUV).rgb;
    float rawDepth   = texture(uDepth, vUV).r;

    // D3 — skybox/배경(far plane, depth≈1.0) 은 fog 제외 (하늘 또렷 유지).
    if (rawDepth >= 0.9999)
    {
        fragColor = vec4(sceneColor, 1.0);
        return;
    }

    // NDC → view-space 복원. perspective divide 로 view 좌표 확정.
    vec4 ndc     = vec4(vUV * 2.0 - 1.0, rawDepth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInverseProjection * ndc;
    viewPos     /= viewPos.w;
    float dist   = length(viewPos.xyz);  // 카메라-픽셀 유클리디안 거리.

    float fogAmount;
    if (uFogMode == 0)      fogAmount = fogFactorLinear(dist, uFogStart, uFogEnd);
    else if (uFogMode == 1) fogAmount = fogFactorExp(dist, uFogDensity);
    else                    fogAmount = fogFactorExp2(dist, uFogDensity);

    fragColor = vec4(mix(sceneColor, uFogColor, fogAmount), 1.0);
}
```

- [ ] **Step 2: GLSL 문법 검증 (선택)**

Run: `glslangValidator apps/_MyApp_/resources/shaders/postprocess/fog.fs` (설치 시 `/opt/homebrew/bin/glslangValidator`)
Expected: 에러 없음. (미설치면 Task 4 빌드+실행 시 셰이더 컴파일 로그로 대체 검증.)

- [ ] **Step 3: 커밋**

```bash
git add apps/_MyApp_/resources/shaders/postprocess/fog.fs
git commit -m "feat(fog.fs): depth-based view 거리 fog — uDepth+uInverseProjection, skybox 제외

vUV.y 스크린 근사 제거. NDC→view 복원으로 카메라-픽셀 거리 산출.
depth>=0.9999(skybox) early-return. depth-based fog (spec 2026-05-31) T3.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 4: main.cpp — sceneFB depth-texture 교체 + fog uniform 배선

**Files:**
- Modify: `apps/_MyApp_/main.cpp`

**근거:** D1(호출자 책임) — sceneFB 를 depth-texture FBO 로 교체, fog material 에 `uDepth`(unit 1) 바인딩 + 매 프레임 `uInverseProjection` 송신, resize 시 재바인딩.

- [ ] **Step 1: `Mat4Inverse` 자유 함수 추가 (익명 네임스페이스)**

`apps/_MyApp_/main.cpp` — 익명 `namespace { ... }` 안, `ProgramConfig` 구조체 정의 **바로 위**(line ~68 `struct ProgramConfig` 직전)에 추가:

```cpp
		// sb7 vmath 는 일반 역행렬 미제공(camera.h 명시) + Camera::InverseAffine 은 affine 전용.
		// perspective projection(비-affine, w≠1) 역행렬 → cofactor 기반 4x4 일반 inverse (MESA gluInvertMatrix 정통).
		// vmath 는 column-major(m[col][row]) — flat 배열도 column-major(m[c*4+r])로 변환.
		vmath::mat4 Mat4Inverse(const vmath::mat4 &src)
		{
			float m[16];
			for (int c = 0; c < 4; ++c)
				for (int r = 0; r < 4; ++r)
					m[c * 4 + r] = src[c][r];

			float inv[16];
			inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
			inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
			inv[8]  =  m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
			inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
			inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
			inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
			inv[9]  = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
			inv[13] =  m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
			inv[2]  =  m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
			inv[6]  = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
			inv[10] =  m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
			inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
			inv[3]  = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
			inv[7]  =  m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
			inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
			inv[15] =  m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

			float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
			if (det == 0.0f)
				return vmath::mat4::identity(); // 특이행렬 가드.
			float invDet = 1.0f / det;

			vmath::mat4 out;
			for (int c = 0; c < 4; ++c)
				for (int r = 0; r < 4; ++r)
					out[c][r] = inv[c * 4 + r] * invDet;
			return out;
		}
```

- [ ] **Step 2: fog InitFloats 를 거리 단위로 갱신**

`apps/_MyApp_/main.cpp` — `POSTFX_PROGRAM_CONFIGS` 의 fog 항목(line ~90):

```cpp
		    {"fog",        "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/fog.fs",
		     {{"uFogDensity", 0.05f}, {"uFogStart", 0.0f}, {"uFogEnd", 1.0f}}},
```

을 아래로 교체 (`uFogEnd` 를 view 거리 단위 50 으로):

```cpp
		    {"fog",        "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/fog.fs",
		     {{"uFogDensity", 0.05f}, {"uFogStart", 0.0f}, {"uFogEnd", 50.0f}}},
```

- [ ] **Step 3: startup 의 sceneFB 생성을 depth-texture factory 로 교체**

`apps/_MyApp_/main.cpp` `startup()` (line ~124):

```cpp
			mSceneFB       = SJH::Framebuffer::Create(fb.Width, fb.Height);
```

→

```cpp
			mSceneFB       = SJH::Framebuffer::CreateWithDepthTexture(fb.Width, fb.Height);
```

- [ ] **Step 4: startup 의 fog 초기화 블록을 헬퍼 경유로 정리 + uDepth 바인딩**

`apps/_MyApp_/main.cpp` `startup()` — 기존 fog 초기값 루프 전체:

```cpp
			// fog 의 non-float 초기값 명시 set (PostFXStageConfig.InitFloats 는 Floats 만 지원).
			for (std::size_t i = 0; i < POSTFX_PROGRAM_CONFIGS.size() && i < mPassComponents.size(); ++i)
			{
				if (!mPassComponents[i] || !mPassComponents[i]->mMaterial)
					continue;
				if (POSTFX_PROGRAM_CONFIGS[i].Name == "fog")
				{
					auto &props = mPassComponents[i]->mMaterial->Properties;
					props.Vec3s["uFogColor"] = vmath::vec3(0.5f, 0.6f, 0.7f);
					props.Ints["uFogMode"]   = 2; // 0=Linear, 1=Exp, 2=Exp2
				}
			}
```

을 아래로 교체 (`FindFogMaterial()` 사용 — fragile `const char* ==` 비교 제거 + uDepth 바인딩):

```cpp
			// fog 의 non-float 초기값 + uDepth 바인딩 (PostFXStageConfig.InitFloats 는 Floats 만 지원).
			if (auto *fogMat = FindFogMaterial())
			{
				fogMat->Properties.Vec3s["uFogColor"] = vmath::vec3(0.5f, 0.6f, 0.7f);
				fogMat->Properties.Ints["uFogMode"]   = 2; // 0=Linear, 1=Exp, 2=Exp2
			}
			RebindFogUniforms(); // uDepth = sceneFB depth 텍스처 (unit 1).
```

- [ ] **Step 5: render() 의 resize 분기에 `RebindFogUniforms()` 추가**

> ⚠ **resize SP 정합(2026-06-01)**: resize 분기는 이미 별도 SP 가 **in-place `mSceneFB->Resize()` + `mPostFXFBs` 동기 Resize** 로 리팩토링함(use-after-free 해소). Task 2 Step 3 에서 `Framebuffer::Resize` 가 depth 텍스처도 재할당하므로 depth-fog 가 추가할 것은 **`RebindFogUniforms()` 방어 호출뿐**(in-place 라 depth 텍스처 객체 유지 → 사실상 no-op이나 의미 보존). sceneFB factory 교체 불필요 — startup(Step 3)만 `CreateWithDepthTexture`.

`apps/_MyApp_/main.cpp` `render()` resize 분기 (현재 코드):

```cpp
				mSceneFB->Resize(fbW, fbH);                       // 교체 → in-place (포인터 안정: 첫 PassComponent.InputFB dangling 해소)
				for (auto &fb : mPostFXFBs)                       // 중간 FB 동기 리사이즈 (스케일 불일치 해소)
					if (fb)
						fb->Resize(fbW, fbH);
				if (mCamera)
					mCamera->SetTargetRenderTarget(mSceneFB.get());
				if (mScreenCamera)
					mScreenCamera->SetTargetRenderTarget(mSceneFB.get());
```

— 마지막 `mScreenCamera->SetTargetRenderTarget(...)` 블록 **바로 아래**에 한 줄 추가:

```cpp
				RebindFogUniforms(); // resize 후 fog uDepth 방어 재바인딩 (in-place 라 no-op이나 의미 보존, D2).
```

- [ ] **Step 6: render() 매 프레임 uInverseProjection 송신**

`apps/_MyApp_/main.cpp` `render()` — skybox `u_time` 동기화 블록 직후(line ~236, `}` 닫은 뒤)에 추가:

```cpp
			// fog — WorldCamera projection 역행렬 송신 (Properties.Mat4s 자동 송신, D4).
			if (auto *fogMat = FindFogMaterial(); fogMat && mCamera)
				fogMat->Properties.Mat4s["uInverseProjection"] =
				    Mat4Inverse(mCamera->GetProjectionMatrix());
```

- [ ] **Step 7: private 멤버 메서드 `FindFogMaterial` + `RebindFogUniforms` 추가**

`apps/_MyApp_/main.cpp` — private 섹션(예: `CreateAndRegisterWorldCamera()` 정의 **바로 위**, line ~400)에 추가:

```cpp
		// fog PassComponent 의 Material 탐색 — POSTFX_PROGRAM_CONFIGS 와 mPassComponents 인덱스 정합.
		// (Name 비교는 strcmp — const char* == 포인터 비교 함정 회피.)
		SJH::Material *FindFogMaterial()
		{
			for (std::size_t i = 0; i < POSTFX_PROGRAM_CONFIGS.size() && i < mPassComponents.size(); ++i)
				if (mPassComponents[i] && std::strcmp(POSTFX_PROGRAM_CONFIGS[i].Name, "fog") == 0)
					return mPassComponents[i]->mMaterial;
			return nullptr;
		}

		// fog material 의 uDepth 를 현재 mSceneFB 의 depth 텍스처(unit 1)로 (재)바인딩.
		// startup + resize 직후 호출 — sceneFB 재생성 시 dangling 방지 (D2).
		void RebindFogUniforms()
		{
			auto *fogMat = FindFogMaterial();
			if (!fogMat || !mSceneFB || !mSceneFB->GetDepthAttachment())
				return;
			fogMat->Properties.Textures["uDepth"] = {mSceneFB->GetDepthAttachment().get(), 1}; // unit 1 (uScene=0).
		}
```

> `<cstring>` 는 main.cpp 가 이미 include (line ~58). `SJH::Material` 은 `material/material.h` 경유 가시 (pass_component.h 가 forward 만 하므로 직접 include 필요 시 추가). 빌드 에러 시 `#include "material/material.h"` 를 상단 include 블록에 추가.

- [ ] **Step 8: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 성공 (경고 0). 실패 시 흔한 원인: `SJH::Material` 불완전 타입 → `#include "material/material.h"` 추가.

- [ ] **Step 9: 커밋**

```bash
git add apps/_MyApp_/main.cpp
git commit -m "feat(_MyApp_): depth-based fog 배선 — sceneFB depth-texture + uDepth/uInverseProjection

CreateWithDepthTexture 교체(startup+resize), RebindFogUniforms 헬퍼,
매 프레임 Mat4Inverse(projection) 송신, fog 거리단위 초기값.
depth-based fog (spec 2026-05-31) T4.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: 시각 검증 (수용 기준)

**Files:** 없음 (실행 + 관찰).

- [ ] **Step 1: 빌드 + 실행**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
Expected: 셰이더 컴파일 로그에 fog.fs 에러 없음. 창 표시.

- [ ] **Step 2: 검증 시나리오 확인 (spec §5)**

| # | 조작 | 기대 | OK? |
|---|---|---|---|
| V1 | F1 → PostFXDebug 에서 fog ON (density 0.05, Exp2) | 지면 *먼 쪽*이 FogColor 누적, 가까운 쪽 원본 | |
| V2 | fog OFF 토글 | fog 도입 전과 동일 | |
| V3 | WASD/마우스 카메라 이동 | fog 가 원근에 *3D 일관* 변화 | |
| V4 | `uFogColor` ImGui ColorEdit3 (있으면) | 즉시 반영 | |
| V5 | 창 리사이즈 드래그 | sceneFB 재생성 후 fog 정상(깨짐/dangling 없음) | |
| V6 | bloom 단계 관찰 | fog 통합이 bloom 깨지 않음 | |
| V7 | 매트릭스 skybox | 고밀도에서도 skybox 또렷 (depth==1 제외) | |

- [ ] **Step 3: 회귀 의심 시 GL 로그 확인**

`info.flags.debug=1` 이므로 GL 에러는 콘솔 출력. `uInverseProjection`/`uDepth` warn-once 누락 경고가 *반복* 출력되면 uniform 미활성(셰이더 최적화 제거) 의심 → fog.fs 에서 실제 사용 확인.

> ⚠ V5(resize)는 **fog uDepth 한정** 보장 (D2). PostFX 체인 전체의 InputFB resize dangling 은 fog 이전부터의 기존 이슈(spec §6) — 본 SP 범위 외. V5 에서 *fog 외* PostFX 깨짐이 보이면 그것은 기존 결함이며 별도 SP.

---

## 완료 기준

- [ ] T1~T4 각 빌드 성공(경고 0) + 커밋 4개
- [ ] Task 5 의 V1·V3·V7 시각 확인 (depth fog 핵심 — 먼 픽셀 fog + 카메라 일관 + skybox 또렷)
- [ ] fog 무관 working tree 변경은 커밋에 미포함
