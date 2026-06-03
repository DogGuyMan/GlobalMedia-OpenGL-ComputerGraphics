#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_ENTITY_PRESENTATION_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_ENTITY_PRESENTATION_H__

#include <vmath.h>

// fwd-decl — 헤더 표면 최소화 (완전형은 .cpp 에서 해소).
namespace SJH::Scene
{
	class Actor;
}
namespace TopdownShooter::Playable
{
	class PlayableDirector;
}

namespace TopdownShooter::Bootstrap
{
	/// @brief Player/Enemy 공통 연출 배선 config. 기본값 = Player 값(dissolve/delay 1.5, 체력바 녹색).
	///        Enemy 는 세 필드 모두 override(0.6 / 0.6 / deps 색).
	struct EntityPresentationConfig
	{
		float       dissolveSeconds   = 1.5f;                                 // "death" SpriteDissolve 길이
		float       deathDelaySeconds = 1.5f;                                 // 사망 후 비활성 지연(=dissolve 가시화 창)
		vmath::vec4 healthBarColor    = vmath::vec4(0.13f, 1.0f, 0.0f, 1.0f); // 체력바 채움색(기본 녹색 = HealthBarConfig 기본과 동일)
	};

	/// @brief Player/Enemy 공통 연출 클러스터 부착 — *pre-entry*(AddChild 전) 호출.
	/// @details 두 빌더의 🟢 공통 5동작 dedup:
	///   ① PlayableDirector 부착 (= Life 의 IActorPresentation sink — entry 전이라야 Life::OnEnter 가 캐시).
	///   ② "hit"=SpriteHitFlash / "death"=SpriteDissolve(dissolveSeconds) 기본 등록.
	///      (Player 는 이후 "hit" 을 Parallel 로 overwrite — Register 동일키 덮어쓰기. Enemy 는 그대로.)
	///   ③ Life::SetDeathDelaySeconds(deathDelaySeconds) — Life 있을 때만.
	///   ④ AttachHealthBar(healthBarColor).
	/// @return 부착된 director (caller 가 "fire"/"hit"-overwrite 등 추가 Register 가능).
	Playable::PlayableDirector *AttachEntityPresentation(
	    SJH::Scene::Actor &actor, const EntityPresentationConfig &cfg = {});
} // namespace TopdownShooter::Bootstrap

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_ENTITY_PRESENTATION_H__
