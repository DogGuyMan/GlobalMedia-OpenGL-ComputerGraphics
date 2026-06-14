# 분할형 체력바(Health Bar) — UV 분할로 조각 나누기와 부드러운 경계

> 대상 독자: 대학 1학년 1학기 미적분을 막 끝낸 학생
> 다루는 코드: `apps/_MyApp_/resources/shaders/healthbar.vs` / `healthbar.fs`
> 함께 읽기: [`ShaderSkybox.md`](./ShaderSkybox.md), [`SpriteAtlas.md`](./SpriteAtlas.md) — 같은 **UV 분할(floor/fract)** 이 또 나온다.

이 문서는 캐릭터 머리 위에 뜨는 **조각난 체력바**가
**가로 좌표 하나(u)를 어떻게 N개의 조각으로 나누고**(UV 분할),
**조각 사이 틈과 채움 경계를 어떻게 매끄럽게(안티에일리어싱) 그리는지**를 1학년 수준으로 설명한다.

---

## 0. 한 장 요약

체력바는 이렇게 생겼다 — 가로로 길쭉한 막대를 여러 **조각(segment)** 으로 쪼개고,
남은 체력만큼 **왼쪽부터** 색을 채운다.

```
체력 70%, 조각 5칸:
   [###][###][###][# .][   ]
    채워짐 ----------^   ^빈 칸(트랙)
            틈(gap)으로 조각 구분
```

핵심 도구는 딱 두 개:
- **`fract(u * N)`** : 가로 좌표를 **N개 조각으로 분할** (스카이박스/아틀라스에서 본 그 분할!)
- **`smoothstep` + `fwidth`** : 조각 틈과 채움 경계를 **계단처럼 깨지지 않게** 부드럽게 그리기

---

## 1. 준비운동 — 정점 셰이더는 "머리 위 빌보드"

`healthbar.vs` 는 [`SpriteAtlas.md`](./SpriteAtlas.md) 5절의 빌보드와 거의 같다.
체력바 quad를 **카메라 정면을 향한 채 캐릭터 머리 위에** 띄운다.

```glsl
vec3 cameraRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
vec3 cameraUp    = vec3(uView[0][1], uView[1][1], uView[2][1]);

vec3 anchor   = center + cameraUp * uHeadOffset;            // 머리 위로 띄우기
vec3 worldPos = anchor + cameraRight * aPos.x * sx          // 가로
                       + cameraUp    * aPos.y * sy;         // 세로
vUv = aTexCoord;  // quad의 0~1 좌표를 그대로 프래그먼트로
```

- `center` : 액터 월드 위치, `uHeadOffset` 만큼 화면상 **위(cameraUp)** 로 올려 머리 위에 배치
- 가로/세로를 카메라 축에 붙여 **어느 각도에서 봐도 정면**으로 보이게 (빌보드)
- 여기서 중요한 건 **`vUv = aTexCoord`** — quad 가로 좌표 `vUv.x` 가 **0(왼쪽)~1(오른쪽)** 으로
  프래그먼트 셰이더에 넘어간다는 것. 이 `vUv.x` 하나로 모든 조각 분할이 일어난다.

---

## 2. 프래그먼트 셰이더 — 가로 좌표 u 하나로 모든 걸 그린다

`healthbar.fs` 가 다루는 정보는 사실상 **가로 좌표 `u = vUv.x` (0~1)** 하나뿐이다.

```glsl
float u = clamp(vUv.x, 0.0, 1.0);  // 0(왼끝) ~ 1(오른끝)
float N = max(uSegmentCount, 1.0); // 조각 수 (예: 5)
```

`clamp(x, 0, 1)` 은 x를 0~1 범위로 가두는 함수(0보다 작으면 0, 1보다 크면 1).
이제 이 `u` 를 두 가지로 해석한다:
- **(A) u가 N개 조각 중 어디인가** → 조각 모양/틈을 그림 (3절)
- **(B) u가 채움 비율 `uFill` 보다 왼쪽인가** → 색을 채울지 결정 (5절)

---

## 3. UV 분할 — 막대를 N조각으로 쪼개기 (`fract(u * N)`)

스카이박스와 **완전히 같은 분할 기법**이 여기 다시 나온다.

```glsl
float f = fract(u * N);   // 현재 조각 안에서의 위치 0~1
```

`u * N` 으로 좌표를 N배 확대하면, **정수 부분이 "몇 번째 조각"**, **소수 부분이 "조각 안 위치"** 가 된다.
([`ShaderSkybox.md`](./ShaderSkybox.md) 3절의 `floor`(칸 번호)/`fract`(칸 안 위치)와 똑같은 원리.)

```
u (0~1):        0 ......... 0.5 ......... 1
u * 5:          0    1    2    3    4    5
fract(u*5):     0→1  0→1  0→1  0→1  0→1     (조각마다 0→1 반복)
조각 번호:       [ 0 ][ 1 ][ 2 ][ 3 ][ 4 ]
```

체력바는 글자처럼 "칸 번호(floor)"가 필요 없고 **칸 안 위치(fract)** 만 있으면 된다.
왜냐하면 모든 조각이 똑같이 생겼고, 우리가 알고 싶은 건 **"지금 조각의 가장자리에 가까운가, 가운데인가"** 뿐이기 때문이다.

### 조각 경계까지의 거리

```glsl
float edge = min(f, 1.0 - f);  // 조각 경계(0 또는 1)까지의 거리
```

`f` 는 조각 안에서 0~1. 양쪽 끝(0과 1)이 **조각의 경계(틈)** 다.
- `f = 0.5` (조각 한가운데) → `min(0.5, 0.5) = 0.5` (경계에서 가장 멀다)
- `f = 0.02` (왼쪽 가장자리) → `min(0.02, 0.98) = 0.02` (경계에 매우 가깝다)

즉 **`edge` 가 작을수록 조각 경계(틈)에 가깝다**. 이 값으로 "여기는 조각 몸체냐, 조각 사이 틈이냐"를 가른다.

```
한 조각 단면:
edge:  0 ......0.5...... 0   <- 가운데서 크고 양 끝에서 0
       |  몸체(불투명) |
       틈            틈
```

---

## 4. smoothstep + fwidth — 계단현상 없이 부드럽게

여기서 조각 틈을 그냥 `if (edge < spacing) 투명` 으로 처리하면 경계가 **픽셀 계단(깨짐)** 으로 보인다.
이를 막는 게 안티에일리어싱(AA)이고, 셰이더는 `smoothstep` 과 `fwidth` 로 처리한다.

### smoothstep — 부드러운 문턱

`smoothstep(a, b, x)` 는 "부드러운 스위치"다.

```
x <= a  : 0
x >= b  : 1
a < x < b : 0에서 1로 매끄러운 S자 곡선 (계단 아닌 경사로)
```

`step`(딱딱한 0/1 계단)과 달리 `smoothstep` 은 `a`~`b` 구간에서 **서서히** 바뀌어 경계가 매끈하다.

### fwidth — "이 값이 한 픽셀 사이에 얼마나 변하나"

`fwidth(x)` 는 **옆 픽셀과 비교했을 때 x가 변하는 양**(대략 `|dx/화면가로| + |dx/화면세로|`)이다.
미적분의 "변화율(미분)"을 화면 픽셀 단위로 잰 것이라고 보면 된다.

왜 필요할까? 체력바가 화면에서 크게 보이든 작게 보이든 **경계 흐림을 딱 1픽셀 폭**으로 유지하려면,
"한 픽셀당 좌표가 얼마나 변하는지"를 알아야 한다. 그게 `fwidth` 다. 이 값을 smoothstep의
경계 폭으로 쓰면 줌과 무관하게 항상 1픽셀짜리 매끈한 경계가 나온다.

```glsl
float aaSeg = fwidth(u * N);  // 조각 좌표의 픽셀당 변화량
float aaU   = fwidth(u);      // u 좌표의 픽셀당 변화량
```

### 조각 몸체 만들기

```glsl
float body = smoothstep(uSegmentSpacing, uSegmentSpacing + aaSeg, edge);
```

- `edge` 가 `uSegmentSpacing`(틈 절반 폭)보다 작으면 → `body = 0` (틈, 투명)
- 충분히 크면 → `body = 1` (조각 몸체, 불투명)
- 그 사이 `aaSeg` 폭만큼만 부드럽게 전환 → 경계 안 깨짐

결과적으로 `body` 는 **조각 몸체=1, 조각 사이 틈=0** 인 마스크가 된다.

---

## 5. 채움 — 체력 비율만큼 왼쪽부터 채우기

이제 "색을 채울지 말지"를 `uFill`(체력 비율 0~1)로 정한다.

```glsl
float fill = 1.0 - smoothstep(uFill - aaU, uFill + aaU, u);
```

- `u < uFill` (왼쪽, 아직 남은 체력) → `smoothstep≈0` → `fill = 1` (채움)
- `u > uFill` (오른쪽, 깎인 체력) → `smoothstep≈1` → `fill = 0` (빈 트랙)
- 경계(`u ≈ uFill`)는 `aaU` 폭으로 부드럽게

즉 **왼쪽부터 채워지고 오른쪽부터 비워진다**. `uFill` 을 매 프레임 `HealthBarDriver`(C++)가 갱신하면
체력바가 줄어드는 애니메이션이 된다.

### 색 합치기

```glsl
vec3 rgb = mix(uBgColor.rgb, uColor.rgb, fill);   // 빈색 <-> 채운색 보간
float a  = mix(uBgColor.a,   uColor.a,   fill) * body;  // 투명도 * 조각마스크
fragColor = vec4(rgb, a);
```

`mix(a, b, t) = a*(1-t) + b*t` 는 **선형 보간(lerp)** 이다(미적분에서 본 1차식 내삽).
- `fill=1` → 채운색(`uColor`), `fill=0` → 빈색(`uBgColor`)
- 마지막에 `* body` 로 **조각 사이 틈(body=0)은 완전히 투명**하게 만든다.

정리하면 한 픽셀의 최종 모습은:

```
색   = (채웠나?) 채운색 또는 트랙색
투명도 = 그 색의 알파 × (조각 몸체면 1, 틈이면 0)
```

---

## 6. 전체 흐름 한눈에

```
vUv.x = u (0~1)  ← 정점 셰이더가 넘긴 가로 좌표
   |
   ├─[UV 분할]→ f = fract(u*N) → edge = min(f,1-f)
   |              → body = smoothstep(...) : 조각 몸체=1 / 틈=0
   |
   └─[채움]→ fill = 1 - smoothstep(uFill±aa, u) : 남은 체력=1 / 깎임=0
                 |
                 v
   rgb = mix(트랙색, 채운색, fill)
   a   = mix(...) * body      ← 틈은 투명
                 |
                 v
            fragColor
```

## 7. 외워둘 한 줄

- **UV 분할**: `fract(u * N)` 으로 막대를 N조각으로 쪼갠다 — 스카이박스/아틀라스의 `floor/fract` 와 같은 기법.
- **경계까지 거리** `min(f, 1-f)` 로 조각 몸체/틈을 구분한다.
- **smoothstep + fwidth** 로 경계를 **줌과 무관하게 1픽셀 폭**으로 매끈하게(안티에일리어싱) 그린다.
- **채움**은 `u` 와 `uFill` 비교 한 번, **색**은 `mix`(선형 보간) 한 번이면 끝.

---

#### 레퍼런스
https://github.com/Sam-Schiffer/UnityCircularHealthBarsLite
