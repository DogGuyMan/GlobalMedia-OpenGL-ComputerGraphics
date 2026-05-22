/**
 * @file pass.h
 * @brief Cocos `.effect` technique / Unity SurfaceType / Unreal BlendMode 의 통합 추상화.
 *
 * @details
 *  ### 디자인 동기 (Cocos2d-x 정통)
 *  Material 이 *자기 렌더링 의도* 를 선언 — "나는 Opaque", "나는 Transparent" 등.
 *  엔진 (RenderQueue::Flush + SortMultiStage) 이 이 선언을 보고 다음을 *자동* 적용:
 *  - Queue layer (sort 우선순위)
 *  - Depth test / write 상태
 *  - Blend enable / func
 *  - Sort 방향 (Opaque = front-to-back z-cull / Transparent = back-to-front blending)
 *
 *  ### 챕터 측 사용 패턴
 *  ```cpp
 *  auto* mat = reg.CreateMaterial("window");
 *  mat->SetProgram(prog);
 *  SJH::Uniforms::SetTexture(*mat, "tex0", tex, 0);
 *  mat->SetPass(SJH::Pass::Kind::Transparent);   // ★ 한 줄로 모든 state 자동
 *  ```
 *
 *  ### 비-책임
 *  - ❌ stencil/cull/blend equation 등 *고급 state* — Pass::Kind 만으로 충분치 않은 경우는
 *    MeshRenderer 의 직접 override (Stencil/DepthTest/DepthWrite) 가 우선 적용 — 학습 가시성.
 *  - ❌ Pass 의 *순서/단계 분리* (Unreal 의 TranslucentRenderer 별도 pass 같은) — 현 엔진은
 *    단일 forward pass. Multi-pass 는 Camera + Layer mask 로 표현 (SP4 postfx_demo 정통).
 *
 *  ### 우선순위 (override 규칙)
 *  RenderQueue::Flush 가 cmd 마다:
 *    1. cmd.material->PassKind 보고 *기본 State* 도출
 *    2. MeshRenderer 측 *명시 override* (DepthTest/DepthWrite/Stencil) 가 있으면 그 값 사용
 *  → Pass 는 *기본값 일괄 적용*, override 는 *세밀 조정* — Unity 의 *Material Render Queue 자동 + override* 정통.
 */
#ifndef __SJH_PASS_H__
#define __SJH_PASS_H__

#include "GL/gl3w.h"
#include <GL/glcorearb.h>

namespace SJH::Pass
{
	/// @brief Material 의 렌더링 의도 종류 — Cocos technique 정통.
	/// @details **underlying value = Unity Render Queue 정수** — *진실의 원천 단일화*.
	///          - `static_cast<int>(Kind::Opaque)` == 2000 (Unity Geometry queue)
	///          - `static_cast<int>(Kind::AlphaTest)` == 2450 (Unity AlphaTest queue)
	///          - `static_cast<int>(Kind::Transparent)` == 3000 (Unity Transparent queue)
	///
	///          추가 종류는 enum 확장 + DefaultStateOf 의 case 추가만으로 도입.
	///          (예: Background=1000, ShadowCaster=2450, UI=3000+, Skybox=2000 등)
	enum class Kind : int
	{
		Opaque = 2000,    ///< depth test/write on, blend off, front-to-back sort
		AlphaTest = 2450, ///< discard 기반 — depth test/write on, blend off
		Skybox = 2500,
		Transparent = 3000, ///< 반투명 alpha-blend — depth test on, write off, blend on, back-to-front sort
	};

	/// @brief Transparent 임계값 — queue 이 이 값 이상이면 back-to-front sort (Unity TransparencySortMode 정통).
	inline constexpr int TRANSPARENT_THRESHOLD = 2500;

	/// @brief Kind + offset 으로 queue 도출 — Outline / Skybox 등 *enum 사이 미세 조정* 케이스.
	/// @details Cocos `Pass::priority` 정통. 예: `QueueOf(Kind::Opaque, 5)` = 2005 (Box 도장 직후 Outline).
	inline constexpr int QueueOf(Kind k, int offset = 0)
	{
		return static_cast<int>(k) + offset;
	}

	/// @brief Pass::Kind 가 결정하는 *기본 GL state*. RenderQueue 가 자동 적용.
	/// @details Cocos `.effect` 의 depthStencilState / blendState / rasterizerState 묶음 대응.
	struct State
	{
		// Depth
		bool DepthTest = true;
		bool DepthWrite = true;

		GLenum DepthFunc = GL_LEQUAL;
		GLenum CullMode = GL_BACK;

		// Blend (BlendEnable=false 면 BlendSrc/Dst 무시)
		bool BlendEnable = false;
		GLenum BlendSrc = GL_SRC_ALPHA;
		GLenum BlendDst = GL_ONE_MINUS_SRC_ALPHA;

		// Sort/Queue
		int QueueLayer = 2000; ///< 작은 값 먼저. Unity: 2000 Opaque, 2450 AlphaTest, 3000 Transparent.
	};

	/// @brief Pass::Kind → 기본 State 매핑 (Unity SurfaceType / Cocos technique 정통).
	/// @note inline — 헤더 only. include 하는 모든 TU 가 각자 도출 (ODR 안전).
	inline State DefaultStateOf(Kind k)
	{
		switch (k)
		{
		case Kind::Opaque:
			return State{
			    /*DepthTest*/  true,      /*DepthWrite*/ true,
			    /*DepthFunc*/  GL_LEQUAL, /*CullMode*/   GL_BACK,
			    /*BlendEnable*/false,     GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA,
			    /*QueueLayer*/ QueueOf(Kind::Opaque)};

		case Kind::AlphaTest:
			return State{
			    /*DepthTest*/  true,      /*DepthWrite*/ true,
			    /*DepthFunc*/  GL_LEQUAL, /*CullMode*/   GL_BACK,
			    /*BlendEnable*/false,     GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA,
			    /*QueueLayer*/ QueueOf(Kind::AlphaTest)};

		case Kind::Skybox:
			// Skybox 정통:
			//  · DepthWrite off — skybox 가 depth 갱신하면 뒤 transparent 가 가려짐
			//  · DepthFunc LEQUAL — 셰이더의 .xyww 트릭으로 NDC z=1.0 강제 → cleared depth(1.0) 와 동등 통과
			//  · CullMode FRONT — cube 안쪽에서 보기 때문에 *back face* 가 view 에 보임 → front 컬링
			//  · Queue 2500 — Opaque 다음, Transparent 전 (z-cull 효율 우월)
			return State{
			    /*DepthTest*/  true,      /*DepthWrite*/ false,
			    /*DepthFunc*/  GL_LEQUAL, /*CullMode*/   GL_FRONT,
			    /*BlendEnable*/false,     GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA,
			    /*QueueLayer*/ QueueOf(Kind::Skybox)};

		case Kind::Transparent:
			// Transparent 정통 (LearnOpenGL Blending / Unity Lit Transparent):
			//  · DepthWrite off — Transparent 들끼리 가리지 않도록
			//  · CullMode 0 (cull off) — *양면 그리기*. 유리창/잎사귀/의류 등 *두께 없는 면*
			//    이 카메라 어느 방향에서든 보이도록. GL_BACK 이면 plane 의 *뒷면* 이 culling 되어
			//    카메라가 plane 뒤로 갈 때 *안 보이는 버그*.
			return State{
			    /*DepthTest*/  true,      /*DepthWrite*/ false,
			    /*DepthFunc*/  GL_LEQUAL, /*CullMode*/   0,       // ★ cull off — 양면 그리기
			    /*BlendEnable*/true,      GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA,
			    /*QueueLayer*/ QueueOf(Kind::Transparent)};
		}
		// unreachable — switch 가 enum 전부 커버.
		return State{};
	}

	/// @brief Queue layer 가 *Transparent 임계값* 이상인지. Sort 방향 분기에 사용.
	/// @details Unity TransparencySortMode 정통 — queue 2501 이상은 back-to-front.
	inline bool IsTransparentQueue(int queueLayer)
	{
		return queueLayer >= TRANSPARENT_THRESHOLD;
	}
} // namespace SJH::Pass

#endif // __SJH_PASS_H__
