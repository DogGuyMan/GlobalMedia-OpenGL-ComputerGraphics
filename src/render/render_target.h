/**
 * @file render_target.h
 * @brief 그릴 대상의 다형 추상 — Default backbuffer / FBO / (미래) Shadow/MRT/MSAA 등의 *공통 슬롯*.
 *
 * @details
 *  ### 존재 의의
 *  ### 미래 구체 후보 (LearnOpenGL / SuperBible 자연 다음 단계)
 *  | # | 구체 클래스 | 도입 시점 | Bind 동작 차이 |
 *  |---|---|---|---|
 *  | 1 | `DepthOnlyFramebuffer` | Shadow Mapping 챕터 | `glDrawBuffer(GL_NONE)` + depth 텍스처만 |
 *  | 2 | `MSAAFramebuffer` | Anti-Aliasing 챕터 | `glRenderbufferStorageMultisample` + resolve pass |
 *  | 3 | `GBufferFramebuffer` | Deferred Shading 챕터 | `glDrawBuffers(N, ...)` — MRT |
 *  | 4 | `HDRFramebuffer` | HDR/Bloom 챕터 | `GL_RGBA16F` 포맷 + Tonemapping |
 *  | 5 | `CubemapFramebuffer` | Point Shadow / Env Probe | 6 face + Layered |
 *  | 6 | `OffscreenRenderTarget` | ImGui Editor 통합 | Texture 로 렌더, ImGui 표시 |
 *
 *  ### 정통 엔진 매핑
 *  - Unity `RenderTexture` / `RTHandle` (base)
 *  - Unreal `FRHIRenderTargetView`
 *  - DX12 RTV/DSV descriptor 추상
 *  - Vulkan `VkImageView` + `VkFramebuffer` 조합
 *  -> 모든 메이저 엔진이 RenderTarget 추상화. *근본적 다형성 요구*.
 *
 *  ### Owner 분담 (SP-RTOwnership)
 *  - `DefaultRenderTarget` — **Application** 이 보유 (DX11 SwapChain 정통)
 *  - `Framebuffer` (FBO) — **ResourceRegistry** 가 보유 (Unity RTHandleSystem 정통)
 *  - `DeviceContext` 는 *보유 없음* — `BeginFrame(RenderTarget&)` 으로 명령만 발행
 */
#ifndef __SJH_RENDER_TARGET_H__
#define __SJH_RENDER_TARGET_H__

#include "GL/gl3w.h"
#include "common/common.h"

namespace SJH
{
	CLASS_PTR(RenderTarget);
	/// @brief 그릴 대상의 *다형 추상* — Bind + Size 만 — 모든 구체가 따라야 할 *LSP 계약*.
	/// @details
	///   - **Bind 책임**: `glBindFramebuffer` + `glViewport` 를 *구체에 적합한 형태* 로 실행.
	///     예: Default = FBO 0 + 화면 viewport, FBO = 자기 FBO + 자기 size viewport,
	///     Shadow = FBO + `glDrawBuffer(GL_NONE)`, MSAA = multisample FBO 활성화.
	///   - **GetSize 책임**: viewport 산출 + Camera aspect 계산용 — 구체가 *자기 size* 반환.
	class RenderTarget
	{
	  public:
		virtual ~RenderTarget() = default;
		virtual void Bind() = 0; ///< glBindFramebuffer + glViewport (구체 특수성 흡수)
		virtual int GetWidth() const = 0;
		virtual int GetHeight() const = 0;

	  private:
	};

	/// @brief 화면 (FBO 0) backbuffer — *유일 인스턴스, Application 책임*.
	/// @details
	///   - **유일성**: window 당 1개 — *이름 키 캐시* 의미 없음 (`ResourceRegistry` 대상 아님).
	///   - **Owner**: 본 프로젝트의 `migrate_demo_app` 같은 *Application 클래스* — DX11 `IDXGISwapChain` 정통.
	///   - **Resize 정책**: *재생성* (`unique_ptr` 교체) — 불변 size 필드를 둔 단순성 우선.
	///   - **Bind 동작**: `glBindFramebuffer(GL_FRAMEBUFFER, 0)` + `glViewport(0,0,w,h)`.
	///
	///   왜 `Resource` 가 아닌가 — 생성 비용 0 + 공유 X + 이름 키 인위적 + 다중 인스턴스 X.
	///   -> ResourceRegistry 의 *5 종 시민 (Texture/Material/Model/Program/Mesh)* 와 *패턴 다름*.
	class DefaultRenderTarget : public RenderTarget
	{
	  public:
		DefaultRenderTarget(int width, int height);
		void Bind() override;
		virtual int GetWidth() const override { return mWidth; }
		virtual int GetHeight() const override { return mHeight; }

	  private:
		int mWidth;
		int mHeight;
	};
} // namespace SJH

#endif // __SJH_RENDER_TARGET_H__
