/**
 * @file renderable_processor.cpp
 * @brief RenderableProcessor 구현 - Sort 정렬 정책 + Process IRenderable flat 발행 흐름.
 *
 * @details
 *  ### Task 2.4 - DrawCommand -> IRenderable flat 전환
 *  - World 루프: ApplyRenderStateBlock(r->GetRenderStateBlock()) -> r->Render(rc, cam).
 *    ROP 위치: Process 루프 (잎 미호출 - 역할 분리 유지).
 *
 *  ### Sort 정렬 정책 요약 (구 SortMultiStage 값 보존)
 *  - Opaque (queue < 2500): front-to-back (z-cull 효율). 구 program/material 그룹핑 키 생략
 *    (A-dedup 흡수 - DECISION 2.4-b). stable_sort 결정성 보장.
 *  - Transparent (queue >= 2500): back-to-front (알파 합성 정확도).
 *  - view-space z 부호: 카메라 앞 = 음수. front-to-back = a.depth > b.depth.
 *
 *  ### 비-책임
 *  - [X] GL 자가 draw - MeshRenderer::Render 잎 위임 (Task 2.3).
 *  - [X] Material 값 uniform 송신 - MeshRenderer::Render 내부 draw_ops 헬퍼.
 *  - [X] GL state machine 캐싱/적용 - DeviceContext::ApplyRenderStateBlock (D-RS-1).
 *  - [X] PostFX ScreenQuad blit - PostFxPass(3.5a) 가 전담.
 */
#include "render/renderable_processor.h"
#include "render/device_context.h"
#include "material/material.h"
#include "material/pass.h"
#include "object/mesh.h"
#include "program/program.h"
#include "buffer/render_texture.h"
#include "scene/camera.h"
#include <algorithm>

namespace SJH
{
	void RenderableProcessor::Sort()
	{
		// --- 정렬 정책 (Unity TransparencySortMode 정통, 구 SortMultiStage 값 보존) --------
		// Opaque (queue < 2500): front-to-back (z-cull 효율)
		//   구 program/material 2/3차 그룹핑 키 생략 (A-dedup 이 흡수 - DECISION 2.4-b).
		//   픽셀 무관: 불투명은 depth-test 가 결과 보존.
		// Transparent (queue >= 2500): back-to-front (먼 객체 먼저 - 알파 합성 정확도).
		//
		// --- view-space z 부호 함정 ------------------------------------------
		// OpenGL 카메라는 -Z 방향 -> 카메라 앞 = 음수 z. 멀수록 더 음수.
		//   가까운 객체: z = -10  (덜 음수, 큰 값)
		//   먼  객체:   z = -100 (더 음수, 작은 값)
		//   front-to-back = a.depth > b.depth (큰 값 = 덜 음수 = 가까움)
		//   back-to-front = a.depth < b.depth (작은 값 = 더 음수 = 멂)
		//
		// --- stable_sort 3가치 -----------------------------------------------
		// 1. z-fighting 깜빡임 차단 - 동등 depth 두 면이 매 프레임 같은 순서 (flicker 회피).
		// 2. Actor DFS 순서 보존 - Submit 순서 = 씬 계층 -> 시각 디버깅 일관성.
		// 3. 골든 이미지 결정성 - 동등 항목 정렬 결과 매 호출 동일 (false-positive flicker 차단).
		std::stable_sort(mWorld.begin(), mWorld.end(),
			[](const WorldEntry &a, const WorldEntry &b) {
				if (a.queueLayer != b.queueLayer) return a.queueLayer < b.queueLayer;
				if (Pass::IsTransparentQueue(a.queueLayer)) return a.depth < b.depth;   // back-to-front
				return a.depth > b.depth;   // front-to-back
			});
	}

	void RenderableProcessor::Process(DeviceContext &rc, const Scene::Camera &cam)
	{
		// Process 진입 - GL state 캐시 무효화 (per-Process 캐시 불변식 보존, D-RS-2).
		//   직전 stage(다른 카메라 Process / Effekseer ParticlePass 등)가 GL state 를
		//   캐시 뒤에서 바꿨을 수 있으므로 첫 ApplyRenderStateBlock 가 전체 강제 적용하도록 한다.
		rc.InvalidateStateCache();

		// -- World flat (IRenderable 잎 위임) ------------------------------------------
		// ROP 는 여기서만 적용 (잎은 ROP 미호출 - 역할 분리 D7).
		for (const auto &e : mWorld)
		{
			rc.ApplyRenderStateBlock(e.r->GetRenderStateBlock());
			e.r->Render(rc, cam);
		}

		// RestoreDefaults 불요 (D-RS-2) - 다음 consumer 진입 시 InvalidateStateCache + 자기 state 적용.
	}
}

