#include "Playable/SpriteFxPlayable.h"

#include "resource_registry/image.h"             // SJH::Image::Load (dissolve.png)
#include "resource_registry/resource_registry.h" // ResourceRegistry — dissolve 노이즈 텍스처 1회 캐시
#include "scene/actor.h"             // SJH::Scene::Actor (GetChildren/GetComponent)
#include "sprite/sprite_component.h" // SJH::Sprite::SpriteRenderer (enableHit/dissolve)

namespace TopdownShooter::Playable
{
	namespace
	{
		// 디졸브 노이즈 텍스처 — 모든 dissolve 가 공유 (ResourceRegistry 키, 1회 로드 후 캐시).
		constexpr const char *kDissolveTexKey = "_dissolve_noise";

		// 대상 액터 자신 + 직속 자식의 SpriteRenderer 에 fn 적용.
		// (player = 4-레이어 자식 / enemy = 자기 또는 자식 — 둘 다 커버, P4 재사용.)
		template <typename Fn>
		void ForEachSpriteRenderer(SJH::Scene::Actor *root, Fn &&fn)
		{
			if (!root) return;
			if (auto *r = root->GetComponent<SJH::Sprite::SpriteRenderer>()) fn(r);
			for (const auto &child : root->GetChildren())
				if (auto *r = child->GetComponent<SJH::Sprite::SpriteRenderer>()) fn(r);
		}
	}

	// ─────────────── SpriteHitFlashPlayable ───────────────
	SpriteHitFlashPlayable::SpriteHitFlashPlayable(SJH::Scene::Actor *target, float durationSec)
	    : mTarget(target), mDuration(durationSec)
	{
	}
	SpriteHitFlashPlayable::~SpriteHitFlashPlayable() = default;

	void SpriteHitFlashPlayable::OnPlay()
	{
		ForEachSpriteRenderer(mTarget, [](SJH::Sprite::SpriteRenderer *r) { r->enableHit = true; });
	}
	void SpriteHitFlashPlayable::OnStop()
	{
		ForEachSpriteRenderer(mTarget, [](SJH::Sprite::SpriteRenderer *r) { r->enableHit = false; });
	}
	void SpriteHitFlashPlayable::OnUpdate(float /*dt*/)
	{
		// elapsed_ 는 PlayableBase 가 매 Update 누적. duration 경과 시 플래시 해제 + one-shot 종료.
		if (elapsed_ >= mDuration)
		{
			ForEachSpriteRenderer(mTarget, [](SJH::Sprite::SpriteRenderer *r) { r->enableHit = false; });
			finished_ = true;
		}
	}

	// ─────────────── SpriteDissolvePlayable ───────────────
	SpriteDissolvePlayable::SpriteDissolvePlayable(SJH::Scene::Actor *target, float durationSec)
	    : mTarget(target), mDuration(durationSec)
	{
	}
	SpriteDissolvePlayable::~SpriteDissolvePlayable() = default;

	void SpriteDissolvePlayable::OnPlay()
	{
		// 노이즈 디졸브 텍스처(dissolve.png) 1회 로드(이후 FindTexture 캐시). 미지정 시 sprite_component 이
		// uDissolveTex 를 unit0(=atlas)로 fallback -> 스프라이트 내용 의존 crude erode(가시성 불안정, 적이 즉시 사라짐).
		// 전용 노이즈로 내용 무관 균일·가시 디졸브 보장 (sprite_component 주석의 "dissolve.png sink 주입" 의도).
		auto               &reg   = SJH::ResourceRegistry::Get();
		const SJH::Texture *noise = reg.FindTexture(kDissolveTexKey);
		if (!noise)
			noise = reg.CreateTexture(kDissolveTexKey,
			                          SJH::Image::Load(kDissolveTexKey, "resources/texture/dissolve.png").get());

		ForEachSpriteRenderer(mTarget, [noise](SJH::Sprite::SpriteRenderer *r) {
			r->enableDissolve    = true;
			r->dissolveThreshold = 0.0f;
			r->dissolveTex       = noise; // 노이즈 디졸브 (nullptr 면 atlas fallback)
		});
	}
	void SpriteDissolvePlayable::OnStop()
	{
		ForEachSpriteRenderer(mTarget, [](SJH::Sprite::SpriteRenderer *r) {
			r->enableDissolve    = false;
			r->dissolveThreshold = 0.0f;
		});
	}
	void SpriteDissolvePlayable::OnUpdate(float /*dt*/)
	{
		float t = (mDuration > 0.0f) ? (elapsed_ / mDuration) : 1.0f;
		if (t > 1.0f) t = 1.0f;
		ForEachSpriteRenderer(mTarget, [t](SJH::Sprite::SpriteRenderer *r) { r->dissolveThreshold = t; });
		if (t >= 1.0f) finished_ = true; // dissolved 상태 유지 (액터는 Life 가 비활성/despawn).
	}
}
