## Blending — 투명 / 반투명 합성

> 출처 노트: `FrameBuffer.md` (Blending / Discard / OIT 섹션 + 블렌딩 API 섹션). 엔진 plumbing(MeshPassProcessor/PassKind) 디버깅 서사는 제외하고 그래픽스 이론 핵심만 보존.
> LearnOpenGL 매핑: Advanced OpenGL — 3. Blending

![Full transparent window vs Partially transparent window](image/2026-05-17-20-54-59.png)

---

> ### 📄 1. 블렌딩 API

**활성화**: `glEnable(GL_BLEND);`

**블렌딩 함수**: `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);`

**블렌딩 수식**:
$$
C_{result} = (C_{source} \cdot F_{source}) + (C_{destination} \cdot F_{destination})
$$

- `glBlendFunc` 으로 `F` 값(factor)을 설정
- `glBlendEquation` 으로 가운데 `+` 연산자를 설정

가능한 factor 값: `GL_ZERO`, `GL_ONE`, `GL_SRC_COLOR`, `GL_SRC_ALPHA`, `GL_ONE_MINUS_SRC_COLOR`, `GL_ONE_MINUS_SRC_ALPHA`, `GL_DST_COLOR`, `GL_DST_ALPHA`, `GL_ONE_MINUS_DST_COLOR`, `GL_ONE_MINUS_DST_ALPHA`, `GL_CONSTANT_COLOR`, `GL_CONSTANT_ALPHA` 등.

**Color/Alpha 분리**: `glBlendFuncSeparate` — color 와 alpha 에 별도 factor 적용.

`glBlendEquation` 값:
```
GL_FUNC_ADD: src + dst
GL_FUNC_SUBTRACT: src - dst
GL_FUNC_REVERSE_SUBTRACT: dst - src
GL_MIN: min(src, dst)
GL_MAX: max(src, dst)
```

---

> ### 📄 2. 무엇이 깨지는가 — Depth Test 가 Blending 보다 *앞*

```
[Vertex Shader] → [Rasterization] → [Fragment Shader] → [Depth Test] → [Blending]
                                          ↓                ↑              ↑
                                  alpha 계산 시점    여기서 alpha 무시   여기서야 alpha 등장
```

**핵심 — Depth Test 단계는 fragment 의 z 만 보고 alpha 는 모른다.** alpha 는 그 뒤 blending 에서야 쓰인다. 따라서:

가까운 반투명 창(Window0)을 먼저 그리면 depth buffer 에 z=Z0 기록 → 뒤의 먼 창(Window1)은 depth test 탈락 → Window1 의 fragment 버려짐 → **유리창인데 뒤가 안 보이는 모순**.

> **셰이더는 depth buffer 의 *값* 을 읽지 못한다.** 현재 fragment 의 z(`gl_FragCoord.z`)만 안다. 통과/탈락은 GPU 고정 단계가 처리.

---

> ### 📄 3. 해결책 A — Sort + Painter's Algorithm (정통)

```
1. Opaque 먼저 (front-to-back z-cull 효율)
2. Transparent 만 back-to-front sort (멀리 → 가까이)
3. Transparent draw 직전 glDepthMask(GL_FALSE) — 자기들끼리 가리지 않게
```

- **장점**: 단순, 모든 엔진 기본
- **한계**: 메시 *내부* self-overlap (오목 알파, intersecting quad) 해결 불가 — 정렬은 *물체 단위* 인데 정합성은 *픽셀 단위* 가 필요

> 📌 위 3-스텝이 실제 한 프레임에서 Color/Depth buffer 를 어떻게 바꾸는지 (Opaque → AlphaTest → Skybox → Transparent 4 Pass 추적) — `04_AdvancedOpenGL/01_DepthTesting.md` §8 참조. `glDepthMask(FALSE)` 가 *반드시* 필요한 이유 (회귀 시나리오 포함) 도 함께.

---

> ### 📄 4. 해결책 B — Alpha-tested Discard (이진 알파 전용)

```glsl
vec4 c = texture(uMainTex, vsTexCoord);
if (c.a < 0.01) discard;     // depth 갱신 회피 + color write 회피
fragColor = c;
```

| 알파 값 | discard 처리 | depth 갱신 | 결과 |
|---|---|---|---|
| `alpha = 0` (완전 투명, 창틀 구석) | ✅ | ❌ 안 함 | 뒤 픽셀 통과 — 해결 |
| `alpha = 0.5` (반투명 유리) | ❌ | ✅ 함 | 뒤 픽셀 가림 — 문제 잔존 |

- **장점**: sort 불필요. 잔디 / 나뭇잎 / 창틀 구석 정통
- **한계**: 부드러운 알파(반투명 유리)는 불가능

---

> ### 📄 5. 해결책 C — OIT (Order-Independent Transparency, 고급)

| 변형 | 핵심 | 비용 |
|---|---|---|
| **Depth Peeling** | depth 를 N pass 로 벗겨서 per-layer 누적 | N pass × N FBO |
| **Weighted Blended OIT** (McGuire 2013) | weight 함수로 단일 pass 근사 합성 | 1 pass, 약간 부정확 |
| **Per-Pixel Linked List** | atomic 으로 fragment 를 픽셀별 리스트에 push, 셰이더가 sort 후 합성 | GL 4.2+ atomic, 메모리 대량 |

- **장점**: 순서 무관 — fragment-level 정확한 합성
- **한계**: GL 4.x 의존, 메모리/시간 비용 큼

---

> ### 📄 6. 투명 객체는 *양면* 이어야 한다 — back-face cull 충돌

두께 없는 면(유리창/잎사귀/의류)에 back-face culling(`GL_BACK`)을 켜두면, 카메라가 면 *뒤쪽* 으로 가면 back face 가 통째로 잘려 **객체가 사라진다**.

- **Back-face culling** = *솔리드(closed) 메시* 성능 최적화 — 뒷면 fragment skip
- **Transparent plane** = 두께 없는 면 — *어느 쪽에서 봐도 보여야* 함
- 두 의도가 직교 → 투명은 **cull off (양면)** 이 정통

| 엔진 / 자료 | Transparent 의 Cull 기본 |
|---|---|
| LearnOpenGL Blending 챕터 | `glDisable(GL_CULL_FACE)` 명시 |
| Unity URP Lit Transparent | Render Face = Both |
| Filament `blending: transparent` | `doubleSided = true` |
| → 정통 결론 | **Transparent = cull off (양면)** |

---

> ### 📄 7. 의사결정 트리

```
alpha 가 있는 텍스처?
   ├─ alpha 가 *이진* (0 또는 1)?
   │     └─ YES → 해결책 B (discard) 단독
   ├─ alpha 가 *그라데이션* (반투명)?
   │     ├─ 메시 *간* 순서만? → 해결책 A (sort + DepthMask=FALSE)
   │     └─ 메시 *내부* self-overlap 도 정확히? → 해결책 C (OIT)
   └─ 두 종류 혼재 (창틀 + 유리)? → A + B 조합
```

## 시험 포인트 요약
- 블렌딩 식: `C = C_src·F_src + C_dst·F_dst`, 표준 alpha blend = `(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`.
- **Depth test 가 Blending 보다 앞** — alpha 모른 채 fragment 통과/탈락. 문제의 근본.
- `discard` = 이진 알파의 정답 (depth 갱신 회피).
- 반투명 = back-to-front sort + `glDepthMask(GL_FALSE)`.
- fragment-level 정확도는 OIT 만.
- 투명 면은 **cull off (양면)**.

## 관련 노트
- depth test / `glDepthMask` 자체: `04_AdvancedOpenGL/01_DepthTesting.md`
- 파이프라인 단계가 state-setting 순서에 의존: `01_GettingStarted/01_OpenGL_상태머신.md`
