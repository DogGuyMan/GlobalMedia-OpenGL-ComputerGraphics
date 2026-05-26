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
