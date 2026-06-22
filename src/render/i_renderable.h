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
	};
}
#endif
