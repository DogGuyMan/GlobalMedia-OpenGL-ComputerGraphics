/**
 * @file WeaponComponents.cpp
 * @brief @c Weapon 컴포넌트 메서드 구현 - bullet_factory + scene.h 의존을 .cpp 에 격리.
 *
 * @details
 *  ### 책임
 *  - @c Weapon::UseWeapon : owner world pos -> box2d 좌표 변환 -> @c BulletConfig 구성 ->
 *    @c CreateBulletActor -> @c Director::Root().AddChild 로 씬에 등록.
 *  - 발사 후 총구 위치(yaw 포함)를 @c mOnFireFx seam 으로 FX 레이어에 전달.
 *  - @c Weapon::ShowInfo : spdlog 로깅.
 *
 *  ### 비-책임
 *  - [X] bullet Actor 의 씬 lifetime 관리 - @c Director::Root() 가 책임.
 *  - [X] 발사 쿨타임/창 제어 - PlayerController 가 Timer 로 관리.
 *
 * @note bullet_factory.h(무거운 include 체인)를 헤더 대신 이 .cpp 에서만 포함해
 *       @c WeaponComponents.h 의 include 전파를 최소화한다.
 */
#include "Entity/Components/WeaponComponents.h"

#include "Entity/Bullet/bullet_factory.h" // CreateBulletActor / BulletConfig (+ box2d)
#include "scene/actor.h"                   // SJH::Scene::Actor (owner Transform)
#include "scene/scene.h"                   // Director::Get().Root()

#include <cmath>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>

namespace TopdownShooter::Entity::Components
{
	/// @brief 무기 이름 + 공격력을 spdlog info 로그로 출력.
	void Weapon::ShowInfo() const
	{
		spdlog::info("[weapon] name={} damage={:.0f}", WeaponName, Damage.GetValue());
	}

	/// @brief @p box2dForward 방향으로 bullet Actor 1개를 스폰해 씬에 추가.
	/// @details
	///  변환 흐름:
	///  1. owner @c Transform.Translate(world XZ) -> box2d 좌표 (world.x, -world.z).
	///  2. @c BulletConfig 구성 (pos/dir/damage) -> @c CreateBulletActor 위탁.
	///  3. @c Director::Root().AddChild 로 씬에 등록.
	///  4. @c mOnFireFx seam 발동 - box2dForward -> world 방향 변환 후 yaw(atan2) 계산.
	///     yaw 컨벤션은 PlayerController facing 각과 동일(EffekseerPlayable SetRotation 호환).
	/// @param box2dForward box2d 좌표계 발사 방향 벡터 (= aimDir.x, -aimDir.z, 정규화 가정).
	void Weapon::UseWeapon(glm::vec2 box2dForward) const
	{
		if (mWorld == nullptr)
		{
			spdlog::warn("[weapon] mWorld 미주입 -- 발사 생략 (SetWorld 누락)");
			return;
		}
		SJH::Scene::Actor *owner = GetOwner();
		if (owner == nullptr)
			return;

		// owner world pos -> box2d. PhysicsSystem: box2d->world = (x, h, -y) -> box2d = (world.x, -world.z).
		const glm::vec3 wp = owner->GetTransform().Translate;
		const glm::vec2 b2pos(wp[0], -wp[2]);

		Bullet::BulletConfig bc;
		bc.world    = mWorld;
		bc.pos      = b2pos;
		bc.dir      = box2dForward; // 정규화 가정 (PlayerController 가 정규화 후 전달)
		bc.damage   = static_cast<int>(Damage.GetValue());

		// bullet 필터(maskBits = Enemy | Wall)가 Player 를 제외 -> 플레이어 중심 스폰이어도 자기충돌 없음.
		SJH::Scene::Director::Get().Root().AddChild(Bullet::CreateBulletActor(bc));

		// 발사 방향 gunshoot FX -- 빌더가 주입한 seam (없으면 no-op).
		// box2dForward(=aimDir.x, -aimDir.z) -> world 방향 (x, 0, -y). yaw 컨벤션은 PlayerController facing 각과 동일.
		if (mOnFireFx)
		{
			const glm::vec3 worldDir(box2dForward[0], 0.0f, -box2dForward[1]);
			const float       yaw       = std::atan2(-worldDir[0], -worldDir[2]); // 라디안 -- EffekseerPlayable SetRotation
			const glm::vec3 muzzlePos = wp + worldDir * 0.5f;                    // 총구 끝 = 플레이어 중심 + forward*0.5
			mOnFireFx(muzzlePos, yaw);
		}
	}
} // namespace TopdownShooter::Entity::Components
