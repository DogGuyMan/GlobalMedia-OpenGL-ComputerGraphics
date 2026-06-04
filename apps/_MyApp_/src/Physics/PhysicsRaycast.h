#ifndef _TOPDOWNSHOOTER_PHYSICS_RAYCAST__
#define _TOPDOWNSHOOTER_PHYSICS_RAYCAST__
#include "Physics/PhysicsLayer.h"
#include <box2d/box2d.h>
#include <vector>
#include <vmath.h>

namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Physics
{
	/// @brief Unity RaycastHit / Unreal FHitResult 정통. fraction = [0,1] (start->end 비율).
	/// @details 입출력 모두 물리 2D 평면 (x, y). 렌더 3D 변환(-y, heightOffset)은 호출측 책임.
	struct RaycastHit
	{
		b2Body*            body     = nullptr;                  // 맞은 body (miss 면 nullptr)
		SJH::Scene::Actor* actor    = nullptr;                  // body userdata 에서 복원 (없으면 nullptr)
		vmath::vec2        point    = vmath::vec2(0.0f, 0.0f);  // 월드 충돌 좌표 (물리 2D)
		vmath::vec2        normal   = vmath::vec2(0.0f, 0.0f);  // 표면 법선
		float              distance = 0.0f;                     // start 로부터 실제 거리
		float              fraction = 0.0f;                     // maxDistance 대비 비율
		bool               hit      = false;                    // 명시적 성공 플래그
		explicit operator bool() const { return hit; }
	};

	/// @brief 단일 b2Body 술어 — Unity Collider.Raycast 정통.
	///        body 의 모든 fixture 를 순회해 가장 가까운 교점을 반환.
	/// @param dir 방향(임의 길이 허용 — 내부 정규화). maxDistance 가 실제 거리 한계.
	RaycastHit Raycast(b2Body* body,
	                   vmath::vec2 start, vmath::vec2 dir, float maxDistance);

	/// @brief 월드 최근접 질의 — Unity Physics.Raycast / Unreal LineTraceSingleByChannel 정통.
	/// @param mask       맞출 레이어 (categoryBits & ToBits(mask) != 0 만 후보)
	/// @param ignore     제외할 Actor (쏘는 주체 자해 방지). nullptr 면 무시 안 함.
	/// @param hitSensors false(기본) 면 isSensor fixture 통과 (Unity QueryTriggerInteraction.Ignore)
	RaycastHit RaycastClosest(b2World& world,
	                          vmath::vec2 start, vmath::vec2 dir, float maxDistance,
	                          PhysicsLayer mask,
	                          SJH::Scene::Actor* ignore = nullptr,
	                          bool hitSensors = false);

	/// @brief 월드 전체 관통 질의 — 경로상 모든 fixture 수집 (Unity Physics.RaycastAll 정통).
	/// @details ClosestCallback(fraction 반환=clip)과 달리 AllCallback 이 1.0 반환 → 더 먼 fixture 도 계속 수집.
	///          body 단위 dedup (한 body 가 여러 fixture 여도 1회) + 거리(fraction) 오름차순 정렬.
	/// @param mask  맞출 레이어 (예: PhysicsLayer::Enemy — 벽 무시 관통)
	std::vector<RaycastHit> RaycastAll(b2World& world,
	                                   vmath::vec2 start, vmath::vec2 dir, float maxDistance,
	                                   PhysicsLayer mask,
	                                   SJH::Scene::Actor* ignore = nullptr,
	                                   bool hitSensors = false);
}; // namespace TopdownShooter::Physics

#endif //_TOPDOWNSHOOTER_PHYSICS_RAYCAST__
