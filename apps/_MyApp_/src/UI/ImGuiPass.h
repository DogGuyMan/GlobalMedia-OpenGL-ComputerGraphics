#ifndef _TOPDOWNSHOOTER_UI_IMGUIPASS_H__
#define _TOPDOWNSHOOTER_UI_IMGUIPASS_H__

#include "render/render_passable/render_passable.h" // SJH::IPassable
#include "render/device_context.h"                  // rec.InvalidateStateCache
#include <imgui.h>                                  // ImGui::Render (v1.53 - io.RenderDrawListsFn 콜백이 실제 GL draw)

namespace TopdownShooter::UI
{
	/// @brief ImGui 발행을 IPassable 로 래핑하는 역할군 Pass (PassIterator 종단 - 항상 최상위).
	/// @details
	///  NewFrame + UI 빌드(ImGuiLayerStack::RenderAll / PostFXDebugLayer 등)는 main 프레임 루프가 담당.
	///  본 Pass 는 *발행만* - ImGui v1.53 은 @c ImGui::Render() 가 io.RenderDrawListsFn 콜백으로
	///  직접 GL draw 하므로 별도 RenderDrawData 호출이 필요 없다.
	/// @note Client 거주 - ImGui 의존이라 executable target (header-only). 직전 ScreenQuadStage 가
	///       backbuffer 를 bind 해둔 상태에서 그 위에 그린다 (현 동작 보존).
	class ImGuiPass : public SJH::IPassable
	{
	  public:
		/// @brief foreign GL(ImGui) 경계 - 진입/이탈 InvalidateStateCache 로 DeviceContext 캐시 desync 차단.
		void Draw(SJH::DeviceContext &rec, const SJH::Texture * /*before*/) override
		{
			rec.InvalidateStateCache();
			ImGui::Render();
			rec.InvalidateStateCache();
		}

		/// @brief 이 Pass 의 출력 - backbuffer 에 직접 그림, 텍스처 결과 없음.
		const SJH::Texture *GetPassResult() const override { return nullptr; }
	};
}

#endif // _TOPDOWNSHOOTER_UI_IMGUIPASS_H__
