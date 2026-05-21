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
// ❌ 컴포넌트 단위 교차 -> [vx, cx, vy, cy, vz, cz, vw, cw]
for(int i = 0; i < 4; i++) {
    data.push_back(vertex[i]);
    data.push_back(color[i]);
}

// ✅ vec4 단위 연속 -> [vx, vy, vz, vw, cx, cy, cz, cw]
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

// ✅ 인덱스 [z][y][x] -> 좌표 (x, y, z) 규칙 준수
vmath::vec4(0.0, 1.0, 1.0, 1.0),  // [1][1][0] = (0, 1, 1)
vmath::vec4(1.0, 1.0, 1.0, 1.0),  // [1][1][1] = (1, 1, 1)
```

**핵심**: 3차원 배열 인덱싱은 실수하기 매우 쉽다. 모든 정점에 주석으로 좌표를 명시하고, `인덱스 -> 좌표` 매핑 규칙을 먼저 정의할 것.

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
//    position = eye, rotation -> forward 방향 계산 -> target 자동 도출
vmath::mat4 GetViewMatrix() const {
    vec3 forward = Ry * Rx * (0, 0, -1);  // 회전에서 전방 벡터 계산
    vec3 target = position + forward;
    return lookat(position, target, worldUp);
}
```

**핵심**: ITransformable을 구현하는 모든 객체는 동일한 변환 체계를 가져야 한다.
Camera도 `SetPosition` -> eye, `SetRotation` -> 시선 방향으로 일관되게 매핑.

---

## Priority 4: 애니메이션/로직 실수

### 4-1. 누적(`Translate`) vs 절대값(`SetPosition`) 혼동

```cpp
// ❌ Translate는 매 프레임 누적 -> 위치가 폭주
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
- 원운동, 왕복운동 -> `SetPosition` (매 프레임 절대 좌표 계산)
- 키보드 이동, 물리 시뮬레이션 -> `Translate` (delta 누적)

---

### 4-2. LookAt 1회성 호출

```cpp
// ❌ startup에서만 호출 -> 카메라가 이동해도 시선 고정
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
1. glGenVertexArrays -> VAO 생성
2. glBindVertexArray(vao) ← 이후 설정이 이 VAO에 기록됨
│
├─ 3. glGenBuffers -> VBO 생성
├─ 4. glBindBuffer(GL_ARRAY_BUFFER, vbo)
├─ 5. glBufferData(GL_ARRAY_BUFFER, ...) ← 정점 데이터 업로드
│
├─ 6. glVertexAttribPointer(0, ...) ← "attribute 0은 이 VBO에서 이렇게 읽어라"
├─ 7. glEnableVertexAttribArray(0)
├─ 8. glVertexAttribPointer(1, ...) ← "attribute 1은 이 VBO에서 이렇게 읽어라"
├─ 9. glEnableVertexAttribArray(1)
│
├─ (선택) 10. glGenBuffers -> EBO 생성
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
-> VBO 인덱스 0부터 순서대로 3개씩 묶어서 삼각형
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
-> EBO에서 인덱스 읽기 -> 해당 VBO 정점으로 삼각형 구성
```

| 장점 | 단점 |
|------|------|
| 정점 재사용으로 메모리 절약 | EBO 추가 관리 필요 |
| GPU 캐시 효율 (동일 정점 재계산 안 함) | 면별 색상/법선 시 정점 분리 필요 (8->24개) |

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

### 1-1. 베이스 클래스 생성자에서 가상함수 호출 -> `Pure virtual function called!`

```cpp
// ❌ 베이스 생성자가 build()를 호출, build()가 순수가상 initModelData()를 호출
class ModelBase {
    virtual void initModelData() = 0;
    virtual void build() {
        glGenVertexArrays(1, &mVAOAddr);
        glBindVertexArray(mVAOAddr);
        initModelData();   // ← 베이스 생성자 시점에는 베이스 vtable이 디스패치됨 -> abort
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

### 1-2. 멤버 변수 미초기화 -> 가비지 행렬

```cpp
// ❌ TRS 벡터를 초기화하지 않음 -> GetModelMatrix()가 가비지 값으로 곱셈
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
        // ← glDrawElements / glDrawArrays 없음 -> 아무것도 안 그려짐
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

**핵심**: 렌더 한 프레임의 최소 단위는 **(program 사용) -> (uniform set) -> (VAO 바인딩) -> (draw call)**. 어느 하나라도 빠지면 화면에 안 나온다.

**체크리스트**:
- [ ] `glUseProgram` 호출했나?
- [ ] `glBindVertexArray` 호출했나?
- [ ] `glDrawElements` 또는 `glDrawArrays` 호출했나?

---

### 2-2. depth buffer clear 누락

```cpp
// ❌ color만 clear -> depth는 이전 프레임 값이 남아 새 도형이 가려짐
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
// ❌ depth test 활성화 안 함 -> 뒤에 있는 도형이 앞 도형을 덮어씀
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

**핵심**: `lookat(eye, target, up)`은 이미 **월드 -> 카메라 좌표계 변환 행렬(view matrix)**을 반환한다. 추가로 카메라의 위치/회전을 곱하면 변환이 두 번 적용된다.

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

// ✅ 표준: T × R × S (정점 v 입장에서 S -> R -> T 순으로 적용됨)
vmath::mat4 GetModelMatrix() {
    return translateMat * (xRot * yRot * zRot) * scaleMat;
}
```

**핵심**: column-major(OpenGL 기본)에서는 `M × v`로 곱하므로, 행렬은 **오른쪽이 먼저** 적용된다. `T × R × S × v`는 v를 먼저 scale -> rotate -> translate한다.

**잘못된 순서의 결과**: translate가 먼저 적용된 후 회전이 오면, 회전의 중심이 원점이 아니라 변환된 위치 기준이 되어 모델이 큰 원을 그리며 움직인다.

**원칙**: 특수한 이유(궤도 운동 등)가 없으면 항상 **T × R × S** 순서.

---

## Priority 4: 도형 / 코드 정합성 실수

### 4-1. 정점 D가 C와 동일 -> 사각형이 삼각형 둘로 겹침

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

// ✅ view/proj는 모델 무관 -> 루프 밖, model만 루프 안
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

---

# Exercise6 — 텍스처 큐브 실수 & 주의사항 학습 노트

> 면별 숫자 텍스처가 붙은 회전 큐브를 만드는 과정에서 발견한 실수들.
> `ModelBase` / `ProgramBase` + 2개 sampler (`tex1` 고정, `tex2` 면별 교체) 구조.

## 🚨 재발한 실수 (최우선 주의)

이전 챕터 노트에서 이미 경고한 내용인데 **같은 실수를 똑같이 반복**했다. 이 세 가지는 머리에 각인시킬 것.

---

### 🔴 R-1. [REPEATED] `mScale` 초기화 누락 -> 모델이 한 점으로 찌부러짐

**이전 경고**: Chapter7 Priority 1-2 — "scale=0이면 모델이 한 점으로 찌부러진다"
**이번 재현**: [apps/exercise6/main.cpp](apps/exercise6/main.cpp) 의 `ModelBase` 에 `vec3 mScale;` 로만 선언, 초기화 없음.

```cpp
// ❌ 이번에 반복한 실수
class ModelBase {
public:
    vec3 mTranslate;   // 기본값 (0,0,0)
    vec3 mEulerRot;    // 기본값 (0,0,0)
    vec3 mScale;       // 기본값 (0,0,0) ← 문제!
};

// GetModelMatrix() 안에서:
vmath::scale<float>(mScale);   // scale(0,0,0) = 0 행렬
// -> 모든 정점이 원점으로 찌그러짐 -> 1픽셀 도트 -> 시각적으로 "안 보임"

// ✅ 기본값 명시
class ModelBase {
public:
    vec3 mTranslate = vec3(0.0f, 0.0f, 0.0f);
    vec3 mEulerRot  = vec3(0.0f, 0.0f, 0.0f);
    vec3 mScale     = vec3(1.0f, 1.0f, 1.0f);   // ← scale은 반드시 1
};
```

**증상**: 렌더가 되긴 되는데 화면에 "아무것도 안 보임". 쉐이더 문제, VAO 문제 등 다른 원인을 먼저 의심하게 돼서 디버깅이 길어졌다.

**판별법**: 화면이 검정일 때 Chapter7 노트의 3가지 의심 중 **scale=0** 을 가장 먼저 확인한다.

**교훈**: 클래스 멤버 선언 시 `=` 로 기본값을 바로 적는 습관. C++11 이후 in-class member initializer 는 디폴트 생성자에서 무조건 적용된다.

---

### 🔴 R-2. [REPEATED] EBO `glBufferData` 에 vertex 데이터 업로드

**이전 경고**: Chapter7 Priority 1-4 — "mElementBuffer는 vector<GLuint>인데 sizeof(GLfloat) 사용"
**이번 재현**: 타입 불일치를 넘어서 **아예 vertex 데이터 자체**를 EBO 에 업로드.

```cpp
// ❌ 이번에 반복한 실수 (Chapter7 경고의 "상위호환")
glBufferData(GL_ELEMENT_ARRAY_BUFFER,
             mBufferData.size() * sizeof(GLfloat),  // ← vertices 크기 × float
             mBufferData.data(),                     // ← vertices 포인터
             GL_STATIC_DRAW);

// ✅ EBO 에는 mElementData (인덱스) 가 들어가야 함
glBufferData(GL_ELEMENT_ARRAY_BUFFER,
             mElementData.size() * sizeof(GLuint),   // ← indices × GLuint
             mElementData.data(),                     // ← indices 포인터
             GL_STATIC_DRAW);
```

**증상**: `glDrawElements` 가 float bit pattern 을 GLuint 인덱스로 해석 -> 엄청나게 큰 값 -> VBO 범위 밖 접근 -> **화면에 아무것도 안 나옴**. GL 에러도 조용함.

**왜 반복했나**: Chapter7 노트는 `sizeof(GLfloat)` vs `sizeof(GLuint)` 에 초점이 있었는데, 이번엔 **완전히 다른 vector (mBufferData)** 를 넘긴 수준이라 "같은 실수" 라고 인지하지 못했다. 본질은 같다: **EBO 에는 인덱스를, VBO 에는 vertex 를** 넣어야 한다.

**교훈**: `glBufferData` 호출마다 "target + size + pointer" 세 개가 일관되게 맞는지 한 번 더 확인. 변수명에 `Buffer` 가 붙어 있어도 vertex 용인지 element 용인지 명확히 구분해서 쓸 것.

---

### 🔴 R-3. [REPEATED] Attribute offset 계산 오류

**이전 경고**: Chapter6 Priority 2-2 — "offset은 이전 attribute가 차지하는 총 바이트 수"
**이번 재현**: UV attribute 의 offset 을 "color 시작 위치(16)" 로 계산.

```cpp
// ❌ 이번에 반복한 실수
void *coffset  = (void *)(VERTEX_POSITION_SIZE * sizeof(GLfloat));  // 4*4 = 16 ✓
void *uvoffset = (void *)(VERTEX_COLOR_SIZE    * sizeof(GLfloat));  // 4*4 = 16 ❌

// ✅ UV 는 pos(4) + color(4) = 8 float 뒤에 시작
void *uvoffset = (void *)((VERTEX_POSITION_SIZE + VERTEX_COLOR_SIZE) * sizeof(GLfloat));  // 8*4 = 32
```

또한 `glVertexAttribPointer` 의 **size 인자도 틀렸다**:

```cpp
// ❌ UV 는 vec2 인데 size=4 로 읽음
glVertexAttribPointer(2, 4, GL_FLOAT, false, stride, uvoffset);

// ✅
glVertexAttribPointer(2, VERTEX_UV_SIZE, GL_FLOAT, false, stride, uvoffset);  // size=2
```

**증상**: UV attribute 가 color 데이터를 읽음. 면 전체가 "색상값 = UV 좌표" 로 단일 점 샘플링. FS 가 색상만 쓸 때는 안 보이고, 텍스처를 붙이려는 순간 드러난다.

**왜 반복했나**: Chapter6 노트는 `sizeof(float)` 을 `4 * sizeof(float)` 로 고치는 단순 사례였다. 이번엔 "`VERTEX_COLOR_SIZE` 라는 상수를 쓴 것" 자체는 올바른 패턴처럼 보여서 오류를 놓쳤다. 본질: **offset 은 "내 attribute 이전까지의 누적" 이어야 한다** — color 에 `VERTEX_POSITION_SIZE`, UV 에 `VERTEX_POSITION_SIZE + VERTEX_COLOR_SIZE`.

**교훈**: offset 상수를 만들 때 "cumulative" 라는 개념을 코드에서 드러내면 좋다:

```cpp
constexpr int POS_OFFSET   = 0;
constexpr int COLOR_OFFSET = POS_OFFSET + VERTEX_POSITION_SIZE;   // 4
constexpr int UV_OFFSET    = COLOR_OFFSET + VERTEX_COLOR_SIZE;    // 8
constexpr int VERTEX_LEN   = UV_OFFSET + VERTEX_UV_SIZE;          // 10
// -> glVertexAttribPointer(..., (void*)(UV_OFFSET * sizeof(GLfloat)));
```

---

## Priority 1: 렌더가 아예 안 되는 치명적 실수

### 1-1. Fragment Shader 의 interface block 을 `out` 으로 선언

```glsl
// ❌ FS 에서 VS 로부터 값을 받아야 하는데 out 으로 선언
out VS_OUT {
    vec4 vsColor;
    vec2 vsTexCoord;
} fs_in;

void main() {
    colors = fs_in.vsColor;  // ← 출력 블록을 읽음 = undefined
}

// ✅ FS 는 in, VS 는 out
in VS_OUT {
    vec4 vsColor;
    vec2 vsTexCoord;
} fs_in;
```

**핵심**: interface block 키워드는 stage 방향을 나타낸다. VS 에서 `out VS_OUT`, FS 에서 `in VS_OUT` — 블록 이름(`VS_OUT`)은 같아야 link 되지만, 인스턴스명(`vs_out` / `fs_in`)은 달라도 된다.

**증상**: FS 가 "출력 블록" 을 선언한 꼴이 돼서 `fs_in.vsColor` 를 읽으면 초기화 안 된 output 을 읽는 셈. 대부분 드라이버는 0 을 반환 -> 모든 픽셀 검정. BG 도 검정이면 "아무것도 안 보임".

**판별법**: FS 에서 블록을 읽고 있는데 결과가 검정이라면 `in`/`out` 키워드부터 확인.

---

### 1-2. GL 객체를 "값 멤버" 로 보유 -> 생성자 순서 함정

```cpp
// ❌ ProgramBase 가 glCreateProgram 을 호출하는데, 이게 MyApplication 의 값 멤버
class MyApplication : public sb7::application {
    ProgramBase program;   // ← MyApplication 생성 시 default-construct
    // ↓
    // 1. new MyApplication() 실행 -> member "program" default construct
    // 2. ProgramBase() -> glCreateProgram() 호출
    // 3. 하지만 이 시점엔 아직 GLFW 초기화 전 -> GL 컨텍스트 없음
    // 4. glCreateProgram 은 함수 포인터 변수 (gl3w 로더) 인데 아직 load 안 됨
    // 5. null pointer dereference -> SEGV
};

// ✅ unique_ptr 로 지연 생성
class MyApplication : public sb7::application {
    std::unique_ptr<ProgramBase> program;   // 기본값 nullptr, GL 호출 없음

    virtual void startup() override {
        // 여기선 GL 컨텍스트가 이미 준비됨
        program = std::make_unique<ProgramBase>();
    }
};
```

**핵심**: sb7 의 실행 흐름은 `main() -> new MyApplication -> run() -> startup() -> render()` 순. **멤버 객체의 생성자는 `new MyApplication` 시점** 에 실행되므로 `startup()` 이전이다. GL 호출이 있는 생성자는 이 시점에 부를 수 없다.

**원칙**: GL 리소스를 다루는 객체는 **`unique_ptr` 로 감싸서 `startup()` 안에서 생성**. Chapter7 / exercise6 에서 쓰는 표준 패턴.

---

### 1-3. `vector<ModelBase>` 에 값 타입 push -> GL 핸들 dangling

```cpp
// ❌ ModelBase 가 소멸자에서 glDelete* 를 호출하는데 복사 금지 선언이 없음
class ModelBase {
    GLuint mVAOAddr, mVBOAddr, mEBOAddr;
    ~ModelBase() {
        glDeleteBuffers(1, &mEBOAddr);
        glDeleteBuffers(1, &mVBOAddr);
        glDeleteVertexArrays(1, &mVAOAddr);
    }
};

vector<ModelBase> models;
auto model = ModelBase();
model.Build(vertices);       // VAO=7, VBO=3, EBO=4 할당
models.push_back(model);     // 암묵적 복사 — models[0]: VAO=7, VBO=3, EBO=4
// startup() 끝 -> 지역 model 소멸 -> VAO 7 파괴
// render() 에서 models[0].Draw() -> glBindVertexArray(7) = 이미 파괴된 핸들 -> 드로우 실패

// ✅ unique_ptr 로 감싸기 (가장 깨끗)
vector<unique_ptr<ModelBase>> models;
auto model = std::make_unique<ModelBase>();
model->Build(vertices);
models.push_back(std::move(model));   // unique_ptr 이동, ModelBase 자체는 heap 에 고정
```

**핵심**: GL 핸들은 **파일 디스크립터 같은 독점 자원**. 암묵적 복사로 복사본이 만들어지면 둘이 같은 핸들을 공유하고, 먼저 소멸하는 쪽이 그 핸들을 파괴하면 나머지 쪽은 dangling.

**세 가지 해결책**:
1. **`vector<unique_ptr<T>>`** — 포인터만 이동, 실체는 heap 고정 (권장, Chapter7 표준)
2. **Move-only 로 만들기** — `T(const T&) = delete;` + 명시적 move ctor 정의
3. **Deep copy** — 복사 시 새 GL 오브젝트 생성 + 데이터 재업로드 (비용 큼, 의미론 혼란)

**원칙**: GL 리소스 소유 클래스는 복사 의미론을 명시적으로 정의하지 않는 한 **복사되면 안 된다**. 개발 초기부터 `= delete` 로 막거나 unique_ptr 로 감싼다.

---

### 1-4. 빈 `std::vector` 에 `operator[]` 접근 -> UB -> SEGV

```cpp
// ❌ AddTexture 를 호출하지 않은 상태에서 Draw 진입
class ModelBase {
    vector<GLuint> mTextureAddrs;   // 비어있음

    void Draw() {
        glBindTexture(GL_TEXTURE_2D, mTextureAddrs[0]);   // ← empty[0] = UB
    }
};

// ✅ 계약을 명확히 하거나 방어 코드 추가
void Draw() {
    if (mTextureAddrs.empty()) return;   // fail-safe
    glBindTexture(GL_TEXTURE_2D, mTextureAddrs[0]);

    for (int f = 0; f < 6; f++) {
        if (1 + f >= (int)mTextureAddrs.size()) break;
        glBindTexture(GL_TEXTURE_2D, mTextureAddrs[1 + f]);
        // draw face f
    }
}
```

**핵심**: `std::vector::operator[]` 는 **bounds check 가 없다**. 빈 vector 를 `[0]` 으로 접근하면 정의되지 않은 메모리를 읽는 것. macOS debug 빌드에선 즉시 SEGV.

**이번 경로**: `exercise_6` (underscore) 에서 `exercise6` 으로 파일을 분기할 때 `startup()` 의 `AddTexture` 호출 7개가 따라오지 않음 -> `mTextureAddrs` 비어있음 -> Draw 에서 SEGV.

**원칙**: `Draw` 와 `AddTexture` 사이의 "이만큼 호출해야 한다" 라는 암묵적 계약은 **호출자 한 곳만 실수해도 크래시**로 이어진다. 방어 코드를 넣거나 `Build()` 안에서 텍스처 슬롯을 강제 할당하도록 API 를 바꾼다.

---

### 1-5. GLSL 에서 bool 에 bitwise OR (`|`) 사용 -> 셰이더 컴파일 실패

```glsl
// ❌ GLSL 에서 | 는 정수 비트연산자 — bool 에 쓸 수 없음
if (tex1.x < 0.9 | tex1.y < 0.9 | tex1.z < 0.9) { /* ... */ }

// ✅ 논리 OR 는 ||
if (tex1.x < 0.9 || tex1.y < 0.9 || tex1.z < 0.9) { /* ... */ }
```

**핵심**: C/C++ 과 달리 GLSL 은 엄격하다. `|` 는 **정수 비트연산 전용**, bool 에는 `||` 만 허용.

**발생 체인**:
1. FS 컴파일 실패 -> `sb7::shader::load` 가 0 반환
2. `createShader` 가 `check_errors=false` 라 **조용히** 0 핸들 반환
3. `glAttachShader(prog, 0)` -> 링크 실패 (역시 조용히)
4. `glUseProgram(prog)` -> invalid program 활성화
5. 모든 draw call drop -> 화면에 아무것도 없음

**이번 함정**: 같은 FS 파일에서 텍스처 샘플링 블록을 주석 처리했다가 **나중에 다시 주석 해제** 하면서 `|` 버그가 부활. 해당 블록이 주석 처리됐을 때는 컴파일러가 그 줄을 보지 않아 정상 동작하던 것. "이전에 되던 코드를 되살렸는데 안 됨" 상황 = 주석 영역 안의 구문 오류를 의심.

**원칙**: **개발 중에는 `check_errors=true`**. `sb7::shader::load` 반환값이 0 이면 최소한 stderr 로 뿜어주는 체크 한 줄 필수.

```cpp
GLuint createShader(GLenum shader_type, const char *shader_path) {
    GLuint shaderAddr = sb7::shader::load(shader_path, shader_type, true);  // ← true
    if (shaderAddr == 0) {
        std::cerr << "shader load fail: " << shader_path << std::endl;
        std::exit(1);
    }
    return shaderAddr;
}
```

---

## Priority 2: 렌더는 되지만 결과가 엉뚱한 실수

### 2-1. Texture 교체 루프 바깥에 draw call

```cpp
// ❌ 루프 안에서 6번 텍스처 교체 -> 루프 밖에서 1번 draw
for (int f = 0; f < 6; f++) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, faceTextures[f]);
}
glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
// -> 마지막에 바인딩된 6번째 텍스처로 36정점 전부 그려짐

// ✅ 각 면을 자기 텍스처와 묶어서 draw 를 루프 안으로
for (int f = 0; f < 6; f++) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, faceTextures[f]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT,
                   (void*)(f * 6 * sizeof(GLuint)));   // 면당 6 인덱스
}
```

**핵심**: OpenGL 은 **draw call 시점** 의 상태로 그린다. 상태를 루프에서 바꿔도 draw 가 한 번만 나가면 최종 상태 하나로만 그려진다.

**원칙**: 상태가 면/모델마다 다르면 draw 도 그만큼 분할. 같은 VBO 안에서 부분만 그리려면 `glDrawElements` 의 **4번째 인자 (indices offset)** 를 활용.

---

### 2-2. Face index 의 winding 일관성 결여 -> 텍스처 X축 mirror

```cpp
// ❌ 각 면이 제각각의 "시작 코너" 에서 시작 -> UV 가 어떤 면은 정상, 어떤 면은 mirror
static const std::vector<GLuint> CUBE_FACE_INDICES[6] = {
    {0, 1, 5, 0, 5, 4},  // -Z (BR 시작)
    {1, 2, 6, 1, 6, 5},  // +X (BR 시작)
    {2, 3, 7, 2, 7, 6},  // +Z (BR 시작) ← 기본 시점에서 x축 mirror 보임
    // ...
};

// ✅ 모든 면이 "외부 시점에서 CCW, BL 시작" 으로 통일
static const std::vector<GLuint> CUBE_FACE_INDICES[6] = {
    {1, 0, 4, 1, 4, 5},  // -Z (BL=1)
    {2, 1, 5, 2, 5, 6},  // +X (BL=2)
    {3, 2, 6, 3, 6, 7},  // +Z (BL=3)
    {0, 3, 7, 0, 7, 4},  // -X (BL=0)
    {0, 1, 2, 0, 2, 3},  // -Y (BL=0)
    {7, 6, 5, 7, 5, 4},  // +Y (BL=7)
};
```

**핵심**: `uvIdx = {0, 1, 2, 0, 2, 3}` 와 `BASE_MESH_UVS = {BL, BR, TR, TL}` 을 결합하면 **면의 첫 슬롯 `[0]` 이 "BL 코너" 에 와야** 텍스처가 정방향으로 붙는다. 면마다 시작 코너가 다르면 텍스처가 rotate/mirror 된다.

**BL 판정법**:
- `view forward`: 그 면의 outward normal
- `view up`: +Y (±X, ±Z 면) 또는 ±Z (±Y 면)
- `view right = forward × up`
- BL = (min view-right, min view-up) 에 해당하는 월드 코너

**원칙**: 테이블 기반 데이터는 한 면을 설계하고 나머지를 "회전 패턴" 으로 확장하면 안 된다 — 시점이 다른 면은 UV 매핑이 달라져야 함을 잊기 쉽다. **각 면을 "외부에서 정면으로 본다는 가정" 으로 독립 판정**하는 게 안전.

---

### 2-3. Sampler uniform 에 엉뚱한 슬롯 연결

```cpp
// ❌ tex1 에 모든 면 공용 텍스처를 넣어놓고, FS 는 tex1 만 출력
model->AddTexture("side1.jpg");   // mTextureAddrs[0] = tex1 (모든 면 공용)
for (f = 0..5)
    model->AddTexture(side[f]);   // mTextureAddrs[1..6] = tex2 (면별 교체)

// FS:
colors = tex1;   // ← 모든 면이 side1.jpg 만 보임
```

**원인**: 두 개의 sampler (`tex1`/`tex2`) 가 서로 다른 의미(고정 vs 면별) 인데 FS 에서 **의도와 다른 쪽을 출력**. 쉐이더를 작성한 본인조차 `tex1` / `tex2` 중 어느 게 면별인지 헷갈림.

**원칙**: Sampler 이름을 역할로 지어라 (`maskTex`, `fillTex`, `staticTex`, `perFaceTex`). 혹은 주석으로 "tex1 = 고정, tex2 = per-face" 를 양쪽 코드에 달아둔다.

---

### 2-4. UV attribute size 4 로 읽기

-> R-3 참조 (재발한 실수).

---

## Priority 3: 설계 / 코드 품질 실수

### 3-1. 함수 파라미터 이름 누락

```cpp
// ❌ 이름 없이 타입만 — 본문에서 접근 불가
void AddTexture(const char* texture_name, const char* ) {
    // 두 번째 인자를 어떻게 쓰지?
    auto *data = stbi_load("wall.jpg", ...);  // ← 하드코딩으로 돌아감
}

// ✅
void AddTexture(const char* sampler_name, const char* image_path) {
    auto *data = stbi_load(image_path, ...);
}
```

**핵심**: C++ 에서 함수 파라미터는 이름을 생략하면 **"사용 안 할 값"** 이라는 선언이다(주로 오버라이드 시그니처 맞추기용). 실제로 본문에서 쓰려는 값에 이름을 빼먹으면 컴파일은 되지만 접근 방법이 없다.

**판별법**: 함수 시그니처에 `(Type, Type ...)` 처럼 이름 없는 파라미터가 있으면 **의도한 것인지 확인**. 대부분은 오타.

---

### 3-2. Namespace scope 에 orphan 문자열 리터럴

```cpp
// ❌ 변수 선언 없이 떠 있는 문자열들
namespace exercise6 {
    "./shaders/default_vs.glsl"
    "./shaders/default_fs.glsl"
    "modelMat"
    // ...
}
```

**어떻게 컴파일이 될까**: 문자열 리터럴은 C++ 표현식으로 유효하고, 표현식 statement 는 namespace 스코프에서 허용되지 않지만 일부 컴파일러가 관대하게 파싱해줄 수 있다. 하지만 **의도한 동작이 전혀 없다** — 변수로 저장되지 않아서 어디서도 참조 불가.

**원인**: "이 문자열들을 상수로 뽑을 예정" 인데 선언 키워드(`static const char*`) 를 빠뜨림.

**✅ 올바른 형태**:
```cpp
static const char *SHADER_VS_PATH = "./shaders/default_vs.glsl";
static const char *UNIFORM_MODEL_MAT = "modelMat";
static const char *TEXTURE_SIDES[6] = {
    "./textures/side1.jpg", /* ... */
};
```

**원칙**: 매직 문자열/숫자는 **선언 키워드 + 이름** 을 붙여 상수로 뽑는다. 리터럴만 남기는 형태는 없다.

---

### 3-3. Shader 컴파일 에러 무음

-> 1-5 참조. `createShader(..., true)` 로 `check_errors` 활성화.

---

### 3-4. Draw 와 AddTexture 간 암묵적 크기 계약

-> 1-4 참조. 빈 vector `[0]` 접근은 UB.

---

### 3-5. "복사-붙여넣기 후 부분 누락" 패턴

`exercise_6` -> `exercise6` 로 파일을 복제했을 때 **`startup()` 의 AddTexture 호출 7개가 따라오지 않아** 크래시. 같은 프로젝트의 Chapter6 노트 4-1 ("복사-붙여넣기 후 정점 인덱스 누락"), Chapter7 2-4 ("for-each 안 `programs.back()`") 와 **동일 계열 실수**.

**원칙**: 파일/블록을 복제할 때는 **diff** 를 먼저 떠서 양쪽이 정확히 무엇이 다른지 확인한다. "뭔가 복사했는데 실행이 안 된다" 면 가장 먼저 누락된 호출/선언을 찾는다.

---

## Priority 4: 수학 / 단위 혼동

### 4-1. `vmath::rotate` 는 degrees 를 받는다

```glsl
// vmath.h 안:
float rads = float(angle) * 0.0174532925f;  // π/180 — 내부에서 radians 변환
```

즉 **입력은 degrees**. `mEulerRot` 성분도 degrees 로 저장/주입해야 한다.

---

### 4-2. 회전 속도 계산에서 불필요한 이중 변환

```cpp
// ❌ radians() 와 180/π 가 서로 상쇄돼서 의도가 흐릿함
float angle = vmath::radians((currentTime * 180) / 3.14) * 20;
//          = currentTime * 57.32 * (π/180) * 20
//          ≈ currentTime * 1.0 * 20
//          = currentTime * 20   (여전히 degrees 로 해석됨)

// ✅ 초당 N도 회전이면 그냥 currentTime * N
float angle = static_cast<float>(currentTime) * 90.0f;   // 초당 90도
```

**핵심**: **단위 변환은 한 쪽에서 한 번만**. 호출 대상 함수(`vmath::rotate`)가 degrees 를 받으면 degrees 로, radians 를 받으면 radians 로 준비한다. "혹시 몰라서" 변환을 여러 번 거치면 코드가 의도와 다른 속도로 동작하면서도 컴파일/실행은 정상이라 디버깅이 어렵다.

**원칙**: 새 수학 함수 쓰기 전에 **단위 규약** 부터 확인(`vmath.h` 소스 또는 문서). 확인이 번거로우면 단위를 변수명에 넣는다: `float angleDeg`, `float angleRad`.

---

## 체크리스트 (Exercise6)

| # | 항목 | 확인 |
|---|------|------|
| 1 | 🔴 **`mScale` 등 TRS 멤버를 `(1,1,1)` / `(0,0,0)` 로 초기화**했는가? [R-1] | |
| 2 | 🔴 **EBO `glBufferData` 가 `mElementData` (인덱스) 를 사용**하는가? [R-2] | |
| 3 | 🔴 **각 attribute offset 이 "이전까지 누적"** 으로 계산됐는가? [R-3] | |
| 4 | FS interface block 이 `in VS_OUT` 인가? (`out` 아님) | |
| 5 | GL 리소스 소유 클래스가 `unique_ptr` 로 감싸져 있는가? | |
| 6 | `ProgramBase` / `ModelBase` 를 값 멤버로 두지 않았는가? | |
| 7 | `std::vector::operator[]` 접근 전 크기 검사를 했는가? | |
| 8 | GLSL 에서 bool 에 `|` 대신 `||` 를 쓰는가? | |
| 9 | `sb7::shader::load` 의 `check_errors` 가 `true` 인가? | |
| 10 | 텍스처 교체 루프 안에 `glDrawElements` 가 함께 있는가? | |
| 11 | CUBE_FACE_INDICES 의 `[0]` 슬롯이 외부 시점 BL 코너인가? | |
| 12 | Sampler uniform 이름이 역할(고정/per-face)을 반영하는가? | |
| 13 | `glVertexAttribPointer` 의 size 가 실제 컴포넌트 수(vec2=2, vec3=3, vec4=4) 인가? | |
| 14 | 함수 파라미터에 이름이 모두 있는가? | |
| 15 | 매직 문자열/숫자를 `static const` 로 뽑았는가? | |
| 16 | `vmath::rotate` 에 degrees 를 넘기는가? (불필요한 변환 없음) | |
| 17 | 코드 복제 시 누락된 호출이 없는지 diff 로 확인했는가? | |

---

## 핵심 교훈 요약 (Exercise6)

1. 🔴 **같은 실수를 또 하지 말 것** — `mScale=0`, EBO 에 vertex 업로드, attribute offset 누적 계산 세 가지는 이전 노트에 이미 경고한 내용인데 다시 발생했다. 코드 작성 전 **체크리스트 항목 1~3 을 의식적으로 읽는 습관**.
2. **GL 리소스는 값 타입으로 다루지 말 것** — `unique_ptr` 로 감싸서 소유권을 명시. 복사는 기본적으로 금지.
3. **생성자가 GL 을 부르는 객체는 지연 생성** — `startup()` 안에서 `make_unique` 로. 값 멤버는 컨텍스트 전에 초기화된다.
4. **Shader 에러는 기본적으로 무음** — `check_errors=true` 를 프로젝트 시작 시점부터 적용. 특히 주석 처리/해제 반복 중엔 언제든 에러가 숨어들 수 있다.
5. **상태를 바꾸는 루프에는 draw call 이 같이 들어가야 한다** — OpenGL 은 draw 시점의 상태 하나로만 그린다.
6. **단위는 한 번만 변환** — degrees/radians 혼용은 그 함수의 규약을 먼저 확인해서 해결.
7. **복사-붙여넣기는 diff 로 검증** — 블록/파일 복제 시 누락은 런타임 증상만 보고 원인 추적이 어렵다.

---

# Exercise6 — 파라메트릭 서피스 (Disk) 확장 노트

> 위의 Exercise6 섹션을 `BuildDisk` 를 추가하는 과정에서 발견한 새 실수들을 이어받아 확장.
> 파라메트릭 생성, 색 보간, 범용 Draw, glUniform 타입 매칭 등 **"렌더는 되는데 결과가 이상한"** 새로운 실수들이 등장.

## 🚨 또 재발한 실수 (Exercise6 원본 노트 참조)

### 🔴 R-4. [REPEATED] 빈/부족한 vector 에 `operator[]` 접근 -> UB

**이전 경고**: Exercise6 Priority 1-4 ("빈 vector operator[] -> UB -> SEGV")
**이번 재현**: Disk 쪽 작업 중, `Draw()` 가 **큐브 전용 하드코딩** 상태였던 탓에 `mTextureAddrs[1 + f]` (f=0..5) 로 접근. 그런데 Disk 는 `AddTexture(TEXTURE_CONTAINER)` 를 **한 번만** 호출해서 `size()==1` -> `[1]`, `[2]`, … 접근이 **out-of-bounds UB**.

```cpp
// ❌ Draw() 안
for (int f = 0; f < 6; f++)
    glBindTexture(GL_TEXTURE_2D, mTextureAddrs[1 + f]);  // size=1 인데 [1] 접근
```

**증상**: `glBindTexture` 가 쓰레기 texture name 을 받아 드라이버가 **`UNSUPPORTED: unit 1 GLD_TEXTURE_INDEX_2D is unloadable ... using zero texture`** 경고 발생. 렌더는 진행되지만 unit 1 은 zero texture 로 폴백.

**교훈**: "빈 vector `[0]`" 뿐 아니라 **"모자란 vector `[N]`"** 도 똑같은 UB. vector 인덱싱은 언제나 `size()` 로 가드.

---

## Priority 1: 치명적 실수

### 1-6. [NEW] `glUniform*` 함수와 쉐이더 타입 불일치 -> 조용히 거절 🔴

```cpp
// ❌ vec2 uniform 을 mat4 용 함수로 업로드
glUniformMatrix4fv(glGetUniformLocation(prog_addr, UNIFORM_UV_OFFSET),
                   1, false, mUVOffset);   // ← mUVOffset 은 vec2 (8 bytes)

// ✅ vec2 는 glUniform2fv
glUniform2fv(glGetUniformLocation(prog_addr, UNIFORM_UV_OFFSET), 1, mUVOffset);
```

**발생 체인**:
1. `glUniformMatrix4fv` 는 **16 float (64 bytes)** 을 읽으려고 함
2. `mUVOffset` 은 **vec2, 8 bytes** 뿐 -> 나머지 56 bytes 는 스택의 쓰레기 메모리
3. 드라이버는 "uniform 타입 vec2 인데 mat4 로 업로드? 타입 불일치" -> **`GL_INVALID_OPERATION` 조용히 반환**
4. 셰이더의 `uniform vec2 uvOffset` / `uvRatio` 가 **영원히 기본값 (0, 0)** 에 머무름

**2차 증상**:
- VS 에서 `vec2 rUv = vec2(uvCoords.x * uvRatio.x, uvCoords.y * uvRatio.y) = (0, 0)` — 모든 정점 UV 가 (0,0)
- FS 에서 `texture(tex1, (0, 0))` — 텍스처의 **단 한 픽셀만 샘플링** -> 디스크 전체가 **단색**
- 매 프레임 `mUVOffset = vec2(currentTime, 1.0)` 으로 바꿔도 uniform 업로드 자체가 실패 -> **애니메이션 무반응**

**이번 함정**: 행렬을 mat4 로 다루는 데 익숙해져서 "uniform 넘길 때는 `glUniformMatrix4fv` 쓰면 된다" 는 잘못된 패턴이 손에 배어버림. 실제로는 **uniform 함수가 쉐이더 타입과 1:1 매칭** 되어야 한다.

**OpenGL uniform 함수 매핑 표** (반드시 외울 것):

| 쉐이더 타입 | CPU 함수 | 주로 쓰는 데 |
|-----------|---------|-------------|
| `float`   | `glUniform1f` / `glUniform1fv`   | scalar |
| `vec2`    | `glUniform2f` / **`glUniform2fv`** | UV, 2D 위치 |
| `vec3`    | `glUniform3f` / `glUniform3fv`   | RGB, 3D 방향 |
| `vec4`    | `glUniform4f` / `glUniform4fv`   | RGBA, 4D 벡터, baseColor |
| `int`     | `glUniform1i`   | ⚠️ **sampler 도 int** (`sampler2D`/`samplerCube`) |
| `mat3`    | **`glUniformMatrix3fv`** | normal matrix |
| `mat4`    | **`glUniformMatrix4fv`** | model/view/proj |

**특히 주의**: `sampler2D` 는 정수 unit 번호로 지정. `glUniform1f` 로 소수점 넘기면 똑같이 조용히 거절. 반드시 `glUniform1i(loc, texture_unit_index)`.

**판별법**: "CPU 에선 uniform 을 매 프레임 바꾸는데 셰이더에 반영이 안 됨" -> 가장 먼저 `glUniform*` 함수 이름부터 확인. `glGetError()` 를 draw 직후에 한 번 호출해보는 것도 빠른 진단.

---

## Priority 2: 렌더는 되지만 결과가 엉뚱한 실수

### 2-5. [NEW] 파라메트릭 서피스의 누적 변수 스코프 오류 -> 아르키메데스 나선 🔴

```cpp
// ❌ currentRad 가 col 루프 안에서 누적 -> 정점마다 반지름 증가
double currentRad = vs;
double currentAngle = us;
for (int row = 0; row < numRows; row++) {
    for (int col = 0; col < numCols; col++) {
        currentRad += deltaRad;       // ← col 마다 누적
        currentAngle += deltaAngle;   // ← col 마다 누적
        // ... pos = (currentRad * cos, 0, -currentRad * sin) ...
    }
}
```

**추적**: `vRes=1, uRes=32` 에서 시작:

| col | currentRad | currentAngle |
|-----|-----------|--------------|
| 0   | 1         | 0.196        |
| 1   | 2         | 0.392        |
| 16  | 17        | 3.14         |
| 32  | 33        | 6.28         |

-> 반지름이 1 -> 33 으로 **선형 증가** 하면서 각도도 한 바퀴 회전 = **아르키메데스 나선**. 육안으로 "점점 커지는 칼날 형태" 로 보임.

또한 `currentRad`/`currentAngle` 이 **row 간에도 reset 되지 않음** -> row 0 끝 지점 값부터 이어서 계속 증가 -> row 0 과 row 1 이 전혀 다른 궤적.

```cpp
// ✅ 누적 대신 인덱스로 직접 계산
for (int row = 0; row < numRows; row++) {
    double currentRad = (vs + row * deltaRad) * radius;   // row 만 의존
    for (int col = 0; col < numCols; col++) {
        double currentAngle = us + col * deltaAngle;       // col 만 의존
        // ... 계산 ...
    }
}
```

**원칙**: **중첩 루프에서 누적 증분은 위험**. 각 축이 독립적이어야 하는 파라메트릭 표면에서는 `index × delta + start` **공식으로 직접 계산** 이 훨씬 안전. 누적 방식의 유일한 장점은 연속 증분 시 성능이지만, 정점 생성은 frame 당 한 번이라 성능 이점이 무의미.

**판별법**: "내가 파라메트릭으로 만든 곡면이 나선/칼날 모양이 되면" -> 누적 변수의 스코프 먼저 확인. 특히 outer loop 의 누적이 inner loop 에 들어가 있는지.

---

### 2-6. [NEW] Per-quad 색 할당 -> 부드러운 그라데이션 불가능

```cpp
// ❌ quad 하나당 색 하나 — 4 정점에 같은 값 주입
for (row, col) {
    float adjU = (float)col / numCols;
    float adjV = (float)row / numRows;
    auto color = bilinearLerp(cornerColors, adjU, adjV);  // quad 전체 색 계산

    for (int idx : indices)
        PushVertex(..., diskPositions[idx], color, ...);   // 모든 정점 같은 색
}

// ✅ per-vertex 색 — 정점 생성 시 위치 기반 색 계산, GPU 가 보간
for (row, col) {
    // 정점 만들면서 정점별 색 계산
    float t = (float)row / (float)(numRows - 1);   // 0..1
    diskVertexColors.push_back(lerp(colorInner, colorOuter, t));
}
// face 루프에서는 정점별 색을 그대로 lookup
for (...) for (int idx : indices)
    PushVertex(..., diskPositions[idx], diskVertexColors[idx], ...);
```

**핵심**: GPU rasterizer 는 **삼각형의 정점 속성을 자동으로 barycentric 보간** 함. CPU 에서 직접 색을 보간하지 말고 **정점에만 값을 주면 된다**. Per-quad 색 할당은 GPU 의 핵심 기능을 포기하는 꼴.

**증상 예시**: `vRes=1, row` 루프가 `row < vRes=1` 이라 **row=0 만 실행** -> `adjV=0` 고정 -> `interp = (1-0)*u2Color + 0*u1Color = u2Color` -> 모든 quad 가 u2Color 한 색으로만 보임. 안쪽 파랑/바깥쪽 빨강 그라데이션을 의도해도 바깥쪽은 절대 나오지 못함.

**판별법**: "bilinear lerp 코드는 있는데 결과가 단색이거나 블로키" -> per-quad 계산인지 확인. 색은 **반드시 per-vertex**.

**보너스**: RGB 공간 4 코너의 bilinear lerp 로는 **무지개 (hue 원)** 를 표현할 수 없다. 무지개가 필요하면:
- HSV/HSL 의 H 축을 직접 파라미터화 후 RGB 로 변환
- 또는 YCbCr 의 Cb/Cr 를 원 위에 배치해서 chroma 원 = hue 원 효과

---

### 2-7. [NEW] `Draw()` 에 특정 메쉬의 상수 하드코딩 -> 범용성 파괴

```cpp
// ❌ "큐브 = 6면 × 6 인덱스 = 36" 이 Draw 에 하드코딩
void Draw(GLuint prog_addr) {
    // ...
    for (int f = 0; f < 6; f++) {   // ← 6 면 고정
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mTextureAddrs[1 + f]);
        glDrawElements(GL_TRIANGLES, 6,    // ← 면당 6 인덱스 고정
                       GL_UNSIGNED_INT,
                       (void *)(f * 6 * sizeof(GLuint)));   // ← f*6 오프셋 가정
    }
}

// ✅ 모델 자신의 인덱스 수를 쓰는 범용 draw
void Draw(GLuint prog_addr) {
    // ... 텍스처 바인딩 (gate + size check) ...
    glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0);
}
```

**증상**: Disk (384 인덱스, 64 slice) 를 이 Draw 로 그리면 **앞 36 인덱스 = 앞 6 slice 만** 그려짐. 육안으로 `36/384 ≈ 9.375%` = 약 **33.75° 부채꼴**. 전체 원의 1/10 정도만 보여서 "한 바퀴를 못 돈다" 고 오해하기 쉽지만 **BuildDisk 의 정점 데이터는 정상** — 드로우 쪽이 앞부분만 잘라 그린 것.

**핵심**: `ModelBase::Draw()` 같은 범용 함수에는 **특정 메쉬의 상수(`6`, `f*6`, `36`) 를 절대 하드코딩하지 말 것**. 인덱스 개수는 객체 상태(`mIndexCount`) 에서, 면별 상태(텍스처 등)는 서브메쉬 구조에서 가져와야 함.

**단일 책임 원칙**:
- "큐브 면별 텍스처" 같은 특수 기능은 서브클래스 / sub-mesh 리스트 / 별도 Model 로 분리
- 범용 `Draw()` 는 "VAO 바인딩 -> uniform -> `glDrawElements(mIndexCount)`" 만 해야 함

**판별법**: 다른 메쉬를 넣었는데 **부분만 그려짐** = `Draw()` 의 인덱스 카운트가 mesh 에 맞춰 계산되는지 확인.

---

### 2-8. [NEW] 정규화 분모 off-by-one: `/numCols` vs `/uRes`

```cpp
int numCols = uRes + 1;   // 정점 개수 = 면 개수 + 1
int numRows = vRes + 1;

// ❌ [0, 1] 전체를 못 덮음 — 최대값이 uRes/(uRes+1) 에서 멈춤
float adjU = (float)col / numCols;
float adjV = (float)row / numRows;

// ✅ [0, 1] 풀 범위
float adjU = (float)col / uRes;    // col ∈ [0, uRes] -> adjU ∈ [0, 1]
float adjV = (float)row / vRes;    // row ∈ [0, vRes] -> adjV ∈ [0, 1]
```

**영향**: `vRes=1` 일 때:
- `adjV = row / numRows = 0/2 = 0` (max)
- 색 보간 / UV 매핑이 [0, 1] 대신 [0, 0] 구간에서만 평가
- `vRes=10` 이어도 `adjV` 최대는 `9/11 ≈ 0.82` — 1.0 에 도달 못함

**일반 규칙**:
- **"N 등분 = 정점 N+1 개"**: UV/색 정규화할 때는 **"등분 수 (= `uRes`/`vRes`)"** 로 나눠야 [0, 1] 을 모두 덮는다
- numCols/numRows 는 정점 개수 세는 용도일 뿐, 정규화 분모가 아님

---

## Priority 3: Mental Model / 개념적 오해

### 3-1. [NEW] Texture Unit 은 global state, 한 draw 는 여러 unit 을 동시 샘플링 🔴

**오해**: "draw call 하나 = 텍스처 하나를 그림". 따라서 "7개 텍스처를 쓰려면 draw 도 7번 해야 한다".

**실제**: OpenGL 의 texture unit (`GL_TEXTURE0`, `GL_TEXTURE1`, …) 은 **독립된 글로벌 슬롯**이고, 한 draw call 의 shader invocation 은 **여러 unit 에서 동시에** 샘플링할 수 있다. 한 번 `glBindTexture` 로 꽂힌 텍스처는 **새로 bind 하기 전까지 그 unit 에 영구 유지**.

### 핵심 모델

```
OpenGL Context State
├── Texture Unit 0  ->  [ 무엇이 꽂혀있음 ]   ← glBindTexture 전까지 유지
├── Texture Unit 1  ->  [ 무엇이 꽂혀있음 ]
├── Texture Unit 2  ->  [ 무엇이 꽂혀있음 ]
├── ...
```

각 unit 은 **독립 슬롯**이고, bind 호출은 "현재 active unit 에 새 texture 를 끼워넣는" 동작일 뿐. **unbind 자체가 없다** — 그냥 다른 걸로 덮어쓰거나 프로그램이 종료될 때까지 그대로.

### 전형적인 예시 (exercise6 Cube::Draw)

```cpp
// 루프 밖 — unit 0 에 container 를 한 번만 꽂음
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, mTextureAddrs[0]);   // container

// 루프 안 — unit 1 만 매 iteration 마다 교체
for (int f = 0; f < 6; f++) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mTextureAddrs[1 + f]);   // side[f]
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(f * 6 * sizeof(GLuint)));
}
```

**각 draw 시점의 실제 state**:

| draw | unit 0 (tex1) | unit 1 (tex2) |
|------|---------------|---------------|
| 1 | container | side1 |
| 2 | container | side2 |
| 3 | container | side3 |
| 4 | container | side4 |
| 5 | container | side5 |
| 6 | container | side6 |

**container 가 모든 draw 에서 unit 0 에 존재** 하는 이유: 아무도 unit 0 에 새 bind 를 안 걸었기 때문. GL 은 "관리 안 하면 자동으로 꺼지는" 방식이 아니라 **"새로 꽂기 전까지는 그대로"** 방식.

### 수학적으로 정리

- **draw call 수**: 6
- **고유 텍스처 수**: 7 (container 1 + side 6)
- **활성 (sampler, texture) 매핑 수**: 6 × 2 = **12** (매 draw 마다 tex1 과 tex2 둘 다 읽음)

**공식**: `activations = draws × samplers_per_draw`, 이는 `draws == textures` 와 전혀 다른 값.

### 게임 엔진 표준 패턴

Texture unit 을 **"bind 빈도"** 로 구분해서 관리:

| Unit | 용도 | bind 빈도 | 예시 |
|------|------|----------|------|
| 0 | albedo/diffuse | 모델마다 | 캐릭터 피부, 벽돌, 천 |
| 1 | normal map | 모델마다 | 범프 디테일 |
| 2 | lightmap | 씬 시작 시 1회 | 미리 구운 조명 |
| 3 | shadow map | 프레임마다 1회 | 동적 그림자 |
| 4 | environment cubemap | 거의 영구 | IBL 반사 |

**핵심**: "자주 바뀌는 유닛만 매 draw 마다 rebind, 공유 유닛은 프레임 시작 시 한 번만 bind". Draw call 별 GL 호출 수가 극적으로 줄어듦.

### ⚠️ 함정: bind 는 했는데 FS 가 샘플링 안 하는 경우

```glsl
uniform sampler2D tex1;
uniform sampler2D tex2;

void main() {
    vec4 c1 = texture(tex1, fs_in.vsTexCoord);
    // vec4 c2 = texture(tex2, fs_in.vsTexCoord);   ← 주석 처리
    fragColor = c1 * fs_in.vsColor;
}
```

이 상태에서 CPU 가 아무리 열심히 `glBindTexture(GL_TEXTURE1, ...)` 를 루프에서 돌려도, **FS 는 tex2 를 안 읽으므로 모든 면이 tex1 (container) 만으로 그려짐**. Draw call 수·텍스처 바인딩 수와 실제 화면은 별개.

**판별법**: "루프에서 분명히 텍스처를 바꾸는데 면이 전부 같은 이미지로 보임" -> FS 가 해당 sampler 를 실제로 사용하는지 확인.

### 원칙 정리

1. **Texture unit = global persistent slot**. "해제" 가 아니라 "덮어쓰기" 로 관리.
2. **한 draw 는 여러 unit 을 동시에** 읽는다. `sampler2D` uniform 이 n 개면 이론상 n 개의 동시 샘플링.
3. **Draw call 수 ≠ 텍스처 수**. 둘 사이 관계는 설계에 따라 자유로움.
4. **"자주 바뀌는 것만 rebind, 공유 자원은 한 번만"** — 엔진 성능 최적화의 기본.
5. **bind 만으로는 반영 안 됨** — FS 가 실제로 해당 sampler 를 샘플링해야 화면에 나타남.

---

### 3-2. [NEW] 대량 텍스처 렌더링은 "시분할" 이 아닌 "집합화 + 가상화"

**흔한 오해**: "texture unit 을 시분할로 계속 교체하면 수백만 개 텍스처도 그릴 수 있다."

**현실**: 시분할 접근은 **3단계의 독립적인 제약** 에 부딪혀서 10~100 개 정도에서만 실용적이다. 그 이상은 전혀 다른 기법이 필요.

### 3단계 제약 벽

#### ① Per-draw unit 상한 (하드웨어)
```cpp
GLint maxUnits;
glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxUnits);
// 대부분 하드웨어: 16~192
```
| 플랫폼 | 상한 |
|-------|------|
| 모바일 | 16 |
| 데스크탑 GL 4.x | 48~96 |
| 최신 고급 | 192 |
| Vulkan (descriptor indexing) | 수천~수만 |

**한 draw call 의 shader invocation 이 참조 가능한 "서로 다른 텍스처"** 의 상한. 192 를 넘기려면 아래의 집합화 기법 필요.

#### ② Draw call overhead (CPU)
```
60 FPS = 16.67 ms / frame
draw call 1회 ≈ 5~50 μs  (드라이버 호출 + state 검증)
100,000 draws × 20 μs = 2000 ms = 2 초 -> 0.5 FPS ❌
```

**현실 예산** (프레임당 draw call):
| 게임 | 예산 |
|------|------|
| 모바일 | 200~500 |
| 콘솔/PC | 1,000~5,000 |
| AAA 최적화 | 5,000~15,000 |
| Vulkan/D3D12 극한 | 30,000~100,000 |

즉 **"100만 텍스처 = 100만 draw"** 는 드라이버 호출만으로 프레임이 수십 초 걸려서 실격.

#### ③ VRAM 용량 (GPU 메모리)
| 해상도 | RGBA8 + mips |
|-------|--------------|
| 64×64 | ~21 KB |
| 512×512 | ~1.3 MB |
| 2048×2048 | ~21 MB |
| 4096×4096 | ~85 MB |

**100만 개 × 64×64** ≈ 21 GB (RTX 4090 급도 겨우 수용)
**100만 개 × 512×512** ≈ 1.3 TB (물리적 불가)

현실 GPU VRAM:
| GPU | VRAM |
|-----|------|
| RTX 4090 | 24 GB |
| RTX 3060 | 12 GB |
| 모바일 통합 | 2~4 GB |

### 실전 해법: "시분할" 대신 "집합화 + 가상화"

| 기법 | 핵심 아이디어 | 규모 | 한계 |
|------|--------------|------|------|
| **Texture Array** (`GL_TEXTURE_2D_ARRAY`) | 1 객체에 N 레이어, 1 bind 로 전체 접근 | 수백~수천 | 모두 같은 해상도/포맷 |
| **Texture Atlas** | 여러 이미지를 큰 하나로 합침, UV 로 영역 선택 | 수백 (2D/UI 표준) | padding 필요, mipmap 제약 |
| **Bindless Textures** (`ARB_bindless_texture`) | 텍스처 = 64-bit handle, unit 대신 배열 인덱싱 | 수천~수만 | 드라이버 지원 필요 |
| **Sparse Textures / Virtual Texturing** | 거대한 logical texture, 페이지 단위 상주 | 사실상 무제한 | 구현 복잡도 높음 |
| **LOD Streaming** | 거리에 따라 해상도 동적 조정/언로드 | Open world 전체 | 스트리밍 로직 필요 |

### 실제 게임 사례

- **id Software의 Rage (2011) — MegaTexture**: 전 월드가 **단일 거대 virtual texture**, 페이지 스트리밍
- **UE5 Nanite + Virtual Shadow Maps**: 기하와 텍스처 모두 가상화
- **오픈월드 RPG**: 디스크에 50~200 GB 텍스처, 프레임당 활성은 3000~10000 장, 실제 동시 샘플링은 4~16 개

### 질문별 정량 답

| "몇 개를 동시에 그릴 수 있나?" | 답 |
|-----|-----|
| 10개 | 쉬움. 한 draw 에 10 unit bind, 일반 OpenGL 범위 |
| 100개 | 가능하나 비효율. Texture Array / Atlas 가 정답 |
| 1,000개 | Bindless Textures 또는 Texture Array 필수 |
| 100,000개 | Virtual Texturing + 스트리밍 필요 |
| 1,000,000개 | "동시에" 라는 말 자체가 성립 안 함. 월드 존재량일 뿐, 매 순간 활성은 수백~수천 |

### 실전 셰이더가 실제로 읽는 텍스처 수

**PBR 표준 머티리얼 1 개가 쓰는 텍스처**:
- Albedo / Normal / Roughness / Metallic / AO / Emissive = **6**
- + Lightmap / Shadow map / Env cubemap = **+3**
- + Detail / Mask = **+2~5**

**합계 10~15 개**. 실전 셰이더에서 한 픽셀을 그리는 데 쓰이는 숫자. 그 이상은 엔진 구조만 복잡해지고 성능은 떨어지는 "다이미니싱 리턴" 영역.

### 원칙 정리 (보강)

6. **"몇 개를 그릴 수 있나" 질문은 세분화 필요**:
   - 한 shader invocation 당 -> 10~192 (하드웨어 상한)
   - 한 frame 당 고유 텍스처 -> 수천~수만 (VRAM + draw call 예산)
   - 월드 전체 존재량 -> 수십만~백만 (디스크 + 스트리밍)
   - VRAM 상주량 -> 수천~수만 (용량 제약)
7. **규모가 커지면 "bind 를 빠르게 바꾸기" 가 아니라 "bind 횟수 자체를 줄이는 기법"** 으로 전환해야 함. Texture Array, Atlas, Bindless 가 그 관문.
8. **"시분할" 은 수십~수백 개까지만 실용적**. 수천 개 이상은 집합화, 수만 개 이상은 가상화/스트리밍이 아니면 프레임이 통째로 날아감.

---

### 3-3. [NEW] vmath 의 곱셈 컨벤션 — `mat·mat` 은 column, `vec·mat` 은 row (비대칭) 🔴

**오해**: "vmath 는 GLSL 처럼 column convention 이니까 `mat * vec` 으로 변환 적용하면 되겠지."

**현실**: vmath 는 **두 곱셈의 컨벤션이 서로 다른** 비대칭 라이브러리. 같은 행렬을 GLSL 에선 정상으로 쓰는데, C++ 에서 vec 에 곱하면 **반대 방향 변환** 이 일어남.

### vmath 컨벤션 한 줄 요약

| 연산 | 정의됨? | 컨벤션 | 결과 |
|------|--------|--------|------|
| `mat * mat` | ✅ | **Column convention** (표준) | 일반 수학 곱셈 그대로 |
| `mat * vec` | ❌ | — | **컴파일 에러** |
| `vec * mat` | ✅ | **Row vector convention** | 수학적으로 **`M^T · v`** |

-> `mat * mat` 은 GLSL 과 똑같지만, `vec * mat` 만 row 컨벤션이라 결과가 **transpose 된 변환** 이 적용됨.

### 검증 — 구체 예시

Y축 +90° 회전 행렬 (column-major 표준):
```
R = ┌  0  0  1  0 ┐
    │  0  1  0  0 │
    │ -1  0  0  0 │
    └  0  0  0  1 ┘
```

벡터 `(1, 0, 0, 1)` 에 적용:

| 방법 | 결과 | 의미 |
|------|------|------|
| GLSL `R * v` (column convention) | `(0, 0, -1, 1)` | **+90° 정상** |
| vmath `pos * R` (C++) | `(0, 0, +1, 1)` | **−90° 반대 방향** ❌ |

회전 행렬은 직교 (`R^T = R^{-1}`) 라서 vmath 의 `vec * R` 는 **inverse rotation** 을 적용하는 꼴.

### 왜 이렇게 됐나

vmath 의 헬퍼 (`vmath::rotate`, `vmath::translate`, `vmath::scale`) 는 **GLSL 호환 column-major 행렬** 을 생성. 그래서 GLSL 에 업로드해서 `mat * vec` 으로 쓰면 정상.

하지만 vmath 의 C++ `operator*` 는 [vmath.h:1235](include/vmath.h#L1235) 에 **`vec * mat` 만** 정의돼 있고, 그 구현이 row vector 컨벤션 (`v^T · M`). 결과적으로:
- 같은 행렬이 GLSL 에선 `M·v`, vmath C++ 에선 `M^T·v` 로 동작 -> **부호/방향이 반대**.

### 실전 규칙 5가지

#### Rule 1 — `mat * mat` 은 GLSL 과 동일
```cpp
auto model = vmath::translate(...) * vmath::rotate(...) * vmath::scale(...);
//           T  ·  R  ·  S — 표준 column 순서, 정점 입장에선 S -> R -> T
```
이 매트릭스를 GLSL 에 uniform 으로 올리면 정상 작동.

#### Rule 2 — CPU 에서 vec 변환은 가능한 한 피하기
정점 변환은 **GLSL 에서** 일어나는 게 표준. C++ 에서 vec 에 매트릭스를 곱할 일이 거의 없어야 함. CPU 측 vec 변환은 보통:
- 디버깅용 좌표 출력
- AABB / culling 계산
- 픽킹 (마우스 클릭 -> world ray)

#### Rule 3 — CPU 에서 vec 에 변환을 꼭 적용해야 한다면

**(A) 안전한 길** — **대각 행렬** (`M = M^T`) 만 `vec * mat` 사용
- Identity, Scale, **Mirror** 는 symmetric 이라 `pos * M = M * pos`
- 회전/이동은 **절대 `vec * mat` 으로 직접 적용 금지**

**(B) 일반적인 길** — 명시적 `transpose()` 사용
```cpp
vmath::vec4 v_new = pos * M.transpose();   // 표준 M·v 효과
```
`mat4::transpose()` 는 vmath 에 정의돼 있음 ([vmath.h:845](include/vmath.h#L845)).

#### Rule 4 — `vmath::translate / rotate / scale` 출력은 GLSL 호환
이 헬퍼들이 만든 행렬은 column-major standard. GLSL 에서 `mat * vec` 로 쓰면 정상.

#### Rule 5 — `mat * mat` 합성 순서는 column convention 그대로
```cpp
auto M = T * R * S;     // 정점 입장에서 S -> R -> T 순으로 적용
```
이건 직관적 — GLSL 책에 있는 것과 동일.

### 빠른 참고 — CPU 에서 헷갈리는 케이스

| 코드 | 결과 | 의도 일치? |
|------|------|----------|
| `T * R * S` (mat-mat) | 표준 column 순서 | ✅ |
| `pos * scale_mat` | scale 적용 | ✅ (대각이라 OK) |
| `pos * mirror_mat` | mirror 적용 | ✅ (symmetric) |
| `pos * rotate_mat` | **반대 방향 회전** | ❌ |
| `pos * translate_mat` | **잘못된 결과** | ❌ |
| `pos * (T*R*S)` | **inverse 변환** | ❌ |
| `pos * (T*R*S).transpose()` | **표준 column M·v 효과** | ✅ |
| `mat * vec` | **컴파일 에러** | — |

### 다른 라이브러리와 비교

| 라이브러리 | mat × mat | mat × vec | vec × mat | 컨벤션 |
|----------|-----------|-----------|-----------|--------|
| **vmath** (sb7) | column 표준 | ❌ 없음 | row vec (`M^T·v`) | **혼합** ⚠️ |
| **GLM** | column 표준 | ✅ `M*v` | ✅ `v*M` (= `M^T·v`) | column |
| **GLSL** | column 표준 | ✅ `M*v` | ✅ `v*M` (= `M^T·v`) | column |
| **DirectXMath** | row 표준 | ❌ | ✅ `v*M` | row |
| **Eigen** | column 표준 | ✅ `M*v` | ❌ | column |

-> **vmath 만 유독 `mat * vec` 이 없어서** "column 인 줄 알았는데 vec 만 row" 라는 함정이 생김.

### 이번 함정 (Octahedron Mirror 변환)

```cpp
vmath::mat4 xzMirror = vmath::mat4::identity();
xzMirror[1][1] = -1;

// ✓ 첫 시도 (결과는 우연히 맞지만 의미 불명확)
for (const auto &pos : positions)
    result.push_back(pos * xzMirror);

// ❌ "표준대로" 바꿔본 두 번째 시도 (mat * vec 는 vmath 에 없음 -> 컴파일 에러)
for (const auto &pos : positions)
    result.push_back(xzMirror * pos);

// ✅ 정답: vmath 컨벤션 인정 + 주석으로 대각 행렬임을 명시
//   vmath 는 vec * mat 만 정의. 일반 행렬엔 M^T·v 효과지만,
//   xzMirror 는 대각이라 결과는 표준 M·v 와 동일.
for (const auto &pos : positions)
    result.push_back(pos * xzMirror);
```

**교훈**: 라이브러리 컨벤션을 모르고 "표준" 을 가정해서 mat-vec 순서를 바꿨다가, 그게 일치하는 컨벤션이 없어서 컴파일 에러. **반드시 사용 중인 라이브러리의 실제 operator 정의를 확인하고 수용**.

### 외워둘 한 줄 멘토링

> **"vmath 에서는 mat 끼리 곱은 표준이지만, vec 가 끼면 모든 게 transpose 된다."**

또는 더 실용적으로:

> **"CPU 에서 정점 변환하지 마라. mat 만 만들고 GLSL 에 보내라."**

---

### 3-4. [NEW] `glUniform1i(sampler)` 와 `glBindTexture` 는 **독립된 두 단계** 🔴

**오해**: "sampler uniform 값을 설정하면 텍스처가 연결되는 거 아닌가?"

**현실**: 쉐이더에 `uniform sampler2D tex1;` 이 있을 때, **"어느 unit 의 텍스처를 읽을지"** 와 **"그 unit 에 어떤 텍스처가 있는지"** 는 **서로 독립된 두 상태**. 둘 다 설정해야 실제 연결이 됨.

### 두 단계의 역할 분리

```cpp
// ❌ 이것만 하면 "sampler 가 unit 0 을 읽도록" 설정만 함
glUniform1i(glGetUniformLocation(prog_addr, "tex1"), 0);
//          ^^^^^^^^ tex1 이라는 이름의 sampler uniform 에
//                   "0" 이라는 int 값 (unit 번호) 저장
// -> 셰이더: "tex1 은 unit 0 에서 읽어라" 라는 매핑 설정
// -> 하지만 unit 0 에 실제로 뭐가 꽂혀있는지는 **별개의 state**

// ✅ 실제 연결: unit 0 에 텍스처를 꽂아야 함
glActiveTexture(GL_TEXTURE0);                       // 이후 bind 가 어느 unit 으로
glBindTexture(GL_TEXTURE_2D, textureHandle);        // 그 unit 에 텍스처 꽂기
```

### 두 state 가 별개라는 증거

OpenGL 의 state 는 대략 이렇게 나뉨:

```
┌─────────────────────────────────────────────────┐
│  Program state (glUseProgram 활성)               │
│  ├── uniform "tex1" = 0       ← glUniform1i     │
│  ├── uniform "tex2" = 1                         │
│  └── ...                                        │
└─────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────┐
│  Context state (global)                         │
│  ├── Unit 0 -> [ texture name ]  ← glBindTexture │
│  ├── Unit 1 -> [ texture name ]                  │
│  ├── Unit 2 -> [ texture name ]                  │
│  └── ...                                        │
└─────────────────────────────────────────────────┘
```

Sampler uniform 은 **"어느 unit 을 볼지"** 를 program 내부에 기록하고, `glBindTexture` 는 **"그 unit 에 뭐가 꽂혀있는지"** 를 context 에 기록. **어느 한쪽이라도 없으면 연결이 깨짐**.

### `glUniform1i` 만 한 상태에서의 실제 동작

Unit 0 에 아무것도 bind 되지 않은 상태로 `glUniform1i(tex1, 0)` 만 호출하면:
- **첫 프레임**: 드라이버 기본값 (보통 검정 또는 흰색 "zero texture")
- **그 이후**: 이전에 누가 unit 0 에 bind 했던 것 (운 좋으면 엉뚱한 텍스처, 운 나쁘면 삭제된 dangling handle)

### 이번 함정 (Cube::Draw)

```cpp
// ❌ Cube::Draw 현재 상태
glUniform1i(glGetUniformLocation(prog_addr, SAMPLER_TEX1), 0);   // "tex1 은 unit 0" 매핑만
glUniform1i(glGetUniformLocation(prog_addr, SAMPLER_TEX2), 1);

// ← 여기서 glActiveTexture(GL_TEXTURE0) + glBindTexture 가 빠짐!
//    -> unit 0 에 container 가 실제로 연결되지 않음
//    -> tex1 sampler 는 엉뚱한 것을 샘플링

for (int f = 0; f < 6; f++) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mTextureAddrs[1 + f]);   // tex2 만 제대로 bind
    glDrawElements(...);
}
```

**증상**: "container 텍스처를 `AddTexture` 로 분명히 넣었는데 면에 나타나지 않음". `mTextureAddrs[0]` 는 존재하지만 unit 0 에 연결되지 않아서 tex1 sampler 가 읽지 못함.

**고치려면** 루프 앞에 추가:
```cpp
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, mTextureAddrs[0]);   // ← container 를 unit 0 에 bind
```

### 원칙

**텍스처 연결은 3-step 이다**:

1. **Unit 활성화** — `glActiveTexture(GL_TEXTUREN)` ← 이후 bind 대상 unit 지정
2. **텍스처 bind** — `glBindTexture(target, handle)` ← 그 unit 에 실제 꽂기
3. **Sampler 매핑** — `glUniform1i(location, N)` ← 셰이더에게 "읽어라" 알림

1, 2 를 **묶어서 생각** 하되 3 은 **별개**. "왜 안 나오지?" 질문이 들면 세 단계 모두 호출됐는지 체크.

**판별법**: "분명 `AddTexture` 했고 sampler uniform 도 세팅했는데 검정/흰색 화면" -> **`glBindTexture` 가 실제 호출되었는지** 가장 먼저 의심.

---

### 3-5. [NEW] 암묵적 순서 계약 (Implicit Ordering Contract) — Fragile Pattern 🔴

**오해**: "인덱스로 매핑하는 게 제일 단순하지."

**현실**: 인덱스 기반 매핑은 **"여러 곳이 같은 순서로 유지되어야" 한다는 암묵 계약**을 만든다. 컴파일러가 강제하지 않는 이 계약은 **아무도 안 볼 때 조용히 깨지는** 리팩토링 폭탄.

### 이번 케이스 — 3-way 순서 동기화

exercise6 의 Cube 텍스처-면 매핑은 **3곳이 동시에 같은 순서**여야 성립:

```
┌─────────────────────────────────┐
│ ① CUBE_FACE_INDICES 배열 순서   │
│    -Z(0) -> +X(1) -> +Z(2) -> ...  │──┐
└─────────────────────────────────┘  │
                                     ▼
┌─────────────────────────────────┐  │
│ BuildCube 가 VBO 에 이 순서로   │  │
│ 정점 push -> VBO 레이아웃        │  │
│    -Z[0..5] -> +X[6..11] -> ...   │  │
└─────────────────────────────────┘  │
                                     │
┌─────────────────────────────────┐  │
│ ② TEXTURE_SIDES 배열 순서       │  │
│    side1 -> side2 -> side3 -> ... │──┤
└─────────────────────────────────┘  │
                                     │
┌─────────────────────────────────┐  │
│ AddCubeTexture 가 순서대로 push │  │
│    mTextureAddrs[1] = side1     │  │
│    mTextureAddrs[2] = side2     │  │
│    ...                          │  │
└─────────────────────────────────┘  │
                                     │
┌─────────────────────────────────┐  │
│ ③ Draw 루프 인덱스 산술         │  │
│    (void*)(f*6*sizeof(GLuint))  │──┘
│    mTextureAddrs[1 + f]         │
└─────────────────────────────────┘
```

세 개가 **우연히** 같은 방향으로 전진해서 "-Z=side1, +X=side2, +Z=side3, -X=side4, -Y=side5, +Y=side6" 이 성립.

### 무엇이 계약을 깨는가

| 변경 | 영향 |
|------|------|
| `CUBE_FACE_INDICES` 에 새 면 삽입 / 순서 변경 | VBO 레이아웃 어긋남 -> 엉뚱한 면에 side 텍스처 |
| `AddTexture(container)` 전에 다른 `AddTexture` 추가 | `mTextureAddrs[1]` 이 side1 이 아님 |
| `TEXTURE_SIDES` 배열 재정렬 | 그대로 반영됨 |
| Draw 루프 오프셋 공식 수정 | 완전히 어긋남 |
| `AddCubeTexture` 가 역순으로 push 하도록 수정 | 조용히 뒤집힘 |

컴파일러는 이 중 어느 것도 **에러로 잡지 않음**. 코드는 정상적으로 돌아가고, 화면에만 잘못된 결과가 나타남.

### 왜 이게 위험한가

1. **디버깅 역추적 비용**: "side3 이 +Z 가 아닌 -X 에 나오네?" -> 원인이 3곳 중 어디인지 모름
2. **코드 이전 시 취약**: 파일 복제할 때 한 곳만 복사하면 깨짐 ([STUDY_NOTE Exercise6 3-5](#3-5) 와 연결)
3. **협업 함정**: 다른 사람이 `TEXTURE_SIDES` 에 side7 추가하면서 오타로 중간에 삽입 -> 매핑 전체 뒤틀림
4. **미래의 나**: 6개월 후 내가 "왜 이렇게 복잡하게 했지?" 하고 `CUBE_FACE_INDICES` 재배열하면 즉시 깨짐

### 구조적 해결 — 순서를 데이터 구조로 박제

**Option A — 명시적 매핑 테이블** (최소 수정):
```cpp
// 면 이름과 텍스처를 페어로 묶어서 의도 명시
struct CubeFaceTexture {
    const char* faceName;  // "-Z", "+X", ...
    const char* imagePath;
};
static const CubeFaceTexture CUBE_FACE_TEXTURES[6] = {
    {"-Z", "./textures/side1.jpg"},
    {"+X", "./textures/side2.jpg"},
    {"+Z", "./textures/side3.jpg"},
    {"-X", "./textures/side4.jpg"},
    {"-Y", "./textures/side5.jpg"},
    {"+Y", "./textures/side6.jpg"},
};
```
-> "side3 는 +Z 면에 붙는다" 가 데이터로 명시됨. 순서가 헷갈릴 때 이 테이블만 보면 됨.

**Option B — Material 추상화** (근본 해결, Material_Texture.md Step 3 참조):
```cpp
model->GetMaterial()
    .SetFaceTexture(Face::NegZ, Texture::Load("side1.jpg"))
    .SetFaceTexture(Face::PosX, Texture::Load("side2.jpg"))
    // ... 이름 기반이라 순서 무관, 실수 불가능
    ;
```
-> enum/string 기반이라 **컴파일러가 일부 실수 (오타, 중복) 를 잡아줌**.

### 원칙

**암묵적 순서 계약 = 리팩토링 폭탄**. 셋 이상이 동시에 맞춰져야 하는 순서는 **데이터 구조로 묶어서 강제**해야 안전.

**판별법**: 코드에서 `[N+f]`, `f*K` 같은 인덱스 산술이 **여러 배열에 걸쳐** 있고, 각각이 서로 다른 데이터를 가리키면 -> **암묵 계약 냄새**. 한 곳에 `struct` 또는 `map` 으로 모아야 함.

**한 줄 멘토링**:
> **"세 개 이상의 배열이 같은 순서로 진행돼야 한다면, 그 시점에 구조체로 묶어야 할 때이다."**

---

## 체크리스트 (Exercise6 확장)

| # | 항목 | 확인 |
|---|------|------|
| 18 | 🔴 **`glUniform*` 함수가 쉐이더 타입과 1:1 매칭**되는가? (vec2->`2fv`, mat4->`Matrix4fv`, sampler->`1i`) | |
| 19 | 🔴 파라메트릭 서피스의 누적 변수가 **올바른 루프 스코프**에 있는가? (가능하면 인덱스 직접 계산) | |
| 20 | 색 보간이 **per-vertex** 인가? (per-quad 아님 — GPU 에게 맡김) | |
| 21 | `Draw()` 가 `mIndexCount` 를 쓰는가? (특정 메쉬 상수 하드코딩 금지) | |
| 22 | UV/색 정규화 분모가 **`uRes`/`vRes`** 인가? (`numCols`/`numRows` 아님) | |
| 23 | `std::vector::operator[]` 접근 전에 **out-of-bounds 도** 체크했는가? (빈 체크만으로 부족) | |
| 24 | `glGetError()` 를 개발 중에 주기적으로 호출하는가? (특히 uniform 업로드 후) | |
| 25 | 🔴 Texture unit 이 **글로벌 지속 상태** 라는 사실을 기억하는가? (bind = persistent) | |
| 26 | 공유 텍스처는 **루프 밖에서 한 번만** bind 하는가? (자주 바뀌는 것만 루프 안) | |
| 27 | CPU 에서 bind 한 텍스처를 **FS 가 실제로 샘플링** 하는지 확인했는가? | |
| 28 | 🔴 vmath 에서 `mat * vec` 는 **컴파일 에러** 임을 알고 있는가? | |
| 29 | 🔴 vmath 의 `vec * mat` 는 **`M^T · v`** 임을 알고 있는가? (대각 행렬 외에는 결과가 다름) | |
| 30 | CPU 에서 vec 변환은 **대각 행렬에만** 직접 적용하는가? (회전/이동은 transpose 나 GLSL 위임) | |
| 31 | 🔴 Sampler uniform 설정 외에 **`glBindTexture` 도 반드시** 호출하는가? | |
| 32 | 텍스처 연결 3-step (activate -> bind -> uniform) 을 모두 거쳤는가? | |
| 33 | 🔴 여러 배열이 **같은 순서로 전진** 해야 하는 암묵 계약이 있다면, 데이터 구조로 묶었는가? | |

---

## 핵심 교훈 요약 (Exercise6 확장)

1. 🔴 **`glUniform*` 은 쉐이더 타입과 반드시 매칭** — mat4 함수로 vec2 업로드 같은 미스매치는 드라이버가 조용히 거절하고 uniform 은 기본값 (0) 에 머무름. 이게 "단색 텍스처" 와 "애니메이션 무반응" 같은 복합 증상으로 나타난다.
2. **파라메트릭 생성은 인덱스 직접 계산** — 누적 변수는 outer/inner 루프 스코프를 혼동하기 쉽고, row 전환 시 reset 을 빼먹으면 나선/엉뚱한 형태가 된다. `index × delta + start` 공식이 안전.
3. **색은 per-vertex, 보간은 GPU 에게** — CPU 에서 quad 평균색을 계산해서 넣는 것은 rasterizer 의 기본 기능을 포기하는 꼴. 정점 속성만 주면 자동 보간됨.
4. **범용 `Draw()` 에 특정 메쉬 상수 금지** — `6` 면, `36` 인덱스, `f*6` 오프셋 같은 "큐브 가정" 이 범용 코드에 박히면 다른 메쉬에서 부채꼴/부분만 그려지는 증상을 낳는다. `mIndexCount` 와 sub-mesh 구조로 분리.
5. **정규화 분모는 "등분 수"** — `uRes`/`vRes` (면 개수) 로 나눠야 [0, 1] 을 덮는다. `numCols`/`numRows` (정점 수) 로 나누면 off-by-one 으로 1 에 도달 못함.
6. **같은 실수 두 번 — vector 인덱스 가드** — 빈 vector `[0]` UB (Exercise6 원 노트 1-4) 에 이어서 **out-of-bounds `[N]`** 으로 한 번 더 재발. **"vector 인덱싱 = 크기 검사 필수"** 를 조건반사로 만들 것.
7. **GL 은 기본적으로 조용함** — 어떤 문제든 "eh? 코드 맞는데 안 되네" 가 들면 `glGetError()` 부터 찔러본다. 특히 uniform 업로드/sampler 바인딩 이후는 함정 천국.
8. **Texture unit = global persistent slot** — "draw 1회 = 텍스처 1개" 라는 오해를 버릴 것. 한 draw 가 여러 unit 을 동시에 읽고, 공유 텍스처는 한 번만 bind 하면 모든 후속 draw 가 자동으로 사용함. Draw call 수와 텍스처 수는 독립 변수. 다만 **FS 가 해당 sampler 를 실제로 읽어야** 화면에 반영됨 (bind ≠ render).
9. **라이브러리 컨벤션은 "표준" 을 가정하지 말고 검증할 것** — vmath 의 `mat * mat` 은 표준 column convention 인데 `vec * mat` 만 row convention 이라 비대칭. "GLSL 처럼 `M * v` 로 쓰면 되겠지" 가 컴파일 에러 + 회전 부호 반전을 동시에 부른다. **CPU 에서 정점 변환은 가능한 한 피하고, 꼭 필요하면 대각 행렬에만 적용 또는 `M.transpose()` 사용**. 더 일반적으로: "내가 쓰는 라이브러리의 operator 정의를 직접 본 적이 없으면 가정하지 말 것".
10. **텍스처 연결은 3-step** — `glActiveTexture(unit)` -> `glBindTexture(handle)` -> `glUniform1i(sampler, unit)`. 세 단계가 **각각 독립된 GL state** 를 바꾸며, 하나라도 빠지면 조용히 실패. 특히 `glUniform1i` 만으로는 "매핑" 만 설정할 뿐 실제 텍스처가 연결되지 않음. "AddTexture 도 했고 uniform 도 세팅했는데 화면이 검정" 이면 `glBindTexture` 호출 여부부터 의심.
11. **암묵적 순서 계약은 리팩토링 폭탄** — 두 개 이상의 배열/인덱스가 "같은 순서로 전진해야 성립" 하는 코드는 컴파일러가 강제하지 않아 조용히 깨진다. 순서 의존 코드가 발견되면 **`struct` / `map` / enum 으로 관계를 박제** 하는 것이 안전. 인덱스 기반 암묵 계약 -> 이름 기반 명시 계약으로 이전.
