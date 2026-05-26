#include "UI/PostFXDebugLayer.h"
#include "material/material.h"
#include "render/scene_renderer.h"
#include <imgui.h>

namespace TopdownShooter::UI
{
	PostFXDebugLayer::PostFXDebugLayer(SJH::SceneRenderer          &renderer,
	                                   std::vector<SJH::PostFXPass> &passes,
	                                   SJH::Mesh                   *&quadMesh,
	                                   float                        &gamma)
	    : mRenderer(renderer), mPasses(passes), mQuadMesh(quadMesh), mGamma(gamma)
	{
	}

	void PostFXDebugLayer::OnBuildUI()
	{
		ImGui::Begin("PostFX Debug");
		bool chainDirty = false;

		for (auto &pass : mPasses)
		{
			if (ImGui::Checkbox(pass.Name.c_str(), &pass.Enabled))
				chainDirty = true;

			if (pass.Name == "gamma" && pass.Enabled)
			{
				if (ImGui::SliderFloat("gamma##val", &mGamma, 0.1f, 2.5f))
				{
					if (pass.Material)
						pass.Material->Properties.Floats["gamma"] = mGamma;
				}
			}
		}

		if (chainDirty && mQuadMesh)
			mRenderer.SetPostFXChain(mPasses, mQuadMesh);

		ImGui::End();
	}
} // namespace TopdownShooter::UI
