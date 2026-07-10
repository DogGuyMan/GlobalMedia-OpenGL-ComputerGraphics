# 핸드오프 — clangd 거짓 진단 (missing/unused-includes 캐스케이드) 원인·경로·타이밍

> **작성** 2026-07-08 · **대상** 다른 Claude Agent (clangd 설정 수정 담당) · **맥락0 자기완결**
> **프로젝트** /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics (C++17/CMake/Ninja/OpenGL 4.1, macOS)

## TL;DR (근본 원인 — 실측 확정)

이번 세션에 쏟아진 clangd "거짓 에러"는 **compile-DB 배선 문제가 아니다**. `build_ninja/compile_commands.json` 은 정상(신규 `frame_capture.cpp`/`pass_capture.cpp`/`golden_compare.cpp` 전부 등재 확인). 원인은 **사용자의 미커밋 `.clangd` 수정**이 IWYU 식 Strict 진단을 새로 켠 것이다:

```yaml
# .clangd 에 새로 추가된 부분 (git diff .clangd)
CompileFlags:
  Add:
    - -Werror
    - -Wconversion        # <- sign-conversion 노이즈 유발
    - -Warray-bounds
    - ... (기타 -W)
Diagnostics:
  UnusedIncludes: Strict   # <- "Included header X is not used directly"
  MissingIncludes: Strict  # <- "No header providing 'std::string'/'GLuint'/'cv::Mat' ..."
```

`MissingIncludes: Strict` + `UnusedIncludes: Strict` 는 **간접(transitive)/우산 헤더에 의존하는 이 코드베이스 스타일과 정면 충돌**한다 — 심볼이 직접 include 헤더가 아니라 전이 헤더로 들어오면 전부 "No header providing X" 로 깃발 세움. 이 프로젝트는 `SJH::engine` 우산 + `material/pass.h` 등 전이 include 를 **의도적으로** 쓰므로 거의 전 파일이 걸린다.

이전 [[clangd-imgui-cascade-false-errors]] 메모리의 "imgui.h not found 캐스케이드" 와는 **다른 신규 원인**이다(그건 compile-DB fallback -I 로 해결됨, 이건 Strict 진단 정책).

## 발생 경로 (path) — 세션 중 관측된 파일 × 진단

Strict 설정은 **레포 전역**에 적용되므로 *편집한 모든 TU* 에 뜬다. 이번 세션 실관측:

| 파일 | 거짓 진단 종류 | 예시 심볼 |
|---|---|---|
| `src/diagnostics/frame_capture.cpp` | missing-includes + **sign-conversion**(real) | std::string, GL_RGBA, GLenum, GL_FRAMEBUFFER / `size_t*int`(36,49) |
| `src/diagnostics/pass_capture.cpp` | missing-includes | std::vector, std::string, std::pair, INT_MIN/MAX |
| `src/render/renderable_processor.cpp` | unused-includes | material.h, mesh.h, program.h, render_texture.h |
| `src/render/renderable_processor.h` | unused-includes | climits (실제로 INT_MIN/MAX 사용 = **거짓**) |
| `src/render/mesh_renderer.h` | missing-includes | Pass::RenderQueue, Pass::RenderStateBlock, Pass::QueueOf |
| `src/render/render_passable/render_passable.impls.{h,cpp}` | missing-includes | glm::mat4, SJH::IRenderable, SJH::Pass::RenderQueue |
| `apps/_MyApp_/main.cpp` | missing-includes + **pessimizing-move**(intended) | EInitTask, DefaultRenderTarget, Image, RenderTextureUPtr / L223 `std::move`(의도된 학습코드) |
| `test/golden_compare/golden_compare.cpp` | unused-includes + missing-includes | algorithm(std::sort 사용=**거짓**), cstdint / cv::Mat, CV_8U, std::error_code |

## 타이밍 (timing)

- **트리거**: 매 파일 open/Edit/Write 시 clangd 가 그 TU 를 재분석 → Strict 진단 재방출. (빌드/ctest 와 무관 — **IDE 인덱싱 전용**, 실빌드는 이번 세션 내내 GREEN·ctest 122 통과.)
- **시작 시점**: 사용자가 이번 세션에 `.clangd` 에 `Diagnostics: Missing/UnusedIncludes: Strict` 를 추가한 순간부터. 그 전(원본 `.clangd` = `CompilationDatabase: build_ninja` 만)엔 이 노이즈 없었음.

## 거짓 vs 진짜 구분 (수정 시 오판 방지)

- **거짓(clangd 정책 노이즈)** — missing-includes("No header providing X") + unused-includes. 빌드는 전이 include 로 정상 컴파일. → 수정 대상.
- **진짜-일시적(편집 도중 정상)** — `override_keyword_only_allowed...`, `No member named 'renderQueue'`, `undeclared identifier mQueueMin` 등. 인터페이스 메서드/필드를 순차 추가하는 중간 상태라 뜬 것 — 편집 완료 시 자동 소멸. **거짓 아님, 수정 불요.**
- **의도된 것** — `main.cpp:223` `-Wpessimizing-move`(학습 가시성용 `return std::move(local)`, [[pessimizing_move_intentional]] — 고치지 말 것) / `frame_capture.cpp` sign-conversion(원본 `golden_capture.cpp:35` 와 바이트동일 관용구 `static_cast<size_t>(h)*stride`, 실빌드 `-Werror` 셋에 없어 GREEN — 소스 수정 불요).

## 수정 담당 에이전트에게 (권장 옵션)

**범위 = `.clangd` 만** (IDE 전용, 빌드 영향 0). 소스의 include 는 이 코드베이스가 전이/우산 헤더를 의도적으로 쓰므로 **소스에 include 를 대량 추가하지 말 것**(스타일 파괴 + 무의미).

- **(A) 권장 — Strict 진단 끄기/완화**: `Diagnostics: UnusedIncludes: None` + `MissingIncludes: None` (또는 섹션 제거). 노이즈 즉시 소멸. 근거: 이 프로젝트는 IWYU 를 강제하지 않는 전이 include 스타일.
- **(B) 절충 — Strict 유지 + -Wconversion 만 제거**: sign-conversion 노이즈만 죽이고 include 진단은 남김(원한다면). 단 include 노이즈는 그대로.
- **(C) IWYU 매핑**: `.clangd` 에 우산/전이 헤더 IWYU pragma/매핑 정의 — 노력 대비 효과 낮음(파일마다 수동).

사용자 의도가 "거짓 에러 해결"이므로 **(A)** 가 직결. `-Werror`도 IDE 표시를 error 로 승격시키니 원하면 함께 완화(`-Wno-error=...` 개별 또는 `-Werror` 제거).

## Guardrails (이 프로젝트)

- 커밋 path-scoped(`git add <경로>`, `git add -A` 금지 — 사용자 병렬 작업). Co-Authored-By 미사용. 주석 한글.
- `.clangd` 는 크로스플랫폼 주의: `build_ninja` 는 macOS/Linux(ninja) 산출물. Windows(MSVC)는 `build_msvc/` — 원본 `.clangd` 주석이 `.vscode/settings.json` 의 `--compile-commands-dir` 로 각자 대응한다고 명시했었음(사용자 diff 가 그 주석 블록을 지웠으니 복원 고려).
- 소스 무수정 원칙(위 "거짓 vs 진짜"): 진짜-일시적/의도된 것은 건드리지 말 것.
- ⚠ `.clangd` 는 현재 **사용자 미커밋(M)** — 수정 전 최신 상태 재확인(`git diff .clangd`), 사용자와 커밋 타이밍 협의.
