#ifndef __MYAPP_PAUSE_BUTTON_LAYER_H__
#define __MYAPP_PAUSE_BUTTON_LAYER_H__

#include "UI/IImGuiLayer.h"
#include "resource_registry/texture.h"
#include <cstdint> // intptr_t (ImTextureID 캐스트)
#include <functional>
#include <imgui.h>
#include <utility> // std::move

namespace TopdownShooter::UI
{
	/// @brief 좌상단 토글 버튼 — 클릭 시 onClick 콜백 (Stage FSM Pause↔CombatPlay 토글).
	/// @details 구 ExitButtonLayer 기능 전환(2026-06-04). UiBootstrap 에서 push(아래 레이어) — Pause 오버레이 뒤.
	///          일시정지 중 화면 클릭 resume 은 PauseState 가 !WantCaptureMouse 게이트로 처리.
	class PauseButtonLayer : public IImGuiLayer
	{
	  public:
		PauseButtonLayer(std::function<void()> onClick, const SJH::Texture *tex)
		    : mOnClick(std::move(onClick)), mTex(tex)
		{
		}

		ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Game; }

		void OnBuildUI() override
		{
			ImGui::SetNextWindowPos(ImVec2(64.0f, 64.0f), ImGuiCond_Always);
			// v1.53: SetNextWindowBgAlpha / NoBackground 미지원 — PushStyleColor 투명화.
			ImGui::PushStyleColor(ImGuiCol_WindowBg,     ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_Border,       ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
			ImGui::Begin("##pause_btn", nullptr,
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
				{
					if (mOnClick)
						mOnClick();
				}

				ImGui::PopStyleVar();
				ImGui::PopStyleColor(3);
			}
			ImGui::End();
			ImGui::PopStyleColor(3); // WindowBg / Border / BorderShadow
		}

	  private:
		std::function<void()> mOnClick;
		const SJH::Texture   *mTex;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_PAUSE_BUTTON_LAYER_H__
