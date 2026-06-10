/**
 * @file UiBootstrap.h
 * @brief ImGui 컨텍스트 초기화 + 게임 UI 레이어 등록 부트스트랩 자유 함수.
 *
 * @details
 *  ### 책임
 *  - ImGui v1.53 CreateContext + StyleColorsDark + ImGui_ImplGlfwGL3_Init(install_callbacks=false).
 *  - sb7 가 GLFW 콜백 소유하므로 Scroll/Char 콜백만 수동 설치.
 *  - PauseButtonLayer + PostFXDebugLayer 를 @c ImGuiLayerStack 에 Push (등록 순서 = 렌더 순서).
 *  - @c ImGuiContext* 반환 - main 이 @c mImGuiCtx 로 보관 후 종료 시 @c DestroyContext 호출.
 *
 *  ### 비-책임
 *  - [X] ImGui 프레임 시작/종료(@c NewFrame/@c Render) - main 의 update/render 루프 책임.
 *  - [X] StateOverlayLayer / VfxSpawnLayer 등록 - 각 Stage FSM 또는 main 이 별도 등록.
 *  - [X] 레이어 lifetime 관리 - @c ImGuiLayerStack 이 unique_ptr 로 소유.
 *
 *  ### 정통 매핑
 *  - Cocos2D @c AppDelegate::applicationDidFinishLaunching - 씬 진입 전 엔진 서브시스템 초기화.
 *
 * @note ImGui 의존이므로 헤더/구현 모두 executable(@c UI_SRC) 에 직접 컴파일 - SJH::engine 에 포함 불가.
 */
#ifndef __MYAPP_UI_BOOTSTRAP_H__
#define __MYAPP_UI_BOOTSTRAP_H__

#include "UI/PostFXDebugLayer.h" // PassDebugEntry (값 멤버라 완전 타입 필요)
#include <functional>
#include <vector>

// fwd - 포인터만 노출.
struct GLFWwindow;
struct ImGuiContext;
namespace SJH
{
	class ResourceRegistry;
}

namespace TopdownShooter::UI
{
	class ImGuiLayerStack;

	/**
	 * @brief @c BuildGameUI 에 전달하는 UI 초기화 의존성 묶음 (Composition Root 에서 주입).
	 * @details
	 *  main(Composition Root) 이 render 객체와 UI 레이어를 매핑해 필드를 채운 뒤 @c BuildGameUI 에 전달.
	 *  포인터 필드(@p window / @p reg / @p stack)는 비소유 - lifetime 은 모두 caller 책임.
	 */
	struct GameUiDeps
	{
		GLFWwindow                 *window = nullptr;  ///< GLFW 윈도우 포인터 - ImGui_ImplGlfwGL3_Init + Scroll/Char 콜백 설치에 사용.
		SJH::ResourceRegistry      *reg    = nullptr;  ///< 텍스처 로드(@c pause_button)에 사용.
		ImGuiLayerStack            *stack  = nullptr;  ///< 레이어를 Push 할 대상 스택 (비소유).
		std::vector<PassDebugEntry> debugEntries;       ///< PostFXDebugLayer 에 주입할 PassComponent 매핑 목록.
		float                      *gamma  = nullptr;  ///< main 의 mGamma - PostFXDebugLayer 가 float& 보유 (lifetime caller-owned).
		std::function<void()>       onPauseToggle;     ///< Pause 버튼(PauseButtonLayer) 클릭 콜백 - main: TogglePause.
	};

	/// @brief ImGui v1.53 컨텍스트 초기화 + 기본 게임 UI 레이어(PauseButton / PostFXDebug) 등록.
	/// @details
	///  내부 등록 순서 (= 렌더 순서):
	///  1. PauseButtonLayer (Game kind, @p onPauseToggle 콜백 연결).
	///  2. PostFXDebugLayer (Editor kind, @p debugEntries + @p *gamma 주입).
	///
	///  기존 main.cpp 의 WarmupImgui 함수와 동일한 역할. ImGui 의존이라 @c UI_SRC 로 executable 직접 컴파일.
	/// @param deps 초기화에 필요한 의존성 묶음 (창/레지스트리/스택/감마/콜백).
	/// @return 생성된 @c ImGuiContext* - main 이 @c mImGuiCtx 로 보관, 종료 시 @c ImGui::DestroyContext 필수.
	ImGuiContext *BuildGameUI(GameUiDeps deps);
}

#endif // __MYAPP_UI_BOOTSTRAP_H__
