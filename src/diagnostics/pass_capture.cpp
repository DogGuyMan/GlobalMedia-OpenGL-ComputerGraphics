/**
 * @file pass_capture.cpp
 * @brief pass_capture 구현 - 변형별 상태 저장/설정/Execute/readback/복원.
 * @note diagnostics = cycle-exempt: render(PassIterator/WorldPass)+buffer 상향 링크.
 */
#include "diagnostics/pass_capture.h"
#include "diagnostics/frame_capture.h"

#include "render/pass_iterator.h"
#include "render/render_passable/render_passable.h"       // IPassable::Enabled
#include "render/render_passable/render_passable.impls.h" // WorldPass::SetQueueFilter

#include <spdlog/spdlog.h>

namespace SJH::Diagnostics
{
	void RunCaptureVariants(PassIterator &it, DeviceContext &rec, RenderTarget &backbuffer,
	                        RenderTarget *worldFbo, WorldPass *worldPass,
	                        const std::vector<CaptureVariant> &variants,
	                        const std::string &outDir, int w, int h)
	{
		for (const auto &v : variants)
		{
			if (v.target == CaptureVariant::WorldFbo && worldFbo == nullptr)
			{
				spdlog::warn("[PassCapture] '{}' WorldFbo 대상인데 worldFbo=null - skip.", v.outName);
				continue;
			}

			// --- 상태 저장 ---
			std::vector<std::pair<IPassable *, bool>> saved; // {pass, 이전 Enabled}
			for (const auto &[key, want] : v.passOverride)
			{
				IPassable *p = it.Find(key);
				if (p)
				{
					saved.emplace_back(p, p->Enabled);
					p->Enabled = want;
				}
			}
			const int savedDebug = it.DebugPassIndex;
			it.DebugPassIndex = v.stopAtPass;
			const bool useQueue =
			    worldPass && (v.worldQueueMin != INT_MIN || v.worldQueueMax != INT_MAX);
			if (useQueue)
				worldPass->SetQueueFilter(v.worldQueueMin, v.worldQueueMax);

			// --- 렌더 + 캡처 ---
			it.Execute(rec, backbuffer);
			const std::string path = outDir + "/" + v.outName + ".png";
			const bool        ok   = (v.target == CaptureVariant::WorldFbo)
			                             ? CaptureTargetToPng(*worldFbo, path, w, h)
			                             : CaptureBackbufferToPng(path, w, h);
			spdlog::info("[PassCapture] {} {}", v.outName, ok ? "OK" : "FAIL");

			// --- 복원 ---
			if (useQueue)
				worldPass->SetQueueFilter(INT_MIN, INT_MAX);
			it.DebugPassIndex = savedDebug;
			for (auto &[p, prev] : saved)
				p->Enabled = prev;
		}
	}
} // namespace SJH::Diagnostics
