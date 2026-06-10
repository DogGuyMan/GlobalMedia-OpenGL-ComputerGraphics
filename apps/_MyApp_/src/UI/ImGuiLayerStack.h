/**
 * @file ImGuiLayerStack.h
 * @brief 등록된 @c IImGuiLayer 들을 보관하고 한 프레임에 순서대로 렌더하는 레이어 스택.
 *
 * @details
 *  ### 책임
 *  - 레이어 소유권 보관 (@c std::unique_ptr 벡터).
 *  - 등록 순서대로 순회하며 @c OnBuildUI 호출 (먼저 Push 한 레이어가 먼저, 즉 더 아래에 그려짐).
 *  - Enabled 게이트 + Editor kind 게이트(@p showEditor) 로 렌더 여부 판정.
 *
 *  ### 비-책임
 *  - [X] 개별 레이어가 그리는 위젯 - 각 구체 레이어의 @c OnBuildUI 담당.
 *  - [X] 레이어 등록(Push) 시점/순서 결정 - @c UiBootstrap 담당.
 *
 *  ### 정통 매핑
 *  - Unity uGUI sibling index 렌더 순서 / Cocos2D `Layer` z-order 스택.
 */
#ifndef __MYAPP_IMGUI_LAYER_STACK_H__
#define __MYAPP_IMGUI_LAYER_STACK_H__

#include "UI/IImGuiLayer.h"
#include <memory>
#include <utility>
#include <vector>

namespace TopdownShooter::UI
{
	/// @brief @c IImGuiLayer 소유 + 등록 순서대로 렌더하는 레이어 스택.
	class ImGuiLayerStack
	{
	  public:
		/// @brief 레이어를 스택 맨 위에 등록(소유권 이전). 나중에 Push 한 레이어가 더 위에 그려진다.
		/// @param layer 등록할 레이어 (소유권을 가져간다).
		void Push(std::unique_ptr<IImGuiLayer> layer)
		{
			mLayers.push_back(std::move(layer));
		}

		/// @brief 전체 레이어 렌더 (등록 순서대로 @c OnBuildUI 호출).
		/// @details Enabled 가 false 인 레이어와, @p showEditor 가 false 일 때의 Editor kind 레이어는 건너뛴다.
		/// @param showEditor false 이면 Editor kind 레이어는 건너뜀.
		void RenderAll(bool showEditor)
		{
			for (auto &layer : mLayers)
			{
				if (!layer->Enabled)
					continue;
				if (layer->GetKind() == ImGuiLayerKind::Editor && !showEditor)
					continue;
				layer->OnBuildUI();
			}
		}

		/// @brief 등록된 모든 레이어 제거(소유 객체 파괴).
		void Clear() { mLayers.clear(); }

	  private:
		std::vector<std::unique_ptr<IImGuiLayer>> mLayers; ///< 등록 순서 = 렌더 순서인 레이어 소유 컬렉션.
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_IMGUI_LAYER_STACK_H__
