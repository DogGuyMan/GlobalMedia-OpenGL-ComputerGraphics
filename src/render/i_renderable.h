/**
 * @file i_renderable.h
 * @brief Pass 안 함께 정렬되는 drawable 순수 추상(mesh-family, D5). LSP-최소 3. 고수준은 구체 DeviceContext(D10).
 */
#ifndef __SJH_I_RENDERABLE_H__
#define __SJH_I_RENDERABLE_H__
#include "material/i_render_state_provider.h"
namespace SJH { class DeviceContext; }
namespace SJH::Scene { class Camera; }
namespace SJH
{
	class IRenderable : public IRenderStateProvider
	{
	  public:
		virtual void Render(DeviceContext &rec, const Scene::Camera &cam) const = 0;
		virtual int  QueueLayer() const = 0;

		/// @brief 순수 material RenderQueue (QueueOffset 미포함) - 골든 per-queue 필터/분류용.
		/// @details QueueLayer() = RenderQueue() + DrawOrder() 관계. 필터는 이 순수값으로 해야
		///          음수 DrawOrder 레이어(플레이어 -1/-2/-3)가 인접 큐로 새지 않는다.
		virtual int  RenderQueue() const = 0;
		/// @brief actor 내부 painter 층(= MeshRenderer::QueueOffset). RenderQueue 와 분리 노출.
		virtual int  DrawOrder() const = 0;
	};
}
#endif
