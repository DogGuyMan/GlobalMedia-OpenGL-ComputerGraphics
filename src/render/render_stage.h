/**
 * @file render_stage.h
 * @brief 최상위 렌더 단계 추상 — 다중 렌더 pass 의 직교 분리
 *
 * @details
 *  ### 존재 의의
 *  RenderStage 는 Application 이 매 프레임 호출하는 *순차적 렌더 단계* 추상.
 *  각 Stage 는 *직교 책임* (Scene / PostFX / ImGui / Skybox / DebugDraw 등)
 *  을 충족하는 자유도를 가지며, 내부적으로 Pass 를 Queue 분류하기 위해
 *  `Material::Pass::Kind` 를 활용 — 따라서 Pass != Stage (Pass 는 한 Stage 내부).
 *
 *  ### 정통 엔진 매핑
 *  - Unity `ScriptableRendererFeature` + `ScriptableRenderPass` (URP 조합)
 *  - Unreal `FSceneRenderer` — tick 마다 순회  FSceneColorRenderTarget 그림
 *  - DX11/DX12 post-effect chain (per-stage 리소스 할당)
 *  -> Application 다형성 — 데모별로 Stage 수용 조합이 다름.
 */
#ifndef __SJH_IRENDER_STAGE_H__
#define __SJH_IRENDER_STAGE_H__

namespace SJH
{
	class RenderTarget;

	/// @brief 최상위 렌더 단계 추상 — Unity ScriptableRendererFeature / Unreal FSceneRenderer 정통.
	/// @details Application 의 render() 가 보유한 IRenderStage* vector 를 순서대로 호출.
	///          각 stage 는 *직교 책임* — Scene / PostFX / ImGui / Skybox / DebugDraw 등.
	///          `Material::Pass::Kind` 와 *다른 레이어* — Pass 는 한 stage *내부* Queue 분류.
	class IRenderStage
	{
	  public:
		virtual ~IRenderStage() = default;

		/// @brief 매 프레임 — Application 이 backbuffer 를 주입.
		/// @details 구체가 자기 안에서 DeviceContext::BeginFrame(target) 호출 책임.
		virtual void Render(RenderTarget& target) = 0;

		/// @brief Window resize broadcast — 자기 internal FBO 등 sync 용. 기본 no-op.
		/// @note SceneRenderer 는 미사용 (Camera 가 매 프레임 target.GetSize 동적 조회).
		virtual void OnResize(int /*w*/, int /*h*/) {}
	};
} // namespace SJH

#endif // __SJH_IRENDER_STAGE_H__
