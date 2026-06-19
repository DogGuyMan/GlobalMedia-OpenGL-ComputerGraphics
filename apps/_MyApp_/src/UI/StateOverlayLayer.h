/**
 * @file StateOverlayLayer.h
 * @brief Stage FSM 전환 시 전체화면 이미지를 오버레이하는 ImGui 레이어.
 *
 * @details
 *  ### 책임
 *  - @c Show(tex) / @c Hide() 로 @c IImGuiLayer::Enabled 를 토글해 오버레이 표시/숨김 제어.
 *  - 활성화 시 @p tex 텍스처를 DisplaySize 전체에 UV V축 반전하여 렌더.
 *  - Game kind - Editor 토글(F1) 와 무관하게 항상 렌더 대상 (Enabled 로만 제어).
 *
 *  ### 비-책임
 *  - [X] 표시 시점 결정 - Stage FSM 상태(@c TitleState / @c PauseState / @c GameOverState)가 OnEnter/OnExit 에서 직접 호출.
 *  - [X] 클릭 이벤트 처리 - @c ImGuiWindowFlags_NoInputs 라 입력 통과; FSM 이 @c ImGui::IsMouseClicked 로 직접 감지.
 *  - [X] 텍스처 로드 - @c ResourceRegistry 에서 로드 후 포인터만 주입.
 *
 *  ### 정통 매핑
 *  - Unity UI Canvas (RenderMode=ScreenSpaceOverlay) + 전체화면 Image 컴포넌트.
 *
 * @note [POST-TEST] 풀스크린 정합(무패딩/Retina DisplaySize/이미지 종횡비)은 빌드 후 육안 재검증 대상.
 */
#ifndef __MYAPP_STATE_OVERLAY_LAYER_H__
#define __MYAPP_STATE_OVERLAY_LAYER_H__

#include "UI/IImGuiLayer.h"
#include "texture/texture.h"
#include <cstdint> // intptr_t (ImTextureID 캐스트)
#include <imgui.h>

namespace TopdownShooter::UI
{
	/**
	 * @brief Stage FSM 전환에 연동되는 전체화면 이미지 오버레이 레이어 (Game kind).
	 * @details
	 *  @c Show(tex) / @c Hide() 로 @c IImGuiLayer::Enabled 를 직접 토글한다.
	 *  @c ImGuiLayerStack::RenderAll 은 Enabled=false 인 레이어를 skip.
	 *
	 *  OnBuildUI 내부:
	 *  - DisplaySize 전체를 덮는 투명 배경 창 (@c ImGuiWindowFlags_NoInputs - 입력 통과).
	 *  - @c ImGui::Image UV: uv0=(0,1) / uv1=(1,0) - @c Image::Load stbi flip(true) 로 인한 Y축 반전 보정.
	 *  - StyleVar/StyleColor Push/Pop 3쌍으로 테두리/배경 완전 투명 처리 (ImGui v1.53 워크어라운드).
	 */
	class StateOverlayLayer : public IImGuiLayer
	{
	  public:
		/// @brief 기본 생성자 - Enabled=false(숨김) 초기화. TitleState::OnEnter 가 Show 호출.
		StateOverlayLayer() { Enabled = false; } // 시작 숨김 - TitleState::OnEnter 가 Show

		/// @brief Game kind 반환 - Enabled 토글로만 가시성 제어 (F1 게이트 없음).
		/// @return @c ImGuiLayerKind::Game.
		ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Game; }

		/// @brief 오버레이 표시 - @p tex 텍스처를 전체화면에 렌더하도록 활성화.
		/// @param tex 표시할 텍스처 (Title/Pause/GameOver 이미지). nullptr 이면 빈 창만 뜸.
		void Show(const SJH::Texture *tex)
		{
			mTex    = tex;
			Enabled = true;
		}

		/// @brief 오버레이 숨김 - Enabled=false.
		void Hide() { Enabled = false; }

		/// @brief 전체화면 이미지 창을 빌드 - DisplaySize 크기 투명 창 + UV 반전 Image.
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
				// UV V축 뒤집기 - Image::Load 가 stbi flip(true) 라 ImGui(Y-down) 기준 상하 반전 보정.
				ImGui::Image((ImTextureID)(intptr_t)mTex->GetTextureID(), sz, ImVec2(0, 1), ImVec2(1, 0));
			ImGui::End();
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();
		}

	  private:
		const SJH::Texture *mTex = nullptr;  ///< 현재 표시 중인 텍스처 (비소유 포인터). nullptr 이면 빈 창.
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_STATE_OVERLAY_LAYER_H__
