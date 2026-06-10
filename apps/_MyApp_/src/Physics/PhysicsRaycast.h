/**
 * @file PhysicsRaycast.h
 * @brief Box2D b2World::RayCast 래퍼 -- 단일 body / 월드 최근접 / 월드 전체 관통 질의 3종.
 *
 * @details
 *  ### 책임
 *  - @c RaycastHit 결과 구조체 정의 (body, actor, point, normal, distance, fraction, hit).
 *  - @c Raycast(b2Body*): 단일 body 의 모든 fixture 순회 -> 가장 가까운 교점.
 *  - @c RaycastClosest(b2World&): 월드 전체 최근접 hit -- Unity @c Physics.Raycast 정통.
 *  - @c RaycastAll(b2World&): 경로상 모든 fixture 수집, body 단위 dedup, fraction 오름차순 --
 *    Unity @c Physics.RaycastAll 정통.
 *  - @c PhysicsLayer mask 로 레이어 필터링 + @p ignore Actor 로 자해 방지.
 *
 *  ### 비-책임
 *  - [X] 결과 3D 변환 (-y, heightOffset) -- 호출측 책임.
 *  - [X] b2World 소유 / Step -- @c PhysicsSystem 담당.
 *
 *  ### 정통 매핑
 *  - Unity @c Physics2D.Raycast / @c Physics2D.RaycastAll.
 *  - Unreal @c LineTraceSingleByChannel / @c LineTraceMultiByChannel.
 *
 * @note 입출력 좌표는 모두 물리 2D 평면 (x, y).
 *       렌더 3D 변환 (-y -> z, heightOffset -> y) 은 호출측이 수행한다.
 */
#ifndef _TOPDOWNSHOOTER_PHYSICS_RAYCAST__
#define _TOPDOWNSHOOTER_PHYSICS_RAYCAST__
#include "Physics/PhysicsLayer.h"
#include <box2d/box2d.h>
#include <vector>
#include <vmath.h>

namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Physics
{
	/**
	 * @brief 레이캐스트 결과 구조체 -- Unity @c RaycastHit2D / Unreal @c FHitResult 정통.
	 * @details
	 *  @c fraction = [0, 1] 은 start->end 구간 대비 비율. @c distance = fraction * maxDistance.
	 *  입출력 좌표 모두 물리 2D 평면 (x, y).
	 *  렌더 3D 변환 (-y -> z, heightOffset -> y) 은 호출측 책임.
	 *  @c operator bool() 으로 hit 여부를 직접 조건식에 사용 가능.
	 */
	struct RaycastHit
	{
		b2Body*            body     = nullptr;                  ///< 맞은 body (miss 면 nullptr).
		SJH::Scene::Actor* actor    = nullptr;                  ///< body userdata 에서 복원한 owner Actor (없으면 nullptr).
		vmath::vec2        point    = vmath::vec2(0.0f, 0.0f);  ///< 월드 충돌 좌표 (물리 2D 평면).
		vmath::vec2        normal   = vmath::vec2(0.0f, 0.0f);  ///< 충돌 표면 법선 벡터 (물리 2D 평면).
		float              distance = 0.0f;                     ///< start 로부터 실제 거리 (= fraction * maxDistance).
		float              fraction = 0.0f;                     ///< maxDistance 대비 [0, 1] 비율.
		bool               hit      = false;                    ///< 명시적 성공 플래그. false 이면 miss.
		explicit operator bool() const { return hit; }
	};

	/// @brief 단일 @c b2Body 술어 -- Unity @c Collider.Raycast 정통.
	/// @details @p body 의 모든 fixture 를 순회해 가장 가까운 교점 1개를 반환.
	///          월드 레이어 필터 없이 특정 body 만 테스트할 때 사용.
	/// @param body        검사할 대상 body (nullptr 이면 miss 반환).
	/// @param start       레이 시작점 (물리 2D 평면).
	/// @param dir         방향 (임의 길이 허용 -- 내부 정규화). @c maxDistance 가 실제 거리 한계.
	/// @param maxDistance 최대 검사 거리.
	/// @return 가장 가까운 @c RaycastHit. miss 이면 @c hit=false.
	RaycastHit Raycast(b2Body* body,
	                   vmath::vec2 start, vmath::vec2 dir, float maxDistance);

	/// @brief 월드 최근접 단일 hit 질의 -- Unity @c Physics2D.Raycast / Unreal @c LineTraceSingleByChannel 정통.
	/// @details 내부적으로 @c b2World::RayCast + @c ClosestCallback 을 사용.
	///          @c ReportFixture 가 fraction 을 반환해 Box2D 가 더 먼 fixture 를 자동 clip -> 최근접 보장.
	/// @param world       질의 대상 @c b2World.
	/// @param start       레이 시작점 (물리 2D 평면).
	/// @param dir         방향 (임의 길이 허용 -- 내부 정규화).
	/// @param maxDistance 최대 검사 거리.
	/// @param mask        맞출 레이어 (@c categoryBits & @c ToBits(mask) != 0 인 fixture 만 후보).
	/// @param ignore      제외할 Actor (쏘는 주체 자해 방지). nullptr 이면 무시 안 함.
	/// @param hitSensors  false(기본) 이면 isSensor fixture 통과 (Unity @c QueryTriggerInteraction.Ignore).
	/// @return 최근접 @c RaycastHit. miss 이면 @c hit=false.
	RaycastHit RaycastClosest(b2World& world,
	                          vmath::vec2 start, vmath::vec2 dir, float maxDistance,
	                          PhysicsLayer mask,
	                          SJH::Scene::Actor* ignore = nullptr,
	                          bool hitSensors = false);

	/// @brief 월드 전체 관통 질의 -- 경로상 모든 fixture 수집 (Unity @c Physics2D.RaycastAll 정통).
	/// @details @c ClosestCallback (fraction 반환 = clip) 과 달리 @c AllCallback 은 항상 1.0 반환 ->
	///          더 먼 fixture 도 계속 수집(관통).
	///          body 단위 dedup (한 body 가 여러 fixture 여도 최근접 1회만 유지) +
	///          거리(fraction) 오름차순 정렬 후 반환.
	/// @param world       질의 대상 @c b2World.
	/// @param start       레이 시작점 (물리 2D 평면).
	/// @param dir         방향 (임의 길이 허용 -- 내부 정규화).
	/// @param maxDistance 최대 검사 거리.
	/// @param mask        맞출 레이어 (예: @c PhysicsLayer::Enemy -- 벽 무시 관통).
	/// @param ignore      제외할 Actor. nullptr 이면 무시 안 함.
	/// @param hitSensors  false(기본) 이면 sensor fixture 통과.
	/// @return 경로상 모든 hit (fraction 오름차순, body 단위 dedup). miss 이면 빈 vector.
	std::vector<RaycastHit> RaycastAll(b2World& world,
	                                   vmath::vec2 start, vmath::vec2 dir, float maxDistance,
	                                   PhysicsLayer mask,
	                                   SJH::Scene::Actor* ignore = nullptr,
	                                   bool hitSensors = false);
}; // namespace TopdownShooter::Physics

#endif //_TOPDOWNSHOOTER_PHYSICS_RAYCAST__
