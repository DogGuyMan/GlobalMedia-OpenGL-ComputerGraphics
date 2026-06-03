#ifndef __MYAPP_STATE_OVERLAY_LAYER_H__
#define __MYAPP_STATE_OVERLAY_LAYER_H__

#include "UI/IImGuiLayer.h"
#include "resource_registry/texture.h"
#include <cstdint> // intptr_t (ImTextureID 캐스트)
#include <imgui.h>

namespace TopdownShooter::UI
{
	/// @brief Stage State 전체화면 오버레이 (Title / Pause / GameOver 이미지).
	/// @details Show(tex)/Hide() 로 base Enabled 토글 — ImGuiLayerStack::RenderAll 가 skip 처리.
	///          PostFx 이후 최상위 렌더. NoInputs 라 클릭은 FSM 이 ImGui::IsMouseClicked 로 직접 감지.
	/// @note    [POST-TEST] 풀스크린 정합(무패딩/Retina DisplaySize/이미지 종횡비)은 빌드 후 육안 재검증 대상.
	class StateOverlayLayer : public IImGuiLayer
	{
	  public:
		StateOverlayLayer() { Enabled = false; } // 시작 숨김 — TitleState::OnEnter 가 Show

		ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Game; }

		void Show(const SJH::Texture *tex)
		{
			mTex    = tex;
			Enabled = true;
		}
		void Hide() { Enabled = false; }

		void OnBuildUI() override
		{
			const ImVec2 sz = ImGui::GetIO().DisplaySize;
			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
			ImGui::SetNextWindowSize(sz, ImGuiCond_Always);
			// v1.53: 투명 배경 워크어라운드(PauseButtonLayer 미러) + 풀스크린 무패딩.
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_WindowBg,     ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_Border,       ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
			ImGui::Begin("##state_overlay", nullptr,
			             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
			                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs);
			if (mTex)
				// UV V축 뒤집기 — Image::Load 가 stbi flip(true) 라 ImGui(Y-down) 기준 상하 반전 보정.
				ImGui::Image((ImTextureID)(intptr_t)mTex->GetTextureID(), sz, ImVec2(0, 1), ImVec2(1, 0));
			ImGui::End();
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();
		}

	  private:
		const SJH::Texture *mTex = nullptr;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_STATE_OVERLAY_LAYER_H__
