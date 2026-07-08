/**
 * @file renderable_processor.h
 * @brief IRenderable flat 큐 정렬/발행 Orchestrator - Task 2.4 RenderableProcessor 로 일반화.
 *
 * @details
 *  ### 책임 (Task 2.4 - DrawCommand -> IRenderable flat)
 *  - @c Submit(IRenderable*, viewDepth) - World flat 수집. queueLayer 는 r->QueueLayer() 캡처.
 *  - @c Sort - queueLayer asc + 투명 back-to-front / 불투명 front-to-back. stable_sort 결정성 보장.
 *  - @c Process(DeviceContext&, Camera&) - ROP 적용(ApplyRenderStateBlock) 후 r->Render(rc, cam) 잎 위임.
 *
 *  본 클래스는 @c mWorld(IRenderable) 단일 큐 프로세서 - World 렌더만 담당한다.
 *
 *  ### 비-책임 (분리된 책임)
 *  - [X] GL 자가 draw 로직 - @c MeshRenderer::Render(잎 자가발행) 위임 (Task 2.3).
 *  - [X] GL state machine 캐싱/적용 - @c DeviceContext::ApplyRenderStateBlock 위임 (D-RS-1).
 *  - [X] Material 값 uniform 송신 - MeshRenderer::Render 내부 @c draw_ops 헬퍼 담당.
 *  - [X] PostFX ScreenQuad blit - @c PostFxPass(3.5a) 가 전담.
 *
 *  ### 정통 매핑
 *  - Unreal @c FMeshPassProcessor - 한 Pass 안의 mesh draw command 들을 처리하는 Orchestrator.
 *  - Cocos2D @c RenderQueue - Layer 정렬 + 순서 발행.
 *
 * @note @c Sort 는 @c std::stable_sort - z-fighting 깜빡임 차단 + 결정성 보장 (골든 이미지 비교 가능).
 */
#ifndef __SJH_MESH_PASS_PROCESSOR_H__
#define __SJH_MESH_PASS_PROCESSOR_H__

#include "render/i_renderable.h"
#include <climits>
#include <cstddef>
#include <vector>

namespace SJH::Scene { class Camera; }
namespace SJH
{
	class DeviceContext;
	class RenderTexture;
	class Material;
	class Mesh;

	/**
	 * @brief IRenderable flat 큐 정렬/GL draw 발행 Orchestrator - Task 2.4 일반화.
	 * @details
	 *  구 MeshPassProcessor(DrawCommand SSoT) 를 IRenderable flat 모델로 교체.
	 *
	 *  World 루프(본질): ApplyRenderStateBlock(r->GetRenderStateBlock()) -> r->Render(rc, cam).
	 *  ROP 는 Process 루프에서만 적용 - 잎(MeshRenderer::Render) 은 ROP 미호출 (역할 분리).
	 */
	class RenderableProcessor
	{
	  public:
		/// @brief World IRenderable 수집.
		/// @param r         렌더할 IRenderable (비소유). nullptr 시 undefined - 호출처 보장.
		/// @param viewDepth view-space z (카메라 전방 음수 - 정렬 시 부호 주의).
		void Submit(const IRenderable *r, float viewDepth)
		{
			mWorld.push_back({r, r->QueueLayer(), r->RenderQueue(), viewDepth});
		}

		/// @brief 큐를 비움 (프레임 시작 시 호출).
		void Clear() { mWorld.clear(); }

		/// @brief 현재 World 큐에 쌓인 항목 수.
		std::size_t Size() const { return mWorld.size(); }

		/// @brief World 큐 정렬 - queueLayer asc -> 투명 back-to-front / 불투명 front-to-back.
		/// @details stable_sort 사용 - z-fighting 깜빡임 차단 + DFS 순서 보존 + 결정성 보장.
		void Sort();

		/// @brief 정렬된 World 큐를 발행 - ROP 적용 후 IRenderable 잎 위임.
		/// @details
		///  World: ApplyRenderStateBlock(r->GetRenderStateBlock()) 후 r->Render(rc, cam).
		///  Process 진입 시 InvalidateStateCache - per-Process 캐시 무효화 (foreign GL 대비, D-RS-2).
		/// @param rc  DeviceContext (BindVAO/DrawIndexed/ApplyRenderStateBlock 등).
		/// @param cam 이번 패스 Camera (MeshRenderer::Render 가 view/proj 도출).
		void Process(DeviceContext &rc, const Scene::Camera &cam);

		/// @brief World draw 를 순수 RenderQueue [min,max) 로 제한(디버그/골든 캡처용). 기본 전범위(무영향).
		/// @details queueLayer(=base+offset) 가 아니라 RenderQueue(순수)로 필터 - 음수 DrawOrder 가 인접 큐로 안 샘.
		void SetQueueFilter(int minQueue, int maxQueue)
		{
			mQueueMin = minQueue;
			mQueueMax = maxQueue;
		}

	  private:
		/// @brief World 항목 - IRenderable 포인터 + 정렬 키.
		struct WorldEntry
		{
			const IRenderable *r;
			int   queueLayer;  ///< 정렬 키 = RenderQueue + DrawOrder(offset). Sort 우선순위용.
			int   renderQueue; ///< 순수 material 큐(offset 미포함) - per-queue 필터용(음수 offset 누수 방지).
			float depth;
		};

		std::vector<WorldEntry>  mWorld; ///< World flat IRenderable 큐.

		int mQueueMin = INT_MIN; ///< World 큐 필터 하한 [min,max) - 캡처 격리용(기본 무영향).
		int mQueueMax = INT_MAX; ///< 상한.
	};
}

#endif // __SJH_MESH_PASS_PROCESSOR_H__
