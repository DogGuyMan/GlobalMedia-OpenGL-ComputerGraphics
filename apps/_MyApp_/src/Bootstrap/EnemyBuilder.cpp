#include <GL/gl3w.h> // 최상단 — resource_registry.h->framebuffer.h->...->gl3w.h 보다 먼저.

#include "Bootstrap/EnemyBuilder.h"

#include "Bootstrap/EntityPresentation.h"  // AttachEntityPresentation — Player/Enemy 공통 연출 클러스터
#include "Entity/Enemy/EnemyFactory.h"   // CreateEnemyActor / EnemyConfig (+box2d)
#include "Playable/Constants.h"           // ENEMY_FRONT
#include "Playable/SpriteLayerFactory.h"  // AttachSpriteLayer — 3-빌더 공유 sprite-layer 부착 헬퍼
#include "Tween/TweenPlayable.h"          // 상시 루프 트윈
#include "playable/composite_playable.h"  // SJH::Playable::ParallelPlayable (동시재생 컨테이너)
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "Bootstrap/Constants.h"           // ENEMY_DISSOLVE/DELAY/트윈MS

#include <tweeny/tweeny.h>                 // tweeny::from / easing (tween 정의)
#include <vmath.h>                         // vmath::vec3 (Transform.Scale)

#include <memory>  // std::make_unique (Join 자식 생성)
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

        // 2) ENEMY_FRONT[variant] 스프라이트 + 2프레임 애니 — owner-direct(child 없음, 단일 레이어).
        //    공유 헬퍼로 atlas+SpriteRenderer(+애니) 부착. QueueOffset=DrawOrder(=0, 기본값과 동일·무해).
        const auto& tex = Playable::ENEMY_FRONT[deps.variant % 3];
        auto& reg = SJH::ResourceRegistry::Get();
        Playable::AttachSpriteLayer(*enemy, reg, tex, deps.spriteFps);

        // 4) 상시 루프 트윈 — ParallelPlayable 로 *동시재생* (스케일 펄스 ∥ z축 회전 워블).
        //    composite 모듈(SJH::Playable::ParallelPlayable, EngineAPI.md §ParallelPlayable)을 컨테이너로 쓰고
        //    각 child 가 스스로 PingPong 루프한다.
        //    !! 왜 par.SetIsLoop 이 아니라 child.SetIsLoop 인가:
        //       ParallelPlayable 의 loop 재시작은 child->Play() 만 호출하는데, TweenPlayable.Play()(=PlayableBase)
        //       는 tweeny progress 를 되감지 않는다 -> "one-shot child + par 루프" 는 끝값에 고정(깨짐).
        //       그래서 child 를 self-loop(PingPong 자가 왕복) 로 두고, par 는 묶음+동시 Play 만 담당.
        {
            SJH::Scene::Actor* self      = enemy.get();             // 이동 후에도 동일 heap Actor — 댕글링 없음
            const vmath::vec3  baseScale = enemy->GetTransform().Scale; // 베이스 스케일 보존 (factory 설정 존중)

            // child A — Y 스케일 펄스 (0.4초 편도, 왕복 0.8초)
            auto scaleTween = tweeny::from(0.85f).to(1.15f)
                                  .during(ENEMY_SCALE_PULSE_MS)
                                  .via(tweeny::easing::sinusoidalInOut);
            auto scaleTw = std::make_unique<Tween::TweenPlayable<float>>(
                std::move(scaleTween),
                [self, baseScale](float s) {
                    self->GetTransform().Scale = vmath::vec3(baseScale[0], baseScale[1] * s, baseScale[2]);
                },
                Tween::TweenPlayable<float>::LoopMode::PingPong);
            scaleTw->SetIsLoop(true);   // child 자가 루프 (par 가 아니라 child 가 무한 반복)

            // child B — z축 회전 워블 (-30~+30, 2000ms 편도, 왕복 4000ms; 2000ms 왕복은 during(1000))
            auto rotTween = tweeny::from(-30.0f).to(30.0f)
                                .during(ENEMY_ROT_WOBBLE_MS)
                                .via(tweeny::easing::sinusoidalInOut);
            auto rotTw = std::make_unique<Tween::TweenPlayable<float>>(
                std::move(rotTween),
                [self](float deg) { 
			self->GetTransform().EulerRot[2] = deg; 
		},   // z축 = EulerRot[2] (degree)
                Tween::TweenPlayable<float>::LoopMode::PingPong);
            rotTw->SetIsLoop(true);

            // 동시재생 컨테이너 — Join 후 Play 하면 두 child 가 같은 프레임에 함께 틱.
            auto* par = enemy->AddComponent<SJH::Playable::ParallelPlayable>();
            par->Join(std::move(scaleTw));
            par->Join(std::move(rotTw));
            par->Play();
        }

        // P4 — 적 IActorPresentation [C] 공통 연출 클러스터 (player 와 동일 헬퍼):
        //   PlayableDirector 부착(=Life sink) + "hit"=SpriteHitFlash/"death"=SpriteDissolve(0.6) 기본 등록 +
        //   SetDeathDelaySeconds(0.6, dissolve 가시화 창) + 체력바(deps 색, 기본 빨강). 전부 AddChild 전(pre-entry).
        //   [B] 폭발/spark 는 delegate(onDeathFx)가 별도 트리거(유지). director 추가 사용 없음 -> 반환 무시.
        {
            EntityPresentationConfig pres;
            pres.dissolveSeconds   = ENEMY_DISSOLVE_SECONDS;
            pres.deathDelaySeconds = ENEMY_DEATH_DELAY;
            pres.healthBarColor    = deps.healthBarColor;
            AttachEntityPresentation(*enemy, pres);
        }

        // 5) 씬 트리 부착 (entry -> 컴포넌트 OnEnter 캐스케이드)
        if (!deps.spawnParent) return nullptr;
        return deps.spawnParent->AddChild(std::move(enemy));
    }
}

