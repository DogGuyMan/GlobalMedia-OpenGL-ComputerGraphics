#include "Entity/Player/PlayerHand.h"

#include "object/transform.h"
#include <cmath>
#include <memory>
#include <utility>
#include <vmath.h>

namespace TopdownShooter::Entity
{
	PlayerSingleHand::PlayerSingleHand(float spreadDeg, float radius, float yOffset)
	    : mSpreadRad(vmath::radians(spreadDeg)), mRadius(radius), mYOffset(yOffset)
	{
	}

	void PlayerSingleHand::OnEnter()
	{
		ApplyLocalOffset();
	}

	void PlayerSingleHand::ApplyLocalOffset()
	{
		auto *owner = GetOwner();
		if (owner == nullptr)
			return;
		// forward = -Z. local = (sin(spread)*r, yOffset, -cos(spread)*r).
		owner->GetTransform().Translate = vmath::vec3(
		    std::sin(mSpreadRad) * mRadius,
		    mYOffset,
		    -std::cos(mSpreadRad) * mRadius);
	}

	void PlayerHands::OnEnter()
	{
		auto *owner = GetOwner();
		if (owner == nullptr)
			return;

		// 자식 Hand actor 2개 — 각 PlayerSingleHand. 부모(player) Y facing 상속으로 자동 궤도.
		// (AddChild 는 owner 가 entered 면 즉시 child OnEnter → ApplyLocalOffset. Actor::OnEnter 의
		//  mEntered 가드 덕에 player OnEnter 중 부착해도 이중 OnEnter 없음.)
		// TODO(user WIP): 손 스프라이트 — 각 Hand actor 에 SpriteRenderer(handAtlas) 추가.
		//                 atlas/프레임/오프셋 수치는 사용자 협의 후 확정.
		auto leftHand = std::make_unique<SJH::Scene::Actor>("LeftHand");
		leftHand->AddComponent<PlayerSingleHand>(+kSpreadDeg, kRadius, kYOffset);
		owner->AddChild(std::move(leftHand));

		auto rightHand = std::make_unique<SJH::Scene::Actor>("RightHand");
		rightHand->AddComponent<PlayerSingleHand>(-kSpreadDeg, kRadius, kYOffset);
		owner->AddChild(std::move(rightHand));
	}
} // namespace TopdownShooter::Entity
