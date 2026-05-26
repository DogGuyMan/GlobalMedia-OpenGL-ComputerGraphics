#ifndef __MYAPP_POSTFX_DEBUG_LAYER_H__
#define __MYAPP_POSTFX_DEBUG_LAYER_H__

#include "UI/IImGuiLayer.h"
#include "render/pass_component.h"
#include <string>
#include <vector>

namespace TopdownShooter::UI
{
	struct PassDebugEntry
	{
		std::string                Name;
		SJH::Scene::PassComponent *Component;  ///< 비소유
	};

	class PostFXDebugLayer : public IImGuiLayer
	{
	  public:
		PostFXDebugLayer(std::vector<PassDebugEntry> passes, float &gamma);

		ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Editor; }
		void           OnBuildUI() override;

	  private:
		std::vector<PassDebugEntry> mPasses;
		float                      &mGamma;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_POSTFX_DEBUG_LAYER_H__
