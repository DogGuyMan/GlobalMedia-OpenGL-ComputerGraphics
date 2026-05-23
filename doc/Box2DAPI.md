# Box2D v2.4.1 API 레퍼런스

이 문서는 `apps/box2d_demo/` 3개 데모(Tumbler / Car / Bridge)와 `box2d_common/debug_draw`에서  
실제로 사용된 API만 정리한 레퍼런스입니다.

---

## 전체 파이프라인

```
[초기화]
  b2World(gravity)
      └─ CreateBody(BodyDef)  ──→  b2Body
              └─ CreateFixture(Shape, density)
              └─ CreateFixture(FixtureDef)
  b2World::CreateJoint(JointDef)  ──→  b2Joint (RevoluteJoint / WheelJoint)
  b2World::SetDebugDraw(b2Draw*)

[매 프레임]
  b2World::Step(dt, velIter, posIter)   ← 물리 시뮬레이션 전진
  b2World::DebugDraw()                  ← b2Draw 콜백 발생
  DebugDraw::Flush()                    ← GPU 업로드 + 드로우콜

[리셋]
  b2World::GetBodyList() + GetNext() 순회 → DestroyBody(b) 전체 제거
```

---

## 1. b2World — 물리 세계

```cpp
b2World world(b2Vec2(0.0f, -10.0f));  // 중력 벡터로 생성
```

| 메서드 | 반환 | 설명 |
|--------|------|------|
| `CreateBody(b2BodyDef*)` | `b2Body*` | body 생성 |
| `DestroyBody(b2Body*)` | void | body + 연결 fixture/joint 전부 파괴 |
| `CreateJoint(b2JointDef*)` | `b2Joint*` | joint 생성 |
| `GetBodyList()` | `b2Body*` | body 연결 리스트 헤드 |
| `Step(dt, velIter, posIter)` | void | 시뮬레이션 1스텝 전진 |
| `SetDebugDraw(b2Draw*)` | void | 디버그 렌더러 등록 |
| `DebugDraw()` | void | 등록된 렌더러로 모든 shape/joint 드로우 |

**Step 권장값**

```cpp
m_world.Step(1.0f / 60.0f, 8, 3);
// velocityIterations=8, positionIterations=3 (Box2D 공식 권장)
```

---

## 2. b2BodyDef + b2Body — 강체

### b2BodyDef (설정 구조체)

```cpp
b2BodyDef bd;
bd.type       = b2_dynamicBody;   // b2_staticBody / b2_dynamicBody / b2_kinematicBody
bd.position.Set(0.0f, 10.0f);
bd.allowSleep = false;            // 정지해도 슬립 방지 (Tumbler 회전통에 사용)
```

| 타입 | 동작 |
|------|------|
| `b2_staticBody` | 움직이지 않음, 무한 질량 (기본값) |
| `b2_dynamicBody` | 힘/충돌에 반응 |
| `b2_kinematicBody` | 속도로만 이동, 힘에 반응 안 함 |

### b2Body (런타임 핸들)

```cpp
b2Body* body = world.CreateBody(&bd);

// fixture 부착 (단순)
body->CreateFixture(&shape, density);

// fixture 부착 (상세 설정)
b2FixtureDef fd;
fd.shape   = &shape;
fd.density  = 1.0f;
fd.friction = 0.9f;
body->CreateFixture(&fd);
```

| 메서드 | 반환 | 설명 |
|--------|------|------|
| `CreateFixture(shape*, density)` | `b2Fixture*` | shape + 밀도로 부착 |
| `CreateFixture(FixtureDef*)` | `b2Fixture*` | 상세 설정으로 부착 |
| `GetNext()` | `b2Body*` | 다음 body (연결 리스트 순회용) |
| `GetPosition()` | `b2Vec2` | 현재 위치 (Car 카메라 추적에 사용) |
| `GetMass()` | `float` | 질량 (WheelJoint 스프링 계수 계산에 사용) |

**Body 전체 삭제 패턴 (리셋 시)**

```cpp
for (b2Body* b = m_world.GetBodyList(); b; )
{
    b2Body* next = b->GetNext();
    m_world.DestroyBody(b);
    b = next;
}
```

---

## 3. Shape — 충돌 형상

### b2PolygonShape

```cpp
b2PolygonShape shape;

// 축 정렬 박스 (hx, hy = 반폭/반높이)
shape.SetAsBox(0.5f, 0.125f);

// 오프셋 박스 (center, angle)
shape.SetAsBox(10.0f, 0.5f, b2Vec2(0.0f, 10.0f), 0.0f);

// 임의 볼록 다각형 (최대 8정점)
b2Vec2 verts[6];
verts[0].Set(-1.5f, -0.5f); // ... 6개 정점
shape.Set(verts, 6);
```

### b2EdgeShape

```cpp
b2EdgeShape shape;
shape.SetTwoSided(b2Vec2(-40.0f, 0.0f), b2Vec2(40.0f, 0.0f));  // 양방향 선분
```

> `SetTwoSided` = v2.4.1에서 추가된 명시적 양방향 edge. ghost vertex 없이 일반 선분을 만들 때 사용.

### b2CircleShape

```cpp
b2CircleShape shape;
shape.m_radius = 0.4f;  // 반지름 직접 설정
```

---

## 4. Joint — 구속 조건

### b2RevoluteJoint (회전 조인트)

두 body를 한 점에서 회전 가능하게 연결. Tumbler의 회전통과 Bridge의 판자 체인에 사용.

```cpp
// 방법 A: 수동 설정 (Tumbler 모터)
b2RevoluteJointDef jd;
jd.bodyA          = ground;
jd.bodyB          = body;
jd.localAnchorA.Set(0.0f, 10.0f);
jd.localAnchorB.Set(0.0f, 0.0f);
jd.referenceAngle = 0.0f;
jd.motorSpeed     = 0.05f * b2_pi;  // rad/s
jd.maxMotorTorque = 1e8f;
jd.enableMotor    = true;
b2RevoluteJoint* joint = (b2RevoluteJoint*)m_world.CreateJoint(&jd);

// 방법 B: Initialize (Bridge 체인)
b2RevoluteJointDef jd;
jd.Initialize(prevBody, body, anchor);  // anchor = 월드 좌표 연결점
m_world.CreateJoint(&jd);
```

| 파라미터 | 설명 |
|----------|------|
| `localAnchorA/B` | 각 body 로컬 좌표계의 연결점 |
| `referenceAngle` | 두 body의 기준 각도 오프셋 |
| `motorSpeed` | 모터 목표 각속도 (rad/s) |
| `maxMotorTorque` | 모터 최대 토크 |
| `enableMotor` | 모터 활성화 여부 |
| `lowerAngle / upperAngle` | 각도 제한 범위 |
| `enableLimit` | 각도 제한 활성화 |

### b2WheelJoint (휠 조인트)

이동 축 방향의 선형 이동 + 회전을 허용. 서스펜션 스프링 시뮬레이션에 사용.

```cpp
b2WheelJointDef jd;
b2Vec2 axis(0.0f, 1.0f);  // 서스펜션 이동 방향 (수직)

jd.Initialize(m_car, m_wheel1, m_wheel1->GetPosition(), axis);

// 모터 (구동축 = 앞바퀴)
jd.motorSpeed     = 0.0f;
jd.maxMotorTorque = 20.0f;
jd.enableMotor    = true;

// 스프링 계수 (임계 감쇠 공식)
float mass  = m_wheel1->GetMass();
float omega = 2.0f * b2_pi * hertz;        // hertz = 스프링 고유 진동수
jd.stiffness = mass * omega * omega;        // k = mω²
jd.damping   = 2.0f * mass * dampRatio * omega;  // c = 2mζω

// 이동 제한 (서스펜션 스트로크)
jd.lowerTranslation = -0.25f;
jd.upperTranslation =  0.25f;
jd.enableLimit      = true;

b2WheelJoint* spring = (b2WheelJoint*)m_world.CreateJoint(&jd);
```

**런타임 제어**

```cpp
spring->SetMotorSpeed(50.0f);   // 전진
spring->SetMotorSpeed(0.0f);    // 정지
spring->SetMotorSpeed(-50.0f);  // 후진
```

---

## 5. b2Draw — 디버그 렌더러

Box2D는 렌더링을 직접 하지 않는다. `b2Draw`를 상속해서 구현한 뒤 world에 등록하면  
`DebugDraw()` 호출 시 콜백이 발생한다.

### 등록

```cpp
m_draw.SetFlags(b2Draw::e_shapeBit | b2Draw::e_jointBit);
m_world.SetDebugDraw(&m_draw);
```

| 플래그 | 표시 대상 |
|--------|----------|
| `e_shapeBit` | 모든 fixture shape |
| `e_jointBit` | 모든 joint 연결선 |
| `e_aabbBit` | AABB 바운딩 박스 |
| `e_centerOfMassBit` | 질량 중심 |

### 구현해야 할 콜백 7개

```cpp
void DrawPolygon(const b2Vec2* verts, int32 count, const b2Color& color);       // 외곽선
void DrawSolidPolygon(const b2Vec2* verts, int32 count, const b2Color& color);  // 채운 다각형
void DrawCircle(const b2Vec2& center, float radius, const b2Color& color);       // 원 외곽선
void DrawSolidCircle(const b2Vec2& center, float radius,
                     const b2Vec2& axis, const b2Color& color);                  // 채운 원 + 방향선
void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color);     // 선분
void DrawTransform(const b2Transform& xf);                                        // 로컬 축
void DrawPoint(const b2Vec2& p, float size, const b2Color& color);               // 점
```

### b2Color

```cpp
b2Color(float r, float g, float b, float a = 1.0f)
// r, g, b, a 모두 0.0 ~ 1.0
```

### b2Transform

```cpp
xf.p           // b2Vec2 — 위치
xf.q           // b2Rot  — 회전
xf.q.GetXAxis()  // b2Vec2 — 로컬 X축 방향 벡터
xf.q.GetYAxis()  // b2Vec2 — 로컬 Y축 방향 벡터
```

### 이 프로젝트의 DebugDraw 구현 (box2d_common)

```
[b2Draw 콜백]
    PushLine / PushTriangle
        → CPU 버퍼 누적 (kMaxVerts = 1536)

[매 프레임 끝]
    Flush()
        FlushTriangles()  → GL_TRIANGLES (채운 shape, alpha 0.5 블렌딩)
        FlushLines()      → GL_LINES     (외곽선)
```

- **셰이더**: `#version 410 core`, 정점당 `(x, y, r, g, b, a)` 6 float
- **투영**: Camera2D의 직교 투영 행렬을 `uniform mat4 uProj`로 전달
- **Camera2D zoom**: `extents = ratio * 25.0f * zoom` — zoom=1이면 화면에 ±25m 표시

---

## 6. 수학 타입

| 타입 / 상수 | 설명 |
|-------------|------|
| `b2Vec2(x, y)` | 2D 벡터, `.Set(x,y)` 로 재설정 |
| `b2_pi` | π (3.14159...) |
| `b2_dynamicBody` / `b2_staticBody` | 체형 타입 열거 |

---

## 7. 데모별 사용 API 요약

| API | Demo1 Tumbler | Demo2 Car | Demo3 Bridge |
|-----|:---:|:---:|:---:|
| `b2World::Step` | ✓ | ✓ | ✓ |
| `b2World::DebugDraw` | ✓ | ✓ | ✓ |
| `b2PolygonShape::SetAsBox` | ✓ | ✓ | ✓ |
| `b2PolygonShape::Set` | | ✓ | ✓ |
| `b2EdgeShape::SetTwoSided` | | ✓ | ✓ |
| `b2CircleShape` | | ✓ | ✓ |
| `b2RevoluteJointDef` (수동) | ✓ | ✓ | |
| `b2RevoluteJointDef::Initialize` | | ✓ | ✓ |
| `b2WheelJointDef::Initialize` | | ✓ | |
| `b2WheelJoint::SetMotorSpeed` | | ✓ | |
| `b2Body::GetPosition` | | ✓ | |
| `b2Body::GetMass` | | ✓ | |
| `b2Draw::e_shapeBit` + `e_jointBit` | ✓ | ✓ | ✓ |
