## Depth Testing — 깊이 테스트

> 출처 노트: `FrameBuffer.md` (Depth Test 섹션)
> LearnOpenGL 매핑: Advanced OpenGL — 1. Depth testing

---

> ### 📄 1. Framebuffer 란 — 여러 buffer 의 묶음 (depth 의 맥락)

화면에 보이는 한 장의 그림은 사실 *여러 개의 buffer* 가 겹쳐 만들어진다.

| Buffer | 저장하는 것 | 비트 |
|--------|------------|------|
| **Color buffer** | 픽셀의 RGBA 색 | 보통 32-bit (RGBA8) |
| **Depth buffer** | 픽셀의 *깊이값* (카메라로부터의 거리) | 보통 24-bit |
| **Stencil buffer** | 픽셀별 마스크/태그 | 보통 8-bit (depth 와 같이 32-bit 패킹) |

`glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)` 가 *두 비트* 를 함께 지우는 이유 — color 와 depth 는 *별개 buffer*. 매 프레임 둘 다 초기화해야 한다. (FBO 전체 구조는 `05_Framebuffers.md`)

---

> ### 📄 2. Depth Test — 왜 필요한가

depth test 없이 3D 를 그리면 *그리는 순서대로* 색이 덮어써진다 (painter's algorithm). 카메라가 돌면 앞뒤가 뒤바뀌어 *뒤 물체가 앞 물체를 덮는* 깨짐이 생긴다.

**Depth test** = 새 fragment 를 그리기 전, *그 픽셀의 기존 깊이값* 과 *새 fragment 의 깊이값* 을 비교. 통과 시에만 color buffer + depth buffer 갱신.

```
fragment 생성 → depth 비교 (glDepthFunc 기준) → 통과? → color/depth 기록
                                              → 실패? → fragment 폐기
```

→ 그리는 순서와 무관하게 *항상 카메라에 가까운 면이 보인다*.

---

> ### 📄 3. Depth 관련 GL 호출 (모두 state-setting)

| GL 호출 | 역할 |
|---------|------|
| `glEnable(GL_DEPTH_TEST)` | depth test *켜기* — 끄면 painter's algorithm 으로 회귀 |
| `glDisable(GL_DEPTH_TEST)` | depth test *끄기* — §6 참조 |
| `glClear(GL_DEPTH_BUFFER_BIT)` | depth buffer 를 `glClearDepth` 값으로 초기화 (매 프레임 첫줄) |
| `glClearDepth(1.0f)` | clear 시 채울 깊이값 — 기본 1.0 (= 가장 멀리) |
| `glDepthFunc(func)` | depth *비교 연산자* 선택 (§4) |
| `glDepthMask(GL_FALSE)` | depth buffer *쓰기 막기* — 테스트는 하되 기록 안 함 (반투명 렌더 등) |

> 모두 **state-setting** — 한 번 호출하면 다음 draw 들이 그 상태를 본다. `glEnable(GL_DEPTH_TEST)` 는 *글로벌* 컨텍스트 상태. (분류: `01_GettingStarted/01_OpenGL_상태머신.md`)

---

> ### 📄 4. Depth 비교 연산자 — `glDepthFunc`

깊이값 범위는 `[0, 1]` — **0 = 가장 가까움, 1 = 가장 멀리**.

| 값 | 의미 |
|-----|------|
| `GL_ALWAYS` | 항상 통과 (depth test 무력화 효과) |
| `GL_NEVER` | 항상 실패 (아무것도 안 그려짐) |
| `GL_LESS` | 새 깊이 < 기존 → 통과 *(기본값)* — 더 가까우면 그림 |
| `GL_LEQUAL` | 같거나 가까우면 통과 |
| `GL_GREATER` | 더 멀면 통과 |
| `GL_GEQUAL` | 같거나 멀면 통과 |
| `GL_EQUAL` | 깊이가 정확히 같을 때만 |
| `GL_NOTEQUAL` | 깊이가 다를 때만 |

기본값 `GL_LESS` — "1(가장 멀리)보다 더 작은(가까운) 것을 통과". 즉 *가까운 면이 이긴다*.

> ImGui Combo 등으로 런타임 전환 시, 라벨 배열과 GL enum 배열의 *인덱스 순서가 1:1 동일* 해야 라벨과 동작이 어긋나지 않는다.

---

> ### 📄 5. Depth 값의 비선형 분포 — z-fighting

Perspective projection 은 깊이값을 `[0, 1]` 로 정규화하며 `w` 로 나눈다. 결과적으로 정규화 z 는 **`1/z` 꼴** 로 분포 — *가까운 곳은 정밀, 먼 곳은 듬성듬성*.

```
실제 거리:  near ──────────────────────────── far
정규화 z :  0   0.5  0.8  0.9 0.95 ......... 1.0
            ↑ 가까운 쪽에 정밀도 집중      ↑ 먼 쪽은 값 차이 미미
```

**z-fighting**: 먼 거리의 두 면은 정규화 z 값이 *거의 같아* depth test 가 앞뒤를 판정 못 해 픽셀이 *깜빡이며 다투는* 현상.

**예방**:
- 면과 면을 *너무 가깝게 겹치지* 않기.
- `near` 평면을 *너무 작게* 잡지 않기 — near 가 작을수록 `1/z` 곡선이 가팔라져 먼 쪽 정밀도가 더 망가짐.
- 더 정밀한 depth buffer (24→32-bit) 사용.

---

> ### 📄 6. Depth Test 를 *끄는* 경우

`glEnable(GL_DEPTH_TEST)` 가 기본이지만, depth 와 무관하게 *항상 앞* 또는 *항상 뒤* 로 그려야 할 때 끈다.

| 상황 | 이유 |
|------|------|
| **ImGui / HUD / UI** | UI 는 3D 씬 *위에 항상* 떠야 함 — depth 비교 대상 아님 |
| **Skybox** | 항상 *가장 뒤* — `GL_LEQUAL` + depth=1.0 트릭 또는 test off |
| **반투명 (blend) 객체** | depth *test* 는 하되 *write* 는 막음 (`glDepthMask(GL_FALSE)`) — 뒤 객체가 비쳐 보이도록 |

---

> ### 📄 7. `DepthTest` vs `DepthWrite` — 직교한 두 노브

§3 에서 본 `glEnable(GL_DEPTH_TEST)` 와 `glDepthMask` 는 *완전히 다른 두 단계* 를 켜고 끈다. 같은 "depth" 라는 단어를 공유할 뿐.

| 플래그 | GL 호출 | 결정하는 것 |
|---|---|---|
| `DepthTest` | `glEnable/Disable(GL_DEPTH_TEST)` | "기존 z vs 새 z **비교** 자체를 할 것인가" |
| `DepthWrite` | `glDepthMask(GL_TRUE/FALSE)` | "통과한 fragment 의 z 를 buffer 에 **기록**할 것인가" |

```
fragment → [DepthTest: 비교]  → [DepthWrite: 기록]
              ↑ 끄면 무조건 통과     ↑ 끄면 read-only (비교는 했지만 z 안 남김)
```

#### 네 가지 조합

| Test | Write | 의미 | 실제 케이스 (`src/material/pass.h` Kind) |
|---|---|---|---|
| ⭕ / ⭕ | 비교 + 기록 | `Opaque`, `AlphaTest` — 일반 불투명 |
| ⭕ / ❌ | 비교만, 기록 안함 | `Skybox`, `Transparent` |
| ❌ / ⭕ | (GL spec 상 무의미 — Test 가 꺼지면 Write 도 자동 무력화) | — |
| ❌ / ❌ | 모두 무시 | UI/HUD |

> ⚠️ `glDisable(GL_DEPTH_TEST)` 면 `glDepthMask(GL_TRUE)` 여도 z 가 *안 써진다*. **`DepthWrite=true` 는 `DepthTest=true` 의 전제 위에서만 의미**. "Test 끄고 Write 만 켜기" 는 사실상 존재하지 않는 조합.

#### `DepthWrite` vs `ColorMask` — *다른 buffer 의 별개 마스크*

흔한 오해: "Skybox 의 `Write=false` 가 *색* 을 안 그리겠다는 뜻인가?" — 아니다. §1 표에서 본 대로 Color buffer 와 Depth buffer 는 별개. 마스크도 별개.

| 마스크 | GL 호출 | 통제하는 buffer |
|---|---|---|
| `DepthWrite` | `glDepthMask` | Depth buffer **만** |
| `ColorMask` | `glColorMask(r,g,b,a)` / `glDrawBuffer(GL_NONE)` | Color buffer **만** |

| ColorMask | DepthWrite | 의미 | 케이스 |
|---|---|---|---|
| ⭕ ⭕ | 보통 그리기 | Opaque |
| ⭕ ❌ | **색은 칠하되 z 안 남김** | Skybox / Transparent |
| ❌ ⭕ | **z 만 남기고 색 안 칠함** | 그림자맵 prepass (`05_Framebuffers.md` §7) |
| ❌ ❌ | 둘 다 차단 | Stencil 전용 마스킹 패스 |

→ Skybox 가 `DepthWrite=false` 라는 건 "**색은 정상적으로 화면에 칠해진다**. 단지 그 픽셀의 z 를 *기록* 하지 않을 뿐". 시각적으로 관련돼 *보이는* 이유는 — 다음 draw 의 *depth test 판정* 이 바뀌어 → 다음 draw 의 color 가 *결과적으로* 달라지는 *간접 영향* 때문 (§8 에서 추적).

---

> ### 📄 8. Pass Queue 시뮬레이션 — Opaque / AlphaTest / Skybox / Transparent

Unity / Cocos 의 *Render Queue 정수* (`src/material/pass.h::Kind`) 순서대로 한 프레임을 그릴 때, Color/Depth buffer 가 어떻게 변하는지 추적. §7 의 4분면이 실제로 *왜* 그런 조합인지 보여준다.

**시나리오**: 화면 한 줄 10픽셀, 카메라는 왼쪽.

```
카메라 →  [유리창]  [건물]  [잎사귀]  [하늘(skybox)]
z=        0.3       0.5     0.7       1.0 (셰이더 강제)
픽셀:     2~5      0~7      6~9       전체 화면
```

매 프레임 시작:
```
Color  = [ ☐ ☐ ☐ ☐ ☐ ☐ ☐ ☐ ☐ ☐ ]  (cleared, 검정)
Depth  = [ 1 1 1 1 1 1 1 1 1 1 ]  (cleared = 1.0)
```

#### Pass A — Opaque (Q=2000) : `Test⭕ Write⭕ Blend❌ Cull=BACK`

건물 fragment z=0.5, 픽셀 0~7.

| 픽셀 0~7 | 기존 z | LEQUAL? | Color | Depth |
|---|---|---|---|---|
| 1.0 | 1.0 | 0.5 ≤ 1.0 ⭕ | 🟫 건물색 | 0.5 |

```
Color  = [ 🟫 🟫 🟫 🟫 🟫 🟫 🟫 🟫 ☐ ☐ ]
Depth  = [.5 .5 .5 .5 .5 .5 .5 .5  1  1 ]
```

→ Write⭕ 라서 **이후 Pass 들이 "z=0.5 영토" 를 인식**. 불투명체의 핵심 역할 — *z 영토를 차지해 뒤 객체를 가리는 권리* 를 얻음.

#### Pass B — AlphaTest (Q=2450) : Opaque 와 **동일** state

잎사귀 z=0.7, 픽셀 6~9. fragment shader 가 `alpha < 0.5` 면 `discard` — 픽셀 6,8 통과, 7,9 잎 사이 구멍.

| 픽셀 | 기존 z | discard? | LEQUAL? | 결과 |
|---|---|---|---|---|
| 6 | 0.5 | 통과 | 0.7 ≤ 0.5 ❌ | (건물 유지) |
| 7 | 0.5 | ❌ discard | — | — |
| 8 | 1.0 | 통과 | 0.7 ≤ 1.0 ⭕ | 🟢 잎, z=0.7 |
| 9 | 1.0 | ❌ discard | — | — |

```
Color  = [ 🟫 🟫 🟫 🟫 🟫 🟫 🟫 🟫 🟢 ☐ ]
Depth  = [.5 .5 .5 .5 .5 .5 .5 .5 .7  1 ]
```

> 💡 **AlphaTest = Opaque + discard 셰이더**. State 가 똑같은데 queue 만 분리하는 이유 — `discard` 가 early-z 최적화를 깨므로 Opaque 다음에 *모아* 처리. `discard` 된 fragment 는 *애초에 없었던 것처럼* 처리되므로 Write 켜도 안전.

#### Pass C — Skybox (Q=2500) : `Test⭕ Write❌ Blend❌ Cull=FRONT DepthFunc=LEQUAL`

셰이더 `.xyww` 트릭으로 *모든* skybox fragment 의 z=1.0 강제. cube *안쪽* 에서 보므로 front-face 컬링.

| 픽셀 | 기존 z | LEQUAL? | Color | Depth |
|---|---|---|---|---|
| 0~7 | 0.5 | 1.0 ≤ 0.5 ❌ | (건물 유지) | 0.5 유지 |
| 8 | 0.7 | 1.0 ≤ 0.7 ❌ | (잎 유지) | 0.7 유지 |
| 9 | 1.0 | 1.0 ≤ 1.0 ⭕ | 🟦 하늘색 | **(Write❌ → 1.0 안 적힘)** |

```
Color  = [ 🟫 🟫 🟫 🟫 🟫 🟫 🟫 🟫 🟢 🟦 ]
Depth  = [.5 .5 .5 .5 .5 .5 .5 .5 .7  1 ]
```

→ 만약 Write⭕ 였다면 픽셀 9 에 z=1.0 *명시 기록*. 어차피 cleared 1.0 과 동일해 *이 프레임은* 결과 동일. 그럼 왜 굳이 끄나? **다음 Pass D 에서 갈린다** (아래).

#### Pass D — Transparent (Q=3000) : `Test⭕ Write❌ Blend⭕ Cull❌(off)`

유리창 z=0.3, 픽셀 2~5, α=0.4.

| 픽셀 2~5 | 기존 z | LEQUAL? | Blend = src·α + dst·(1-α) | Depth |
|---|---|---|---|---|
| 0.3 | 0.5 | 0.3 ≤ 0.5 ⭕ | 🟫·0.6 + 🩵·0.4 = 🟦 합성 | **(Write❌ → 0.5 유지)** |

```
Color  = [ 🟫 🟫 🟦 🟦 🟦 🟦 🟫 🟫 🟢 🟦 ]
Depth  = [.5 .5 .5 .5 .5 .5 .5 .5 .7  1 ]
```

#### `Write=⭕` 회귀 시나리오 — 왜 Transparent 는 *반드시* 꺼야 하나

z 가 거의 같은 두 반투명 (잎사귀 0.70 / 0.71) 이 잘못된 정렬로 들어오면:
- **Write⭕**: 뒤 잎이 먼저 z=0.71 기록 → 앞 잎(0.70) 의 일부 픽셀이 z 차이로 fail → 알파 합성에 *구멍*
- **Write❌**: 둘 다 건물 z=0.5 만 비교 → 둘 다 통과 → 부드러운 합성

#### 4 Pass 의 영토 권리 요약

| Pass | Color 기여 | Depth 영토 | 이유 |
|---|---|---|---|
| Opaque | ⭕ 칠함 | ⭕ **차지** | 모든 비교의 기준점 |
| AlphaTest | ⭕ (구멍 제외) | ⭕ 차지 (구멍 제외) | discard 가 fragment 죽임 → Write 켜도 안전 |
| Skybox | ⭕ (빈 곳만) | ❌ 안 남김 | 남기면 뒤 Transparent 가 *역가림* 당함 |
| Transparent | ⭕ 합성 | ❌ 안 남김 | 반투명끼리 서로 통과시켜 알파 합성 유지 |

> **한 줄 요약: "Color 는 4 Pass 모두 칠한다. 차이는 *Depth 라는 영토* 를 누가 차지하느냐."** Opaque/AlphaTest 만 차지, Skybox/Transparent 는 *통행만*.

---

> ### 📄 9. 다음 단계 — Framebuffer Object (FBO)

여기까지가 *default framebuffer* (윈도우 화면) + depth buffer 의 기초. depth test 가 *default framebuffer 의 depth buffer* 를 다뤘다면, FBO 단계에선 *내가 depth attachment 를 직접 만들어 붙인다* — depth buffer 가 "자동으로 거기 있는 것"이 아니라 *framebuffer 의 한 attachment* 임을 그때 체감한다. (→ `05_Framebuffers.md`)

## 시험 포인트 요약
- depth test = 픽셀별 기존 z vs 새 z 비교, 통과 시만 갱신. 기본 `GL_LESS`.
- z 는 `[0,1]`, 0=가까움 1=멀리. 분포는 `1/z` 라 먼 쪽이 듬성 → z-fighting.
- 반투명: depth **test 는 하되 write 는 막음**(`glDepthMask(FALSE)`).
- UI/Skybox 는 depth test off 또는 트릭.
- **DepthTest vs DepthWrite**: 비교 비트 / 기록 비트 — 직교한 두 노브. Test 가 꺼지면 Write 도 무력.
- **DepthWrite vs ColorMask**: 서로 다른 buffer 의 별개 마스크. Skybox `Write=false` 가 색을 막는 게 아니다.
- **Pass Queue 4 종**: Color 는 모두 칠하되 *Depth 영토* 는 Opaque/AlphaTest 만 차지.

## 관련 노트
- depth 관련 호출이 state-setting 이라 순서 의존: `01_GettingStarted/01_OpenGL_상태머신.md`
- depth 가 blending 보다 앞이라 생기는 투명 문제: `04_AdvancedOpenGL/03_Blending.md`
- FBO 의 depth attachment (Texture vs RBO): `04_AdvancedOpenGL/05_Framebuffers.md`
- depth 와 짝이지만 다른 일을 하는 stencil: `04_AdvancedOpenGL/02_StencilTesting.md`
- Pass Kind ↔ Queue 정수 ↔ GL state 매핑: `src/material/pass.h` (`DefaultPipelineStateOf`)
