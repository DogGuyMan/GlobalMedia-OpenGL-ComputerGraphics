#include "Playable/PostFXTweenPlayable.h"

#include "Playable/PostFXRegistry.h"
#include "material/material.h" // SJH::Material::Properties.Floats

#include <cstdint>
#include <utility>

namespace TopdownShooter::Playable
{
	PostFXTweenPlayable::PostFXTweenPlayable(std::string passName, std::string uniformName, tweeny::tween<float> tween)
	    : mPassName(std::move(passName)), mUniformName(std::move(uniformName)), mTween(std::move(tween))
	{
	}

	PostFXTweenPlayable::~PostFXTweenPlayable() = default;

	void PostFXTweenPlayable::OnPlay()
	{
		mTween.seek(0.0f); // 매 Play 마다 트윈을 처음으로 — Stop()+Play() 재트리거 시 깔끔히 재생.
	}

	void PostFXTweenPlayable::OnUpdate(float dt)
	{
		const int32_t dtMs  = static_cast<int32_t>(dt * 1000.0f); // !! ms 오버로드 (float 금지)
		const float   value = mTween.step(dtMs);

		// pass Material 은 매 프레임 조회 — 없으면(미등록/파괴) 조용히 무시.
		if (auto *mat = PostFXRegistry::Get().Material(mPassName))
			mat->Properties.Floats[mUniformName] = value;

		if (mTween.progress() >= 1.0f)
			mIsFinished = true; // one-shot — Composite 안에서 1회 재생 후 종료.
	}
}
