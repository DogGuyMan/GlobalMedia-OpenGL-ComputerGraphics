/**
 * @file PauseButtonLayer.h
 * @brief 좌상단에 일시정지 토글 버튼(이미지 버튼)을 그리는 Game kind 레이어.
 *
 * @details
 *  ### 책임
 *  - 화면 좌상단(64,64)에 48px 투명 배경 이미지 버튼 1개를 그린다.
 *  - 클릭 시 생성자에서 받은 @c onClick 콜백 호출 (main: Stage FSM Pause/CombatPlay 토글).
 *
 *  ### 비-책임
 *  - [X] 일시정지 상태 보유/전이 - Stage FSM(PauseState) 담당. 본 레이어는 콜백만 발사.
 *  - [X] 일시정지 중 화면 클릭 resume - PauseState 가 @c !WantCaptureMouse 게이트로 처리.
 *
 *  ### 정통 매핑
 *  - HUD 단일 버튼 위젯 - 게임 일시정지 메뉴 진입 버튼.
 *
 * @note 구 ExitButtonLayer 에서 기능 전환(2026-06-04). UiBootstrap 에서 먼저 Push 되어
 *       Pause 오버레이보다 아래에 그려진다. ImGui v1.53 핀이라 투명 배경은 PushStyleColor 워크어라운드.
 */
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
	/// @brief 좌상단 토글 버튼 - 클릭 시 onClick 콜백 (Stage FSM Pause<->CombatPlay 토글).
	/// @details 구 ExitButtonLayer 기능 전환(2026-06-04). UiBootstrap 에서 push(아래 레이어) - Pause 오버레이 뒤.
	///          일시정지 중 화면 클릭 resume 은 PauseState 가 !WantCaptureMouse 게이트로 처리.
	class PauseButtonLayer : public IImGuiLayer
	{
	  public:
		/// @brief 클릭 콜백과 버튼 텍스처를 주입.
		/// @param onClick 버튼 클릭 시 호출할 콜백 (예: main 의 TogglePause). null 이면 클릭 무시.
		/// @param tex     버튼에 그릴 텍스처 (비소유 - lifetime caller-owned). null 이면 버튼 미표시.
		PauseButtonLayer(std::function<void()> onClick, const SJH::Texture *tex)
		    : mOnClick(std::move(onClick)), mTex(tex)
		{
		}

		/// @brief 항상 표시되는 HUD 요소이므로 Game kind.
		/// @return @c ImGuiLayerKind::Game.
		ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Game; }

		/// @brief 좌상단 투명 윈도우에 48px 이미지 버튼을 그리고, 클릭 시 @c mOnClick 발사.
		void OnBuildUI() override
		{
			ImGui::SetNextWindowPos(ImVec2(64.0f, 64.0f), ImGuiCond_Always);
			// v1.53: SetNextWindowBgAlpha / NoBackground 미지원 - PushStyleColor 투명화.
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
		std::function<void()> mOnClick;       ///< 버튼 클릭 콜백 (main 의 Pause 토글).
		const SJH::Texture   *mTex;           ///< 버튼 텍스처 (비소유).
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_PAUSE_BUTTON_LAYER_H__
