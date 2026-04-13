# Chapter6 Extra 1 — 실수 & 주의사항 학습 노트

## Priority 1: GPU에 데이터가 아예 안 올라가는 치명적 실수

### 1-1. `sizeof(vector.size())` ≠ 데이터 크기

```cpp
// ❌ sizeof(size_t) = 8바이트만 업로드
glBufferData(GL_ARRAY_BUFFER, sizeof(mModelData.size()), ...);

// ✅ 전체 데이터 크기
glBufferData(GL_ARRAY_BUFFER, mModelData.size() * sizeof(GLfloat), ...);
```

**핵심**: `sizeof()`는 타입의 바이트 수를 반환한다. `size()`의 반환값이 아니라 `size_t` 타입 자체의 크기(8바이트)가 나온다.

**판별법**: C 배열은 `sizeof(arr)`로 전체 크기를 얻을 수 있지만, `std::vector`는 반드시 `size() * sizeof(element)`를 써야 한다.

---

### 1-2. 셰이더에서 position 미적용

```glsl
// ❌ mat4를 vec4에 대입 — 정점 위치가 무시됨
vec4 modeled = modelMat;

// ✅ 행렬 × 정점 곱셈
vec4 modeled = modelMat * position;
```


**핵심**: GLSL에서 `mat4`를 `vec4`에 대입하면 첫 번째 열벡터만 들어간다. `position`을 곱하지 않으면 모든 정점이 같은 위치가 된다.

---

### 1-3. 잘못된 VAO 바인딩

```cpp
// ❌ my_application::vaoAddr = 0 (초기값, 빈 VAO)
glBindVertexArray(vaoAddr);

// ✅ 실제 메쉬 데이터가 있는 Model의 VAO
glBindVertexArray(models.back()->GetVaoAddr());
```

**핵심**: OpenGL 리소스(VAO/VBO)는 생성한 객체가 소유한다. application에 별도 핸들을 두면 Model이 가진 핸들과 혼동된다.

**원칙**: 리소스 생성과 사용의 소유권을 한 곳에서 관리할 것.

---

## Priority 2: 렌더링은 되지만 결과가 엉뚱한 실수

### 2-1. 인터리브 데이터 레이아웃 불일치

```cpp
// ❌ 컴포넌트 단위 교차 → [vx, cx, vy, cy, vz, cz, vw, cw]
for(int i = 0; i < 4; i++) {
    data.push_back(vertex[i]);
    data.push_back(color[i]);
}

// ✅ vec4 단위 연속 → [vx, vy, vz, vw, cx, cy, cz, cw]
for(int i = 0; i < 4; i++) data.push_back(vertex[i]);
for(int i = 0; i < 4; i++) data.push_back(color[i]);
```

**핵심**: `glVertexAttribPointer`는 연속된 N개 float를 하나의 attribute로 읽는다.
데이터가 `[vx,cx,vy,cy...]`로 교차되어 있으면 position으로 `(vx,cx,vy,cy)`를 읽게 된다.

**원칙**: GPU 레이아웃과 CPU 데이터 배치 순서가 정확히 일치해야 한다.

---

### 2-2. attribute offset 계산 오류

```cpp
// ❌ float 1개(4바이트) 건너뜀
glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)sizeof(float));

// ✅ vec4 1개(16바이트) 건너뜀
glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
```

**핵심**: offset은 "이전 attribute가 차지하는 총 바이트 수"이다.
`vec4` = 4 × `sizeof(float)` = 16바이트.

---

### 2-3. 큐브 정점 좌표 오류

```cpp
// ❌ [1][1][0]과 [1][1][1]의 좌표값 오타
vmath::vec4(0.0, 0.0, 1.0, 1.0),  // [1][1][0] — y가 0 (1이어야 함)
vmath::vec4(0.0, 1.0, 1.0, 1.0),  // [1][1][1] — x가 0 (1이어야 함)

// ✅ 인덱스 [z][y][x] → 좌표 (x, y, z) 규칙 준수
vmath::vec4(0.0, 1.0, 1.0, 1.0),  // [1][1][0] = (0, 1, 1)
vmath::vec4(1.0, 1.0, 1.0, 1.0),  // [1][1][1] = (1, 1, 1)
```

**핵심**: 3차원 배열 인덱싱은 실수하기 매우 쉽다. 모든 정점에 주석으로 좌표를 명시하고, `인덱스 → 좌표` 매핑 규칙을 먼저 정의할 것.

---

### 2-4. 면 구성 시 다른 평면의 정점 혼합

```cpp
// ❌ "앞면 (z=0)"인데 z=1 정점([1][...][...])을 섞어 사용
pushVertex(mBaseVertices[0][0][0], ...);  // z=0 ✓
pushVertex(mBaseVertices[1][1][0], ...);  // z=1 ✗

// ✅ z=0 면은 [0][?][?] 정점만 사용
pushVertex(mBaseVertices[0][0][0], ...);  // z=0 ✓
pushVertex(mBaseVertices[0][1][0], ...);  // z=0 ✓
```

**원칙**: 각 면은 해당 평면에 속하는 정점만 사용 + CCW winding으로 법선이 바깥을 향하도록 구성.

---

## Priority 3: 설계 수준의 구조적 실수

### 3-1. 하나의 mat4에 T/R/S를 부분 덮어쓰기

```cpp
// ❌ 같은 행렬의 서로 다른 영역을 독립적으로 수정
void SetRotation(vec3 euler) {
    // 3x3 회전부를 덮어쓰기
    for(i=0..2) for(j=0..2) matrix[i][j] = rotMatrix[i][j];
}
void SetPosition(vec3 t) {
    // 4열 이동부를 덮어쓰기
    matrix[4][0] = t[0];  // ← 인덱스 4는 범위 밖 (UB)
}

// ✅ TRS를 독립 저장, 필요 시 합성
vmath::vec3 mPosition, mEulerAngles, mScale;  // 독립 저장

vmath::mat4 GetModelMatrix() const {
    return translate(mPosition) * rotY * rotX * rotZ * scale(mScale);
}
```

**문제점 3가지**:
1. `matrix[4][...]` — `mat4`는 인덱스 0~3, 4는 UB (undefined behavior)
2. 회전 합성에 `+=` 사용 — 회전 행렬은 곱셈으로 합성해야 함
3. T와 R을 같은 행렬에서 독립 수정하면 합성 순서(T×R×S)가 보장 안 됨

**게임엔진 원칙**: Position, Rotation, Scale을 별도 값으로 저장하고, `GetModelMatrix()`에서 T × R × S 순서로 합성한다.

---

### 3-2. Camera의 eye/target 직접 저장 vs Transformer 일관성

```cpp
// ❌ Camera만 eye/target 별도 저장, ITransformable 구현은 빈 껍데기
void SetRotation(vec3 euler) override { }  // 아무것도 안 함
void Rotate(float, vec3) override { }      // 아무것도 안 함

// ✅ Model과 동일하게 Transformer에 위임
//    position = eye, rotation → forward 방향 계산 → target 자동 도출
vmath::mat4 GetViewMatrix() const {
    vec3 forward = Ry * Rx * (0, 0, -1);  // 회전에서 전방 벡터 계산
    vec3 target = position + forward;
    return lookat(position, target, worldUp);
}
```

**핵심**: ITransformable을 구현하는 모든 객체는 동일한 변환 체계를 가져야 한다.
Camera도 `SetPosition` → eye, `SetRotation` → 시선 방향으로 일관되게 매핑.

---

## Priority 4: 애니메이션/로직 실수

### 4-1. 누적(`Translate`) vs 절대값(`SetPosition`) 혼동

```cpp
// ❌ Translate는 매 프레임 누적 → 위치가 폭주
void testCameraRotate(double t) {
    camera.Translate(vec3(angle, 0, sin(t)));  // 매 프레임 += 
}

// ✅ 원운동은 절대 위치 지정
void testCameraRotate(double t) {
    camera.SetPosition(vec3(cos(t)*R, Y, sin(t)*R));  // 매 프레임 = 
    camera.LookAt(vec3(0, 0, 0));  // 시선도 매 프레임 갱신
}
```

**판별 기준**:
- 원운동, 왕복운동 → `SetPosition` (매 프레임 절대 좌표 계산)
- 키보드 이동, 물리 시뮬레이션 → `Translate` (delta 누적)

---

### 4-2. LookAt 1회성 호출

```cpp
// ❌ startup에서만 호출 → 카메라가 이동해도 시선 고정
void startup() {
    camera.LookAt(vec3(0, 0, 0));
}

// ✅ 카메라 위치가 변할 때마다 LookAt 갱신
void testCameraRotate(double t) {
    camera.SetPosition(...);
    camera.LookAt(vec3(0, 0, 0));  // 매 프레임 시선 재계산
}
```

---

### 4-3. 애니메이션 함수 미호출

```cpp
// ❌ 함수를 만들어놓고 render()에서 호출 안 함
void render(double currentTime) {
    // testModelRotate(currentTime);  ← 빠져있음
    // testCameraRotate(currentTime); ← 빠져있음
    glDrawArrays(...);
}
```

**원칙**: 새 함수를 작성하면 반드시 호출부도 함께 추가할 것.

---

### 4-4. degree 값을 좌표로 사용

```cpp
// ❌ angle은 degree(114°, 228°...) — 좌표로 쓰면 의미 없음
camera.Translate(vec3(angle, 0, sin(t)));

// ✅ 좌표에는 cos/sin, 회전에는 degree
camera.SetPosition(vec3(cos(t) * radius, height, sin(t) * radius));
```

**핵심**: degree(각도)와 position(좌표)은 단위가 다르다. 혼용하면 안 된다.

---

## 체크리스트 (코딩 전 확인)

| # | 항목 | 확인 |
|---|------|------|
| 1 | `glBufferData` 크기 인자가 `sizeof()`가 아니라 `size() * sizeof(element)`인가? | |
| 2 | 셰이더에서 `uniform * position` 곱셈을 빠뜨리지 않았나? | |
| 3 | `glBindVertexArray`에 올바른 VAO를 넘기고 있나? | |
| 4 | CPU 데이터 배치와 `glVertexAttribPointer` offset/stride가 일치하나? | |
| 5 | 큐브 정점 좌표가 인덱스 매핑 규칙과 일치하나? | |
| 6 | 각 면이 같은 평면의 정점만 사용하고 CCW winding인가? | |
| 7 | 행렬 합성에 `+=` 대신 `*`를 쓰고 있나? | |
| 8 | `Translate`(누적) vs `SetPosition`(절대) 용도가 맞나? | |
| 9 | LookAt 등 시선 갱신이 매 프레임 호출되나? | |
| 10 | 새로 만든 함수를 render()에서 호출하고 있나? | |

---

## 부록 A: VAO / VBO / EBO의 역할과 관계

### 한 줄 요약

| 객체 | 정체 | 저장하는 것 | 비유 |
|------|------|-------------|------|
| **VBO** | GPU 메모리 버퍼 | 정점 데이터 (위치, 색상, 법선, UV...) | 엑셀 시트의 **데이터 행** |
| **EBO** | GPU 메모리 버퍼 | 인덱스 배열 (어떤 정점으로 삼각형을 만들지) | 데이터 행을 가리키는 **참조 목록** |
| **VAO** | 상태 스냅샷 | VBO 바인딩 + attribute 설정 + EBO 바인딩 | 엑셀 시트의 **열 서식 설정** |

### 생성 시점 (startup / 생성자)

```
1. glGenVertexArrays → VAO 생성
2. glBindVertexArray(vao) ← 이후 설정이 이 VAO에 기록됨
│
├─ 3. glGenBuffers → VBO 생성
├─ 4. glBindBuffer(GL_ARRAY_BUFFER, vbo)
├─ 5. glBufferData(GL_ARRAY_BUFFER, ...) ← 정점 데이터 업로드
│
├─ 6. glVertexAttribPointer(0, ...) ← "attribute 0은 이 VBO에서 이렇게 읽어라"
├─ 7. glEnableVertexAttribArray(0)
├─ 8. glVertexAttribPointer(1, ...) ← "attribute 1은 이 VBO에서 이렇게 읽어라"
├─ 9. glEnableVertexAttribArray(1)
│
├─ (선택) 10. glGenBuffers → EBO 생성
├─ (선택) 11. glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo)
├─ (선택) 12. glBufferData(GL_ELEMENT_ARRAY_BUFFER, ...) ← 인덱스 업로드
│
13. glBindVertexArray(0) ← 기록 종료
```

**핵심**: 3~12번의 모든 설정이 VAO에 기록된다.

### 렌더링 시점 (render)

```cpp
glBindVertexArray(vao);  // VAO 하나만 바인딩하면 VBO + EBO + attribute 전부 복원

// EBO 없는 경우 (현재 코드 방식)
glDrawArrays(GL_TRIANGLES, 0, 36);       // VBO에서 순서대로 36개 정점 사용

// EBO 있는 경우 (인덱스 방식)
glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);  // EBO 인덱스 참조
```

### EBO 유무에 따른 비교

#### EBO 없이 (glDrawArrays) — 현재 코드 방식

```
VBO 데이터: [v0, v1, v2, v3, v4, v5, v0, v2, v3, ...]  ← 정점 중복 발생
                 △1         △2         △3

glDrawArrays(GL_TRIANGLES, 0, 36);
→ VBO 인덱스 0부터 순서대로 3개씩 묶어서 삼각형
```

| 장점 | 단점 |
|------|------|
| 구조 단순 | 정점 중복 (큐브: 36개 필요, 고유 정점은 8~24개) |
| 코드 짧음 | 메모리 낭비 |

#### EBO 사용 (glDrawElements)

```
VBO 데이터: [v0, v1, v2, v3, v4, v5, v6, v7]  ← 고유 정점만
EBO 데이터: [0,3,2, 0,2,1, 5,6,7, 5,7,4, ...]  ← 삼각형 조합

glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
→ EBO에서 인덱스 읽기 → 해당 VBO 정점으로 삼각형 구성
```

| 장점 | 단점 |
|------|------|
| 정점 재사용으로 메모리 절약 | EBO 추가 관리 필요 |
| GPU 캐시 효율 (동일 정점 재계산 안 함) | 면별 색상/법선 시 정점 분리 필요 (8→24개) |

### 큐브 기준 메모리 비교

| 방식 | 정점 수 | float 수 (pos+color) | 인덱스 | 총 데이터 |
|------|---------|---------------------|--------|-----------|
| DrawArrays (현재) | 36 | 36 × 8 = 288 | 없음 | 288 float |
| 24 정점 + EBO | 24 | 24 × 8 = 192 | 36 uint | 192 float + 36 uint |
| 8 정점 + EBO | 8 | 8 × 8 = 64 | 36 uint | 64 float + 36 uint |

### VAO가 기록하는 것 / 기록하지 않는 것

| VAO에 기록됨 ✅ | VAO에 기록 안 됨 ❌ |
|-----------------|-------------------|
| GL_ARRAY_BUFFER 바인딩 (각 attribute별) | GL_ARRAY_BUFFER의 현재 전역 바인딩 |
| GL_ELEMENT_ARRAY_BUFFER 바인딩 | uniform 값 |
| glVertexAttribPointer 설정 | shader program |
| glEnableVertexAttribArray 상태 | glEnable(GL_CULL_FACE) 등 전역 상태 |

**결론**: `render()` 시점에 `glBindVertexArray(vao)` 한 번이면 정점 데이터 접근에 필요한 모든 것이 복원된다. VBO/EBO를 다시 바인딩할 필요 없다.

---

# Chapter7 — 실수 & 주의사항 학습 노트

> Plane 한 장을 그리는 ModelBase / ProgramBase / Camera 구조에서 발견한 실수들

## Priority 1: 실행 자체가 안 되는 치명적 실수

### 1-1. 베이스 클래스 생성자에서 가상함수 호출 → `Pure virtual function called!`

```cpp
// ❌ 베이스 생성자가 build()를 호출, build()가 순수가상 initModelData()를 호출
class ModelBase {
    virtual void initModelData() = 0;
    virtual void build() {
        glGenVertexArrays(1, &mVAOAddr);
        glBindVertexArray(mVAOAddr);
        initModelData();   // ← 베이스 생성자 시점에는 베이스 vtable이 디스패치됨 → abort
    }
public:
    ModelBase() { build(); }   // ← 여기가 문제
};

// ✅ 방법 A: 파생 생성자에서 명시적으로 build() 호출
class ModelBase {
protected:
    virtual void initModelData() = 0;
    void build() { /* ... */ initModelData(); }
public:
    ModelBase() { /* build() 호출하지 않음 */ }
};
class PlaneModel : public ModelBase {
public:
    PlaneModel() : ModelBase() { build(); }   // ← 파생 vtable이 완성된 후 호출
};

// ✅ 방법 B: 정적 팩토리 + private 생성자
static std::unique_ptr<PlaneModel> Create() {
    auto p = std::unique_ptr<PlaneModel>(new PlaneModel());
    p->build();
    return p;
}
```

**핵심**: C++에서 베이스 클래스 생성자/소멸자가 실행되는 동안 vtable은 **베이스 클래스의 것**으로 고정된다. 파생 클래스의 오버라이드는 디스패치되지 않으며, 순수 가상함수면 즉시 abort된다.

**원칙**: **생성자/소멸자에서 가상함수를 호출하지 말 것.** 두 단계 초기화(2-phase init) 또는 정적 팩토리를 사용한다.

---

### 1-2. 멤버 변수 미초기화 → 가비지 행렬

```cpp
// ❌ TRS 벡터를 초기화하지 않음 → GetModelMatrix()가 가비지 값으로 곱셈
class ModelBase {
    vmath::vec4 mTranslateVec;     // ← 미초기화
    vmath::vec4 mEulerRotateVec;   // ← 미초기화
    vmath::vec4 mScaleVec;         // ← 미초기화 (특히 scale=0이면 모델이 사라짐)
    vmath::vec4 mOffset;
public:
    ModelBase(vmath::vec4 _offset = vmath::vec4(0,0,0,0)) {
        build();   // _offset도 mOffset에 대입 안 함
    }
};

// ✅ 멤버 초기화 리스트로 명시
ModelBase::ModelBase(vmath::vec4 _offset)
    : mOffset(_offset),
      mTranslateVec(0, 0, 0, 0),
      mEulerRotateVec(0, 0, 0, 0),
      mScaleVec(1, 1, 1, 0)   // ← scale은 1이 기본값
{ }
```

**핵심**: C++의 비-POD 멤버는 기본 생성자가 호출되지만, vmath의 vec/mat은 초기화되지 않을 수 있다. **항상 명시적으로 초기화**한다. 특히 scale=0이면 모델이 한 점으로 찌부러진다.

**판별법**: 화면이 검은색이면 (1) 그리지 않음 (2) scale=0 (3) 카메라 밖 — 세 가지 의심.

---

### 1-3. `GL_TEXTURE_BINDING_2D` ≠ `GL_TEXTURE_2D`

```cpp
// ❌ GL_TEXTURE_BINDING_2D는 glGetIntegerv 쿼리용 enum
glBindTexture(GL_TEXTURE_BINDING_2D, mTexAddr);

// ✅ 실제 바인딩 타겟
glBindTexture(GL_TEXTURE_2D, mTexAddr);
```

**핵심**: OpenGL enum 중 `GL_*_BINDING_*` 형식은 **현재 무엇이 바인딩되어 있는지 쿼리할 때** 쓰는 상수이지, 바인딩 함수의 인자가 아니다. IDE 자동완성에 속지 말 것.

**판별법**: `glBind*` 류 함수의 첫 인자에 `BINDING`이 들어가 있으면 거의 항상 잘못된 사용이다.

---

### 1-4. EBO `glBufferData`에서 `sizeof(GLfloat)` 사용

```cpp
// ❌ mElementBuffer는 std::vector<GLuint>인데 sizeof(GLfloat) 사용
mElementBuffer = {0, 1, 2, 0, 2, 3};
glBufferData(GL_ELEMENT_ARRAY_BUFFER,
             mElementBuffer.size() * sizeof(GLfloat),   // ← 우연히 둘 다 4바이트라 통과될 수 있음
             mElementBuffer.data(), GL_STATIC_DRAW);

// ✅ 컨테이너의 실제 원소 타입 사용
glBufferData(GL_ELEMENT_ARRAY_BUFFER,
             mElementBuffer.size() * sizeof(GLuint),
             mElementBuffer.data(), GL_STATIC_DRAW);
```

**핵심**: `GLfloat`와 `GLuint`가 둘 다 4바이트라 데스크탑에서는 우연히 동작할 수 있지만, **타입 불일치는 잠재적 버그**다. 나중에 `GLushort` 인덱스로 바꾸면 즉시 깨진다.

**원칙**: `sizeof(decltype(vec)::value_type)` 또는 명시적 타입을 사용한다.

---

### 1-5. `<_abort.h>` 같은 시스템 내부 헤더 include

```cpp
// ❌ 자동완성/잘못된 IDE 추천으로 들어온 내부 헤더
#include <_abort.h>

// ✅ 표준 헤더
#include <cstdlib>   // abort, exit
```

**핵심**: 헤더 이름이 `_`로 시작하면 시스템/구현 내부용이므로 직접 include 금지. 이식성이 깨지고 다른 플랫폼에서 컴파일 안 된다.

---

## Priority 2: 화면에 아무것도 안 그려지는 실수

### 2-1. `glDrawElements` / `glDrawArrays` 호출 누락

```cpp
// ❌ uniform만 set하고 draw call이 없음
void render(double currentTime) {
    for (const auto& prog : programs) {
        prog->UseProgram();
        for (const auto& model : prog->GetModels())
            glUniformMatrix4fv(modelLoc, 1, false, model->GetModelMatrix());
        // ← glDrawElements / glDrawArrays 없음 → 아무것도 안 그려짐
    }
}

// ✅ VAO 바인딩 + draw call까지 한 세트
void render(double currentTime) {
    for (const auto& prog : programs) {
        prog->UseProgram();
        glUniformMatrix4fv(viewLoc, 1, false, camera.GetViewMatrix());
        glUniformMatrix4fv(projLoc, 1, false, camera.GetProjectionMatrix(w, h));
        for (const auto& model : prog->GetModels()) {
            glUniformMatrix4fv(modelLoc, 1, false, model->GetModelMatrix());
            glBindVertexArray(model->GetVertexArrayObject());
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }
}
```

**핵심**: 렌더 한 프레임의 최소 단위는 **(program 사용) → (uniform set) → (VAO 바인딩) → (draw call)**. 어느 하나라도 빠지면 화면에 안 나온다.

**체크리스트**:
- [ ] `glUseProgram` 호출했나?
- [ ] `glBindVertexArray` 호출했나?
- [ ] `glDrawElements` 또는 `glDrawArrays` 호출했나?

---

### 2-2. depth buffer clear 누락

```cpp
// ❌ color만 clear → depth는 이전 프레임 값이 남아 새 도형이 가려짐
const GLfloat backgroundColor[4] = {0, 0, 0, 1};
glClearBufferfv(GL_COLOR, 0, backgroundColor);

// ✅ depth도 함께 clear
glClearBufferfv(GL_COLOR, 0, backgroundColor);
const GLfloat one = 1.0f;
glClearBufferfv(GL_DEPTH, 0, &one);
```

**핵심**: depth test가 활성화되어 있고 depth를 clear하지 않으면, **이전 프레임의 depth 값** 때문에 현재 프레임의 모든 픽셀이 depth test에서 떨어진다.

---

### 2-3. `glEnable(GL_DEPTH_TEST)` 누락

```cpp
// ❌ depth test 활성화 안 함 → 뒤에 있는 도형이 앞 도형을 덮어씀
void startup() override {
    programs.push_back(std::make_unique<Program::ProgramBase>());
    /* ... */
}

// ✅ startup에서 한 번 enable
void startup() override {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    // (선택) glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    /* ... */
}
```

**핵심**: depth test는 **기본적으로 비활성화**되어 있다. 3D 장면에서는 반드시 켜야 한다.

---

### 2-4. 루프 변수 무시 (`programs.back()` 오타)

```cpp
// ❌ for-each 변수 prog를 무시하고 programs.back() 호출
for (const auto& prog : programs) {
    programs.back()->UseProgram();   // ← 항상 마지막 program만 사용됨
    GLuint progAddr = prog->GetProgramAddress();
    // ...
}

// ✅ 루프 변수 그대로 사용
for (const auto& prog : programs) {
    prog->UseProgram();
    GLuint progAddr = prog->GetProgramAddress();
    // ...
}
```

**핵심**: 복사-붙여넣기 후 변수명을 안 바꾸면 발생. 컴파일러가 잡아주지 않는다.

**원칙**: for-each 안에서 컨테이너에 직접 접근(`vec.back()`, `vec[i]`)하는 코드를 보면 의심한다.

---

## Priority 3: 카메라 / 행렬 수학 실수

### 3-1. View 행렬 이중 변환

```cpp
// ❌ lookat이 이미 view 행렬인데 또 translate를 곱함
vmath::mat4 GetViewMatrix() const {
    return GetModelMatrix() * vmath::lookat(mEye, mTarget, mWorldUp);
}

// ✅ lookat 결과 그 자체가 view 행렬
vmath::mat4 GetViewMatrix() const {
    return vmath::lookat(mEye, mTarget, mWorldUp);
}
```

**핵심**: `lookat(eye, target, up)`은 이미 **월드 → 카메라 좌표계 변환 행렬(view matrix)**을 반환한다. 추가로 카메라의 위치/회전을 곱하면 변환이 두 번 적용된다.

**원칙**: View 행렬은 "카메라가 월드 원점에 있는 것처럼" 만드는 역변환이다. 카메라 위치는 `lookat`의 첫 인자(`eye`)에 이미 들어 있다.

---

### 3-2. Projection 행렬에 View를 곱해서 반환

```cpp
// ❌ projection이라면서 view × perspective를 반환
vmath::mat4 GetProjectionMatrix(int w, int h) const {
    return GetViewMatrix() * vmath::perspective(mFov, /*...*/);
}

// ✅ projection만 반환
vmath::mat4 GetProjectionMatrix(int w, int h) const {
    return vmath::perspective(mFov, (float)w / h, mNearPlane, mFarPlane);
}
```

**핵심**: MVP 분리 원칙. 셰이더에서 `gl_Position = proj * view * model * pos`로 곱해야 한다. CPU에서 미리 합쳐 보내면 셰이더의 계산 순서가 이상해지고 디버깅이 어렵다.

**원칙**: 함수명이 `GetXMatrix`면 X 그 자체만 반환한다.

---

### 3-3. aspect ratio 오타: `height / height`

```cpp
// ❌ window_height / window_height = 1.0 (정사각형 전제)
vmath::perspective(mFov, (float)window_height / window_height, mNearPlane, mFarPlane);

// ✅ width / height
vmath::perspective(mFov, (float)window_width / window_height, mNearPlane, mFarPlane);
```

**핵심**: 자동완성으로 변수명 입력 시 매우 흔한 실수. 정사각형 윈도우에서는 우연히 동작해서 발견이 늦어진다.

**판별법**: 윈도우 크기를 바꿨을 때 도형이 가로/세로 비율이 깨지면 의심한다.

---

### 3-4. 모델 행렬 곱 순서: `S × R × T` (잘못)

```cpp
// ❌ Translate가 먼저 적용되어 회전이 오프셋도 함께 회전시킴
vmath::mat4 GetModelMatrix() {
    return identity * scaleMat * xRot * yRot * zRot * translateMat;
}

// ✅ 표준: T × R × S (정점 v 입장에서 S → R → T 순으로 적용됨)
vmath::mat4 GetModelMatrix() {
    return translateMat * (xRot * yRot * zRot) * scaleMat;
}
```

**핵심**: column-major(OpenGL 기본)에서는 `M × v`로 곱하므로, 행렬은 **오른쪽이 먼저** 적용된다. `T × R × S × v`는 v를 먼저 scale → rotate → translate한다.

**잘못된 순서의 결과**: translate가 먼저 적용된 후 회전이 오면, 회전의 중심이 원점이 아니라 변환된 위치 기준이 되어 모델이 큰 원을 그리며 움직인다.

**원칙**: 특수한 이유(궤도 운동 등)가 없으면 항상 **T × R × S** 순서.

---

## Priority 4: 도형 / 코드 정합성 실수

### 4-1. 정점 D가 C와 동일 → 사각형이 삼각형 둘로 겹침

```cpp
// ❌ C와 D가 동일 정점 [1][1]
// C
for (int i = 0; i < 4; i++) mBufferObject.push_back(mCubeVertices[1][1][i]);  // (1,1,0)
// D
for (int i = 0; i < 4; i++) mBufferObject.push_back(mCubeVertices[1][1][i]);  // (1,1,0) ← 중복

// ✅ D는 [1][0] (좌상단)
// D
for (int i = 0; i < 4; i++) mBufferObject.push_back(mCubeVertices[1][0][i]);  // (0,1,0)
```

**핵심**: A=(0,0), B=(1,0), C=(1,1), D=(0,1) — 시계 반대 방향으로 사각형 4개 정점.
복사-붙여넣기 후 인덱스를 안 바꾸면 발생.

**원칙**: Chapter6 노트의 "각 정점에 좌표 주석 명시" 규칙을 항상 따른다.

---

### 4-2. 모델 루프 3중 분리 (uniform별로 따로 순회)

```cpp
// ❌ 같은 모델을 3번 순회
for (const auto& model : prog->GetModels())
    glUniformMatrix4fv(modelLoc, 1, false, model->GetModelMatrix());
for (const auto& model : prog->GetModels())
    glUniformMatrix4fv(viewLoc, 1, false, camera.GetViewMatrix());
for (const auto& model : prog->GetModels())
    glUniformMatrix4fv(projLoc, 1, false, camera.GetProjectionMatrix(w, h));

// ✅ view/proj는 모델 무관 → 루프 밖, model만 루프 안
glUniformMatrix4fv(viewLoc, 1, false, camera.GetViewMatrix());
glUniformMatrix4fv(projLoc, 1, false, camera.GetProjectionMatrix(w, h));
for (const auto& model : prog->GetModels()) {
    glUniformMatrix4fv(modelLoc, 1, false, model->GetModelMatrix());
    glBindVertexArray(model->GetVertexArrayObject());
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
```

**핵심**: 루프 분리는 (1) 성능 낭비 (2) draw call이 빠진 것을 가리는 위장 — 두 가지 문제가 동시에 발생한다.

**원칙**: **모델별로 변하는 것 vs 변하지 않는 것**을 구분한다. 변하지 않는 것(view, proj, light)은 루프 밖에서 한 번만 set한다.

---

### 4-3. `glLinkProgram` 후 link 상태 검사 누락

```cpp
// ❌ 링크 실패해도 무음
glLinkProgram(mProgramAddr);
glDeleteShader(vsAddr);
glDeleteShader(fsAddr);

// ✅ 링크 상태 확인 + 로그 출력
glLinkProgram(mProgramAddr);
GLint linkStatus = 0;
glGetProgramiv(mProgramAddr, GL_LINK_STATUS, &linkStatus);
if (linkStatus == GL_FALSE) {
    GLint logLen = 0;
    glGetProgramiv(mProgramAddr, GL_INFO_LOG_LENGTH, &logLen);
    std::vector<char> log(logLen);
    glGetProgramInfoLog(mProgramAddr, logLen, nullptr, log.data());
    std::cerr << "Program link failed:\n" << log.data() << std::endl;
    abort();
}
glDeleteShader(vsAddr);
glDeleteShader(fsAddr);
```

**핵심**: 셰이더 컴파일이 성공해도 링크는 실패할 수 있다(varying 불일치, in/out 타입 불일치 등). 검사하지 않으면 까만 화면만 보고 원인을 못 찾는다.

**원칙**: GPU 리소스 생성/컴파일/링크 후에는 **항상** 상태를 검사한다.

---

## 체크리스트 (Chapter7)

| # | 항목 | 확인 |
|---|------|------|
| 1 | 베이스 클래스 생성자에서 가상함수를 호출하지 않는가? | |
| 2 | 모든 멤버 변수(특히 scale)가 초기화되었는가? | |
| 3 | `glBindTexture`에 `GL_TEXTURE_2D`를 넘기는가? (`GL_TEXTURE_BINDING_2D` 아님) | |
| 4 | `glBufferData` 크기 계산에 컨테이너의 실제 원소 타입을 쓰는가? | |
| 5 | `<_xxx.h>` 같은 시스템 내부 헤더를 include하지 않는가? | |
| 6 | render()에 `glDrawElements` / `glDrawArrays` 호출이 있는가? | |
| 7 | depth buffer를 매 프레임 clear하는가? | |
| 8 | startup에서 `glEnable(GL_DEPTH_TEST)` 호출했는가? | |
| 9 | for-each 안에서 컨테이너에 직접 접근(`vec.back()`)하지 않는가? | |
| 10 | View 행렬이 `lookat` 결과 그 자체인가? (이중 변환 없음) | |
| 11 | Projection 함수가 perspective만 반환하는가? (view 곱 없음) | |
| 12 | aspect ratio가 `width / height`인가? (`height / height` 아님) | |
| 13 | 모델 행렬 합성 순서가 `T × R × S`인가? | |
| 14 | 사각형/큐브 정점 인덱스가 복사 후에도 정확한가? | |
| 15 | 모델 무관 uniform(view, proj)을 루프 밖에서 set하는가? | |
| 16 | `glLinkProgram` 후 link status를 검사하는가? | |

---

## 패턴: CPU 데이터 채우기 vs GPU 업로드 분리

### 핵심 원칙

**CPU 데이터 작성과 GPU 업로드는 독립적**이다. 둘을 한 함수에 섞으면 책임이 흐려지고, 모델별 `Build()`가 모두 똑같은 boilerplate가 되어 중복이 늘어난다.

```cpp
// ❌ 한 함수에 CPU 데이터 작성과 GPU 업로드가 뒤섞임
void PlaneModel::initModelData() {
    glGenBuffers(1, &mVBOAddr);                  // GPU
    glBindBuffer(GL_ARRAY_BUFFER, mVBOAddr);     // GPU
    pushVertex(0, {0, 0}, {0.0, 0.0});           // CPU
    pushVertex(1, {1, 0}, {1.0, 0.0});           // CPU
    pushVertex(1, {1, 1}, {1.0, 1.0});           // CPU
    pushVertex(1, {0, 1}, {0.0, 1.0});           // CPU
    glBufferData(GL_ARRAY_BUFFER, ...);          // GPU
    mElementBuffer = {0, 1, 2, 0, 2, 3};         // CPU (GPU 사이에 끼어있음)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ...);  // GPU
    glVertexAttribPointer(0, ...);               // GPU
    // ...
}

// ✅ 도형 정의(CPU) ↔ GPU 업로드(boilerplate) 분리
void PlaneModel::initModelData() {
    // CPU only — GL 호출 0개. "이 도형은 어떤 정점/인덱스로 이루어지는가"만 정의
    pushVertex(0, {0, 0}, {0.0, 0.0});
    pushVertex(1, {1, 0}, {1.0, 0.0});
    pushVertex(1, {1, 1}, {1.0, 1.0});
    pushVertex(1, {0, 1}, {0.0, 1.0});
    mElementBuffer = {0, 1, 2, 0, 2, 3};
}

ModelBase& PlaneModel::Build() {
    if (mIsBuilted) return *this;

    initModelData();   // 1) CPU 데이터 준비

    // 2) VAO 생성/바인딩 — 이후 호출이 VAO에 기록됨
    glGenVertexArrays(1, &mVAOAddr);
    glBindVertexArray(mVAOAddr);

    // 3) VBO 생성/바인딩/업로드
    glGenBuffers(1, &mVBOAddr);
    glBindBuffer(GL_ARRAY_BUFFER, mVBOAddr);
    glBufferData(GL_ARRAY_BUFFER,
                 mBufferObject.size() * sizeof(GLfloat),
                 mBufferObject.data(), GL_STATIC_DRAW);

    // 4) EBO 생성/바인딩/업로드 (VAO 바인딩 중이라 VAO에 EBO도 기록됨)
    glGenBuffers(1, &mEBOAddr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBOAddr);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mElementBuffer.size() * sizeof(GLuint),
                 mElementBuffer.data(), GL_STATIC_DRAW);

    // 5) attribute layout (VAO에 기록)
    GLuint stride = 10 * sizeof(GLfloat);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void*)(0));
    glEnableVertexAttribArray(0);
    // ... 나머지 attribute ...

    mIsBuilted = true;
    return *this;
}
```

### 의존성 표

| 작업 | 종류 | GL 상태 의존? |
|------|------|------------|
| `pushVertex()` (vector에 push) | CPU | ❌ 없음 |
| `mElementBuffer = {...}` | CPU | ❌ 없음 |
| `glGenBuffers` | GPU | ❌ 이름만 생성 |
| `glBindBuffer` | GPU | 컨텍스트만 필요 |
| `glBufferData(GL_ARRAY_BUFFER)` | GPU | ✅ **VBO 바인딩** + CPU 데이터 준비 |
| `glBufferData(GL_ELEMENT_ARRAY_BUFFER)` | GPU | ✅ **VAO 바인딩** (EBO 기록 위해) |
| `glVertexAttribPointer` | GPU | ✅ **VAO + VBO 둘 다 바인딩** |

### 절대 어기면 안 되는 순서 (Build 내부)

```
glBindVertexArray(VAO)                                ← 가장 먼저
  ├─ glBindBuffer(GL_ARRAY_BUFFER, VBO)
  │     ├─ glBufferData(GL_ARRAY_BUFFER, ...)         ← VBO 바인딩 후 + CPU 데이터 준비 후
  │     └─ glVertexAttribPointer(...)                  ← VBO 바인딩 후 + VAO 바인딩 중
  └─ glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO)       ← VAO 바인딩 중이어야 VAO에 EBO 기록
        └─ glBufferData(GL_ELEMENT_ARRAY_BUFFER, ...)
```

CPU 부분(`pushVertex`, `mElementBuffer = {...}`)은 이 순서 안의 어디든 끼어들 수 있지만, **각 `glBufferData` 호출 전까지 해당 데이터가 준비되어 있어야** 한다.

### 이 분리가 가져오는 이점

| # | 이점 | 설명 |
|---|------|------|
| 1 | **책임 분리(SRP)** | `initModelData()` = "도형 정의 (CPU)", `Build()` = "GPU 업로드 (boilerplate)" |
| 2 | **base 클래스 통합 가능** | 모든 모델의 `Build()`가 동일해지므로 `ModelBase::Build()`로 끌어올려 중복 제거 가능 (`initModelData()`만 순수가상으로 남김) |
| 3 | **테스트 용이** | CPU 데이터 부분만 단위 테스트 가능 (GL 컨텍스트 불필요) |
| 4 | **디버깅 용이** | GL 호출과 데이터 작성이 섞이지 않아 실패 지점 추적이 쉬움 |

### 발전형: base 클래스로 Build() 끌어올리기

```cpp
class ModelBase {
protected:
    virtual void initModelData() = 0;   // ← 도형마다 다른 부분만 순수가상

public:
    // base에서 한 번만 정의 — 더 이상 virtual 아님
    ModelBase& Build() {
        if (mIsBuilted) return *this;
        initModelData();                          // 파생이 mBufferObject/mElementBuffer 채움
        glGenVertexArrays(1, &mVAOAddr);
        glBindVertexArray(mVAOAddr);
        glGenBuffers(1, &mVBOAddr);
        glBindBuffer(GL_ARRAY_BUFFER, mVBOAddr);
        glBufferData(GL_ARRAY_BUFFER, mBufferObject.size() * sizeof(GLfloat),
                     mBufferObject.data(), GL_STATIC_DRAW);
        // ... EBO + attribute layout ...
        mIsBuilted = true;
        return *this;
    }
};
```

이렇게 하면 `CubeModel`, `SphereModel` 추가 시 **`initModelData()` 하나만 작성**하면 된다. attribute layout(stride/offset)이 모델마다 다르면 그 부분도 가상화할 수 있다.

---

## 핵심 교훈 요약

1. **C++ 생성자/소멸자에서 가상함수 호출 금지** — 두 단계 초기화 패턴 사용
2. **OpenGL 상태는 매 프레임 명시적으로 설정** — depth test, depth clear 누락이 잦음
3. **MVP 행렬은 분리해서 저장** — `GetXMatrix()`는 X만 반환
4. **자동완성/복붙 후 반드시 변수명 검토** — `height/height`, `programs.back()`, 정점 인덱스 중복
5. **GPU 리소스 생성 후 항상 상태 검사** — 컴파일/링크/바인딩 실패는 무음으로 까만 화면이 됨
6. **CPU 데이터 작성과 GPU 업로드 분리** — `initModelData()`는 도형 정의 전용, `Build()`는 GPU 업로드 boilerplate
