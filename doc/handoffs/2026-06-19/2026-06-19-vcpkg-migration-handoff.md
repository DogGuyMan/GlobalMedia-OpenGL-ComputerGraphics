# vcpkg 의존성 마이그레이션 — 핸드오프 (다른 Claude Agent 위탁용)

- **날짜**: 2026-06-19
- **목적**: 현재 "prebuilt lib 체크인 + git 서브모듈" 의존 관리를 **vcpkg(서브모듈 + manifest 모드)** 로 전이. WebEditor 작업보다 **먼저** 수행한다 (사용자 지시).
- **이 문서**: §1~§9 = 컨텍스트/계획(Artifact B). §10 = 다른 에이전트에 그대로 붙여넣을 프롬프트(Artifact A).
- **연관**: 이 마이그레이션 완료 후 WebEditor 작업 재개 → [doc/superpowers/specs/2026-06-19-webeditor-master-data-foundation-design.md](../../doc/superpowers/specs/2026-06-19-webeditor-master-data-foundation-design.md), 리서치 [doc/webeditor/00-INDEX.md](../../doc/webeditor/00-INDEX.md).

---

## 1. TL;DR + 다음 행동

게임 의존성을 vcpkg로 옮기되, **버전 핀 우선(코드 무변경)** 으로. **다음 행동 = Phase 0 감사**: 각 의존의 vcpkg 가용성/정확 핀 버전/블래스트 반경을 실측해 보고하고 **사용자 승인 후** 실제 전이 시작. (box2d/glfw는 코드·ABI 호환이 걸려 있어 감사 없이 전이 금지.)

## 2. 결정 컨텍스트 — 왜 지금 OK인가 (CLAUDE.md 정면 반전)

- CLAUDE.md는 *"교수 제출용으로 vcpkg 미사용 — CMake 단독 완결 + lib/·include/ prebuilt 체크인"* 이라 명시. **이 제약의 근거(교수 평가 제출)는 종료됨** (사용자 확인 2026-06-19). → 자체완결성/오프라인 빌드는 **더 이상 필수 아님**. CLAUDE.md 갱신 승인됨.
- 따라서 vcpkg manifest의 "configure 시 네트워크 복원"이 허용된다. (오프라인 바이너리 캐시/`vcpkg export`는 *선택*으로 격하 — 재현성 위해 권장하되 필수 아님.)

## 3. 잠긴 결정 (재논의 금지)

1. **vcpkg 통합 = 서브모듈 + manifest 모드.** vcpkg를 `extern/vcpkg` 서브모듈로 추가, repo 루트 `vcpkg.json` manifest + `builtin-baseline` 핀, CMake `CMAKE_TOOLCHAIN_FILE`로 연결.
2. **전이 범위 = 최대, 단 "버전 핀 우선" 해석.** vcpkg로 옮기되 *현재 버전을 핀해 코드 변경을 회피*. 핀 불가능한 의존만 (a) 업그레이드+코드수정(블래스트 반경 보고 후) 또는 (b) 현행 잔류 — **사용자 결정**.
3. **확정 잔류 (vcpkg 전이 안 함):**
   - **FMOD** — 독점 SDK, vcpkg 부재. prebuilt dylib/dll 유지 (`lib/`).
   - **Effekseer (+EffekseerRendererGL)** — 공개 레지스트리 부재 추정. **일단 현행 유지** (사용자 확인).
   - **sb7** — SuperBible 커스텀 정적 라이브러리, vcpkg 부재. 유지. (sb7code 서브모듈 + prebuilt; **sb7 수정 금지** 메모리 가드.)
   - **stb** — 헤더온리, 사용자가 `extern`/`include` 유지 선택.
   - **imgui (v1.53)** — vcpkg 버전 DB에 1.53(2017) 부재 거의 확실 + GLFW 3.0.4 호환 핀이라 게임은 v1.53 유지. (신형 ImGui는 WebEditor가 별도 사용 — 본 마이그레이션 무관.)
4. **공개 API 불변**: `project_deps` / `game_deps` INTERFACE 우산 + `SJH::engine` 의 *이름/의미*는 유지. 내부 backing만 IMPORTED→vcpkg `find_package`로 교체 → 17개 모듈/앱 CMakeLists 무수정 목표.

## 4. 현재 상태 (grounded, 2026-06-19)

**서브모듈 (`.gitmodules`)**: sb7code, box2d, Effekseer, tweeny, stb, assimp, spdlog, imgui(master), Catch2(devel).

**prebuilt (`lib/macos`, `lib/windows`)**: Effekseer(+RendererGL), assimp(+zlibstatic), box2d, spdlog, glfw3, sb7, **fmod(+studio, L 변형)**. (windows는 fmod·glfw3·sb7만.)

**의존 등록 = [cmake/Dependency.cmake](../../cmake/Dependency.cmake)**:
- `project_deps` (INTERFACE) = sb7 + glfw3 + OpenGL + 플랫폼 프레임워크. include = `${SOURCE}/include`.
- `game_deps` (별도, CLAUDE.md 기준) = box2d + Effekseer + assimp(+zlibstatic) + spdlog + tweeny + stb + (조건부)FMOD.
- 전부 `add_library(X STATIC IMPORTED)` + `IMPORTED_LOCATION(_DEBUG)` 패턴. spdlog는 `SPDLOG_COMPILED_LIB` INTERFACE 전파. tweeny/stb_extra는 INTERFACE 표식(헤더는 `include/` 체크인).

**프리셋 = [CMakePresets.json](../../CMakePresets.json)**: base / base-ninja / base-msvc / ninja / ninja-release / msvc / msvc-2022 (+ build presets). **toolchainFile 미설정** → vcpkg toolchain을 base에 추가해야 함.

**imgui**: 게임이 client-side로 `extern/imgui` v1.53 코어 + `opengl3_example` backend를 executable에 직접 add (`apps/_MyApp_/CMakeLists.txt` IMGUI_SRC). vcpkg 전이 대상 아님(§3).

**블래스트 반경(실측)**:
- **Box2D API 사용 = 36개 파일** (`apps/_MyApp_/src` 내 b2World/b2Body/b2Vec2 등). v2.4.1. → **v3(C API)로 가면 36파일 재작성**. 반드시 2.4.x 핀.
- imgui = v1.53 고정 확인 (`extern/imgui/imgui.h` IMGUI_VERSION "1.53").

## 5. 의존 처리표 (잠정 — Phase 0이 확정)

| 의존 | 현재 | 목표 | 핀/리스크 | 비고 |
|---|---|---|---|---|
| assimp (+zlib) | prebuilt+submodule | **vcpkg** | 비교적 안전 | vcpkg `assimp`가 zlib 동반 |
| spdlog | prebuilt+submodule | **vcpkg** | 안전 | `SPDLOG_COMPILED_LIB` 유지 확인 |
| tweeny | header(submodule) | **vcpkg** | 안전(헤더온리) | vcpkg `tweeny` 존재 확인 |
| Catch2 | submodule(add_subdir) | **vcpkg** | 안전 | `find_package(Catch2 3)` + `catch_discover_tests` |
| glfw3 | prebuilt | **vcpkg(조건부)** | ⚠ **sb7 ABI 결합** | sb7(잔류)가 glfw 3.0.4에 링크. vcpkg glfw(3.4)로 sb7 링크 깨지면 **glfw도 잔류** |
| box2d | prebuilt+submodule v2.4.1 | **vcpkg(조건부)** | ⚠ **36파일** | vcpkg에 2.4.x 핀 가능하면 전이(무변경), 불가면 잔류 또는 v3 재작성(사용자 결정) |
| **FMOD** | prebuilt(독점) | **잔류** | — | vcpkg 부재 확정 |
| **Effekseer(+GL)** | prebuilt+submodule | **잔류(일단)** | — | 레지스트리 부재 추정, 사용자 유지 선택 |
| **sb7** | prebuilt+submodule | **잔류** | — | 커스텀, 수정 금지 |
| **stb** | header(submodule) | **잔류** | — | 사용자 유지 선택 |
| **imgui** | submodule v1.53(client build) | **잔류** | — | 1.53 핀, GLFW3.0.4 호환 |

## 6. 단계별 계획

**Phase 0 — 가용성/버전 감사 (STOP & REPORT, 승인 게이트)**
- vcpkg 레지스트리에서 각 전이 후보(assimp/spdlog/tweeny/catch2/glfw3/box2d)의 **존재 + 핀 가능 정확 버전**(특히 **box2d 2.4.x**, glfw가 sb7과 호환되는 버전) 실측.
- box2d 2.4.x 핀 불가 시 v3 마이그레이션 블래스트 반경(36파일) 명시.
- glfw 전이 시 sb7 링크 호환 검증(작은 PoC 또는 ABI 추론).
- **산출**: 처리표 확정본 + 리스크 + 권장 → **사용자 승인 받고 Phase 1 진입.** (승인 전 코드/빌드 변경 금지.)

**Phase 1 — vcpkg 부트스트랩 + 툴체인 + manifest**
- `extern/vcpkg` 서브모듈 추가, `.gitmodules` 갱신. bootstrap.
- 루트 `vcpkg.json`(dependencies + `builtin-baseline`) 작성. box2d/glfw 등은 `overrides`로 버전 핀.
- `CMakePresets.json` `base`에 `CMAKE_TOOLCHAIN_FILE = extern/vcpkg/scripts/buildsystems/vcpkg.cmake` 추가(+ 필요 시 chainload). msvc/ninja/msvc-2022 모두 상속 확인.

**Phase 2 — 의존별 cutover (한 번에 하나, 빌드 GREEN 유지)**
- `cmake/Dependency.cmake`에서 전이 대상 `IMPORTED` 블록 → `find_package(X CONFIG REQUIRED)` + `target_link_libraries(project_deps/game_deps INTERFACE X::X)`로 교체. **우산 타겟 이름/의미 불변.**
- 의존 하나 바꿀 때마다 `_MyApp_` 빌드 GREEN 확인 후 다음.

**Phase 3 — 정리 + 문서**
- 전이된 의존의 `lib/` prebuilt + `include/` 헤더 + `.gitmodules` 항목 제거. **잔류(FMOD/Effekseer/sb7/stb/imgui)는 보존.**
- **CLAUDE.md 갱신**(승인됨): "vcpkg 미사용" → "vcpkg 사용(서브모듈+manifest)"로, 잔류 의존 목록/근거 명시. Dependency.cmake/Extern 섹션 동기화.

**Phase 4 — 전 프리셋 검증 + CI**
- ninja / ninja-release / msvc / msvc-2022 전부 `_MyApp_` 빌드 + `ENABLE_TESTING=ON` ctest GREEN.
- GitHub Actions(`build-msvc.yml` 등) vcpkg 부트스트랩 반영.

## 7. 가드레일 & 컨벤션

- **빌드 GREEN이 진실의 기준**: 각 cutover 후 `cmake --build --preset ninja --target _MyApp_` exit 0 확인. 막연한 "되겠지" 금지.
- **부분 커밋만**: 사용자가 같은 워킹트리에서 병렬 작업 → `git add -A` 금지, **경로 지정 커밋**(`git commit <path>`). 탈락 시 reflog 복구.
- **커밋 메시지에 `Co-Authored-By` 미사용** (프로젝트 관례). 커밋은 사용자 승인 시에만.
- **잔류 의존 절대 손대지 말 것**: FMOD / Effekseer / sb7 / stb / imgui 경로·버전. **sb7code 수정 금지**(불가변).
- **단위 테스트 자동 추가 금지** (요청 시에만). Catch2 전이는 빌드 wiring만.
- **주석 한국어**, 식별자 명명 규칙(멤버 mPascalCase 등) 준수.
- **box2d/glfw는 Phase 0 승인 없이 전이 금지** (코드/ABI 결합).

## 8. 병렬 트랙 충돌 매트릭스

| 파일/영역 | 이 마이그레이션 | WebEditor 작업(이후) | 비고 |
|---|---|---|---|
| `cmake/Dependency.cmake` | **소유(편집)** | 읽기 | vcpkg 먼저 → WebEditor는 그 위에서 |
| `CMakePresets.json` | **소유**(toolchain/emscripten) | 추가(emscripten 프리셋) | 순서: vcpkg 먼저, WebEditor가 emscripten 프리셋 추가 |
| 루트 `CMakeLists.txt` | 소유(vcpkg wiring) | 추가(`editor/`) | 충돌 적음 |
| `doc/webeditor/*`, WebEditor 스펙/핸드오프 | **건드리지 않음** | 소유 | 본 마이그레이션과 무관 |
| `apps/_MyApp_/src/**` | **건드리지 않음**(box2d 핀 시) | — | box2d v3 결정 시에만 예외 |

→ **순서: vcpkg 마이그레이션 완료 → WebEditor 재개.** (사용자 지시.)

## 9. 검증 커맨드 (정본)

```bash
cmake --preset ninja                                  # configure (vcpkg toolchain 적용 확인)
cmake --build --preset ninja --target _MyApp_         # 빌드 GREEN
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target tests
ctest --test-dir build_ninja --output-on-failure      # 테스트 GREEN
# Windows: msvc / msvc-2022 프리셋 동일 검증
```
**완료 정의**: 전 프리셋에서 `_MyApp_` + ctest GREEN, 전이 의존은 vcpkg에서, 잔류 의존은 보존, CLAUDE.md 동기화.

**(전방 시너지 메모)**: vcpkg는 `wasm32-emscripten` triplet을 지원 → 추후 WebEditor의 신형 ImGui/cpp-httplib도 vcpkg로 관리 가능(본 마이그레이션 범위 밖, 참고만).

---

## 10. Artifact A — 다른 Claude Agent에 붙여넣을 프롬프트

```
역할: 너는 이 C++17 CMake 프로젝트(OpenGL 탑다운 슈터 게임 + SJH 엔진)의 의존성 관리를
"prebuilt lib 체크인 + git 서브모듈"에서 vcpkg(서브모듈 + manifest 모드)로 전이하는 빌드
엔지니어다. 작업 디렉토리 루트에서 작동. 게임 코드(apps/_MyApp_/src)는 가급적 건드리지 않는다.

[필독 컨텍스트]
- 상세 계획·근거·처리표·검증은 doc/handoffs/2026-06-19/2026-06-19-vcpkg-migration-handoff.md 에 있다. 먼저 읽어라.
- 단, 그 문서의 모든 "현재 상태"(파일/버전/경로)는 네가 라이브로 재검증하라(repo가 병렬로 바뀔 수 있음).

[하드 룰]
- 빌드 GREEN이 유일한 진실 기준. 각 변경 후 `cmake --build --preset ninja --target _MyApp_` exit 0 확인.
- 커밋은 사용자 승인 시에만, 경로 지정(`git commit <path>`)으로만. `git add -A` 금지. 커밋 메시지에 Co-Authored-By 금지.
- 단위 테스트 자동 추가 금지. 주석 한국어. 식별자 명명 규칙 준수.

[잠긴 결정]
- vcpkg = extern/vcpkg 서브모듈 + 루트 vcpkg.json(manifest) + builtin-baseline 핀 + CMakePresets base에 toolchain.
- 공개 우산 타겟(project_deps / game_deps / SJH::engine) 이름·의미 불변. backing만 IMPORTED→find_package.
- 잔류(전이 금지): FMOD(독점), Effekseer(+GL, 일단 유지), sb7(커스텀·수정금지), stb(헤더), imgui(v1.53 게임용).
- 전이 후보: assimp/spdlog/tweeny/catch2/glfw3/box2d. 단 "버전 핀 우선 = 코드 무변경"이 원칙.

[검증된 사실 — 그래도 재확인하라]
- cmake/Dependency.cmake: 모든 의존이 add_library(X STATIC IMPORTED)+IMPORTED_LOCATION(_DEBUG) 패턴. project_deps=sb7+glfw3+GL.
- CMakePresets.json: base/base-ninja/base-msvc/ninja/ninja-release/msvc/msvc-2022. toolchainFile 미설정.
- Box2D API가 apps/_MyApp_/src의 36개 파일에 사용됨. 버전 v2.4.1. → v3(C API)면 36파일 재작성.
- imgui는 게임이 extern/imgui v1.53를 client-side로 컴파일(apps/_MyApp_/CMakeLists.txt IMGUI_SRC). 전이 대상 아님.
- glfw3 prebuilt는 sb7(잔류)이 링크. vcpkg glfw로 바꾸면 sb7 ABI 호환을 검증해야 함.

[STEP 1 = Phase 0 감사 — 여기서 멈추고 보고]
- vcpkg 레지스트리에서 assimp/spdlog/tweeny/catch2/glfw3/box2d의 존재 + 핀 가능 정확 버전 실측.
  특히: box2d 2.4.x가 vcpkg에 핀 가능한가? glfw가 sb7과 호환되는 버전은?
- box2d 2.4.x 핀 불가 시 v3 마이그레이션 비용(36파일) 명시. glfw 전이 시 sb7 링크 리스크 평가.
- 산출 = 처리표 확정본 + 리스크 + 권장. **사용자 승인 전까지 파일/빌드 변경 절대 금지.**
- 보고 형식: 의존별 (vcpkg 가용 Y/N, 핀 버전, 코드영향, 권장: 전이/핀전이/잔류).

[STEP 2+ = 승인 후에만]
- Phase 1(vcpkg 부트스트랩+toolchain+manifest) → Phase 2(의존별 cutover, 하나씩 빌드GREEN)
  → Phase 3(전이된 lib/·include/·.gitmodules 제거 + CLAUDE.md 갱신) → Phase 4(전 프리셋+ctest 검증).
- 각 cutover: Dependency.cmake의 IMPORTED 블록 → find_package(X CONFIG REQUIRED)+우산 타겟 링크. 우산 이름 불변.
- 잔류 의존(FMOD/Effekseer/sb7/stb/imgui)의 lib/·include/·gitmodules는 보존.

[경계]
- 소유: cmake/Dependency.cmake, CMakePresets.json, 루트 CMakeLists.txt, vcpkg.json, .gitmodules, lib/, include/(전이분만), CLAUDE.md.
- 절대 금지: apps/_MyApp_/src/** 수정(box2d v3 승인 시 예외), doc/webeditor/**, WebEditor 스펙/핸드오프, sb7code, 잔류 의존 경로.

[검증]
- `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_` exit 0.
- `cmake --preset ninja -DENABLE_TESTING=ON && cmake --build --preset ninja --target tests && ctest --test-dir build_ninja --output-on-failure` GREEN.
- Windows면 msvc/msvc-2022도.

[자체 점검]
- 우산 타겟 이름/의미 안 바뀌었나? 잔류 의존 손 안 댔나? 빌드 GREEN 실제 확인했나? 경로 지정 커밋인가?

[보고]
- 상태(DONE / DONE_WITH_CONCERNS / BLOCKED) + 변경 파일 목록 + 빌드/테스트 결과(실제 출력) + 미결/지연 항목.
- 커밋하지 마라(사용자 승인 대기). Phase 0 보고 후 멈춰라.
```

**오케스트레이터 메모(프롬프트 밖)**: 권장 커밋 메시지 예 `[build] : vcpkg 의존성 마이그레이션 (Phase N)`. 이 마이그레이션 종료 후 WebEditor 작업(스펙/리서치 준비됨) 재개.
