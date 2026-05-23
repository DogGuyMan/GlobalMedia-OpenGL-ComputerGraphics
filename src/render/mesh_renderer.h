#ifndef __SJH_SCENE_COMPONENTS_H__
#define __SJH_SCENE_COMPONENTS_H__

#include "scene/actor.h"

namespace SJH
{
	class Mesh;
	class Material;
} // namespace SJH

namespace SJH::Scene
{
	/// @brief Unity MeshRenderer 식 통합 컴포넌트 — Mesh + Material + Visible + QueueOffset.
	/// @details
	///   SceneRenderer 가 이 컴포넌트를 수집 -> DrawCommand 빌드.
	///
	///   ### SP-MaterialSSoT — *Material 이 완전한 진실의 원천*
	///   GL state (Stencil/Depth/Cull/Blend) override 멤버 **모두 폐기**:
	///   - **이전**: MeshRenderer 에 `Stencil` / `DepthTest` / `DepthWrite` 부분 override (비대칭/모호)
	///   - **현재**: GL state 는 *오직 `Material::SetPass(Kind)`* 가 결정 — Unity/Unreal/Cocos 정통
	///   - **변형 사용**: `reg.CreateMaterialInstanceFrom(key, template)` 으로 *별도 Material 인스턴스* 생성
	///
	///   ### Queue 결정 모델 (Unity 정통, 직교 축)
	///   - **절대 queue** = `Material.PassKind` (Material 측 — "어떤 종류" 의도 선언)
	///   - **per-renderer 미세 조정** = `MeshRenderer.QueueOffset` (Renderer 측 — "같은 종류 내 순서")
	///   - 최종 queue = `Pass::QueueOf(material.PassKind, mr.QueueOffset)`
	///
	///   예:
	///   - Box: Material.SetPass(Opaque) + QueueOffset=0  -> 2000
	///   - Outline (Box 직후): MaterialInstance(SetPass(OutlineVisible)) + QueueOffset=5  -> 4005
	///   - Window: Material.SetPass(Transparent) + QueueOffset=0  -> 3000
	///   - Skybox: Material.SetPass(Skybox) + QueueOffset=0  -> 2500
	///
	///   Unity 매핑: `Material.renderQueue` ↔ Material.PassKind / `Renderer.sortingOrder` ↔ MeshRenderer.QueueOffset
	class MeshRenderer : public Component
	{
	  public:
		MeshRenderer() = default;
		/// @param queueOffset  Material.PassKind 의 queue 에 더해질 *정수 offset* (Unity Renderer.sortingOrder).
		///                     기본 0 = Material 의 queue 그대로 (정통 경로).
		///                     Outline 등 *같은 Pass 내 미세 순서* 필요 시 양수 (예: +5).
		MeshRenderer(SJH::Mesh *const mesh, SJH::Material *const material,
		             int queueOffset = 0)
		    : Mesh(mesh), Material(material), QueueOffset(queueOffset)
		{
		}

		virtual void OnEnter() override
		{
		}
		virtual void OnExit() override
		{
		}
		virtual void Update(float dt) override
		{
		}

		SJH::Mesh *const Mesh = nullptr;
		SJH::Material *const Material = nullptr;
		bool Visible = true;
		int QueueOffset = 0;
	};
} // namespace SJH::Scene

#endif // __SJH_SCENE_COMPONENTS_H__
