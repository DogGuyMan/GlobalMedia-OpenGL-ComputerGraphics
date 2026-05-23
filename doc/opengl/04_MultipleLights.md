## Multiple Lights — 3종 광원 합산 파이프라인

> 출처 노트: `멀티플라이팅.md` §1.2, §2, §6 (진단 모듈 §5·유니폼 헬퍼 §4·빌드 §7 은 이론 외라 제외)
> LearnOpenGL 매핑: Lighting — 6. Multiple lights

---

> ### 📄 1. 합산 파이프라인 (`basic_lighting_fs.glsl`)

단일 광원은 Phong 한 번 계산이지만, 다중 광원은 **세 종류를 합산**한다:

```glsl
result  = CalcDirLight(dirLight, ...);                       // 1) 방향광
for (i in NUM_POINT_LIGHTS) result += CalcPointLight(pointLights[i], ...);  // 2) 점광원 N개
result += CalcSpotLight(spotLight, ...);                     // 3) 스포트라이트
fragColor = vec4(result, 1.0);
```

각 `Calc*` 함수가 ambient/diffuse/specular 를 계산해 vec3 을 돌려주고, 최종 색은 세 결과의 단순 합.

---

> ### 📄 2. GLSL 배열 유니폼 — 컴파일 타임 상수 크기

GLSL 은 동적 배열이 불가능하므로 컴파일 타임 상수 크기 배열을 쓴다:

```glsl
#define NUM_POINT_LIGHTS 2          // C++/GLSL 양쪽에서 동일하게 정의
uniform PointLight pointLights[NUM_POINT_LIGHTS];
```

> **주의**: 같은 매크로 `NUM_POINT_LIGHTS` 를 C++ 와 GLSL 양쪽에 두고 값이 일치해야 한다. 어긋나면 C++ 가 송신하는 배열 인덱스와 셰이더 배열 크기가 불일치.

배열 유니폼 이름은 C++ 에서 `"pointLights[" + i + "]"` 처럼 포매팅해 인덱스별로 송신한다.

---

> ### 📄 3. 광원 배치 & 애니메이션

| 광원 | 배치 |
|---|---|
| 방향광 | `direction = (-0.2, -1.0, -0.3)` 고정, 약한 ambient |
| 점광원 N개 | **위상차(`2π/N`)를 두고** 원궤도, 광원마다 높이 다르게 (`0.25 + 0.4*i`) |
| 스포트라이트 | **카메라에 부착** — `position = eye`, `direction = center - eye` (손전등 효과) |
| 광원 표시용 피라미드 | `NUM_POINT_LIGHTS` 개를 루프로 그림. projection/view/color 는 루프 밖 1회 설정 |

머티리얼 shininess 는 C++ 에서 `32.0f` 로 유니폼 전달.

---

> ### 📄 4. 셰이더 측 변화 (vertex shader)

- `#version 410 core`
- `vsPosition` 을 월드 공간으로: `model * pos` (빛 계산용 — Basic Lighting Q2 참조)
- `vsNormal = mat3(transpose(inverse(model))) * normal` — **노멀 행렬 적용** (Basic Lighting Q1 참조)
- 라이팅에서 안 쓰는 vertex color(`location=1`)는 선언하지 않음 (VBO 인터리브 레이아웃 11-float 자체는 유지)

## 시험 포인트 요약
- 다중 광원 = `dir + Σ point + spot` 단순 합.
- GLSL 배열은 **컴파일 타임 상수 크기** — `NUM_POINT_LIGHTS` 매크로를 C++/GLSL 동기화.
- 스포트라이트를 카메라에 붙이면 손전등 효과.
- vertex shader 의 노멀 행렬·world position 변환은 Basic Lighting 과 동일 원리.

## 관련 노트
- normal 행렬 `transpose(inverse(M))` 와 world/clip 좌표 분리: `02_Lighting/01_BasicLighting_Phong.md`
- 3종 광원 각각의 lightDir·감쇠·소프트에지: `02_Lighting/03_LightCasters.md`
- Material 의 텍스처 유닛·shininess: `02_Lighting/02_Materials_LightingMaps.md`
