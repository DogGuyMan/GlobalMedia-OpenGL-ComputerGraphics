/**
 * @file device_context.cpp
 * @brief DeviceContext 구현 - GL 상태 래퍼 + 컴파일 타임 싱글톤 의미론 검증.
 *
 * @details
 *  ### 구현 노트
 *  - @c Get() : Meyer's 싱글톤 (C++11 static local - thread-safe 초기화).
 *  - @c BeginFrame : @c BindTarget -> (depth/stencil write mask 복원) -> @c Clear(color|depth|stencil) ->
 *    @c InvalidateStateCache -> @c ApplyPipelineState(Opaque baseline) 순 (D-RS-1/D-RS-5).
 *    Stencil 도 함께 clear (@c DEPTH24_STENCIL8 포맷 가정 - Stencil 미사용 패스는 추가 비용 미미).
 *  - @c ApplyPipelineState : 구 @c PipelineStateSetter 의 Stencil4/Depth3/Cull1/Blend2 dirty-apply 흡수
 *    (anonymous namespace 헬퍼). @c mLast 단일 캐시 + @c mStateInitialized first-call 강제.
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
	namespace
	{
		// ===================================================================================
		//  GL state machine dirty-apply 헬퍼 (D-RS-1 - 구 PipelineStateSetter 이식)
		//  결정 단위 (Stencil 4 + Depth 3 + Cull 1 + Blend 2) = GL 호출에 1:1 매핑.
		//  각 헬퍼는 initialized + last 비교로 dirty check - 변경된 state 만 GL 호출.
		// ===================================================================================

		/// @brief Stencil 결정 1 - GL_STENCIL_TEST 토글.
		void SetStencilToggle(bool want, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && last.StencilEnable == want)
				return;
			want ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
			last.StencilEnable = want;
		}

		/// @brief Stencil 결정 2 - glStencilFunc(Func, Ref, ReadMask) 3 인자 묶음.
		void SetStencilFunc(const Pass::PipelineState &s, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && last.StencilFunc == s.StencilFunc
			    && last.StencilRef == s.StencilRef && last.StencilReadMask == s.StencilReadMask)
				return;
			glStencilFunc(s.StencilFunc, s.StencilRef, s.StencilReadMask);
			last.StencilFunc = s.StencilFunc;
			last.StencilRef = s.StencilRef;
			last.StencilReadMask = s.StencilReadMask;
		}

		/// @brief Stencil 결정 3 - glStencilOp(SFail, DpFail, DpPass) 3 인자 묶음.
		void SetStencilOp(const Pass::PipelineState &s, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && last.StencilOpSFail == s.StencilOpSFail
			    && last.StencilOpDPFail == s.StencilOpDPFail && last.StencilOpDPPass == s.StencilOpDPPass)
				return;
			glStencilOp(s.StencilOpSFail, s.StencilOpDPFail, s.StencilOpDPPass);
			last.StencilOpSFail = s.StencilOpSFail;
			last.StencilOpDPFail = s.StencilOpDPFail;
			last.StencilOpDPPass = s.StencilOpDPPass;
		}

		/// @brief Stencil 결정 4 - glStencilMask(WriteMask) (0x00 = 읽기 전용).
		void SetStencilMask(const Pass::PipelineState &s, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && last.StencilWriteMask == s.StencilWriteMask)
				return;
			glStencilMask(s.StencilWriteMask);
			last.StencilWriteMask = s.StencilWriteMask;
		}

		/// @brief Stencil state 일괄 적용 - disabled 시 sub-state GL 호출 무의미 (early return).
		void ApplyStencil(const Pass::PipelineState &s, Pass::PipelineState &last, bool initialized)
		{
			SetStencilToggle(s.StencilEnable, last, initialized);
			if (!s.StencilEnable)
				return;
			SetStencilFunc(s, last, initialized);
			SetStencilOp(s, last, initialized);
			SetStencilMask(s, last, initialized);
		}

		/// @brief Depth 결정 1 - GL_DEPTH_TEST 토글.
		void SetDepthToggle(bool test, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && test == last.DepthTest)
				return;
			test ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
			last.DepthTest = test;
		}

		/// @brief Depth 결정 2 - glDepthMask (Transparent/Skybox=false 의 핵심).
		void SetDepthMask(bool write, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && write == last.DepthWrite)
				return;
			glDepthMask(write ? GL_TRUE : GL_FALSE);
			last.DepthWrite = write;
		}

		/// @brief Depth 결정 3 - glDepthFunc (Skybox LEQUAL / X-Ray GREATER 자동 전환).
		void SetDepthFunc(GLenum func, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && func == last.DepthFunc)
				return;
			glDepthFunc(func);
			last.DepthFunc = func;
		}

		/// @brief Depth state 일괄 적용.
		void ApplyDepth(const Pass::PipelineState &want, Pass::PipelineState &last, bool initialized)
		{
			SetDepthToggle(want.DepthTest, last, initialized);
			SetDepthMask(want.DepthWrite, last, initialized);
			SetDepthFunc(want.DepthFunc, last, initialized);
		}

		/// @brief Cull state 적용 - cullMode == 0 이면 face culling disable (sentinel).
		void ApplyCull(GLenum cullMode, Pass::PipelineState &last, bool initialized)
		{
			const bool wantOn = (cullMode != 0);
			const bool lastOn = (last.CullMode != 0);
			if (!initialized || wantOn != lastOn)
			{
				wantOn ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
			}
			if (wantOn && (!initialized || cullMode != last.CullMode))
			{
				glCullFace(cullMode);
			}
			last.CullMode = cullMode;
		}

		/// @brief Blend 결정 1 - GL_BLEND 토글.
		void SetBlendToggle(bool enable, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && last.BlendEnable == enable)
				return;
			enable ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
			last.BlendEnable = enable;
		}

		/// @brief Blend 결정 2 - glBlendFunc(src, dst) (BlendEnable=false 면 호출 생략).
		void SetBlendFunc(GLenum src, GLenum dst, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && last.BlendSrc == src && last.BlendDst == dst)
				return;
			glBlendFunc(src, dst);
			last.BlendSrc = src;
			last.BlendDst = dst;
		}

		/// @brief Blend state 일괄 적용.
		/// @details ★ blend func 강제 함정: GL 의 blendFunc 기본값은 (GL_ONE, GL_ZERO) 인데 캐시
		///          @c mLast.BlendSrc/Dst 기본값은 (SRC_ALPHA, ONE_MINUS_SRC_ALPHA) 라 *불일치*.
		///          off->on 전이(또는 invalidate 후 첫 enable)에서 func 을 *무조건* 적용하지 않으면
		///          dirty-check 가 "이미 일치"로 판단해 @c glBlendFunc 을 영영 skip -> GL 은 (ONE,ZERO)
		///          잔재로 알파 블렌딩이 replace 처럼 동작(알파 무시). 구 코드는 BeginFrame 의 명시
		///          glBlendFunc 호출이 이를 마스킹했으나 D-RS-5(BeginFrame=Opaque baseline)로 사라짐.
		void ApplyBlend(const Pass::PipelineState &want, Pass::PipelineState &last, bool initialized)
		{
			// 직전에 *확실히* blend 가 켜져 있었는가 (캐시가 유효하고 BlendEnable=true).
			const bool prevEnabled = initialized && last.BlendEnable;
			SetBlendToggle(want.BlendEnable, last, initialized);
			if (!want.BlendEnable)
				return;
			if (!prevEnabled)
			{
				// off->on 전이 / invalidate 후 첫 enable - GL func 상태 미확정이므로 무조건 적용.
				glBlendFunc(want.BlendSrc, want.BlendDst);
				last.BlendSrc = want.BlendSrc;
				last.BlendDst = want.BlendDst;
			}
			else
			{
				// 연속 blend - func 이 바뀐 경우만 (dirty-check).
				SetBlendFunc(want.BlendSrc, want.BlendDst, last, initialized);
			}
		}
	} // anonymous namespace

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

	void DeviceContext::ApplyPipelineState(const Pass::PipelineState &want)
	{
		ApplyStencil(want, mLast, mStateInitialized);
		ApplyDepth(want, mLast, mStateInitialized);
		ApplyCull(want.CullMode, mLast, mStateInitialized);
		ApplyBlend(want, mLast, mStateInitialized);
		mStateInitialized = true;
	}

	void DeviceContext::InvalidateStateCache()
	{
		// 캐시 무효화 - 다음 ApplyPipelineState 가 first-call 처럼 전체 강제 적용.
		// foreign GL 소비자(Effekseer/Box2D/ImGui)가 GL state 를 캐시 뒤에서 바꾼 후 호출.
		mStateInitialized = false;
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
		// depth/stencil 버퍼를 비우려면 write mask 가 열려 있어야 한다 - 직전 패스(Transparent/Outline)가
		// glDepthMask(false)/glStencilMask(0x00) 로 닫았을 수 있으므로 clear 전 명시 복원 (구 RestoreDefaults 역할).
		// Stencil 도 함께 clear - Framebuffer 가 DEPTH24_STENCIL8 라 함께 사용 가정.
		glDepthMask(GL_TRUE);
		glStencilMask(0xFFu);
		glClearStencil(0);
		Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		// 위 raw mask 변경으로 캐시가 GL 과 어긋났을 수 있으니 무효화 후 Opaque baseline 을 단일 권위로 적용.
		// (D-RS-5 - 구 SetDepthTest(true)+SetBlend(true) 명령형 토글 대체. 각 draw 가 자기 PipelineState override.)
		InvalidateStateCache();
		ApplyPipelineState(Pass::DefaultPipelineStateOf(Pass::Kind::Opaque));
	}

} // namespace SJH
