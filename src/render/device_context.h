#ifndef __SJH_DEVICE_CONTEXT_H__
#define __SJH_DEVICE_CONTEXT_H__

#include "GL/gl3w.h"
#include "program/program.h"
#include "render/render_target.h"
#include <memory>

namespace SJH
{

	/// @brief GL 호출의 단일 게이트웨이 + 현재 bound 상태 추적 (싱글톤).
	/// @details
	///   책임 5 가지:
	///   -# 현재 bound program 추적 (@c mBoundProgram)
	///   -# GL 상태 변경 단일 진입점 (glUseProgram / glBindVertexArray / glBindTexture / glClear / glEnable)
	///   -# Draw 명령 발행 (glDrawElements / glDrawArrays)
	///   -# RenderTarget 바인딩 위임 (@c BindTarget / @c BeginFrame)
	///   -# SP4 멀티패스 진입 지점 (@c BeginFrame 이 패스마다 다른 target 받음)
	///
	///   ### 순서 계약 (POLA 준수: 코드 강제 없음, 문서 명시)
	///   - @c UseProgram(prog) 호출 후에만 그 prog 에 @c Uniforms::Set* 호출 가능.
	///   - 디버그 빌드의 assertion 등은 *추가하지 않음* — hidden astonishment 회피.
	class DeviceContext
	{
	  public:
		/// @brief 싱글톤 접근. GL context 가 활성 상태일 때만 호출 유효.
		static DeviceContext &Get();

		// bound 상태 변경

		void UseProgram(const Program &prog);
		void BindVAO(GLuint vao);
		void BindTexture(GLuint unit, GLuint tex);
		void BindTarget(RenderTarget &target);
		void Clear(GLbitfield mask);
		void SetDepthTest(bool enabled, GLenum func = GL_LESS);
		void SetBlend(bool enabled, GLenum srcFactor = GL_SRC_ALPHA,
		              GLenum dstFactor = GL_ONE_MINUS_SRC_ALPHA);

		// Draw 명령 

		void DrawIndexed(GLsizei count);
		void DrawArrays(GLenum mode, GLsizei count);

		//  유틸리티.

		/// @brief BindTarget + Clear(color|depth) + SetDepthTest(true) + SetBlend(true) 의 alias.
		/// @note  Stencil pass 등 커스텀 상태가 필요하면 primitive 메서드를 *직접* 호출.
		void BeginFrame(RenderTarget &target);

		// SP-RTOwnership — Default backbuffer 의 lifetime owner 는 *Application* 으로 이전.
		//   DefaultRenderTarget 의 *유일성 + 이름 키 인위적* 이라 Resource 가 아님.
		//   Framebuffer 는 `ResourceRegistry::CreateFramebuffer` 로 위탁.

		// 복사,이동 차단 (싱글톤)
		DeviceContext(const DeviceContext &) = delete;
		DeviceContext &operator=(const DeviceContext &) = delete;
		DeviceContext(DeviceContext &&) = delete;
		DeviceContext &operator=(DeviceContext &&) = delete;

	  private:
		DeviceContext() = default;
		~DeviceContext() = default;

		const Program *mBoundProgram = nullptr;
	};
} // namespace SJH

#endif // __SJH_DEVICE_CONTEXT_H__
