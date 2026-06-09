/**
 * @file device_context.cpp
 * @brief DeviceContext 구현 - GL 상태 래퍼 + 컴파일 타임 싱글톤 의미론 검증.
 *
 * @details
 *  ### 구현 노트
 *  - @c Get() : Meyer's 싱글톤 (C++11 static local - thread-safe 초기화).
 *  - @c BeginFrame : @c BindTarget -> @c Clear(color|depth|stencil) -> @c SetDepthTest(true) ->
 *    @c SetBlend(true) 순으로 패스 시작 상태를 일관되게 설정.
 *    Stencil 도 함께 clear (@c DEPTH24_STENCIL8 포맷 가정 - Stencil 미사용 패스는 추가 비용 미미).
 *  - static_assert 두 개 (SP2) - 복사/이동 생성자가 실수로 추가될 경우 컴파일 에러로 차단.
 */
#include "render/device_context.h"
#include "program/program.h"
#include <type_traits>

// SP2 - 싱글톤/리소스 의미론 컴파일 타임 검증.
static_assert(!std::is_copy_constructible_v<SJH::DeviceContext>,
              "SJH::DeviceContext must be non-copy-constructible (singleton)");
static_assert(!std::is_move_constructible_v<SJH::DeviceContext>,
              "SJH::DeviceContext must be non-move-constructible (singleton)");

namespace SJH
{
	DeviceContext &DeviceContext::Get()
	{
		static DeviceContext instance;
		return instance;
	}

	void DeviceContext::UseProgram(const Program &prog)
	{
		glUseProgram(prog.GetProgramAddr());
		mBoundProgram = &prog;
	}

	void DeviceContext::BindVAO(GLuint vao)
	{
		glBindVertexArray(vao);
	}

	void DeviceContext::BindTexture(GLuint unit, GLuint tex)
	{
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, tex);
	}

	void DeviceContext::BindTarget(RenderTarget &target)
	{
		target.Bind();
	}

	void DeviceContext::Clear(GLbitfield mask)
	{
		glClear(mask);
	}

	void DeviceContext::SetDepthTest(bool enabled, GLenum func)
	{
		if (enabled)
		{
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(func);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}
	}

	void DeviceContext::SetBlend(bool enabled, GLenum srcFactor, GLenum dstFactor)
	{
		if (enabled)
		{
			glEnable(GL_BLEND);
			glBlendFunc(srcFactor, dstFactor);
		}
		else
		{
			glDisable(GL_BLEND);
		}
	}

	void DeviceContext::DrawIndexed(GLsizei count)
	{
		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
	}

	void DeviceContext::DrawArrays(GLenum mode, GLsizei count)
	{
		glDrawArrays(mode, 0, count);
	}

	void DeviceContext::BeginFrame(RenderTarget &target)
	{
		BindTarget(target);
		// Stencil 도 함께 clear - Framebuffer 가 DEPTH24_STENCIL8 라 함께 사용 가정.
		// Stencil 미사용 패스는 영향 없음 (단 한 번의 clear 비용만 추가).
		glClearStencil(0);
		Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		SetDepthTest(true, GL_LESS);
		SetBlend(true, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

} // namespace SJH
