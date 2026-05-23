/**
 * @file pipeline_state_setter.cpp
 * @brief GL state machine 전환 — 9 helper (Stencil 4 + Depth 3 + Blend 2) + Set + RestoreDefaults.
 */
#include "render/pipeline_state_setter.h"
#include "GL/gl3w.h"

namespace SJH
{
	namespace
	{
		// ─── Stencil 4 결정 단위 (= 4 GL 호출에 1:1 매핑) ─────────────────
		// SP-MaterialSSoT — Pass::PipelineState 안의 Stencil 필드를 *직접* 사용 (Scene::StencilState 의존 제거)

		/// @brief 결정 1 — GL_STENCIL_TEST 토글.
		void SetStencilToggle(bool want, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && last.StencilEnable == want)
				return;
			want ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
			last.StencilEnable = want;
		}

		/// @brief 결정 2 — `glStencilFunc(Func, Ref, ReadMask)` 한 호출의 3 인자 묶음.
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

		/// @brief 결정 3 — `glStencilOp(SFail, DpFail, DpPass)` 한 호출의 3 인자 묶음.
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

		/// @brief 결정 4 — `glStencilMask(WriteMask)` (0x00 = 읽기 전용).
		void SetStencilMask(const Pass::PipelineState &s, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && last.StencilWriteMask == s.StencilWriteMask)
				return;
			glStencilMask(s.StencilWriteMask);
			last.StencilWriteMask = s.StencilWriteMask;
		}

		/// @brief Stencil state 일괄 적용 — 결정 순서만 표현.
		void ApplyStencil(const Pass::PipelineState &s, Pass::PipelineState &last, bool initialized)
		{
			SetStencilToggle(s.StencilEnable, last, initialized);
			// disabled -> sub-state GL 호출 무의미 (early return)
			if (!s.StencilEnable)
				return;

			SetStencilFunc(s, last, initialized);
			SetStencilOp(s, last, initialized);
			SetStencilMask(s, last, initialized);
		}

		// ─── Depth 3 결정 단위 (= 3 GL 호출에 1:1 매핑) ───────────────────
		/// @brief 결정 1 — GL_DEPTH_TEST 토글.
		void SetDepthToggle(bool test, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && test == last.DepthTest)
				return;
			test ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
			last.DepthTest = test;
		}

		/// @brief 결정 2 — `glDepthMask` (depth buffer 쓰기 마스크). Transparent/Skybox=false 의 핵심.
		void SetDepthMask(bool write, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && write == last.DepthWrite)
				return;
			glDepthMask(write ? GL_TRUE : GL_FALSE);
			last.DepthWrite = write;
		}

		/// @brief 결정 3 — `glDepthFunc` (Skybox 의 GL_LEQUAL / X-Ray outline 의 GL_GREATER 등 자동 전환).
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

		// ─── Cull (rasterizer 단계) ───────────────────────────────────────
		/// @brief Pass 기반 cull state 자동 전환 — Skybox 의 GL_FRONT / 기본 GL_BACK.
		/// @details `cullMode == 0` 이면 face culling disable (CullMode 의 sentinel).
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

		// ─── Blend 2 결정 단위 (= 2 GL 호출에 1:1 매핑) ───────────────────
		/// @brief 결정 1 — GL_BLEND 토글.
		void SetBlendToggle(bool enable, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && last.BlendEnable == enable)
				return;
			enable ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
			last.BlendEnable = enable;
		}

		/// @brief 결정 2 — `glBlendFunc(src, dst)` (BlendEnable=false 면 호출 의미 없음).
		void SetBlendFunc(GLenum src, GLenum dst, Pass::PipelineState &last, bool initialized)
		{
			if (initialized && last.BlendSrc == src && last.BlendDst == dst)
				return;
			glBlendFunc(src, dst);
			last.BlendSrc = src;
			last.BlendDst = dst;
		}

		/// @brief Pass 기반 blend state 일괄 적용.
		void ApplyBlend(const Pass::PipelineState &want, Pass::PipelineState &last, bool initialized)
		{
			SetBlendToggle(want.BlendEnable, last, initialized);
			// disabled -> BlendFunc 호출 의미 없음 (GL 상태는 남지만 unused)
			if (want.BlendEnable)
				SetBlendFunc(want.BlendSrc, want.BlendDst, last, initialized);
		}
	} // anonymous namespace

	void PipelineStateSetter::Set(const Pass::PipelineState &want)
	{
		ApplyStencil(want, mLast, mInitialized);
		ApplyDepth(want, mLast, mInitialized);
		ApplyCull(want.CullMode, mLast, mInitialized);
		ApplyBlend(want, mLast, mInitialized);
		mInitialized = true;
	}

	void PipelineStateSetter::RestoreDefaults()
	{
		// 다음 패스/단계가 표준 opaque 가정하도록 복원
		// (stencil off / depth on+write on+func LESS / cull back / blend off).
		if (mLast.StencilEnable)
			glDisable(GL_STENCIL_TEST);
		glStencilMask(0xFFu);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS); // Skybox 가 LEQUAL 로 바꾼 상태 복원.
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK); // Skybox 가 FRONT 로 바꾼 상태 복원.
		glDisable(GL_BLEND); // Pass 가 blend on 한 상태 복원.

		// 캐시 무효화 — 다음 Set 호출이 first-call 처럼 강제 적용.
		mInitialized = false;
	}
} // namespace SJH
