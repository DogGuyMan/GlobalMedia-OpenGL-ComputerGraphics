/**
 * @file render_target.h
 * @brief 그릴 대상의 다형 추상 - Default backbuffer / FBO / (미래) Shadow/MRT/MSAA 등의 *공통 슬롯*.
 *
 * @details
 *  ### 책임
 *  - @c RenderTarget - @c Bind() + @c GetWidth/Height() 만을 강제하는 LSP 계약 추상.
 *  - @c DefaultRenderTarget - window backbuffer(FBO 0) 구현체.
 *
 *  ### 비-책임
 *  - [X] FBO 생성/소멸 - @c Framebuffer (ResourceRegistry) 가 담당.
 *  - [X] DeviceContext 상태 관리 - @c RenderTarget::Bind() 는 @c glBindFramebuffer + @c glViewport 만.
 *
 *  ### 미래 구체 후보 (LearnOpenGL / SuperBible 자연 다음 단계)
 *  | # | 구체 클래스 | 도입 시점 | Bind 동작 차이 |
 *  |---|---|---|---|
 *  | 1 | @c DepthOnlyFramebuffer | Shadow Mapping 챕터 | @c glDrawBuffer(GL_NONE) + depth 텍스처만 |
 *  | 2 | @c MSAAFramebuffer | Anti-Aliasing 챕터 | @c glRenderbufferStorageMultisample + resolve pass |
 *  | 3 | @c GBufferFramebuffer | Deferred Shading 챕터 | @c glDrawBuffers(N, ...) - MRT |
 *  | 4 | @c HDRFramebuffer | HDR/Bloom 챕터 | @c GL_RGBA16F 포맷 + Tonemapping |
 *  | 5 | @c CubemapFramebuffer | Point Shadow / Env Probe | 6 face + Layered |
 *  | 6 | @c OffscreenRenderTarget | ImGui Editor 통합 | Texture 로 렌더, ImGui 표시 |
 *
 *  ### 정통 엔진 매핑
 *  - Unity @c RenderTexture / @c RTHandle (base)
 *  - Unreal @c FRHIRenderTargetView
 *  - DX12 RTV/DSV descriptor 추상
 *  - Vulkan @c VkImageView + @c VkFramebuffer 조합
 *  -> 모든 메이저 엔진이 RenderTarget 추상화. *근본적 다형성 요구*.
 *
 *  ### Owner 분담 (SP-RTOwnership)
 *  - @c DefaultRenderTarget - **Application** 이 보유 (DX11 SwapChain 정통)
 *  - @c Framebuffer (FBO) - **ResourceRegistry** 가 보유 (Unity RTHandleSystem 정통)
 *  - @c DeviceContext 는 *보유 없음* - @c BindTarget(RenderTarget&) 으로 명령만 발행
 *
 * @note @c Framebuffer 는 @c RenderTarget 을 상속한다 (@c class @c Framebuffer @c : @c public @c RenderTarget). @c ScreenQuadStage 는
 *       소스(FBO)를 @c const @c Framebuffer* 로 직접 보유하고, 출력 대상(@c target)만 @c RenderTarget& 로 받는다.
 */
#ifndef __SJH_RENDER_TARGET_H__
#define __SJH_RENDER_TARGET_H__

#include "GL/gl3w.h"
#include "common/common.h"

namespace SJH
{
	CLASS_PTR(RenderTarget);

	/**
	 * @brief 그릴 대상의 *다형 추상* - Bind + Size 만 - 모든 구체가 따라야 할 *LSP 계약*.
	 * @details
	 *  - **Bind 책임**: @c glBindFramebuffer + @c glViewport 를 *구체에 적합한 형태* 로 실행.
	 *    예: Default = FBO 0 + 화면 viewport, FBO = 자기 FBO + 자기 size viewport,
	 *    Shadow = FBO + @c glDrawBuffer(GL_NONE), MSAA = multisample FBO 활성화.
	 *  - **GetSize 책임**: viewport 산출 + Camera aspect 계산용 - 구체가 *자기 size* 반환.
	 */
	class RenderTarget
	{
	  public:
		virtual ~RenderTarget() = default;

		/// @brief @c glBindFramebuffer + @c glViewport 를 구체 특성에 맞게 실행.
		virtual void Bind() = 0;

		/// @brief 렌더 대상의 가로 크기 (픽셀). Camera aspect 계산 및 viewport 설정에 사용.
		virtual int GetWidth() const = 0;

		/// @brief 렌더 대상의 세로 크기 (픽셀). Camera aspect 계산 및 viewport 설정에 사용.
		virtual int GetHeight() const = 0;

	  private:
	};

	/**
	 * @brief 화면 backbuffer (FBO 0) 구현체 - *유일 인스턴스, Application 책임*.
	 * @details
	 *  - **유일성**: window 당 1개 - 이름 키 캐시 의미 없음 (@c ResourceRegistry 대상 아님).
	 *  - **Owner**: Application 클래스 (@c unique_ptr 보유) - DX11 @c IDXGISwapChain 정통.
	 *  - **Resize 정책**: *재생성* (@c unique_ptr 교체) - 불변 size 필드를 둔 단순성 우선.
	 *  - **Bind 동작**: @c glBindFramebuffer(GL_FRAMEBUFFER, 0) + @c glViewport(0,0,w,h).
	 *
	 *  왜 @c ResourceRegistry 가 아닌가 - 생성 비용 0 + 공유 X + 이름 키 인위적 + 다중 인스턴스 X.
	 *  -> ResourceRegistry 의 5종 시민(Texture/Material/Model/Program/Mesh)과 패턴 다름.
	 */
	class DefaultRenderTarget : public RenderTarget
	{
	  public:
		/// @brief 창 크기로 초기화.
		/// @param width  backbuffer 가로 크기 (픽셀).
		/// @param height backbuffer 세로 크기 (픽셀).
		DefaultRenderTarget(int width, int height);

		/// @brief @c glBindFramebuffer(GL_FRAMEBUFFER, 0) + @c glViewport(0,0,mWidth,mHeight).
		void Bind() override;

		/// @brief 창 가로 크기 (픽셀).
		virtual int GetWidth() const override { return mWidth; }

		/// @brief 창 세로 크기 (픽셀).
		virtual int GetHeight() const override { return mHeight; }

	  private:
		int mWidth;  ///< 창 가로 크기 (픽셀).
		int mHeight; ///< 창 세로 크기 (픽셀).
	};
} // namespace SJH

#endif // __SJH_RENDER_TARGET_H__
