#ifndef __MYAPP_IMGUI_LAYER_STACK_H__
#define __MYAPP_IMGUI_LAYER_STACK_H__

#include "UI/IImGuiLayer.h"
#include <memory>
#include <utility>
#include <vector>

namespace TopdownShooter::UI
{
	class ImGuiLayerStack
	{
	  public:
		void Push(std::unique_ptr<IImGuiLayer> layer)
		{
			mLayers.push_back(std::move(layer));
		}

		/// @brief 전체 레이어 렌더.
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

		void Clear() { mLayers.clear(); }

	  private:
		std::vector<std::unique_ptr<IImGuiLayer>> mLayers;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_IMGUI_LAYER_STACK_H__
