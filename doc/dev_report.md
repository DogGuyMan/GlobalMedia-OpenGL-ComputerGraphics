# game/main 개발 보고서 — 게임 엔진/데모 전환기

**대상 브랜치:** `game/main`
**보고 기간:** 2026-05-19 ~ 2026-06-10 (16 작업일)
**대상 커밋:** 243건 (first-parent 기준)
**프로젝트:** OpenGL Computer Graphics (C++17 / CMake)

## 1. 개요

본 보고서는 OpenGL Computer Graphics 프로젝트의 `game/main` 브랜치에서 수행된 일일 개발·리팩토링·수정 내역을 정리한 것이다. 해당 기간 동안 프로젝트는 SuperBible 7th Edition 코스워크 기반에서 출발하여, Core/Client 분리 + Actor/Component 씬 그래프 + SceneRenderer를 갖춘 자체 게임 엔진과 탑다운 슈터 데모(`_MyApp_`)로 전환되었다. 엔진은 `SJH::<module>` STATIC 라이브러리 17종과 이를 묶은 `SJH::engine` INTERFACE 우산 타겟으로 구성된다.

## 2. 작성 방법

`scripts/traversal_git.py`로 `game/main`의 first-parent 커밋 243건(시작 커밋 `28fbc73 [init] : game project`)을 시간순으로 추출하여 1차 사료(`doc/work_history.md` — 커밋 메시지 + `.cpp`/`.h` 코드 diff 발췌)를 생성하고, 이를 1일 단위로 통합하여 본 보고서를 작성하였다. 각 일자는 다음 고정 섹션으로 구성된다.

- **개요** — 그날 작업의 목적·맥락·핵심 성과
- **개발 (Feature)** — 신규 구현 기능
- **리팩토링** — 구조 개선
- **수정 (Fix / Chore)** — 버그·빌드·정리
- **기술적 결정 및 이슈** — 설계 결정·트레이드오프·함정
- **산출물** — 변경 모듈 및 커밋 요약

## 3. 마일스톤 개관

| 마일스톤 | 내용 | 주요 일자 |
|---|---|---|
| 엔진 리팩토링 (SP1~SP6) | RAII 자원 · RenderContext · DeviceContext · 씬 그래프 · 렌더 파이프라인 | 05-19 ~ 05-23, 05-27 |
| M1 | 스프라이트/아틀라스 (`SJH::sprite`) | 05-24 |
| M2 | 오브젝트 상태 기계 (`SJH::fsm`) | 05-25 |
| M3 | Box2D v2.4.1 물리 통합 | 05-25 |
| M3.5 | Playable 코어 (`SJH::playable`) | 05-26 |
| M4 | 도메인 (Stat / Entity 컴포넌트) | 06-01, 06-03 |
| M5 | Effekseer / FMOD leaf Playable | 05-26, 05-31 |
| M6 | 단발 FX 스폰 인프라 (`Spawns`) | 05-31 |
| M7 | Stage FSM (Title/CombatPlay/Pause/GameOver) | 06-04 |
| 부가 | World Text (`SJH::text`) · Timer (`SJH::timer`) · Doxygen 문서화 | 06-01, 06-02, 06-10 |

## 4. 일자별 목차

- [2026-05-19 (화)](#2026-05-19-화) — 게임 엔진 기반 수립: 외부 라이브러리 의존성 등록 + 코어 모듈 마이그레이션
- [2026-05-20 (수)](#2026-05-20-수) — SP1 RAII 완결 + SP2 RenderContext 설계·구현 + ImGui/Catch2 인프라 정착
- [2026-05-21 (목)](#2026-05-21-목) — Actor+Component 씬 그래프 설계 완성 및 렌더 파이프라인 구조화
- [2026-05-22 (금)](#2026-05-22-금) — `migrate_demo` 마이그레이션 완료 및 렌더 큐 스텐실·Pass 확장
- [2026-05-23 (토)](#2026-05-23-토) — 렌더 파이프라인 책임 분할 및 FrameBuffer Resource화 리팩토링
- [2026-05-24 (일)](#2026-05-24-일) — `SJH::sprite` 모듈 신설 및 보조 데모 확장
- [2026-05-25 (월)](#2026-05-25-월) — M2 FSM 코어 신설 + M3 Box2D 물리 통합 + SP5 렌더 스테이지 정착
- [2026-05-26 (화)](#2026-05-26-화) — M3.5 Playable 코어 정착 + SceneContext 자동 등록 + M5 leaf 배선 선행
- [2026-05-27 (수)](#2026-05-27-수) — PostFX 2-카메라 아키텍처 전환 및 ImGui 레이어 분리
- [2026-05-28 (목)](#2026-05-28-목) — 매트릭스 스카이박스·PCB 모델 도입 및 스테이지 리소스 교체
- [2026-05-31 (일)](#2026-05-31-일) — 렌더 파이프라인 추출·스카이박스 통합·M6 단발 FX 스폰 인프라 구축
- [2026-06-01 (월)](#2026-06-01-월) — `SJH::timer` 코어 승격과 PlayerBehavior 분해 선행 인터페이스 정착
- [2026-06-02 (화)](#2026-06-02-화) — World Text(`SJH::text`) 코어 모듈 신설 및 엔진 17모듈 완성
- [2026-06-03 (수)](#2026-06-03-수) — PlayerBehavior 분해 완료 + Timer 중앙화 + 대규모 리팩토링
- [2026-06-04 (목)](#2026-06-04-목) — Stage FSM 완성 + 적 생사 관리 재설계 + 궁극기/데미지 텍스트 구현
- [2026-06-10 (수)](#2026-06-10-수) — 전체 코드베이스 Doxygen 주석 전면 정비 및 제출 준비

---

## 2026-05-19 (화)

**게임 엔진 기반 수립: 외부 라이브러리 의존성 등록 + 코어 모듈 마이그레이션**

### 개요

본일은 OpenGL 코스워크 브랜치에서 `game/main` 브랜치를 분기하여 탑다운 슈터 게임 프로젝트(`_MyApp_`)를 본격적으로 착수한 날이다. 작업은 크게 두 축으로 진행되었다. 첫째, 게임 엔진에 필요한 외부 라이브러리 7종(Box2D, Effekseer, entt, Tweeny, stb, assimp, spdlog)을 Git 서브모듈로 등록하고 CMake `game_deps` INTERFACE 타겟으로 묶는 의존성 인프라를 구축하였다. 둘째, 기존 엔진 코드를 `src/` 모듈 체계(buffer, common, diagnostics, input, layout, material, object 등)로 마이그레이션하고, `SJH::Geometry` 도형 생성기 어댑터를 신규 추가한 후 `Mesh` 팩토리를 이에 위임하는 리팩토링을 수행하였다. 총 10개 커밋, +3979 / -1229 줄 규모의 대규모 변경으로 엔진/게임 데모 전체의 빌드 기반이 확립된 날이다.

### 개발 (Feature)

**외부 라이브러리 의존성 등록 (`c6563ec`)**

Box2D v2.4.1, Effekseer 1.7.3.0, entt, Tweeny, stb, assimp v5.4.3, spdlog v1.17.0 을 `extern/` 하위 Git 서브모듈로 등록하고 `cmake/Dependency.cmake` 에 `game_deps` INTERFACE 타겟을 신설하였다. 컴파일이 필요한 라이브러리(Box2D, Effekseer, assimp, spdlog)는 macOS/Windows 사전 빌드 정적 라이브러리를 `IMPORTED STATIC` 타겟으로 등록하고, 헤더 전용 라이브러리(Tweeny, stb)는 `include/` 경로만 노출하는 `INTERFACE` 타겟으로 처리하였다. `game_deps` 의 서드파티 헤더 경로는 `SYSTEM` 인클루드로 지정하여 Debug 빌드의 `-Werror` 가 서드파티 헤더를 대상으로 적용되지 않도록 격리하였다. 등록 완료 후 검증용 임시 타겟 `_deptest_` 를 통해 각 라이브러리의 링크 및 헤더 포함 가능 여부를 `startup()` 내 최소 호출로 확인하고 비활성화하였다.

**`SJH::Geometry` 도형 데이터 생성기 신설 (`ded83f0`)**

`src/object/geometry.h/.cpp` 에 `SJH::Geometry` 네임스페이스를 신설하고, Box / Plane / Cone / Tetrahedron / Octahedron / Disk / Cylinder / HemiSphere 8개 도형 생성 함수를 제공하였다. 내부적으로는 기존 `Engine::Model` 빌더(13-float interleaved 출력)를 재사용하고, 이를 `SJH::Vertex`(position + normal + texCoord) + 인덱스 쌍인 `MeshData` 구조체로 변환하는 내부 함수 `FromInterleaved`를 통해 어댑터 패턴을 구현하였다. 도형의 배치(offset)는 `Transform` 책임으로 두고, Winding 반전만 `back_face` 파라미터로 노출하는 최소 인터페이스를 유지하였다.

**엔진 코어 모듈 마이그레이션 (`0c1849e`)**

`Buffer` / `Framebuffer` / `Shader` / `Program` / `VertexLayout` / `Material` / `Mesh` / `ResourceRegistry` 등 엔진 핵심 클래스들을 `src/<module>/` 디렉토리 구조로 이식하였다. `CLASS_PTR` 매크로(`common.h`)로 `UPtr` / `Ptr` / `WPtr` 스마트 포인터 별칭을 일괄 생성하는 공통 패턴을 적용하였으며, 각 클래스의 헤더에 Doxygen 문서 주석을 병행 작성하였다. `SJH::Const` 네임스페이스(`constants.h`)에 정점 레이아웃 크기 상수, UV/위치 리터럴, 인덱스 배열 등 전역 리터럴을 집약하였다.

### 리팩토링

**`Vertex` 구조체 헤더 분리 (`1ceab3a`)**

`mesh.h` 에 인라인으로 정의되어 있던 `struct Vertex` 를 `src/object/vertex.h` 로 추출하였다. 분리 동기는 헤더 의존성 정리로, `vertex.h` 는 `vmath.h` 만 의존하고 GL 로더(`gl3w`/`glad`)를 포함하지 않아 `geometry.cpp` 등 향후 어느 번역 단위에서든 충돌 없이 포함할 수 있게 되었다.

**`Mesh::CreateBox` / `CreatePlane` 의 데이터 생성 위임 (`036429d`)**

두 팩토리 메서드에 하드코딩되어 있던 정점 리터럴(Box 24정점/36인덱스, Plane 4정점/6인덱스)과 인덱스 배열을 전부 제거하고, `Geometry::Box()` / `Geometry::Plane()` 호출 결과인 `MeshData` 를 소비하도록 단순화하였다. 이후 `Mesh` 클래스는 데이터 생성 책임을 완전히 `SJH::Geometry` 에 위임하고, GL 업로드 및 VAO 구성에만 집중하게 되었다.

### 수정 (Fix / Chore)

**`EffekseerRendererGL` 헤더 인클루드 경로 수정 (`c21261d`):** `EffekseerRendererGL.h` 가 내부적으로 `<Effekseer.h>` 를 포함하는데, `include/Effekseer` 경로가 `INTERFACE_INCLUDE_DIRECTORIES` 에 누락되어 있어 컴파일이 실패하였다. `Effekseer` 와 `EffekseerRendererGL` 양쪽 타겟 모두에 해당 경로를 추가하고, `_MyApp_` 가 `game_deps` 를 링크하도록 수정하여 해소하였다.

**`assimp` 서브모듈 in-place 패치 원복 (`c6563ec` 일부):** `BuildExternLibs.sh` 빌드 스크립트가 `extern/assimp` 를 in-place 패치한 채 종료하여 `m extern/assimp` 오염이 발생하던 문제를 수정하였다. 빌드 직후 `git checkout` 으로 서브모듈을 원복하도록 변경하였으며, 패치는 멱등성을 갖추어 다음 실행 시 자동 재적용되도록 하였다.

### 기술적 결정 및 이슈

**`game_deps` SYSTEM 인클루드 격리 설계:** Debug 빌드에서 `-Werror` 를 활성화한 상태로 서드파티 헤더를 포함하면, Effekseer 등 외부 헤더의 경고가 빌드 실패로 이어진다. 이를 해결하기 위해 `game_deps` 의 `include/` 디렉토리를 `target_include_directories(game_deps SYSTEM INTERFACE ...)` 로 지정하여 서드파티 헤더를 진단 대상에서 제외하였다. 프로젝트 자체 코드만 `-Werror` 검사를 받는 이중 정책이 확립된 셈이다.

**`SJH::Geometry` 어댑터 계층 도입 이유:** `Engine::Model` 빌더는 13-float interleaved 포맷(pos4 + color4 + normal3 + uv2)을 출력하는 레거시 인터페이스이다. 이 포맷을 직접 `Mesh` 에 노출하면 color 채널이 엔진 컨벤션(`SJH::Vertex` 에는 color 없음)과 충돌한다. `Geometry` 어댑터가 color 4개 float를 무음으로 폐기하고 `SJH::Vertex` 로 변환함으로써, 기존 빌더를 재작성하지 않고도 새 모듈 체계와 정합성을 유지하였다.

**`_deptest_` 임시 타겟 검증 후 비활성화 전략:** 신규 라이브러리를 등록할 때마다 링크 오류를 즉시 포착하기 위해 의존성 검증 전용 실행 파일 `_deptest_` 를 활용하였다. 각 라이브러리의 헤더 포함과 최소 API 호출(Box2D `b2World::Step`, Effekseer `Manager::Create`, spdlog `info`)이 성공함을 확인한 뒤 `apps/CMakeLists.txt` 에서 주석 처리하는 방식으로, 검증 비용을 최소화하면서도 흔적을 남겨 재현 가능성을 보장하였다.

### 산출물

- 변경 모듈/영역: `cmake/Dependency.cmake`(game_deps 신설), `src/buffer`, `src/common`, `src/diagnostics`, `src/input`, `src/layout`, `src/material`, `src/object`(geometry 신설 + vertex 분리 + mesh 위임), `src/program`, `src/resource_registry`, `src/shader`, `apps/_MyApp_`, `apps/_deptest_`, `.gitmodules`(서브모듈 8종 추가)
- 커밋 10건 (주요: `c6563ec` deps 서브모듈 7종 등록, `0c1849e` 엔진 모듈 마이그레이션, `ded83f0` SJH::Geometry 신설, `036429d` Mesh 팩토리 위임, `1ceab3a` Vertex 헤더 분리, `c21261d` Effekseer 인클루드 경로 수정)

---

## 2026-05-20 (수)

**SP1 RAII 완결 + SP2 RenderContext 설계·구현 + ImGui/Catch2 인프라 정착**

### 개요

본 날은 엔진 렌더링 아키텍처 재설계 연속 작업(SP1~SP2)의 마무리와 테스트·UI 인프라 구축이 집중적으로 이루어진 날이다. SP1의 마지막 단계로 `SJH::Shader`·`SJH::Program`의 RAII 의미론 명시화와 레거시 `Engine::Program::ShaderProgram` 물리 제거를 완료하였으며, SP2로 `RenderContext` 싱글톤과 `RenderTarget` 추상 계층을 신설하여 `glUseProgram`의 소유권을 `Program::Use()`에서 `RenderContext`로 이전하였다. 병행하여 ImGui v1.53 통합(`SJH::imgui` STATIC 라이브러리)과 Catch2 v3.15.0 기반 단위 테스트 인프라를 프로젝트에 합류시켰다. 커밋 5건, 코드 순증 +4,844 / -1,433줄로 해당 주차에서 가장 규모가 큰 하루였다.

### 개발 (Feature)

**SP2 — `SJH::render` 모듈 신설 및 `RenderContext` 싱글톤 구현.**
`src/render/render_target.h/.cpp`에 `RenderTarget` 순수 가상 인터페이스(`Bind()` + `GetSize()`)와 기본 구현체 `DefaultRenderTarget`(`glBindFramebuffer(0)` + `glViewport`)을 추가하였다. `src/render/render_context.h/.cpp`에는 GL 게이트웨이 싱글톤 `RenderContext`를 구현하였다. 책임은 `mBoundProgram` 추적, `glUseProgram`/`glBindVertexArray`/`glBindTexture`/`glClear`/`glEnable*`, `glDrawElements`/`glDrawArrays`, `RenderTarget` 바인딩 위임으로 총 13개 메서드로 구성된다. 복사·이동을 `= delete`로 명시하고 `static_assert` 2종으로 싱글톤 불변식을 컴파일 타임에 검증하였다.

**`Program` uniform 캐시 멤버화(Pattern Y, A6).**
`mUniformCache: unordered_map<string, UniformEntry>`를 `Program` 멤버로 이전하고 `BuildUniformCache()` (링크 직후 active uniform 전체 eager 캐시)·`GetLocation(name) const`·`GetType(name) const`를 추가하였다. 기존 TU-local static `sCacheRegistry`와 `Uniforms::BuildCache`/`Forget` 자유 함수를 폐기하고 `Uniforms` 자유 함수 내부가 `prog.GetLocation`을 호출하도록 전환하였다. 외부 호출자 시그니처는 무수정으로 유지되었다.

**ImGui v1.53 통합.**
`extern/imgui` 서브모듈을 v1.53 태그로 핀하고, `src/imgui/CMakeLists.txt`에서 코어 3개 파일과 GLFW+OpenGL3 결합형 backend(`imgui_impl_glfw_gl3.cpp`) 1개 파일을 `SJH::imgui` STATIC 라이브러리로 인라인 컴파일하여 `project_deps`에 자동 합류시켰다. smoke test 챕터 `apps/imguitest/`를 추가하여 `ImGui::ShowDemoWindow()` 표시 및 GL 4.1 Core 정책을 검증하였다.

**Catch2 v3.15.0 단위 테스트 인프라 활성화.**
`extern/Catch2` 서브모듈을 v3.15.0으로 핀하고, 루트 `CMakeLists.txt`에 `ENABLE_TESTING` 옵션(기본 OFF)을 추가하였다. `test/` 디렉토리의 144개 단위 테스트를 프로젝트 표준(`glad→gl3w`, `spdlog::spdlog` ALIAS 등)에 맞춰 갱신하여 `ctest` 144/144 전통과를 달성하였다. 지원 헬퍼(`gl_test_fixture`, `spdlog_capture`, `gl_state_snapshot`) 3종도 신규 추가되었다.

### 리팩토링

**`Engine::Program::ShaderProgram` 물리 제거.**
`src/engine/shader_program.{h,cpp}` (총 184줄)를 삭제하였다. 해당 파일은 `src/CMakeLists.txt`에 `add_subdirectory(engine)` 자체가 없어 빌드 그래프 밖에 있던 dead code였으며 `SJH::Shader` + `SJH::Program`으로 완전 대체된 상태였다. `src/engine/constants.h`(144줄)와 `src/engine/geometry.{h,cpp}`(867줄)도 함께 제거되었고, 해당 기능은 `src/common/constants.h`와 `src/object/geometry.cpp`로 이전·흡수되었다.

**`Uniforms` 자유 함수 내부 전환 및 `Program::Use()` 제거.**
`Program::Use()`를 선언·정의에서 제거하고, 활성 테스트(`test_program_uniforms.cpp`)의 `prog->Use()` 호출을 `rc.UseProgram(*prog)`로 치환하여 `glUseProgram`의 단일 진입점 원칙을 달성하였다.

### 수정 (Fix / Chore)

**`SJH::Shader` / `SJH::Program` 복사·이동 = delete 명시.**
두 클래스는 팩토리 + `unique_ptr` 패턴으로 설계되어 있어 외부에서 복사·이동할 경로가 원래 없었으나, 암묵적 규칙에 머물러 있어 미래의 실수(`*prog` 복사 대입 등)가 `glDeleteProgram`/`glDeleteShader` 이중 호출로 이어질 위험이 있었다. 각 `.h`에 4종(`copy constructor`, `copy assign`, `move constructor`, `move assign`) `= delete`를 명시하고, 각 `.cpp`에 `static_assert` 4종으로 컴파일 타임 불변식을 보강하였다.

**`src/common/constants.h` 부동소수점 리터럴 수정.**
삼각형·사면체 정점 좌표(`0.866`, `0.816`, `0.2886` 등)가 정수형으로 추론될 수 있는 위치에 `f` 접미사를 누락하고 있었다. MSVC 및 Clang narrowing 경고 대상으로, 해당 리터럴에 `0.866f` 등 명시적 `float` 형 접미사를 추가하였다.

**`.gitignore` `.worktrees/` 추가.**
sub-agent 기반 병렬 작업 시 생성되는 격리 worktree 디렉토리를 추적 대상에서 제외하였다.

### 기술적 결정 및 이슈

**SP2 Pattern Y (A6) 선택 — uniform 캐시 소유권 Program 멤버로.**
설계 후보 중 RenderContext가 캐시를 보유하는 방안(전역 레지스트리 유지)과 Program 멤버로 이전하는 방안을 비교하였다. 후자가 CQS(Command-Query Separation) 원칙에 부합하고, 프로그램 파괴 시 캐시 소멸이 `mUniformCache` 멤버의 자동 파괴로 보장되어 `Uniforms::Forget` 수동 호출 실수를 원천 차단한다는 점에서 Pattern Y를 채택하였다. `GetLocation`/`GetType`은 pure const query로 남겨 side-effect 격리를 준수하였다.

**ImGui v1.53 버전 핀 필요성.**
`extern/sb7code`의 GLFW는 3.0.4로 고정되어 있으며 수정 금지 제약이 있다. ImGui v1.54부터 `imgui_impl_glfw`가 GLFW 3.1+ cursor API(`GLFWcursor` 등)를 무조건 참조하여 v1.53이 본 프로젝트와 호환되는 마지막 태그임을 확인하였다. 당초 v1.92.8로 추가된 서브모듈을 당일 v1.53으로 재핀하는 수정이 이루어졌다.

**SP2 `Program::Use()` 제거 시 활성 챕터 무수정 가능 여부 확인.**
SP2 완료 시점에 활성 챕터는 `imguitest`였고 해당 챕터는 `prog->Use()`를 호출하지 않았기 때문에 `apps/` 변경 없이 `test_program_uniforms.cpp`만 `rc.UseProgram(*prog)`로 치환하여 SP2를 완결할 수 있었다. 향후 `_MyApp_` 복귀 시 동일 패턴 적용이 필요하다는 seam이 Doxygen 주석으로 표기되어 있다.

### 산출물

- 변경 모듈/영역: `src/shader/`, `src/program/`, `src/render/` (신설), `src/engine/` (레거시 3파일 삭제), `src/object/geometry`, `src/common/constants.h`, `src/diagnostics/uniform_diagnostics.h`, `apps/imguitest/` (신설), `test/` (144개 테스트 + 지원 헬퍼 3종 신설), `extern/imgui` (v1.53 핀), `extern/Catch2` (v3.15.0 핀)
- 커밋 5건 (주요: `9b541e4` SJH::Shader RAII 명시화, `b38775c` SJH::Program RAII 명시화, `e771e80` Engine::ShaderProgram 물리 제거, `1b078ed` SP2 seam Doxygen 표기, `8c6afff` ECS 렌더링 아키텍처 재설계 — RenderContext/ImGui/Catch2/SP2 전범위)

---

## 2026-05-21 (목)

**Actor+Component 씬 그래프 설계 완성 및 렌더 파이프라인 구조화**

### 개요

본 날은 기존 데이터 ECS(entt) 기반 설계를 전면 폐기하고, Cocos2D `cc.Node/cc.Component` 정통에 Unreal `AActor/UActorComponent` 영감을 흡수한 OOP Actor+Component 씬 그래프 아키텍처를 신규 설계·구현하는 작업이 집중적으로 이루어졌다. 동시에 렌더 파이프라인의 `RenderQueue`, `RenderSystem`, `Framebuffer` 모듈을 정립하였으며, 셰이더 로드 경로의 `sb7` 의존 리팩토링, FMOD Core 엔진 통합 시도 및 Material 속성 bag 패턴 도입까지 단일 세션에 다섯 커밋이 순차적으로 적층된 날이다. 전체 엔진 리팩토링의 SP1~SP6에 걸친 핵심 축에 해당하는 작업이며, 이후 마일스톤(M1 스프라이트 → M3.5 Playable)의 구조적 기반이 이 날 확립되었다.

### 개발 (Feature)

`src/scene/` 신규 모듈로 `Actor`, `Component`, `Scene::Director`, `MeshRenderer`, `ModelSpawner`를 구현하였다. `Actor`는 `unique_ptr` 기반 컴포넌트 맵과 자식 트리를 보유하며, `OnEnter/OnExit/Update` 라이프사이클을 Cocos 정통으로 계층 cascade 한다. `AddComponent<T>` 는 construct + insert + 라이프사이클 디스패치를 단일 호출로 수행하며, `mEntered` 가드로 진입 후 즉시 `OnEnter`를 발사하는 D-9 계약을 구현하였다. `Scene::Director`는 네임스페이스 `SJH::Scene`의 nested 싱글톤으로 설계하여 같은 식별자의 namespace/class 충돌을 회피하였다. `src/render/`에 `RenderQueue`(7필드 `DrawCommand` + Multi-stage sort), `RenderSystem`(DFS 트리 순회 → Queue Flush), `MaterialApplier` 자유함수 family(`WriteUniforms`/`BindTextures`)를 신설하였다. `Framebuffer` 클래스를 `RenderTarget` 인터페이스로부터 public 상속하여 기본 백버퍼와 FBO를 동일 코드로 처리하는 seam을 실현하였다. 시각 검증 챕터 `apps/ecs_demo`를 신설하여 오렌지 박스 렌더링으로 SP3 전 과정을 검증하였다. FMOD Core 엔진 통합(`30f01a7`) 및 `audio_demo`의 Bank 로드·Event 재생·ImGui v1.53 통합도 동일 세션에서 진행되었다.

### 리팩토링

`Mesh::Draw()`, `Material::Apply()`, `Model::Draw()` 를 완전 폐기하고 자원 클래스를 순수 데이터 역할로 정리하였다(SP2 Pattern Y 완성). GL 호출 책임은 `MaterialApplier`(자유함수) → `RenderSystem`으로 이전되었다. `src/engine/` 디렉토리 전체(`scene_graph`, `transform`, `camera`, `model_base` 등 dead code)를 삭제하여 레거시를 청산하였다. 셰이더 로드 경로를 `common::LoadTextFile` 12줄 구현에서 `sb7::shader::load` 단일 위임으로 대체하였다(`e2833c8`). 세션 말미(`a503ab6`)에는 SP6 작업으로서 `UniformCache` 클래스를 `src/program/`으로 분리하고, `Program` 소멸자가 의존 `Material`을 `OnProgramReleased`로 알리는 Observer 패턴을 도입하였으며, Material의 고정 schema(diffuse/specular/shininess)를 Unity `material.SetFloat/SetVector/SetTexture` 정통의 properties bag으로 교체하는 작업이 시작되었다. `entt` 서브모듈을 `.gitmodules`와 cmake에서 완전히 제거하였다.

### 수정 (Fix / Chore)

- `AddComponent` exception safety: `make_unique` throw 시 map이 오염되는 문제를 construct-first 패턴(`find+assert → make_unique → emplace` 순서)으로 수정하였다.
- `src/scene/actor.h`의 `GetWorldMatrix` ternary 패턴이 `glm::mat4` base type 반환으로 컴파일 실패하는 spec 오류를 if-block 분기 패턴으로 정정하였다.
- `MaterialApplier`의 `GLint → GLuint` sign-conversion 경고를 명시적 `static_cast<GLuint>`로 해소하였다.
- `RenderQueue`에서 raw pointer 비교(`<`)가 UB(C++ [expr.rel])를 야기하는 버그를 `std::less<const Program*>` 로 교체하였다.
- `render_queue.h`의 `material/material_applier.h` 의존이 render↔material 순환을 유발함을 발견하고, `MaterialApplier`를 `src/material/`에서 `src/render/`로 이전하여 순환 링크 위험을 해소하였다.
- `include/shader.h`의 헤더 가드가 `src/shader/shader.h`와 동일하여 `sb7::shader::load` 심볼이 누락되는 빌드 에러를 가드명 충돌 수정(`__SB7_SHADER_H__`)으로 해결하였다.
- `src/shader/CMakeLists.txt`의 `PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}`가 sb7 헤더 해상 우선순위를 역전시키는 문제를 발견하고 해당 경로를 제거 후 재추가 금지 주석을 명시하였다.

### 기술적 결정 및 이슈

1. **entt 데이터 ECS 폐기 결정**: 경량 stand-alone OOP Actor+Component 라이브러리가 부재함을 조사로 확인하고, 사용자 결정에 따라 직접 설계 노선을 채택하였다. Cocos2D의 `cc.Node/cc.Component` 구조를 1차 정통으로, Unreal의 `SetEnabled` 토글을 영감으로 흡수한 것이 핵심 설계 방향이다.

2. **MaterialApplier 모듈 귀속 결정**: 최초 `src/material/`에 배치한 `MaterialApplier`가 `src/render/`의 `RenderContext`를 호출하여 render↔material 순환 의존을 유발하였다. GL 호출은 render concern이라는 원칙을 근거로 `src/render/`로 이전하였으며, 이로써 `src/material/`이 순수 데이터 leaf 모듈로 확립되었다.

3. **namespace SJH::Scene vs class SJH::Scene 충돌**: C++에서 동일 스코프에 namespace와 class가 동일 식별자를 공유할 수 없다는 언어 제약을 빌드 전 spec 검증 단계에서 발견하였다. Cocos `cc::Director` 정통 명명을 채택하여 `class SJH::Scene::Director`(nested)로 설계를 변경함으로써 컴파일 에러를 사전 차단하였다.

### 산출물

- 변경 모듈/영역: `src/scene/`(신규), `src/render/`(RenderQueue·RenderSystem·MaterialApplier 신규), `src/buffer/`(Framebuffer 확장), `src/material/`(Draw/Apply 폐기·properties bag 착수), `src/program/`(UniformCache 분리·Observer 도입), `src/engine/`(전체 삭제), `apps/ecs_demo/`(신설·시각 검증), `test/test_actor_lifecycle.cpp`(7 TEST_CASE 신규), `cmake/Dependency.cmake`(entt 제거)
- 커밋 5건 (주요: `d3bcc03` Actor+Component+RenderQueue 설계 완성, `e2833c8` 셰이더 sb7 위임·FMOD Core 통합, `7a0a8e1` Framebuffer RenderTarget 상속, `a503ab6` UniformCache 분리·Material properties bag 착수)

---

## 2026-05-22 (금)

**`migrate_demo` 마이그레이션 완료 및 렌더 큐 스텐실·Pass 확장**

### 개요

본 일자의 작업은 기존 레퍼런스 소스(`OpenGL-With-CMake/src/context/context.cpp`)를 `SJH::engine` 기반 Actor/Component 씬 그래프와 `RenderSystem`으로 이식하는 `migrate_demo` 마이그레이션을 완결하는 데 집중되었다. 단순한 씬 재현을 넘어, 엔진 코어(`src/render/`, `src/material/`, `src/scene/`)에 스텐실 버퍼 지원·Pass 추상화·PostFX 체인이라는 세 가지 기능을 함께 신설하였다. 이로써 `migrate_demo`는 Plane·Box·Outline(스텐실 아웃라인)·투명 Window·다중 광원·5종 PostFX를 모두 포함하는 종합 검증 챕터로 완성되었다. 커밋 3건으로 총 +1,509 / -373줄이 변경되었으며, 본 작업은 전체 마일스톤의 렌더링 인프라 정비(SP-Pass 설계) 구간에 해당한다.

### 개발 (Feature)

**PostFX 5종 체인 (`blurring → gamma → invert → sharpening → sobel`)**
`migrate_demo`에 멀티 패스 PostFX 파이프라인을 구현하였다. 각 패스는 독립적인 레이어 비트·Camera·Screen Quad·중간 Framebuffer를 보유하며, 매 프레임 활성 패스만 추려 입출력 Framebuffer를 동적으로 재배선한다(`input = 첫 패스 ? SceneFB : 이전 활성 패스의 outputFB`). ImGui 패널을 통해 패스별 개별 토글이 가능하다. PostFX Camera 레이어는 `kFxLayer(i)` 헬퍼로 비트 1~5를 할당하여 `LAYER_SCENE`과 충돌하지 않도록 설계하였다.

**`StencilState` 구조체 및 `MeshRenderer` 확장 (`src/scene/components.h`)**
`MeshRenderer` 컴포넌트에 `StencilState`, `DepthTest`, `DepthWrite` 세 필드를 추가하였다. `StencilState`는 Unity `ShaderLab Stencil` 블록을 정통 모방한 구조체로, `Func`, `Ref`, `TestMask`, `SFail`, `DpFail`, `DpPass`, `WriteMask` 7개 필드를 포함한다. 기본값은 `Enabled=false`이므로 기존 액터의 동작은 변경되지 않는다.

**`SJH::Pass` 모듈 신설 (`src/material/pass.h`, `src/material/material.h`)**
`Pass::Kind` 열거형(Opaque=2000, AlphaTest=2450, Skybox=2500, Transparent=3000)과 `Pass::State` 구조체를 신설하였다. Material에 `SetPass(Pass::Kind)` / `GetPass()` / `GetQueueLayer()` API를 추가하여, 챕터 코드가 `mat->SetPass(Pass::Kind::Transparent)` 한 줄로 queue layer·blend·depth write를 일괄 설정할 수 있도록 하였다. Filament/Unreal/Cocos의 SurfaceType·BlendMode 설계를 따른다.

**`SceneRefs` 구조체 및 FlashLight 모드**
`Scene.Warmup.h`에 `SceneRefs` 구조체를 도입하여 DirLight·PointLight 2개·SpotLight에 대한 포인터를 `main.cpp`가 보유하도록 정리하였다. SpotLight이 카메라를 추종하는 FlashLight 모드(`F` 키 토글)와 ImGui 광원 enable 체크박스가 구현되었다.

**`Constants.h` 신설**
`apps/migrate_demo/src/Constants.h`를 신설하여 `main.cpp`에 산재하던 프로그램 이름·경로·PostFX 키·uniform 이름·로그 메시지·ImGui 라벨 등을 `MigrateDemo::Constants` 네임스페이스 하위 카테고리별로 추출하였다. `inline constexpr const char*`로 선언하여 ODR-safe를 보장한다.

### 리팩토링

**스텐실 아웃라인 방식 전환 (shell scale 트릭 → 정통 스텐실 마스킹)**
초기 커밋(`96ed47f`)에서는 Outline 셸을 `queueLayer=1995`로 Box2보다 먼저 그려 깊이 트릭으로 림(rim)만 노출하는 방식을 사용하였다. 두 번째 커밋(`cd7f30c`)에서 정통 스텐실 마스킹 방식으로 전환하였다. Box2가 `GL_REPLACE`로 `ref=1`을 버퍼에 도장하고, Outline이 `GL_NOTEQUAL` 조건과 `DepthTest=false`로 Box2 내부 픽셀을 제외한 림만 출력한다. Queue 순서도 `QUEUE_OUTLINE=2005`(Box2 이후)로 수정되었다.

**`RenderSystem::Submit` 구조체화**
`render_system.cpp`에서 `mQueue.Submit({...})` 집성체 초기화를 `DrawCommand cmd; cmd.xxx = ...;` 명시적 필드 대입으로 전환하였다. 이를 통해 per-actor GL 상태 override(`cmd.stencil`, `cmd.depthTest`, `cmd.depthWrite`)가 `MeshRenderer`에서 `DrawCommand`로 전파되는 경로가 명확해졌다.

### 수정 (Fix / Chore)

**`RenderContext::BeginFrame` 스텐실 클리어 누락 (`src/render/render_context.cpp`)**
기존 `BeginFrame`이 `GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT`만 클리어하여 스텐실 버퍼가 이전 프레임 값을 유지하는 문제가 있었다. `Framebuffer`가 `DEPTH24_STENCIL8` 포맷을 사용하므로 `glClearStencil(0)` + `GL_STENCIL_BUFFER_BIT` 플래그를 추가하여 매 프레임 클리어하도록 수정하였다.

**`samples/extra2/main.cpp` 화살표 주석 문자 정리**
코드 주석 내 `←` 유니코드 화살표를 `<-` ASCII 형식으로 교체하여 크로스 플랫폼 소스 파일 가독성을 개선하였다.

### 기술적 결정 및 이슈

**Pass 의도 선언의 단일 진실 원천 설계**
`QueueLayer`, `DepthWrite`, `BlendEnable` 등 렌더 상태를 챕터 코드가 직접 지정하던 방식에서, `Material::SetPass(Pass::Kind)` 한 줄이 모든 파생 상태를 결정하는 구조로 전환하였다. 이는 Unity의 Render Queue 자동 결정·Filament의 BlendingMode·Cocos의 technique 설계를 따른 것으로, 챕터 코드의 실수로 인한 상태 불일치(예: Transparent Material임에도 `DepthWrite=true` 방치)를 원천 방지한다. 단, `MeshRenderer`의 `Stencil`·`DepthTest`·`DepthWrite` 명시 override가 Pass 기본값보다 우선 적용되는 2-계층 구조를 유지하여 스텐실 아웃라인처럼 세밀한 제어가 필요한 경우를 수용하였다.

**`RenderQueue::Flush` 내 상태 캐시 도입 (`LastState` 구조체)**
매 `DrawCommand`마다 GL 상태 함수를 무조건 호출하면 동일 상태 연속 적용 시 불필요한 드라이버 호출이 발생한다. `LastState` 내부 구조체를 도입하여 이전 프레임과 동일한 스텐실·깊이·블렌드 설정은 GL 호출을 생략하는 상태 캐싱 기법을 적용하였다. 이 구조는 Cocos2d-x `RenderState` 캐시와 동일한 원칙을 따른다.

### 산출물

- 변경 모듈/영역: `apps/migrate_demo` (main.cpp, Scene.Warmup.h, Constants.h, client.h), `src/render/render_queue.{h,cpp}`, `src/render/render_system.cpp`, `src/render/render_context.cpp`, `src/material/pass.h` (신설), `src/material/material.h`, `src/scene/components.h`, `src/scene/compound_actor.cpp`, `samples/extra2/main.cpp`
- 커밋 3건 (주요: `96ed47f` 마이그레이팅 일부 진행, `cd7f30c` 렌더 큐·컨텍스트 스텐실 확장, `5ebac11` 마이그레이팅 완료 + `pass.h` 신설)

---

## 2026-05-23 (토)

**렌더 파이프라인 책임 분할 및 FrameBuffer Resource화 리팩토링**

### 개요

본 날의 작업은 커밋 `063377d` 단 1건으로 구성되나, 변경 범위는 +1524 / -1455 줄에 달하는 대규모 리팩토링이다. 이전까지 `RenderContext` + `RenderSystem` 이라는 두 클래스가 프로그램 바인딩, GL 상태 변경, 드로우 발행, 렌더 타깃 관리, 씬 순회를 혼재하여 처리하고 있었다. 이번 리팩토링은 이 책임들을 `DeviceContext`, `PipelineStateSetter`, `PropertyBlockSetter`, `MeshPassProcessor`, 그리고 새로운 `RenderTarget` 계층으로 명확히 분리하였다. 또한 `Framebuffer` 를 `ResourceRegistry` 에 위탁하는 소유권 재정의(SP-RTOwnership)가 함께 이루어졌다. 이 작업은 M1 스프라이트/아틀라스 구현을 앞두고 렌더 코어의 구조적 기반을 다지는 단계로, 이후 M2 FSM, M3 Box2D, M3.5 Playable 코어 등 게임플레이 레이어를 안정적으로 쌓기 위한 사전 정지 작업에 해당한다.

### 개발 (Feature)

**`DeviceContext` 신설 (GL 호출 단일 게이트웨이).**
기존 `RenderContext` 를 대체하는 `DeviceContext` 싱글톤을 도입하였다. 이 클래스는 현재 바인딩된 프로그램(`mBoundProgram`)을 추적하고, `glUseProgram` / `glBindVertexArray` / `glBindTexture` / `glClear` / `glEnable` 과 같은 GL 상태 변경을 단일 진입점으로 집약하며, `DrawIndexed` / `DrawArrays` 드로우 명령을 발행한다. `BeginFrame(RenderTarget&)` 메서드는 타깃 바인딩 + 클리어 + 기본 depth/blend 설정을 한 번에 처리하는 편의 alias 로 설계되었다.

**`PipelineStateSetter` 신설 (GL 상태 머신 단일 진실의 원천).**
`Pass::PipelineState` 값 객체와 `MeshRenderer` 의 Stencil 오버라이드를 입력받아, GL 상태 머신(`glEnable/Disable`, `glDepthFunc`, `glStencilFunc/Op/Mask`, `glCullFace`, `glBlendFunc`)으로 변환하는 전담 클래스이다. 내부적으로 직전 적용 상태를 캐시(`mLast`, `mInitialized`)하여 중복 GL 호출을 회피하며, `RestoreDefaults()` 로 Flush 종료 후 표준 Opaque 상태로 복원하는 라이프사이클을 갖는다. DirectX 12의 `ID3D12CommandList::SetPipelineState` 및 Unity SRP의 `DrawRenderers RenderStateBlock` 내부 적용 단계에 대응하는 정통 구조이다.

**`PropertyBlockSetter` 신설 / `MaterialApplier` 제거.**
기존 `MaterialApplier` 를 폐기하고, `MaterialPropertyBlock` 값 객체와 `Program` 을 받아 유니폼 송신 및 텍스처 바인딩만을 전담하는 `PropertyBlockSetter::Set` 네임스페이스 자유 함수로 대체하였다. Unity `Renderer.SetPropertyBlock` 내부 적용 단계와 동일한 역할 분리이다.

**`MaterialPropertyBlock` 신설.**
`Material` 에서 셰이더 무관 properties bag을 `MaterialPropertyBlock` 이라는 독립 구조체로 분리하였다. `float / int / vec3 / vec4 / mat4 / TextureBinding` 6종 타입별 `unordered_map` 으로 구성되며, Unity `MaterialPropertyBlock` 의 typed store 정통을 따른다. `Material` 은 `Program` 참조 + `Pass::Kind` + `MaterialPropertyBlock` 1개를 보유하는 역할로 축소되었다.

**`Pass::PipelineState` 구조체 이름 변경 및 Stencil 필드 추가.**
기존 `Pass::State` 를 `Pass::PipelineState` 로 개명하고, `StencilEnable / StencilFunc / StencilRef / StencilReadMask / StencilOpSFail·DPFail·DPPass / StencilWriteMask` 필드를 추가하였다. 또한 `Pass::Kind` 에 `OutlineVisible` (queue 4000, DepthFunc=LEQUAL) 과 `OutlineXRay` (queue 4001, DepthFunc=GREATER) 두 종류를 추가하여 정통 Stencil Outline 패스를 명시적으로 표현할 수 있게 하였다. `DefaultStateOf` 함수는 `DefaultPipelineStateOf` 로 개명되었다.

**`RenderTarget` 추상 계층 확장 및 `DefaultRenderTarget` 소유권 재정의 (SP-RTOwnership).**
`RenderTarget` 인터페이스의 `GetSize()` 를 `GetWidth() / GetHeight()` 분리 시그니처로 변경하고, `DefaultRenderTarget` 의 lifetime owner 를 기존 `DeviceContext` 에서 **Application** 으로 이전하였다. `Framebuffer`(FBO) 는 `ResourceRegistry::CreateFramebuffer` 로 위탁하는 소유권 분담 규칙을 확립하였다. `migrate_demo` 의 `render()` 에서 `SJH::RenderContext::Get().SetDefaultTargetSize(fbW, fbH)` 호출을 `std::make_unique<SJH::DefaultRenderTarget>(fbW, fbH)` 로 대체하여 이 규칙을 반영하였다.

### 리팩토링

**`RenderContext` + `RenderSystem` → `DeviceContext` + `SceneRenderer` 전면 교체.**
`render_context.h/cpp` 및 `render_queue.cpp`(238줄), `render_system` 관련 코드를 제거하고, `DeviceContext` + `SceneRenderer` + `MeshPassProcessor` 의 3층 구조로 재편하였다. `migrate_demo` 의 인클루드가 `render/render_context.h` + `render/render_system.h` + `scene/components.h` 에서 `render/device_context.h` + `render/scene_renderer.h` + `render/mesh_renderer.h` 로 교체되었다. `Scene.Warmup.h` 의 주석도 `RenderSystem` → `SceneRenderer` 로 일관되게 정정하였다.

**임시 검증 데모 정리 (`_deptest_`, `imguitest`, `postfx_demo` 전체 삭제).**
의존성 링크 검증 전용 `apps/_deptest_/main.cpp` (78줄), ImGui 통합 smoke test `apps/imguitest/main.cpp` (66줄), 초기 PostFX 검증용 `apps/postfx_demo/main.cpp` (154줄)이 이번 커밋에서 완전히 제거되었다. 이 세 타깃은 초기 의존성 등록 및 기능 검증 목적으로 작성된 임시 코드였으며, 해당 기능이 `_MyApp_` 및 `migrate_demo` 로 흡수됨으로써 역할이 소멸하였다. `apps/CMakeLists.txt` 에서 대응 `add_subdirectory` 항목도 함께 제거 또는 비활성 처리되었다.

**entt 코드 제거.**
`apps/_MyApp_/main.cpp` 에서 `entt::registry` 생성 및 컴포넌트 등록/조회 코드가 제거되었다. 이는 이후 OOP Actor+Component 방식을 선호하는 설계 원칙("entt 제거 결정")의 조기 정지 작업이다.

### 수정 (Fix / Chore)

`migrate_demo/main.cpp` 주석 내 화살표 표기를 `→` (특수문자)에서 `->` (ASCII)로 일괄 교정하였다. 이는 이후 확립되는 Doxygen+ASCII 주석 컨벤션(특수문자 0 clean 정책)의 선행 조치에 해당한다. `src/buffer/buffer.cpp` 의 인클루드 순서를 정리하고 불필요한 예시 주석 블록을 제거하였다. `test/test_material.cpp` 및 `test/test_program_uniforms.cpp` 의 테스트 코드도 변경된 API 시그니처(`PipelineState` 구조체 이름 변경 등)에 맞게 일괄 갱신되었다.

### 기술적 결정 및 이슈

**SP-RTOwnership — Framebuffer 소유권 3분할 결정.**
`DefaultRenderTarget`(백버퍼)은 Application이, `Framebuffer`(FBO)는 ResourceRegistry가, `DeviceContext`는 소유 없이 명령 발행만 수행하는 3분할 소유권 규칙을 확립하였다. 이는 DX11 SwapChain이 백버퍼를 소유하고 Unity RTHandleSystem이 오프스크린 RT를 관리하는 정통 엔진 구조와 일치한다. `DeviceContext`가 RenderTarget을 보유할 경우 발생할 수 있는 라이프사이클 역전 문제를 사전에 차단한 결정이다.

**`Pass::PipelineState` 에 인라인 학습 주석 통합.**
`DepthWrite = false` 의 의미(Skybox/Transparent가 왜 z를 기록하지 않는가), `DepthFunc` 옵션별 사용 시나리오(GL_EQUAL의 2-pass 활용, GL_GREATER의 X-Ray outline, GL_NOTEQUAL이 Stencil과 혼동되는 함정), Stencil 2-pass 패턴(LearnOpenGL 정통 REPLACE+NOTEQUAL) 등을 헤더 내 상세 주석으로 기록하였다. 이는 교수 제출용 코스워크라는 프로젝트 성격상 구현과 학습 서술을 함께 유지하는 의도적 선택이다.

**`RenderContext` 전면 교체로 인한 소비자 API 일괄 갱신.**
기존 `SJH::RenderContext::Get()` 호출 지점이 다수 데모에 분산되어 있었으므로, 이번 리팩토링은 `migrate_demo` 뿐 아니라 `apps/CMakeLists.txt` 단위까지 영향을 미쳤다. 임시 데모 3종의 전체 삭제는 이 영향 범위를 축소하기 위한 부수적 정리이기도 하다.

### 산출물

- 변경 모듈/영역: `src/render/` (DeviceContext·PipelineStateSetter·PropertyBlockSetter·MeshPassProcessor·RenderTarget 재편), `src/material/` (MaterialPropertyBlock 신설·Pass::PipelineState 이름 변경 및 Stencil 확장), `src/buffer/buffer.cpp`, `src/resource_registry/resource_registry.{h,cpp}`, `apps/migrate_demo/`, `apps/_deptest_`·`imguitest`·`postfx_demo` 제거
- 커밋 1건 (`063377d` `[refactor] : FrameBuffer Resource 화`)

---

## 2026-05-24 (일)

**`SJH::sprite` 모듈 신설 및 보조 데모 확장**

### 개요

본 일자는 탑다운 슈터 게임 `_MyApp_`의 M1(스프라이트/아틀라스) 마일스톤 착수일이다. `SJH::sprite` STATIC 라이브러리를 신설하고 `SJH::engine` 우산 타겟에 편입하는 것이 핵심 목표였다. 이와 병행하여 `SJH::material` 모듈의 Shared/Instance 분리 리팩토링, `common.h`에 `DeltaTime`/`FixedTime` 유틸 함수 추가, `tweeny_demo` 스켈레톤 구성, `audio_demo` 다중 서브타겟 분리, `effekseer_demo` 신규 등록 등 보조 데모 일체를 정리하는 작업이 동시에 진행되었다. 총 17개 커밋, 코드 +3938/-1760줄이 발생하였다.

### 개발 (Feature)

**`SJH::sprite` 모듈 신설 (M1 Task 1~5).**
`src/sprite/uniform_atlas.{h,cpp}`와 `src/sprite/sprite_component.h`를 신설하였다.

- `ComputeUVRect` 자유 함수는 `frameIdx`·`cols`·`tileSize`·`atlasWidth`·`atlasHeight`를 받아 UV 직사각형(`glm::vec4`)을 GL 호출 없이 순수 수학 연산으로 계산한다. GL 비의존 덕분에 `test_uniform_atlas.cpp`(4×4 그리드, 8×4 비정방 그리드, 잘못된 입력 등 5 케이스)를 GL 픽스처 없이 작성할 수 있었다.
- `UniformAtlas::LoadFromPNG`는 PNG 디코딩·GL 텍스처 업로드·NEAREST 필터 설정의 세 단계를 수행하며, 아틀라스 크기가 타일 크기의 정수 배수가 아닐 경우 `spdlog::error`를 출력하고 `false`를 반환하도록 방어 처리하였다.
- `SpriteComponent`는 `SJH::Scene::Component`를 상속하는 POD-ish 구조체로, `atlas*`·`frameIdx`·`size`·`tint`·`flipX`를 공개 멤버로 보유한다. 시간 축 갱신 책임은 없으며 M3.5 이후 `SpriteSequencePlayable`이 담당하도록 설계하였다.
- `b6a0cfd` 커밋에서 `src/CMakeLists.txt`에 `SJH::sprite`를 등록하고 `SJH::engine` INTERFACE 우산에 합류시켜, 데모가 `target_link_libraries(타겟 PRIVATE SJH::engine)` 한 줄로 스프라이트 기능을 자동으로 포함하도록 하였다.
- `945fa70` 커밋에서 `apps/CMakeLists.txt`에 `add_subdirectory(_MyApp_)`를 주석 해제하고 `apps/_MyApp_/CMakeLists.txt`에 `SJH::engine` PRIVATE 링크를 추가하여 빌드 타겟을 활성화하였다.

**`common.h` 유틸 함수 추가.**
`SJH::DeltaTime(double currentTime)` 및 `SJH::FixedTime()` 자유 함수를 추가하였다. `DeltaTime`은 `static` 내부 변수로 직전 호출 시각을 보관하여 프레임 간 경과 시간을 반환하고, `FixedTime`은 `1.0f/60.0f`를 반환하는 `constexpr` 함수이다. 이를 Box2D 데모 3종(`demo1~3`)의 `b2World::Step`과 Effekseer 데모, `migrate_demo`의 dt 계산에 일괄 적용하였다.

**`tweeny_demo` 신규 구성.**
11종 이징 함수를 핑퐁 왕복 애니메이션으로 시각화하는 `tweeny_demo`를 구성하였다. `PingPongTween` 컴포넌트(`Component` 파생)가 `tweeny::tween<float>`을 보유하고 `Update(float dt)`에서 `step(int32_t dtMs)`를 호출하며 소유 Actor의 `Transform.Translate`를 갱신한다. `Constants.h`에 이징 이름 상수와 셰이더 경로를 `inline constexpr`로 정의하였다.

**`audio_demo` 다중 서브타겟 분리 및 `effekseer_demo` 추가.**
`audio_demo/main.cpp` 단일 파일 구조를 `demo1/main.cpp`, `demo2/main.cpp`로 분리하였다. FMOD Studio System·Bank·EventInstance·`ParamCache`(Continuous/Labeled/Switch/DiscreteInt 4종) 기반의 파라미터 조작 데모를 ImGui와 함께 구성하였다. `effekseer_demo` 디렉토리와 `demo1`을 등록하였다.

### 리팩토링

**`SJH::material` — Shared / Instance 분리(`e7bad97`).**
`material.h`에서 공유 불변 데이터(`SharedMaterial`)와 인스턴스별 가변 데이터를 분리하도록 `Material` 클래스를 재구성하였다. `MeshPassProcessor`, `PipelineStateSetter`, `SceneRenderer`, `ResourceRegistry` 등 렌더 파이프라인 관련 파일 9종이 함께 수정되었으며, `ResourceRegistry`가 `Material`을 소유하는 방식이 정리되었다.

**`UniformAtlas` — `SJH::Image`·`SJH::Texture` 위임(`f51726d`).**
초기 구현(`b336072`)에서는 `stb_image` 및 `glGenTextures`/`glTexImage2D`를 `uniform_atlas.cpp` 내부에서 직접 호출하였다. 이후 리팩토링에서 이미지 디코딩은 `SJH::Image::Load`에, GL 텍스처 업로드는 `SJH::Texture::CreateTexture`에 위임하도록 변경하였다. `mTextureId(GLuint)` 멤버를 `mTexture(SJH::TextureUPtr)`로 교체하여 소멸자에서의 `glDeleteTextures` 호출도 `SJH::Texture` RAII로 위탁하였다. 소멸자 명시 정의가 필요 없어져 `~UniformAtlas() = default`로 단순화되었다.

**`_MyApp_` 클린업(`abb3ee3`).**
기존 `deptest_application` 클래스(Tweeny·Box2D·Assimp·Effekseer 심볼 링크 검증용 임시 코드 포함)를 제거하고 `namespace TopdownShooter { class game_application }` 구조로 교체하였다. 불필요한 `#include <mutex>` 및 임시 검증 코드 약 50줄이 삭제되었다.

### 수정 (Fix / Chore)

**`STB_IMAGE_IMPLEMENTATION` 중복 정의 문제(`88d5552` → `3432881` revert).**
`b6a0cfd`에서 `SJH::sprite`를 엔진 우산에 합류시키기 위한 사전 작업으로 `image.cpp`의 `STB_IMAGE_IMPLEMENTATION` 정의를 제거하고 `uniform_atlas.cpp`를 단일 owner로 지정하였다. 그러나 이 변경이 `test_texture`·`test_material` Catch2 테스트에서 링크 회귀를 유발함을 확인하고, `3432881` 커밋에서 즉시 되돌렸다. 이후 `f51726d`에서 `UniformAtlas`가 stb_image를 직접 사용하지 않도록 리팩토링함으로써 `image.cpp` 단일 owner 정책을 유지한 채 문제를 근본적으로 해소하였다.

**`SetWrap` 암묵 의존 제거(`477f195`).**
`Texture::CreateTexture`의 기본값이 `GL_CLAMP_TO_EDGE`임에 암묵적으로 의존하던 코드를 `mTexture->SetWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE)` 명시 호출로 교체하였다. 향후 `Texture` 모듈의 기본값 변경에도 atlas tile bleed가 발생하지 않도록 교차 모듈 결합을 해소하였다.

**`MaterialPropertyBlock`에 `Vec2s` 맵 추가.**
`MaterialPropertyBlock`에 `std::unordered_map<std::string, glm::vec2> Vec2s` 멤버가 추가되었고, 이에 대응하는 `property_block_setter.cpp` 디스패치 경로가 보완되었다.

### 기술적 결정 및 이슈

1. **`STB_IMAGE_IMPLEMENTATION` 단일 owner 위치 확정.** 초기에는 `uniform_atlas.cpp`를 단일 owner로 지정하려 하였으나, 이미 `image.cpp`에 의존하는 테스트들이 링크 오류를 일으켜 즉각 되돌렸다. `UniformAtlas`가 stb_image를 직접 사용하지 않도록 아키텍처를 수정함으로써 `image.cpp`를 원래 owner로 유지하면서도 중복 정의 문제를 회피하는 방식으로 귀결되었다.

2. **`UniformAtlas`의 stb_image·GL 직접 호출 즉각 제거.** 초기 구현 단계에서 스프라이트 모듈이 하위 레이어(`SJH::resource_registry`)의 `Image`·`Texture` 추상을 우회하여 GL을 직접 조작하는 문제가 동일 세션 내에서 발견되었다. 설계 일관성 관점에서 같은 날 `f51726d` 커밋으로 즉시 수정하였다. 이는 `UniformAtlas`가 GL 텍스처 수명 관리를 직접 책임지지 않고 `SJH::Texture` RAII에 위탁하는 현재의 컨벤션을 확립한 시점이다.

3. **`DeltaTime`의 `static` 내부 상태 의존.** `SJH::DeltaTime`은 `static double lastTime`으로 직전 호출 시각을 보관하므로, 여러 호출 지점이 동일 함수를 공유하면 간섭이 발생한다. 결정론적 시뮬레이션이 필요한 Box2D Step에는 `FixedTime()`을, 렌더 루프 dt에는 `DeltaTime()`을 구분 적용하는 컨벤션을 함께 수립하였다.

### 산출물

- 변경 모듈/영역: `src/sprite/` (신설), `src/common/`, `src/material/`, `src/render/`, `src/resource_registry/`, `apps/_MyApp_/`, `apps/tweeny_demo/`, `apps/audio_demo/`, `apps/effekseer_demo/`, `apps/box2d_demo/`, `apps/CMakeLists.txt`, `test/test_uniform_atlas.cpp` (신설)
- 커밋 17건 (주요: `4922a00` feat(sprite): SJH::sprite module skeleton with ComputeUVRect, `b336072` feat(sprite): UniformAtlas::LoadFromPNG implementation, `2d97453` feat(sprite): SpriteComponent POD-ish, `f51726d` refactor(sprite): UniformAtlas delegates to SJH::Image + SJH::Texture, `b6a0cfd` feat(engine): SJH::sprite를 SJH::engine 우산에 합류, `945fa70` build(_MyApp_): activate apps/_MyApp_ + link SJH::engine, `e7bad97` [refactor]: Shared, Instance Material 분리)

---

## 2026-05-25 (월)

**M2 FSM 코어 신설 + M3 Box2D 물리 통합 + SP5 렌더 스테이지 정착**

### 개요

본 날은 탑다운 슈터 `_MyApp_` 개발의 핵심 마일스톤이 집중된 날로, 총 43건의 커밋이 집행되었다. 크게 세 줄기로 요약된다. 첫째, `SJH::fsm` 코어 모듈을 신설하고 두 단계에 걸쳐 FSM 설계를 완성하였다(M2). 둘째, Box2D v2.4.1 물리 통합을 Client 한정으로 수행하여 M3을 완료하였다. 셋째, `SJH::IRenderStage` 추상과 `SJH::Scene::Layer` enum class를 도입하는 SP5 렌더 아키텍처 작업이 병행되었다. 이와 함께 `Director` + `SceneRenderer` + `Material` 패턴으로의 전환, `SpriteAnimator` 신설 등 M1/M2 P2 정착 작업도 완료되었다.

### 개발 (Feature)

**FSM 코어 모듈 신설 (M2 P1 → P1.5)**

`src/fsm/` 디렉토리를 신설하고 `SJH::fsm` INTERFACE(헤더-온리) 라이브러리를 구성하였다. 1단계(P1)에서는 `StateMachineProcessor<TState, TTransit>` 템플릿을 구현하였다. `Bind(transit, target)` 빌더, `TryTransit` AND 판정, `OnEnter`/`OnUpdate`/`OnExit` 가상 함수를 포함하며, `Component::Update` → `Tick` 위임 구조로 Actor 재귀 갱신에 자동 연동된다. Catch2 테스트 5건(`test_fsm`)도 함께 추가하여 초기 진입 / AND 판정 / Enter+Exit 순서 / NONE 가드 / 미등록 target 가드를 커버하였다. 1.5단계(P1.5)에서는 `IFsmState<TOwner>` 인터페이스와 `ObjectStateMachine<TState, TTransit, TOwner>` Aggregate Root를 추가하였다. `State` 객체의 lifetime을 `unique_ptr`로 소유하며, `TryTransit`(조건부)과 `ForceTransit`(강제, 계약 위반 검출용)을 분리하였다. Godot `_state.physics_process` 정통의 구조로 설계하였다.

**M3 Box2D v2.4.1 물리 통합**

`apps/_MyApp_/src/Physics/` STATIC 라이브러리를 신설하였다(876 insertions). 주요 구성 요소는 다음과 같다. `PhysicsBodyComponent`는 `b2Body*` 비소유 래퍼로, `Actor*`를 `b2Body::UserData`에 등록하고 `SetSensor` 전파를 처리한다. `PhysicsMovement`는 `IMovable`을 구현하여 `SetLinearVelocity`로 입력 방향을 전달하며, XZ(OpenGL) → XY(Box2D) 좌표 변환을 담당한다. `PhysicsContactListener`는 `b2ContactListener` → `IContactable` 컴포넌트 디스패치로 Trigger/Collision을 분리하였으며, Unity `isTrigger` 패턴을 따른다. `SyncToTransform`이 `b2Body` 위치를 Actor Transform으로 매핑(`x, heightOffset, -y`)하고, 벽 4개(Solid)와 Pickup Sensor가 10×10 아레나에 배치되었다. `ForEachComponent<Fn>` 템플릿 반복자를 `src/scene/actor.h`에 추가하여 contact dispatch에 활용하였다.

**M2 P2 정착 및 M4 도메인 선행**

`Player WASD` 이동, `TargetFollowableCameraController` 카메라 추종, `SpriteAnimator` 컴포넌트를 구현하였다. `PlayerActor.h`는 Pattern C(PoD Config) factory 패턴으로 신설되었다. `IMovable::DoForward(dir, float dt)` 시그니처를 변경하여 fps-independent 이동을 보장하고, zero-vec 가드(normalize NaN 방지)를 추가하였다. `PlayerEntity.h`는 `BaseEntity + IMovable + IAttackable` Facade로 구성하여 Aggregate Root 정통 구조를 따랐다. M4 도메인 선행으로 `Stat` 수정자 시스템과 연산자 데이터 시스템을 추가하였다.

**SP5 렌더 스테이지 — `IRenderStage` + `Layer` 시스템**

`SJH::IRenderStage` 추상 인터페이스를 신설하였다(Unity `ScriptableRendererFeature` / Unreal `FSceneRenderer` 정통). `Render(RT&)` 순수 가상 함수와 `OnResize(int, int)` no-op 기본을 정의하고 vtable home TU를 분리하였다. `SJH::Scene::Layer` enum class(`uint64_t`)를 신설하여 Default/Player/Enemy/UI/DebugDraw 5 비트를 예약하고 `operator|/&` 자유 함수를 제공하였다. `Actor::mLayer`와 `Camera::CullingMask`를 `uint32` → `uint64`로 확장하여 Layer 시스템과 연동하였다.

**스프라이트 시스템**

`SpriteAnimator`(헤더-온리 Component)를 신설하여 atlas frame을 경과 시간에 따라 자동 순환한다. `UniformAtlas` Fluent Builder를 `LoadFromPNG(path)` + `SetGrid(cols, rows)` / `SetTileSize(tilePx)` 분리 형태로 재설계하였다.

### 리팩토링

**FSM Stage 4 — `TTransit` 제거**

`ObjectStateMachine<TState, TTransit, TOwner>` → `StateMachine<TState, TOwner>`로 template parameter를 축소하였다. 전이 그래프 정보를 `StateMachine`의 `RegisterTransit`/`targetOf_` 맵에서 각 `IFsmState` 객체의 `GetTransitFlag()` 상수로 응집하였다(Entity within Aggregate 정통). 이중 진실(StateMachine 측 맵 + State 측 비트)을 해소하고, `RegisterState` 시그니처를 `(id, unique_ptr<State>)` → `(unique_ptr<State>)` 한 인자로 단순화하였다.

**물리 컴포넌트 계층 구조화**

단일 `PhysicsBodyComponent` 클래스를 `Components::Physics` abstract base + `CircleBody`/`BoxBody` concrete 서브클래스로 분리하였다. `FindPhysics(Actor*)` 헬퍼(`ForEachComponent` + `dynamic_cast<Physics*>`)를 추가하여, `Actor::GetComponent<T>`가 type_index 정확 매치만 지원하는 한계를 우회하였다. 동시에 `IContactable`/`IPhysicsContactListener` 중복 인터페이스를 통합하여 `contact_interface.h`를 삭제하고 `GetIsTrigger()` 중복 소스를 `PhysicsBodyComponent::IsSensor()` 단일 진실로 수렴하였다.

**`DrawCommand` 필드 축소**

`DrawCommand` struct의 직접 필드(`program / mesh / material / actor` 4개)를 제거하고 `meshRenderer` 단일 포인터 경유 접근으로 통합하였다(Filament/Unreal/Cocos 정통). `SceneRenderer::CollectFromActor`의 빌드 코드가 6줄 → 3줄로 축소되었다.

**`Camera` 의존성 역전**

`Camera::mTargetFB(Framebuffer*)` → `mTargetRT(RenderTarget*)` 추상 포인터로 교체하여, 향후 `ShadowMap`/MRT 등 다양한 렌더 타겟을 동일 슬롯에 주입 가능하도록 하였다.

**`_MyApp_` 렌더 패턴 전환**

직접 GL 호출(`glUseProgram`, `glBindTexture`, `glDrawElements`, `glClear` 등)을 완전 제거하고 `SceneRenderer.Render(*mDefaultTarget)` 한 줄로 위임하는 컨벤션을 정착하였다.

### 수정 (Fix / Chore)

**테스트 API 불일치 수정**: SP5 이전 API 변경으로 테스트 5개 파일에서 `Uniforms::Get` → `GetLocation`, `BindKey` 인자 순서 역전, `ResourceRegistry::Create()` → `Get()` 싱글톤 등 불일치가 발생하였다. 수정 후 145/147 통과를 확인하였다.

**macOS GLFW `chdir` 워크어라운드**: macOS에서 GLFW 초기화 후 CWD가 리셋되는 문제를 `SJH::ChdirToExecutableDir()` 공통 유틸로 추출하고, 중복 존재하던 5개 `main.cpp`의 인라인 코드를 일괄 제거하였다.

**`physics_movement.cpp` 빈 파일 삭제 + 중복 라이브러리 경고 silencing**: 빌드 잡음 요인을 정리하였다.

**GLSL uniform 컨벤션 정정**: 셰이더 내 `snake_case` uniform을 `camelCase`로 통일하고 `uModel` 전송을 SceneRenderer 자동 처리로 흡수하였다.

### 기술적 결정 및 이슈

**FSM `TTransit` 제거 — Entity within Aggregate 응집 결정**

초기 설계에서 `StateMachine`이 별도 `targetOf_` 맵으로 전이 그래프를 보유하던 구조는 State 클래스의 `GetTransitFlag()`와 이중 진실을 형성하였다. `TTransit` template parameter 자체를 제거하고 그래프 정보를 State 객체 내부로 완전히 응집함으로써, `RegisterState` 호출 한 줄로 FSM 빌드가 완성되는 구조로 수렴하였다. 이는 DDD의 Entity within Aggregate 패턴에 부합하며, 컴파일 타임 상수화라는 추가 이점을 제공한다.

**`FindPhysics(Actor*)` 우회 헬퍼 도입**

`Actor::GetComponent<T>`는 `std::type_index` 정확 매치 기반이므로, `Physics` 추상 베이스를 직접 조회할 수 없다. `ForEachComponent` + `dynamic_cast<Physics*>` 조합의 헬퍼 함수를 client 측에 추가하여 이 제약을 우회하였다. 이는 `GetComponent`의 polymorphic 확장 한계를 인식한 설계 타협점이다.

**Physics Layer `uint64_t` 선택**

Box2D 2.4.1의 `b2Filter::categoryBits`/`maskBits`는 `uint16_t`이지만, 미래 레이어 확장 여유를 위해 내부 표현을 `uint64_t`로 채택하고 `ToBits()` 어댑터로 변환하는 구조를 선택하였다. `SJH::Scene::Layer`(`uint64_t`) 및 FSM 비트 enum class와 동일한 컨벤션을 유지한다.

### 산출물

- 변경 모듈/영역: `src/fsm/` (신설), `src/render/` (IRenderStage/DrawCommand/SceneRenderer/Camera), `src/scene/` (Layer/Actor), `src/sprite/` (SpriteAnimator/UniformAtlas), `apps/_MyApp_/src/Physics/` (신설), `apps/_MyApp_/src/Entity/` (PlayerActor/PlayerEntity/Movement), `test/` (test_fsm 신설, 기존 5개 수정), `doc/superpowers/specs/` (FSM 설계 정본)
- 커밋 43건 (주요: `2d2bc25` feat(fsm): SJH::fsm 코어 모듈 신설 / `5ab0db2` feat(fsm): ObjectStateMachine + IFsmState 추가 / `041fa36` refactor(fsm): TTransit 제거 + GetTransitFlag 기반 그래프 정보 응집 / `2057e0e` feat(_MyApp_): M3 Box2D v2.4.1 물리 통합 / `64d62ab` refactor: PhysicsBodyComponent → Components::Physics base + BoxBody/CircleBody / `89a741c` feat(render): SJH::IRenderStage 추상 신설 / `52796c0` feat(scene): SJH::Scene::Layer enum class 신설 / `2c0a7e2` feat(_MyApp_): M2 P2 정착 + M4 도메인 선행)

---

## 2026-05-26 (화)

**M3.5 Playable 코어 정착 + SceneContext 자동 등록 + M5 leaf 배선 선행**

### 개요

본 일자는 탑다운 슈터 게임 `_MyApp_`의 마일스톤 M3.5에 해당하는 Playable 코어 인터페이스를 엔진 모듈로 정착시킨 날이다. `IPlayable` 순수 인터페이스, `PlayableBase` 추상 베이스, `SequencePlayable`/`ParallelPlayable` Composite를 `SJH::playable` STATIC 라이브러리로 신설하고, `SpriteFrameClip`·`SpriteSequencePlayable`을 `SJH::sprite` 모듈 내에 통합하였다. 이와 병행하여 `Camera::Depth` 의존 렌더 순서 정렬 방식을 폐기하고 `SceneContext`를 통한 Camera/Light 자동 등록 방식으로 전환하는 대규모 렌더 리팩토링을 완료하였다. 아울러 M5 leaf Playable 선행 작업으로 `AudioSystem`, `FmodPlayable`, `FmodStudioPlayable`, `EffekseerPlayable`, `VFXSystem`, `TweenPlayable`의 Client 측 골격을 일괄 배치하였다. 커밋 총 19건, 코드 순증가 +4066 / -1528줄에 달하는 집중 작업일이었다.

### 개발 (Feature)

**`SJH::playable` 모듈 신설 (`7641fff`, `9421562`, `420cf32`).**
`IPlayable` 순수 인터페이스(Play/Pause/Stop/GetIsLoop/IsFinished 5메서드)와 `PlayableBase`(IPlayable + `SJH::Scene::Component` 다중 상속, `paused_`/`finished_`/`elapsed_`/`isLoop_` 상태 흡수, `Update → OnUpdate` 훅)를 구현하였다. `SequencePlayable`(cursor 기반 순차 실행, Append/Insert 빌더)과 `ParallelPlayable`(모든 child 동시 실행, Join 빌더)을 Composite 패턴으로 추가하였다. 빈 컨테이너 무한 루프 가드, `dynamic_cast<Component*>` 디스패치, `size_t → ptrdiff_t` 명시 캐스트 등 안전 장치를 포함하였다.

**`SpriteFrameClip` + `SpriteSequencePlayable` 통합 (`c23dbda`, `40d7b24`, `15e2da8`).**
atlas 프레임 시퀀스 정의 POD인 `SpriteFrameClip`(startFrame/frameCount/fps)과 이를 소비하는 `SpriteSequencePlayable`을 구현하였다. 초기에는 독립적인 `SJH::sprite_sequence` 모듈로 분리 생성하였으나, 이후 `SJH::sprite` 모듈 내로 통합 정착시켰다. `SpriteAnimator` 클래스는 이 시점에 공식 폐기(삭제)되었다.

**`SpriteRenderer` 신설 (`c9faf14`).**
`MeshRenderer`를 상속하는 `SpriteRenderer`를 구현하였다. ResourceRegistry의 고정 키(`_sprite_plane`, `_sprite_billboard_program`, `_sprite_billboard`)를 통해 공유 plane mesh, billboard 셰이더 Program, template Material을 자동 해결하고, 인스턴스마다 per-instance Material을 생성하는 방식을 채택하였다. 매 `Update`에서 `uUvRect`/`uTint`/`uFlipX` uniform을 자동 송신한다.

**M5 leaf Playable Client 골격 배치 (`9b2b43a`).**
`AudioSystem`(FMOD Core + Studio 초기화/뱅크 로드/이벤트 캐시), `FmodPlayable`(Core .wav 재생), `FmodStudioPlayable`(Studio EventDescription 인스턴스), `VFXSystem`(Effekseer Manager/Renderer 래퍼), `EffekseerPlayable`(Effect 핸들 lifecycle), `TweenPlayable`(Tweeny 기반 수치 트위닝) 총 6종을 `apps/_MyApp_/src/` Client 영역에 배치하였다. `Director` 싱글턴이 AudioSystem·VFXSystem·PhysicsSystem을 통합 초기화·업데이트·종료하는 중앙화 구조로 `main.cpp`를 정리하였다.

**`SceneContext` 신설 + Camera/Light 자동 등록 (`ea921d8`).**
`src/scene/scene_context.{h,cpp}`를 신설하여 Camera/DirLight/PointLight/SpotLight 컬렉션의 Aggregate Root 역할을 담당하게 하였다. `DirLight`, `PointLight`, `SpotLight` 각각에 `OnEnter`/`OnExit` 훅 본문을 추가하여 Actor 트리 부착 시 `Director::GetContext().AddLight(this)`가 자동 호출되도록 구현하였다. Camera 도 동일 패턴을 적용하였다.

**`Stage` 도메인 디렉토리 신설 (`e530a48`).**
`EStageStatus` 비트 플래그 열거형, `StageState` Component, `PickupTriggerLogger`(트리거 콜백 로그), `StageBuilder`(벽 4면 + pickup + StageState를 묶은 Factory 함수), `StageConfig` POD를 `apps/_MyApp_/src/Stage/` 하위에 구성하였다. 기존 `Physics/` 디렉토리에 분산되어 있던 `PickupFactory`, `WallFactory`를 `Stage/Factories/`로 이전하고 네임스페이스를 `TopdownShooter::Stage::Factories`로 정리하였다.

**`SP-UniversalRenderTarget` Phase A+B (`08753b4`).**
`ScreenQuadStage`(IRenderStage 구현체 — source Framebuffer 텍스처 바인딩 후 full-screen quad draw)를 신설하고, Camera의 `SetTargetRenderTarget`에 `nullptr` 금지 assert를 추가하였다.

**`MAX_*_LIGHTS` 상수 정비 + `migrate_demo` 활성 (`231b1cc`).**
`NUM_POINT_LIGHTS=2`를 `MAX_POINT_LIGHTS=16`으로 이름과 값을 동시 정정하고, `MAX_SPOT_LIGHTS=16`을 신설하였다. 단수 `UNI_SPOT_LIGHT`/`UNI_SPOT_LIGHT_ENABLED` 상수를 복수 prefix 방식(`UNI_SPOT_LIGHTS_PREFIX`, `UNI_SPOT_LIGHTS_ENABLED_PREFIX`)으로 교체하였다.

### 리팩토링

**Program-Material Observer 패턴 제거 (`e16cd17`, `0bd9aae`, `1dd7502`).**
`Program`이 의존 `Material` 목록(`mDependentMaterials`)을 보유하고 `~Program`에서 `OnProgramReleased` cascade를 호출하던 Observer(SP6) 패턴을 제거하였다. `Material::SetProgram`은 단순 비소유 raw 포인터 저장으로 단순화되었으며, Observer 제거 과정에서 함께 삭제되었던 `GetProgram()`을 `MeshPassProcessor`의 정렬 키 사용을 위해 복구하였다. 당초 도입 예정이었던 `EagerBuild`(Properties 키 사전 prune) 기능은 같은 날 커밋에서 재검토 후 삭제하는 과정을 거쳤다.

**`SceneRenderer` DFS 3종 폐기 (`ea921d8`, `f078270`).**
`CollectCameras`, `CollectLights`, `CollectPrograms` 세 DFS 메서드와 `Camera::Depth` 기반 `std::sort`를 완전히 제거하였다. 렌더 순서는 `addCamera` 호출 순서가 곧 렌더 순서가 되는 Cocos2D `addChild` 정통 방식으로 전환되었다. `SendLightUniforms`의 첫 인자도 `unordered_set<const Program*>`에서 `vector<Program*>`으로 교체하였으며, SpotLight 수집이 첫 1개 단일에서 최대 16개 배열로 확장되었다.

**`ResourceRegistry::GetAllPrograms()` 신설 + `SceneRenderer` 슬림화.**
`ResourceRegistry`에 캐시된 모든 Program의 raw 포인터 벡터를 반환하는 `GetAllPrograms()` 메서드를 추가하여, `SceneRenderer`가 DFS 없이 light uniform 송신 대상 Program 집합을 일괄 획득할 수 있게 하였다.

**`UniformAtlas` + `SpriteRenderer` 통합 및 위임 패턴 확립.**
`UniformAtlas`에 `CLASS_PTR` 매크로를 추가하고, `ResourceRegistry`에 `CreateUniformAtlas` 메서드를 신설하여 `LoadFromPNG().SetGrid()` 빌더를 Registry 경유로 실행할 수 있게 하였다. `main.cpp`에서 atlas, Program, Material, Mesh를 직접 생성하던 코드가 `SpriteRenderer` 생성자 한 줄로 대체되었다.

### 수정 (Fix / Chore)

**strict-include 정책 위반 정리 (`420cf32`, `a4bdd21`).**
`.clangd`의 `MissingIncludes: Strict` 정책에 의해, `composite_playable.cpp`와 `sprite_sequence_playable.cpp`에서 전이 포함에 의존하던 헤더를 모두 직접 include 하도록 수정하였다. `Insert` 메서드의 `size_t → iterator::difference_type` 암묵 변환은 `static_cast<std::ptrdiff_t>`로 명시하였다.

**`SpriteComponent` → `SpriteRenderer` 타입명 전파 오류 수정 (`40d7b24`).**
초기 커밋에서 `SpriteSequencePlayable`의 파라미터 타입이 폐기 예정의 `SpriteComponent*`로 선언된 것을 신규 `SpriteRenderer*`로 수정하고, 관련 include 경로와 주석을 일괄 갱신하였다.

**`SetActiveCamera` / `Camera::Depth` 호출 제거 (`ea921d8`).**
`_MyApp_`과 `migrate_demo`의 `main.cpp`에서 `dir.SetActiveCamera(cam)` 호출 2건과 `cam->Depth = ...` 대입 3건을 제거하였다. Camera `OnEnter` 훅이 자동 등록을 담당하므로 명시 호출이 불필요해졌다.

### 기술적 결정 및 이슈

**결정 1 — `sprite_sequence` 별도 모듈 대신 `SJH::sprite` 내 통합.**
spec §3.4에서는 `SJH::sprite_sequence`를 별도 STATIC 라이브러리로 분리하는 방향을 제시하였으나, `SpriteSequencePlayable`이 `SJH::sprite`의 `SpriteRenderer`에 직접 의존하는 구조상 모듈 경계를 분리할 실익이 없다고 판단하여 `SJH::sprite` 모듈 안에 `sprite_frame_clip.h`와 `sprite_sequence_playable.{h,cpp}`를 통합 정착시켰다. 이에 따라 `SJH::engine` 우산의 모듈 수는 16개로 확정(이후 timer, text 추가로 17개)되었다.

**결정 2 — Observer(SP6) 완전 제거와 Lifetime 컨벤션 전환.**
`Program`과 `Material` 간의 Observer 패턴은 설계 복잡도에 비해 실익이 없다고 판단하였다. `Material`이 `Program`보다 먼저 소멸하는 것을 ResourceRegistry가 보유 순서로 보장하는 방식(Lifetime 컨벤션)으로 대체하였다. 이 전환 과정에서 Observer와 무관한 `GetProgram()` 참조가 함께 삭제되었다가 빌드 실패 후 복구되는 시행착오가 있었으며, 이를 통해 "Observer 관련 코드"와 "단순 참조 코드"를 명확히 구분하는 계기가 되었다.

**결정 3 — M5 leaf Playable은 Client 거주.**
`FmodPlayable`, `FmodStudioPlayable`, `EffekseerPlayable`은 `game_deps`(FMOD/Effekseer)에 의존하므로 엔진 코어 모듈(`src/`)이 아닌 `apps/_MyApp_/src/`의 Client 영역에 거주하는 것으로 확정하였다. 엔진 코어의 `IPlayable` 인터페이스만 의존하므로 Core/Client 분리 원칙이 유지된다.

### 산출물

- 변경 모듈/영역: `src/playable` (신설), `src/sprite` (SpriteRenderer + SpriteSequencePlayable 통합), `src/render` (SceneContext 연동 슬림화), `src/scene` (SceneContext 신설, Camera/Light 자동 등록), `src/object` (Light OnEnter/OnExit), `src/material` (Observer 제거), `src/program` (Observer 제거), `src/common` (MAX_LIGHTS 상수 정비), `src/resource_registry` (GetAllPrograms, CreateUniformAtlas, Sound/Effect 래퍼), `apps/_MyApp_/src/` (Stage 도메인, Audio, VFX, Tween, Director)
- 커밋 19건 (주요: `7641fff` SJH::playable 신설, `9421562` Composite 신설, `c23dbda` SpriteSequencePlayable, `ea921d8` SceneContext 전환, `9b2b43a` M5 leaf 골격, `231b1cc` MAX_LIGHTS 정비, `e16cd17`·`0bd9aae`·`1dd7502` Observer 제거 3-step)

---

## 2026-05-27 (수)

**PostFX 2-카메라 아키텍처 전환 및 ImGui 레이어 분리**

### 개요

본 날은 `SJH::render` 모듈과 `_MyApp_` 데모 전반에 걸쳐 포스트 프로세싱(PostFX) 파이프라인을 전면 재설계한 날이다. 기존의 `SetPostFXChain` / `PostFXPass` 단일 경로 방식을 폐기하고, World 카메라(원근 투영)와 Screen 카메라(직교 투영) 두 개를 명시 순회하는 **2-카메라 + `PassComponent` 아키텍처**로 교체하였다. 동시에 ImGui UI를 `Game` / `Editor` 두 종류로 구분하는 레이어 스택 시스템을 도입하고, Client 측 `Director` 클래스를 `Manager`로 개명하여 역할과 명칭의 정합성을 높였다. 이 작업은 M3.5 Playable 코어 정착 이후 렌더링 파이프라인을 정비하는 단계의 일환이다.

### 개발 (Feature)

**PostFX 2-카메라 파이프라인 도입** (`beb40c6`): `src/render/pass_component.h` 에 `SJH::Scene::PassComponent`를 신설하였다. 이 컴포넌트는 화면 공간 `MeshRenderer`의 역할을 하며, `InputFB`와 `OutputFB` 포인터를 보유함으로써 Pass 간 프레임버퍼 공유(포인터 일치 = 파이프라인 경계)를 표현한다. `DrawCommand`에 `Kind::ScreenQuad` 분기를 추가하고, `MeshPassProcessor`가 `ScreenQuad` 커맨드를 받으면 `outputFB`로 `BeginFrame`을 전환하여 `uScene` 텍스처를 바인딩하도록 구현하였다. `Scene::Layer::Screen`(`1ull<<5`) 레이어를 신설하고, `Camera`에 `IsOrthographic` 필드와 직교 투영 행렬 직접 구현(vmath 미지원)을 추가하였다.

**PostFX 셰이더 세트 추가** (`e2ff1f6`): Blur, Gamma, Invert, Sharpening, Sobel 5종의 포스트프로세스 셰이더(`resources/shader/postprocess/`)를 신설하고, 설계 문서 `doc/design/PostFX.md`를 작성하였다.

**ImGui 레이어 분리** (`3a93a05`): `IImGuiLayer` 인터페이스(`ImGuiLayerKind::Game / Editor` 구분), `ImGuiLayerStack`(`RenderAll(bool showEditor)` 필터링), `PostFXDebugLayer`(PostFX 패스 Enabled 토글 + gamma 슬라이더)를 `apps/_MyApp_/src/UI/` 하위에 신설하였다. HUD 등 항상 표시되는 게임 UI와 디버그용 에디터 UI를 런타임에 선택적으로 렌더링할 수 있는 구조를 갖추었다.

**`ScreenQuadStage` / `LightUniformDispatcher` 분리** (`30a1e5f`): 최종 화면 출력 단계를 `ScreenQuadStage`로 독립시키고, 조명 uniform 업로드 로직을 `SceneRenderer`에서 분리하여 `LightUniformDispatcher`로 위임하였다.

### 리팩토링

**`CameraStage` + IRenderStage 패턴 전환** (`9a650b9`): `SceneRenderer::RenderWithCamera`를 `public`으로 노출하고, `CameraStage`(`src/render/camera_stage.h/.cpp`) 를 새로 도입하여 단일 카메라를 `IRenderStage`로 감쌌다. `main.cpp`의 렌더 루프는 `std::vector<unique_ptr<IRenderStage>>`를 순회하는 방식으로 전환하여, `mScreenQuadStage` 단독 멤버를 `mStages` 컬렉션으로 흡수하였다. 기존 `Render(rt)` 자동 순회 경로는 `[[deprecated]]` 어트리뷰트를 부여하여 점진적 마이그레이션을 유도하였다. 시각 결과는 전환 전과 동일하다.

**`Director` → `Manager` 개명** (`4203309`): Client 측 `Director.h/.cpp`를 `Manager.h/.cpp`로 교체하였다. 엔진 레이어의 `Director` 개념과 혼동을 방지하고, 클라이언트 내 씬/오브젝트 총괄자 역할을 명확히 하기 위한 명명 정리이다.

**`main.cpp` 분할** (`30a0979`): 비대해진 `main.cpp`를 역할별로 분할하여 가독성을 개선하였다.

### 수정 (Fix / Chore)

- `cmake/CXXStandard.cmake`에 `-Wno-error=deprecated-declarations` 옵션을 추가하여(`9a650b9`), `[[deprecated]]` 어트리뷰트가 붙은 API를 사용하는 `migrate_demo` / `audio_demo`가 `-Werror` 환경에서도 빌드를 통과하도록 조치하였다.
- `mesh_pass_processor.cpp`에 `vmath.h` 직접 include를 명시하여 clangd `MissingIncludes: Strict` 정책 위반을 해소하였다(`beb40c6`).
- `blurring.fs` 셰이더 정리 및 `postfx_pass.h` 폐기(`b7bced4`): 구 `PostFXPass` 구조체 헤더를 삭제하여 신 아키텍처 전환을 완결하였다.

### 기술적 결정 및 이슈

1. **`PostFXPass` 체인 방식 폐기 → `PassComponent` Actor 방식 채택**: 기존 `SceneRenderer::SetPostFXChain`은 렌더러가 PostFX 순서를 직접 소유하는 방식이었다. 이를 씬 그래프 Actor에 `PassComponent`를 추가하는 방식으로 전환함으로써, PostFX 패스가 씬 그래프의 일급 객체로 편입되었다. 결과적으로 `SetPostFXChain / RunPostFXChain / ClearPostFXChain / GetActiveFXOutput` 등 4개의 렌더러 API가 제거되어 `SceneRenderer`의 책임이 줄었다. 이 패턴은 Cocos2d-x `RenderTexture` / Unity Camera Stack의 설계 정통을 따른다.

2. **직교 투영 행렬 직접 구현**: `vmath`가 `ortho` 함수를 지원하지 않거나 부호 버그가 있음을 확인하여, `camera.cpp`에서 직교 투영 행렬을 직접 계산하도록 구현하였다. 이는 외부 수학 라이브러리의 한계를 코드 레벨에서 명시적으로 우회한 사례이다.

3. **ImGui Game/Editor 이중 레이어 설계**: 단일 렌더 루프에서 `showEditor` 플래그 하나로 디버그 창의 노출을 제어하는 구조를 채택함으로써, 릴리스 모드와 디버그 모드 전환 시 UI 소스를 따로 컴파일할 필요가 없어졌다. `ImGuiLayerStack::RenderAll(bool)` 한 곳에서 필터링을 집중 처리하는 방식이다.

### 산출물

- 변경 모듈/영역: `src/render` (PassComponent, CameraStage, LightUniformDispatcher, ScreenQuadStage, SceneRenderer, MeshPassProcessor), `src/scene` (Camera, Layer), `apps/_MyApp_/src/UI` (IImGuiLayer, ImGuiLayerStack, PostFXDebugLayer), `apps/_MyApp_` (main.cpp 분할, Manager 개명), `cmake/CXXStandard.cmake`, 포스트프로세스 셰이더 5종
- 커밋 9건 (주요: `beb40c6` 2-카메라 PassComponent 아키텍처 전환, `3a93a05` Editor/Game GUI 분리, `9a650b9` CameraStage+IRenderStage 패턴 도입, `30a1e5f` LightUniformDispatcher 분리 + ScreenQuadStage 신설)

---

## 2026-05-28 (목)

**매트릭스 스카이박스·PCB 모델 도입 및 스테이지 리소스 교체**

### 개요

본 날의 작업은 M3.5 Playable 코어 정착(2026-05-26) 이후의 비주얼 보강 단계로, 탑다운 슈터 게임 `_MyApp_`의 씬 배경을 의미 있는 3D 자원으로 교체하는 것을 목표로 하였다. 구체적으로는 두 가지 작업이 병행되었다. 첫째, 화면 전체를 감싸는 매트릭스 테마 스카이박스(`MatrixSkybox`) 오브젝트를 Warmup 패턴으로 씬에 등록하고 매 프레임 카메라 위치와 시간 uniform을 동기화하는 런타임 갱신 루프를 구축하였다. 둘째, 스테이지 바닥·벽·픽업 오브젝트에 붙어 있던 프로시저럴 평면 메시(`stage_plane`) 기반 렌더링을 Assimp로 로드한 `pcb.fbx` 3D 모델로 교체하여 스테이지의 시각적 구체성을 높였다. 포스트 프로세싱 패스도 감마 보정(`gamma.fs`) 대신 포그 효과(`fog.fs`)로 전환함으로써 매트릭스 세계관의 분위기를 일관되게 맞추었다. 비코드 파일 175개 변경분(셰이더, 텍스처, 모델 등 리소스 추가)이 커밋에 포함되어 있으며, 이는 `resources/shaders/matrix_skybox.{vs,fs}`, `resources/texture/characters.png`, `resources/texture/matrix_noise.png`, `resources/model/pcb.fbx` 등의 신규 자산 도입에 해당한다.

### 개발 (Feature)

**매트릭스 스카이박스 도입.** `main.cpp`에 `WarmupSkybox(SJH::ResourceRegistry&, SJH::Scene::Director&)` 메서드를 신설하였다. 이 메서드는 `reg.CreateProgram("matrix_skybox", …)` 호출로 전용 셰이더 프로그램을 등록하고, `reg.CreateTexture`를 통해 문자(chars) 텍스처와 노이즈(noise_tex) 텍스처를 `SJH::ResourceRegistry`에 캐싱한다. `reg.CreateSharedMaterial("mat_matrix_skybox")`로 생성된 `SJH::Material`에는 두 텍스처와 함께 `u_time` float property가 초기값 `0.0f`로 등록된다. 스카이박스 메시는 `SJH::Mesh::CreateBox()`로 생성한 박스 메시를 사용하며, 스케일을 `glm::vec3(50.0f, 50.0f, 50.0f)`로 설정하여 카메라 클리핑 범위 안에서 씬 전체를 덮도록 하였다. `SJH::Scene::MeshRenderer` 컴포넌트로 머티리얼을 부착한 뒤 `dir.Root().AddChild`로 씬 루트에 등록하고, 반환된 포인터를 멤버 `mSkyboxActor`와 `mSkyboxMat`에 보관한다.

**런타임 동기화 루프 추가.** `render` 루프 안에 두 개의 조건부 갱신 블록을 삽입하였다. `mSkyboxMat->Properties.Floats["u_time"] = static_cast<float>(currentTime)`로 매 프레임 경과 시간을 셰이더에 전달하여 애니메이션 효과를 구동하고, `mSkyboxActor->GetTransform().Translate = mCamera->GetOwner()->GetTransform().Translate`로 스카이박스의 위치를 카메라와 동기화하여 플레이어가 이동하더라도 배경이 항상 카메라를 중심으로 유지되도록 하였다.

**PCB 모델 스테이지 배치.** `StageBuilder.cpp`에 `EnsurePcbModel(SJH::ResourceRegistry&)` 헬퍼를 추가하고, `StageConfig`에서 전달받은 `ResourceRegistry`에 `"stage_pcb"` 키로 `pcb.fbx` 모델을 idempotent하게 등록한다. 로드된 `SJH::Model`의 머티리얼 슬롯을 순회하여 `GetProgram() == nullptr`인 슬롯에 `solidProg`를 주입함으로써 Assimp 로드 시 셰이더 미할당 상태를 방어적으로 처리하였다. `SJH::Scene::ModelSpawner::SpawnEntities(*pcbActor, *pcbModel)` 유틸리티를 사용하여 모델의 모든 `RenderUnit`을 자식 액터로 전개한 뒤 스테이지 루트 액터에 자식으로 편입하였다. 배치 트랜스폼은 `SetTransformWithVectors(vec3(0, -1.75, 0), vec3(90, 0, 0), vec3(0.75, 0.75, 0.75))`으로 수평 모델을 X축 90° 회전하여 바닥 정렬하고 적절한 스케일로 조정하였다.

**카메라 팔로우 오프셋 조정.** `TargetFollowableCameraController::SetFollowOffset`의 Y/Z 값을 `(0, 5, 5)`에서 `(0, 10, 10)`으로 상향하여 스카이박스 및 PCB 모델 배치 후의 씬 뎁스에 맞게 시야를 넓혔다.

### 리팩토링

`StageBuilder.cpp`에서 프로시저럴 `plane` 메시 + `wallMat` / `pickupMat` Material로 구성된 벽·픽업 렌더러 추가 코드를 주석 처리하고 PCB 모델 기반 배치로 전환하였다. 기존에는 `EnsurePlane`, `EnsureMaterial` 두 함수로 생성한 단색 평면이 벽과 픽업 액터에 `AddComponent<MeshRenderer>` 형태로 부착되었으나, 이 날 이후에는 해당 컴포넌트 추가 코드가 제거되고 스테이지 배경 오브젝트 역할을 FBX 모델이 담당한다. `StageBuilder.h`의 Doxygen 주석도 등록 자원 목록에 `pcb model` 및 키 `"stage_pcb"`를 추가 반영하여 문서와 구현의 동기화를 유지하였다.

### 수정 (Fix / Chore)

포스트 프로세싱 패스 등록 항목에서 셰이더 경로 오류를 수정하였다. 기존 `{"gamma", "./resources/shader/postprocess/gamma.fs"}`는 실제 파일 경로(`resources/shaders/postprocess/fog.fs`, 슬래시 구조 상이)와 불일치하였다. 이를 `{"fog", "./resources/shaders/postprocess/fog.fs"}`로 교체하고, 감마 항목은 주석으로 보존하였다.

### 기술적 결정 및 이슈

**스카이박스 위치 카메라 추적 방식.** 일반적으로 스카이박스는 뷰 행렬에서 이동 성분을 제거하는 셰이더 기법(뷰 행렬의 3×3 부분만 사용)으로 구현되나, 본 구현에서는 스카이박스 액터의 월드 위치를 매 프레임 카메라 위치와 동기화하는 CPU 측 갱신 방식을 채택하였다. 이는 기존 `SceneRenderer` 파이프라인의 변경 없이 적용 가능한 단순한 접근으로, 엔진 내부의 View/Projection 행렬 생성 경로에 스카이박스 전용 처리를 삽입하지 않아도 된다는 이점이 있다. (해당 방식은 2026-05-31 셰이더 기반으로 재전환된다.)

**Assimp 모델 셰이더 주입 패턴 확립.** `SJH::ResourceRegistry::CreateModel`이 반환하는 `SJH::Model`은 Assimp가 파싱한 머티리얼 정보를 담고 있으나, 자체 엔진 셰이더(`SJH::Program`)는 별도로 연결해야 한다. `GetMaterialCount` 루프로 미할당 슬롯에만 셰이더를 주입하는 방어 코드는 이후 텍스처 전용 셰이더로 교체할 때도 동일한 경로를 재사용할 수 있도록 확장성을 고려한 설계이다.

### 산출물

- 변경 모듈/영역: `apps/_MyApp_/main.cpp` (스카이박스 Warmup + 런타임 동기화), `apps/_MyApp_/src/Stage/StageBuilder.{h,cpp}` (PCB 모델 배치 + plane 렌더러 교체), 비코드 리소스 175파일 (셰이더·텍스처·모델·빌드 아티팩트)
- 커밋 1건 (`3548990` [dev] : 리소스 정리)

---

## 2026-05-31 (일)

**렌더 파이프라인 추출·스카이박스 통합·M6 단발 FX 스폰 인프라 구축**

### 개요

본 일차는 `_MyApp_` 탑다운 슈터 데모의 세 가지 축을 병행하여 진행하였다. 첫째, `main.cpp` 에 인라인으로 존재하던 렌더 부트스트랩 코드를 엔진 코어 자유 함수로 추출하여 약 130줄을 감축하였다. 둘째, 스카이박스 위치 동기화를 셰이더 처리로 전환하고 `TargetFollowableCameraController` 를 `ActorFolower` 로 개명하였다. 셋째, M6 단발 시퀀스 스폰 인프라(`Spawns` 라이브러리)를 신설하여 Effekseer VFX·FMOD Studio 오디오 일회성 재생과 자동 Actor 파괴를 구현하였다. 또한 `ParticleStage` 를 렌더 스테이지 컬렉션에 통합함으로써 Effekseer 파티클이 `sceneFB` 에 정식으로 합성되기 시작하였다. 이 모든 작업은 마일스톤 M5(Effekseer/FMOD leaf Playable) 및 M6(단발 FX 스폰) 진입 흐름의 일부이다.

### 개발 (Feature)

**ParticleStage 렌더 스테이지 통합** (`770215a`): `apps/_MyApp_/src/VFX/ParticleStage.{h,cpp}` 를 신규 작성하였다. `IRenderStage` 를 상속하며, `worldCam->GetTargetRenderTarget()` 으로 `sceneFB` 를 동적 도출하여 `DeviceContext::BindTarget` 후 `VFXSystem::Draw` 를 호출한다(NoClear — 3D 월드 드로 결과 보존). 렌더 스테이지 순서가 `[worldCam, ParticleStage, screenCam, ScreenQuadStage]` 4-element 로 확정되었다.

**Actor 트리 탐색·소유권 이전 API** (`2f8bbf2`): `src/scene/actor.{h,cpp}` 에 `DetachChild` (소유권 반환, Godot `remove_child` 정통), `FindChild` (이름 재귀 탐색, Cocos `getChildByName` 정통), `FindChildIf<Fn>` (술어 탐색, 템플릿) 세 API를 추가하였다.

**M6 Spawns 라이브러리 신설** (`12f269f`, `560fdff`, `abc2427`, `50be686`, `ca1f16c`): `apps/_MyApp_/src/Spawns/` 하위에 다음 구성 요소를 작성하였다.
- `SequenceContext` — 의존성 묶음 구조체 (VFXSystem·AudioSystem·ResourceRegistry·b2World·sceneRoot·fxRoot).
- `AutoDespawnOnFinish` — `IPlayable::IsFinished` 를 매 프레임 폴링하여 `mDone` 마킹하는 Component.
- `OneShotSweeper` (`SweepFinishedChildren`) — `FindChildIf` 로 `mDone` 자식을 찾아 `RemoveChild` 로 파괴하는 end-of-frame 클린업 자유 함수.
- `AudioInstance` (`SpawnAudioInstance`) — `fxParent` 에 Actor 스폰 후 `FmodStudioPlayable` + `AutoDespawnOnFinish` 부착·`Play`.
- `VfxInstance` (`SpawnVfxInstance`) — 동일 패턴, Effekseer Effect 재생.
- `CombatSequences` — `SpawnHitSpark`·`SpawnEnemyDeathFX`·`SpawnPickupChime` 조합 팩토리.
- `AmbientSequences` (`BuildBGM`) — BGM 전용 지속 루프 (fxRoot 미사용, `sceneRoot` 직접 부착).

**FMOD 3D Listener 갱신** (`560fdff`): `AudioSystem::SetListener` 를 추가하여 매 프레임 카메라 Transform 위치로 FMOD Studio + FMOD Core 양쪽의 리스너를 갱신하도록 하였다. `FmodStudioPlayable` 에 `std::optional<glm::vec3> worldPos` 파라미터를 추가하여 3D 사운드 위치를 주입할 수 있게 하였다.

**fog/bloom PostFX 셰이더 통합** (`1ac0f26`): 기존 `uDepth` 의존을 제거하고 `vUV.y` 기반 스크린 공간 근사로 재작성한 `fog.fs` 와 Rec.709 휘도 기반 bright-pass `bloom.fs` 를 `POSTFX_PROGRAM_CONFIGS` 체인에 추가하였다. `PostFXDebugLayer` 에 fog/bloom 전용 `SliderFloat`·`ColorEdit3` 파라미터 슬라이더를 추가하였다.

**플레이어 텍스처 상수 정의** (`560fdff`): `Playable/Constants.h` 에 4방향×아이들/이동 조합 `PlayerTextureConfig` 구조체 배열 8종을 정의하였다.

### 리팩토링

**렌더 부트스트랩 엔진 코어 추출** (`eee2fab`): `main.cpp` 의 인라인 Warmup 메서드 3개를 `SJH::Render` 네임스페이스 자유 함수로 이주하였다.
- `GetFramebufferInfo` → `src/common/window_helper.{h,cpp}` 신규 (GLFW forward decl 격리, `FramebufferInfo` 패킹 반환).
- `SetupDefaultPipeline` → `src/render/render_pipeline.{h,cpp}` 신규 (passthrough Program·ScreenQuad Mesh·bypass Material 등록 + `ScreenQuadStage` UPtr 반환).
- `BuildPostFXChain` → 동일 파일 (PostFX 단계별 Program/Material/Framebuffer 생성 + PassActor 배선 + `PostFXChainResult` 반환).
- `CreateScreenCameraActor`·`CreateSkyboxActor` → `src/scene/compound_actor.{h,cpp}` 에 추가 (PreBuilt factory 컨벤션 일치).
- `PostFXStageConfig.InitFloats` 도입으로 gamma 특수 케이스를 제거하고 data-driven 초기값 일반화(D-6).
- 결과: `main.cpp` 순감소 약 130줄.

**스카이박스 위치 동기화 셰이더 위임** (`e65a715`): 매 프레임 CPU에서 `mSkyboxActor` 위치를 카메라에 맞추던 코드를 제거하고, 셰이더가 view 행렬의 이동 성분을 제거하는 방식으로 전환하였다.

**`TargetFollowableCameraController` → `ActorFolower` 개명** (`e65a715`): 클래스 역할을 보다 범용적으로 나타내도록 이름을 변경하고 `SetFollowRotate(glm::vec2)` Fluent setter를 추가하였다.

**BGM 인라인 → `Spawns::BuildBGM` 이관** (`ca1f16c`): `main.cpp` 에 인라인이었던 BGM 배선을 `Spawns::BuildBGM` 팩토리로 이관하였다.

### 수정 (Fix / Chore)

- **fog UV 방향 반전** (`e740b29`): `fog.fs` 의 `(1.0 - vUV.y)` 가 화면 하단을 멀게 처리하는 오류를 수정하여 `vUV.y` 방향으로 상단(지평선 방향)이 안개에 쌓이도록 수정하였다.
- **PostFXDebug ImGui 패널 위치·크기 명시** (`770215a` 내 fix): ExitButton `(64,64)` 과의 겹침으로 클릭이 인식되지 않던 문제를 `SetNextWindowPos((20,140), FirstUseEver)` + `SetNextWindowSize((280,220), FirstUseEver)` 명시로 해결하였다.
- **PostFX 5단계 복구 및 셰이더 경로 오타 수정** (`770215a` 내 fix): 단독 fog만 활성화된 `POSTFX_PROGRAM_CONFIGS` 를 5단계(blurring/gamma/invert/sharpening/sobel) 체인으로 복원하고, `shader` 단수 경로를 `shaders` 복수로 일괄 수정하였다.
- **`ImGuiStack.Push` 누락 복구** (`770215a` 내 fix): `ExitButtonLayer`·`PostFXDebugLayer` 두 Push 호출이 누락된 것을 `WarmupImgui` 안으로 통합하여 복구하였다.
- **PCB 모델 셰이더 교체** (`eee2fab`): 단순 solid 셰이더를 `phong_albedo.fs` 로 교체하고, `model.cpp` 에서 `aiColor_Diffuse` 를 `material.albedo` (vec3)로 항상 추출하도록 수정하였다.
- **`ParticleStage` CMake PUBLIC/PRIVATE 조정** (`770215a` 내 fix): `SJH::scene` 의존이 `.cpp` 내부 전용임을 확인하여 `PRIVATE` 으로 강등, 불필요한 의존성 누수를 차단하였다.

### 기술적 결정 및 이슈

1. **단발 FX 자동 파괴 패턴 — Actor 비상속 조합 컨벤션 적용**: 단발 이펙트/사운드 재생은 새 Actor 클래스를 정의하지 않고 `FmodStudioPlayable` + `AutoDespawnOnFinish` 두 Component를 동일 Actor에 조립하는 방식으로 구현하였다. 씬 Update 이후 `SweepFinishedChildren` 을 별도 패스로 호출함으로써 Update 중 반복자 무효화 문제를 원천 차단하였다(Cocos end-of-frame cleanup 정통). `FindChildIf` 는 매 호출 새 스캔을 수행하므로 `RemoveChild` 직후 재호출해도 안전하다.

2. **`PostFXStageConfig.InitFloats` 일반화(D-6) — 단, non-float 초기값의 한계**: `BuildPostFXChain` 의 `InitFloats` 맵은 `float` 타입 uniform 초기값만 지원하므로, fog의 `uFogColor`(vec3)와 `uFogMode`(int)는 `BuildPostFXChain` 호출 이후 `mPassComponents` 를 직접 순회하여 별도로 초기화하는 패치가 필요하였다. 이는 현재 `PostFXStageConfig` 가 `InitVec3s`·`InitInts` 필드를 제공하지 않는 데서 기인한 미완성으로, 향후 해소 여지가 있다.

3. **`SJH::scene` 직접 의존 명시 필요성**: `compound_actor.cpp` 가 `render/mesh_renderer.h` 를 포함하면서 `SJH::render` 에 의존하는데, 이를 transitive 체인에 암묵적으로 의존하는 것은 아키텍처 안전성을 해친다. 이에 `src/scene/CMakeLists.txt` 에 `SJH::render` 를 명시적 PRIVATE link로 추가하여 upstream 모듈의 의존성 변경에 의한 빌드 깨짐을 예방하였다.

### 산출물

- 변경 모듈/영역: `apps/_MyApp_` (게임 로직·렌더 스테이지·UI), `src/scene` (Actor API), `src/common` (window_helper 신규), `src/render` (render_pipeline 신규), `src/object` (model, transform)
- 커밋 11건 (주요: `770215a` ParticleStage 신규, `eee2fab` Warmup 보일러플레이트 추출, `2f8bbf2` Actor 트리 탐색 API, `12f269f` Spawns lib 골격, `50be686` 단발 빌더 CombatSequences/AmbientSequences, `1ac0f26` fog/bloom PostFX 통합)

---

## 2026-06-01 (월)

**`SJH::timer` 코어 승격과 PlayerBehavior 분해 선행 인터페이스 정착**

### 개요

본일은 탑다운 슈터 `_MyApp_` 의 엔티티 도메인 정비를 집중적으로 수행한 날이다. 핵심 성과는 두 축으로 나뉜다. 첫째, 클라이언트(`apps/_MyApp_/src/Timer/`)에 임시 stub으로만 존재하던 타이머 클래스를 `SJH::timer` 코어 모듈로 승격하고, `SJH::engine` 우산에 합류시켜 엔진 레이어에서 타이머를 공식 지원하도록 하였다. 둘째, M4~M5 마일스톤의 선행 작업으로 `PlayerBehavior` 분해(god-component 해체)를 위한 인터페이스 정의(Step0·Step1)와 `Life` 컴포넌트 강화를 수행하였다. 이와 병행하여 입력 리팩토링, `SpriteRenderer` 피격/디졸브 FX 배선, `EffekseerDiagnostics` 신설, 적 웨이브 빌더 분리 등 다수의 기능 보완이 이루어졌다.

### 개발 (Feature)

**`SJH::timer` 모듈 신설 — 커밋 `b46684d`, `c4dc4b7`, `c813612`, `03c2851`**

- `src/timer/timer.h` 에 `SJH::Timer::Timer` 클래스를 구현하였다. C# `TimerComposite` 를 C++17로 포팅한 순수 시간 누적기로, `SetAcceleration`/`SetInterval` Fluent Builder, `Tick(dt)`, `IsTimesUp()`/`PollInterval()`/`IsBlocked()` 조회 API를 제공한다. `PollInterval()`은 한 프레임에 경계를 복수 초과하더라도 1회만 보고하는 이연 발사 계약을 명시하였다(`21cba08`).
- `src/timer/multiple_timer.h` 에 `SJH::Timer::MultipleTimer : Component` 를 구현하였다. `unordered_map<string, Timer>` 를 소유하며, `Register(key, Timer)`/`Find(key)` 로 핸들 기반 타이머 관리를 지원한다. `Update(dt)` 에서 모든 Timer를 중앙 Tick하여 엔티티 각처에 분산된 raw-float 타이머 산술을 일원화하는 기반을 마련하였다.
- `c813612` 에서 `SJH::timer` INTERFACE 모듈 CMakeLists를 작성하고, `03c2851` 에서 `SJH::engine` 우산에 합류(16 모듈)시켰다.

**`IActorPresentation`·`IImpulsable`·`EFacing`/`EPose` 인터페이스 — 커밋 `555d5e4`**

- `Components.Interfaces.h` 에 연출 sink 인터페이스 `IActorPresentation`(ReactDamaged/ReactDied/ReactAttack/FaceAim/SetFacing/SetPose 가상 함수)과 일회성 속도 버스트 인터페이스 `IImpulsable(DoImpulse)`을 추가하였다.
- 4방향 양자화 free function `Quantize4(vec2)` 를 `EFacing` enum과 함께 정의하였다. `|x|>|z|` 이면 좌우, 아니면 전후로 분기한다.

**`Life` 컴포넌트 강화 — 커밋 `a4a9fd4`, `ae70c8a`**

- i-frame(`mIFrameSeconds`/`mInvincibleTimer`), 사망 연출 지연(`mDeathDelaySeconds`/`mDying`/`mDeathTimer`), `IActorPresentation* mSink` OnEnter 캐시를 `Life` 에 통합하였다. `DoDamaged` 가 i-frame 조건을 먼저 확인하고, `mSink->ReactDamaged(damage)` 를 Template-Method 패턴으로 forward한다.

**`EffekseerDiagnostics` 신설 — 커밋 `1e7a076`**

- `src/diagnostics/` 에 `EffekseerDiagnostics` static 클래스를 추가하였다. `.efk` 바이너리를 직접 파싱하여 UTF-16LE 텍스처 참조를 추출하고 파일 존재를 검증하는 `CheckEffectTextures`, 핸들 유효성을 확인하는 `CheckPlayHandle`/`CheckHandleAlive` API로 구성된다. GL 및 Effekseer 헤더에 의존하지 않아 어떤 시점에서도 호출 가능하다.

**적 빌더 분리 및 웨이브 통합 — 커밋 `6a3151f`**

- `apps/_MyApp_/src/Bootstrap/EnemyBuilder.{h,cpp}` 를 신설하여 `CreateEnemyActor` 직접 호출을 `BuildEnemy(EnemyDeps)` 팩토리로 캡슐화하였다. `ENEMY_FRONT` 아틀라스 로드·`SpriteSequencePlayable` 애니 배선·씬 부착을 한 함수에 통합하였다.
- `main.cpp` 에서 `WaveController` 를 씬 트리에 부착하여 자동 tick 기반 웨이브 스폰을 활성화하였다.

### 리팩토링

**`GetComponent` 게이트 완화 — 커밋 `6d101cf`**

- `src/scene/actor.h` 의 `GetComponent<T>` template 제약을 `is_base_of_v<Component, T>` 에서 `is_polymorphic_v<T>` 로 변경하였다. 이전에는 순수 인터페이스(`IActorPresentation`, `IDamageable` 등)로 컴포넌트를 조회할 수 없었으나, 변경 후 인터페이스 타입으로도 조회가 가능해졌다. Fast-path는 `if constexpr (is_base_of_v<Component, T>)` 가드로 보호하여 인터페이스 조회 시 컴파일 오류가 발생하지 않도록 하였다.

**입력 아키텍처 정리 — 커밋 `3085144`**

- `main.cpp` 의 `onKeyPress`/`onMouseButton` 콜백에 인라인으로 작성되어 있던 Composite 연출 로직(Shot Composite, Damage Composite)을 `PlayerController` 콜백(`onFire`/`onDamage`) 주입 방식으로 분리하였다. `PlayerController` 에 `SetMouseInput`, `SetFireCallback`, `SetDamageCallback` API를 추가하고 `ControllerCfg` 에 콜백 필드를 반영하였다.

**`SpriteSequencePlayable` 값-소유 ctor — 커밋 `74d065c`**

- `SpriteSequencePlayable` 생성자가 `SpriteFrameClip` 포인터를 받던 방식을 값(복사)으로 변경하여 clip의 lifetime footgun을 제거하였다. `PlayerBuilder` 의 `clipStorage` 멤버 및 `main.cpp` 의 `mWholeAtlasClip` 도 함께 제거하였다.

### 수정 (Fix / Chore)

**구 Timer stub 제거 — 커밋 `93a487d`**

- `apps/_MyApp_/src/Timer/timer.h` (288줄, 대부분 주석 처리된 C# 레퍼런스 코드)를 삭제하였다. 코어 `SJH::timer` 승격으로 클라이언트 stub이 불필요해진 데 따른 정리이다.

**`SpriteRenderer` 피격/디졸브 uniform 배선 — 커밋 `ae70c8a`**

- `src/sprite/sprite_component.h/.cpp` 에 `enableHit`/`enableDissolve`/`dissolveThreshold` 등 FX 공개 멤버와 `mEffectClock` 내부 clock을 추가하고, `Update(dt)` 에서 `uEnableHit`/`uTime`/`uEnableDissolve` 등을 `PropertyBlockSetter` 를 통해 매 프레임 업로드하도록 하였다. 이전에는 셰이더 uniform이 업로드되지 않아 피격 깜빡임과 디졸브 연출이 무반응 상태였다.

### 기술적 결정 및 이슈

**`SJH::timer` 헤더 전용 INTERFACE 모듈 선택**

`Timer`/`MultipleTimer` 는 모두 `.h` 만으로 구현되어 별도 `.cpp` 가 없다. CMakeLists에서 `STATIC` 대신 `INTERFACE` 라이브러리로 등록하고 `SJH::engine` 우산에 합류시켰다. 헤더 전용 모듈은 링크 심볼 없이 include 경로만 전파되므로, 모든 `SJH::engine` consumer가 추가 설정 없이 타이머를 사용할 수 있다. 그러나 `MultipleTimer` 가 `scene/actor.h(Component)`를 포함하므로 `project_deps` 전파에 의존한다는 점이 의존 방향 상 유의 사항이다.

**`GetComponent` 게이트: `is_polymorphic_v` vs `is_base_of_v<Component, T>`**

인터페이스 조회 허용을 위해 게이트를 `is_polymorphic_v<T>` 로 완화하되, Fast-path 내부의 `static_cast<T*>(Component*)` 가 인터페이스-Component 간 무관 타입 캐스트로 컴파일 에러를 일으키는 문제를 `if constexpr (is_base_of_v<Component, T>)` 로 차단하였다. 이 패턴은 Unity `GetComponent<IInterface>()` 정통 사용성을 C++17 template 메타프로그래밍으로 구현한 것이다.

**`EffekseerDiagnostics` — Effekseer 헤더 미포함 진단 설계**

Effekseer 텍스처 참조 누락은 `Effect::Create` 가 조용히 `nullptr` 를 반환하는 형태로 나타나 디버깅이 어렵다. `EffekseerDiagnostics` 는 Effekseer 런타임에 전혀 의존하지 않고 `.efk` 바이너리를 직접 파싱하여 참조 경로를 추출하므로, 로드 실패 원인을 사전에 진단할 수 있다. 이는 `src/diagnostics` 모듈의 "GL·엔진 비의존 진단" 컨벤션을 동일하게 적용한 것이다.

### 산출물

- 변경 모듈/영역: `src/timer` (신설), `src/scene/actor.h`, `src/sprite/sprite_component`, `src/diagnostics/effekseer_diagnostics` (신설), `apps/_MyApp_/src/Entity/Components` (`Life`, `WeaponComponents`, `Components.Interfaces.h`), `apps/_MyApp_/src/Bootstrap` (`PlayerBuilder`, `EnemyBuilder` 신설), `apps/_MyApp_/src/InputHandler/PlayerController`, `apps/_MyApp_/src/VFX/EffekseerPlayable`
- 커밋 29건 (주요: `b46684d` feat(timer): SJH::Timer::Timer 순수 시간 누적기, `c4dc4b7` feat(timer): SJH::Timer::MultipleTimer Component, `03c2851` build(engine): SJH::timer 우산 합류, `6d101cf` GetComponent 게이트 is_polymorphic 완화, `555d5e4` IActorPresentation+EFacing/EPose 인터페이스, `1e7a076` EffekseerDiagnostics 신설, `93a487d` 구 Timer stub 제거)

---

## 2026-06-02 (화)

**World Text(`SJH::text`) 코어 모듈 신설 및 엔진 17모듈 완성**

### 개요

탑다운 슈터 데모 `_MyApp_` 에서 데미지 수치·알림 등을 월드 공간에 표시하기 위한 텍스트 렌더링 기반을 마련한 날이다. `SJH::text` 엔진 코어 모듈(`BitmapFont` + `TextRenderer`)을 신설하고 `SJH::engine` INTERFACE 우산에 편입함으로써 모듈 수가 16개에서 17개로 확정되었다. 클라이언트 레이어에서는 `MyApp::Text::WorldTextSystem` 을 `Manager` 에 배선하고, Tweeny 트윈 상승·페이드 애니메이션과 자동 소멸을 갖춘 `SpawnWorldText` 스폰 함수를 구현하였다. 아울러 Doxygen 문서 시스템 도입과 GitHub Pages 자동 배포 워크플로도 함께 착수하여 문서화 인프라가 정비되었다.

### 개발 (Feature)

**`SJH::text` 모듈 — BMFont 로더 + TextRenderer**

`src/text/bitmap_font.h/.cpp` 에 `BitmapFont::LoadFromBMFont` 정적 팩토리를 구현하였다. BMFont 형식의 XML을 행 단위로 파싱하여 `<common>` 태그에서 `scaleW`, `scaleH`, `lineHeight` 를 추출하고, `<char>` 태그 목록에서 코드포인트별 `Glyph{frameIndex, xadvance}` 를 구성한다. 내부 파일 입출력은 크로스플랫폼 안전을 위해 바이너리 모드로 열며, 픽셀 단위 아틀라스 좌표를 `UniformAtlas` 그리드 프레임 인덱스로 변환하여 기존 스프라이트 파이프라인에 올려 탄다. `ResourceRegistry` 를 통해 중복 키가 있으면 기존 아틀라스를 재사용하는 Find-or-Create 패턴을 준수하였다.

`src/text/text_renderer.h/.cpp` 에 `TextRenderer : Component` 를 구현하였다. `SetText` 호출 시 `Rebuild` 가 기존 글리프 child Actor 를 모두 제거한 뒤, 문자열의 각 코드포인트마다 `SpriteRenderer` 를 보유한 child Actor 를 생성하여 X축으로 펼쳐 배치한다. 정렬은 중앙 앵커(`penX = -totalW * 0.5f`), 앵커 기준은 하단 중앙이며, 글리프 크기는 `mCharHeight / CellH()` 의 worldPerPx 계수로 일정하게 환산된다. `SetAlpha` 와 `SetColor` 는 모든 글리프 child 의 `tint` 를 일괄 변경하여 페이드 연출을 지원한다.

**`SJH::engine` 우산 편입 (16→17 모듈)**

`src/CMakeLists.txt` 의 `target_link_libraries(sjhopengl_engine INTERFACE ...)` 에 `SJH::text` 를 추가(`db45df2`)하여 `SJH::engine` 을 링크하는 모든 데모가 자동으로 텍스트 기능을 상속받도록 하였다.

**클라이언트 레이어 — `WorldTextSystem` + `SpawnWorldText`**

`apps/_MyApp_/src/Text/WorldTextSystem` 을 신설하고 `Manager::Init()` 에서 초기화 호출(`mWorldText.Init()`)을 추가하였다(`889138c`). `Init` 은 무인자로 정의하여 GL 헤더 포함 순서 충돌을 회피하였다. `SpawnWorldText` 자유 함수(`a28644f`)는 월드 좌표에 텍스트 Actor 를 스폰하고, `TweenPlayable<float>` 로 0→1 진행도를 구동하여 easeOutQuad 상승과 후반 페이드를 인라인 람다로 처리한다. 트윈 종료 후 `AutoDespawnOnFinish` 컴포넌트가 Actor 를 씬에서 자동으로 제거한다.

**Doxygen 문서 시스템 도입**

`cmake/Doxygen.cmake` + `doc/Doxyfile.in` 을 신설하고 루트 `CMakeLists.txt` 에 `SJH_OPENGL_BUILD_DOCS` 옵션 및 `doxygen` 커스텀 타겟을 wiring 하였다. `.github/workflows/doc.yml` 에서 `game/main` push 시 Ubuntu 러너에서 Doxygen + Graphviz 로 HTML 을 생성하고 GitHub Pages 로 자동 배포하는 파이프라인을 구성하였다(`d1269cd`).

### 리팩토링

**`CreatePlayerActor` 자유함수 분해 (`696265c`)**

`PlayerActor.cpp` 의 팩토리 함수를 컴포넌트 초기화 단계, 의존성 연결 단계, 조건 분기 함수화 세 부분으로 재분해하여 가독성을 높이고 단일 책임 원칙을 강화하였다.

**Physics BodyConfig 패턴 정착 (`3441d0f` 전후)**

`wall_factory.h` 에서 `b2BodyDef` / `b2PolygonShape` / `b2FixtureDef` 를 직접 조합하던 방식을 `BodyConfig` 구조체 하나로 대체하였다. Enemy, Bullet, Pickup 등 여러 팩토리가 동일 패턴으로 전환되어 Box2D 보일러플레이트가 대폭 감소하였다.

**튜닝 상수 중앙화 (`962f496`)**

`bullet_factory.h` 의 `BulletConfig` 기본값(`speed`, `damage`, `lifetime`)과 적 반지름·감쇠 계수를 `Entity/Constants.h` 의 `BULLET_*`, `ENEMY_*` 매크로로 교체하여 수치 조정의 단일 진입점을 확보하였다.

### 수정 (Fix / Chore)

- **PostFX 파라미터 튜닝 (`c311d17`)**: bloom(`uBloomThreshold=0.769`, `uBloomSpread=2.342`, `uBloomIntensity=0.927`)과 fog(`uFogDensity=0.042`) 수치를 조정하여 시각적 결과물을 정제하였다.
- **특수문자 → ASCII 화살표 (`3441d0f`)**: 코드 주석·Doxygen 설명의 `→` 기호를 `->` 로 일괄 치환하여 MSVC `/utf-8` 없는 환경에서의 컴파일 경고를 예방하였다.
- **Effekseer 1.7 리소스 추가 (`67c8df7`)**: 비코드 986개 파일(efk/텍스처/빌드 아티팩트)을 저장소에 편입하였다. 코드 변경 없이 리소스 번들만 추가한 커밋이다.

### 기술적 결정 및 이슈

**1. `TextRenderer` 는 `Component` 직접 상속 — `PlayableBase` 비의존**

텍스트 렌더러가 자체 애니메이션을 내장하면 `SJH::playable` 모듈에 대한 역방향 의존이 생긴다. 대신 `TextRenderer` 는 `SetAlpha`/`SetColor` 만 노출하고, 실제 트윈 구동은 `SpawnWorldText` 가 `TweenPlayable<float>` 을 별도 컴포넌트로 조합하는 방식을 채택하였다. 이로써 `SJH::text` 가 `SJH::playable` 을 include 하지 않아 모듈 의존 방향이 단방향으로 유지된다.

**2. `WorldTextSystem::Init` 무인자 설계 — GL 헤더 충돌 회피**

초기화 시 GL 로더 헤더(`gl3w.h`)가 `resource_registry.h` 경유로 간접 포함될 수 있어, Init 인자에 GL 관련 타입을 노출하면 포함 순서 위반 빌드 오류가 발생한다. 이를 무인자 메서드로 정의하고 내부에서 `ResourceRegistry::Get()` 을 직접 호출하는 것으로 외부 파라미터 노출을 원천 차단하였다.

**3. `SpawnWorldText` 의 easeOutQuad 인라인 구현**

Tweeny 의 `step(int32_t ms)` / `step(float ratio)` 혼동 함정을 피하기 위해 진행도 float 0→1 단일 트윈으로만 구동하고, 상승 보간은 `e = 1-(1-t)²` 수식을 람다 내부에서 직접 계산하였다. 이는 Tweeny easing enum 이 `vec3` 축별 보간에 부적합하기 때문이며, 부동소수점 직접 계산이 보간 단계 추가 없이 동일 효과를 낸다.

### 산출물

- 변경 모듈/영역: `src/text/` (신설 — `BitmapFont`, `TextRenderer`), `src/CMakeLists.txt` (16→17 모듈), `apps/_MyApp_/src/Text/` (신설 — `WorldTextSystem`), `apps/_MyApp_/src/Spawns/WorldTextInstance.cpp/.h`, `cmake/Doxygen.cmake`, `.github/workflows/doc.yml`, `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` (팩토리 분해), Physics 팩토리 다수 (`wall_factory.h` 등 BodyConfig 패턴)
- 커밋 18건 (주요: `a843e1b` World Text Core SJH::text 모듈 + BitmapFont, `4925410` TextRenderer 글리프 child 조립, `db45df2` SJH::engine 16→17 모듈 편입, `889138c` WorldTextSystem + Manager 배선, `a28644f` SpawnWorldText 트윈 상승·페이드 + 자동 despawn, `d1269cd` Doxygen 문서 시스템 + GitHub Pages 배포)

---

## 2026-06-03 (수)

**PlayerBehavior 분해 완료 + Timer 중앙화 + 대규모 리팩토링**

### 개요

이날은 탑다운 슈터 `_MyApp_` 의 PlayerBehavior god-component 분해를 완결짓고, 도메인 전반에 걸친 대규모 구조 개선을 수행하였다. 총 43건의 커밋(코드 +2327 / -1504줄)이 이루어졌으며, 크게 ① 접촉 핸들러·이동 컴포넌트를 `Carrier` 패턴으로 교체하여 god-component를 제거, ② `SJH::Timer`를 `BaseEntity` 중앙 컨테이너(`MultipleTimer`)로 통합, ③ 흩어진 매직 넘버를 도메인별 `Constants.h`로 단일화, ④ HUD 체력바 모듈 신설, ⑤ World Text·`UniformAtlas` 비정사각 타일 버그 수정, ⑥ 엔진 코어 `Camera::GetInverseProjectionMatrix()` 닫힌 해 도입의 여섯 그룹으로 분류된다. M4(도메인 엔티티 컴포넌트) 작업의 일환으로 진행되었다.

### 개발 (Feature)

**HUD 체력바 모듈 신설**
`apps/_MyApp_/src/HUD/` 하위에 `HealthBarDriver` + `HealthBarFactory`(`AttachHealthBar`)를 신설하였다. `HealthBarDriver`는 `Entity::ILivable` 포인터와 `SJH::Material*`를 생성자 주입받아, 매 프레임 HP 비율(`CurHp/MaxHp`)을 Material의 `uFill` uniform에 기록한다. 팩토리 `AttachHealthBar`는 플레이어(`PlayerBuilder`)와 적(`EnemyBuilder`) 양쪽에 모두 연결되었다(`11b676e`, `cca79b1`). 적 체력바는 `EnemyDeps.healthBarColor`(기본 빨강)로 색상 차별화를 지원한다.

**HealthBar 셰이더**
구면 빌보드 vertex shader + 분절형 1D 슬라이더 fragment shader를 신규 작성하였다(`011c85d`).

**Box2D Raycast 히트스캔**
`PhysicsRaycast.h/.cpp`에 `RaycastHit` 구조체 + 단일 바디 및 월드 최근접 자유함수를 구현·CMake에 등록하였다(`0de1099`, `4275133`).

**EnemyEntity facade + 적 넉백**
`EnemyEntity` Accessor-facade를 신설하고, `Impulse` 컴포넌트를 `SJH::Timer`화하여 넉백을 구현하였다. 넉백 방향은 위치 차분(접촉 관통 깊이 부호 역전 버그 발생) 대신 총알 비행 방향(`mLaunchDir`, `SetLaunchDir()` 주입)을 사용하도록 수정하였다(`d27eddb`).

**World Text 개선**
`WorldTextStyle.scale` 배율 필드 추가(`c7f020c`)와 1~3자리 데모 숫자 순환 테이블(`05d9f77`)을 구현하였다.

### 리팩토링

**PlayerBehavior god-component 분해 완료 — `Carrier` 패턴 도입**
기존 `PlayerBehavior` + `PlayerMovementComponents` + `BulletContactHandler` + `EnemyContactHandler` 4개 파일을 완전 제거하고, 접촉 데미지 배달 책임을 `Spawns/Carrier.h` 의 `Carrier::Projectile`(총알 — `IDamageable` + `IImpulsable` + self-despawn)과 `Carrier::ContactCarrier`(적 접촉 — IDamageable 배달)로 교체하였다(`1ffc2ab`). `BulletFactory`와 `EnemyFactory`의 `AddComponent` 호출도 각각 `Carrier::Projectile`, `Carrier::ContactCarrier`로 전환하였다.

**`CreatePlayerActor` 자유함수 파이프라인 분해**
기존 `PlayerActor.cpp`의 거대 if/else 분기를 `InitLife` / `InitPhysicsBranch` / `InitMovementBranch` / `WireWeapon` 등 단계별 자유함수로 분해하여 가독성과 의존성 흐름을 개선하였다(`696265c`).

**`EntityPresentation` + `SpriteLayerFactory` 추출**
`PlayerBuilder.cpp`에 인라인되어 있던 스프라이트 레이어 생성 로직과 연출 프레젠테이션 배선을 `Bootstrap::EntityPresentation`과 `Playable::SpriteLayerFactory`로 분리하였다(`1083f2c`).

**Timer 중앙화 — `BaseEntity::MultipleTimer`**
`BaseEntity`에 `SJH::Timer::MultipleTimer mTimers`를 단일 멤버로 두고, `BaseEntity::Update`가 일괄 tick하는 구조를 확립하였다(`0ae7ac9`). 이후 `Impulse`의 `mActiveTimer`/`mCooldownTimer`와 `Life`의 i-frame/사망 지연 타이머를 모두 `BaseEntity::Timers().Register("impulse.active", …)` 패턴의 핸들로 전환하였다(`bd8fa64`, `06592df`, `708840c`). `WaveController`의 스폰 간격 raw-float도 `SJH::Timer::Timer` 객체화하였다(`9e3d3fd`).

**매직 넘버 단일화 — 도메인별 `Constants.h`**
Physics(`IMPULSE_FORCE`/`IMPULSE_COOLDOWN`/`IMPULSE_DURATION`), HUD(`HEALTHBAR_*`), Stage(`WAVE_*`/`ARENA_HALF_EXTENT`/`WALL_THICKNESS`), Playable(`SPRITE_*_DURATION`/`VIGNETTE_*`), Bootstrap(`DISSOLVE`/`DELAY`/트윈MS), Entity(`PLAYER_*`/`HAND_*`/`ENEMY_*`/`BULLET_*`), VFX(`MUZZLE_EFFECT`/`TEST_EFFECTS`) 등 7개 도메인의 상수를 도메인별 헤더로 격리하였다(`18c57ad`, `75856bf`, `397d28f`, `1c5551d`, `8d861a5`, `d0a0ade`, `439695d`, `962f496`, `a20d826`, `a3dc010`).

**명명 규칙 통일 — trailing underscore 폐기**
`SJH::Playable` 엔진 코어 전체(playable_base, composite_playable, interval_playable 등)에서 `paused_`/`finished_`/`isLoop_`/`elapsed_`/`children_` 등의 trailing underscore 멤버명을 `mPaused`/`mIsFinished`/`mIsLoop`/`mElapsed`/`mPlayableChildrens` 형식(mPascalCase 컨벤션)으로 일괄 전환하였다(`b72a246`). 클라이언트 측 Playable 구현체(FmodPlayable, EffekseerPlayable, SpriteFxPlayable, TweenPlayable 등)도 동일하게 갱신하였다.

**`UniformAtlas` 비정사각 타일 지원**
기존 `tileSize`(정사각 가정)를 `tileW`/`tileH` 분리 오버로드로 확장하고, `mTileHeight` 멤버를 추가하였다. 기존 시그니처는 정사각 편의 오버로드로 위임하여 하위 호환을 유지하였다(`05d9f77`).

**BitmapFont V-flip 보정**
`bitmap_font.cpp`에서 multi-row atlas의 프레임 행 인덱스 계산 시 `frameRow = (rows-1) - (r.y / font.mCellH)` V-flip 보정을 적용하였다. `SJH::Image::Load`가 PNG를 상하 반전 업로드하기 때문에 발생하는 문제로, 단일 행(`Nx1`) atlas에서는 무관했으나 다중 행 폰트에서 처음 발견되었다(`05d9f77`).

**명명 규칙 위반 전수조사 스크립트**
`scripts/find_naming_violations.py`를 추가하여 A(trailing underscore)/B(m 없는 private 멤버)/C(비-camelCase 지역변수) 3등급 위반을 검출하고, `--vocab` 모드로 mPascalCase 어휘 dedupe 정렬을 제공하였다(`889129a`).

### 수정 (Fix / Chore)

**총알 kinematic→dynamic 전환 (벽 명중 버그)**
총알 `b2Body` 타입이 `b2_kinematicBody`이면 `b2_staticBody` 벽과 접촉 이벤트가 생성되지 않아 벽 통과·despawn 실패가 발생하였다. `b2_dynamicBody`(+ 월드 중력 0)로 전환하여 수정하였다(`ba99195`).

**넉백 방향 역전 버그 수정**
위치 차분 기반 넉백 방향이 접촉 관통 깊이에 따라 부호가 뒤집혀 "플레이어 쪽으로 돌진" 현상이 나타났다. 총알 비행 방향(`mLaunchDir`, box2d XY)을 직접 주입받는 방식으로 수정하였다(`d27eddb`).

**역행렬 개선 — `Camera::GetInverseProjectionMatrix()` 닫힌 해 도입**
fog PostFX에서 사용하는 투영 역행렬을 기존 MESA cofactor 일반 역행렬(`Mat4Inverse`) 대신 Camera 클래스에 `GetInverseProjectionMatrix()` 닫힌 해 메서드로 구현하였다. perspective/ortho 양분기를 내장하여 sparse 구조를 활용한다(`ca2b44a`). 기존 `Mat4Inverse` 자유함수는 `main.cpp`에서 제거되었다.

**Doxygen/코드 주석 특수문자 정리**
`→`, `↔` 등 특수 화살표 문자를 `->` 로 치환하여 Doxygen ASCII 컨벤션을 준수하도록 전수 정리하였다(`1370b0d`, `3340460`).

### 기술적 결정 및 이슈

**1. Carrier 패턴 — 책임 분리와 의존 방향 역전**
`BulletContactHandler` / `EnemyContactHandler`는 충돌 이벤트 처리, 데미지 배달, self-despawn을 한 컴포넌트에서 담당하는 구조였다. `Carrier::Projectile`과 `Carrier::ContactCarrier`로 교체하면서 "데미지 배달"은 `IDamageable` / `IImpulsable` 인터페이스 경유로 추상화되고, Entity 모듈이 Spawns 구현에 직접 의존하지 않는 방향을 확립하였다.

**2. Timer 중앙화의 null-guard 비용**
`BaseEntity::Timers().Register(key, duration)`에서 반환한 `SJH::Timer::Timer*` 핸들을 각 컴포넌트가 보유하는 구조상, `OnEnter` 이전 `DoImpulse` 호출 등 미등록 상태에서의 nullptr 접근을 막기 위해 `if (!mActiveTimer || …)` null-guard를 모든 접근 지점에 추가하였다. 이는 timer 소유를 분산하던 기존 방식 대비 코드 단순성을 일부 희생하는 트레이드오프이다.

**3. `UniformAtlas` 비정사각 확장 — 기존 단위 테스트 호환 유지**
`ComputeUVRect(frameIdx, cols, tileSize, atlasW, atlasH)` 시그니처를 변경하는 대신 `tileW`/`tileH` 분리 오버로드를 추가하고 기존 시그니처는 위임 오버로드로 유지하여, 21개 활성 단위 테스트(특히 `test_buffer` 등 `ComputeUVRect` 호출 포함 테스트)의 수정 없이 다중 행 폰트 atlas를 지원하도록 하였다.

### 산출물

- 변경 모듈/영역: `apps/_MyApp_/src/` (Entity, Spawns, HUD, Physics, Playable, Stage, VFX, Bootstrap, InputHandler, UI, Audio, Tween), `src/sprite/`, `src/text/`, `src/timer/`, `src/playable/`, `src/scene/` (Camera 닫힌 역행렬)
- 커밋 43건 (주요: `1ffc2ab` PlayerBehavior 분해 완료/Carrier 패턴 도입, `0ae7ac9`+`bd8fa64`+`06592df`+`708840c` Timer 중앙화 4연작, `1083f2c` Entity 컴포넌트 빌딩 전면 리팩토링, `b72a246` naming 컨벤션 통일, `5d1d052`+`11b676e`+`cca79b1` HUD 체력바 신설·배선, `ca2b44a` 역행렬 개선)

---

## 2026-06-04 (목)

**Stage FSM 완성 + 적 생사 관리 재설계 + 궁극기/데미지 텍스트 구현**

### 개요

이 날은 탑다운 슈터 데모 `_MyApp_`의 게임 루프를 완결하는 핵심 작업이 집중적으로 이루어졌다. M7 마일스톤인 Stage FSM(Title/CombatPlay/Pause/GameOver 4-상태)을 `SJH::fsm` 모듈 기반으로 구현하고, 적(Enemy) 생사(生死) 관리 구조를 폴링 방식에서 observer 패턴으로 전면 재설계하였다. 동시에 궁극기(Ult) 히트스캔 레이저, 발밑 데칼, 데미지 수치 텍스트, FMOD Tweeny 페이드 등 게임플레이 요소들이 추가·정착되었다. 25건의 커밋으로 코드 순증가 2,045줄을 기록하였으며, 이전 세션의 `PlayerBehavior` 분해 작업 이후 게임 전체 루프를 플레이어블(Playable)한 상태로 끌어올리는 날이었다.

### 개발 (Feature)

**Stage FSM 구현** (`3b499bb`): `StageState.Impl.h`에 `TitleState` / `CombatPlayState` / `PauseState` / `GameOverState` 4개 클래스를 구현하였다. 각 상태는 `BaseStageFsmState`(`IFsmState<Actor>` 계층)를 상속하고, `OnEnter` / `OnUpdate` / `OnExit` 훅에서 오버레이 표시·숨김, 블러 패스 토글, 게임 로직 tick 게이트를 담당한다. `CombatPlayState::OnUpdate`가 `Manager::Update` + `Director::Update` + 물리 동기화를 호출하여 Title/Pause/GameOver 상태에서 게임 시뮬레이션이 동결(freeze)되도록 설계하였다. Title에서 빈 화면 클릭 시 CombatPlay로 전이되고, Pause 버튼(좌상단 `PauseButtonLayer`)이 CombatPlay↔Pause를 토글한다. GameOver는 terminal 상태(전이 대상 없음)로 선언하였다.

**WaveController death observer 재설계** (`701d936`, `cfdac8c`): `Life::SetOnDeath(fn)` seam을 추가하여 적 사망(HP0) 시점에 `WaveController::OnEnemyDeath`를 콜백으로 발화하도록 하였다. `OnEnemyDeath`는 `mEnemies`에서 해당 적을 제거하고 `mDying` 큐에 enqueue하며, `SweepDespawned`(렌더 루프 밖 호출)가 `Life::IsDespawnReady()`를 확인한 뒤 `RemoveChild` → `Physics::OnExit::DestroyBody` 순으로 완전 제거한다. Player 사망 역시 `Life::SetOnDeath`로 `mPlayerDead = true` 플래그를 세우고, Update가 `StageStateMachine::TryTransit(GameOver)`를 구동한다.

**발밑 데칼(Shadow/HitRange 원)** (`42a7b6b`, `4a765e3`, `5ed0869`): 플레이어·적 모두 `groundActor` 자식에 `simple_texture Transparent` 머티리얼 Plane을 부착하여 그림자와 피격 범위 원을 렌더링한다. fixture 반경을 `b2Body::GetFixtureList`에서 자동 추출하여 크기를 동기화하였다. 공유 자원(`entity_shadow`, `hit_range_circle` 텍스처 + 머티리얼)은 find-or-create 패턴으로 단 1회 생성하도록 구현하였다.

**궁극기(Ult) 히트스캔 레이저** (`6c3df8f`): R키 입력 시 `Spawns::SpawnUltimateLaser`를 호출하여 플레이어 중심에서 마우스 조준각을 시작각으로 하는 회전 히트스캔 레이저를 생성한다(`ULTIMATE_DURATION=3.0f`초, `ULTIMATE_ROT_PER_SEC=2π rad/s`, `ULTIMATE_TICK=0.2f`초 틱 데미지). `PhysicsRaycast`로 경로상 적을 감지하며 `b2World` 포인터를 `PlayerController::SetWorld`로 주입받는다.

**데미지 텍스트** (`84c015e`): `Life::SetOnDamageNumber(fn)` seam을 추가하고, `WorldText::SetSpawnContext` + `WorldText::SpawnDamage(int d, vec3 pos)` 파사드를 통해 피격 시 월드 공간에 데미지 수치를 표시하도록 하였다. 플레이어·적 빌더 모두 `SetOnHitFx` 체인에 `.SetOnDamageNumber` 호출을 연결하였다.

**발사 핀치 연출** (`3b499bb`, `0f38640`): `PlayerHands::TriggerFire`에서 양손을 즉시 최소각(15°)으로 좁힌 뒤 `TweenPlayable<float>`(`quadraticOut`, `HAND_FIRE_PINCH_MS=500ms`)으로 거리 기반 각도로 복귀하도록 구현하였다.

**대시 스킬** (`0f38640`): Shift 키 입력에 대시 기능을 연결하고 FMOD Studio에 대시 사운드 이벤트를 배선하였다.

### 리팩토링

**`EnemyDeathHandler` 제거** (`58a0cd3`): 기존 `EnemyDeathHandler` Component는 `mOnDeathFx`가 한 번도 바인딩되지 않아 사실상 dead code 상태였다. 이를 완전 삭제하고 사망 기구를 `Life::DoDie` + Update self-poll 흐름으로 흡수하였다.

**Physics body 생명주기 통합** (`243a18b`): `BoxBody` / `CircleBody`의 빈 `OnExit` override를 제거하고, 기반 클래스 `Physics::OnExit`에서 `b2World::DestroyBody`를 직접 호출하도록 하였다. 아울러 `SetBodyEnabled(bool)` API를 추가하여 사망 시 시각 디졸브는 유지하면서 충돌만 정지할 수 있도록 하였다.

**`ForEachSpriteRenderer` 재귀화** (`fc7a386`): 기존에는 직속 자식 레벨까지만 탐색하던 로직을 서브트리 전체 재귀 탐색으로 변경하였다. `renderActor` 하위 손 스프라이트까지 hit-flash/dissolve 효과가 도달하도록 하기 위함이다.

**데칼 Y offset 상수 추출** (`ff72274`): `PLAYER_DECAL_Y`, `ENEMY_DECAL_Y`, `CIRCLE_DELTA` 상수를 `Entity::Constants.h`로 이전하고, `AttachGroundDecals`에 `baseY` 파라미터를 추가하여 중복 매직 넘버를 제거하였다.

**FMOD `mAudio.Update` 위치 이동** (`de58b15`): `Manager::Update`(CombatPlay에서만 호출)에서 `mAudio.Update` 호출을 제거하고 `render()` 내 ungated 위치로 이전하였다. FMOD Studio update는 비동기 명령 큐를 처리하는 펌프로, Title/Pause 상태에서도 매 프레임 호출해야 BGM이 정상 재생되기 때문이다.

### 수정 (Fix / Chore)

**Box2D Step 잠금 크래시** (`95a1645`): `OnEnemyDeath`(contact 콜백 경유 = `b2World::Step` 잠금 중)에서 `b2Body::SetEnabled`를 직접 호출하여 `IsLocked() assert` 크래시가 발생하였다. body 비활성화를 `SweepDespawned`(Step 종료 후 호출) 안으로 이동함으로써 수정하였다. 해당 교훈은 `doc/Box2DAPI.md §8`로 문서화하였다(`1ce1fcd`).

**`simple_texture.vs` 셰이더 컴파일 버그** (`ce6ef73`): `vsPosition` / `vsNormal` / `vsTexCoord` 세 `out` 선언이 누락되어 셰이더 컴파일이 실패하던 문제를 수정하였다.

**데칼 자원 로드 실패 가드** (`b2d544b`, `0dd7e9f`): texture null-deref 방지를 위해 program/texture 취득 실패 시 조기 반환하도록 플레이어·적 데칼 생성 경로에 공통 가드를 추가하였다.

### 기술적 결정 및 이슈

1. **Observer vs 폴링 — WaveController 구조 결정**: 기존 `IsActive()` 폴링 방식은 동적 추가·제거 시 정확성을 보장하기 어려웠다. `Life::SetOnDeath` seam을 통해 사망 통지를 단방향 observer로 전환함으로써 `LiveCount`가 `mEnemies.size()`의 순수 계산이 되고, 폴링 루프가 완전히 제거되었다. 단, Box2D Step 잠금 중 콜백이 발화될 수 있으므로 body 구조 변경은 반드시 Step 밖으로 deferred 처리해야 한다는 규칙이 확정되었다.

2. **FMOD `mAudio.Update` 위치 — Title/Pause BGM 무음 버그의 근본 원인**: Stage FSM 도입 후 Title 화면에서 BGM이 재생되지 않는 무음 버그가 발생하였다. 원인은 `Manager::Update`가 `CombatPlayState`에서만 호출되어 FMOD Studio의 비동기 명령 큐가 다른 상태에서 펌프되지 않는 것이었다. `mAudio.Update`를 `render()` 내부 ungated 위치로 이전하여 해결하였으며, 오디오 펌프는 게임 상태와 무관하게 매 프레임 처리되어야 한다는 설계 결정이 주석으로 명시되었다.

3. **궁극기 구현 — 히트스캔 vs 물리 콜리전**: Ult 레이저를 Box2D 물리 body로 구현하면 생성·파괴 비용과 프레임 당 Step 잠금 문제가 복합된다. 대신 `PhysicsRaycast` + `b2World::RayCast`로 히트스캔을 수행하여 틱 데미지(`ULTIMATE_TICK=0.2s`)만 적에 전달하는 방식을 채택하였다. 이는 짧은 생존 시간 내 다수 적에 도달해야 하는 빔 계열 스킬에 적합한 정통 히트스캔 패턴이다.

### 산출물

- 변경 모듈/영역: `apps/_MyApp_/src/Stage/State/` (FSM 4-상태), `Stage/WaveController`, `Entity/Components/LifeComponents`, `Physics/PhysicsComponent`, `Bootstrap/EnemyBuilder` + `PlayerBuilder`, `Spawns/UltimateLaser` + `WorldTextInstance`, `InputHandler/PlayerController` + `ActorFolower`, `Audio/` (FMOD Tweeny Fade), `UI/StateOverlayLayer`, `doc/Box2DAPI.md`
- 커밋 25건 (주요: `3b499bb` Stage FSM 4-상태 구현, `cfdac8c` WaveController death observer, `95a1645` Box2D Step 잠금 크래시 수정, `6c3df8f` 궁극기 히트스캔 레이저, `84c015e` 데미지 텍스트 작동 완료)

---

## 2026-06-10 (수)

**전체 코드베이스 Doxygen 주석 전면 정비 및 제출 준비**

### 개요

본 일자는 탑다운 슈터 게임 `_MyApp_` 와 그 기반 엔진 레이어(`SJH::<module>`) 전 영역에 걸쳐 Doxygen 호환 주석을 일괄 정비한 날로, 기능 구현이 아닌 문서화·코드 가독성 향상에 집중하였다. M1~M7 마일스톤에 걸쳐 축적된 엔진 17개 모듈 및 클라이언트 모듈 전체 공개 API에 `@file`, `@brief`, `@details`, `@param`, `@return`, `@note` 어노테이션을 체계적으로 부여하고, 이전에 작성된 주석의 문체·기호 표기를 통일하는 재검토까지 포함하였다. 커밋 4건으로 구성되며, CI Doxygen 워크플로 수정부터 시작하여 엔진 모듈 주석 작성·클라이언트 모듈 주석 작성·최종 재검토 순으로 진행된 교수 제출용 정리 작업이다.

### 개발 (Feature)

신규 기능 구현은 없으며, 이전 기간에 구현된 엔진·클라이언트 코드의 공개 API를 Doxygen이 추출 가능한 형태로 보강하였다.

`45d137b` 커밋은 엔진 레이어 전체를 대상으로 하였다. `src/buffer/buffer.h`, `framebuffer.h`, `src/common/common.h`, `src/common/constants.h` 등 공통 유틸부터 `src/render/render_pipeline.h`, `src/playable/composite_playable.h`, `src/sprite/uniform_atlas.h`, `src/timer/timer.h`, `src/text/bitmap_font.h` 에 이르기까지 100개 이상의 파일에 파일 수준 `@file` 블록을 신규 추가하였다. 각 블록에는 **책임(담당하는 것)** 과 **비-책임(담당하지 않는 것)** 을 명시적으로 구분하는 `[X]` 항목 리스트 형식을 도입하여, 모듈 간 경계가 문서 수준에서도 명확히 드러나도록 하였다.

`1e5a6ca` 커밋은 `apps/_MyApp_/src/` 하위 클라이언트 전 영역을 대상으로 동일한 작업을 수행하였다. `Algebraic/Stat.h`, `Audio/AudioSystem.h`, `Bootstrap/PlayerBuilder.h`, `Entity/Player/PlayerEntity.h`, `Stage/WaveController.h`, `Playable/PlayableDirector.h`, `VFX/VFXSystem.h` 등 120개 이상의 파일에 `@file`/`@brief`/`@details` 블록과 멤버 변수·열거자 단위 `///` 어노테이션이 추가되었다. 특히 Unity GAS(Gameplay Ability System)와의 정통 매핑을 `@details` 안에 명시하여(`Algebraic.Common.h` 의 `ENumericStatType`, `StatModifier`), 외부 독자가 개념 구조를 빠르게 파악할 수 있도록 하였다.

### 리팩토링

주석 내용의 대규모 문체 통일 작업이 `bf94351` 커밋에서 이루어졌다. 구체적으로는 em 대시(`—`)를 하이픈(`-`)으로 일괄 교체하고, 문서 내 섹션 참조 표기를 `§12` 형식에서 `sec.12` 형식으로 변환하였다. 이는 Doxygen의 HTML/PDF 출력에서 비ASCII 기호가 깨질 수 있다는 점과, "ASCII/한글만 사용" 컨벤션(`doxygen-ascii-comment-convention`)을 일관 적용하기 위한 조치다. 해당 변경은 `AudioSystem.cpp/.h` 등 클라이언트 파일들에서 순수 기호 치환으로만 이루어졌으며, 기능 로직에는 영향을 주지 않았다.

`src/common/constants.h`의 `@file` 블록에는 `@note`로 "ODR 위반 없음" 근거(const 배열의 암묵적 내부 링크)를 명시하였다. 이는 향후 다른 번역 단위에서 동일 헤더를 include 할 때 불필요한 혼란을 사전에 차단하기 위한 것이다.

### 수정 (Fix / Chore)

`8054706` 커밋에서 GitHub Actions의 Doxygen 빌드 워크플로를 수정하였다. 코드 파일 변경은 없으며, `.github/workflows/` 하위의 비코드 파일 1건 수정으로 구성된다. CI에서 Doxygen 문서 빌드가 올바르게 동작하도록 워크플로를 보정한 것으로, 이후 주석 추가 커밋들이 CI를 통해 검증될 수 있는 전제 조건을 마련한 정리 작업이다.

### 기술적 결정 및 이슈

**1. 책임/비-책임 이중 선언 패턴 채택.** `@details` 블록에 "책임"과 "비-책임(`[X]`)" 항목을 병기하는 방식은 단순한 기능 설명을 넘어 모듈 경계 계약을 명시하는 효과를 가진다. 예컨대 `Framebuffer`가 텍스처를 단독 소유하지 않는다(`TexturePtr`는 `shared_ptr`)는 사실, `AudioSystem`이 실제 재생을 담당하지 않고 leaf Playable(`FmodPlayable`)에 위임한다는 사실이 헤더만으로도 전달된다. 이는 향후 다른 개발자(또는 AI 에이전트)가 코드베이스를 파악할 때 진입 비용을 낮추는 설계 문서화 전략이다.

**2. em 대시 전면 제거.** ASCII 전용 주석 정책을 기술적 결정으로 확정한 배경에는 두 가지 이유가 있다. 첫째, Doxygen이 HTML 출력 시 비ASCII 기호를 잘못 인코딩하는 환경이 존재한다. 둘째, MSVC에서 `/utf-8` 플래그 없이 빌드할 때 주석 내 비ASCII 문자가 경고 원인이 되는 사례가 있다. 이번 `bf94351` 재검토 커밋은 `1e5a6ca`에서 추가된 주석에 잔류한 em 대시를 소멸시키는 후처리 단계였다.

**3. 정통 매핑 명시의 의의.** `Algebraic` 도메인의 `Stat`/`StatModifier`를 Unity GAS의 `GameplayAttribute`/`GameplayModifierInfo`와 대응시켜 주석으로 기술함으로써, 이 클래스들이 즉흥적 설계가 아니라 산업 표준 패턴을 의도적으로 적용한 결과임을 보고서 수준에서 입증할 수 있게 되었다.

### 산출물

- 변경 모듈/영역: `src/buffer`, `src/common`, `src/diagnostics`, `src/fsm`, `src/input`, `src/layout`, `src/material`, `src/object`, `src/playable`, `src/program`, `src/render`, `src/resource_registry`, `src/scene`, `src/shader`, `src/sprite`, `src/text`, `src/timer` (엔진 17개 모듈 전체), `apps/_MyApp_/src/` 전 하위 디렉터리 (Algebraic, Audio, Bootstrap, Entity, HUD, InputHandler, Physics, Playable, Spawns, Stage, Text, Tween, UI, VFX), `.github/workflows/`
- 커밋 4건:
  - `8054706` [fix] : doxygen workflow update
  - `45d137b` [doc] : 주석 작성 (엔진 레이어 100+ 파일)
  - `1e5a6ca` [doc] : 주석 재검토 (클라이언트 모듈 120+ 파일)
  - `bf94351` [doc] : client 모듈 (em 대시 -> 하이픈 + 섹션 참조 표기 정규화)

---

*본 보고서는 `scripts/traversal_git.py` 가 추출한 `doc/work_history.md` 를 1차 사료로, 16개 작업일을 개발·리팩토링·수정 3축으로 정리한 것이다. 각 일자의 원시 커밋·코드 diff는 `doc/work_history.md` 의 동일 일자 절을 참조한다.*
