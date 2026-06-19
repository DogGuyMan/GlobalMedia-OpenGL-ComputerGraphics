/**
 * @file device_context.h
 * @brief GL 파이프라인 상태 facade - 바운드 상태 추적 + draw 발행 + RT 바인딩 싱글톤.
 *
 * @details
 *  ### 책임
 *  - 현재 bound Program 추적 (@c mBoundProgram).
 *  - GL 상태 변경 단일 진입점 (@c glUseProgram / @c glBindVertexArray / @c glBindTexture /
 *    @c glClear / @c glEnable).
 *  - Draw 명령 발행 (@c glDrawElements / @c glDrawArrays).
 *  - RenderTarget 바인딩 위임 (@c BindTarget / @c BeginFrame).
 *  - 멀티패스 진입 지점 - @c BeginFrame 이 패스마다 다른 target 을 받아 FBO 전환.
 *
 *  ### 비-책임 (SP-RenderFacadeBoundary)
 *  - [X] Shader uniform 상태 - @c SJH::Uniforms (자유 함수, @c program_uniforms.h) 담당.
 *    @c glUniform* 호출은 *의도적으로* 본 facade 를 우회 (OCP + 캐시 로직 가시성 보존).
 *  - [X] Uniform location 조회 - @c Program::GetLocation 직접 사용.
 *  - [X] Material/Pass GL state 전환 - @c PipelineStateSetter 담당.
 *
 *  ### 호출자 가이드
 *  | 의도                  | 경로              | 예                                     |
 *  |-----------------------|-------------------|----------------------------------------|
 *  | Program 활성화        | DeviceContext     | `rc.UseProgram(*prog)`                 |
 *  | VAO/Tex/RT/draw       | DeviceContext     | `rc.BindVAO(...)`, `rc.DrawIndexed(...)` |
 *  | Uniform 값 설정       | Uniforms 자유 함수 | `Uniforms::SetMat4(*prog, "uModel", m)` |
 *  | Uniform location 조회 | Program 직접      | `prog->GetLocation("uModel")`          |
 *
 *  ### 순서 계약 (POLA 준수 - 코드 강제 없음, 문서 명시)
 *  @c UseProgram(prog) 호출 후에만 해당 @p prog 에 @c Uniforms::Set* 호출 유효.
 *
 * @note Meyer's 싱글톤 @c DeviceContext::Get() - GL context 가 활성 상태일 때만 호출 유효.
 */
#ifndef __SJH_DEVICE_CONTEXT_H__
#define __SJH_DEVICE_CONTEXT_H__

#include "GL/gl3w.h"
#include "program/program.h"
#include "buffer/render_target.h"
#include <memory>

namespace SJH
{

	/**
	 * @brief GL 파이프라인 상태 facade - bound Program / VAO / RT 추적 + draw 발행 싱글톤.
	 * @details
	 *  본 클래스는 *pipeline state* 의 단일 진입점 (facade). Shader uniform 상태는 본 facade 밖.
	 *
	 *  - @c mBoundProgram: 현재 @c glUseProgram 으로 활성화된 Program 포인터.
	 *  - Uniform 상태는 @c SJH::Uniforms 자유 함수(@c program_uniforms.h) 가 담당 -
	 *    새 uniform 타입 추가 시 본 클래스 헤더 변동 0 (OCP).
	 *  - @c SJH::Program 은 location 캐시 + lifetime 만 책임 (SRP).
	 *
	 *  복사/이동 금지 - Meyer's 싱글톤. GL context 활성 상태에서만 @c Get() 호출 유효.
	 */
	class DeviceContext
	{
	  public:
		/// @brief Meyer's 싱글톤 접근. GL context 가 활성 상태일 때만 호출 유효.
		static DeviceContext &Get();

		// -- bound 상태 변경 --------------------------------------------------

		/// @brief @p prog 를 활성 Program 으로 설정. @c glUseProgram 래퍼.
		/// @param prog 활성화할 Program 레퍼런스.
		void UseProgram(const Program &prog);

		/// @brief @p vao 를 활성 VAO 로 바인딩. @c glBindVertexArray 래퍼.
		/// @param vao 바인딩할 VAO ID.
		void BindVAO(GLuint vao);

		/// @brief 텍스처 유닛 @p unit 에 텍스처 @p tex 를 바인딩.
		/// @details @c glActiveTexture(GL_TEXTURE0 + unit) + @c glBindTexture(GL_TEXTURE_2D, tex).
		/// @param unit 텍스처 유닛 인덱스 (0부터).
		/// @param tex  GL 텍스처 오브젝트 ID.
		void BindTexture(GLuint unit, GLuint tex);

		/// @brief @p target 의 FBO 를 바인딩 - @c RenderTarget::Bind() 위임.
		/// @param target 바인딩할 RenderTarget.
		void BindTarget(RenderTarget &target);

		/// @brief 현재 바인딩된 FBO 를 @p mask 에 따라 클리어. @c glClear 래퍼.
		/// @param mask @c GL_COLOR_BUFFER_BIT / @c GL_DEPTH_BUFFER_BIT / @c GL_STENCIL_BUFFER_BIT 조합.
		void Clear(GLbitfield mask);

		/// @brief Depth test 활성/비활성 + Depth 함수 설정.
		/// @param enabled true 면 @c glEnable(GL_DEPTH_TEST), false 면 @c glDisable.
		/// @param func    Depth 비교 함수 (기본 @c GL_LESS).
		void SetDepthTest(bool enabled, GLenum func = GL_LESS);

		/// @brief Alpha blend 활성/비활성 + BlendFunc 설정.
		/// @param enabled   true 면 @c glEnable(GL_BLEND), false 면 @c glDisable.
		/// @param srcFactor 소스 팩터 (기본 @c GL_SRC_ALPHA).
		/// @param dstFactor 대상 팩터 (기본 @c GL_ONE_MINUS_SRC_ALPHA).
		void SetBlend(bool enabled, GLenum srcFactor = GL_SRC_ALPHA,
		              GLenum dstFactor = GL_ONE_MINUS_SRC_ALPHA);

		// -- Draw 명령 --------------------------------------------------------

		/// @brief 인덱스 기반 삼각형 드로우. @c glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0).
		/// @param count 인덱스 개수.
		void DrawIndexed(GLsizei count);

		/// @brief 배열 기반 드로우. @c glDrawArrays(mode, 0, count).
		/// @param mode  GL primitive 모드 (@c GL_TRIANGLES 등).
		/// @param count 버텍스 개수.
		void DrawArrays(GLenum mode, GLsizei count);

		// -- 유틸리티 ---------------------------------------------------------

		/// @brief 패스 시작 alias - BindTarget + Clear(color|depth|stencil) + SetDepthTest(true) + SetBlend(true).
		/// @details Stencil 도 함께 clear (@c DEPTH24_STENCIL8 포맷 가정).
		///          Stencil / 커스텀 상태가 필요하면 primitive 메서드(@c BindTarget / @c Clear 등)를 직접 조합.
		/// @param target 이번 패스의 출력 RenderTarget.
		void BeginFrame(RenderTarget &target);

		// SP-RTOwnership - DefaultRenderTarget(window backbuffer) lifetime owner 는 Application.
		// Framebuffer 는 ResourceRegistry::CreateFramebuffer 로 위탁.

		// -- 싱글톤 - 복사/이동 금지 ------------------------------------------
		DeviceContext(const DeviceContext &) = delete;
		DeviceContext &operator=(const DeviceContext &) = delete;
		DeviceContext(DeviceContext &&) = delete;
		DeviceContext &operator=(DeviceContext &&) = delete;

	  private:
		DeviceContext() = default;
		~DeviceContext() = default;

		const Program *mBoundProgram = nullptr;  ///< 현재 glUseProgram 으로 활성화된 Program.
	};
} // namespace SJH

#endif // __SJH_DEVICE_CONTEXT_H__
