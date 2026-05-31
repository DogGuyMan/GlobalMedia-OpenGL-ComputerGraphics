#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__

#include "Algebraic/Stat.h"
#include "Components.Interfaces.h"
#include "scene/actor.h"
#include <string>
#include <vmath.h>

// fwd — UseWeapon 이 bullet body 를 생성할 물리 월드 (포인터 멤버 — 전방 선언으로 충분).
class b2World;

namespace TopdownShooter::Entity::Components
{
	/// @brief 단발 발사 무기 컴포넌트 — 좌클릭 시 조준 방향으로 bullet 1개 스폰.
	/// @details
	///   - Damage 는 Stat 으로 통합 (NumericType::Power, UseType::Natural) — modifier 시스템 적용 가능.
	///   - `mWorld` 는 비소유 — bullet body 를 생성할 b2World (PlayerBuilder 가 SetWorld 로 주입).
	///   - UseWeapon 의 정의는 WeaponComponents.cpp (bullet_factory + scene.h 무거운 include 헤더 분리).
	class Weapon : public SJH::Scene::Component
	{
	  private:
		// Damage 는 Stat 으로 통합 — modifier 시스템 적용 가능 (NumericType::Power, UseType::Natural).
		// 기존 `const int Damage` 의 const 는 제거 — Stat 자체 BaseValue 가 const 라 base 값은 여전히 불변,
		// 그러나 멤버는 modifier 추가/제거가 가능해야 하므로 non-const.
		Algebraic::Numeric::Stat Damage;
		const std::string WeaponName;
		b2World *mWorld = nullptr; // 비소유 — bullet body 생성용 (SetWorld 주입)

	  public:
		Weapon(int damage, const char *literal_str)
		    : Damage(static_cast<float>(damage), Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::Power),
		      WeaponName(literal_str)
		{
		}

		/// @brief bullet 스폰용 물리 월드 주입 (비소유). UseWeapon 전 필수. Fluent self 반환.
		Weapon &SetWorld(b2World *world)
		{
			mWorld = world;
			return *this;
		}

		void ShowInfo() const;

		/// @brief box2dForward 방향(정규화 가정)으로 단발 bullet 스폰.
		/// @param box2dForward box2d 좌표계 발사 방향 = (aimDir.x, -aimDir.z). mWorld 미주입 시 no-op.
		void UseWeapon(vmath::vec2 box2dForward) const;

		// === Component lifecycle — 데이터 전용 컴포넌트라 no-op (추상 베이스 충족) ===
		void OnEnter() override {}
		void OnExit() override {}
		void Update(float) override {}
	};
} // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__
