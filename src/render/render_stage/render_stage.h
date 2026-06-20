/**
 * @file render_stage.h
 * @brief 최상위 렌더 단계 순수 추상 - 다중 렌더 pass 의 직교 분리.
 *
 * @details
 *  ### 책임
 *  - Application 이 매 프레임 호출하는 *순차적 렌더 단계* 의 공통 인터페이스 제공.
 *  - 각 Stage 는 *직교 책임* (Scene / PostFX / ImGui / Skybox / DebugDraw 등) 을 충족하는
 *    자유도를 가지며, 내부적으로 Pass 를 Queue 분류하기 위해 @c Material::Pass::Kind 를 활용.
 *    Pass != Stage - Pass 는 한 Stage *내부* Queue 분류 단위.
 *
 *  ### 비-책임
 *  - [X] Stage 생성/소유 - Application 의 @c mStages (vector<unique_ptr<IRenderStage>>) 책임.
 *  - [X] Pass Queue 자체 - @c Material::Pass::Kind 와 @c MeshRenderer::QueueOffset 의 조합.
 *
 *  ### 정통 엔진 매핑
 *  - Unity `ScriptableRendererFeature` + `ScriptableRenderPass` (URP 조합)
 *  - Unreal `FSceneRenderer` - tick 마다 순회 FSceneColorRenderTarget 그림
 *  - DX11/DX12 post-effect chain (per-stage 리소스 할당)
 *  - Application 다형성 - 데모별로 Stage 수용 조합이 다름.
 *
 * @note 최상위 추상이라 render_stage/ 폴더에 *파일 그대로* 유지 (impls 로 병합 안 함).
 *       vtable 홈 TU 는 같은 폴더의 render_stage.cpp (header-only abstract ODR 보장).
 */
#ifndef __SJH_IRENDER_STAGE_H__
#define __SJH_IRENDER_STAGE_H__

namespace SJH
{
	class RenderTarget;

	/**
	 * @brief 최상위 렌더 단계 순수 추상 - Unity ScriptableRendererFeature / Unreal FSceneRenderer 정통.
	 * @details
	 *  Application 의 @c render() 가 보유한 @c IRenderStage* 벡터를 순서대로 호출.
	 *  각 stage 는 *직교 책임* - Scene / PostFX / ImGui / Skybox / DebugDraw 등.
	 *  @c Material::Pass::Kind 와 *다른 레이어* - Pass 는 한 stage *내부* Queue 분류.
	 *
	 *  ### 구체 클래스 목록 (render_stage.impls.{h,cpp})
	 *  | 클래스 | 책임 |
	 *  |---|---|
	 *  | @ref SceneRenderer | Actor 트리 순회 + DrawCommand Flush (orchestrator) |
	 *  | @ref CameraStage | 단일 Camera 의 씬 렌더 위임 |
	 *  | @ref ScreenQuadStage | FBO -> backbuffer 합성 (PostFX 최종 출력) |
	 */
	class IRenderStage
	{
	  public:
		virtual ~IRenderStage() = default;

		/// @brief 매 프레임 호출 - Application 이 backbuffer RenderTarget 을 주입.
		/// @details 구체가 자기 안에서 @c DeviceContext::BindTarget(target) 호출 책임.
		/// @param target Application 이 보유한 @c DefaultRenderTarget (backbuffer).
		virtual void Render(RenderTarget& target) = 0;

		/// @brief Window resize broadcast - 내부 FBO 크기 sync 용. 기본 no-op.
		/// @details @c CameraStage 는 Camera 가 매 프레임 @c target.GetSize 를 동적 조회하므로 미사용.
		/// @param w 새 창 가로 크기 (픽셀).
		/// @param h 새 창 세로 크기 (픽셀).
		virtual void OnResize(int /*w*/, int /*h*/) {}
	};
} // namespace SJH

#endif // __SJH_IRENDER_STAGE_H__
