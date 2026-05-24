#ifndef __TWEENY_DEMO_CONTROLLER_PING_PONG_TWEEN_H__
#define __TWEENY_DEMO_CONTROLLER_PING_PONG_TWEEN_H__

#include <tweeny/tweeny.h>
#include <utility>
#include "scene/actor.h"

namespace TweenyDemo::Controller
{
	class PingPongTween : public SJH::Scene::Component
	{
	  public:
		PingPongTween() = default;

		template <typename Easing>
		PingPongTween &SetPingPong(float from, float to, int durationMs, Easing &&easing)
		{
			mTween = tweeny::from(from)
			             .to(to)
			             .during(durationMs)
			             .via(std::forward<Easing>(easing))
			             .to(from)
			             .during(durationMs)
			             .via(std::forward<Easing>(easing));
			return *this;
		}

		PingPongTween &SetAxis(int axis) {mAxis = axis; return *this;}

		void OnEnter() override
		{
		}
		void OnExit() override
		{
		}
		void Update(float dt) override;

	  private:
		tweeny::tween<float> mTween;
		int mAxis = 0;
	};
}; // namespace TweenyDemo::Controller

#endif //__TWEENY_DEMO_CONTROLLER_PING_PONG_TWEEN_H__