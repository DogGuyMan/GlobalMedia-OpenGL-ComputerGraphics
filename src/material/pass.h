/**
 * @file pass.h
 * @brief Material 의 *렌더링 의도* 한 줄 선언 — Unity Render Queue + Filament/Unreal/Cocos 정통 (EngineAPI §4.7).
 *
 * @details
 *  ### 핵심 철학 — *진실의 원천 단일화*
 *  `Material::SetPass(Kind)` 한 호출이 **queue 정수 + 7 GL state** 모두를 결정한다.
 *  엔진 (`MeshPassProcessor::Process` + `SortMultiStage`) 이 이 한 값에서:
 *  - Queue layer (sort 우선순위, `enum` underlying = Unity Render Queue 정수)
 *  - Depth test/write/func, Cull mode, Blend enable/src/dst — `DefaultPipelineStateOf(Kind)`
 *  - Sort 방향 — `IsTransparentQueue(q)` 가 분기 (queue 2500 = Opaque/Transparent 경계)
 *
 *  ### 직교 축 분리 (§4.7)
 *  | 축 | 결정자 | 역할 |
 *  |---|---|---|
 *  | `Pass::Kind` | **Material** | "어떤 종류" (큰 분류 + GL state 일괄) |
 *  | `QueueOffset` | **MeshRenderer** | "같은 종류 안 인스턴스 순서" (Outline 후행 등) |
 *
 *  ### MeshRenderer override 와의 관계
 *  `MeshPassProcessor::Process` 는 *Pass 기본 -> MeshRenderer 명시 override* 순으로 적용.
 *  Pass 는 *대부분의 정상 use case 를 기본값으로 흡수*, override 는 세밀 조정용 (Stencil 등).
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
	///          추가 종류는 enum 확장 + DefaultPipelineStateOf 의 case 추가만으로 도입.
	///          (예: Background=1000, ShadowCaster=2450, UI=3000+, Skybox=2000 등)
	enum class Kind : int
	{
		StencilMaskWrite = 1999, ///< Outline 2-pass의 Pass 1 — Opaque 처럼 그리면서 stencil buffer에 ref=1 기록
		                         ///<   OutlineVisible/XRay 가 이 값에 의존 (GL_NOTEQUAL ref=1 이 의미를 가짐)
		Opaque = 2000,    ///< depth test/write on, blend off, front-to-back sort
		AlphaTest = 2450, ///< discard 기반 — depth test/write on, blend off
		Skybox = 2500,
		Transparent = 3000,    ///< 반투명 alpha-blend — depth test on, write off, blend on, back-to-front sort
		OutlineVisible = 4000, ///< Stencil-masked outline — DepthFunc=LEQUAL (Opaque 에 가려질 수 있음, 자연스러운 윤곽선)
		OutlineXRay = 4001,    ///< Stencil-masked outline — DepthFunc=GREATER (벽 뒤에 가려진 부분만 — Apex/Overwatch 적 표시 정통)
	};

	/// @brief Transparent 임계값 — queue 이 이 값 이상이면 back-to-front sort (Unity TransparencySortMode 정통).
	inline constexpr int TRANSPARENT_THRESHOLD = 2500;

	/// @brief Kind + offset 으로 queue 도출 — Outline / Skybox 등 *enum 사이 미세 조정* 케이스.
	/// @details Cocos `Pass::priority` 정통. 예: `QueueOf(Kind::Opaque, 5)` = 2005 (Box 도장 직후 Outline).
	inline constexpr int QueueOf(Kind k, int offset = 0)
	{
		return static_cast<int>(k) + offset;
	}

	/// @brief Material.PassKind 가 결정하는 *GPU 파이프라인 고정 단계 설정 묶음*.
	/// @details DirectX12 `D3D12_PIPELINE_STATE_DESC` / Vulkan `Vk*StateCreateInfo` 정통.
	///          - GoF *State Pattern* (행위 패턴) 과는 *완전히 다른 데이터 구조*.
	///          - 4 영역 (Depth / Cull / Blend / Stencil) 의 fixed-function GL state + 1 sort key.
	///          - `MeshPassProcessor::Process` 가 매 DrawCommand 직전 이 묶음을 풀어 GL 호출로 적용.
	struct PipelineState
	{
		// Depth
		bool DepthTest = true; // "기존 z vs 새 z 비교 자체를 할 것인가"

		bool DepthWrite = true; // "통과한 fragment 의 z 를 buffer 에 기록할 것인가"
		                        // true : Opaque, AlphaTest, 가까운게 이긴다.
		                        // 	이것을 끄면 무조건 통과
		                        // false : Skybox, Transparent ... 아니 왜?
		                        // 	비교는 했지만 ZBuffer에 기록 못한다. 안한다.
		                        // 		[Pass A] DepthWrite=false 로 draw -> color ⭕ 칠해짐, z 는 안 남음
		                        // 		[Pass B] draw -> A 의 z 가 없으므로 depth test 통과 ⭕
		                        //               -> B 의 color 가 *그 위에 덮어 칠해짐*

		GLenum DepthFunc = GL_LEQUAL; // GL_ALWAYS : UI/HUD, decal, gizmo 사용
		                              // 	혹은 단순 X-Ray 무시 (상시 Outline 표시 & Opaque에 가려져도 그려진다. )
		                              // GL_LESS/LEQUAL : 웬만해서 일반적인 사용
		                              // GL_EQUAL : 정확히 같은 z만
		                              // 	Albedo 가 적용된 z거리가 정확한 픽셀 딱 그것!
		                              //	Light 덕분에 적용된 약간 밝아진 오버라이딩 z거리가 정확한 픽셀 딱 그것!
		                              // 	2Pass : 등등 등거리 적용하고 싶은데 Z픽셀 딱 그것!
		                              // GL_NOTEQUAL : 다른 Z만, Outline "일명 Stencil로 사용하겠다는 선언" ❌
		                              // 	❌ Outline 은 보통 *Stencil* 의 NOTEQUAL 이 주력 (Depth 아님)  ❌
		                              // 	❌ 이 Func를 사용하면 지금부터 스텐실을 쓰겠습니다!   ❌
		                              // 	❌ 하고 상태를 말뚝 박는것과 동치  ❌
		                              // 	새 z != 기존 z 인 fragment 만 통과 — 합법이지만 실용성 낮음
		                              // 	float 정밀도 + z-fighting 때문에 "정확히 같다" 판정이 불안정
		                              // 	!! "Outline 의 NOTEQUAL" 은 *Stencil* 의 glStencilFunc(GL_NOTEQUAL, ref, mask) 이지
		                              // 	  glDepthFunc(GL_NOTEQUAL) 이 아니다. 같은 enum 이름의 다른 함수.
		                              // 	  Depth state 를 NOTEQUAL 로 바꿔도 Stencil test 는 자동으로 안 켜진다
		                              // 	  (Stencil 활성화는 glEnable(GL_STENCIL_TEST) 별도 호출 필요)
		                              // GL_GREATER  : 새 z > 기존 z
		                              // 	Reverse-Z 또는 X-Ray outline (벽 뒤에 가려진 곳만)
		                              //	진정한 X-Ray: GL_GREATER 로 가려진 부분만
		                              //		```cpp
		                              //		glDepthFunc(GL_GREATER);  // * 새 z > 기존 z
		                              // 		draw(outline);
		                              //		```
		                              //		GL_GREATER 의 의미를 풀어 보면:
		                              // 			"새 fragment 의 z (outline) > buffer 의 z (이미 그린 벽)"
		                              // 			= outline 이 벽보다 뒤에 있는 픽셀에서만 통과
		                              // 			= 벽에 가려진 부분만 그려짐
		                              //			wallhack
		                              // GL_GEQUAL   : 새 z ≥ 기존 z
		                              // 	Reverse-Z + multi-pass
		GLenum CullMode = GL_BACK;

		// Blend (BlendEnable=false 면 BlendSrc/Dst 무시)
		bool BlendEnable = false;
		GLenum BlendSrc = GL_SRC_ALPHA;
		GLenum BlendDst = GL_ONE_MINUS_SRC_ALPHA;

		// Sort/Queue
		int QueueLayer = 2000; ///< 작은 값 먼저. Unity: 2000 Opaque, 2450 AlphaTest, 3000 Transparent.

		// Stencil (StencilEnable=false 면 나머지 필드 무시 — Outline 등 특수 효과에만 사용)
		// LearnOpenGL Stencil Testing 정통:
		//    Pass 1 (stencil write): glStencilOp(KEEP, KEEP, REPLACE) + glStencilFunc(ALWAYS, 1, 0xFF) + WriteMask=0xFF
		//    Pass 2 (outline draw):  glStencilOp(KEEP, KEEP, KEEP)    + glStencilFunc(NOTEQUAL, 1, 0xFF) + WriteMask=0x00
		bool StencilEnable = false;
		GLenum StencilFunc = GL_ALWAYS;   ///< stencil 비교 — GL_NOTEQUAL 이 Outline 의 정통
		GLint StencilRef = 0;             ///< 비교 기준값
		GLuint StencilReadMask = 0xFFu;   ///< 비교 시 적용 마스크
		GLenum StencilOpSFail = GL_KEEP;  ///< stencil test 실패 시 동작
		GLenum StencilOpDPFail = GL_KEEP; ///< stencil pass + depth fail 시 동작
		GLenum StencilOpDPPass = GL_KEEP; ///< stencil + depth 모두 pass — GL_REPLACE 가 stencil write 정통
		GLuint StencilWriteMask = 0xFFu;  ///< stencil 쓰기 마스크 (0x00 = 읽기 전용)
	};

	/// @brief Pass::Kind -> 기본 PipelineState 매핑
	/// @note inline — 헤더 only. include 하는 모든 TU 가 각자 도출 (ODR 안전).
	inline PipelineState DefaultPipelineStateOf(const Kind k)
	{
		switch (k)
		{
		case Kind::StencilMaskWrite: {
			// Outline 2-pass — Pass 1 (stencil write):
			//   > Opaque 와 동일한 색상/깊이 렌더링 (물체 정상 표시)
			//   > StencilOpDPPass=GL_REPLACE + Ref=1 + WriteMask=0xFF — depth 통과 픽셀에 ref=1 기록
			//   > Queue 1999 — Opaque(2000) 직전 -> OutlineVisible(4000) 이 stencil 값에 의존 가능
			PipelineState s;
			s.QueueLayer = QueueOf(Kind::StencilMaskWrite);
			s.StencilEnable = true;
			s.StencilFunc = GL_ALWAYS; // 항상 stencil 통과 — 도장이 목적
			s.StencilRef = 1;
			s.StencilOpDPPass = GL_REPLACE; // depth 통과 시 ref=1 기록
			s.StencilWriteMask = 0xFFu;
			return s;
		}

		case Kind::Opaque:
			return PipelineState{
			    /*DepthTest*/ true, /*DepthWrite*/ true,
			    /*DepthFunc*/ GL_LEQUAL, /*CullMode*/ GL_BACK,
			    /*BlendEnable*/ false, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			    /*QueueLayer*/ QueueOf(Kind::Opaque)};

		case Kind::AlphaTest:
			// CullMode 0 = face culling 비활성 — sprite(빌보드)는 카메라-정면 2D quad 라 컬링이 무의미하고,
			// flipX(=-1, RIGHT 등 좌우반전)가 quad winding 을 뒤집어 GL_BACK 컬링 시 투명해지는 버그를 막는다.
			// (AlphaTest 는 현재 sprite 전용 — blast radius = 스프라이트만.)
			return PipelineState{
			    /*DepthTest*/ true, /*DepthWrite*/ true,
			    /*DepthFunc*/ GL_LEQUAL, /*CullMode*/ 0,
			    /*BlendEnable*/ false, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			    /*QueueLayer*/ QueueOf(Kind::AlphaTest)};

		case Kind::Skybox:
			// Skybox 정통:
			//  > DepthWrite off — skybox 가 depth 갱신하면 뒤 transparent 가 가려짐
			//  > DepthFunc LEQUAL — 셰이더의 .xyww 트릭으로 NDC z=1.0 강제 -> cleared depth(1.0) 와 동등 통과
			//  > CullMode FRONT — cube 안쪽에서 보기 때문에 *back face* 가 view 에 보임 -> front 컬링
			//  > Queue 2500 — Opaque 다음, Transparent 전 (z-cull 효율 우월)
			return PipelineState{
			    /*DepthTest*/ true, /*DepthWrite*/ false,
			    /*DepthFunc*/ GL_LEQUAL, /*CullMode*/ GL_FRONT,
			    /*BlendEnable*/ false, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			    /*QueueLayer*/ QueueOf(Kind::Skybox)};

		case Kind::Transparent:
			// Transparent 정통 (LearnOpenGL Blending / Unity Lit Transparent):
			//  > DepthWrite off — Transparent 들끼리 가리지 않도록
			//  > CullMode 0 (cull off) — *양면 그리기*. 유리창/잎사귀/의류 등 *두께 없는 면*
			//    이 카메라 어느 방향에서든 보이도록. GL_BACK 이면 plane 의 *뒷면* 이 culling 되어
			//    카메라가 plane 뒤로 갈 때 *안 보이는 버그*.
			return PipelineState{
			    /*DepthTest*/ true, /*DepthWrite*/ false,
			    /*DepthFunc*/ GL_LEQUAL, /*CullMode*/ 0, // * cull off — 양면 그리기
			    /*BlendEnable*/ true, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
			    /*QueueLayer*/ QueueOf(Kind::Transparent)};

		case Kind::OutlineVisible: {
			// Outline 정통 (LearnOpenGL Stencil testing — *outline draw pass*):
			//  > 전제: 직전에 *stencil write pass* 가 ref=1 을 객체 영역에 기록.
			//          -> 같은 mesh 를 Opaque 로 그리되 MeshRenderer override 로
			//             StencilOpDPPass=GL_REPLACE + WriteMask=0xFF + Ref=1 설정.
			//  > 이 패스는 *scale-up 한* 동일 mesh 를 그림 — 외곽선이 stencil != 1 인
			//    바깥 영역에만 나타나 윤곽선 효과.
			//  > DepthFunc=LEQUAL — 다른 Opaque 객체에 가려질 수 있음 (자연스러움)
			//  > DepthWrite=false — outline 자체가 z 영토 차지 X (위에 다른 객체 정상 그려짐)
			//  > StencilFunc=NOTEQUAL ref=1, WriteMask=0x00 — 읽기 전용 (stencil 갱신 안 함)
			PipelineState s;
			s.DepthWrite = false;
			s.QueueLayer = QueueOf(Kind::OutlineVisible);
			s.StencilEnable = true;
			s.StencilFunc = GL_NOTEQUAL;
			s.StencilRef = 1;
			s.StencilWriteMask = 0x00u;
			return s;
		}

		case Kind::OutlineXRay: {
			// X-Ray outline 월핵 효과
			//  > OutlineVisible 과 동일 stencil 설정, *DepthFunc 만* GL_GREATER
			//  > "새 z (outline) > 기존 z (이미 그린 벽)" -> outline 이 벽보다 *뒤에 있는*
			//    픽셀에서만 통과 = **벽에 가려진 부분만** 외곽선이 그려짐
			//  > 일반 OutlineVisible 과 *함께* 그리면 (Visible=밝게, XRay=어둡게) 하이브리드 효과
			PipelineState s;
			s.DepthWrite = false;
			s.DepthFunc = GL_GREATER; // * 핵심 — 가려진 곳만
			s.QueueLayer = QueueOf(Kind::OutlineXRay);
			s.StencilEnable = true;
			s.StencilFunc = GL_NOTEQUAL;
			s.StencilRef = 1;
			s.StencilWriteMask = 0x00u;
			return s;
		}
		}
		// unreachable — switch 가 enum 전부 커버.
		return PipelineState{};
	}

	/// @brief Queue layer 가 *Transparent 임계값* 이상인지. Sort 방향 분기에 사용.
	/// @details Unity TransparencySortMode 정통 — queue 2501 이상은 back-to-front.
	inline bool IsTransparentQueue(int queueLayer)
	{
		return queueLayer >= TRANSPARENT_THRESHOLD;
	}
} // namespace SJH::Pass

#endif // __SJH_PASS_H__
