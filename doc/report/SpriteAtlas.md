# 스프라이트 아틀라스 — sub-rect UV 매핑 (요약)

> 코드: `billboard_atlas.vs` / `billboard_atlas.fs` · 대상: 1학년 1학기 미적분 수료
> 함께 읽기: [`ShaderSkybox.md`](./ShaderSkybox.md) — 같은 **UV 분할**이 이어진다.

## 1. 무엇을 하나

**아틀라스(atlas)** = 여러 그림을 한 장에 모은 큰 텍스처. 텍스처는 GPU에 **한 번만** 올리고,
**좌표만 바꿔** 원하는 한 칸만 떼어 쓴다(텍스처 교체가 없어 빠르다).

```
   +-------+-------+-------+-------+
   | 걷기0 | 걷기1 | 걷기2 | 사망  |
   +-------+-------+-------+-------+   "이번 프레임엔 '걷기2' 칸만" -> 좌표만 갱신
```

## 2. 핵심: sub-rect 매핑

원하는 칸은 uniform `uUvRect = (uMin, vMin, uSize, vSize)` 로 알려준다.
`xy` = 칸의 **왼아래 모서리**, `zw` = 칸의 **가로·세로 크기** (모두 아틀라스 전체 $0\sim1$ 기준).

```glsl
vUv = uUvRect.xy + aTexCoord * uUvRect.zw;   // 시작점 + 지역좌표 × 크기
```

$\mathbf{aTexCoord}$ 는 quad의 $0\sim1$ **지역 좌표**다. 이를 그 칸 영역으로 확대·평행이동한다:

$$
\mathbf{vUv} = \underbrace{\mathbf{o}}_{\text{시작점}} + \underbrace{\mathbf{t}}_{0\sim1}\odot \underbrace{\mathbf{s}}_{\text{칸 크기}}
\quad\Rightarrow\quad
(0,0)\to\text{왼아래},\ \ (1,1)\to\text{오른위}
$$

이게 0~1을 원하는 사각형으로 옮기는 **아핀 매핑(affine remap)** 이다.
**`uUvRect` 숫자 4개만 바꾸면** 애니메이션 프레임·무기·이펙트 종류가 전부 교체된다.

## 3. 스카이박스의 UV 분할과 같은 원리

[`ShaderSkybox.md`](./ShaderSkybox.md) 글자칸 매핑 `(cellLeftPx + inCell.x * CHAR_W) / ATLAS_W` 도
**`시작점 + 지역좌표 × 칸크기`** 로 동일하다. 차이는 **"칸을 누가 고르느냐"** 하나뿐:

| | 스카이박스 | 아틀라스 빌보드 |
|---|---|---|
| 칸 선택 | **셰이더가** `floor/fract` 로 즉석 계산 | **CPU가** 미리 계산 → `uUvRect` 전달 |
| 지역좌표 | `fract(grid_uv)` | `aTexCoord` |
| 매핑식 | `시작 + 지역 × 크기` | `시작 + 지역 × 크기` (동일) |

아틀라스는 C++ `ComputeUVRect` 가 "몇 행 몇 열, 몇 번째 칸"을 $0\sim1$ 사각형으로 바꿔 넘긴다.

## 4. Forward vs Inverse Mapping — 둘 다 **역매핑(Inverse)**

| | 진행 방향 | 한 줄 |
|---|---|---|
| Forward (scatter) | 텍스처 → 화면 | "이 텍셀을 화면 어디로 뿌리지?" |
| **Inverse (gather)** | **화면 → 텍스처** | **"이 화면 픽셀은 텍스처 어디서 가져오지?"** |

프래그먼트 셰이더의 `texture(uAtlas, vUv)` 는 **각 픽셀이 텍스처에서 한 텍셀을 끌어오는(gather)** 동작이라
**Inverse Mapping**이다. 2절의 아핀 remap은 "**끌어올 위치(칸)를 옮기는**" 역할일 뿐, 읽기 자체는 gather다.
(스카이박스는 그 끌어올 좌표를 FS에서 역투영 $\operatorname{atan2}/\arcsin$ 으로 직접 계산한다는 점만 다르다.)
GPU 래스터가 늘 gather라 출력 픽셀마다 한 번씩 읽어 **구멍·겹침이 없다.**

## 5. 곁가지 (UV 분할과 독립)

- **빌보드 정점 셰이더**: quad를 월드축이 아니라 **카메라의 right/up 축**에 붙여(`cameraRight/cameraUp`)
  어느 각도에서 봐도 그림이 **정면**을 보게 한다(종이 인형). `uFlipX` 좌우반전, `uRoll` 화면 내 회전.
- **프래그먼트 효과**: `alpha < 0.01` → `discard`(투명 배경 버림) / `uEnableHit` 흰↔빨강 깜빡임 /
  `uEnableDissolve` 노이즈 임계치로 너덜너덜 사라지며 경계는 발광색 테두리. 모두 색칠 단계라 UV와 무관.

## 6. 한 줄 요약

- **sub-rect 매핑**: `vUv = uUvRect.xy + aTexCoord × uUvRect.zw` = `시작점 + 지역좌표 × 칸크기`.
- 스카이박스와 **같은 아핀 매핑·같은 역매핑(gather)**, 차이는 칸 선택을 **CPU가 미리 해서 넘긴다**는 것뿐.

---

### 레퍼런스
* 함께 읽기: [`ShaderSkybox.md`](./ShaderSkybox.md) · 구현: `src/sprite/` (`ComputeUVRect`, `UniformAtlas`)
