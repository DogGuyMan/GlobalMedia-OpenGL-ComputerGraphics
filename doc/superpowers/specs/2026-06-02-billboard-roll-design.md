# Billboard Roll (Z축) 대응 — Design Spec

> **대상**: `SJH::sprite` (billboard) + `apps/_MyApp_` 셰이더. 브랜치 `game/module/ingame/temp`.
> **작성**: 2026-06-02. 버그 확정 + 설계 승인 후.

## 0. 버그 / 목표
- **버그**: `billboard_atlas.vs` 가 `uModel` 에서 center+scale 만 추출하고 **회전(EulerRot)을 버린다**. EnemyBuilder 가 tween 으로 `EulerRot[2]`(z-roll)를 바꿔도(로그 확인) 빌보드는 무반응 — 스케일만 반응.
- **목표**: 빌보드가 **z축(roll, 카메라 정면축) 회전만** `Transform.EulerRot.z` 로 반영. x/y(pitch/yaw)는 빌보드(카메라 정면) 유지.

## 1. 결정
- **`uRoll` uniform (EulerRot[2] 만 송신)**. x/y 섞인 `uModel` 에서 in-shader roll 추출은 오염 → 깨끗한 Z-euler 만 CPU 송신. SpriteRenderer 가 이미 owner Transform 접근 + 매 프레임 uniform 송신.

## 2. 변경 (2 파일)

### T1 — `apps/_MyApp_/resources/shaders/billboard_atlas.vs`
- `uniform float uRoll;`(radians, 기본 0) 추가.
- flip 적용 후 quad XY 를 cameraRight/cameraUp 평면 내에서 2D 회전:
```glsl
vec2  p  = vec2(aPos.x * uFlipX, aPos.y);
float cr = cos(uRoll), sr = sin(uRoll);
vec2  rp = vec2(p.x * cr - p.y * sr, p.x * sr + p.y * cr);
vec3 worldPos = center + cameraRight * rp.x * sx + cameraUp * rp.y * sy;
```
- `uRoll=0` ⇒ 기존과 동일(하위 호환). `uFlipX` 를 `p.x` 에 선반영(flip 보존).

### T2 — `src/sprite/sprite_component.cpp` `SpriteRenderer::Update`
- owner EulerRot.z 송신:
```cpp
float rollDeg = 0.0f;
if (auto *o = GetOwner()) rollDeg = o->GetTransform().EulerRot[2];
Uniforms::SetFloat(*Material, "uRoll", vmath::radians(rollDeg));
```

## 3. 검증
- enemy(EnemyBuilder rotTw) → 스프라이트가 화면 평면에서 회전(roll) — 스케일 펄스와 동시.
- EulerRot=(θ,0,0)/(0,θ,0) → 변화 없음(빌보드 유지).
- 회전 0 스프라이트 → 시각 동일.
- 양수 roll 시각 방향(시계/반시계) → 빌드 후 확인, 반대면 부호 1줄 조정.

## 4. 비목표
- 부모 회전 누적 world-roll 미반영(local EulerRot.z = "transform.Euler" 직접 의미).

**spec 끝.**
