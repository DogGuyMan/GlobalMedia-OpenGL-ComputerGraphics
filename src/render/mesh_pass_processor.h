/**
 * @file mesh_pass_processor.h
 * @brief IRenderable flat 큐 정렬/발행 Orchestrator - Task 2.4 RenderableProcessor 로 일반화.
 *
 * @details
 *  ### 책임 (Task 2.4 - DrawCommand -> IRenderable flat)
 *  - @c Submit(IRenderable*, viewDepth) - World flat 수집. queueLayer 는 r->QueueLayer() 캡처.
 *  - @c SubmitScreenQuad - PassComponent (PostFX) ScreenQuad 수집 (Phase 3.5 전이 - PostFxPass 이관 예정).
 *  - @c Sort - queueLayer asc + 투명 back-to-front / 불투명 front-to-back. stable_sort 결정성 보장.
 *  - @c Process(DeviceContext&, Camera&) - ROP 적용(ApplyRenderStateBlock) 후 r->Render(rc, cam) 잎 위임.
 *
 *  ### 비-책임 (분리된 책임)
 *  - [X] GL 자가 draw 로직 - @c MeshRenderer::Render(잎 자가발행) 위임 (Task 2.3).
 *  - [X] GL state machine 캐싱/적용 - @c DeviceContext::ApplyRenderStateBlock 위임 (D-RS-1).
 *  - [X] Material 값 uniform 송신 - MeshRenderer::Render 내부 @c draw_ops 헬퍼 담당.
 *
 *  ### 정통 매핑
 *  - Unreal @c FMeshPassProcessor - 한 Pass 안의 mesh draw command 들을 처리하는 Orchestrator.
 *  - Cocos2D @c RenderQueue - Layer 정렬 + 순서 발행.
 *
 * @note @c Sort 는 @c std::stable_sort - z-fighting 깜빡임 차단 + 결정성 보장 (골든 이미지 비교 가능).
 * @note 파일명/헤더가드는 유지 (git mv 개명은 Phase 5). 클래스명만 RenderableProcessor 로 변경.
 */
#ifndef __SJH_MESH_PASS_PROCESSOR_H__
#define __SJH_MESH_PASS_PROCESSOR_H__

#include "render/i_renderable.h"
#include <cstddef>
#include <vector>

namespace SJH::Scene { class Camera; }
namespace SJH
{
	class DeviceContext;
	class Framebuffer;
	class Material;
	class Mesh;

	/**
	 * @brief IRenderable flat 큐 정렬/GL draw 발행 Orchestrator - Task 2.4 일반화.
	 * @details
	 *  구 MeshPassProcessor(DrawCommand SSoT) 를 IRenderable flat 모델로 교체.
	 *
	 *  World 루프(본질): ApplyRenderStateBlock(r->GetRenderStateBlock()) -> r->Render(rc, cam).
	 *  ROP 는 Process 루프에서만 적용 - 잎(MeshRenderer::Render) 은 ROP 미호출 (역할 분리).
	 *
	 *  ### [DEAD-PHASE5] ScreenQuad 서브시스템 = 미사용(dead, 삭제 후보)
	 *  본 클래스의 *최종 책임* 은 "IRenderable 들을 정렬해 발행" 하나뿐이다. ScreenQuad(PostFX 화면합성)
	 *  관련 멤버/메서드(@c mScreen / @c ScreenEntry / @c SubmitScreenQuad / @c mScreenQuadMesh /
	 *  @c mBypassMat / @c mLastOutputFB / @c GetLastOutputFB / @c SetScreenQuadMesh / @c SetBypassMaterial)는
	 *  per-effect @c PostFxPass(3.5a) 가 흡수 완료 -> **현재 호출처 0(dead)** (유일 호출자였던 @c SceneRenderer 도 dead).
	 *  삭제는 *모든 Task 종료 후 Phase 5* 판정 - 제거 시 본 클래스는 @c mWorld(IRenderable) 단일 큐로 응집.
	 *  신규 코드에서 screen API 사용 금지. grep: 본 파일 + .cpp 의 `[TRANSITIONAL-3.5]`(=삭제 라인) + `[DEAD-PHASE5]`.
	 */
	class RenderableProcessor
	{
	  public:
		/// @brief World IRenderable 수집.
		/// @param r         렌더할 IRenderable (비소유). nullptr 시 undefined - 호출처 보장.
		/// @param viewDepth view-space z (카메라 전방 음수 - 정렬 시 부호 주의).
		void Submit(const IRenderable *r, float viewDepth)
		{
			mWorld.push_back({r, r->QueueLayer(), viewDepth});
		}

		/// @brief [TRANSITIONAL-3.5] ScreenQuad(PassComponent) 수집 - Phase 3.5 PostFxPass 로 이관 후 제거 대상.
		/// @details passMat=nullptr 이면 bypass(passthrough). 본 메서드/관련 screen 멤버는 응집도 위반 - 제거 예정.
		void SubmitScreenQuad(Framebuffer *in, Framebuffer *out, Material *passMat)
		{
			mScreen.push_back({in, out, passMat});
		}

		/// @brief 큐를 비우고 mLastOutputFB 를 nullptr 로 초기화 (프레임 시작 시 호출).
		void Clear()
		{
			mWorld.clear();
			mScreen.clear();
			mLastOutputFB = nullptr;
		}

		/// @brief 현재 큐에 쌓인 항목 총합 (World + Screen).
		std::size_t Size() const { return mWorld.size() + mScreen.size(); }

		// ===== [TRANSITIONAL-3.5] 아래 3 ScreenQuad 접근자 = Phase 3.5 PostFxPass 이관 후 제거 =====
		/// @brief [TRANSITIONAL-3.5] ScreenQuad 드로우용 풀스크린 메시 지정. m=nullptr 이면 skip.
		void SetScreenQuadMesh(Mesh *m) { mScreenQuadMesh = m; }

		/// @brief [TRANSITIONAL-3.5] disabled PassComponent bypass blit passthrough material. m=nullptr 이면 skip.
		void SetBypassMaterial(Material *m) { mBypassMat = m; }

		/// @brief [TRANSITIONAL-3.5] 마지막 처리된 ScreenQuad outputFB. 없으면 nullptr.
		const Framebuffer *GetLastOutputFB() const { return mLastOutputFB; }

		/// @brief World 큐 정렬 - queueLayer asc -> 투명 back-to-front / 불투명 front-to-back.
		/// @details stable_sort 사용 - z-fighting 깜빡임 차단 + DFS 순서 보존 + 결정성 보장.
		void Sort();

		/// @brief 정렬된 큐를 발행 - World(ROP + 잎 위임) + ScreenQuad(구 동작 보존).
		/// @details
		///  World: ApplyRenderStateBlock(r->GetRenderStateBlock()) 후 r->Render(rc, cam).
		///  Screen: 구 ScreenQuad 분기와 픽셀 동등.
		///  Process 진입 시 InvalidateStateCache - per-Process 캐시 무효화 (foreign GL 대비, D-RS-2).
		/// @param rc  DeviceContext (BindVAO/DrawIndexed/ApplyRenderStateBlock 등).
		/// @param cam 이번 패스 Camera (MeshRenderer::Render 가 view/proj 도출).
		void Process(DeviceContext &rc, const Scene::Camera &cam);

	  private:
		/// @brief World 항목 - IRenderable 포인터 + 정렬 키.
		struct WorldEntry
		{
			const IRenderable *r;
			int   queueLayer;
			float depth;
		};

		/// @brief [TRANSITIONAL-3.5] ScreenQuad 항목 - PassComponent 배선 데이터 (Phase 3.5 제거).
		struct ScreenEntry
		{
			Framebuffer *in;
			Framebuffer *out;
			Material    *mat;
		};

		std::vector<WorldEntry>  mWorld;                ///< World flat IRenderable 큐 (본질 - 유지).
		// ----- [TRANSITIONAL-3.5] 아래 screen 멤버 = Phase 3.5 PostFxPass 이관 후 전량 삭제 -----
		std::vector<ScreenEntry> mScreen;               ///< [TRANSITIONAL-3.5] ScreenQuad(PassComponent) 큐.
		Mesh              *mScreenQuadMesh = nullptr;   ///< [TRANSITIONAL-3.5] ScreenQuad 풀스크린 메시.
		Material          *mBypassMat      = nullptr;   ///< [TRANSITIONAL-3.5] disabled bypass blit material.
		const Framebuffer *mLastOutputFB   = nullptr;   ///< [TRANSITIONAL-3.5] 마지막 ScreenQuad outputFB.
	};
}

#endif // __SJH_MESH_PASS_PROCESSOR_H__
