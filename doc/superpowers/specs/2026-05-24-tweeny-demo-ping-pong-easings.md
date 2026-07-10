# tweeny_demo — 11 Easing Ping-Pong Planes — 설계 스펙

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 작성일: 2026-05-24

## 목표

신규 챕터 `apps/tweeny_demo/` 를 추가해 **Tweeny 의 11개 대표 easing 함수** 를 좌↔우 ping-pong 으로 시연한다. 화면에 11개의 단색 plane 을 세로로 스택해, 각 행이 자기 고유의 easing 곡선으로 움직이는 모습을 나란히 비교할 수 있게 한다.

학습 목적의 single-file 챕터 — `sb7::application` + Tweeny 한 파일 안에서 라이프사이클 완결. macOS(Ninja) + Windows(MSVC) 양쪽에서 빌드·실행되어야 한다.

## 배경 / 제약

- Tweeny 는 `cmake/Dependency.cmake` 에서 `tweeny` INTERFACE 타겟으로 등록되어 `game_deps` 자동 합류 — 챕터는 `project_deps + game_deps` 만 링크하면 `#include "tweeny.h"` 가능.
- `<extern>/tweeny/include/easing.h` 의 `easing` 클래스 안에 31개 easing 이 `static constexpr struct ...Easing { ... }` 로 정의됨 (linear + 10 패밀리 × in/out/inOut). `tween.via(tweeny::easing::cubicInOut)` 형식으로 주입.
- 본 데모는 **2D 단색 quad 11개** 만 렌더 — 텍스처 / depth / blend / SceneRenderer / Camera / ImGui 전부 불필요.
- GL 정책 — GLSL 410 [[glsl_410_project_policy]]. 기존 `apps/tweeny_demo/resources/shader/simple.fs` 는 `#version 330 core` 라 410 으로 패치 필요.
- 본 챕터의 skeleton (`<apps>/tweeny_demo/main.cpp` 현 상태) 은 SceneRenderer/PostFX/ImGui 보일러플레이트가 들어 있지만 실제 학습 목적(easing 비교)에 과합 — 전면 재작성으로 가닥.
- `apps/CMakeLists.txt` 에 `add_subdirectory(tweeny_demo)` 가 빠져 있음 (현재 비등록) — 이번 작업에서 등록.

## 결정 사항

| 항목 | 결정 |
|---|---|
| 챕터 위치 | `apps/tweeny_demo/` (디렉토리 존재, main.cpp 전면 재작성) |
| 챕터 패턴 | 패턴 B — single `main.cpp` + `DECLARE_MAIN` |
| 구조 | 엔진 최소 — `project_deps + game_deps + SJH::engine` link, 실제 호출은 `SJH::program` / `SJH::shader` 위주 |
| Easing 세트 | 11개 InOut 대표 — `linear`, `quadraticInOut`, `cubicInOut`, `quarticInOut`, `quinticInOut`, `sinusoidalInOut`, `exponentialInOut`, `circularInOut`, `bounceInOut`, `elasticInOut`, `backInOut` |
| 재생 모드 | Ping-pong — `forward` 플래그로 `step(±dtMs)` 방향 토글, `progress()` 양 끝 도달 시 반전 |
| Tween 범위 | X ∈ `[-0.85, +0.85]` (NDC), 한 패스 2000 ms |
| 레이아웃 | 2D Ortho — NDC 직접 사용, projection 행렬 없음. 11행 y 분포 `lerp(0.9, -0.9, i/10)` |
| Plane 크기 | `uScale = vec2(0.05, 0.035)` — 행 폭 ~ 화면 5% × 높이 3.5% |
| 색상 | plane 당 1회 RGB 랜덤 (alpha=1). `std::mt19937{42}` 고정 시드 → 결정적 |
| Geometry | 단위 quad 4-vertex `TRIANGLE_STRIP` — `SJH::Object::PlaneGeometry` 존재 시 사용, 미존재 시 인라인 VBO/VAO fallback |
| Draw 전략 | VAO 1회 bind + 11회 uniform 갱신 + 11회 `glDrawArrays` (instancing 미사용 — 학습 가시성) |
| 셰이더 | `simple.vs` 신규 + `simple.fs` 기존(330 → 410 패치). 둘 다 `#version 410 core` |
| Uniforms | `uOffset: vec2 (tweenX, rowY)`, `uScale: vec2`, `baseColor: vec4` |
| GL 정책 | GL 4.1 Core / GLSL 410 (`init()` override 에서 강제) — skeleton 의 `info.majorVersion = 4 / minorVersion = 1` 유지 |
| 활성화 | `apps/CMakeLists.txt` 에 `add_subdirectory(tweeny_demo)` 추가 (현재 활성 3개 옆에 — "한 번에 하나" 컨벤션 완화) |
| ImGui / SceneRenderer / Camera | **컷 (YAGNI)** — 단색 quad 11개에 과함 |
| Actor / Component | **컷 (YAGNI)** — 11행이 평면 컬렉션이라 `EasingRow` POD 로 충분 |

## 파일 레이아웃

```
apps/tweeny_demo/
├── CMakeLists.txt              # 신규 — box2d_demo 패턴 복제 (POST_BUILD 리소스 copy)
├── main.cpp                    # 전면 재작성 — single file
└── resources/shader/
    ├── simple.vs               # 신규 — ortho + uOffset + uScale
    └── simple.fs               # 기존 → #version 330 → 410 패치만
```

## 데이터 구조

```cpp
struct EasingRow {
    const char*          name;     // "linear", "cubicInOut", ...
    tweeny::tween<float> tween;    // X 좌표 (스칼라) 만 tween
    vmath::vec4          color;    // baseColor uniform 값
    float                y;        // 행 위치 (시작 시 고정)
    bool                 forward;  // ping-pong 방향. true → +dtMs, false → -dtMs
};
```

생성 시점 (startup) — 11개의 `EasingRow` 를 `std::vector<EasingRow>` 에 push_back. 각 entry 는 `tweeny::from(-0.85f).to(0.85f).during(2000).via(<easing>)` 로 초기화.

## 렌더 루프 의사 코드

```cpp
void render(double currentTime) {
    static double prevTime = currentTime;
    float dtMs = static_cast<float>((currentTime - prevTime) * 1000.0);
    prevTime = currentTime;

    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    mProgram->Use();
    mVAO->Bind();
    for (auto& row : mRows) {
        float x = row.forward ? row.tween.step(dtMs) : row.tween.step(-dtMs);
        if      ( row.forward && row.tween.progress() >= 1.0f) row.forward = false;
        else if (!row.forward && row.tween.progress() <= 0.0f) row.forward = true;

        mProgram->SetUniform("uOffset",   vmath::vec2(x, row.y));
        mProgram->SetUniform("uScale",    vmath::vec2(0.05f, 0.035f));
        mProgram->SetUniform("baseColor", row.color);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}
```

> `SJH::Program::SetUniform(vec2)` 시그니처는 구현 단계에서 확인 — 없으면 `program_uniforms.h` 의 자유 함수 family (`Uniforms::Set*`) 로 대체.

## CMake

`apps/tweeny_demo/CMakeLists.txt`:
```cmake
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_executable(${CHAPTER_NAME} main.cpp)
target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    game_deps
    SJH::engine
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/resources"
                "$<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources"
        VERBATIM)
endif()
```

`apps/CMakeLists.txt` — 한 줄 추가:
```cmake
add_subdirectory(tweeny_demo)
```

## 빌드 / 실행

```bash
cmake --preset ninja
cmake --build --preset ninja --target tweeny_demo
cd build_ninja/apps/tweeny_demo && ./tweeny_demo
```

## 검증

수동 검증 (자동화된 골든 이미지 미사용 — easing 곡선 비교가 본질적으로 시각적):

1. **빌드** — Ninja Debug 빌드 무에러 (`-Werror` 통과). MSVC 는 본 세션 범위 밖이지만 windows.h / `long` / 백슬래시 위반 없도록 작성.
2. **시각** — 11행 plane 이 각자 다른 곡선으로 좌↔우 ping-pong. linear 가 등속, bounce/elastic 행이 가장 눈에 띄게 튐, sinusoidal/quadratic 은 매끈.
3. **결정성** — RNG seed=42 고정으로 매 실행 동일한 11색.
4. **종료** — ESC 또는 창 닫기로 정상 종료, GL 에러 / 누수 없음.

## 위험 / 가정 (PreMortem)

1. **`PlaneGeometry` 존재 미확인** — `src/object/geometry.h` 가 노출하는 형식이 다르면 인라인 `glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(vec2), ...)` fallback. 첫 implementation step 에서 헤더 검사 후 분기.
2. **`SJH::Program::SetUniform(vec2)` 시그니처 미확인** — 없으면 `program_uniforms.h` 의 자유 함수 (`Uniforms::Set2f` 등) 로 즉시 대체.
3. **tweeny `tween<float>` 컨테이너 — 11개 다른 easing 보관 가능성** — tweeny 의 `via()` 는 *tween point 마다* easing 설정. 각 `EasingRow` 가 자기 `tween` 객체를 갖고 vector 에 push_back 하므로 컨테이너 차원 충돌 없음.
4. **NDC 직접 사용 시 종횡비 왜곡** — 창 크기 800×600 에서 `uScale=(0.05, 0.035)` 의 실제 픽셀 폭이 ~40×21 px. 작아도 시각적 비교 가능. 종횡비 보정은 `uScale.x *= height/width` 로 추후 추가 가능 (YAGNI 컷).
5. **본 작업의 진정한 미니멀 의존성** — `SJH::engine` 우산 link 는 12개 모듈을 모두 포함하지만 static link 라 미사용 심볼은 prune. 빌드 시간 오버헤드만 약간 (수용 가능).

## YAGNI 컷 (의도된 비포함)

- ImGui 토글 / 슬라이더
- SceneRenderer / Camera / Actor 그래프
- PostFX / FBO
- 텍스처 / Material
- Instancing (`glDrawArraysInstanced`)
- Audio / Box2D / Effekseer
- 골든 이미지 (`golden-capture` 스킬 — easing 곡선은 시각 검증으로 충분)

## 후속 작업 (별도 세션)

- 본 데모를 ImGui 슬라이더로 easing 동적 교체 가능하게 확장 (별도 챕터 또는 v2)
- 같은 패턴으로 multi-point tween (`from(a).to(b).to(c).during(...)`) 데모 추가
