# 매트릭스 스카이박스 — 구면좌표 투영과 UV 매핑 (요약)

> 코드: `matrix_skybox.vs` / `matrix_skybox.fs` · 대상: 1학년 1학기 미적분 수료

## 1. 무엇을 하나

스카이박스는 **"바라보는 방향(3D 화살표) → 칠할 색"** 함수다.
프래그먼트마다 **단위 방향벡터** $\mathbf{d}=(x,y,z),\ |\mathbf{d}|=1$ 를 받아,
이를 2D 텍스처 좌표 $(U,V)$ 로 바꾼다. 이 변환이 **구면좌표 ↔ 평면 투영**이다.

## 2. 구(球) ↔ 평면 투영 수식 (등장방형 / equirectangular)

단위구 위의 한 점은 **경도 $\theta$**, **위도 $\phi$** 두 각으로 완전히 정해진다.

**순방향 (각 → 3D 점):**

$$
x=\cos\phi\cos\theta,\qquad y=\sin\phi,\qquad z=\cos\phi\sin\theta
$$

**역방향 (3D 방향 → 각)** — 셰이더가 실제로 하는 것:

$$
\theta=\operatorname{atan2}(z,\,x)\in(-\pi,\ \pi],
\qquad
\phi=\arcsin(y)\in\left[-\tfrac{\pi}{2},\ \tfrac{\pi}{2}\right]
$$

- $|\mathbf{d}|=1$ 이므로 $y=\sin\phi$ 가 그대로 성립 → $\phi=\arcsin y$ (추가 계산 불필요).
- 좌우 **한 바퀴**를 구분하려고 $\arctan(z/x)$ 대신 부호 2개를 보는 $\operatorname{atan2}(z,x)$ 를 쓴다.

### Forward vs Inverse Mapping — 이 셰이더는 **역매핑(Inverse)** 이다

| | 진행 방향 | 한 줄 |
|---|---|---|
| Forward (scatter) | 텍스처 → 화면 | "이 텍셀을 화면 어디로 뿌리지?" |
| **Inverse (gather)** | **화면 → 텍스처** | **"이 화면 픽셀은 텍스처 어디서 가져오지?"** |

프래그먼트 셰이더는 **이미 그려진 화면 픽셀(목적지)마다** 실행된다. 그 픽셀의 3D 방향 $\mathbf{d}$ 에서
$(U,V)$ 를 **거꾸로 계산**하므로 목적지 → 소스 = **Inverse Mapping**이다.
그래서 §2의 **역방향식**($xyz\to\theta,\phi$)을 쓰고, 순방향식($\theta,\phi\to xyz$)은 안 쓴다.
GPU 래스터는 늘 이 gather 방식이라 출력 픽셀마다 정확히 한 번 읽어 **구멍·겹침이 없다**.

## 3. U, V ↔ 경도/위도 매핑

각을 0 기준 비율로 **정규화**해 텍스처 좌표를 만든다:

$$
U=\frac{\theta}{2\pi},\qquad V=\frac{\phi}{\pi}
$$

| 좌표 | 대응 각 | 각의 범위 | 나누는 값 | 결과 범위 |
|------|---------|-----------|-----------|-----------|
| $U$ | 경도 $\theta$ (좌우, 한 바퀴) | $(-\pi,\ \pi]$ | $2\pi$ | $(-0.5,\ 0.5]$ |
| $V$ | 위도 $\phi$ (상하, 반 바퀴) | $[-\tfrac{\pi}{2},\ \tfrac{\pi}{2}]$ | $\pi$ | $[-0.5,\ 0.5]$ |

즉 **$U$ = 동서 경도, $V$ = 남북 위도**. 지구본의 경위도 격자를 그대로 평면(세계지도)으로 편 것이다.
코드 상수 `invAtan` 이 곧 $\left(\tfrac{1}{2\pi},\ \tfrac{1}{\pi}\right)=(0.15915\ldots,\ 0.31831\ldots)$ 다.

```glsl
vec2 uv = vec2(atan(v.z, v.x), asin(v.y)); // (θ, φ)
uv *= invAtan;                             // (θ/2π, φ/π) = (U, V)
```

## 4. UV 분할 (평면 좌표 → 글자 격자)

평면 좌표 $(U,V)$ 를 칸 수 $(C,R)=(256,128)$ 만큼 확대한 뒤 **정수/소수**로 쪼갠다:

$$
\mathbf{g}=(U,V)\cdot(C,R),\qquad
\text{cell}=\lfloor \mathbf{g}\rfloor,\qquad
\text{local}=\operatorname{frac}(\mathbf{g})
$$

- **$\lfloor\mathbf{g}\rfloor$** : 몇 번째 칸인가 → 칸당 글자 **하나** 선택(칸 전체가 같은 값).
- **$\operatorname{frac}(\mathbf{g})$** : 칸 안 $0\sim1$ 지역 좌표 → 글자 텍스처를 샘플.
- 항등식 $\mathbf{g}=\lfloor\mathbf{g}\rfloor+\operatorname{frac}(\mathbf{g})$. floor=칸 주소, fract=칸 안 위치.

## 5. 한 줄 요약

$$
\mathbf{d}\ \xrightarrow{\ \text{역투영}\ }\ (\theta=\operatorname{atan2}(z,x),\ \phi=\arcsin y)
\ \xrightarrow{\ \text{정규화}\ }\ (U=\tfrac{\theta}{2\pi},\ V=\tfrac{\phi}{\pi})
\ \xrightarrow{\ \times(C,R)\ }\ \lfloor\cdot\rfloor,\ \operatorname{frac}(\cdot)
$$

화면 픽셀에서 방향을 받아 **역매핑**으로 두 각을 구하고(구면좌표), 각을 $2\pi,\pi$ 로 나눠 $U,V$ 로 매핑한 뒤,
floor/fract 로 격자를 잘라 글자 한 칸을 채운다.

---

### 레퍼런스 자료
* https://godotshaders.com/shader/matrix-rain/
