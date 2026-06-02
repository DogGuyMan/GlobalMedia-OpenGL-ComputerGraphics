#ifndef __TOPDOWNSHOOTER_PLAYABLE_SPRITE_FX_PLAYABLE_H__
#define __TOPDOWNSHOOTER_PLAYABLE_SPRITE_FX_PLAYABLE_H__

#include "playable/playable_base.h"

// fwd — 대상 액터는 포인터만 보유 (자식 SpriteRenderer 를 .cpp 에서 수집).
namespace SJH::Scene
{
	class Actor;
}

namespace TopdownShooter::Playable
{
	/// @brief [C] 피격 hit-flash leaf Playable — 대상 액터(+자식) 의 모든 SpriteRenderer.enableHit 를
	///        @p durationSec 동안 true 로 (셰이더 billboard_atlas 가 자체 uTime 클럭으로 플래시 애니).
	///        one-shot. PlayableDirector "hit" 컴포지트에 합성. player/enemy 공용(target 만 바꿔 재사용 — P4).
	class SpriteHitFlashPlayable : public SJH::Playable::PlayableBase
	{
	  public:
		explicit SpriteHitFlashPlayable(SJH::Scene::Actor *target, float durationSec = 0.18f);
		~SpriteHitFlashPlayable() override;

	  protected:
		void OnPlay() override;           // enableHit = true (대상 서브트리 전체)
		void OnStop() override;           // enableHit = false (리셋)
		void OnUpdate(float dt) override; // durationSec 경과 → enableHit=false + finished_

	  private:
		SJH::Scene::Actor *mTarget;
		float              mDuration;
	};

	/// @brief [C] 사망 dissolve leaf Playable — 대상 액터(+자식) SpriteRenderer 의 enableDissolve=true +
	///        dissolveThreshold 를 0→1 로 @p durationSec 에 걸쳐 선형 구동(사라짐). one-shot — 완료 후 dissolved 유지.
	///        PlayableDirector "death" 컴포지트에 합성. player/enemy 공용(P4 재사용).
	class SpriteDissolvePlayable : public SJH::Playable::PlayableBase
	{
	  public:
		explicit SpriteDissolvePlayable(SJH::Scene::Actor *target, float durationSec = 1.0f);
		~SpriteDissolvePlayable() override;

	  protected:
		void OnPlay() override;           // enableDissolve=true, threshold=0
		void OnStop() override;           // enableDissolve=false, threshold=0 (리셋)
		void OnUpdate(float dt) override; // threshold = clamp(elapsed/duration) → 1 시 finished_

	  private:
		SJH::Scene::Actor *mTarget;
		float              mDuration;
	};
}

#endif // __TOPDOWNSHOOTER_PLAYABLE_SPRITE_FX_PLAYABLE_H__
