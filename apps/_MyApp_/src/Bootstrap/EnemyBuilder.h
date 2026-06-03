#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__

#include "Entity/Constants.h"
#include <vmath.h>

class b2World;
namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Bootstrap
{
    /// @brief BuildEnemy 입력 의존 (PlayerDeps 미러 — 비싱글턴만).
    struct EnemyDeps
    {
        b2World*           world        = nullptr;
        SJH::Scene::Actor* spawnParent  = nullptr;   ///< 적 child 부착 부모 (WaveController.mSpawnParent)
        SJH::Scene::Actor* playerTarget = nullptr;   ///< SimplePursueAI 추적 대상
        vmath::vec2        pos          = vmath::vec2(0.0f);
        int                hp           = Entity::ENEMY_HP;
        float              speed        = Entity::ENEMY_SPEED;
        int                damage       = Entity::ENEMY_DAMAGE;
        int                variant      = 0;          ///< 0~2 -> ENEMY_FRONT[variant % 3]
        float              spriteFps    = Entity::ENEMY_SPRITE_FPS;        ///< 2프레임 walk 애니 속도
        vmath::vec4        healthBarColor = vmath::vec4(1.0f, 0.15f, 0.12f, 1.0f); ///< 머리 위 체력바 채움 색 (기본 빨강 — 적 베리에이션)
    };

    /// @brief 적 1체 조립 — CreateEnemyActor + ENEMY_FRONT 스프라이트/애니 + spawnParent 부착.
    /// @return 트리 부착된 적 Actor* (실패 시 nullptr).
    SJH::Scene::Actor* BuildEnemy(const EnemyDeps& deps);
}

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__
