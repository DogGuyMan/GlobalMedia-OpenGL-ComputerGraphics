/**
 * @file EnemyBuilder.cpp
 * @brief @c BuildEnemy 구현 - 적 1체 조립 파이프라인 + 발밑 데칼 헬퍼.
 *
 * @details
 *  ### 책임
 *  - @c EnemyFactory 코어(물리/Life/AI/contact) 위에 표현 레이어를 누적 부착.
 *  - 적 전용: renderActor 스프라이트 + 상시 펄스/워블 트윈(@c ParallelPlayable) +
 *    EntityPresentation 공통 연출 + "hit" 에 Damaged 사운드 추가 + hit FX/데미지 숫자 seam.
 *  - 익명 namespace @c AttachGroundDecals - 발밑 그림자 + 피격범위 원 데칼 2장.
 *
 *  ### 비-책임([X])
 *  - [X] 적 코어 컴포넌트 생성 - @c Entity::Enemy::CreateEnemyActor 위임 (무변경 호출).
 *  - [X] VFX/사운드 자원 owner - registry/AudioSystem 보유, 빌더는 seam 람다만 주입.
 *
 * @note PlayerBuilder 와 @c AttachGroundDecals 본문이 동일 (각 빌더 익명 namespace 에 별도 사본).
 *       적 "hit" 은 헬퍼 기본 SpriteHitFlash 를 Parallel(flash + Damaged 사운드)로 덮어쓴다 -
 *       화면 비네팅은 피해자가 적이라 제외(Player 만 비네팅).
 */
#include <GL/gl3w.h> // 최상단 — resource_registry.h->framebuffer.h->...->gl3w.h 보다 먼저.

#include "Bootstrap/EnemyBuilder.h"

#include "Bootstrap/EntityPresentation.h"  // AttachEntityPresentation — Player/Enemy 공통 연출 클러스터
#include "Entity/Enemy/EnemyFactory.h"   // CreateEnemyActor / EnemyConfig (+box2d)
#include "Playable/Constants.h"           // ENEMY_FRONT
#include "Playable/SpriteLayerFactory.h"  // AttachSpriteLayer — 3-빌더 공유 sprite-layer 부착 헬퍼
#include "Playable/SpriteFxPlayable.h"    // SpriteHitFlashPlayable ("hit" 덮어쓰기)
#include "Playable/PlayableDirector.h"    // director->Register("hit", ...) 덮어쓰기
#include "Tween/TweenPlayable.h"          // 상시 루프 트윈
#include "Entity/Components/LifeComponents.h" // GetComponent<Life> (SetOnHitFx seam 주입)
#include "Audio/AudioSystem.h"            // 적 피격음 — Manager().Audio().LoadEvent
#include "Audio/Constants.h"              // EVENT_DAMAGED (적 hit 재사용)
#include "Audio/FmodStudioPlayable.h"     // 적 "hit" Parallel 의 Damaged 사운드
#include "Manager.h"                      // TopdownShooter::Manager::Get().Audio()
#include "playable/composite_playable.h"  // SJH::Playable::ParallelPlayable (동시재생 컨테이너)
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "Spawns/VfxInstance.h"            // VFX::Spawn 파사드 (hit seam 주입 람다 본문)
#include "Spawns/WorldTextInstance.h"      // WorldText::SpawnDamage 파사드 (데미지 숫자 seam 주입)
#include "Bootstrap/Constants.h"           // ENEMY_DISSOLVE/DELAY/트윈MS
#include "object/mesh.h"               // SJH::Mesh::CreatePlane
#include "render/mesh_renderer.h"      // SJH::Scene::MeshRenderer
#include "material/material.h"         // SJH::Material
#include "material/pass.h"             // SJH::Pass::Kind::Transparent
#include "resource_registry/image.h"  // SJH::Image::Load
#include "Physics/PhysicsComponent.h"  // FindPhysics / Physics::GetBody

#include <tweeny/tweeny.h>                 // tweeny::from / easing (tween 정의)
#include <box2d/box2d.h>               // b2Shape / b2PolygonShape / b2Fixture
#include <spdlog/spdlog.h>             // spdlog::error (자원 로드 실패 가드)
#include <vmath.h>                         // vmath::vec3 (Transform.Scale)

#include <algorithm>                   // std::max
#include <memory>  // std::make_unique (Join 자식 생성)
#include <utility> // std::move

namespace TopdownShooter::Bootstrap
{
	namespace
	{
		/// @brief [데칼] 엔티티 발밑 그림자 + 피격범위 원 — groundActor 자식 1개 아래 MeshRenderer 2장.
		/// @details depth+1: root.groundActor.{decal_shadow, decal_hitrange}. 워블/스케일과 독립 Transform.
		///          공유 자원은 find-or-create (스폰마다 호출돼도 1회 생성). 크기는 첫 fixture 반경 자동.
		void AttachGroundDecals(SJH::Scene::Actor &root, float baseY)
		{
			auto &reg = SJH::ResourceRegistry::Get();

			// ── 공유 자원 (find-or-create) ──
			SJH::Program *prog = reg.FindProgram("simple_texture");
			if (!prog)
				prog = reg.CreateProgram("simple_texture",
				    "./resources/shaders/simple_texture.vs",
				    "./resources/shaders/simple_texture.fs");
			if (!prog)
			{
				spdlog::error("AttachGroundDecals: simple_texture program 생성 실패 — 데칼 생략");
				return;
			}

			SJH::Mesh *plane = reg.FindMesh("_ground_plane");
			if (!plane)
				plane = reg.RegisterMesh("_ground_plane", SJH::Mesh::CreatePlane());
			if (!plane)
			{
				spdlog::error("AttachGroundDecals: _ground_plane 메시 생성 실패 — 데칼 생략");
				return;
			}

			SJH::Texture *shadowTex = reg.FindTexture("entity_shadow");
			if (!shadowTex)
			{
				auto img = SJH::Image::Load("entity_shadow", "resources/texture/EntityShadow.png");
				if (img)
					shadowTex = reg.CreateTexture("entity_shadow", img.get());
			}

			SJH::Texture *circleTex = reg.FindTexture("hit_range_circle");
			if (!circleTex)
			{
				auto img = SJH::Image::Load("hit_range_circle", "resources/texture/Circle_albedo.png");
				if (img)
					circleTex = reg.CreateTexture("hit_range_circle", img.get());
			}
			if (!shadowTex || !circleTex)
			{
				spdlog::error("AttachGroundDecals: 데칼 텍스처 로드 실패 — 데칼 생략");
				return;
			}

			SJH::Material *shadowMat = reg.FindSharedMaterial("shadow_decal_mat");
			if (!shadowMat)
			{
				shadowMat = reg.CreateSharedMaterial("shadow_decal_mat");
				if (!shadowMat)
				{
					spdlog::error("AttachGroundDecals: shadow_decal_mat 생성 실패 — 데칼 생략");
					return;
				}
				shadowMat->SetProgram(prog);
				shadowMat->SetPass(SJH::Pass::Kind::Transparent);
				shadowMat->Properties.Textures["uTex"]   = {shadowTex, 0};
				shadowMat->Properties.Vec4s["baseColor"] = vmath::vec4(1.0f, 1.0f, 1.0f, 0.5f);
			}
			SJH::Material *hitMat = reg.FindSharedMaterial("hitrange_decal_mat");
			if (!hitMat)
			{
				hitMat = reg.CreateSharedMaterial("hitrange_decal_mat");
				if (!hitMat)
				{
					spdlog::error("AttachGroundDecals: hitrange_decal_mat 생성 실패 — 데칼 생략");
					return;
				}
				hitMat->SetProgram(prog);
				hitMat->SetPass(SJH::Pass::Kind::Transparent);
				hitMat->Properties.Textures["uTex"]   = {circleTex, 0};
				hitMat->Properties.Vec4s["baseColor"] = vmath::vec4(1.0f, 0.0f, 0.0f, 0.45f);
			}

			// ── 충돌 반경 (첫 fixture) ──
			float hitRadius = 0.5f;
			if (auto *phys = TopdownShooter::Physics::Components::FindPhysics(&root))
			{
				if (b2Body *body = phys->GetBody())
				{
					if (b2Fixture *fx = body->GetFixtureList())
					{
						const b2Shape *sh = fx->GetShape();
						if (sh->GetType() == b2Shape::e_circle)
							hitRadius = sh->m_radius;
						else if (sh->GetType() == b2Shape::e_polygon)
						{
							const auto *poly   = static_cast<const b2PolygonShape *>(sh);
							float        maxExt = 0.0f;
							for (int32 i = 0; i < poly->m_count; ++i)
								maxExt = std::max(maxExt, poly->m_vertices[i].Length());
							hitRadius = maxExt;
						}
					}
				}
			}
			const float hitD    = hitRadius * 2.0f;
			const float shadowD = hitRadius * 2.0f * 1.2f;

			// ── groundActor + 데칼 2장 ──
			auto *ground = root.AddChild(std::make_unique<SJH::Scene::Actor>("groundActor"));

			auto *shadow = ground->AddChild(std::make_unique<SJH::Scene::Actor>("decal_shadow"));
			shadow->AddComponent<SJH::Scene::MeshRenderer>(plane, shadowMat, /*queueOffset*/ 0);
			shadow->GetTransform().EulerRot[0] = -90.0f;                        // XY → XZ 눕힘
			shadow->GetTransform().Scale       = vmath::vec3(shadowD, 1.0f, shadowD);
			shadow->GetTransform().Translate   = vmath::vec3(0.0f, baseY, 0.0f);                        // z-fight 회피

			auto *circle = ground->AddChild(std::make_unique<SJH::Scene::Actor>("decal_hitrange"));
			circle->AddComponent<SJH::Scene::MeshRenderer>(plane, hitMat, /*queueOffset*/ 1);
			circle->GetTransform().EulerRot[0] = -90.0f;
			circle->GetTransform().Scale       = vmath::vec3(hitD, 1.0f, hitD);
			circle->GetTransform().Translate   = vmath::vec3(0.0f, baseY + DECAL_CIRCLE_Y_DELTA, 0.0f);
		}
	} // namespace

    /// @brief 적 1체 조립 파이프라인 (선언부 doc 은 EnemyBuilder.h 참조).
    /// @details 단계별 주석은 본문 1)~5) 블록 참조. 코어 factory 호출 후 표현/연출/데칼/seam 을
    ///          순차 부착하고 마지막에 spawnParent 로 entry 시킨다.
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
        auto enemy = Entity::Enemy::CreateEnemyActor(cfg);   // unique_ptr<Actor> (미부착)

        // 2) renderActor(root 직속 자식) — sprite + 워블 전용. groundActor 데칼과 Transform 독립.
        //    (ForEachSpriteRenderer 가 root 직속자식을 훑으므로 hit-flash/dissolve 는 그대로 도달.)
        auto* renderActor = enemy->AddChild(std::make_unique<SJH::Scene::Actor>("renderActor"));
        const auto& tex = Playable::ENEMY_FRONT[deps.variant % 3];
        auto& reg = SJH::ResourceRegistry::Get();
        Playable::AttachSpriteLayer(*renderActor, reg, tex, deps.spriteFps);

        // 4) 상시 루프 트윈 — ParallelPlayable 로 *동시재생* (스케일 펄스 ∥ z축 회전 워블).
        //    composite 모듈(SJH::Playable::ParallelPlayable, EngineAPI.md §ParallelPlayable)을 컨테이너로 쓰고
        //    각 child 가 스스로 PingPong 루프한다.
        //    !! 왜 par.SetIsLoop 이 아니라 child.SetIsLoop 인가:
        //       ParallelPlayable 의 loop 재시작은 child->Play() 만 호출하는데, TweenPlayable.Play()(=PlayableBase)
        //       는 tweeny progress 를 되감지 않는다 -> "one-shot child + par 루프" 는 끝값에 고정(깨짐).
        //       그래서 child 를 self-loop(PingPong 자가 왕복) 로 두고, par 는 묶음+동시 Play 만 담당.
        {
            SJH::Scene::Actor* self      = renderActor;            // root 자식 — 주소 안정(enemy children 보유)
            const vmath::vec3  baseScale = renderActor->GetTransform().Scale; // 신규 Actor 기본 (1,1,1)

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
            auto* par = renderActor->AddComponent<SJH::Playable::ParallelPlayable>();
            par->Join(std::move(scaleTw));
            par->Join(std::move(rotTw));
            par->Play();
        }

        // P4 — 적 IActorPresentation [C] 공통 연출 클러스터 (player 와 동일 헬퍼):
        //   PlayableDirector 부착(=Life sink) + "hit"=SpriteHitFlash/"death"=SpriteDissolve(0.6) 기본 등록 +
        //   SetDeathDelaySeconds(0.6, dissolve 가시화 창) + 체력바(deps 색, 기본 빨강). 전부 AddChild 전(pre-entry).
        //   [B] 폭발/spark death FX 는 Life::SetOnDeathFx seam (현재 미배선 — Task9). director 추가 사용 없음 -> 반환 무시.
        {
            EntityPresentationConfig pres;
            pres.dissolveSeconds   = ENEMY_DISSOLVE_SECONDS;
            pres.deathDelaySeconds = ENEMY_DEATH_DELAY;
            pres.healthBarColor    = deps.healthBarColor;
            auto *director = AttachEntityPresentation(*enemy, pres);

            // 적 "hit" 에 피격음 추가 — 헬퍼 기본(SpriteHitFlash flash-only)을 Parallel(flash ∥ Damaged)로 덮어쓰기.
            //   Player 는 PlayerBuilder 가 비네팅 포함으로 덮어씀; 적은 전면 비네팅 제외(피해자가 적이라 화면효과 부적합) —
            //   flash + 사운드만. bank 에 적 전용 hurt 이벤트가 없어 event:/Damaged 재사용
            //   (별도 event:/EnemyHurt 추가 시 Audio::EVENT_DAMAGED 한 곳만 교체).
            if (director)
            {
                auto *damagedEvt = TopdownShooter::Manager::Get().Audio().LoadEvent(Audio::EVENT_DAMAGED);
                auto  par        = std::make_unique<SJH::Playable::ParallelPlayable>();
                par->Join(std::make_unique<TopdownShooter::Playable::SpriteHitFlashPlayable>(enemy.get()));
                if (damagedEvt)
                    par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(damagedEvt));
                director->Register("hit", std::move(par));
            }
        }

        // hit FX seam — 적 Life 피격 시 hit.efk (Player 와 공통, 빌더가 VFX::Spawn 주입).
        if (auto* life = enemy->GetComponent<Entity::Components::Life>())
            life->SetOnHitFx([](const vmath::vec3& p) { VFX::Spawn("hit", p); })
                .SetOnDamageNumber([](int d, const vmath::vec3& p) { WorldText::SpawnDamage(d, p); });

        // 발밑 그림자 + 피격범위 원 (groundActor 자식, renderActor 와 형제). AddChild 전 = pre-entry.
		AttachGroundDecals(*enemy, ENEMY_DECAL_Y);

        // 5) 씬 트리 부착 (entry -> 컴포넌트 OnEnter 캐스케이드)
        if (!deps.spawnParent) return nullptr;
        return deps.spawnParent->AddChild(std::move(enemy));
    }
}

