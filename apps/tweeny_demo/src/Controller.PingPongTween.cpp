
#include "Controller.PingPongTween.h"
#include <cstdint>

namespace TweenyDemo::Controller
{
	void PingPongTween::Update(float dt)
	{
		int32_t dtMs = static_cast<int32_t>(dt * 1000.0f);
		if (dtMs < 0)
			dtMs = 0;

		const float v = mTween.step(dtMs);
		if (mTween.progress() >= 1.0f)
			mTween.seek(0);

		if (auto *owner = GetOwner())
			owner->GetTransform().Translate[mAxis] = v;
	}
}; // namespace TweenyDemo::Controller
