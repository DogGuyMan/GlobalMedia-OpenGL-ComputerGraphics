#ifndef __MYAPP_EXIT_BUTTON_LAYER_H__
#define __MYAPP_EXIT_BUTTON_LAYER_H__

#include "UI/IImGuiLayer.h"
#include <GLFW/glfw3.h>
#include <imgui.h>

namespace SJH
{
class Texture;
}

namespace TopdownShooter::UI
{
	class ExitButtonLayer : public IImGuiLayer
	{
	  public:
		ExitButtonLayer(GLFWwindow *win, const SJH::Texture *tex)
		    : mWindow(win), mTex(tex)
		{
		}

		ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Game; }

		void OnBuildUI() override
		{
			ImGui::SetNextWindowPos(ImVec2(64.0f, 64.0f), ImGuiCond_Always);
			// v1.53: SetNextWindowBgAlpha / NoBackground 미지원 — PushStyleColor 투명화.
			// 3종 Push 는 Begin() 이전, Pop 은 End() 이후 (CheckStacksSize 규칙).
			ImGui::PushStyleColor(ImGuiCol_WindowBg,     ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_Border,       ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
			ImGui::Begin("##exit_btn", nullptr,
			             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize |
			                 ImGuiWindowFlags_NoMove);

			if (mTex)
			{
				ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.25f));
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

				if (ImGui::ImageButton(
				        (ImTextureID)(intptr_t)mTex->GetTextureID(),
				        ImVec2(48.0f, 48.0f)))
					glfwSetWindowShouldClose(mWindow, 1);

				ImGui::PopStyleVar();
				ImGui::PopStyleColor(3);
			}
			ImGui::End();
			ImGui::PopStyleColor(3); // WindowBg / Border / BorderShadow
		}

	  private:
		GLFWwindow         *mWindow;
		const SJH::Texture *mTex;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_EXIT_BUTTON_LAYER_H__
