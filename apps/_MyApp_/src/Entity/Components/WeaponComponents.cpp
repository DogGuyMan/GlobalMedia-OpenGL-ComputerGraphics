#include "Entity/Components/WeaponComponents.h"

#include "Entity/Bullet/bullet_factory.h" // CreateBulletActor / BulletConfig (+ box2d)
#include "scene/actor.h"                   // SJH::Scene::Actor (owner Transform)
#include "scene/scene.h"                   // Director::Get().Root()

#include <spdlog/spdlog.h>
#include <vmath.h>

namespace TopdownShooter::Entity::Components
{
	void Weapon::ShowInfo() const
	{
		spdlog::info("[weapon] name={} damage={:.0f}", WeaponName, Damage.GetValue());
	}

	void Weapon::UseWeapon(vmath::vec2 box2dForward) const
	{
		if (mWorld == nullptr)
		{
			spdlog::warn("[weapon] mWorld 미주입 — 발사 생략 (SetWorld 누락)");
			return;
		}
		SJH::Scene::Actor *owner = GetOwner();
		if (owner == nullptr)
			return;

		// owner world pos -> box2d. PhysicsSystem: box2d->world = (x, h, -y) -> box2d = (world.x, -world.z).
		const vmath::vec3 wp = owner->GetTransform().Translate;
		const vmath::vec2 b2pos(wp[0], -wp[2]);

		Bullet::BulletConfig bc;
		bc.world    = mWorld;
		bc.pos      = b2pos;
		bc.dir      = box2dForward; // 정규화 가정 (PlayerController 가 정규화 후 전달)
		bc.damage   = static_cast<int>(Damage.GetValue());

		// bullet 필터(maskBits = Enemy | Wall)가 Player 를 제외 -> 플레이어 중심 스폰이어도 자기충돌 없음.
		SJH::Scene::Director::Get().Root().AddChild(Bullet::CreateBulletActor(bc));
	}
} // namespace TopdownShooter::Entity::Components
