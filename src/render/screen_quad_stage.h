/**
 * @file screen_quad_stage.h
 * @brief N 개 FBO color attachment → backbuffer 합성 단계.
 *
 * @details
 *  ### 존재 의의
 *  SP-UniversalRenderTarget Phase A — "모든 렌더링 대상은 RenderTarget 을 가진다.
 *  backbuffer 는 ScreenQuadStage 만의 출력" 원칙의 출력 측 구현.
 *
 *  SceneRenderer 가 각 Camera 마다 자기 FBO 에 렌더 후, ScreenQuadStage 가
 *  최종 합성 결과를 backbuffer(DefaultRenderTarget) 에 덮어쓴다.
 *
 *  ### 정통 엔진 매핑
 *  - Unity URP `FinalBlitPass` / `FinalPostProcessPass`
 *  - Unreal `FRCPassPostProcessTonemap` → backbuffer blit
 *  - Godot `RenderingServer::canvas_item_add_texture_rect` (2D blit final)
 *  - Cocos `Backend::RenderPipeline` final blit step
 *
 *  ### Owner 분담 (SP-RTOwnership 연속)
 *  - `Program` (passthrough 셰이더) — ResourceRegistry 소유, ScreenQuadStage 비소유
 *  - `Mesh` (screen quad) — ResourceRegistry 소유, ScreenQuadStage 비소유
 *  - `Framebuffer` 소스 — ResourceRegistry 소유, ScreenQuadStage 는 포인터만 보관
 */
#ifndef __SJH_SCREEN_QUAD_STAGE_H__
#define __SJH_SCREEN_QUAD_STAGE_H__

#include "render/render_stage.h"
#include <vector>

namespace SJH
{
	class Program;
	class Framebuffer;
	class Mesh;

	/// @brief FBO color attachment 를 backbuffer 로 합성하는 최종 렌더 스테이지.
	/// @details
	///   - `SetSources` 로 등록된 FBO 들을 순서대로 backbuffer 에 합성.
	///   - 첫 소스: depth test OFF + blend OFF (replace). 2+ 소스: alpha blend ON.
	///   - passthrough Program 의 sampler 이름 컨벤션 = `uScene` (migrate_demo SP4 정통).
	class ScreenQuadStage : public IRenderStage
	{
	  public:
		/// @brief passthrough Program + screen quad Mesh 주입.
		/// @param passthrough  `uScene` sampler + `aPos`/`aUV` attribute 만 사용하는 셰이더 프로그램.
		/// @param screenQuad   NDC 화면 가득 덮는 quad mesh (Geometry::ScreenQuad 산출물).
		/// @note 두 인자 모두 *비소유* 참조 — ScreenQuadStage 보다 오래 살아야 한다.
		ScreenQuadStage(Program &passthrough, Mesh &screenQuad);

		/// @brief 합성 소스 FBO 목록 교체 — 매 프레임 또는 resize 시 호출.
		/// @param sources color attachment 를 backbuffer 에 합성할 FBO 포인터 목록 (순서 = 합성 순서).
		///                nullptr 원소는 자동 skip.
		void SetSources(std::vector<const Framebuffer *> sources);

		/// @brief IRenderStage — sources 를 target(backbuffer) 에 합성.
		/// @param target Application 이 보유한 DefaultRenderTarget — glBindFramebuffer(0) + viewport.
		/// @note sources 가 비어 있으면 no-op (backbuffer 변경 없음).
		void Render(RenderTarget &target) override;

		/// @brief Window resize 시 no-op (소스 FBO 크기는 호출자 책임).
		void OnResize(int /*w*/, int /*h*/) override {}

	  private:
		Program &mProgram;                         ///< passthrough 셰이더 (비소유).
		Mesh &mMesh;                               ///< screen quad (비소유).
		std::vector<const Framebuffer *> mSources; ///< 합성 소스 FBO 목록 (비소유 포인터).
	};

} // namespace SJH

#endif // __SJH_SCREEN_QUAD_STAGE_H__
