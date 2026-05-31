#include "UI/PostFXDebugLayer.h"
#include "material/material.h"
#include <imgui.h>
#include <utility>
#include <vector>
#include <vmath.h>

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

			if (!entry.Component->Enabled || !entry.Component->mMaterial)
				continue;

			auto &props = entry.Component->mMaterial->Properties;

			if (entry.Name == "gamma")
			{
				if (ImGui::SliderFloat("gamma##val", &mGamma, 0.1f, 2.5f))
					props.Floats["gamma"] = mGamma;
			}
			else if (entry.Name == "fog")
			{
				ImGui::SliderFloat("density##fog", &props.Floats["uFogDensity"], 0.0f, 0.5f);
				ImGui::SliderFloat("start##fog",   &props.Floats["uFogStart"],   0.0f, 1.0f);
				ImGui::SliderFloat("end##fog",     &props.Floats["uFogEnd"],     0.0f, 1.0f);
				ImGui::ColorEdit3("color##fog",    &props.Vec3s["uFogColor"][0]);
				ImGui::SliderInt("mode##fog",      &props.Ints["uFogMode"], 0, 2); // 0=Linear, 1=Exp, 2=Exp2
			}
			else if (entry.Name == "bloom")
			{
				ImGui::SliderFloat("threshold##bloom", &props.Floats["uBloomThreshold"], 0.0f, 1.5f);
				ImGui::SliderFloat("spread##bloom",    &props.Floats["uBloomSpread"],    0.1f, 5.0f);
				ImGui::SliderFloat("intensity##bloom", &props.Floats["uBloomIntensity"], 0.0f, 4.0f);
			}
			else if (entry.Name == "grayscale_vignetting")
			{
				// Health[0,1] → grayscale 강도 (1=원본색, 0=무채색). 셰이더는 Health 를 모름(uGrayscaleAmount 만).
				ImGui::SliderFloat("Health##gv",    &props.Floats["uGrayscaleAmount"], 0.0f, 1.0f);
				// Vignette — grayscale 과 독립. 색(필수) + 강도.
				ImGui::SliderFloat("vignette##gv",  &props.Floats["uVignetteAmount"],  0.0f, 1.0f);
				ImGui::ColorEdit3 ("vig color##gv", &props.Vec3s["uVignetteColor"][0]);
			}
		}

		ImGui::End();
	}
} // namespace TopdownShooter::UI
