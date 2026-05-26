#ifndef __MYAPP_POSTFX_DEBUG_LAYER_H__
#define __MYAPP_POSTFX_DEBUG_LAYER_H__

#include "UI/IImGuiLayer.h"
#include "render/postfx_pass.h"
#include <vector>

namespace SJH
{
class SceneRenderer;
class Mesh;
} // namespace SJH

namespace TopdownShooter::UI
{
	class PostFXDebugLayer : public IImGuiLayer
	{
	  public:
		/// @param renderer  SetPostFXChain 재전달 대상
		/// @param passes    Enabled 토글 대상 (game_application 소유 vector)
		/// @param quadMesh  ref-to-ptr — BuildPostFXChain 이후 설정되므로 참조로 바인딩
		/// @param gamma     gamma.fs uniform 실시간 연동
		PostFXDebugLayer(SJH::SceneRenderer          &renderer,
		                 std::vector<SJH::PostFXPass> &passes,
		                 SJH::Mesh                   *&quadMesh,
		                 float                        &gamma);

		ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Editor; }
		void           OnBuildUI() override;

	  private:
		SJH::SceneRenderer           &mRenderer;
		std::vector<SJH::PostFXPass> &mPasses;
		SJH::Mesh                   *&mQuadMesh;
		float                        &mGamma;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_POSTFX_DEBUG_LAYER_H__
