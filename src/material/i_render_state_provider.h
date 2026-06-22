/**
 * @file i_render_state_provider.h
 * @brief RenderStateBlock(ROP) 조회 Facade - Material(저장)/IRenderable(위임) 구현 (D7). D9 중립 fallback.
 */
#ifndef __SJH_I_RENDER_STATE_PROVIDER_H__
#define __SJH_I_RENDER_STATE_PROVIDER_H__
// GL 격리: pass.h(-> GL/gl3w.h) 를 include 하지 않는다. 반환이 const& 라 전방선언으로 충분.
// (pass.h 를 include 하면 이 인터페이스를 거쳐 Effekseer 클라이언트까지 gl3w.h 가 전파돼 gl3.h 와 충돌.)
// 실제 RenderStateBlock 을 *값/멤버* 로 쓰는 곳(material.h 등)은 각자 material/pass.h 를 include 한다.
namespace SJH::Pass
{
	struct RenderStateBlock;
}
namespace SJH
{
	class IRenderStateProvider
	{
	  public:
		virtual ~IRenderStateProvider() = default;
		virtual const Pass::RenderStateBlock &GetRenderStateBlock() const = 0;
	};
}
#endif
