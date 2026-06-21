/**
 * @file PhysicsRaycast.cpp
 * @brief PhysicsRaycast.h 에 선언된 3종 레이캐스트 함수 구현.
 *
 * @details
 *  ### 내부 구조
 *  - @c NormalizeDir: dir 정규화 + 영벡터 판정 헬퍼.
 *  - @c ActorOf: body userdata 포인터 -> @c SJH::Scene::Actor* 복원 헬퍼.
 *  - @c ClosestCallback: @c b2RayCastCallback 상속 -- @c ReportFixture 가 fraction 반환
 *    -> Box2D 가 더 먼 fixture 를 자동 clip -> 최근접 1개 보장.
 *  - @c AllCallback: @c b2RayCastCallback 상속 -- @c ReportFixture 가 1.0 반환
 *    -> 관통(모든 fixture 수집). 결과는 fraction 오름차순 정렬 + body 단위 dedup.
 *
 *  ### 필터 공통 로직 (ClosestCallback / AllCallback 동일)
 *  1. isSensor 이고 @c hitSensors=false 이면 -1.0 반환(무시, 탐색 계속).
 *  2. @c categoryBits & @c maskBits == 0 이면 -1.0 반환.
 *  3. body userdata 의 Actor 가 @c ignore 와 같으면 -1.0 반환(자해 방지).
 */
#include "Physics/PhysicsRaycast.h"
#include <algorithm>
#include <cmath>

namespace TopdownShooter::Physics
{
	namespace
	{
		// dir 정규화 + 길이 0 판정. ok=false 면 호출측이 miss 반환.
		bool NormalizeDir(glm::vec2 dir, glm::vec2& out)
		{
			float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
			if (len <= 1e-8f) return false;
			out = glm::vec2(dir[0] / len, dir[1] / len);
			return true;
		}

		// body userdata 에 저장된 owner Actor* 복원 (pointer=0 이면 nullptr).
		SJH::Scene::Actor* ActorOf(b2Body* body)
		{
			return reinterpret_cast<SJH::Scene::Actor*>(body->GetUserData().pointer);
		}

		// b2World::RayCast 최근접 closest-hit 관용구 콜백.
		class ClosestCallback : public b2RayCastCallback
		{
		  public:
			uint16_t           maskBits   = 0;
			SJH::Scene::Actor* ignore     = nullptr;
			bool               hitSensors = false;
			RaycastHit         result;

			float ReportFixture(b2Fixture* fx, const b2Vec2& point,
			                    const b2Vec2& normal, float fraction) override
			{
				// 필터 - 하나라도 걸리면 -1 (이 fixture 무시, 탐색 계속)
				if (!hitSensors && fx->IsSensor())
					return -1.0f;
				if ((fx->GetFilterData().categoryBits & maskBits) == 0)
					return -1.0f;
				SJH::Scene::Actor* a = ActorOf(fx->GetBody());
				if (ignore != nullptr && a == ignore)
					return -1.0f;

				// 후보 기록 후 fraction 반환 -> Box2D 가 더 먼 fixture 를 자동 클립 = 최근접 보장.
				result.body     = fx->GetBody();
				result.actor    = a;
				result.point    = glm::vec2(point.x, point.y);
				result.normal   = glm::vec2(normal.x, normal.y);
				result.fraction = fraction;
				result.hit      = true;
				return fraction;
			}
		};

		// b2World::RayCast 전체수집 콜백 - ReportFixture 가 1.0 반환해 더 먼 fixture 도 계속 수집(관통).
		class AllCallback : public b2RayCastCallback
		{
		  public:
			uint16_t                maskBits   = 0;
			SJH::Scene::Actor*      ignore     = nullptr;
			bool                    hitSensors = false;
			std::vector<RaycastHit> results;

			float ReportFixture(b2Fixture* fx, const b2Vec2& point,
			                    const b2Vec2& normal, float fraction) override
			{
				if (!hitSensors && fx->IsSensor())
					return -1.0f;
				if ((fx->GetFilterData().categoryBits & maskBits) == 0)
					return -1.0f;
				SJH::Scene::Actor* a = ActorOf(fx->GetBody());
				if (ignore != nullptr && a == ignore)
					return -1.0f;

				RaycastHit h;
				h.body     = fx->GetBody();
				h.actor    = a;
				h.point    = glm::vec2(point.x, point.y);
				h.normal   = glm::vec2(normal.x, normal.y);
				h.fraction = fraction;
				h.hit      = true;
				results.push_back(h);
				return 1.0f; // 계속 - 더 먼 fixture 도 수집 (최근접 clip 안 함)
			}
		};
	} // anonymous namespace

	RaycastHit Raycast(b2Body* body, glm::vec2 start, glm::vec2 dir, float maxDistance)
	{
		RaycastHit  result;
		glm::vec2 d;
		if (body == nullptr || maxDistance <= 0.0f || !NormalizeDir(dir, d))
			return result;

		b2RayCastInput in;
		in.p1.Set(start[0], start[1]);
		in.p2.Set(start[0] + d[0] * maxDistance, start[1] + d[1] * maxDistance);
		in.maxFraction = 1.0f;

		float  bestFraction = 1.0f;
		b2Vec2 bestNormal(0.0f, 0.0f);
		bool   found        = false;

		for (b2Fixture* fx = body->GetFixtureList(); fx != nullptr; fx = fx->GetNext())
		{
			int32 childCount = fx->GetShape()->GetChildCount();
			for (int32 child = 0; child < childCount; ++child)
			{
				b2RayCastOutput out;
				if (fx->RayCast(&out, in, child) && out.fraction <= bestFraction)
				{
					bestFraction = out.fraction;
					bestNormal   = out.normal;
					found        = true;
				}
			}
		}

		if (found)
		{
			result.body     = body;
			result.actor    = ActorOf(body);
			result.fraction = bestFraction;
			result.distance = maxDistance * bestFraction;
			result.point    = glm::vec2(start[0] + d[0] * result.distance,
			                              start[1] + d[1] * result.distance);
			result.normal   = glm::vec2(bestNormal.x, bestNormal.y);
			result.hit      = true;
		}
		return result;
	}

	RaycastHit RaycastClosest(b2World& world, glm::vec2 start, glm::vec2 dir,
	                          float maxDistance, PhysicsLayer mask,
	                          SJH::Scene::Actor* ignore, bool hitSensors)
	{
		glm::vec2 d;
		if (maxDistance <= 0.0f || !NormalizeDir(dir, d))
			return RaycastHit{};

		ClosestCallback cb;
		cb.maskBits   = ToBits(mask);
		cb.ignore     = ignore;
		cb.hitSensors = hitSensors;

		b2Vec2 p1(start[0], start[1]);
		b2Vec2 p2(start[0] + d[0] * maxDistance, start[1] + d[1] * maxDistance);
		world.RayCast(&cb, p1, p2);

		if (cb.result.hit)
		{
			cb.result.distance = maxDistance * cb.result.fraction;
			cb.result.point    = glm::vec2(start[0] + d[0] * cb.result.distance,
			                                 start[1] + d[1] * cb.result.distance);
		}
		return cb.result;
	}

	std::vector<RaycastHit> RaycastAll(b2World& world, glm::vec2 start, glm::vec2 dir,
	                                   float maxDistance, PhysicsLayer mask,
	                                   SJH::Scene::Actor* ignore, bool hitSensors)
	{
		std::vector<RaycastHit> out;
		glm::vec2 d;
		if (maxDistance <= 0.0f || !NormalizeDir(dir, d))
			return out;

		AllCallback cb;
		cb.maskBits   = ToBits(mask);
		cb.ignore     = ignore;
		cb.hitSensors = hitSensors;

		b2Vec2 p1(start[0], start[1]);
		b2Vec2 p2(start[0] + d[0] * maxDistance, start[1] + d[1] * maxDistance);
		world.RayCast(&cb, p1, p2);

		// 거리(fraction) 오름차순 정렬.
		std::sort(cb.results.begin(), cb.results.end(),
		          [](const RaycastHit& a, const RaycastHit& b) { return a.fraction < b.fraction; });

		// body 단위 dedup (가장 가까운 hit 1개만 유지) + distance/point 보정.
		out.reserve(cb.results.size());
		for (auto& h : cb.results)
		{
			bool dup = false;
			for (const auto& kept : out)
				if (kept.body == h.body) { dup = true; break; }
			if (dup) continue;
			h.distance = maxDistance * h.fraction;
			h.point    = glm::vec2(start[0] + d[0] * h.distance, start[1] + d[1] * h.distance);
			out.push_back(h);
		}
		return out;
	}
}; // namespace TopdownShooter::Physics
