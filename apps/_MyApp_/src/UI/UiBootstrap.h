#ifndef __MYAPP_UI_BOOTSTRAP_H__
#define __MYAPP_UI_BOOTSTRAP_H__

#include "UI/PostFXDebugLayer.h" // PassDebugEntry (값 멤버라 완전 타입 필요)
#include <functional>
#include <vector>

// fwd — 포인터만 노출.
struct GLFWwindow;
struct ImGuiContext;
namespace SJH
{
	class ResourceRegistry;
}

namespace TopdownShooter::UI
{
	class ImGuiLayerStack;

	/// @brief BuildGameUI 입력 의존. debugEntries 는 main(Composition Root)이 render↔UI 매핑해 주입.
	struct GameUiDeps
	{
		GLFWwindow                 *window = nullptr;
		SJH::ResourceRegistry      *reg    = nullptr;
		ImGuiLayerStack            *stack  = nullptr;
		std::vector<PassDebugEntry> debugEntries;
		float                      *gamma  = nullptr; ///< main 의 mGamma — PostFXDebugLayer 가 float& 보유 (lifetime caller-owned).
		std::function<void()>       onPauseToggle;    ///< Pause 버튼(PauseButtonLayer) 클릭 콜백 — main: TogglePause.
	};

	/// @brief ImGui v1.53 init(install_callbacks=false) + Exit/PostFXDebug 레이어 등록.
	///        기존 main.cpp WarmupImgui 와 동일. ImGui 의존이라 UI_SRC 로 executable 직접 컴파일.
	/// @return ImGuiContext* — main -> mImGuiCtx (shutdown DestroyContext 용).
	ImGuiContext *BuildGameUI(GameUiDeps deps);
}

#endif // __MYAPP_UI_BOOTSTRAP_H__
