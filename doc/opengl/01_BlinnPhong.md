## Advanced Lighting — Blinn-Phong

> 출처 노트: `Lighting.md` Q4 의 Blinn-Phong 변형 소절
> LearnOpenGL 매핑: Advanced Lighting — 1. Advanced Lighting
> (Phong specular 의 view 의존성 본체는 `02_Lighting/01_BasicLighting_Phong.md` Q4 참조)

---

> ### 📄 1. Halfway Vector

Phong 의 `reflect()` 대신 **halfway vector** 를 사용한다:

$$
H = \text{normalize}(L + V)
$$

Phong 이 `(R·V)^n` 으로 specular 를 계산했다면, Blinn-Phong 은 `(N·H)^n` 으로 계산한다.

```glsl
// 기존 Phong:
// vec3 reflectDir = reflect(-lightDir, pixelNorm);
// float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

// Blinn-Phong:
vec3 halfwayDir = normalize(lightDir + viewDir);
float spec = pow(max(dot(pixelNorm, halfwayDir), 0.0), shininess);
```

---

> ### 📄 2. Phong 대비 차이

| 항목 | Phong | Blinn-Phong |
|---|---|---|
| specular 식 | `(R·V)^n`, `R = reflect(-L, N)` | `(N·H)^n`, `H = normalize(L+V)` |
| 각도 | R 과 V 사이 | N 과 H 사이 (Phong 의 *절반* 각) |
| 같은 sharpness 위한 shininess | 기준 | **2~4배** 키워야 함 |
| grazing angle (view 가 표면에 거의 평행) | highlight 끊김 | 더 자연스러움 |
| 비용 | `reflect()` 필요 | `reflect()` 한 번 줄어 약간 저렴 |

> `dot(N, H)` 는 `dot(R, V)` 보다 각도가 절반이라, 동일한 하이라이트 폭을 내려면 shininess 지수를 더 크게 잡아야 한다.

---

> ### 📄 3. 광원 방향에 *고정된* highlight 를 원한다면

그건 표준 specular 가 아니다 (표준 Phong/Blinn-Phong specular 는 *반드시* view-dependent). 대안:
- **Toon/cel shader 의 specular band** — N·L 기반 계단 함수
- **Diffuse 강조 + emissive** — 자체 발광
- **Half-Lambert (Valve)** — `(N·L * 0.5 + 0.5)^2` 로 wraparound

## 시험 포인트 요약
- Blinn-Phong: `H = normalize(L+V)`, specular = `(N·H)^n`.
- N·H 각도는 R·V 의 **절반** -> 같은 sharpness 에 shininess **2~4배**.
- grazing angle 에서 Phong 보다 자연스럽고 `reflect()` 가 빠져 약간 저렴.
- specular 의 view 의존성은 Phong·Blinn-Phong 공통 (정의).

## 관련 노트
- Phong specular·reflect·view 의존성 본체: `02_Lighting/01_BasicLighting_Phong.md`
