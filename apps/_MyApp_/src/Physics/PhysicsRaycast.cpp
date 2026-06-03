#include "Physics/PhysicsRaycast.h"
#include <cmath>

namespace TopdownShooter::Physics
{
	namespace
	{
		// dir 정규화 + 길이 0 판정. ok=false 면 호출측이 miss 반환.
		bool NormalizeDir(vmath::vec2 dir, vmath::vec2& out)
		{
			float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
			if (len <= 1e-8f) return false;
			out = vmath::vec2(dir[0] / len, dir[1] / len);
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
				// 필터 — 하나라도 걸리면 -1 (이 fixture 무시, 탐색 계속)
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
				result.point    = vmath::vec2(point.x, point.y);
				result.normal   = vmath::vec2(normal.x, normal.y);
				result.fraction = fraction;
				result.hit      = true;
				return fraction;
			}
		};
	} // anonymous namespace

	RaycastHit Raycast(b2Body* body, vmath::vec2 start, vmath::vec2 dir, float maxDistance)
	{
		RaycastHit  result;
		vmath::vec2 d;
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
			result.point    = vmath::vec2(start[0] + d[0] * result.distance,
			                              start[1] + d[1] * result.distance);
			result.normal   = vmath::vec2(bestNormal.x, bestNormal.y);
			result.hit      = true;
		}
		return result;
	}

	RaycastHit RaycastClosest(b2World& world, vmath::vec2 start, vmath::vec2 dir,
	                          float maxDistance, PhysicsLayer mask,
	                          SJH::Scene::Actor* ignore, bool hitSensors)
	{
		vmath::vec2 d;
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
			cb.result.point    = vmath::vec2(start[0] + d[0] * cb.result.distance,
			                                 start[1] + d[1] * cb.result.distance);
		}
		return cb.result;
	}
}; // namespace TopdownShooter::Physics
