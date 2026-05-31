#include "UI/PostFXDebugLayer.h"
#include "material/material.h"
#include <imgui.h>
#include <utility>
#include <vector>

namespace TopdownShooter::UI
{
	PostFXDebugLayer::PostFXDebugLayer(std::vector<PassDebugEntry> passes, float &gamma)
	    : mPasses(std::move(passes)), mGamma(gamma)
	{
	}

	void PostFXDebugLayer::OnBuildUI()
	{
		// 위치/크기 명시 — ExitButton ((64,64)+48px) 과 겹침 회피.
		// FirstUseEver 라 사용자가 이동하면 그 위치 유지.
		ImGui::SetNextWindowPos(ImVec2(20.0f, 140.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(280.0f, 220.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("PostFX Debug");

		for (auto &entry : mPasses)
		{
			if (!entry.Component)
				continue;

			ImGui::Checkbox(entry.Name.c_str(), &entry.Component->Enabled);

			if (entry.Name == "gamma" && entry.Component->Enabled)
			{
				if (ImGui::SliderFloat("gamma##val", &mGamma, 0.1f, 2.5f))
				{
					if (entry.Component->mMaterial)
						entry.Component->mMaterial->Properties.Floats["gamma"] = mGamma;
				}
			}
		}

		ImGui::End();
	}
} // namespace TopdownShooter::UI
