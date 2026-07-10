# Box2D Raycast 히트스캔 유틸리티 Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Box2D b2Body/b2World 에 대한 광선 교차 판정(히트스캔) 자유함수 2종을 `TopdownShooter::Physics` 에 추가한다.

**Architecture:** 신규 파일 `apps/_MyApp_/src/Physics/PhysicsRaycast.{h,cpp}`. 단일-바디 술어 `Raycast`(fixture 순회) + 월드 최근접 질의 `RaycastClosest`(`b2RayCastCallback` 서브클래스). 둘 다 `RaycastHit` 구조체 반환. `PhysicsSystem` 비침투 — `b2World&` 만 인자로 받음.

**Tech Stack:** C++17, Box2D 2.4.1 (`b2Fixture::RayCast` / `b2World::RayCast`), vmath, CMake (`myapp_physics` STATIC).

**테스트:** 사용자 결정으로 단위 테스트 제외. 검증은 빌드 성공(`_MyApp_` 컴파일)으로 갈음.

**커밋 규약:** 사용자 병렬 git 작업 중 — `git add`/`git commit` 은 **반드시 명시 경로만** 지정(인덱스 전체 커밋 금지). Co-Authored-By 트레일러 미사용.

**정본 spec:** `doc/superpowers/specs/2026-06-03-box2d-raycast-hitscan-design.md`

---

### Task 1: PhysicsRaycast 헤더 (RaycastHit + 선언)

**Files:**
- Create: `apps/_MyApp_/src/Physics/PhysicsRaycast.h`

- [ ] **Step 1: 헤더 파일 작성**

`apps/_MyApp_/src/Physics/PhysicsRaycast.h` 전체 내용:

```cpp
#ifndef _TOPDOWNSHOOTER_PHYSICS_RAYCAST__
#define _TOPDOWNSHOOTER_PHYSICS_RAYCAST__
#include "apps/_MyApp_/src/Physics/PhysicsLayer.h"
#include <<box2d>/box2d.h>
#include <vmath.h>

namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Physics
{
	/// @brief Unity RaycastHit / Unreal FHitResult 정통. fraction = [0,1] (start→end 비율).
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
}; // namespace TopdownShooter::Physics

#endif //_TOPDOWNSHOOTER_PHYSICS_RAYCAST__
```

- [ ] **Step 2: 커밋**

```bash
git add apps/_MyApp_/src/Physics/PhysicsRaycast.h
git commit -m "[dev] : Box2D raycast 히트스캔 헤더 (RaycastHit + 자유함수 선언)"
```

---

### Task 2: PhysicsRaycast 구현 + CMake 등록 + 빌드 검증

**Files:**
- Create: `apps/_MyApp_/src/Physics/PhysicsRaycast.cpp`
- Modify: `apps/_MyApp_/src/Physics/CMakeLists.txt:1-4`

- [ ] **Step 1: 구현 파일 작성**

`apps/_MyApp_/src/Physics/PhysicsRaycast.cpp` 전체 내용:

```cpp
#include "apps/_MyApp_/src/Physics/PhysicsRaycast.h"
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

				// 후보 기록 후 fraction 반환 → Box2D 가 더 먼 fixture 를 자동 클립 = 최근접 보장.
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
```

- [ ] **Step 2: CMakeLists 에 소스 등록**

`apps/_MyApp_/src/Physics/CMakeLists.txt` 의 `add_library` 블록(1-4 줄)을 다음으로 교체:

```cmake
add_library(myapp_physics STATIC
    PhysicsSystem.cpp
    ContactListener.cpp
    PhysicsRaycast.cpp
)
```

(link 변경 불필요 — `game_deps`(Box2D) + `SJH::scene`(Actor) 이미 PUBLIC link.)

- [ ] **Step 3: 빌드 검증**

Run:
```bash
cmake --preset ninja && cmake --build --preset ninja --target _MyApp_
```
Expected: `PhysicsRaycast.cpp.o` 컴파일 + 링크 성공, exit 0, error 0.

- [ ] **Step 4: 커밋**

```bash
git add apps/_MyApp_/src/Physics/PhysicsRaycast.cpp apps/_MyApp_/src/Physics/CMakeLists.txt
git commit -m "[dev] : Box2D raycast 구현 (단일-바디 + 월드 최근접) + CMake 등록"
```

---

## Self-Review

**1. Spec coverage:**
- §3 배치(A안) → Task 1+2 (PhysicsRaycast.{h,cpp}, PhysicsSystem 비침투) ✅
- §4 API(RaycastHit + 두 함수 통일 반환) → Task 1 헤더 ✅
- §5 알고리즘(fixture 순회 / closest-hit 콜백) → Task 2 cpp ✅
- §6 엣지(널/길이0/maxDist<=0/userdata0/childCount) → `NormalizeDir` 가드 + childCount 루프 + `ActorOf` ✅
- §6 내부 dir 정규화 → `NormalizeDir` ✅
- §7 테스트 제외 → 빌드 검증으로 갈음 ✅
- §8 크로스플랫폼(uint16_t / `__..._` 가드 / GL 비의존) → 헤더 가드 + ToBits uint16 ✅

**2. Placeholder scan:** 모든 step 에 실제 코드/명령 포함. TBD/TODO 없음 ✅

**3. Type consistency:**
- `RaycastHit` 필드명(body/actor/point/normal/distance/fraction/hit) — Task1 정의 ↔ Task2 사용 일치 ✅
- `NormalizeDir(vmath::vec2, vmath::vec2&)→bool`, `ActorOf(b2Body*)→Actor*` — cpp 내부 일관 ✅
- `ToBits(PhysicsLayer)→uint16_t` — PhysicsLayer.h 기존 시그니처와 일치 ✅
- `b2RayCastInput{p1,p2,maxFraction}` / `b2RayCastOutput{normal,fraction}` / `b2Fixture::RayCast(out,in,child)` / `b2World::RayCast(cb,p1,p2)` — Box2D 2.4.1 API 일치 ✅
```
