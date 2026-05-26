/**
 * @file screen_quad_stage.cpp
 * @brief ScreenQuadStage — N 개 FBO color attachment → backbuffer 합성 구현.
 */
#include "render/screen_quad_stage.h"
#include "buffer/framebuffer.h"
#include "object/mesh.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "render/device_context.h"
#include "render/render_target.h"
#include <cstddef>
#include <utility>
#include <vector>

namespace SJH
{
	ScreenQuadStage::ScreenQuadStage(Program &passthrough, Mesh &screenQuad)
	    : mProgram(passthrough), mMesh(screenQuad)
	{
	}

	void ScreenQuadStage::SetSources(std::vector<const Framebuffer *> sources)
	{
		mSources = std::move(sources);
	}

	void ScreenQuadStage::Render(RenderTarget &target)
	{
		if (mSources.empty())
			return;

		auto &rc = DeviceContext::Get();

		// backbuffer 바인딩 + 클리어 (depth test / blend 는 ScreenQuad 특성에 맞게 직접 설정).
		rc.BindTarget(target);
		rc.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		rc.SetDepthTest(false);   // NDC quad — z-buffer 불필요.
		rc.SetBlend(false);       // 첫 소스: replace (전 프레임 백버퍼 잔상 차단).

		rc.UseProgram(mProgram);
		rc.BindVAO(mMesh.GetVAO());

		for (std::size_t i = 0; i < mSources.size(); ++i)
		{
			const Framebuffer *fb = mSources[i];
			if (!fb)
				continue;

			const auto &tex = fb->GetColorAttachment();
			if (!tex)
				continue;

			// sampler 컨벤션: `uScene` (SP4 migrate_demo / Unity _MainTex 정통).
			rc.BindTexture(0, tex->GetTextureID());
			Uniforms::SetInt(mProgram, "uScene", 0);

			// 2+ 소스 — 전 pass 위에 alpha blend 합성.
			if (i == 1)
				rc.SetBlend(true);

			rc.DrawIndexed(mMesh.GetIndexCount());
		}

		// 상태 복원 — 후속 Stage (ImGui 등) 가 blend 를 기대할 수 있으므로.
		rc.SetDepthTest(true);
		rc.SetBlend(true);
	}
} // namespace SJH
