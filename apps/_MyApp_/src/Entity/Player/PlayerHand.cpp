#include <GL/gl3w.h> // 반드시 최상단 — resource_registry.h→framebuffer.h→render_target.h→gl3w.h 보다 먼저.

#include "Entity/Player/PlayerHand.h"

#include "Playable/Constants.h"                  // TopdownShooter::Playable::HAND_PART
#include "object/transform.h"
#include "resource_registry/resource_registry.h" // ResourceRegistry / UniformAtlas
#include "sprite/sprite_component.h"              // SJH::Sprite::SpriteRenderer (빌보드)

#include <cmath>
#include <memory>
#include <spdlog/spdlog.h>
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

		// HAND_PART 아틀라스 (정적 1x1 빌보드) — key=path, 양손 공유 캐시.
		auto &reg = SJH::ResourceRegistry::Get();
		const auto &hp = TopdownShooter::Playable::HAND_PART;
		auto *atlas = reg.FindUniformAtlas(hp.TexturePath);
		if (atlas == nullptr)
			atlas = reg.CreateUniformAtlas(hp.TexturePath, hp.TexturePath, hp.ColCount, hp.RowCount);
		if (atlas == nullptr)
			spdlog::error("[hands] HAND_PART atlas load 실패: {}", hp.TexturePath);

		// 자식 Hand actor 2개 — 각 PlayerSingleHand(고정 ±벌림각 local) + 빌보드 SpriteRenderer.
		// 부모(player) Y facing 상속으로 양손 위치가 조준 방향으로 자동 궤도. 빌보드라 항상 카메라 향함.
		// (AddChild 가 owner(entered)면 즉시 child OnEnter → ApplyLocalOffset. Actor::OnEnter 의 mEntered
		//  가드 덕에 player OnEnter 중 부착해도 이중 OnEnter 없음.)
		auto makeHand = [&](const char *name, float spreadDeg) {
			SJH::Scene::Actor *hand =
			    owner->AddChild(std::make_unique<SJH::Scene::Actor>(name));
			hand->AddComponent<PlayerSingleHand>(spreadDeg, kRadius, kYOffset);
			if (atlas != nullptr)
			{
				auto *spr = hand->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
				spr->flipX = hp.Flip;
				spr->QueueOffset = kHandQueueOffset; // 플레이어 몸통 위 레이어 (튜닝 대상)
			}
		};
		makeHand("LeftHand", +kSpreadDeg);
		makeHand("RightHand", -kSpreadDeg);
	}
} // namespace TopdownShooter::Entity
