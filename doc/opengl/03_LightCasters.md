## Light Casters — 방향광 / 점광원 / 스포트라이트

> 출처 노트: `멀티플라이팅.md` §1.1, §1.3, §1.4
> LearnOpenGL 매핑: Lighting — 5. Light casters

---

> ### 📄 1. 세 가지 광원 타입 (C++ 구조체)

| 타입 | 핵심 필드 | 특징 |
|---|---|---|
| `DirLight` | `direction`, `ambient/diffuse/specular` | 위치 없는 전역 평행광(태양광). 감쇠 없음 |
| `PointLight` | `position`, **`c1`, `c2`**, `ambient/diffuse/specular` | 위치 있음 + 거리 감쇠 |
| `SpotLight` | `position`, `direction`, **`cutOff`, `outerCutOff`**, `c1`, `c2`, `ambient/diffuse/specular` | 점광원 + 원뿔 제한 + 소프트 에지 |

각 광원 함수(`CalcDirLight`, `CalcPointLight`, `CalcSpotLight`)는 **광원 방향(lightDir) 산출 방식** 과 **감쇠/강도 곱** 만 다르고, 공통 항(ambient/diffuse/specular)은 `CalcAmbient / CalcDiffuse / CalcSpecular` 헬퍼로 분리한다.

| 함수 | lightDir 산출 | 감쇠/강도 |
|---|---|---|
| `CalcDirLight` | `normalize(-light.direction)` | 없음 |
| `CalcPointLight` | `normalize(light.position - fragPos)` | 거리 감쇠 곱 |
| `CalcSpotLight` | 점광원 방식 | 소프트 에지(intensity) + 거리 감쇠를 ambient/diffuse/specular 모두에 곱 |

---

> ### 📄 2. 거리 감쇠 (Attenuation)

$$
\text{attenuation} = \frac{1}{1 + c_1 d + c_2 d^2}
$$

`CalcAttenuation(vec2(c1,c2), d)` 로 구현. 셰이더 주석에 커버 거리별 권장 `c1/c2` 테이블(거리 7 -> 0.7/1.8 … 3250 -> 0.0014/0.000007). 거리 50 기준값은 `c1=0.09, c2=0.032`.

| 항 | 거리에 따른 영향 |
|---|---|
| 상수항 `1` | 거리 0 에서도 분모가 0 이 되지 않게 |
| `c1·d` (1차) | 중거리 감쇠 |
| `c2·d²` (2차) | 원거리에서 급격히 감쇠 (물리적 역제곱 근사) |

---

> ### 📄 3. 스포트라이트 소프트 에지

`CalcSoftEdge(theta, phi, gamma)`:
- `theta = dot(lightDir, normalize(-light.direction))` — 프래그먼트가 원뿔 축에서 얼마나 벗어났는지(코사인).
- `theta > cutOff` -> 강도 1 (안쪽 원뿔)
- `theta < outerCutOff` -> 강도 0 (바깥)
- 그 사이 -> `(theta - gamma)/(phi - gamma)` 선형 페이드

$$
\text{intensity} = \text{clamp}\!\left(\frac{\theta - \gamma}{\phi - \gamma},\ 0,\ 1\right)
$$
(φ = cutOff, γ = outerCutOff)

C++ 설정: `cutOff = cos(radians(12.5°))`, `outerCutOff = cos(radians(15.5°))`.

> **주의**: cutOff/outerCutOff 는 *각도* 가 아니라 *코사인 값* 으로 저장한다. theta 도 dot 결과(코사인)라 둘을 직접 비교 가능. 코사인은 각도가 클수록 작아지므로 `theta > cutOff` 가 "안쪽"을 뜻한다.

## 시험 포인트 요약
- 세 광원의 차이는 **lightDir 산출 + 감쇠 유무**. 공통 Phong 항은 동일.
- 감쇠 = `1/(1 + c1·d + c2·d²)` — 상수+1차+2차.
- 스포트라이트 cutOff/outerCutOff 는 **코사인 값**, 안쪽일수록 큰 값.

## 관련 노트
- 이 3종 광원을 한 프래그먼트에서 합산하는 파이프라인 + 광원 배치/애니메이션: `02_Lighting/04_MultipleLights.md`
- Phong ambient/diffuse/specular 항 자체의 수식: `02_Lighting/01_BasicLighting_Phong.md`
