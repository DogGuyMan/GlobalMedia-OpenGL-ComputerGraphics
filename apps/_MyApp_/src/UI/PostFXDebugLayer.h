/**
 * @file PostFXDebugLayer.h
 * @brief PostFX PassComponent 파라미터를 실시간으로 조정하는 ImGui 디버그 레이어.
 *
 * @details
 *  ### 책임
 *  - gamma / fog / bloom / grayscale_vignetting 등 PassComponent 별 ImGui 위젯 빌드.
 *  - 각 @c PassDebugEntry 의 @c PassComponent::Enabled 토글 + @c Material::Properties 직접 수정.
 *  - Editor kind — F1 토글 시에만 렌더 (개발/디버그 전용 창).
 *
 *  ### 비-책임
 *  - [X] PassComponent 생성/소유 - @c main(Composition Root) 이 소유, 포인터만 참조.
 *  - [X] ImGui 컨텍스트 초기화 - @c UiBootstrap::BuildGameUI 담당.
 *  - [X] gamma 값 저장 - 참조(@c float&)로 main 의 mGamma 를 직접 수정.
 *
 *  ### 정통 매핑
 *  - Unity Editor Inspector / Unreal Details Panel 역할 — 런타임 셰이더 파라미터 조정.
 *
 * @note PassDebugEntry::Name 문자열이 PassComponent 를 식별하는 키 — 이름 오타 시 위젯 미노출.
 */
#ifndef __MYAPP_POSTFX_DEBUG_LAYER_H__
#define __MYAPP_POSTFX_DEBUG_LAYER_H__

#include "UI/IImGuiLayer.h"
#include "render/pass_component.h"
#include <string>
#include <vector>

namespace TopdownShooter::UI
{
	/**
	 * @brief PostFX 디버그 창 한 항목 — 이름과 해당 PassComponent 포인터 쌍.
	 * @details @c PostFXDebugLayer 생성자에 주입되며, @c PassComponent 소유권은 갖지 않는다.
	 */
	struct PassDebugEntry
	{
		std::string                Name;       ///< 패스 식별 문자열 ("gamma"/"fog"/"bloom"/"grayscale_vignetting" 등).
		SJH::Scene::PassComponent *Component;  ///< 비소유 — PassComponent 의 lifetime 은 main 이 책임.
	};

	/**
	 * @brief PostFX 파라미터 실시간 조정 ImGui 레이어 (Editor kind).
	 * @details
	 *  등록된 @c PassDebugEntry 목록을 순회하며 패스 이름에 따라 전용 위젯을 빌드한다.
	 *  - "gamma"              : gamma SliderFloat + @c mGamma float& 동기.
	 *  - "fog"                : density/start/end SliderFloat + color ColorEdit3 + mode SliderInt.
	 *  - "bloom"              : threshold/spread/intensity SliderFloat.
	 *  - "grayscale_vignetting" : Health/vignette SliderFloat + vig color ColorEdit3.
	 *
	 *  창 위치는 @c ImGuiCond_FirstUseEver 로 초기화 — 사용자 이동 후 위치 유지.
	 */
	class PostFXDebugLayer : public IImGuiLayer
	{
	  public:
		/// @brief 생성자.
		/// @param passes 디버그 대상 PassComponent 항목 목록 (값 복사 후 내부 보유).
		/// @param gamma  main 의 gamma 값 참조 — SliderFloat 조작 시 이 값이 직접 변경된다.
		PostFXDebugLayer(std::vector<PassDebugEntry> passes, float &gamma);

		/// @brief Editor kind 반환 — F1 토글 시에만 렌더.
		/// @return @c ImGuiLayerKind::Editor.
		ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Editor; }

		/// @brief "PostFX Debug" 창을 빌드 — mPasses 순회하며 패스별 위젯 출력.
		void           OnBuildUI() override;

	  private:
		std::vector<PassDebugEntry> mPasses;   ///< 디버그 대상 PassComponent 항목 목록 (값 소유).
		float                      &mGamma;    ///< main 의 gamma float 참조 — lifetime 은 caller 책임.
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_POSTFX_DEBUG_LAYER_H__
