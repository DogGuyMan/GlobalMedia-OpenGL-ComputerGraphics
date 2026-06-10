/**
 * @file EntityPresentation.cpp
 * @brief @c AttachEntityPresentation 구현 - Player/Enemy 공통 연출 클러스터 부착.
 * @details 4단계 순서로 부착: (1) PlayableDirector(=Life sink) -> (2) "hit"/"death" 기본 Playable
 *          등록 -> (3) Life::SetDeathDelaySeconds(Life 있을 때만) -> (4) 머리 위 체력바.
 *          전부 pre-entry 라야 Life::OnEnter 가 director 를 캐시하고 자식들이 함께 OnEnter 한다.
 */
#include <GL/gl3w.h> // 최상단 - resource_registry/scene 헤더가 끌어오는 gl3.h 보다 먼저.

#include "Bootstrap/EntityPresentation.h"

#include "Entity/Components/LifeComponents.h" // Components::Life - SetDeathDelaySeconds
#include "HUD/HealthBarFactory.h"             // AttachHealthBar + HealthBarConfig
#include "Playable/PlayableDirector.h"        // 부착 + Register (Component)
#include "Playable/SpriteFxPlayable.h"        // SpriteHitFlashPlayable / SpriteDissolvePlayable
#include "scene/actor.h"

#include <memory> // std::make_unique

namespace TopdownShooter::Bootstrap
{
	Playable::PlayableDirector *AttachEntityPresentation(
	    SJH::Scene::Actor &actor, const EntityPresentationConfig &cfg)
	{
		// (1) director 부착 - Life 의 IActorPresentation sink (AddChild 전이라야 Life::OnEnter 가 캐시).
		auto *director = actor.AddComponent<Playable::PlayableDirector>();

		// (2) 기본 연출 등록 - "hit" 은 Player 가 Parallel 로 overwrite, Enemy 는 그대로.
		director->Register("hit", std::make_unique<Playable::SpriteHitFlashPlayable>(&actor));
		director->Register("death", std::make_unique<Playable::SpriteDissolvePlayable>(&actor, cfg.dissolveSeconds));

		// (3) 사망 후 비활성 지연 - dissolve 가시화 창 (Life 있을 때만).
		if (auto *life = actor.GetComponent<Entity::Components::Life>())
			life->SetDeathDelaySeconds(cfg.deathDelaySeconds);

		// (4) 머리 위 분절형 체력바 - pre-entry(entry 시 자식과 함께 OnEnter). 색만 cfg, 나머지 HealthBarConfig 기본.
		HUD::HealthBarConfig barCfg;
		barCfg.fillColor = cfg.healthBarColor;
		HUD::AttachHealthBar(actor, barCfg);

		return director;
	}
} // namespace TopdownShooter::Bootstrap
