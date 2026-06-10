/**
 * @file IImGuiLayer.h
 * @brief ImGui UI 레이어 인터페이스 + 레이어 종류 enum.
 *
 * @details
 *  ### 책임
 *  - ImGui 위젯 한 묶음을 그리는 화면 레이어의 공통 계약 (@c OnBuildUI).
 *  - 레이어를 항상 그릴지(Game) F1 토글 시에만 그릴지(Editor) 구분 (@c GetKind).
 *
 *  ### 비-책임
 *  - [X] 레이어 보관/순회 - @c ImGuiLayerStack 담당.
 *  - [X] ImGui 컨텍스트 생성/레이어 등록 - @c UiBootstrap 담당.
 *
 *  ### 정통 매핑
 *  - 게임 UI 레이어 시스템의 base interface - Cocos2D `Layer` / Unity uGUI Canvas 레이어.
 */
#ifndef __MYAPP_IMGUI_LAYER_H__
#define __MYAPP_IMGUI_LAYER_H__

namespace TopdownShooter::UI
{
	/// @brief 레이어 가시성 정책 구분 - @c ImGuiLayerStack::RenderAll 의 showEditor 게이트에 사용.
	enum class ImGuiLayerKind
	{
		Game,   ///< 항상 렌더 (HUD, 버튼 등).
		Editor, ///< showEditor(F1 토글)가 true 일 때만 렌더 (디버그 창).
	};

	/// @brief ImGui 위젯 한 묶음을 그리는 화면 레이어의 공통 인터페이스.
	/// @details 구체 레이어(PauseButtonLayer / PostFXDebugLayer / StateOverlayLayer / VfxSpawnLayer)가
	///          상속하며, @c ImGuiLayerStack 이 등록된 레이어들을 순회하며 @c OnBuildUI 를 호출한다.
	class IImGuiLayer
	{
	  public:
		virtual ~IImGuiLayer() = default;

		/// @brief 이 레이어의 ImGui 위젯을 빌드/렌더. ImGui 프레임 안에서 매 프레임 호출된다.
		virtual void OnBuildUI() = 0;

		/// @brief 레이어 종류 반환 - Game(항상) / Editor(토글 시).
		/// @return @c ImGuiLayerKind::Game 또는 @c ImGuiLayerKind::Editor.
		virtual ImGuiLayerKind GetKind() const = 0;

		bool Enabled = true; ///< false 면 @c ImGuiLayerStack::RenderAll 이 이 레이어를 건너뛴다.
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_IMGUI_LAYER_H__
