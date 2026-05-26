#ifndef __SJH_POSTFX_PASS_H__
#define __SJH_POSTFX_PASS_H__

/**
 * @file postfx_pass.h
 * @brief PostFX 한 단계 — full-screen quad blit 의 모든 입력.
 *
 * @details
 *  ### 설계 출처
 *  doc/design/PostFX.md §3.1 — PostFX = 비-카메라 ordered pass list.
 *  - 4 엔진 정통: Unity ScriptableRenderPass / Unreal UPostProcessMaterial /
 *                 Godot CompositorEffect / Cocos Custom Render Pipeline Pass.
 *  - Camera 는 *부착되지 않음* — Aspect/Fov/Near/Far 무의미.
 *  - 체인 인덱스 = 실행 순서 (Godot Array[RID] 정통).
 *  - Camera.Depth 는 *진짜 다중 view* 전용으로 복귀.
 */

#include <string>

namespace SJH
{
	class Material;
	class Framebuffer;

	/// @brief Stage enum — Unreal BlendableLocation / Godot effect_callback_type 정통.
	/// @note  현 단계에서는 LinearChain 만 구현. Stage 분기는 PostFX.md §6.2 future.
	enum class PostFXStage : int
	{
		BeforeTonemap = 0,   ///< HDR 공간 (bloom, exposure)
		AfterTonemap  = 100, ///< LDR 공간 (vignette, color grade, FXAA) — 현재 모든 효과
		AfterUI       = 200, ///< 화면 캡쳐 등
	};

	/// @brief PostFX 한 단계 — Godot CompositorEffect 정통.
	/// @details InputFB 는 멤버가 아님 — RunPostFXChain 이 체인 순서대로 이전 단계 OutputFB 를 전달.
	///          OutputFB 는 *항상 비-null* — 마지막 단계도 자기 FB 에 씀.
	///          ScreenQuadStage 가 GetActiveFXOutput() 으로 최종 FB 를 받아 backbuffer 에 합성.
	struct PostFXPass
	{
		std::string  Name;                                               ///< 디버그/UI 식별자
		Material *   Material = nullptr;                                 ///< full-screen 셰이더 + uScene sampler
		Framebuffer *OutputFB = nullptr;                                 ///< 이 단계 출력 FB (항상 비-null)
		bool         Enabled  = true;                                    ///< ImGui 토글
		int          Stage    = static_cast<int>(PostFXStage::AfterTonemap); ///< 미래 Stage 분기용 sortkey
	};

} // namespace SJH

#endif // __SJH_POSTFX_PASS_H__
