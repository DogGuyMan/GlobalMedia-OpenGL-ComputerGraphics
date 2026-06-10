/**
 * @file PlayerHand.cpp
 * @brief PlayerSingleHand / PlayerHands Component 구현 — 손 배치, spread 보간, 발사 핀치 연출.
 *
 * @details
 *  ### 책임
 *  - @c PlayerSingleHand::OnEnter / @c ApplyLocalOffset: forward(-Z) 기준 local 위치 공식으로
 *    owner Transform 을 초기 세팅. SetSpreadDeg 호출마다 즉시 재적용.
 *  - @c PlayerHands::OnEnter: ResourceRegistry 에서 HAND_PART atlas 를 획득(캐시 우선)한 뒤,
 *    람다 팩토리로 LeftHand/RightHand actor 를 orbitParent(또는 owner)에 AddChild.
 *    각 actor 에 PlayerSingleHand + SpriteRenderer 를 부착.
 *  - @c PlayerHands::Update: 형제 PlayerController lazy 캐시 -> GetAimScreenT 로 half-angle 보간 ->
 *    발사 핀치 TweenPlayable 구동 -> 좌/우손 SetSpreadDeg.
 *  - @c PlayerHands::TriggerFire: mFireBlend=1 즉시 설정 + 새 TweenPlayable(1->0, quadraticOut) 생성·Play.
 *
 *  ### 비-책임
 *  - [X] 조준 방향 자체 계산 — 부모 Y 회전(scene graph)이 자동 처리.
 *  - [X] 스프라이트 렌더링 — SpriteRenderer 가 담당.
 *
 * @note GLFW_INCLUDE_NONE 을 모든 include 이전에 선언하는 이유:
 *       PlayerController.h -> mouse_input.h -> <GLFW/glfw3.h> 가 자체 GL 헤더를 끌어와
 *       gl3w 와 PFNGL* 심볼 충돌을 일으킨다. NONE 선언으로 GLFW 자체 GL 로딩을 비활성화한다.
 */
// PlayerController.h -> mouse_input.h 의 <GLFW/glfw3.h> 가 자체 GL 헤더를 끌어와 gl3w 와 PFNGL* 충돌하지
// 않도록, *모든 include 이전* 에 NONE 선언.
#define GLFW_INCLUDE_NONE

#include <GL/gl3w.h> // 반드시 최상단 — resource_registry.h->framebuffer.h->render_target.h->gl3w.h 보다 먼저.

#include "Entity/Player/PlayerHand.h"

#include "InputHandler/PlayerController.h"        // 형제 컴포넌트 — aim 거리(GetAimDistance) 읽기
#include "Playable/Constants.h"                  // TopdownShooter::Playable::HAND_PART
#include "Entity/Constants.h"                     // HAND_* (배치 튜닝 단일 소스)
#include "object/transform.h"
#include "resource_registry/resource_registry.h" // ResourceRegistry / UniformAtlas
#include "sprite/sprite_component.h"              // SJH::Sprite::SpriteRenderer (빌보드)

#include <cmath>
#include <memory>
#include <spdlog/spdlog.h>
#include <vmath.h>

namespace TopdownShooter::Entity
{
	PlayerSingleHand::PlayerSingleHand(float spreadDeg, float radius, float yOffset, float scale)
	    : mSpreadRad(vmath::radians(spreadDeg)), mRadius(radius), mYOffset(yOffset), mScale(scale)
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
		auto &tr = owner->GetTransform();
		// forward = -Z. local = (sin(spread)*r, yOffset, -cos(spread)*r).
		tr.Translate = vmath::vec3(
		    std::sin(mSpreadRad) * mRadius,
		    mYOffset,
		    -std::cos(mSpreadRad) * mRadius);
		// 시각 크기 — Translate(궤도 오프셋)와 직교. SetTransformWithVectors 로 한꺼번에 세팅하면
		// Translate 가 (0,0,0) 으로 덮여 궤도가 깨지므로, Scale 만 별도로 둔다 (단일 소유).
		tr.Scale = vmath::vec3(mScale, mScale, mScale);
	}

	void PlayerSingleHand::SetSpreadDeg(float spreadDeg)
	{
		mSpreadRad = vmath::radians(spreadDeg);
		ApplyLocalOffset(); // 각도 갱신 즉시 local 위치 재적용 (Scale/궤도와 직교)
	}

	void PlayerHands::OnEnter()
	{
		auto *owner = GetOwner();
		if (owner == nullptr)
			return;

		// HAND_PART 아틀라스 (정적 1x1 빌보드) — key=path, 양손 공유 캐시.
		auto &reg = SJH::ResourceRegistry::Get();
		const auto &hp = TopdownShooter::Playable::PLAYER_HAND_PART;
		auto *atlas = reg.FindUniformAtlas(hp.TexturePath);
		if (atlas == nullptr)
			atlas = reg.CreateUniformAtlas(hp.TexturePath, hp.TexturePath, hp.ColCount, hp.RowCount);
		if (atlas == nullptr)
			spdlog::error("[hands] HAND_PART atlas load 실패: {}", hp.TexturePath);

		// 자식 Hand actor 2개 — 각 PlayerSingleHand(고정 ±벌림각 local) + 빌보드 SpriteRenderer.
		// 부모(player) Y facing 상속으로 양손 위치가 조준 방향으로 자동 궤도. 빌보드라 항상 카메라 향함.
		// (AddChild 가 owner(entered)면 즉시 child OnEnter -> ApplyLocalOffset. Actor::OnEnter 의 mEntered
		//  가드 덕에 player OnEnter 중 부착해도 이중 OnEnter 없음.)
		// 크기(HAND_SCALE)는 PlayerSingleHand 가 ApplyLocalOffset 에서 Scale 로 세팅 — Translate(궤도) 와 분리.
		// 반환된 컴포넌트 포인터를 보관 → Update 에서 거리 보간으로 spread 갱신.
		// 손 child 는 orbit parent(주입 시 aimPivot)에 부착 — 미주입이면 owner(root).
		// (PlayerHands 컴포넌트는 root 유지 → controller↔hands 양방향 조회 무수정. 손 위치만 aimPivot 궤도.)
		SJH::Scene::Actor *handParent = (mOrbitParent != nullptr) ? mOrbitParent : owner;
		auto makeHand = [&](const char *name, float spreadDeg) -> PlayerSingleHand * {
			SJH::Scene::Actor *hand =
			    handParent->AddChild(std::make_unique<SJH::Scene::Actor>(name));
			auto *hc = hand->AddComponent<PlayerSingleHand>(spreadDeg, HAND_RADIUS, HAND_Y_OFFSET, HAND_SCALE);
			if (atlas != nullptr)
			{
				auto *spr = hand->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
				spr->flipX = hp.Flip;
				spr->QueueOffset = HAND_QUEUE_OFFSET; // 플레이어 몸통 위 레이어
			}
			return hc;
		};
		mLeft  = makeHand("LeftHand", +HAND_SPREAD_DEG);
		mRight = makeHand("RightHand", -HAND_SPREAD_DEG);
	}

	void PlayerHands::Update(float dt)
	{
		auto *owner = GetOwner();
		if (owner == nullptr || mLeft == nullptr || mRight == nullptr)
			return;
		// 형제(같은 player actor) PlayerController lazy 캐시 — aim 거리 출처.
		if (mController == nullptr)
			mController = owner->GetComponent<Controller::PlayerController>();
		if (mController == nullptr)
			return;

		// 화면(NDC) 거리 → half-angle 보간. t=0(커서가 플레이어 화면위치 위) → MAX(90°: 양팔 180°),
		// t=1(커서 화면 가장자리) → MIN(7.5°: 양팔 15°). GetAimScreenT 는 이미 0~1 포화·정규화.
		const float t = mController->GetAimScreenT();
		float halfDeg = HAND_HALF_ANGLE_MAX + (HAND_HALF_ANGLE_MIN - HAND_HALF_ANGLE_MAX) * t;

		// 발사 핀치 — TweenPlayable 이 mFireBlend 를 1→0 으로 감쇠. blend 만큼 MIN(7.5°)으로 좁힘.
		// blend=1: finalHalf=MIN(완전 핀치=15°), blend=0: 거리기반 halfDeg.
		if (mFireTween && !mFireTween->IsFinished())
			mFireTween->Update(dt); // onStep 이 mFireBlend 갱신
		halfDeg += (HAND_HALF_ANGLE_MIN - halfDeg) * mFireBlend;

		mLeft->SetSpreadDeg(+halfDeg);  // 좌손 +
		mRight->SetSpreadDeg(-halfDeg); // 우손 -
	}

	void PlayerHands::TriggerFire()
	{
		mFireBlend = 1.0f; // 클릭 즉시 완전 핀치(15°)
		// 1→0 (HAND_FIRE_PINCH_MS) — quadraticOut: 빠르게 풀렸다 끝에서 부드럽게 정착.
		// 클릭마다 새 트윈 생성 = 재시작(one-shot 의 Stop 은 tween progress 를 안 되돌리므로).
		auto tw = tweeny::from(1.0f).to(0.0f).during(HAND_FIRE_PINCH_MS).via(tweeny::easing::quadraticOut);
		mFireTween = std::make_unique<Tween::TweenPlayable<float>>(
		    std::move(tw), [this](float v) { mFireBlend = v; });
		mFireTween->Play();
	}
} // namespace TopdownShooter::Entity
