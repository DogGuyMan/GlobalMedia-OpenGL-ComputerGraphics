/**
 * @file mesh_renderer.h
 * @brief Unity MeshRenderer 식 통합 컴포넌트 - Mesh + Material + Visible + QueueOffset.
 *
 * @details
 *  ### 책임
 *  - SceneRenderer 가 이 컴포넌트를 수집해 DrawCommand 를 빌드.
 *  - @c Mesh (지오메트리) + @c Material (셰이더 + GL state) + @c Visible (렌더 여부) 를 한 컴포넌트로 묶음.
 *  - @c QueueOffset 으로 같은 PassKind 내 *미세 렌더 순서* 조정.
 *
 *  ### 비-책임
 *  - [X] GL state (Stencil/Depth/Cull/Blend) 직접 override - SP-MaterialSSoT 원칙.
 *    GL state 는 *오직* @c Material::SetPass(Kind) 가 결정 (Unity/Unreal/Cocos 정통).
 *  - [X] 변형 사용 시 Material 복사 - @c ResourceRegistry::CreateMaterialInstanceFrom() 으로 인스턴스 생성.
 *
 *  ### Queue 결정 모델 (Unity 정통, 직교 축)
 *  - **절대 queue** = @c Material.PassKind (Material 측 - "어떤 종류" 의도 선언)
 *  - **per-renderer 미세 조정** = @c MeshRenderer.QueueOffset (Renderer 측 - "같은 종류 내 순서")
 *  - 최종 queue = @c Pass::QueueOf(material.PassKind, mr.QueueOffset)
 *
 *  예:
 *  - Box: @c Material.SetPass(Opaque) + @c QueueOffset=0  -> 2000
 *  - Outline (Box 직후): @c MaterialInstance(SetPass(OutlineVisible)) + @c QueueOffset=5  -> 4005
 *  - Window: @c Material.SetPass(Transparent) + @c QueueOffset=0  -> 3000
 *  - Skybox: @c Material.SetPass(Skybox) + @c QueueOffset=0  -> 2500
 *
 *  Unity 매핑: @c Material.renderQueue <-> @c Material.PassKind / @c Renderer.sortingOrder <-> @c MeshRenderer.QueueOffset
 *
 * @note 이전에 존재하던 @c Stencil / @c DepthTest / @c DepthWrite override 멤버는 SP-MaterialSSoT 에서 **모두 폐기**.
 */
#ifndef __SJH_SCENE_COMPONENTS_H__
#define __SJH_SCENE_COMPONENTS_H__

#include "scene/actor.h"
#include "render/i_renderable.h"
#include "material/material.h"

namespace SJH
{
	class Mesh;
} // namespace SJH

namespace SJH::Scene
{
	/**
	 * @brief Unity @c MeshRenderer 식 통합 컴포넌트 - Mesh + Material + Visible + QueueOffset.
	 * @details
	 *  SceneRenderer 가 씬 그래프에서 이 컴포넌트를 수집해 @c DrawCommand 를 빌드.
	 *  GL state 는 @c Material::SetPass(Kind) 가 단독으로 결정 (SP-MaterialSSoT).
	 *  같은 PassKind 내 렌더 순서가 필요할 때 @c QueueOffset 사용.
	 *
	 *  Task 2.3: IRenderable 구현 - 잎 자가발행. RenderableProcessor 가 소비하는 건 다음 Task.
	 *  다중상속 다이아몬드 없음 - Component 는 IRenderStateProvider 를 상속하지 않음.
	 */
	class MeshRenderer : public Component, public IRenderable
	{
	  public:
		/// @brief 기본 생성자 - Mesh/Material null, Visible=true, QueueOffset=0.
		MeshRenderer() = default;

		/// @brief Mesh + Material + 선택적 QueueOffset 주입 생성자.
		/// @param mesh        렌더할 지오메트리 (비소유). @c nullptr 허용 (SceneRenderer 가 skip).
		/// @param material    셰이더 + GL state 담당 머티리얼 (비소유). @c nullptr 허용.
		/// @param queueOffset Material.PassKind 의 queue 에 더해질 정수 offset (Unity @c Renderer.sortingOrder).
		///                    기본 @c 0 = Material 의 queue 그대로 (정통 경로).
		///                    Outline 등 *같은 Pass 내 미세 순서* 필요 시 양수 (예: @c +5).
		MeshRenderer(SJH::Mesh *const mesh, SJH::Material *const material,
		             int queueOffset = 0)
		    : Mesh(mesh), Material(material), QueueOffset(queueOffset)
		{
		}

		virtual void OnEnter() override {}
		virtual void OnExit() override  {}
		virtual void Update(float /*dt*/) override {}

		/// @brief D7 Facade 위임 - Material 저장처. null 이면 중립(D9).
		const Pass::RenderStateBlock &GetRenderStateBlock() const override
		{
			static const Pass::RenderStateBlock kNeutral{};
			return Material ? Material->GetRenderStateBlock() : kNeutral;
		}

		/// @brief Material PassKind + QueueOffset 으로 draw sort 우선순위 도출.
		int QueueLayer() const override
		{
			return Material ? Pass::QueueOf(Material->GetPass(), QueueOffset) : QueueOffset;
		}

		/// @brief 순수 material RenderQueue(offset 미포함) - 필터/분류용. Material null 이면 Opaque.
		int RenderQueue() const override
		{
			return Material ? static_cast<int>(Material->GetPass())
			                : static_cast<int>(Pass::RenderQueue::Opaque);
		}

		/// @brief painter 층 offset 노출 (RenderQueue 와 분리 - Sort tiebreak 리팩토링 대비).
		int DrawOrder() const override { return QueueOffset; }

		/// @brief 잎 자가발행 draw - DeviceContext + Camera 로 mesh+material 을 GL 에 전송.
		/// @details dormant(미호출) - RenderableProcessor::Process 가 소비하는 건 다음 Task.
		void Render(DeviceContext &rec, const Camera &cam) const override;

		SJH::Mesh     *const Mesh     = nullptr; ///< 렌더 지오메트리 (비소유). nullptr 시 SceneRenderer skip.
		SJH::Material *const Material = nullptr; ///< 셰이더 + GL state 진실의 원천 (비소유). nullptr 시 skip.
		bool Visible      = true; ///< false 시 SceneRenderer DrawCommand 제외.
		int  QueueOffset  = 0;    ///< Material.PassKind queue 에 더해지는 미세 정렬 offset. 기본 0.
	};
} // namespace SJH::Scene

#endif // __SJH_SCENE_COMPONENTS_H__
