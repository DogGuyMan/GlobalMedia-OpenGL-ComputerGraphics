/**
 * @file WeaponComponents.h
 * @brief 단발 발사 무기 컴포넌트 선언 - 조준 방향으로 bullet 1개 스폰.
 *
 * @details
 *  ### 책임
 *  - Damage 수치를 @c Algebraic::Numeric::Stat 으로 보유(modifier 스택 적용 가능).
 *  - @c UseWeapon(box2dForward) 호출 시 @c BulletConfig 를 구성해 @c CreateBulletActor 에 위탁.
 *  - 총구 위치 + yaw 를 계산해 @c mOnFireFx seam 으로 연출 레이어에 전달.
 *
 *  ### 비-책임
 *  - [X] Scene 에 직접 Actor 추가 - @c UseWeapon 이 @c Director::Root().AddChild 위탁.
 *  - [X] 발사 쿨타임/창 관리 - 호출자(PlayerController) 가 @c Timer 로 관리.
 *  - [X] bullet_factory 헤더 include - WeaponComponents.cpp 에서만 include (헤더 오염 방지).
 *
 *  ### 정통 매핑
 *  - Unity @c Weapon.Fire() / Cocos2D 총알 팩토리 패턴. Actor 비상속, Component 위임.
 *
 * @note @c mWorld(b2World*) 는 비소유. @c SetWorld 주입 전 @c UseWeapon 호출 시 warn 후 no-op.
 *       @c UseWeapon 구현은 WeaponComponents.cpp (bullet_factory + scene.h 무거운 include 분리).
 */
#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__

#include "Algebraic/Stat.h"
#include "Contracts/EntityContracts.h"
#include "scene/actor.h"
#include <functional>
#include <string>
#include <utility>
#include <glm/glm.hpp>

// fwd - UseWeapon 이 bullet body 를 생성할 물리 월드 (포인터 멤버 - 전방 선언으로 충분).
class b2World;

namespace TopdownShooter::Entity::Components
{
	/// @brief 단발 발사 무기 컴포넌트 - 좌클릭 시 조준 방향으로 bullet 1개 스폰.
	/// @details
	///   - Damage 는 Stat 으로 통합 (NumericType::Power, UseType::Natural) - modifier 시스템 적용 가능.
	///   - `mWorld` 는 비소유 - bullet body 를 생성할 b2World (PlayerBuilder 가 SetWorld 로 주입).
	///   - UseWeapon 의 정의는 WeaponComponents.cpp (bullet_factory + scene.h 무거운 include 헤더 분리).
	class Weapon : public SJH::Scene::Component
	{
	  private:
		// Damage 는 Stat 으로 통합 -- modifier 시스템 적용 가능 (NumericType::Power, UseType::Natural).
		// 기존 `const int Damage` 의 const 는 제거 -- Stat 자체 BaseValue 가 const 라 base 값은 여전히 불변,
		// 그러나 멤버는 modifier 추가/제거가 가능해야 하므로 non-const.
		Algebraic::Numeric::Stat Damage;                                     ///< 공격력 Stat (modifier 스택 적용 가능, NumericType::Power).
		const std::string WeaponName;                                         ///< 무기 이름 (ShowInfo 로깅 + 식별용).
		b2World *mWorld = nullptr;                                            ///< 총알 body 를 등록할 Box2D 월드 (비소유, SetWorld 주입).
		std::function<void(const glm::vec3 &pos, float yaw)> mOnFireFx;    ///< 발사 시 총구 위치(pos) + 방향(yaw 라디안) FX seam (빌더 주입, nullptr = no-op).

	  public:
		/// @brief 공격력과 이름으로 무기 생성.
		/// @param damage      기본 공격력.
		/// @param literal_str 무기 이름 문자열 리터럴.
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

		/// @brief 발사 시 총구 위치+방향(yaw 라디안)으로 발동할 FX seam 주입 (빌더 전용 fluent).
		Weapon &SetOnFireFx(std::function<void(const glm::vec3 &, float)> fx)
		{
			mOnFireFx = std::move(fx);
			return *this;
		}

		/// @brief 무기 이름 + 공격력을 spdlog info 로 출력 (디버그용).
		void ShowInfo() const;

		/// @brief box2dForward 방향(정규화 가정)으로 단발 bullet 스폰.
		/// @param box2dForward box2d 좌표계 발사 방향 = (aimDir.x, -aimDir.z). mWorld 미주입 시 no-op.
		void UseWeapon(glm::vec2 box2dForward) const;

		// === Component lifecycle -- 데이터 전용 컴포넌트라 no-op (추상 베이스 충족) ===
		/// @brief 씬 진입 시 초기화 (데이터 전용 - no-op).
		void OnEnter() override {}
		/// @brief 씬 이탈 시 정리 (데이터 전용 - no-op).
		void OnExit() override {}
		/// @brief 프레임 갱신 (데이터 전용 - no-op, 발사는 UseWeapon 직접 호출).
		void Update(float) override {}
	};
} // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_WEAPON__
