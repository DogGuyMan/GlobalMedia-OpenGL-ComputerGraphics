#ifndef __MYAPP_IMGUI_LAYER_H__
#define __MYAPP_IMGUI_LAYER_H__

namespace TopdownShooter::UI
{
	enum class ImGuiLayerKind
	{
		Game,   ///< 항상 렌더 (HUD, 버튼 등)
		Editor, ///< mShowEditor == true 일 때만 렌더 (디버그 창)
	};

	class IImGuiLayer
	{
	  public:
		virtual ~IImGuiLayer()                 = default;
		virtual void           OnBuildUI()     = 0;
		virtual ImGuiLayerKind GetKind() const = 0;
		bool                   Enabled         = true;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_IMGUI_LAYER_H__
