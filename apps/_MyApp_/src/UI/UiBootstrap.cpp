/**
 * @file UiBootstrap.cpp
 * @brief BuildGameUI 구현 — ImGui v1.53 init + 게임 UI 레이어 Push.
 *
 * @details
 *  ### 구현 요점
 *  - GL/gl3w.h 는 반드시 최상단 include — imgui_impl_glfw_gl3 + resource_registry 의 GL 심볼보다 먼저 필요.
 *  - install_callbacks=false: sb7 가 GLFW 콜백 루트를 소유하므로 ImGui 는 Scroll/Char 두 콜백만 수동 설치.
 *  - pause_button 텍스처는 @c CreateTexture("pause_button", ...) 로 ResourceRegistry 에 위탁 (lifetime 레지스트리 소유).
 *  - Push 순서 = 렌더 순서: PauseButtonLayer(Game) -> PostFXDebugLayer(Editor).
 */
#include <GL/gl3w.h> // 반드시 최상단 — imgui_impl_glfw_gl3 / resource_registry 의 GL 보다 먼저.

#include "UI/UiBootstrap.h"

#include "UI/PauseButtonLayer.h"
#include "UI/ImGuiLayerStack.h"
#include "UI/PostFXDebugLayer.h"
#include "resource_registry/image.h"
#include "resource_registry/resource_registry.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

#include <memory>
#include <utility>

namespace TopdownShooter::UI
{
	ImGuiContext *BuildGameUI(GameUiDeps deps)
	{
		// === ImGui v1.53 init (install_callbacks=false — sb7 가 GLFW 콜백 소유) ===
		ImGuiContext *ctx = ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplGlfwGL3_Init(deps.window, /*install_callbacks=*/false);
		glfwSetScrollCallback(deps.window, ImGui_ImplGlfwGL3_ScrollCallback);
		glfwSetCharCallback(deps.window, ImGui_ImplGlfwGL3_CharCallback);

		// Pause 버튼 텍스처 — Game UI Layer (구 ExitButton 기능 전환 2026-06-04)
		const auto *pauseTex = deps.reg->CreateTexture(
		    "pause_button",
		    SJH::Image::Load("pause_button", "resources/texture/pause_button.png").get());

		// ImGui 레이어 등록 — Game(항상) / Editor(F1 토글)
		deps.stack->Push(std::make_unique<PauseButtonLayer>(deps.onPauseToggle, pauseTex));
		deps.stack->Push(std::make_unique<PostFXDebugLayer>(std::move(deps.debugEntries), *deps.gamma));

		return ctx;
	}
}
