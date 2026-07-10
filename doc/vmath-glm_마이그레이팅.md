# vmath -> glm 마이그레이션 회고 핸드오프

> 작성: 2026-06-21 / 대상 브랜치: game/slang-phase2-ubo / glm 1.0.3 (vcpkg)
> 이 문서는 (1) 미완 마이그레이션에서 추가로 고쳐야 했던 부분, (2) vmath/glm 의 결정적 차이로
> 단일 Python 스크립트가 불가능했던 이유, (3) 서브에이전트가 발견한 버그와 수정 회고를 담는다.

## TL;DR

- sb7 `include/vmath.h` 의존을 glm 으로 전환. working tree 는 *이미 절반쯤 진행되다 멈춘* 상태였고
  (타입은 glm 인데 자유함수/캐스트/헤더가 vmath 잔재) 그 상태로는 **컴파일조차 안 됐다**.
- 이번 세션: 안전분은 Python 스크립트로 일괄, 위험분(행렬/단위/캐스트)은 수동으로 완료.
  결과 = **19개 소스 수정 + 신규 스크립트 1개**, 마이그레이션 코드 전부 빌드 GREEN.
- 핵심 교훈: **glm 의 차이는 "어휘적(token)"이 아니라 "타입/의미적(semantic)"** 이라,
  `vmath::` 토큰 grep + 텍스트 치환만으로는 절대 못 잡는 파손(암시적 `float*` 캐스트)이 남는다.
  -> 이 사각지대를 **독립 빌드 서브에이전트**가 메웠다.
- 남은 빌드 차단 에러 1건(`main.cpp` `CreateStageActor`)은 **vmath 와 무관**한 Stage 리팩토링 미완분.
- 후속 개선(6장): glm 도입으로 `camera.cpp` 의 손-구현 행렬코드(ortho/affine-inverse/역투영)를
  glm 빌트인으로 축약(**138->79줄**, 컴파일 + 수치 동등성 검증). 전체 작업은 커밋 `2a2e659` 에 포함.

---

## 0. 현재 상태 (재측정)

**현재 상태: 이 작업은 커밋 `2a2e659` (`[update][refactor] : imgui vcpkg화, vmath -> glm 마이그레이션`) 에
모두 포함되어 working tree clean.** 아래는 커밋 직전 `git status --short` 스냅샷:

```
 M apps/_MyApp_/src/Entity/Components/MovementComponents.h   (normalize)
 M apps/_MyApp_/src/Entity/Player/PlayerHand.cpp             (radians)
 M apps/_MyApp_/src/InputHandler/ActorFolower.cpp            (length)
 M apps/_MyApp_/src/InputHandler/PlayerController.cpp        (radians/dot/length/degrees + bare normalize 명시화)
 M apps/_MyApp_/src/Physics/PhysicsMovement.h               (normalize)
 M src/common/common.h                                       (주석)
 M src/object/geometry.cpp                                   (normalize/cross/identity)
 M src/object/light.h                                        (dot/max)
 M src/object/transform.h                                    (translate/scale/rotate + 헤더)   [행렬]
 M src/object/vertex.h                                       (주석)
 M src/program/program_uniforms.cpp                          (const float* 캐스트 -> value_ptr) [서브에이전트 발견]
 M src/render/light_ubo_uploader.cpp                         (radians)
 M src/render/mesh_pass_processor.h                          (identity)
 M src/scene/actor.cpp                                       (잉여 mat4 래핑 제거)
 M src/scene/camera.cpp     (migration: lookAt/perspective/identity; + 후속 6장: ortho/affineInverse/inverse 빌트인 축약, 138->79줄)[행렬]
 M src/scene/camera.h                                        (주석 + InverseAffine 선언 제거 [6장])
 M src/scene/compound_actor.cpp                              (normalize)
 M src/scene/light.cpp                                       (normalize)
 M src/sprite/sprite_component.cpp                           (vmath::radians 잔존) [서브에이전트 발견, unstaged 밖이었음]
?? scripts/migrate_vmath_to_glm.py                           (신규 - 안전분 일괄 치환 스크립트)
```

검증 명령: `cmake --build --preset ninja --target _MyApp_`
그라운드 트루스 glm 헤더: `build_ninja/vcpkg_installed/arm64-osx/include/glm/` (1.0.3)

---

## 1. 사용자가 마이그레이션하다 만 부분 - 추가로 고쳐야 했던 것

working tree 진입 시점: 타입(`glm::vec/mat`)은 거의 전환됐고 vcpkg.json 에 glm 합류 완료.
그러나 아래가 미완이라 빌드 불가 상태였다.

| # | 미완 잔재 | 증상 | 수정 | 처리 |
|---|---|---|---|---|
| a | `vmath::` 자유함수 13파일 잔존 | undeclared (vmath.h 를 include 하는 파일이 0개였음) | `glm::` 동일 함수로 치환 | 스크립트 |
| b | `glm::mat4::identity()` 4곳 | **컴파일 불가** (glm 엔 정적 identity() 없음) | `glm::mat4(1.0f)` | 스크립트 |
| c | `vmath::translate/scale/rotate/perspective/lookat` (행렬 빌더) | 시그니처/단위 불일치 | 아래 2장 표대로 수동 변환 | 수동 |
| d | `<glm/gtc/matrix_transform.hpp>` 헤더 누락 | translate/rotate/scale/perspective/lookAt 미선언 | transform.h, camera.cpp 에 추가 | 수동 |
| e | `program_uniforms.cpp` 의 `(const float*)glm::mat4` 캐스트 4곳 | **컴파일 불가** (glm 암시적 float* 변환 없음) | `glm::value_ptr()` + `<glm/gtc/type_ptr.hpp>` | 수동 (서브에이전트 발견) |
| f | `sprite_component.cpp:43` `vmath::radians` | **컴파일 불가** (unstaged 스코프 밖이라 1차에서 누락) | `glm::radians` | 수동 (서브에이전트 발견) |
| g | vmath 를 가리키는 stale 주석 13곳 | 문서 오류 (빌드엔 무해) | glm 기준으로 갱신 | 수동 |

> 중요: 이 working tree 는 **이미 컴파일이 안 되는 상태**였다(아무 파일도 vmath.h 를 include 하지 않는데
> `vmath::` 심볼을 쓰고 있었음 = 미완 마이그레이션). 따라서 b/e/f 는 "내가 깨뜨린" 게 아니라
> "사용자가 마저 못 끝낸" 부분을 완성한 것이다.

---

## 2. vmath vs glm 결정적 차이 - 왜 단일 Python 으로 안 됐나

**핵심 논지: glm 의 차이는 어휘적(lexical token)이 아니라 타입/의미적(semantic)이다.**
Python 스크립트는 텍스트 토큰만 본다. 그래서 안전하게 자동화 가능한 것은
"시그니처/단위/반환형이 vmath 와 *완전히 동일* 한 1:1 매핑" 뿐이고, 나머지는 전부 컴파일러(타입 시스템)가
개입해야만 드러나거나 고칠 수 있다.

### 2.1 자동화 가능했던 것 (스크립트 allowlist)

| 종류 | 매핑 | 안전한 이유 |
|---|---|---|
| 자유함수 | `vmath::{normalize,dot,cross,length,radians,degrees,max}` -> `glm::동일` | 시그니처/단위/반환형 동일. 순수 텍스트 치환 |
| identity | `glm::matN::identity()` -> `glm::matN(1.0f)` | 1:1 패턴, 문맥 무관 |

스크립트 실측 치환량: normalize 8, radians 8, dot 5, length 3, max 2, cross 1, degrees 1, identity 4.

### 2.2 자동화가 불가능했던 4가지 차이 (= 단일 스크립트가 막힌 지점)

1. **시그니처 차이** - `glm::translate/rotate/scale` 은 *기존 행렬을 1번째 인자* 로 받는다.
   `vmath::translate(v)` -> `glm::translate(glm::mat4(1.0f), v)`. 인자 개수가 바뀌므로 단순 치환 불가.

2. **단위 차이** - `glm::rotate`/`glm::perspective` 는 **radian**, vmath 는 **degree**.
   `glm::radians(deg)` 로 감싸야 한다. 안 감싸도 *컴파일은 되지만 화면이 망가진다* (조용한 런타임 버그).
   토큰만 보는 스크립트는 "이 각도가 degree 인지" 의미를 모른다.

3. **암시적 `float*` 변환 제거 - 가장 결정적.**
   vmath 의 vec/mat 은 `operator const T*()` 가 있어 `glUniformMatrix4fv(.., (const float*)mat)`,
   `memcpy(.., mat)` 같은 C-API 전달이 암시적으로 동작했다. **glm 엔 그 연산자가 없다.**
   결정타: 이 파손 지점에는 **`vmath` 토큰이 단 한 글자도 없다** (`(const float*)m4`).
   따라서 `grep vmath` 로는 영원히 못 찾고, 오직 컴파일러의 타입 검사만 잡아낸다.
   -> `glm::value_ptr(x)` (`<glm/gtc/type_ptr.hpp>`) 또는 `&m[0][0]` 로 명시 변환 필요.

4. **헤더 의존 분화** - vmath 는 단일 헤더였지만 glm 의 행렬변환은 `<glm/gtc/matrix_transform.hpp>`,
   포인터는 `<glm/gtc/type_ptr.hpp>` 로 분리돼 있다. 어떤 파일에 어떤 헤더가 필요한지는
   "그 파일이 무슨 함수를 쓰는가"를 파싱해야 알 수 있다 (텍스트 치환의 범위 밖).

### 2.3 결론

> 안전한 1:1 은 스크립트로, **시그니처/단위/암시적변환/헤더가 걸린 곳은 컴파일러-인-더-루프**
> (사람 또는 빌드를 돌리는 에이전트)로 처리한다. 특히 (3)번은 "텍스트 검색 자가검증"의 구조적 사각지대다.

---

## 3. 서브에이전트로 발견한 버그 + 수정 회고

### 3.1 검증 설계 (안티게이밍)

내가 만든 변경을 내가 검증하면 같은 사각지대를 공유한다. 그래서 **독립 Sonnet 서브에이전트 3개**를
역할 분리하여 적대적("깨지는 곳을 찾아라", 불확실하면 ISSUE)으로 병렬 검증했다.
그라운드 트루스 = **추정이 아니라 실제 컴파일되는 glm 1.0.3 헤더 + 실제 빌드 실행**.

| 에이전트 | 담당 축 | 판정 | 핵심 산출 |
|---|---|---|---|
| 1 | 행렬 곱 순서/시맨틱 | PASS | vmath/glm operator* 수식 동치, m[col][row] 규약 동일, 클립공간 RH_NO. `P*P^-1=I` 수치검증 |
| 2 | 시그니처/단위 표 전수 | FAIL | **버그 B** (sprite_component vmath::radians) + bare normalize RISK |
| 3 | 잠재(value_ptr/memcpy) | FAIL | **버그 A** (program_uniforms 캐스트) - 직접 빌드로 확증 |

### 3.2 버그 A - GL uniform 업로드의 암시적 캐스트 (확정 컴파일 에러)

- 위치: `src/program/program_uniforms.cpp` 53/63/73/83 (`SetMat4/SetVec4/SetVec3/SetVec2`)
- 코드: `glUniformMatrix4fv(loc, 1, GL_FALSE, (const float*)m4);` 외 3종
- 증상 (에이전트3 실제 빌드 로그):
  `error: cannot cast from type 'const glm::mat4' ... to pointer type 'const float *'`
- 원인: 위 2.2-(3). 타입은 `glm::mat4` 로 바뀌었는데 캐스트 표현식은 vmath 의 암시적 변환에 의존한 채였다.
- 수정:
  ```cpp
  // before: glUniformMatrix4fv(loc, 1, GL_FALSE, (const float*)m4);
  // after : glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m4));
  ```
  파일 상단에 `#include <glm/gtc/type_ptr.hpp>` 추가.
- 왜 1차에서 놓쳤나: 나는 `value_ptr` 부재만 grep 했고 *암시적 캐스트에 의존하던 4줄* 은 못 봤다.
  토큰엔 vmath 가 없어서다. **엔진이 GL 업로드를 이 한 파일로 단일화**해둔 덕에 수정은 4줄로 끝났다.
- 안전 확인: memcpy 경계(`VFXSystem`/`ParticleStage`)는 `&view[0][0]` 라 glm 에서 정상 동작 (에이전트3 확인).

### 3.3 버그 B - 스코프 밖 잔존 vmath::radians (확정 컴파일 에러)

- 위치: `src/sprite/sprite_component.cpp:43`
- 코드: `Uniforms::SetFloat(*Material, "uRoll", vmath::radians(rollDeg));`
- 원인: 이 파일은 내 1차 작업 스코프("unstaged 변경분")에 없었다. include 체인 어디에도 vmath.h 가 없어
  `vmath` 미선언 -> 컴파일 에러. 에이전트2 가 include 체인을 추적해 발견.
- 수정: `glm::radians(rollDeg)` (헤더 추가 불필요 - `<glm/glm.hpp>` 이미 체인에 있음).

### 3.4 견고화 + 보너스 발견

- 견고화: `PlayerController.cpp:449` 의 bare `normalize(...)` (ADL 로 glm::normalize 해석되지만 brittle)
  -> `glm::normalize(...)` 로 명시화.
- 보너스(개선): 구 `vmath::lookat` 은 right 벡터 `s` 를 정규화하지 않았는데 `glm::lookAt` 은 정규화한다.
  즉 회귀가 아니라 잠재 버그가 *고쳐진* 것 (에이전트1).

### 3.5 빌드 검증 결과

수정 후 빌드 -> 마이그레이션 TU 전부 `.o` 생성 성공(program_uniforms/sprite_component/PlayerController/
light_ubo_uploader/compound_actor/light). 즉 버그 A/B 해소가 빌드로 입증됨.

빌드가 멈춘 **유일한 에러는 vmath 무관**:
`main.cpp:174: no member named 'CreateStageActor' in namespace 'TopdownShooter::Stage'`
-> `CreateStageActor` 는 `apps/_MyApp_/src/Stage/StageBuilder.cpp:152` 에 *존재* 하나 main.cpp 가
다른 네임스페이스로 찾는 **네임스페이스 불일치** = 사용자의 Stage 리팩토링 미완분. (별도 작업)

---

## 4. transpose 우려 - 최종 판정: 비-이슈

처음 우려: "vmath 와 glm 이 transpose 관계라 행렬 곱이 반대일 수 있다."
재검증 결과 **그렇지 않다**:

- vmath/glm 둘 다 **column-major**, `operator*` 수식 동치, `m[col][row]` 인덱싱 규약 동일.
- 따라서 `T*R*S`, `Rz*Ry*Rx` 합성 순서가 그대로 보존된다.
- working tree 에 `vec*mat`(행벡터) 곱이 없음 (PlayerController 는 dot 분해, camera 는 명시 인덱싱).
- 클립공간: `GLM_FORCE_DEPTH_ZERO_TO_ONE`/`GLM_FORCE_LEFT_HANDED` 미정의 -> 기본 RH_NO(-1..1) = OpenGL 일치.
- 수치검증: `P * P^-1 = I` (perspective/ortho 모두) 통과.

즉 단위(degree->radian)와 시그니처(identity 1번째 인자)만 맞추면 결과 행렬이 수학적으로 동일하다.

---

## 5. 남은 작업 / 이어서 할 때

1. **unstaged 밖 / 다른 브랜치의 vmath 잔재**: 이번엔 unstaged 만 했다. 다른 커밋/브랜치에 vmath:: 가 더 있으면
   동일 스크립트(`scripts/migrate_vmath_to_glm.py`)로 안전분 처리 후, 위 2.2 함정표대로 수동분 처리.
   **반드시 빌드를 돌려라** (2.2-(3) 캐스트 파손은 grep 으로 안 잡힌다).
2. **무관 선재 에러 `CreateStageActor`**: 빌드 최종 링크를 막는다. vmath 와 무관한 Stage 네임스페이스 작업.
   먼저 해결돼야 `_MyApp_` 가 완전히 링크된다.
3. **육안 검증**: 빌드 GREEN 후, FOV/회전이 degree->radian 으로 바뀐 곳(카메라, 빌보드 roll, 손 spread)이
   화면에서 정상인지 확인 (단위 실수가 있었다면 여기서 드러난다).

### 가드레일 (이 프로젝트 컨벤션)

- 빌드는 사용자가 직접 (working tree 병렬 작업/staging 존재). 커밋도 사용자 판단.
- 커밋 시 `git commit <경로>` partial 만 (인덱스 전체 `git add -A` 금지 - 병렬 작업 휩쓺).
- 주석/문서는 한국어 + ASCII(특수문자 지양, `replace_special_chars.py` 컨벤션).
- `include/vmath.h`, `extern/sb7code` 등 vendored 는 수정 금지 (의존성 충돌은 항상 다른 쪽에서 해결).

---

## 6. 후속 개선 - 손으로 짠 행렬 코드를 glm 빌트인으로 축약 (camera.cpp)

vmath 시절엔 제공 함수가 없어 `camera.cpp` 가 ortho/affine-inverse/inverse-projection 을
주먹구구로 직접 구현했었다. glm 도입으로 전부 빌트인 1줄로 대체했다 (동작 보존, 컴파일 + 수치 동등성 검증).
**camera.cpp 138 -> 79 줄 (-59).** 커밋 `2a2e659` 에 포함.

| 구 (hand-rolled) | 신 (glm 빌트인) | 비고 |
|---|---|---|
| ortho 행렬 6원소 직접 대입 (14줄) | `glm::ortho(-halfW, halfW, -OrthoSize, OrthoSize, NearZ, FarZ)` | glm orthoRH_NO 공식이 원소 동일 |
| `InverseAffine()` 헬퍼 (회전 transpose + `-R^T t`, 20줄) | `glm::affineInverse(ownerW)` | scale=1 이면 동일, scale!=1 이면 *오히려 더 정확* (3x3 일반 역). 헬퍼/헤더 선언 제거 |
| 역투영 닫힌 해 (ortho/perspective 분기, 28줄) | `glm::inverse(GetProjectionMatrix())` | 항상 참 역행렬. self-maintaining (투영식 변경 시 자동 추종) |
| `glm::vec3(m[3][0], m[3][1], m[3][2])` | `glm::vec3(m[3])` | vec4 -> vec3 절삭 생성자 |
| `#include <cmath>` (std::tan 용) | 제거 | 닫힌 해 삭제로 std::tan 미사용 |
| `InverseAffine()` 선언 (camera.h) | 제거 | 단일 호출처라 GetViewMatrix 에 인라인 |

주의 (정확성): **뷰 행렬엔 `affineInverse`, 투영 역행렬엔 `inverse`** 로 구분해야 한다.
perspective 투영은 비-affine(w-row != [0,0,0,1]) 이라 `affineInverse` 를 쓰면 *틀린다*.
헤더: `glm::affineInverse` = `<glm/gtc/matrix_inverse.hpp>`, `glm::inverse` = `<glm/glm.hpp>`(matrix.hpp).

수치 동등성 검증(numpy): `affineInverse==구 InverseAffine` / `glm::ortho==구 ortho` /
`glm::inverse(P)==구 closed-form` 모두 `allclose True`. + scene 라이브러리 컴파일 GREEN.

교훈: "glm 함수가 없어서 직접 구현했다" 류의 주석이 붙은 손-구현 행렬 코드는 glm 도입 후 빌트인
대체 후보다. 단 affine/비-affine 구분과 헤더만 주의.

## 부록 A. 스크립트 사용법 (`scripts/migrate_vmath_to_glm.py`)

```bash
python3 scripts/migrate_vmath_to_glm.py            # git unstaged 변경파일 dry-run (미리보기)
python3 scripts/migrate_vmath_to_glm.py --apply    # 실제 기록
python3 scripts/migrate_vmath_to_glm.py src apps --apply   # 지정 경로
```

- 안전 1:1(7함수 + identity)만 allowlist 치환. **행렬 빌더(translate/scale/rotate/perspective/lookat)는
  절대 건드리지 않고 "수동 대상" 으로 잔존 리포트** 한다 (silent 오변환 방지).
- 기본 dry-run, `--apply` 로 기록. 치환 후 남은 `vmath::` 를 file:line 으로 보고.

## 부록 B. vmath -> glm 변환 함정표 (수동분)

| vmath | glm | 차이 | 헤더 |
|---|---|---|---|
| `translate(v)` | `glm::translate(glm::mat4(1.0f), v)` | 행렬 1st 인자 | `<glm/gtc/matrix_transform.hpp>` |
| `scale(v)` | `glm::scale(glm::mat4(1.0f), v)` | 행렬 1st 인자 | 동상 |
| `rotate(deg,x,y,z)` | `glm::rotate(glm::mat4(1.0f), glm::radians(deg), glm::vec3(x,y,z))` | 행렬 + **radian** | 동상 |
| `perspective(fovDeg,..)` | `glm::perspective(glm::radians(fovDeg),..)` | **radian** | 동상 |
| `lookat` | `glm::lookAt` | 대문자 A (+ s 정규화 차이) | 동상 |
| `mat4::identity()` | `glm::mat4(1.0f)` | glm 엔 정적 메서드 없음 | `<glm/glm.hpp>` |
| `(const float*)mat` (암시적) | `glm::value_ptr(mat)` | **암시적 변환 제거 - grep 불가** | `<glm/gtc/type_ptr.hpp>` |
| `mat.transpose()` (멤버) | `glm::transpose(mat)` (자유함수) | 멤버->자유함수 | `<glm/glm.hpp>` |
