#include "Playable/PlayableDirector.h"

#include "scene/actor.h"             // Actor::GetOwner/SetActive
#include "sprite/sprite_component.h" // SJH::Sprite::SpriteRenderer (Component 완전형 — GetOwner 호출)

#include <utility> // std::move

namespace TopdownShooter::Playable
{
	PlayableDirector &PlayableDirector::Register(const std::string &key, std::unique_ptr<SJH::Playable::PlayableBase> p)
	{
		auto &slot   = mPlayables[key];
		slot.playable = std::move(p);
		slot.playing  = false; // 등록만 — Play(key) 전까지 tick 제외.
		return *this;
	}

	void PlayableDirector::Play(const std::string &key)
	{
		auto it = mPlayables.find(key);
		if (it == mPlayables.end() || !it->second.playable) return; // 미등록 = silent no-op
		it->second.playable->Stop(); // 리셋 (elapsed_=0, 자식 Stop)
		it->second.playable->Play();  // 첫 프레임부터 재생 (OnPlay 가 자식 spawn)
		it->second.playing = true;
	}

	void PlayableDirector::Stop(const std::string &key)
	{
		auto it = mPlayables.find(key);
		if (it == mPlayables.end() || !it->second.playable) return;
		it->second.playable->Stop();
		it->second.playing = false;
	}

	bool PlayableDirector::Has(const std::string &key) const
	{
		return mPlayables.count(key) != 0;
	}

	void PlayableDirector::Update(float dt)
	{
		if (!IsEnabled()) return;
		// 활성(Play 됨) + 미완료 슬롯만 직접 tick (Actor 미부착 — MultipleTimer 식 중앙 디스패치).
		for (auto &entry : mPlayables)
		{
			auto &slot = entry.second;
			if (!slot.playing || !slot.playable) continue;
			if (slot.playable->IsFinished())
			{
				slot.playing = false; // 완료 — 다음 Play(key) 까지 휴면.
				continue;
			}
			slot.playable->Update(dt);
		}
	}

	void PlayableDirector::SetFacing(Entity::EFacing f)
	{
		if (f == mFacing) return; // velocity 안 읽음 — 계산은 PlayerController(RD5)
		mFacing = f;
		Apply();
	}

	void PlayableDirector::SetPose(Entity::EPose p)
	{
		if (p == mPose) return;
		mPose = p;
		Apply();
	}

	void PlayableDirector::Apply()
	{
		// (mFacing,mPose) 그룹만 활성. SetActive 토글 → 렌더+tick 동시 게이트(2프레임 애니라 freeze 무차별).
		for (int f = 0; f < 4; ++f)
			for (int p = 0; p < 2; ++p)
			{
				const bool active = (f == idx(mFacing) && p == idx(mPose));
				for (auto *layer : mGroups[f][p].layers)
					if (layer && layer->GetOwner())
						layer->GetOwner()->SetActive(active);
			}
	}

	void PlayableDirector::ReactDied(vmath::vec3 /*pos*/)
	{
		// 역할별 분리(P2): director 는 순수 [C]. 화면/엔티티 death 연출은 등록된 "death" Playable.
		// 월드점 death FX[B](폭발 등)는 도메인 seam(Life::mOnDeathFx / 적 seam)이 Spawns/ 로 직접 트리거.
		Play("death"); // 미등록이면 silent no-op (death Playable 은 병렬 트랙이 등록).
	}
}
