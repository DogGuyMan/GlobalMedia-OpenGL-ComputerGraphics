#include <GL/gl3w.h> // 반드시 최상단 — imgui_impl_glfw_gl3 / resource_registry 의 GL 보다 먼저.

#include "UI/UiBootstrap.h"

#include "UI/ExitButtonLayer.h"
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

		// ExitButton 텍스처 — Game UI Layer
		const auto *exitTex = deps.reg->CreateTexture(
		    "exit_texture",
		    SJH::Image::Load("exit_texture", "resources/texture/exit_texture.png").get());

		// ImGui 레이어 등록 — Game(항상) / Editor(F1 토글)
		deps.stack->Push(std::make_unique<ExitButtonLayer>(deps.window, exitTex));
		deps.stack->Push(std::make_unique<PostFXDebugLayer>(std::move(deps.debugEntries), *deps.gamma));

		return ctx;
	}
}
