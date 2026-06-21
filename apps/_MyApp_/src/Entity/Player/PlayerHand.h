/**
 * @file PlayerHand.h
 * @brief 플레이어 양손 표현 - 조준 거리 비례 spread 보간 + 발사 핀치 연출 Component 2종.
 *
 * @details
 *  ### 책임
 *  - @c PlayerSingleHand: 단일 손 Component - forward(-Z) 기준 고정 벌림각 local 위치 세팅.
 *    부모(player) WorldMatrix 합성으로 조준 방향을 자동 상속하므로 매 프레임 각도 재계산 불필요.
 *  - @c PlayerHands: 양손 조립 Component - OnEnter 에서 자식 Hand actor 2개 생성/AddChild,
 *    매 프레임 aim 거리 보간으로 양손 spread 갱신, 발사 시 핀치 -> Tweeny 복귀 연출.
 *
 *  ### 비-책임
 *  - [X] 조준 방향 계산 - 부모 actor Y 회전(Scene graph WorldMatrix)이 처리, 손은 local 위치만 담당.
 *  - [X] aim 거리/NDC 커서 계산 - PlayerController::GetAimScreenT() 가 담당.
 *  - [X] 스프라이트 렌더링 - SpriteRenderer Component 가 담당 (PlayerHands 는 부착만).
 *
 *  ### 정통 매핑
 *  - Cocos2D @c Sprite::setPosition + @c addChild 패턴: 자식 actor 에 Component 부착 후
 *    부모 회전을 상속(scene graph WorldMatrix).
 *
 * @note 배치 수치(@c HAND_SPREAD_DEG / @c HAND_RADIUS / @c HAND_Y_OFFSET / @c HAND_SCALE 등)는
 *       @c Entity/Constants.h 단일 소스. 두 클래스 모두 이 상수를 기본값으로 사용한다.
 * @note 손 child actor 의 scale(@c HAND_SCALE)은 @c PlayerSingleHand::ApplyLocalOffset 에서
 *       @c Transform::Scale 로 세팅 - Translate(궤도 오프셋)와 독립적으로 처리된다.
 */
#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__

#include "scene/actor.h"
#include "Contracts/EntityContracts.h" // IFireTrigger (좌클릭 발사 통지 계약)
#include "Entity/Constants.h"
#include "Tween/TweenPlayable.h" // 발사 핀치 복귀 (Tweeny Playable)
#include <memory>
#include <glm/glm.hpp>

namespace TopdownShooter::Controller
{
	class PlayerController; // 손 spread 거리 보간 - aim 거리 읽기용 (포인터 멤버, 전방 선언)
}
// CLAUDE ASSIST
namespace TopdownShooter::Entity
{
	/**
	 * @brief 손 1개 Component - owner(자식 Hand actor)의 local Transform 을 forward(-Z) 기준 고정 +/-벌림각 위치로 배치.
	 * @details
	 *  궤도(조준 방향 추종)는 부모 player actor 의 EulerRot.Y 를 scene graph 가 WorldMatrix 합성으로
	 *  상속해 자동 처리한다. 따라서 본 Component 는 고정 local 위치만 1회 세팅 - 각도 자체계산 없음
	 *  (이전 OrbitWithYAngle 의 acos 스텁 제거).
	 *
	 *  local 위치 공식: ( sin(spread)*r, yOffset, -cos(spread)*r )  -- forward = -Z
	 *
	 *  @c PlayerHands 가 매 프레임 @c SetSpreadDeg 를 호출해 aim 거리 보간 값을 주입하며,
	 *  본 Component 는 즉시 @c ApplyLocalOffset 으로 위치를 재적용한다.
	 */
	class PlayerSingleHand : public SJH::Scene::Component
	{
	  private:
		float mSpreadRad; ///< forward(-Z) 기준 좌(+)/우(-) 벌림각 (radian). SetSpreadDeg 로 갱신.
		float mRadius;    ///< forward 거리 (player local 단위). 궤도 반지름.
		float mYOffset;   ///< 높이 오프셋 (player local Y). 손을 몸통 위/아래로 띄움.
		float mScale;     ///< 손 스프라이트 균등 Scale - Translate(궤도)와 독립, ApplyLocalOffset 에서 세팅.

	  public:
		/// @brief 벌림각/거리/높이/크기 초기화.
		/// @param spreadDeg forward(-Z) 기준 좌(+)/우(-) 벌림각 (degree). 좌손 +, 우손 -.
		/// @param radius    forward 거리 (player local).
		/// @param yOffset   높이 오프셋.
		/// @param scale     손 스프라이트 균등 Scale (Translate 와 직교 - 궤도에 영향 없음).
		explicit PlayerSingleHand(float spreadDeg = HAND_SPREAD_DEG, float radius = HAND_RADIUS, float yOffset = HAND_Y_OFFSET, float scale = 1.0f);

		/// @brief 고정 local 위치 1회 세팅 - ApplyLocalOffset 호출.
		void OnEnter() override;
		void OnExit() override {}
		/// @brief no-op - 위치 갱신은 PlayerHands 가 SetSpreadDeg 로 주도한다.
		void Update(float dt) override {}

		/// @brief 벌림각 갱신 - 부호 포함 degree (좌손 +, 우손 -). 즉시 local 위치 재적용.
		/// @details 거리 보간(@c PlayerHands::Update)이 매 프레임 호출. Scale/궤도(부모 회전)와 직교.
		/// @param spreadDeg 새 벌림각 (degree, 부호 포함).
		void SetSpreadDeg(float spreadDeg);

	  private:
		/// @brief mSpreadRad/mRadius/mYOffset/mScale 로 owner Transform 을 즉시 갱신.
		void ApplyLocalOffset();
	};

	/**
	 * @brief 양손 조립 Component - player actor 에 부착 -> OnEnter 에서 자식 Hand actor 2개 생성/AddChild.
	 * @details
	 *  각 자식 actor 는 PlayerSingleHand(고정 +-벌림각 local) + 빌보드 SpriteRenderer(HAND_PART) 를 보유.
	 *  부모(player) Y facing 을 상속해 양손 위치가 조준 방향으로 자동 궤도(WorldMatrix 합성). 스프라이트는
	 *  빌보드라 항상 카메라를 향한다. HAND_PART 텍스처는 Playable/Constants.h 정의(양손 공유 atlas 캐시).
	 *
	 *  매 프레임 흐름:
	 *  1. @c PlayerController::GetAimScreenT() 로 NDC 거리(0~1) 획득.
	 *  2. t 를 half-angle 로 선형 보간 (MAX~MIN).
	 *  3. 발사 핀치 TweenPlayable 이 활성이면 mFireBlend 를 1->0 으로 감쇠 후 halfDeg 를 MIN 방향으로 blend.
	 *  4. 좌손(+halfDeg)/우손(-halfDeg) 각각 SetSpreadDeg.
	 *
	 *  @note spread/radius/yOffset/QueueOffset 수치는 @c Entity/Constants.h (HAND_*) 단일 소스.
	 */
	class PlayerHands : public SJH::Scene::Component, public IFireTrigger
	{
	  public:
		/// @brief 손 child 액터를 부착할 orbit parent 주입 (선택적). 미주입이면 owner(root).
		/// @details @p orbitParent 주입 시 손이 root 대신 aimPivot 회전을 상속
		///          (데칼 spin 분리 후에도 궤도 유지). @c PlayerHands Component 자체는 owner(root)에 유지.
		/// @param orbitParent 손 child actor 를 AddChild 할 부모. nullptr 이면 owner(root) 사용.
		explicit PlayerHands(SJH::Scene::Actor *orbitParent = nullptr) : mOrbitParent(orbitParent) {}

		/// @brief 자식 Hand actor 2개 생성/부착 (+ HAND_PART 스프라이트 세팅).
		/// @details Actor::mEntered 가드 덕에 player OnEnter 중 AddChild 해도 이중 OnEnter 없음.
		void OnEnter() override;
		void OnExit() override {}

		/// @brief 매 프레임 aim 거리로 양손 spread 보간 + 발사 핀치 TweenPlayable 구동.
		/// @param dt 직전 프레임 경과 시간(초).
		void Update(float dt) override;

		/// @brief 좌클릭 발사 - 양팔을 즉시 최소각(15deg)으로 핀치한 뒤 @c HAND_FIRE_PINCH_MS 동안
		///        거리 기반 각도로 Tweeny 복귀. 클릭마다 트윈 재생성(재시작).
		void TriggerFire() override;

	  private:
		SJH::Scene::Actor *mOrbitParent = nullptr; ///< 손 child 부착 대상 (nullptr -> owner). 데칼 spin 분리 후 손 궤도 유지.

		// OnEnter 에서 생성한 자식 손 컴포넌트 (비소유 - 자식 Actor 가 소유).
		PlayerSingleHand *mLeft  = nullptr; ///< 좌손 Component 비소유 핸들 (자식 Actor 소유).
		PlayerSingleHand *mRight = nullptr; ///< 우손 Component 비소유 핸들 (자식 Actor 소유).

		// 형제(같은 player actor) PlayerController - aim 거리 출처. lazy 캐시.
		Controller::PlayerController *mController = nullptr; ///< 형제 PlayerController lazy 캐시 - aim 거리(GetAimScreenT) 출처.

		// 발사 핀치 - 1(완전 핀치=15deg) -> 0(거리기반) 으로 Tween 감쇠. finalHalf = lerp(거리기반, MIN, blend).
		float mFireBlend = 0.0f;                                     ///< 핀치 블렌드 비율 (1=완전핀치, 0=거리기반). TweenPlayable 이 1->0 감쇠.
		std::unique_ptr<Tween::TweenPlayable<float>> mFireTween;     ///< 핀치 복귀 트윈 (클릭마다 재생성 - one-shot 재시작).
	};
}; // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER_HAND__
