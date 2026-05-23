## Basic Lighting — Phong 의 4가지 헷갈리는 지점

> 출처 노트: `Lighting.md` (Q4 의 Blinn-Phong 변형 소절은 `05_AdvancedLighting/01_BlinnPhong.md` 로 분리)
> LearnOpenGL 매핑: Lighting — 2. Basic Lighting
> **이 노트가 "Normal 행렬" 과 "Specular view 의존성" 의 단일 출처(SSoT)다.**

---

> ### 📄 Q1. `transpose(inverse(model)) * vec4(aNormal, 0.0)` — 왜 normal 에만 이런 변환?

#### 한 줄 요약
**normal 은 정점이 아니라 "표면에 수직인 방향" 이라서, 모델이 변형되면 normal 도 *반대로* 변형해야 직각이 유지된다.**

#### 직관 — 풍선을 위에서 누르는 그림
풍선을 발판 위에서 손바닥으로 누른다고 상상. 풍선은 위아래로 짜부라지고 옆으로 늘어남(Y축 0.5배, X축 2배 = non-uniform scale). 표면에 박아둔 압정(=normal)은 — 세로로 짜부 시켰으면 압정의 Y 성분이 *더 커져야*(더 위를 향해야) 새 표면에 직각.

-> 정점에 적용한 변환을 normal 에는 *반대로* 적용 = `inverse`. vector 와 covector 의 차이로 한 번 더 transpose 가 붙어 `transpose(inverse(M))`.

#### 수식 유도
표면 위 접선 벡터 t, normal 을 n 이라 하면 정의상 `n · t = 0`. 변환 후에도 `n' · t' = 0` 유지되어야 하니, `t' = M·t` 일 때:
$$
n' = (M^{-1})^{T} \cdot n
$$

#### 언제 생략 가능한가
- M 이 **회전 + 균등 스케일 + 평행이동** 만이면 `(M⁻¹)ᵀ == M` (방향 성분 한정) -> `mat3(M) * aNormal` 로 끝.
- 자유 변형(non-uniform scale, shear)을 허용하면 normal matrix 필수.

#### `vec4(aNormal, 0.0)` 의 0
방향 벡터(w=0) 라서 평행이동 칸이 무시됨. normal 은 위치가 아니라 *방향*. 위치 벡터(`vec4(aPos, 1.0)`)의 w=1 과 대조.

---

> ### 📄 Q2. `gl_Position` 따로, `positionVector` 따로 — 왜 같은 정점을 두 번 변환?

#### 한 줄 요약
**둘은 *서로 다른 좌표계* 의 위치다. 화면에 그릴 위치(clip space)와 빛 계산용 위치(world space)는 다른 공간이라 분리.**

| 변수 | 곱한 행렬 | 결과 좌표계 | 용도 |
|---|---|---|---|
| `gl_Position` | `proj × view × model` | clip / NDC | 래스터라이저가 화면 픽셀 어디에 그릴지 |
| `positionVector` | `model` 만 | world space | FS 에서 빛까지 거리·방향 계산 |

#### 직관 — "지도 vs 사진"
- **gl_Position = 사진 속 픽셀 좌표.** 원근 때문에 멀리 있는 큐브는 작게 찍힘.
- **positionVector = 실제 세상 좌표.** "큐브 (3,0,-5), 빛 (3,3,3) -> 두 점 사이 벡터 (0,3,8)" 같은 물리적 거리/방향 계산용.

```glsl
vec3 lightDir = normalize(lightPos - positionVector);
```
`lightPos` 도 world space, `positionVector` 도 world space 여야 두 벡터의 차가 의미 있는 빛 방향이 됨. `gl_Position`(clip space)으로 계산하면 perspective divide 후 거리 비율이 z 깊이에 따라 비선형으로 일그러진다.

> **핵심**: light · position · viewer 가 *같은 공간*에 있어야 한다. (world space 또는 view space 통일)

---

> ### 📄 Q3. `reflect(I, N)` 의 정확한 수식

#### GLSL 정의 (정확)
$$
\text{reflect}(I, N) = I - 2 (N \cdot I) N
$$

흔한 오해 `2 * N * dot(N, -L)` 는 위 식의 *두 번째 항만* — 입사 벡터 `I` 본체가 누락된 식.

#### 직관 — 당구공이 쿠션에 부딪히기
입사 벡터 I 를 두 성분으로 분해:
- `I_∥ = (N·I)·N` — N 방향(수직) 성분. 충돌 후 부호 반전
- `I_⊥ = I - I_∥` — 표면 평행 성분. 충돌 후 유지
- 반사: `R = I_⊥ - I_∥ = I - 2·I_∥ = I - 2(N·I)N`

#### Phong specular 에서의 의미
`reflect(-lightDir, pixelNorm)` = "빛이 표면에 부딪힌 뒤 튕겨 나가는 방향". 이 방향과 카메라(`viewDir`)가 일치할수록 specular 가 세짐:
```glsl
float spec = pow(max(dot(viewDir, reflectDir), 0.0), specularShininess);
```
`pow(..., shininess)` : shininess 클수록 하이라이트 좁고 뾰족.

---

> ### 📄 Q4. 광원이 +X 에 있는데 specular spot 이 거기 안 보임 — 버그 아닌가?

#### 한 줄 요약
**Specular 는 광원 방향이 아니라 *광원과 카메라의 중간 방향(halfway)* 에 나타난다. View 에 의존하는 것이 specular 의 *정의 자체*.**

#### Diffuse vs Specular — 의존 변수 차이

| 항 | 수식 | 의존 변수 | View-dependent? | 표면 |
|---|---|---|---|---|
| Diffuse  | `k_d · max(N·L, 0)`        | N, L      | ❌ No  | matte (무광) |
| Specular | `k_s · max(R·V, 0)^n`      | N, L, **V** | ✅ Yes | glossy (광택) |

-> Diffuse 는 카메라를 옮겨도 *같은 face* 가 밝다 (Lambertian). Specular 는 카메라가 움직이면 highlight 도 따라 움직인다 (mirror-like).

#### 직관 — 거울 속 천장 조명
욕실 거울에 천장 조명이 비치는 위치는 *내가 어디 서 있느냐*에 따라 달라진다. 한 발짝 옆으로 가면 반사 spot 도 나를 따라 옆으로 움직인다 — 조명은 가만히 있어도. **빛 -> 표면 -> 카메라** 의 거울 반사 경로가 성립하는 픽셀에서만 highlight 가 보인다.

#### 카메라 위치별 spot 위치 — halfway vector H 로 보면 명확
`H = normalize(L + V)` (자세한 Blinn-Phong 모델은 Advanced Lighting 노트):

| 카메라 위치 | H 방향 | spot 이 나타나는 face |
|---|---|---|
| 광원 바로 옆 | H ≈ L | 광원 방향 face (이때만 diffuse 와 일치) |
| 광원 정반대편 | H ≈ V | 카메라 쪽 face |
| 90° 옆 | L 과 V 의 중간 | 광원·카메라 중간 방향 face |

#### 모델 회전 시 (광원·카메라 고정)
H 방향은 월드 좌표에서 변하지 않음. 모델이 회전하면 어떤 face 의 normal 이 H 에 가까운지가 시간에 따라 바뀜 -> spot 이 face 사이를 **미끄러져** 다른 면으로 넘어가는 것이 **정상**.

#### 흔한 오해

| 오해 | 사실 |
|---|---|
| "광원 방향 face 가 밝아야 한다" | Diffuse 만 맞음. Specular 는 V 에 따라 다른 face |
| "모델 회전 시 같은 표면 점에 spot 이 붙어 함께 회전" | 그건 normal 미변환 *버그*. 정상은 spot 이 표면을 *미끄러짐* |
| "광원이 고정이면 spot 도 고정" | Diffuse 는 그렇지만 specular 는 V 도 고정이어야 |

#### 검증 방법 — "정상인가 버그인가" 1분 확인
1. 모델 회전 끄고 카메라만 움직이며 spot 관찰

| 관찰 | 진단 |
|---|---|
| Diffuse 영역이 카메라 이동과 무관 | 정상 |
| Specular spot 이 카메라 이동에 따라 따라 움직임 | 정상 |
| Specular spot 이 카메라 이동과 무관 | View-independent 버그 (V 누락) |
| Diffuse·specular 둘 다 카메라 따라 움직임 | 좌표공간 불일치 버그 |

---

## 한 장 요약

| 질문 | 핵심 | 수식 |
|---|---|---|
| Q1 normal 변환 | 정점은 M, normal 은 inverse-transpose. 방향 + 직각 보존 | `n' = (M⁻¹)ᵀ·n`, `w=0` |
| Q2 position 두 번 | clip space=그릴 픽셀, world space=빛/거리 계산. 광원·정점·뷰어 같은 공간 | `gl_Position = P·V·M·p`, `posVec = M·p` |
| Q3 reflect 식 | 입사 벡터 I 의 N-수직 성분만 부호 반전, 평행 성분 유지 | `R = I − 2(N·I)N` |
| Q4 specular view 의존 | Diffuse(N·L)는 광원만, Specular(R·V)는 카메라까지. spot 은 광원·카메라 halfway face | `L_s = k_s·(R·V)^n` |

## Diffuse vs Specular — 시험용 핵심 비교

| 항목 | Diffuse | Specular |
|------|---------|----------|
| 의존 변수 | N, L | N, L, **V** |
| 수식 | `k_d · max(N·L, 0)` | `k_s · max(R·V, 0)^n` |
| View dependent? | ❌ | ✅ |
| 표면 특성 | matte (무광) | glossy (광택) |
| Highlight 위치 | 광원 방향 face | 광원·카메라 halfway face |
| 카메라 이동 시 | 변화 없음 | Highlight 따라 움직임 |
| 광원 고정 + 모델 회전 | 광원 향한 face 가 시간차로 밝아짐 | spot 이 표면 위를 미끄러져 face 옮겨감 |
| 물리 모델 | Lambertian (균일 산란) | Mirror-like (거울 반사) |

## 관련 노트
- Blinn-Phong 변형(halfway vector 직접 사용, grazing angle, shininess 2~4배): `05_AdvancedLighting/01_BlinnPhong.md`
- 광원·정점·뷰어를 같은 공간에 두는 좌표계 일치 문제: 본 노트 Q2
- 다중 광원 합산에서 이 normal 행렬·Phong 항을 그대로 사용: `02_Lighting/04_MultipleLights.md`
