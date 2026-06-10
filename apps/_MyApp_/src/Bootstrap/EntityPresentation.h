/**
 * @file EntityPresentation.h
 * @brief Player/Enemy 두 빌더가 공유하는 연출 클러스터 부착 헬퍼 + config struct.
 *
 * @details
 *  ### 책임
 *  - 두 빌더의 공통 5동작(director 부착 / "hit"+"death" 기본 등록 / death delay / 체력바)을 dedup.
 *  - @c EntityPresentationConfig 로 dissolve/delay/체력바 색 차이만 파라미터화 (기본=Player 값).
 *
 *  ### 비-책임([X])
 *  - [X] "hit" overwrite (Player 비네팅 / Enemy 사운드) - 반환된 director 로 caller 가 추가 Register.
 *  - [X] 스프라이트/물리 조립 - 각 빌더가 별도로 부착 (본 헬퍼는 연출 레이어만).
 *
 *  ### 정통 매핑
 *  - Unity 의 공통 prefab 변형 베이스 - 차이 필드만 override 하는 config 주입 패턴.
 *
 * @note 반드시 *pre-entry*(AddChild 전) 호출 - Life::OnEnter 가 director 를 IActorPresentation
 *       sink 로 캐시하려면 director 가 entry 시점에 이미 부착돼 있어야 한다.
 */
#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_ENTITY_PRESENTATION_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_ENTITY_PRESENTATION_H__

#include <vmath.h>
#include "Bootstrap/Constants.h"
#include "HUD/Constants.h"

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
		float       dissolveSeconds   = PLAYER_DISSOLVE_SECONDS;  // "death" SpriteDissolve 길이
		float       deathDelaySeconds = PLAYER_DEATH_DELAY;       // 사망 후 비활성 지연(=dissolve 가시화 창)
		vmath::vec4 healthBarColor    = HUD::HEALTHBAR_FILL_COLOR; // 체력바 채움색(녹색 — HUD 단일 소스, C3)
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
