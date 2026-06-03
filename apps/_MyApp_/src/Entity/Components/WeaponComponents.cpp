#include "Entity/Components/WeaponComponents.h"

#include "Entity/Bullet/bullet_factory.h" // CreateBulletActor / BulletConfig (+ box2d)
#include "scene/actor.h"                   // SJH::Scene::Actor (owner Transform)
#include "scene/scene.h"                   // Director::Get().Root()

#include <cmath>
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

		// 발사 방향 gunshoot FX — 빌더가 주입한 seam (없으면 no-op).
		// box2dForward(=aimDir.x, -aimDir.z) -> world 방향 (x, 0, -y). yaw 컨벤션은 PlayerController facing 각과 동일.
		if (mOnFireFx)
		{
			const vmath::vec3 worldDir(box2dForward[0], 0.0f, -box2dForward[1]);
			const float       yaw       = std::atan2(-worldDir[0], -worldDir[2]); // 라디안 — EffekseerPlayable SetRotation
			const vmath::vec3 muzzlePos = wp + worldDir * 0.5f;                    // 총구 끝 = 플레이어 중심 + forward*0.5
			mOnFireFx(muzzlePos, yaw);
		}
	}
} // namespace TopdownShooter::Entity::Components
