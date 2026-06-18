# 후처리(Post-Processing) — Bloom · Vignetting · Fog (요약)

> 코드: `postprocess/bloom.fs` · `grayscale_vignetting.fs` · `fog.fs` · 대상: 1학년 1학기 미적분 수료

## 0. 후처리란? (공통 구조)

3D 장면을 일단 텍스처(`uScene`)에 한 번 그려 두고, **그 결과 이미지를 화면 전체 사각형에 다시 칠하면서
색을 가공**하는 단계다. 각 셰이더는 똑같은 약속을 따른다.

- `in vec2 vUV` : 화면 위치 $0\sim1$ (왼아래 $(0,0)$, 오른위 $(1,1)$)
- `uniform sampler2D uScene` : **직전 단계까지 그려진 장면 이미지**
- 픽셀마다 `texture(uScene, vUV)` 로 색을 읽어 → 가공 → `fragColor` 출력

여러 효과를 **패스 체인**으로 줄줄이 통과시킨다(예: 장면 → Bloom → Vignette → Fog → 화면).
즉 한 효과의 출력이 다음 효과의 `uScene` 입력이 된다.

---

## 1. Bloom = Bright-pass + **Box Blur** + Additive

밝은 부분(네온, 발광체)이 **빛 번짐**처럼 주변으로 새어 나오게 하는 효과다. 세 조각의 조합이다.

```glsl
vec2 texel = 1.0 / vec2(textureSize(uScene, 0)); // 픽셀 하나의 UV 크기

vec3 bloom = vec3(0.0);
for (int y = -4; y <= 4; ++y)
for (int x = -4; x <= 4; ++x) {
    vec2 off  = vec2(x, y) * texel * uBloomSpread;          // 이웃 9x9 위치
    vec3 samp = texture(uScene, vUV + off).rgb;
    float lum = dot(samp, vec3(0.299, 0.587, 0.114));        // (a) 휘도
    bloom += samp * step(uBloomThreshold, lum);              // (b) bright-pass
}
bloom /= 81.0;                                               // (c) 9x9 박스 블러 평균

vec3 base = texture(uScene, vUV).rgb;
fragColor = vec4(base + bloom * uBloomIntensity, 1.0);       // (d) additive
```

**(a) 휘도(luminance)** = `dot(rgb, (0.299, 0.587, 0.114))` 는 RGB를 **밝기 한 숫자**로 합친 것이다.
가중치가 다른 이유는 사람 눈이 **초록>빨강>파랑** 순으로 민감하기 때문(Rec.709 표준).

**(b) Bright-pass** = `step(threshold, lum)` 는 밝기가 문턱($0.7$)보다 크면 $1$, 작으면 $0$ 인 **딱딱한 스위치**.
즉 **밝은 픽셀만 골라내고** 어두운 픽셀은 $0$ 으로 버린다.

**(c) Box Blur(박스 블러)** = 한 픽셀을 그릴 때 **주변 $9\times9=81$ 칸을 모두 똑같은 비중으로 평균**낸다.

$$
\text{bloom} = \frac{1}{81}\sum_{y=-4}^{4}\sum_{x=-4}^{4}\big(\text{samp}\cdot \text{step}(\cdot)\big)
$$

평균을 내면 밝은 점이 **이웃으로 퍼져 흐릿한 덩어리**가 된다. `uBloomSpread` 가 샘플 간격(번짐 폭)을,
`textureSize` 로 구한 `texel` 이 "1픽셀 = UV 몇 칸인지"를 정해 해상도와 무관하게 동작한다.

> **Box vs Gaussian**: 박스 블러는 81칸을 **모두 동일 가중**으로 평균한다(구현이 싸다).
> 2절의 가우시안은 가운데일수록 비중이 큰 **종 모양 가중**이다 — 같은 "흐리기"라도 가중치 분포가 다르다.

**(d) Additive 합성** = 원본(`base`)에 번진 밝은 빛(`bloom`)을 **더한다**. 빼거나 섞지 않고 더하므로
밝은 곳은 더 밝아지며 주변으로 빛이 새어 나오는 글로우가 된다.

---

## 2. Vignetting = 중심 거리 + **가우시안 마스킹**

화면 **가장자리를 어둡게(또는 특정 색으로)** 눌러 시선을 가운데로 모으는 효과다.
(같은 셰이더의 grayscale 은 휘도로 무채색을 만들어 `mix` 하는 별개 기능이라 여기선 생략.)

```glsl
float dist  = distance(vUV, vec2(0.5));            // (a) 화면 중심에서의 거리
float intensity = uVignetteAmount * 20.0;          //     0~1 -> 0~20 강도
float vignetteFactor = exp(-dist * dist * intensity); // (b) 가우시안 마스크
finalColor = mix(uVignetteColor, finalColor, vignetteFactor); // (c) 합성
```

**(a) 중심 거리** : `distance(vUV, (0.5,0.5))` 는 픽셀이 **화면 한가운데에서 얼마나 떨어졌나**.
중앙은 $0$, 모서리로 갈수록 커진다.

**(b) 가우시안 마스크** : 거리 $d$ 에 대한 **종 모양(정규분포) 함수**다.

$$
M(d) = e^{-k\,d^2}\quad(k=\text{intensity}),\qquad M(0)=1,\quad M(d)\xrightarrow{d\ \uparrow}0
$$

미적분에서 본 $e^{-x^2}$ 곡선이다 — **가운데($d=0$)에서 최대 $1$**, 멀어질수록 **매끄럽게 $0$** 으로 떨어진다.
$d^2$(제곱)을 쓰기 때문에 중앙에 뾰족한 꼭짓점 없이 **둥글고 부드러운** 감쇠가 된다. $k$(강도)가 클수록 빨리 어두워진다.

**(c) 마스크로 섞기** : `mix(a, b, t) = a(1-t) + b\,t` 에 마스크를 비율 $t$ 로 넣는다.

$$
\text{finalColor} = (1-M)\cdot \text{vignetteColor} + M\cdot \text{scene}
$$

- 중앙($M=1$) → **원본 장면** 그대로
- 모서리($M\to0$) → **vignette 색**(기본 빨강)으로 덮임

즉 가우시안 마스크가 "중앙=장면, 외곽=비네트색"을 결정하는 **부드러운 원형 스텐실** 역할을 한다.

---

## 3. Fog = **Depth Map**으로 실제 거리를 복원

안개는 **카메라에서 멀수록 안개색으로 흐려지는** 효과다. 핵심은 "그 픽셀이 얼마나 먼가?"를 아는 것이고,
그 정보가 바로 **깊이 맵(Depth Map)** 에 들어 있다. 색(`uScene`)과 별도로 깊이 텍스처(`uDepth`)를 함께 읽는다.

```glsl
float rawDepth = texture(uDepth, vUV).r;   // 0(가까움) ~ 1(멂), 단 비선형 NDC 값

if (rawDepth >= 0.9999) {                  // (a) 하늘/배경(far plane) 은 안개 제외
    fragColor = vec4(sceneColor, 1.0); return;
}

vec4 ndc     = vec4(vUV * 2.0 - 1.0, rawDepth * 2.0 - 1.0, 1.0); // (b) NDC 재구성
vec4 viewPos = uInverseProjection * ndc;   // (c) 역투영: NDC -> view 공간
viewPos /= viewPos.w;                       //     perspective divide 되돌리기
float dist = length(viewPos.xyz);           // (d) 카메라-픽셀 실제 거리
```

### 깊이 맵을 왜 그대로 못 쓰나

`uDepth.r` 에는 픽셀마다 $0\sim1$ 깊이가 적혀 있지만, 이 값은 **원근 투영으로 휘어진 NDC 좌표**라
실제 미터 거리와 비례하지 않는다(가까운 쪽이 훨씬 촘촘). 그래서 **역투영(un-project)** 으로 되돌린다.

**(b) NDC 재구성** : 화면 좌표와 깊이로 정규화 장치 좌표(NDC, $[-1,1]^3$ 큐브) 한 점을 만든다.
`vUV*2-1` 로 화면 $xy$ 를, `rawDepth*2-1` 로 $z$ 를 $[-1,1]$ 로 펼친다.

**(c) 역투영** : `uInverseProjection`(투영행렬의 역행렬)을 곱해 NDC를 **카메라(view) 공간**으로 되돌리고,
`viewPos /= viewPos.w` 로 **원근 나눗셈을 역으로 풀어** 진짜 3D 위치를 확정한다.

**(d) 거리** : `length(viewPos.xyz)` = 카메라 원점에서 그 픽셀 표면까지의 **유클리디안 거리**.
이렇게 깊이 맵 한 채널 → 실제 거리 $\text{dist}$ 로 복원하는 것이 **Fog의 Depth Map 활용 핵심**이다.

**(a) 하늘 제외** : `rawDepth >= 0.9999`(far plane)면 스카이박스/빈 배경이므로 안개를 입히지 않고 또렷이 둔다.

### 거리 → 안개량 → 합성

복원한 거리로 안개 농도 $\text{fogAmount}\in[0,1]$ 를 계산한다(3가지 모드).

$$
\text{Linear: } 1-\frac{\text{end}-d}{\text{end}-\text{start}},\quad
\text{Exp: } 1-e^{-\rho d},\quad
\text{Exp2: } 1-e^{-(\rho d)^2}
$$

- **Linear** : start~end 사이를 직선으로 보간(start 전=맑음, end 후=완전 안개).
- **Exp / Exp2** : 거리가 멀수록 지수적으로 짙어짐. Exp2는 $d^2$ 이라 더 급격(가우시안 형태).

마지막에 거리로 정한 농도만큼 안개색을 섞는다:

$$
\text{fragColor} = \text{mix}(\text{sceneColor},\ \text{fogColor},\ \text{fogAmount})
$$

$\text{fogAmount}=0$(가까움)이면 장면 그대로, $1$(멂)이면 완전히 안개색.

---

## 4. 한눈 요약

| 효과 | 입력 | 핵심 수학 | 합성 |
|------|------|-----------|------|
| **Bloom** | `uScene` | 휘도→`step`(bright-pass) + $9\times9$ **박스 블러 평균** | **덧셈** `base + bloom` |
| **Vignette** | `uScene` | 중심 거리 → **가우시안** $e^{-k d^2}$ 마스크 | `mix(비네트색, 장면, M)` |
| **Fog** | `uScene` + **`uDepth`** | 깊이 **역투영**→실제 거리 → 지수 안개량 | `mix(장면, 안개색, fog)` |

공통점: 모두 **풀스크린 패스**에서 `uScene` 을 읽어 픽셀별로 가공하고 `mix`/덧셈으로 합성한다.
차이점: Bloom·Vignette는 **색만**, Fog는 **깊이 맵을 추가로** 읽어 거리를 알아낸다는 점이다.

---

### 레퍼런스
* Fog factor 함수: https://github.com/hughsk/glsl-fog
* 함께 읽기: [`ShaderSkybox.md`](./ShaderSkybox.md) · [`SpriteAtlas.md`](./SpriteAtlas.md)

---

### 대본 — Bloom (Box Filter)

> 발표 시간: **1분 내외**. `[화면]` = `apps/_MyApp_/resources/shaders/postprocess/bloom.fs` (또는 본 문서 §1의 코드 블록).

"포스트프로세싱 중 **Bloom**을 설명드리겠습니다. Bloom은 네온이나 발광체처럼 밝은 부분이 빛 번짐으로 주변에 새어 나오게 하는 효과인데, 세 조각의 조합입니다."

> `[화면] bloom.fs` — 전체 셰이더를 띄우고, 아래 세 부분을 차례로 짚는다.

"먼저 **휘도**입니다. `dot(rgb, vec3(0.299, 0.587, 0.114))`로 RGB를 밝기 한 숫자로 합치는데, 가중치가 다른 건 사람 눈이 초록을 가장 민감하게 보기 때문입니다(Rec.709)."

> `[화면]` 휘도 계산 줄 — `float lum = dot(samp, vec3(0.299, 0.587, 0.114));`

"다음은 **bright-pass**. `step` 함수로 밝기가 문턱보다 큰 픽셀만 1로 골라내고, 어두운 픽셀은 0으로 버립니다 — 밝은 부분만 추출하는 거죠."

> `[화면]` `bloom += samp * step(uBloomThreshold, lum);`

"핵심은 **박스 블러**입니다. 한 픽셀을 그릴 때 주변 9×9, 즉 81칸을 모두 *똑같은 비중*으로 평균냅니다. 가우시안 블러는 가운데일수록 비중이 큰 종 모양 가중이지만, 박스 블러는 균일 가중이라 구현이 훨씬 쌉니다. 이 평균 덕분에 밝은 점이 이웃으로 퍼져 흐릿한 덩어리가 됩니다."

> `[화면]` 이중 for 루프(9×9 이웃 샘플) + `bloom /= 81.0;`

"마지막으로 원본 색에 이 번진 빛을 그냥 **더합니다**(additive). 섞거나 빼지 않고 더하니까 밝은 곳이 더 밝아지면서 글로우가 완성됩니다. 이상입니다."

> `[화면]` `fragColor = vec4(base + bloom * uBloomIntensity, 1.0);`

> **한 줄 흐름**: 휘도로 밝기 측정 → `step`으로 밝은 픽셀만 추출 → 9×9 박스 블러로 번지게 → 원본에 덧셈.

