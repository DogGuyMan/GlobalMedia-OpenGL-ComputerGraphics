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
		float mScale;     // 손 스프라이트 시각 크기 (local Scale, 균등)

	  public:
		/// @param spreadDeg forward(-Z) 기준 좌(+)/우(-) 벌림각 (degree). 좌손 +, 우손 -.
		/// @param radius    forward 거리 (player local).
		/// @param yOffset   높이 오프셋.
		/// @param scale     손 스프라이트 균등 Scale (Translate 와 직교 — 궤도에 영향 없음).
		explicit PlayerSingleHand(float spreadDeg = 25.0f, float radius = 0.6f, float yOffset = 0.0f, float scale = 1.0f);

		void OnEnter() override;       // 고정 local 위치 1회 세팅
		void OnExit() override {}
		void Update(float dt) override {} // 고정 위치 — 궤도는 부모 회전 상속이 담당 (no-op)

	  private:
		void ApplyLocalOffset();
	};

	/// @brief 양손 조립 컴포넌트 — player actor 에 부착 -> OnEnter 에서 자식 Hand actor 2개 생성·AddChild.
	/// @details
	///   각 자식 actor 는 PlayerSingleHand(고정 ±벌림각 local) + 빌보드 SpriteRenderer(HAND_PART) 를 보유.
	///   부모(player) Y facing 을 상속해 양손 *위치* 가 조준 방향으로 자동 궤도(WorldMatrix 합성). 스프라이트는
	///   빌보드라 항상 카메라를 향한다. HAND_PART 텍스처는 Playable/Constants.h 정의(양손 공유 atlas 캐시).
	///   ※ spread/radius/yOffset/QueueOffset 수치는 비주얼 튜닝 대상.
	class PlayerHands : public SJH::Scene::Component
	{
	  public:
		void OnEnter() override;       // 자식 Hand actor 2개 생성·부착 (+ HAND_PART 스프라이트)
		void OnExit() override {}
		void Update(float dt) override {}

	  private:
		// 양손 기본 배치 — forward(-Z) 기준 ±벌림각 + 거리 + 스프라이트 크기. (비주얼 튜닝 대상)
		static constexpr float kSpreadDeg       = 25.0f;
		static constexpr float kRadius          = 0.6f;
		static constexpr float kYOffset         = 0.0f;
		static constexpr float kHandScale       = 0.25f; // 손 스프라이트가 커서 축소 (Scale 만 — 궤도 무관)
		static constexpr int   kHandQueueOffset = 10; // 플레이어 몸통 레이어(DrawOrder 0~3) 위 (튜닝 대상)
	};
}; // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__
