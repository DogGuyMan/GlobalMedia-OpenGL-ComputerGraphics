/**
 * @file PostFXDebugLayer.cpp
 * @brief PostFXDebugLayer 구현 - 패스별 ImGui 위젯 빌드.
 *
 * @details
 *  ### 구현 요점
 *  - 생성자: std::move 로 passes 복사 비용 제거, gamma 참조 직결.
 *  - OnBuildUI: mPasses 순회 -> Component nullptr/Enabled/mMaterial 3단계 가드
 *    -> entry.Name 분기로 전용 위젯 빌드.
 *  - gamma 패스: SliderFloat 변경 시 @c mGamma 와 @c props.Floats["gamma"] 동시 갱신
 *    (셰이더 uniform 은 draw 시점 mesh_pass 가 props 에서 읽어 UBO 멤버로 전송).
 *  - fog 패스: uFogMode 0=Linear / 1=Exp / 2=Exp2.
 */
#include "UI/PostFXDebugLayer.h"
#include "material/material.h"
#include "Playable/Constants.h"   // PASS_* 패스명
#include <imgui.h>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

namespace TopdownShooter::UI
{
	PostFXDebugLayer::PostFXDebugLayer(std::vector<PassDebugEntry> passes, float &gamma)
	    : mPasses(std::move(passes)), mGamma(gamma)
	{
	}

	void PostFXDebugLayer::OnBuildUI()
	{
		// 위치/크기 명시 - ExitButton ((64,64)+48px) 과 겹침 회피.
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

			if (entry.Name == Playable::PASS_GAMMA)
			{
				if (ImGui::SliderFloat("gamma##val", &mGamma, 0.1f, 2.5f))
					props.Floats["gamma"] = mGamma;
			}
			else if (entry.Name == Playable::PASS_FOG)
			{
				ImGui::SliderFloat("density##fog", &props.Floats["uFogDensity"], 0.0f, 0.5f);
				ImGui::SliderFloat("start##fog",   &props.Floats["uFogStart"],   0.0f, 1.0f);
				ImGui::SliderFloat("end##fog",     &props.Floats["uFogEnd"],     0.0f, 1.0f);
				ImGui::ColorEdit3("color##fog",    &props.Vec3s["uFogColor"][0]);
				ImGui::SliderInt("mode##fog",      &props.Ints["uFogMode"], 0, 2); // 0=Linear, 1=Exp, 2=Exp2
			}
			else if (entry.Name == Playable::PASS_BLOOM)
			{
				ImGui::SliderFloat("threshold##bloom", &props.Floats["uBloomThreshold"], 0.0f, 1.5f);
				ImGui::SliderFloat("spread##bloom",    &props.Floats["uBloomSpread"],    0.1f, 5.0f);
				ImGui::SliderFloat("intensity##bloom", &props.Floats["uBloomIntensity"], 0.0f, 4.0f);
			}
			else if (entry.Name == Playable::PASS_GRAYSCALE_VIGNETTING)
			{
				// Health[0,1] -> grayscale 강도 (1=원본색, 0=무채색). 셰이더는 Health 를 모름(uGrayscaleAmount 만).
				ImGui::SliderFloat("Health##gv",    &props.Floats["uGrayscaleAmount"], 0.0f, 1.0f);
				// Vignette - grayscale 과 독립. 색(필수) + 강도.
				ImGui::SliderFloat("vignette##gv",  &props.Floats["uVignetteAmount"],  0.0f, 1.0f);
				ImGui::ColorEdit3 ("vig color##gv", &props.Vec3s["uVignetteColor"][0]);
			}
		}

		ImGui::End();
	}
} // namespace TopdownShooter::UI
