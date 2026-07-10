# Box2D Raycast 히트스캔 유틸리티 — 설계

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

- **날짜**: 2026-06-03
- **대상**: `apps/_MyApp_` (탑다운 슈터)
- **모듈**: `TopdownShooter::Physics` (Client 거주, `apps/_MyApp_/src/Physics/`)
- **목적**: FPS/탑다운 히트스캔(즉발 사격) 을 위한 광선-바디 교차 판정 유틸리티

## 1. 동기

발사 순간 광선을 쏘아 "무엇을 맞췄는가" 를 즉시 판정하는 히트스캔이 필요하다.
현재 `Physics` 모듈은 b2World Step + Contact 이벤트만 제공하며, 임의 광선 질의 수단이 없다.

엔진 정통 레퍼런스:
- **Unity** — `Physics.Raycast(origin, dir, out hit, maxDist, layerMask)` (월드), `Collider.Raycast(ray, out hit, maxDist)` (단일). `RaycastHit{collider, point, normal, distance}`. 트리거 처리 = `QueryTriggerInteraction`.
- **Unreal** — `LineTraceSingleByChannel(hit, start, end, channel, params)`. `FHitResult{Actor, ImpactPoint, ImpactNormal, Distance}`. ignore-actor 목록 지원.
- **Godot** — `PhysicsDirectSpaceState2D.intersect_ray(query)` → 결과 딕셔너리(`collider/position/normal`). `exclude` 리스트.

본 설계는 Unity 의 의미론(단위 dir + maxDistance 분리, RaycastHit 필드)을 따르되,
반환 규약은 두 함수 모두 `RaycastHit` 직접 반환으로 통일한다(Godot 철학).

## 2. 범위

- ✅ 단일 b2Body 술어 `Raycast`
- ✅ 월드 최근접 질의 `RaycastClosest`
- ❌ 관통 다중 히트 `RaycastAll` (YAGNI — 필요 시 후속)

## 3. 배치 (결정 A안)

- 신규 파일 `apps/_MyApp_/src/Physics/PhysicsRaycast.{h,cpp}`
- 네임스페이스 `TopdownShooter::Physics`
- 자유 함수 2종 (`FindPhysics` 자유함수 컨벤션 일치)
- `PhysicsSystem` **비침투** — `b2World&` 만 인자로 받음 (PhysicsSystem 헤더 오염 없음)
- CMake: `Physics/CMakeLists.txt` 에 `PhysicsRaycast.cpp` 1줄 추가

## 4. 데이터 구조 + 공개 API

```cpp
// PhysicsRaycast.h
#include "scene/actor.h"
#include "apps/_MyApp_/src/Physics/PhysicsLayer.h"
#include <<box2d>/box2d.h>
#include <vmath.h>

namespace TopdownShooter::Physics
{
    /// @brief Unity RaycastHit / Unreal FHitResult 정통. fraction = [0,1] (start→end 비율).
    struct RaycastHit
    {
        b2Body*            body     = nullptr;   // 맞은 body (miss 면 nullptr)
        SJH::Scene::Actor* actor    = nullptr;   // body userdata 에서 복원 (없으면 nullptr)
        vmath::vec2        point    = vmath::vec2(0.0f, 0.0f);  // 월드 충돌 좌표 (물리 2D)
        vmath::vec2        normal   = vmath::vec2(0.0f, 0.0f);  // 표면 법선
        float              distance = 0.0f;      // start 로부터 실제 거리
        float              fraction = 0.0f;      // maxDistance 대비 비율
        bool               hit      = false;     // 명시적 성공 플래그
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
}
```

### 규약

- `dir` 은 **내부에서 1회 정규화** (길이 0 가드 포함). 호출측이 비정규화 벡터를 넘겨도 `maxDistance` 가 진짜 거리 한계가 됨.
- `maxDistance <= 0` 또는 `dir` 길이 0 → 빈 `RaycastHit{hit=false}` 즉시 반환.
- `point = start + normalize(dir) * distance`, `distance = maxDistance * fraction`.
- 입출력 모두 **물리 2D 평면 (x, y)**. 렌더 3D 변환(`-y`, heightOffset)은 호출측 책임.
- `actor = reinterpret_cast<Actor*>(body->GetUserData().pointer)` — pointer=0 이면 nullptr.

## 5. 알고리즘 / 데이터 흐름

### 단일-바디 `Raycast` (콜백 불필요)

```
1. 가드: body==null || maxDist<=0 || len(dir)==0  →  빈 RaycastHit
2. d  = normalize(dir)
   p1 = start;  p2 = start + d*maxDist
3. b2RayCastInput in{ p1, p2, maxFraction = 1.0 }
4. bestFraction = +inf
   for fixture in body->GetFixtureList():
       for childIdx in [0, fixture->GetShape()->GetChildCount()):   // chain/edge 대비
           b2RayCastOutput out;
           if fixture->RayCast(&out, in, childIdx) && out.fraction < bestFraction:
               bestFraction = out.fraction;  bestNormal = out.normal
5. 히트 시 RaycastHit 채움:
       distance = maxDist * bestFraction
       point    = start + d * distance
       normal   = bestNormal;  fraction = bestFraction
       body, actor(userdata), hit = true
```

### 월드 `RaycastClosest` (b2RayCastCallback 서브클래스, .cpp 내부)

```cpp
struct ClosestCallback : b2RayCastCallback {
    uint16_t           maskBits;
    SJH::Scene::Actor* ignore;
    bool               hitSensors;
    RaycastHit         result;   // 콜백이 채움

    float ReportFixture(b2Fixture* fx, const b2Vec2& point,
                        const b2Vec2& normal, float fraction) override {
        // 필터 — 하나라도 걸리면 -1 (이 fixture 무시, 탐색 계속)
        if (!hitSensors && fx->IsSensor())                          return -1.0f;
        if ((fx->GetFilterData().categoryBits & maskBits) == 0)     return -1.0f;
        auto* a = reinterpret_cast<SJH::Scene::Actor*>(
                      fx->GetBody()->GetUserData().pointer);
        if (ignore && a == ignore)                                  return -1.0f;
        // 후보 기록 후 fraction 반환 → Box2D 가 더 먼 fixture 를 자동 클립 = 최근접 보장
        result.body = fx->GetBody();  result.actor = a;
        result.point = {point.x, point.y};  result.normal = {normal.x, normal.y};
        result.fraction = fraction;  result.hit = true;
        return fraction;
    }
};
// 호출부: 가드 → normalize → world.RayCast(&cb, p1, p2)
//         → cb.result.distance = maxDist * cb.result.fraction (히트 시)
```

핵심: 콜백이 `fraction` 을 반환하면 Box2D 가 광선을 그 지점까지 줄여 더 먼 fixture 를 배제 → 최근접 보장. `-1` 반환 = "이 fixture 패스, 탐색 지속". Box2D 공식 closest-hit 관용구.

## 6. 엣지 케이스

| 상황 | 처리 |
|---|---|
| `body==null` / `dir` 길이 0 / `maxDistance<=0` | 빈 `RaycastHit{hit=false}` 즉시 반환 (예외·로그 없음) |
| `dir` 비정규화 | 내부 1회 정규화로 흡수 |
| body userdata pointer=0 | `actor=nullptr`, `body` 는 채워짐 — 호출측 널 체크 |
| chain/edge shape | `GetChildCount()` 순회 (현재 box/circle child=1, 미래 대비) |
| 시작점이 fixture 내부 | Box2D 가 해당 fixture 히트 안 함(정상). 자기 몸 시작은 `ignore` 로 커버 |

## 7. 테스트

**제외** (프로젝트 컨벤션: 테스트는 사용자 요청 시에만 작성). raycast 자체는 헤드리스 b2World 로 GL 없이 검증 가능하므로, 추후 필요 시 `[raycast]` 태그로 6 케이스(정면 명중/빗나감/거리 밖/센서 통과/ignore 제외/최근접) 추가 가능.

## 8. 크로스 플랫폼

- 고정 크기 타입(`uint16_t`)만 사용, `long` 없음.
- 헤더 `#ifndef` 가드 (프로젝트 `__..._H__` / `_..._` 컨벤션).
- GL 비의존 — 순수 Box2D + vmath.

## 9. 비목표 (YAGNI)

- `RaycastAll` (관통 다중)
- 박스/스피어 캐스트(두께 있는 trace)
- 3D 광선 (게임은 물리 2D 평면)
