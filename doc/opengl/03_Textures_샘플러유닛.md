## 텍스처 바인딩 & 샘플러 Uniform — 어디에 두어야 하는가

> 출처 노트: `UniformTexture.md`
> LearnOpenGL 매핑: Getting started — 6. Textures (+ Lighting 3·4 Materials/Lighting maps 에서 재사용)
> **이 노트가 "텍스처 핸들 vs 유닛 번호" 와 "텍스처 상태 스코프" 의 단일 출처(SSoT)다.**

> **상황**: 박스 10개를 같은 셰이더 + 같은 diffuse/specular 텍스처로 그리는 중. 다음 6줄을 `startup()` / `render()` 루프 바깥 / `render()` 루프 안 중 어디에 둘지 결정한다.
>
> ```cpp
> glActiveTexture(GL_TEXTURE1);
> glBindTexture(GL_TEXTURE_2D, textures[1]);
> glActiveTexture(GL_TEXTURE2);
> glBindTexture(GL_TEXTURE_2D, textures[2]);
> glUniform1i(glGetUniformLocation(prog, "material.diffuse"),  ???);
> glUniform1i(glGetUniformLocation(prog, "material.specular"), ???);
> ```

---

> ### 📄 1. 메커니즘부터 — 상태가 어디에 저장되는가

위치를 결정하기 전에, **각 GL 콜이 어떤 저장소에 쓰는지**를 먼저 구분해야 한다. 이게 핵심 분기점이다.

#### (A) 컨텍스트 전역 상태 — `glActiveTexture` + `glBindTexture`

- `GL_TEXTURE0`, `GL_TEXTURE1`, ... `GL_TEXTUREn` 슬롯은 **드라이버 컨텍스트 전역 상태**.
- "유닛 1번에 어떤 텍스처가 바인딩되어 있는가" 는 누군가 `glBindTexture`로 다시 바꾸기 전까지 유지된다.
- -> **다른 코드가 같은 유닛을 침범하면 깨진다 (state leak).**
- 멀티스레드, ImGui, 프레임워크 내부, 다른 셰이더 패스가 모두 잠재적 침범자.

#### (B) 프로그램 객체 로컬 상태 — `glUniform1i(sampler, unit)`

- 샘플러 uniform은 **프로그램 객체에 저장**된다. 평범한 정수 1개.
- 한 번 set하면 다음 셋 중 하나가 일어나기 전까지 영구 유지: ① 그 프로그램이 다시 링크됨 (`glLinkProgram`) ② 다시 `glUniform1i`로 덮어씀 ③ 프로그램 삭제
- `glUseProgram`을 다른 프로그램으로 바꿔도 **이전 프로그램의 uniform은 그대로 살아있다**.
- ⚠️ uniform을 set하려면 그 시점에 해당 프로그램이 `glUseProgram`된 상태여야 한다 (DSA의 `glProgramUniform*` 사용 시 예외).

#### (A)와 (B)의 분업

| 질문 | 답하는 곳 |
|------|-----------|
| 샘플러는 **몇 번 유닛**을 보러 가는가? | (B) 프로그램 객체에 저장된 uniform 정수값 |
| 그 유닛에는 **어떤 텍스처**가 박혀 있는가? | (A) 컨텍스트 전역 슬롯 |

-> **샘플러는 "어떤 텍스처를 볼지"가 아니라 "몇 번 유닛을 볼지"를 저장한다.**

---

> ### 📄 2. 가장 흔한 함정 — `glUniform1i`에 텍스처 ID 넣기

```cpp
// ❌ 잘못된 코드
glUniform1i(... "material.diffuse",  m_material.diffuseTexture);  // textures[1]
glUniform1i(... "material.specular", m_material.specularTexture); // textures[2]
```

`glGenTextures`가 반환한 핸들(예: 2, 3)을 샘플러 uniform에 넣으면:
- `material.diffuse` ← **2번 유닛**을 보러 감 -> 그런데 거기엔 specular가 박혀 있음
- `material.specular` ← **3번 유닛**을 보러 감 -> 거기엔 아무것도 없음 (기본 black)

```cpp
// ✅ 올바른 코드
glUniform1i(glGetUniformLocation(prog, "material.diffuse"),  1); // GL_TEXTURE1
glUniform1i(glGetUniformLocation(prog, "material.specular"), 2); // GL_TEXTURE2
```

**규칙**: 샘플러 uniform에는 **유닛 인덱스(0, 1, 2…)** 만 넣는다. 텍스처 객체 ID 절대 금지.

---

> ### 📄 3. 세 가지 위치별 분석

#### (1) `startup()`에 두는 경우

| 콜 | 두어도 되는가? | 비고 |
|----|----------------|------|
| `glUniform1i` (샘플러->유닛) | ✅ **이상적** | 프로그램에 영구 저장 |
| `glBindTexture` (유닛->텍스처) | ⚠️ **조건부** | 외부 침범에 취약 |

`glBindTexture` 를 startup 에 두려면 그 유닛에 항상 같은 텍스처가 유지되고, 다른 패스/라이브러리가 절대 침범하지 않아야 한다 — 실제 프로젝트에선 **거의 불가능**.

**올바른 startup 패턴**
```cpp
glUseProgram(shader_programs[1]);   // ← 반드시 먼저!
glUniform1i(glGetUniformLocation(shader_programs[1], "material.diffuse"),  1);
glUniform1i(glGetUniformLocation(shader_programs[1], "material.specular"), 2);
glUseProgram(0);                     // (선택) 정리
```
`glUseProgram` 없이 호출하면 현재 활성 프로그램에 잘못 set되거나 무시된다.

#### (2) `render()` 의 루프 바깥 (프레임당 1회) — **가장 균형 좋은 위치**

| 콜 | 두어도 되는가? | 비고 |
|----|----------------|------|
| `glBindTexture` | ✅ **권장** | 프레임마다 재바인딩 = 방어적 (defensive rebinding) |
| `glUniform1i` | ⚠️ 가능하지만 **불필요** | startup에서 이미 했으면 중복 |

state leak에 강하고, 박스 36정점 × 10개 = 360정점 그릴 동안 1회 바인딩.

```cpp
glUseProgram(shader_programs[1]);
// ... projection/view/light uniform 세팅 ...
glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, textures[1]);
glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, textures[2]);
glBindVertexArray(VAOs[1]);
for (int i = 0; i < boxPositions.size(); i++) {
    glUniformMatrix4fv(uloc_model, 1, GL_FALSE, model);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}
```

#### (3) `glDrawArrays` 직전마다 — 안티패턴

- 모든 박스가 같은 텍스처를 쓰는데 매번 4개 GL 콜 + 2개 `glGetUniformLocation` -> **순수 낭비**.
- `glGetUniformLocation`은 **문자열 해시 검색**이라 특히 비싸다. **위치는 미리 캐싱하는 게 정석.**
- 정당화되는 유일한 경우: 박스마다 텍스처가 다른 경우(인스턴스마다 material 다름).

---

> ### 📄 4. 모범 배치 — 결정표

박스 10개가 모두 같은 diffuse/specular를 쓰는 경우:

| 콜 | 권장 위치 | 이유 |
|----|-----------|------|
| `glUniform1i(sampler, unit)` × 2 | **startup** (단, `glUseProgram` 후) | 프로그램 로컬, 영구 저장 |
| `glGetUniformLocation`(model, light, …) | **startup**에서 캐싱 | 문자열 검색 비용 회피 |
| `glActiveTexture` + `glBindTexture` × 2 | **render의 루프 바깥**, 셰이더 전환 직후 | 프레임당 1회 = state leak 방어 + 낭비 없음 |
| `glUniformMatrix4fv("model", …)` | **루프 안** | 박스마다 다름 |

---

> ### 📄 5. 일반 법칙

> **위치 결정의 유일한 기준은 "무엇이 객체별로 바뀌는가" 이다.**
> 안 바뀌는 것은 가장 바깥으로 끌어올리고, 바뀌는 것만 안쪽에 둔다.

```
프로그램 생성 1회 (startup)
└─ 샘플러 uniform location 캐싱
└─ 샘플러 uniform 값 (sampler -> unit)         ← 영구 (B)

프레임당 1회 (render 진입부)
└─ 텍스처 유닛 바인딩 (unit -> texture object)  ← 방어적 (A)
└─ 카메라/라이트 uniform (view, projection, light.*)

객체당 1회 (draw 직전)
└─ model 행렬 uniform
└─ glDrawArrays / glDrawElements
```

---

> ### 📄 6. 빠른 체크리스트

- [ ] `glUniform1i`의 두 번째 인자가 **유닛 인덱스(0,1,2…)** 인가? 텍스처 ID 아닌가?
- [ ] `glUniform1i` 호출 시점에 해당 프로그램이 `glUseProgram` 되어 있는가?
- [ ] `glGetUniformLocation`은 startup에서 캐싱했는가?
- [ ] `glActiveTexture` 후 반드시 `glBindTexture`가 따라오는가?
- [ ] 한 프레임에 여러 셰이더 패스가 같은 유닛을 건드리는가? (그러면 패스마다 재바인딩 필요)

## 관련 노트
- 텍스처 슬롯이 *컨텍스트 전역*, 샘플러 uniform이 *program-local* 인 더 큰 상태 스코프 그림: `01_GettingStarted/01_OpenGL_상태머신.md`
- Material 구조체에서 핸들 vs 유닛 번호를 분리·일원화하는 방식: `02_Lighting/02_Materials_LightingMaps.md`
