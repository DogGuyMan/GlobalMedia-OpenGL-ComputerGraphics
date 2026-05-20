# Doxygen 문서화 계획

> 본 문서는 `newEnv` 브랜치 src/ 모듈에 Doxygen 주석을 단계적으로 추가하고, 빌드 시스템·다이어그램을 포함한 통합 문서를 산출하는 계획서다.
>
> **목표**: `cmake --build build_Darwin --target doxygen` 실행 시 모듈 API + 클래스 다이어그램 + 의존성 그래프 + 빌드 시스템 가이드를 포함한 HTML 문서가 `doc/html/` 에 생성되어야 한다.
>
> **진행 상태 (2026-05-08 기준)**: Phase 1~6 완료. 이후 `buffer` / `layout` / `resource_management` 가 placeholder 에서 실제 활성 모듈로 승격되며 자체적으로 Doxygen 주석을 보유. 신규 `object` 모듈(`camera.h`) 및 `context` 의 입력 위임 메서드(@c ProcessInput / @c MouseMove / @c MouseButton / @c Reshape) 는 **Phase 7** 에서 추가.

## 1. 산출물

| 산출물 | 위치 | 생성 방식 |
|--------|------|----------|
| 모듈 API 레퍼런스 | `doc/html/` | Doxygen 자동 (헤더 주석 → HTML) |
| 클래스 상속/포함 그래프 | `doc/html/` 내 임베드 | Doxygen + Graphviz 자동 |
| 헤더 include 그래프 | `doc/html/` 내 임베드 | Doxygen + Graphviz 자동 |
| 모듈 책임/레이어 다이어그램 | `\page` 메인 페이지 | PlantUML 수동 (의도 표현) |
| 빌드 시스템 가이드 | `\page` | `.claude/build-system.md` 변환 또는 링크 |
| 의존 라이브러리 표 | `\page` | vcpkg.json 패키지 + 사용처 매핑 |

## 2. 주석 스타일 합의

- **언어**: 한국어 (기존 `diagnostics/gl_log.h` 톤 유지)
- **범위**: 헤더(.h)에만 표준 Doxygen 주석. .cpp는 비-자명한 의도만 한 줄 인라인.
- **태그 사용**: `@brief`, `@param`, `@return`, `@details`, `@note`, `@warning`, `@code` (필요 시)
- **레퍼런스 모범 사례**: [src/diagnostics/gl_log.h](../src/diagnostics/gl_log.h)

## 3. Phase 구성 (의존성 leaf → root)

각 Phase 진행 사이클:
1. 해당 모듈 헤더 현재 상태 확인
2. 추가/변환할 Doxygen draft 제시
3. 사용자 검토 → 톤·내용 OK 시 적용
4. 다음 Phase

| Phase | 모듈 | 의존 | 상태 | 비고 |
|-------|------|------|------|------|
| **1** | `common` + `diagnostics` 검토 | (없음) | ✅ 완료 | `CLASS_PTR` 매크로 톤 합의가 핵심. diagnostics는 이미 완성, 검토만. |
| **2** | `shader` | common, diagnostics | ✅ 완료 | 팩토리 패턴(`CreateFromFile`) 한글 블록 코멘트 → Doxygen 변환 |
| **3** | `program` | shader | ✅ 완료 | shader와 유사 (벡터 입력 차이). `ProgramUniforms` 도 같은 모듈에 포함. |
| **4** | `context` | program | ✅ 완료 | "Context 책임 분담" 설계 철학 보존 + 변환. 입력/카메라 추가는 Phase 7 에서. |
| **5** | placeholder 모듈 | — | ⚠️ 의미 변화 | 본래 `buffer` / `layout` / `resource_management` placeholder 주석. **이후 모두 실제 활성 모듈로 승격** — Doxygen 주석은 각 헤더 자체에 작성됨 (Phase 7 의 §부록 참조). |
| **6** | 다이어그램 + 페이지 | — | ✅ 완료 | Doxyfile 갱신 (HAVE_DOT 등) + `\page` 메인 + PlantUML |
| **7** | `object/camera` + `context` 입력 위임 | program, glm | ✅ 완료 | Camera POD-like 상태 + Context 의 GLFW 콜백 위임 메서드들 Doxygen 작성 (커밋 `3696136` 카메라 리팩토링까지 반영). |
| **8** | VAO/VBO/EBO 정리 + ImGui/imguizmo 패키지 도입 | — | ✅ 완료 | 코드만 변경(헤더 영향 X) + `vcpkg.json` 의 imgui/imguizmo 를 `20-dependencies.md` 패키지 표 + 의존성 그래프에 반영. |
| **9** | GL State 진단 인프라 (`GLStateFields`, `GLStateLog`) | diagnostics | ✅ 완료 | `gl_state_fields.h` 에 `@file` 헤더 보강(다른 진단 헤더와 톤 통일). `gl_state_log.h` 는 작성 시점부터 완전. 클래스 의존 그래프에 신규 클래스 1개 + struct 2개 추가. |
| **10** | ImGui 컴포넌트 + Light + Specular | object, glm, imgui | ✅ 완료 | `context.h` 의 라이팅 멤버 9개 (`mClearColor` / `mAnimation` / `mLight*` / `mObjectColor` / `mAmbient*` / `mSpecular*`) Doxygen `///` 추가 + `light.h` placeholder 주석. |
| **11** | 테스트 인프라 (`gl_state_snapshot`, `spdlog_capture`) + 사보타지 드릴 | diagnostics, Catch2 | ✅ 완료 | `test/support/*` 헤더는 작성 시점부터 완전 (`@file` + `@code` + `@see` 모두 보유). 추가 작업 없음. |
| **12** | Material & Lighting (`Light` / `Material` 클래스 신설) | object, shader, glm | ✅ 완료 | `Light` (point light + Phong 3항 색상) + `Material` (color-기반) 신설. `light.h` 의 placeholder 주석 → 실 클래스 docstring + 멤버 4개 Doxygen. `material.h` 빈 `@file` → Phong 책임/비-책임/네임스페이스 일관성 이슈 명시. Context 의 라이팅 멤버 9개가 두 클래스로 흡수됨. |
| **13** | Uniform setter 시그니처 변경 (`const T*` → `const T&`) | program | ✅ 완료 | `program_uniforms.h` 의 6개 setter 시그니처 변경 — 헤더 docstring 은 이미 일반 표현으로 작성되어 있어 본문 갱신 불필요. |
| **14** | 스페큘러/디퓨즈 텍스처링 + `Program::CreateWithVSFS` | program, shader | ✅ 완료 | `Material` 멤버를 색상 vec3 → 텍스처 *이름 키* 로 전면 교체 (변경 이력 docstring 에 기록). `Program::CreateWithVSFS` 편의 팩토리 Doxygen 추가. `Context::mSimpleProgram` (광원 큐브용) docstring 추가. 클래스 의존 그래프에 Light/Material 노드 + Material→ResourceManagement (이름 키 해석) 엣지 추가. |
| **15** | Blinn-Phong + Multi-light caster (`DirLight`/`PointLight`/`SpotLight`) | object, glm | ✅ 완료 | `light.h` 에 `DirLight`/`PointLight`/`SpotLight` 3종 광원 클래스 추가. `DirLight::mDirection` `@brief` 보강. `PointLight` 클래스 레벨 docstring 추가. `GetAttenuationCoeff` 자유 함수 Doxygen (`@brief`/`@param`/`@return`/`@note`) 추가. `context.h` 의 `mDirLight`/`mPointLights`/`mSpotLight` + Enable 토글 멤버 docstring 이미 작성 완료. |
| **16** | Mesh 리팩토링 + `gl_validate` 진단 + `Transform` | object, diagnostics | ✅ 완료 | `mesh.h` 신규 — `@file` 헤더 + `Vertex` struct + `Mesh` 클래스 전체 Doxygen 작성. `transform.h` 신규 — `INameTagInterface`/`Transform`/`UVTransform` Doxygen 작성. `gl_validate.h` 신규 — 작성 시점부터 `@file` 헤더 + 6종 Cat 함수 Doxygen 완전 보유. 클래스 의존 그래프에 `Mesh` 노드 추가 (cluster_gl). Context 의 VAO/VBO 직접 소유 → Mesh 로 이전 반영. |
| **17** | `Uniforms` 네임스페이스 분리 + 광원 helper | program | ✅ 완료 | `program_uniforms.h` 신규 파일로 분리 — 작성 시점부터 `@file` 헤더 + 전 함수 Doxygen 완전. `SetDirLight`/`SetPointLight`/`SetSpotLight` 광원 helper 포함. |
| **18** | `Model` 클래스 신설 (Assimp 로드) | object, resource | ✅ 완료 | `model.h` 신규 — `RenderUnit` struct + `Model` 클래스 레벨 docstring 추가. `Load`/`GetMeshCount`/`GetMesh`/`Draw` 메서드 `@brief` 추가. `context.h` 의 `mBox`/`mModel` docstring 추가. 클래스 의존 그래프에 `Model` 노드 + `Model→Mesh`/`Model→Material`/`Model→Texture` 엣지 추가. |
| **19** | `ResourceRegistry` 리팩토링 (구 `ResourceManagement` 교체) | resource | ✅ 완료 | `src/resource_registry/` 신규 모듈 — `resource_registry.h`/`image.h`/`texture.h` 작성 시점부터 전체 Doxygen 완전. 클래스 의존 그래프에서 `ResourceManagement` → `ResourceRegistry` 대체. `DirLight`/`PointLight`/`SpotLight` 노드를 cluster_scene 에 추가. Context→VertexLayout/Buffer 직접 소유 엣지 제거 (Mesh 로 이전). |
| **20** | 사용자 Doxygen 정비 커밋 (205bbc3) | 전반 | ✅ 완료 | `light.h` PascalCase 컨벤션 대응 + 멤버명 갱신. `mesh.h`/`model.h`/`transform.h` Doxygen 추가. `camera.h` 컨벤션 + docstring 추가. `context.h` mProgram label 보강. `00-mainpage.md` Graphviz 갱신. |
| **21** | Material.Apply + Model Material 로드 + SpotLight 손전등 | material, object | ✅ 완료 | `material.h`: `SetProgram()`/`Apply()` + `mProgram` 멤버 Doxygen 추가. `model.h`: `GetMaterialCount()`/`GetMaterial()` docstring 추가. `mesh.h`: `CreatePlane()` / `GetPrimitiveType()` @brief 추가. |
| **22** | Material 모듈 이동 (`shader/` → `material/`) | material | ✅ 완료 | `src/material/material.h` 신설 — 모듈 위치 변경 이유(순환 의존 회피) docstring 에 기록. `resource_registry.h`: `CreateMaterial` 시그니처 변경 반영. |
| **23** | 텍스처 로딩 버그 + Camera/Light PascalCase 컨벤션 | object | ✅ 완료 | `camera.h` 멤버명 m-prefix 제거 (PascalCase) — docstring 의 stale `m*` 참조 전면 수정. `light.h` 멤버명 동일 컨벤션 갱신. `context.h`: `mTextureProgram`/`mPostProgram`/`mPlane`/`mFlashLightMode`/`mDepthFuncIndex` 신규 멤버 @brief 추가. |
| **24** | Material 경로 의존 제거 | material, resource | ✅ 완료 | 의존 방향 정리 — 코드 변경 위주, 헤더 Doxygen 영향 없음. |
| **25** | Depth buffer / Depth 시각화 / 원복 | context | ✅ 완료 | `context.cpp` 전용 변경, 헤더 변경 없음. |
| **26** | Depth test (GLState 정책 문서화) | context, common | ✅ 완료 | `constants.h` 신규 — `@file` 헤더 + 전역 상수 그룹 작성 시점부터 완전. `camera.h` 컨벤션 갱신 추가 반영. |
| **27** | 스텐실 테스트 | context | ✅ 완료 | `context.h` 대규모 재작성 — 신규 멤버 @brief 추가 완료. |
| **28** | 블렌딩 | context, mesh | ✅ 완료 | `mesh.h` `CreatePlane()` docstring 추가. 나머지 context.cpp 변경 위주. |
| **29** | 프레임버퍼 | buffer, resource, texture | ✅ 완료 | `framebuffer.h` 신규 — `@file` 헤더 + `Framebuffer` 클래스 전체 Doxygen 신규 작성. `texture.h`: `Create(width,height,format)` + `GetWidth/GetHeight/GetFormat` + `mWidth/mHeight/mFormat` Doxygen 추가. 클래스 의존 그래프에 `Framebuffer` 노드 추가 (cluster_gl). |
| **30** | 컨벤션 + Viewport 정합 진단 | diagnostics, framebuffer | ✅ 완료 | `gl_validate.h` Cat G (`CheckViewport`) 신규 함수 — 작성 시점부터 완전. `framebuffer.h` 컨벤션 적용. `resource_registry.h` 인터페이스 갱신 반영. |
| **31** | 포스트프로세스 셰이더 | context, constants | ✅ 완료 | `context.h`: `mPostProgram`/`mGamma` docstring 추가. `constants.h` 포스트프로세스 경로/uniform 상수 추가 — 파일 자체 이미 완전. 클래스 의존 그래프: Context→Material/Model 직접 소유 엣지 제거, Context→Framebuffer 추가, `Material→Program` 비소유 관찰자 엣지 추가. |

## 4. 비활성 모듈 처리 정책 (Phase 5 시점) — 현재는 *역사적 항목*

> **현재 (2026-05-08):** 본 항목은 Phase 5 작성 당시의 정책. 이후 세 모듈 모두 활성화되었고
> 자체 헤더에 정상 Doxygen 주석을 보유한다. Phase 7 표의 `객체/모듈` 항목 참조.

- (당시) `buffer`, `layout`, `resource_management` 는 `src/CMakeLists.txt` 에서 `add_subdirectory` 주석 처리됨
- (당시) Doxygen `EXTRACT_ALL=YES` 이므로 빈 namespace도 출력 → 짧은 placeholder 주석 추가하여 "의도된 빈 상태" 명시
- (현재) 세 모듈 모두 실제 구현 + Doxygen 주석 보유. placeholder 헤더 의미 자체 소멸.

## 5. 다이어그램 전략 (A + B 병행)

### A. Doxygen + Graphviz 자동 그래프 (사실)
- Doxyfile 변경: `HAVE_DOT=YES`, `CLASS_GRAPH=YES`, `COLLABORATION_GRAPH=YES`, `INCLUDE_GRAPH=YES`, `INCLUDED_BY_GRAPH=YES`, `CALL_GRAPH=NO`(노이즈), `DOT_IMAGE_FORMAT=svg`
- 의존성: `brew install graphviz` (macOS), Windows는 vcpkg 또는 별도 설치
- 코드와 항상 동기화 — 클래스 상속, include 관계

### B. PlantUML 수동 다이어그램 (설계 의도)
- Doxygen `\dot ... \enddot` 또는 `\plantuml` 디렉티브 사용
- 대상:
  - **레이어 다이어그램**: `app` → `context` → (`program`, `shader`) → (`common`, `diagnostics`) → 외부(`glad`, `glfw`, `spdlog`)
  - **렌더링 시퀀스**: `main` → `Context::Create` → `Context::Render` 흐름
  - **팩토리/RAII 패턴 설명도**: `Shader::CreateFromFile` 의 nullptr 반환 시 자원 정리 흐름

### 통합
- 메인 페이지(`\page index`)에 PlantUML 임베드 + 자동 그래프 링크
- README.md를 `USE_MDFILE_AS_MAINPAGE=README.md` 로 메인 페이지로 사용 (Doxyfile 기존 설정 유지)

## 6. Doxyfile 변경 항목 (Phase 6 작업)

| 키 | 현재값 | 목표값 | 이유 |
|----|--------|--------|------|
| `HAVE_DOT` | (미설정) | `YES` | Graphviz 활성화 |
| `DOT_IMAGE_FORMAT` | (기본 png) | `svg` | 확대해도 선명 |
| `CLASS_GRAPH` | (기본 YES) | `YES` | 명시 |
| `COLLABORATION_GRAPH` | (기본 NO) | `YES` | 멤버 객체 관계 시각화 |
| `INCLUDE_GRAPH` | (기본 YES) | `YES` | 명시 |
| `INCLUDED_BY_GRAPH` | (기본 YES) | `YES` | 명시 |
| `CALL_GRAPH` | (기본 NO) | `NO` | 노이즈 회피 |
| `EXTRACT_PRIVATE` | `NO` | `NO` 유지 | 캡슐화 정보 차단 |
| `WARN_IF_UNDOCUMENTED` | `YES` | `YES` 유지 | 누락 검출 |
| `INPUT` | `@DOXYGEN_INPUT_DIR@` | `@DOXYGEN_INPUT_DIR@ @DOXYGEN_PAGES_DIR@` | `\page` 마크다운 포함 |

## 7. 검증 방법

각 Phase 완료 시:
```bash
cmake --build build_Darwin --target doxygen
open doc/html/index.html  # macOS
```

확인 항목:
- 새 주석이 HTML에 반영되었는가?
- `WARN_IF_UNDOCUMENTED` 경고가 줄어들었는가?
- (Phase 6 이후) 다이어그램이 SVG로 렌더되는가?

## 8. 산출물 위치 정리

```
doc/
├── Doxyfile.in                    Phase 6 에서 갱신
├── doxygen-documentation-plan.md  본 문서
├── pages/                         Phase 6 신설 — \page 마크다운 모음
│   ├── 00-mainpage.md             메인 + 레이어 다이어그램
│   ├── 10-build-system.md         빌드 가이드
│   └── 20-dependencies.md         vcpkg 의존성
└── html/                          Doxygen 생성 출력 (gitignore)
```

## 9. 범위 외 (out of scope)

- src/ 외부 코드(`app/main.cpp`, `include/input/`, `test/`) Doxygen 주석 — 이번 작업 제외
- 영문 번역 — 한국어 단일
- README.md 갱신 — 별도 작업
- CI 통합 — 별도 작업

## 10. Phase 7 — 신규 추가 모듈 처리 (2026-05-08~)

### 10.1 트리거 — 무엇이 새로 들어왔는가

> 본 표는 **커밋 `3696136` (카메라 리팩토링)** 까지 반영. 이전 작성분의 옛 이름(`mCameraPos`, `mCameraFront`, `mCameraControl` 등)은 모두 새 m-prefix 명명규약으로 갱신.

| 신규 항목 | 위치 | 비고 |
|----------|------|------|
| `Camera` 클래스 | `src/object/camera.h` | POD-like 상태 컨테이너. `glm::vec3 mPos / mTarget / mCamUp` + `mEulerYaw / mEulerPitch` + `mFov / mAspect / mNearPlane / mFarPlane` + `mIsCamControl`. |
| `Camera` 메서드 | 동상 | `GetFront()` (Yaw→Pitch 회전식 front 산출), `GetForwardViewMatrix()` / `GetLookAtViewMatrix()` 분리, `GetProjMatrix()` (인자 없음 — `mAspect` 멤버 사용), `SetAspect(w,h)` (height==0 가드 내장). |
| `Context::ProcessInput` | `src/context/context.h` | GLFW 키 폴링 → 카메라 위치 이동 (W/A/S/D/Q/E). 매 프레임 `GetFront()` 1회 캐싱 후 재사용. |
| `Context::Reshape` | 동상 | framebuffer_size_callback 위임 — width/height 갱신 + `glViewport` + `mCamera.SetAspect`. |
| `Context::MouseMove` | 동상 | cursor_pos_callback 위임 — `mEulerYaw/Pitch` 갱신 + clamp/wrap. front 직접 갱신은 제거 (`GetFront()` 게터가 매번 재계산). |
| `Context::MouseButton` | 동상 | mouse_button_callback 위임 — `mCamera.mIsCamControl` 토글. |
| `Context` 추가 멤버 | 동상 | `mCamera` / `mWidth` / `mHeight` / `mPrevMousePos` + 기존 `mProgram` / `mVertexArrayObject` / `mVertexBufferObject` / `mElementBufferObject` / `mRM` 의 추가 노출. |
| `app/main.cpp` 콜백 위임 | `app/main.cpp` | `glfwSetWindowUserPointer(window, ctx.get())` 로 Context 주입 → `Handle*` 콜백이 `glfwGetWindowUserPointer` 로 역참조하여 `Context::*` 메서드에 위임. |

### 10.2 작성 원칙

- **Camera 는 상태 컨테이너 — 책임/비-책임 명시**: 입력 처리 자체는 Context 의 책임이며 Camera 는 *데이터*만 보관함을 헤더 클래스 docstring 에 명시.
- **Context 입력 메서드는 GLFW 콜백 위임 진입점**: 각 메서드 docstring 에 *어느 GLFW 콜백 에서 위임받는지* 를 `@note` 로 표기.
- **단위 표기**: yaw/pitch 의 *degree*, fov 의 *degree* 등 단위를 명시 (헤드리스 테스트 시 모호성 제거).

### 10.3 부록 — `buffer` / `layout` / `resource_management` 의 *역설계 검수*

세 모듈은 Phase 5 placeholder 시점 이후 실제로 구현되며 자체적으로 Doxygen 주석을 작성받았다.
주석 톤은 본 계획의 §2 와 일치하므로 추가 변환 필요 없음. 점검 항목은 다음 한 가지뿐:

- placeholder 정책 시점의 잔존 주석 (예: "Placeholder 모듈 — VBO/EBO RAII 래퍼 구현 예정")
  이 남아 있는지 확인. 발견 시 즉시 제거 (현재 세 헤더 모두 정상 docstring 보유).
