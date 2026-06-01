#include <GL/gl3w.h> // 반드시 최상단 — resource_registry.h→framebuffer.h→render_target.h→gl3w.h 보다 먼저.

#include "Entity/Player/PlayerActor.h"

#include "Physics/PhysicsImpulse.h"

#include "resource_registry/resource_registry.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_frame_clip.h"
#include "sprite/sprite_sequence_playable.h"

#include <box2d/box2d.h>
#include <spdlog/spdlog.h>
#include <string>

namespace TopdownShooter::Entity::Player
{
	std::unique_ptr<SJH::Scene::Actor> CreatePlayerActor(const PlayerActorConfig &cfg)
	{
		auto actor = std::make_unique<SJH::Scene::Actor>(cfg.name);

		actor->AddComponent<Components::Life>(cfg.life.hp);

		if (cfg.physics.world != nullptr)
		{
			// Physics body 생성 — b2World 가 lifetime 소유.
			b2BodyDef bd;
			bd.type = b2_dynamicBody;
			bd.position.Set(cfg.physics.startPosition[0], cfg.physics.startPosition[1]);
			bd.linearDamping = cfg.physics.linearDamping;
			b2Body *body = cfg.physics.world->CreateBody(&bd);

			b2PolygonShape box;
			box.SetAsBox(cfg.physics.size[0] * 0.5f, cfg.physics.size[1] * 0.5f);

			b2FixtureDef fd;
			fd.shape = &box;
			fd.density = cfg.physics.density;
			fd.friction = cfg.physics.friction;
			fd.isSensor = cfg.physics.isSensor;
			fd.filter.categoryBits = cfg.physics.categoryBits;
			fd.filter.maskBits = cfg.physics.maskBits;
			body->CreateFixture(&fd);

			auto *pb = actor->AddComponent<Physics::Components::BoxBody>();
			pb->SetBody(body);
			pb->SetSensor(cfg.physics.isSensor);

			auto *pm = actor->AddComponent<Physics::PhysicsMovement>(cfg.movement.speed);

			// Dash/Knockback 속도버스트 — dash 입력 미배선이라 현재 dormant (Action::Dash 바인딩 시 활성).
			actor->AddComponent<Physics::Impulse>();

			// Weapon — bullet 스폰은 box2d 의존이라 physics 분기에서만 부착. 좌클릭 시 controller 가 호출.
			auto *weapon = actor->AddComponent<Components::Weapon>(cfg.weapon.damage, "default");
			weapon->SetWorld(cfg.weapon.world);

			if (cfg.controller.keyboard != nullptr)
			{
				auto *controller = actor->AddComponent<Controller::PlayerController>();
				controller->SetKeyboardInput(cfg.controller.keyboard);
				controller->SetMouseInput(cfg.controller.mouse);
				controller->SetWorldCamera(cfg.controller.camera);
				controller->SetMovableTarget(pm);
				controller->SetFireCallback(cfg.controller.onFire);
				controller->SetDamageCallback(cfg.controller.onDamage);
				controller->SetUp();
			}
		}
		else
		{
			// physics 미사용 — 기존 Movement (Transform 직접 조작).
			auto *movement = actor->AddComponent<Components::Movement>(cfg.movement.speed);

			if (cfg.controller.keyboard != nullptr)
			{
				auto *controller = actor->AddComponent<Controller::PlayerController>();
				controller->SetKeyboardInput(cfg.controller.keyboard);
				controller->SetMouseInput(cfg.controller.mouse);
				controller->SetWorldCamera(cfg.controller.camera);
				controller->SetMovableTarget(movement);
				controller->SetFireCallback(cfg.controller.onFire);
				controller->SetDamageCallback(cfg.controller.onDamage);
				controller->SetUp();
			}
		}

		// === 4-레이어 스프라이트 합성 (spec §6.3) — direction 지정 시에만 ===
		// 각 파트 PNG = 자기 UniformAtlas (정적=SetGrid(1,1), 애니 B파트=SetGrid(ColCount,1) 스트립).
		// DrawOrder 별 child Actor (local 0,0,0 → parent world 공유) + QueueOffset=DrawOrder painter 합성.
		if (cfg.sprite.direction != nullptr)
		{
			auto &reg = SJH::ResourceRegistry::Get();
			for (const auto &t : *cfg.sprite.direction)
			{
				// atlas 먼저 (child 생성 전) — key=path. 있으면 재사용(Find), 없으면 생성(Create).
				// CreateUniformAtlas 는 중복 키에서 nullptr 이므로 LEFT/RIGHT 가 같은 PNG 를 공유할 때 Find 필수.
				auto *atlas = reg.FindUniformAtlas(t.TexturePath);
				if (!atlas)
					atlas = reg.CreateUniformAtlas(t.TexturePath, t.TexturePath, t.ColCount, t.RowCount);
				if (!atlas)
				{
					spdlog::error("[4layer] atlas load 실패: {}", t.TexturePath);
					continue; // child 미생성 — 빈 child 를 트리에 남기지 않음
				}

				auto child = std::make_unique<SJH::Scene::Actor>(
				    cfg.name + "_L" + std::to_string(t.DrawOrder));
				SJH::Scene::Actor *childPtr = actor->AddChild(std::move(child));

				auto *spr = childPtr->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
				spr->flipX = t.Flip;
				spr->QueueOffset = t.DrawOrder; // 2450+DrawOrder → distinct 층

				if (t.ColCount > 1) // 애니 파트 (가로 N프레임 스트립)
				{
					auto *seq = childPtr->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
					    spr, SJH::SpriteSequence::SpriteFrameClip{0, t.ColCount, cfg.sprite.fps});
					seq->SetIsLoop(true);
					seq->Play();
				}
			}
		}

		return actor;
	}
} // namespace TopdownShooter::Entity::Player
