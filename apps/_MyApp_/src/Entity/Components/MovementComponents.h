/**
 * @file MovementComponents.h
 * @brief fps-independent 지속 이동을 담당하는 @c Movement Component 선언.
 *
 * @details
 *  ### 책임
 *  - 이동 속력(@c Algebraic::Numeric::Stat mMoveSpeed) 보유.
 *  - @c IMovable::DoForward 구현 - 방향 벡터 + dt 로 프레임 변위 산출 후 owner Transform 에 적용.
 *  - box2d XZ 매핑 컨벤션 준수 (DoForward 의 vec2(x,y) -> world transform x/z).
 *
 *  ### 비-책임
 *  - [X] 순간 가속(Dash/Knockback) - @c IImpulsable 별도 인터페이스 담당.
 *  - [X] box2d body 직접 조작 - Transform 만 변경(물리는 Physics::Components 담당).
 *
 *  ### 정통 매핑
 *  - Unity @c CharacterController.Move / Cocos2D @c MoveBy - dt 곱 fps-independent 이동.
 *
 * @note @c DoForward 는 zero-vector 가드(@c dir == 0) 적용 - normalize(0) 의 NaN 발생 방지.
 */
#ifndef _TOPDOWNSHOOTER_ENTITY_COMPONENTS_MOVEMENT__
#define _TOPDOWNSHOOTER_ENTITY_COMPONENTS_MOVEMENT__

#include "Algebraic/Stat.h"
#include "Components.Interfaces.h"
#include "scene/actor.h"

namespace TopdownShooter::Entity::Components
{
    /**
     * @brief fps-independent 지속 이동 컴포넌트 - @c IMovable 구현체.
     * @details
     *  @c mMoveSpeed (units/sec) * dt = 프레임 변위 공식으로 owner @c Actor 의
     *  @c Transform.Translate 를 직접 수정한다.
     *
     *  좌표계 매핑: @c DoForward 의 @p dir 는 box2d/XZ 평면 2D 벡터(x, y) ->
     *  world Transform 의 (x, 0, z) 로 적용(@c tr.Translate[0] / @c tr.Translate[2]).
     *
     *  파생 클래스는 @c mMoveSpeed 에 직접 접근 가능(protected). modifier 스택 추가는
     *  @c Algebraic::Numeric::Stat 의 AddModifier 로 처리한다.
     */
	class Movement : public SJH::Scene::Component,
	                 public IMovable
	{
	  protected:
		Algebraic::Numeric::Stat mMoveSpeed;  ///< 이동 속력 Stat (units/sec, modifier 확장 가능).

	  public:
		/// @brief 속력 0 으로 기본 생성.
		Movement()
		    : mMoveSpeed(0.0f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MoveSpeed)
		{
		}

		/// @brief 초기 이동 속력 지정 생성.
		/// @param movespeed 이동 속력(units/sec).
		Movement(float movespeed)
		    : mMoveSpeed(movespeed, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::MoveSpeed)
		{
		}

		/// @brief 씬 진입 시 초기화 (데이터 전용 - no-op).
		virtual void OnEnter() override {};
		/// @brief 프레임 갱신 (데이터 전용 - no-op, 이동은 DoForward 호출자가 직접 구동).
		/// @param dt 직전 프레임 경과 시간(초, 미사용).
		virtual void Update(float dt) override {};
		/// @brief 씬 이탈 시 정리 (데이터 전용 - no-op).
		virtual void OnExit() override {};

		/// @brief 단위 방향 @p dir 로 @p dt 초만큼 fps-independent 이동.
		/// @details @c mMoveSpeed(units/sec) * @p dt = 프레임 변위. zero-vector 입력은 NaN 방지를 위해 skip.
		///          box2d XZ 매핑: dir[0] -> tr.Translate[0](x), dir[1] -> tr.Translate[2](z).
		/// @param dir 이동 방향 벡터(정규화 미보장 시 내부에서 normalize 처리).
		/// @param dt  경과 시간(초).
		virtual void DoForward(vmath::vec2 dir, float dt) override
		{
			auto *owner = GetOwner();
			if (!owner)
				return;
			// zero-vec 가드 - normalize(0) 는 NaN 발생.
			if (dir[0] == 0.0f && dir[1] == 0.0f)
				return;
			auto &tr = owner->GetTransform();
			// mMoveSpeed = units/sec  dt(초) 곱해 *프레임 변위* 산출. fps-independent.
			auto displacement = vmath::normalize(dir) * (mMoveSpeed.GetValue() * dt);
			tr.Translate[0] += displacement[0];
			tr.Translate[2] += displacement[1];
		}
	};
}; // namespace TopdownShooter::Entity::Components

#endif //_TOPDOWNSHOOTER_ENTITY_COMPONENTS_MOVEMENT__