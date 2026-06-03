#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__

#include "scene/actor.h"
#include "Entity/Constants.h"
#include "Tween/TweenPlayable.h" // 발사 핀치 복귀 (Tweeny Playable)
#include <memory>
#include <vmath.h>

namespace TopdownShooter::Controller
{
	class PlayerController; // 손 spread 거리 보간 — aim 거리 읽기용 (포인터 멤버, 전방 선언)
}
// CLAUDE ASSIST
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
		explicit PlayerSingleHand(float spreadDeg = HAND_SPREAD_DEG, float radius = HAND_RADIUS, float yOffset = HAND_Y_OFFSET, float scale = 1.0f);

		void OnEnter() override;       // 고정 local 위치 1회 세팅
		void OnExit() override {}
		void Update(float dt) override {} // 위치 갱신은 PlayerHands(부모)가 SetSpreadDeg 로 주도 (no-op)

		/// @brief 벌림각 갱신 — 부호 포함 degree (좌손 +, 우손 -). 즉시 local 위치 재적용.
		///        거리 보간(PlayerHands)이 매 프레임 호출. Scale/궤도(부모 회전)와 직교.
		void SetSpreadDeg(float spreadDeg);

	  private:
		void ApplyLocalOffset();
	};

	/// @brief 양손 조립 컴포넌트 — player actor 에 부착 -> OnEnter 에서 자식 Hand actor 2개 생성·AddChild.
	/// @details
	///   각 자식 actor 는 PlayerSingleHand(고정 ±벌림각 local) + 빌보드 SpriteRenderer(HAND_PART) 를 보유.
	///   부모(player) Y facing 을 상속해 양손 *위치* 가 조준 방향으로 자동 궤도(WorldMatrix 합성). 스프라이트는
	///   빌보드라 항상 카메라를 향한다. HAND_PART 텍스처는 Playable/Constants.h 정의(양손 공유 atlas 캐시).
	///   ※ spread/radius/yOffset/QueueOffset 수치는 Entity/Constants.h(HAND_*) 단일 소스.
	class PlayerHands : public SJH::Scene::Component
	{
	  public:
		void OnEnter() override;       // 자식 Hand actor 2개 생성·부착 (+ HAND_PART 스프라이트)
		void OnExit() override {}
		void Update(float dt) override; // 매 프레임 aim 거리로 양손 spread 보간 + 발사 핀치 블렌드 (.cpp)

		/// @brief 좌클릭 발사 — 양팔을 즉시 최소각(15°)으로 핀치한 뒤 HAND_FIRE_PINCH_MS 동안
		///        거리 기반 각도로 Tweeny 복귀. 클릭마다 트윈 재생성(재시작).
		void TriggerFire();

	  private:
		// OnEnter 에서 생성한 자식 손 컴포넌트 (비소유 — 자식 Actor 가 소유).
		PlayerSingleHand *mLeft  = nullptr;
		PlayerSingleHand *mRight = nullptr;
		// 형제(같은 player actor) PlayerController — aim 거리 출처. lazy 캐시.
		Controller::PlayerController *mController = nullptr;

		// 발사 핀치 — 1(완전 핀치=15°) → 0(거리기반) 으로 Tween 감쇠. finalHalf = lerp(거리기반, MIN, blend).
		float mFireBlend = 0.0f;
		std::unique_ptr<Tween::TweenPlayable<float>> mFireTween; // 클릭마다 새로 생성 (one-shot 재시작)
	};
}; // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__
