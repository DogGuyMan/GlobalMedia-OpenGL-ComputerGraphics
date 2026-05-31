#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__

#include "scene/actor.h"
#include <vmath.h>

namespace TopdownShooter::Entity
{
	/// @brief 손 1개 — owner(자식 Hand actor)의 local Transform 을 forward(-Z) 기준 고정 ±벌림각 위치로 배치.
	/// @details
	///   궤도(조준 방향 추종)는 *부모 player actor 의 EulerRot.Y* 를 scene graph 가 WorldMatrix 합성으로
	///   상속해 자동 처리한다. 따라서 본 컴포넌트는 *고정 local 위치만 1회 세팅* — 각도 자체계산 없음
	///   (이전 OrbitWithYAngle 의 acos 스텁 제거).
	///   local = ( sin(spread)*r, yOffset, -cos(spread)*r )  // forward = -Z
	class PlayerSingleHand : public SJH::Scene::Component
	{
	  private:
		float mSpreadRad; // forward(-Z) 기준 좌(+)/우(-) 벌림각 (radian)
		float mRadius;    // forward 거리 (player local)
		float mYOffset;   // 높이 오프셋

	  public:
		/// @param spreadDeg forward(-Z) 기준 좌(+)/우(-) 벌림각 (degree). 좌손 +, 우손 -.
		/// @param radius    forward 거리 (player local).
		/// @param yOffset   높이 오프셋.
		explicit PlayerSingleHand(float spreadDeg = 25.0f, float radius = 0.6f, float yOffset = 0.0f);

		void OnEnter() override;       // 고정 local 위치 1회 세팅
		void OnExit() override {}
		void Update(float dt) override {} // 고정 위치 — 궤도는 부모 회전 상속이 담당 (no-op)

	  private:
		void ApplyLocalOffset();
	};

	/// @brief 양손 조립 컴포넌트 — player actor 에 부착 → OnEnter 에서 자식 Hand actor 2개 생성·AddChild.
	/// @details
	///   각 자식 actor 는 PlayerSingleHand 를 보유 (컴포넌트는 actor 에 부착되는 게 정통 —
	///   값 멤버 보유 구조 폐기). 부모(player) Y facing 을 상속해 양손이 조준 방향으로 자동 궤도.
	///   ⚠ 손 스프라이트(atlas/프레임/오프셋)는 사용자 WIP — 현재는 구조(자식+상속 궤도)만 정착.
	class PlayerHands : public SJH::Scene::Component
	{
	  public:
		void OnEnter() override;       // 자식 Hand actor 2개 생성·부착
		void OnExit() override {}
		void Update(float dt) override {}

	  private:
		// 양손 기본 배치 — forward(-Z) 기준 ±벌림각 + 거리. (사용자 WIP 튜닝 대상)
		static constexpr float kSpreadDeg = 25.0f;
		static constexpr float kRadius    = 0.6f;
		static constexpr float kYOffset   = 0.0f;
	};
}; // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__
