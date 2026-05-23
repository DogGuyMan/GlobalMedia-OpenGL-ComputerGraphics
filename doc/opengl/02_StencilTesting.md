## Stencil Testing — 스텐실 테스트

> 출처 노트: `FrameBuffer.md` §8~10
> LearnOpenGL 매핑: Advanced OpenGL — 2. Stencil testing

---

> ### 📄 1. Depth 와 무엇이 다른가

depth buffer 다음으로 만나는 framebuffer 의 또 한 attachment 가 **stencil buffer**. 이름이 비슷하지만 *완전히 다른 일* 을 한다.

> 🎨 **Photoshop 비유**
> - **Depth buffer** = 레이어의 *앞뒤 순서* 를 픽셀 단위로 자동 판정. "이 픽셀에선 어느 레이어가 위인가?"
> - **Stencil buffer** = *선택 영역(마퀴 / 레이어 마스크)*. "여기는 칠해도 되는 영역, 여기는 안 되는 영역" 을 흑백 도장처럼 찍어 두는 것.

| | **Depth Test** | **Stencil Test** |
|---|---|---|
| 픽셀당 저장 자료형 | `float` `[0,1]` — *거리* | `integer` 8-bit `0~255` — *태그/마스크 번호* |
| 값을 누가 쓰나 | GPU 가 fragment 의 z 를 **자동** 기록 | 개발자가 `glStencilOp` 로 **무슨 값 쓸지 직접 지정** |
| 비교하는 것 | 새 fragment z `vs` 기존 z | 새 fragment 의 `ref` `vs` 기존 stencil 값 |
| 고유 작업 | **가림 판정** — 누가 더 앞인가 | **영역 마스킹** — 어디에 그릴/안 그릴 것인가 |

#### 서로 대체 불가능한 이유
- **Depth 로는 "이 모양 안쪽만" 같은 임의 영역 마스킹을 못 한다.** depth 는 *거리* 만 안다 — 모양 개념이 없다.
- **Stencil 로는 "누가 더 가까운가" 판정을 못 한다.** stencil 은 *거리* 개념이 없다 — 그냥 정수 도장.

→ 둘은 *겹치지 않는 책임* 을 가진 별개 buffer.

---

> ### 📄 2. Stencil 핵심 함수 3종

| 함수 | 역할 | Photoshop 비유 🎨 |
|------|------|------------------|
| `glStencilFunc(func, ref, mask)` | stencil **테스트 조건** — `(ref & mask)` 와 `(저장값 & mask)` 를 `func` 로 비교 | "선택 영역 안 픽셀만 통과" 조건 |
| `glStencilOp(sfail, dpfail, dppass)` | 테스트 **결과별로 stencil 값을 어떻게 바꿀지** | 붓질 닿은 자리에 마스크 도장을 어떻게 찍을지 |
| `glStencilMask(mask)` | stencil buffer **쓰기 비트 마스크** — `0x00` 이면 *쓰기 잠금* | 마스크 레이어 자체를 수정 잠금 |

#### `glStencilOp(sfail, dpfail, dppass)` — 3가지 결과 분기

| 인자 | 언제 동작 |
|------|----------|
| `sfail` | stencil test *실패* |
| `dpfail` | stencil 통과했지만 depth test *실패* |
| `dppass` | stencil + depth **둘 다 통과** |

가능한 동작 값: `GL_KEEP`(유지, 기본) / `GL_ZERO` / `GL_REPLACE`(ref 로 교체) / `GL_INVERT` / `GL_INCR`·`GL_DECR`(±1, 한계 정지) / `GL_INCR_WRAP`·`GL_DECR_WRAP`(±1, wrap).

---

> ### 📄 3. 예제 — Object Outlining (오브젝트 외곽선)

stencil 의 대표 활용. 오브젝트를 그릴 때 그 자리에 도장을 찍어두고, 살짝 키운 외곽선 셰이더로 다시 그리되 *도장 안 찍힌 자리에만* 그리면 — 테두리만 남는다.

#### 이론 6단계 ↔ 코드

```cpp
// ── 1. stencil buffer 를 0 으로 클리어 ──
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
glEnable(GL_STENCIL_TEST);

// ── 2. 오브젝트 자리에 stencil=1 도장 ──
//    항상 통과(GL_ALWAYS), 통과 픽셀의 stencil 을 ref(=1) 로 교체
glStencilFunc(GL_ALWAYS, 1, 0xFF);
glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);   // dppass 시 stencil ← 1
glStencilMask(0xFF);                          // 쓰기 허용
DrawObject(normalShader);                     // 본체 렌더

// ── 3. depth test off + stencil 쓰기 잠금 ──
//    외곽선이 본체에 가려지지 않도록 depth off. 이제 stencil 은 읽기만
glDisable(GL_DEPTH_TEST);
glStencilMask(0x00);

// ── 4. 오브젝트를 살짝 키워 외곽선 전용 셰이더로 ──
glm::mat4 scaledUp = glm::scale(modelMatrix, glm::vec3(1.1f));

// ── 5. stencil != 1 인 픽셀에만 그림 ──
//    본체가 찍은 1 영역은 건너뛰고, 키워서 삐져나온 가장자리만 칠함
glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
DrawObject(outlineShader, scaledUp);

// ── 6. 원상복구 ──
glEnable(GL_DEPTH_TEST);
glStencilMask(0xFF);
glStencilFunc(GL_ALWAYS, 1, 0xFF);
```

| 단계 | 핵심 | 🎨 Photoshop |
|------|------|-------------|
| 1 | 마스크 빈 상태로 | 선택 해제 |
| 2 | 본체를 그리며 그 모양 = stencil 1 영역 | 도형 + 같은 모양 선택 영역 |
| 3 | depth off + stencil 잠금 | 마스크 잠금 + 레이어 순서 무시 |
| 4 | 1.1배 확대 | 선택 영역 확장 |
| 5 | `NOTEQUAL 1` → 본체 영역 제외, 삐져나온 테두리만 | 확장 영역 − 원본 = 테두리 링 |
| 6 | 상태 원복 | 도구 정리 |

#### 핵심 직관
> outline 의 본질 = **"키운 모양"에서 "원래 모양"을 뺀 차집합**. stencil buffer 가 그 "원래 모양"을 기억하는 도구. depth buffer 로는 절대 못 한다 — 모양을 기억하는 건 stencil 의 고유 능력.

---

## 주의사항
- stencil 을 쓰려면 *framebuffer 에 stencil attachment 가 있어야* 한다. GLFW default framebuffer 는 보통 24+8 packed (depth+stencil) 를 제공해 별도 설정 없이 대개 사용 가능.
- `glClear` 시 `GL_STENCIL_BUFFER_BIT` 를 빠뜨리면 이전 프레임 도장이 남아 outline 이 깨진다 (depth 의 `GL_DEPTH_BUFFER_BIT` 와 같은 규칙).

## 시험 포인트 요약
- stencil = **8-bit 정수 마스크/태그**, depth = **float 거리**. 책임이 직교 (대체 불가).
- 3종 함수: `glStencilFunc`(조건) / `glStencilOp`(결과별 쓰기) / `glStencilMask`(쓰기 잠금).
- `glStencilOp` 3분기: sfail / dpfail / dppass.
- outlining = 확대 도형 − 원본 도형 = 테두리 (`GL_REPLACE` 로 도장 → `GL_NOTEQUAL` 로 바깥만).

## 관련 노트
- depth 와의 차이·짝 관계: `04_AdvancedOpenGL/01_DepthTesting.md`
- stencil attachment 를 FBO 에 직접 붙이는 경우: `04_AdvancedOpenGL/05_Framebuffers.md`
