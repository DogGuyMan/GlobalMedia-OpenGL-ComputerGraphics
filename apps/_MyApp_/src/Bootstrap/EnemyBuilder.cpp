#include <GL/gl3w.h> // 최상단 — resource_registry.h→framebuffer.h→...→gl3w.h 보다 먼저.

#include "Bootstrap/EnemyBuilder.h"

#include "Entity/Enemy/EnemyFactory.h"   // CreateEnemyActor / EnemyConfig (+box2d)
#include "Playable/Constants.h"           // ENEMY_FRONT
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_frame_clip.h"
#include "sprite/sprite_sequence_playable.h"

#include <spdlog/spdlog.h>
#include <utility> // std::move

namespace TopdownShooter::Bootstrap
{
    SJH::Scene::Actor* BuildEnemy(const EnemyDeps& deps)
    {
        // 1) 물리+Life+AI+contact = 기존 factory (무변경)
        Entity::Enemy::EnemyConfig cfg;
        cfg.world        = deps.world;
        cfg.pos          = deps.pos;
        cfg.playerTarget = deps.playerTarget;
        cfg.hp           = deps.hp;
        cfg.speed        = deps.speed;
        cfg.damage       = deps.damage;
        cfg.onDeathFx    = deps.onDeathFx;
        auto enemy = Entity::Enemy::CreateEnemyActor(cfg);   // unique_ptr<Actor> (미부착)

        // 2) ENEMY_FRONT[variant] 스프라이트 + 2프레임 애니 (owner-direct, 단일 레이어)
        const auto& tex = Playable::ENEMY_FRONT[deps.variant % 3];
        auto& reg = SJH::ResourceRegistry::Get();
        auto* atlas = reg.FindUniformAtlas(tex.TexturePath);
        if (!atlas)
            atlas = reg.CreateUniformAtlas(tex.TexturePath, tex.TexturePath, tex.ColCount, tex.RowCount);
        if (atlas)
        {
            auto* spr = enemy->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
            spr->flipX = tex.Flip;
            if (tex.ColCount > 1)   // 애니 (2프레임 walk)
            {
                auto* seq = enemy->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
                    spr, SJH::SpriteSequence::SpriteFrameClip{0, tex.ColCount, deps.spriteFps});
                seq->SetIsLoop(true);
                seq->Play();
            }
        }
        else
        {
            spdlog::error("[enemy] atlas load 실패: {}", tex.TexturePath);
        }

        // 3) 씬 트리 부착 (entry → 컴포넌트 OnEnter 캐스케이드)
        if (!deps.spawnParent) return nullptr;
        return deps.spawnParent->AddChild(std::move(enemy));
    }
}
