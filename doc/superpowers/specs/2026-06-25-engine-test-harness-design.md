# 엔진 테스트 하네스 설계 — Track A (CPU 안전망) + cross-version differential

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **날짜** 2026-06-25 · **브랜치** game/main (HEAD `7949a65`) · **상태** 설계 확정 → /writing-plans 대기
> **범위** Phase 1 (Track A: CPU 순수 로직 단위 테스트) + cross-version differential 하네스 **만**.
> Phase 2 (Track B 골든 이미지)·CI·Mull 뮤테이션은 **별도 plan** (§9).
> **이 문서는 `doc/` 가 .gitignore 로컬이라 커밋되지 않는다** (프로젝트 정책 — 다른 spec 과 동일).

> **마커 범례** — ⚙ **BACKEND-VOLATILE**: 현재 OpenGL 전제, 향후 Graphics API Backend(RHI) 교체(GL→Metal/Vulkan) 시 변경 대상. 🟢 **BACKEND-AGNOSTIC**: 백엔드 무관, 교체 후에도 그대로 유효. (전수 분석 = §11)

---

## 0. 목적 / 배경

게임 엔진 리팩토링 과정을 위한 **강건한 회귀 안전망 + 적대적 검증** 환경 구축. 현 브랜치 HEAD(`7949a65` "Multipass 모듈 리팩토링")가 가장 안정적인 빌드로 판단되어 **모범 동작 oracle** 로 고정한다.

리서치 근거(`doc/Tdd/`): `catch2_pipeline_korean.md`(Catch2+CTest 코어 #1 권고), `<ENV_SETUP_PLAN_v2>/v3.md`(gate-0 결정성·2-Track), Wang et al. survey(arXiv 2307.07221 — test-oracle 문제·differential testing). + 본 저장소 실측(폐기 `test/` git `094362d`, blast-radius 분석).

핵심 통찰: 이 프로젝트는 macOS-primary(GL 4.1 Core/NSGL)라 리서치가 전제한 Linux EGL/llvmpipe 가 안 통한다. 그래서 **결정성 위험 0 인 순수 CPU 로직을 먼저** 친다(D1/D3). 골든 이미지(GPU)는 성격이 완전히 달라 별도 트랙·별도 plan(D1·§9). ⚙ *단 이 "macOS-GL 헤드리스 난점" 전제 자체가 backend-volatile — Metal/Vulkan 백엔드 교체 시 해소될 수 있음(§11·D9).*

---

## 1. 결정 로그 (LOCKED)

| ID | 결정 | 근거 |
|----|------|------|
| **D1** | 순서 = CPU 로직 안전망 **우선**. 렌더 패스 골든 = **확정 2차 트랙**(drop 금지, 별도 plan) | macOS 헤드리스 난점 회피 + 즉시 가치 + catch2_pipeline 우선순위 |
| **D2** | Mull 뮤테이션 = **보류**. P1+P2 안정 후 엔진/위치 재결정 | macOS Apple clang 에 Mull IR 플러그인 불가(Homebrew LLVM 필요) |
| **D3** | Phase 1 첫 타겟 = **순수 로직 모듈**: Timer / object(Transform·Light) / sprite(ComputeUVRect) / fsm(StateMachine) | GL 없이 100% 결정적, first-green 빠름 |
| **D4** | 현 HEAD(`7949a65`) = **검증 기준선(oracle)**. 테스트 철학 = **characterization**(현 동작 잠금) | 사용자 확정: 현 브랜치가 가장 안정 |
| **D5** | 테스트 타겟 구조 = **C** (test_smoke first-green + test/ 모듈별 exe) | cross-version 이 단일-exe(B)를 탈락시킴(§3.2) |
| **D6** | **contract-anchoring**(공개 헤더에만 바인딩) + **cross-version differential 하네스**(`crossver_verify.sh`, `315269a` 대조군). 어댑터 shim 은 API churn 모듈에서만(P1 불요) | "모듈 변경에도 강한 테스트" 요구(사용자) |
| **D7** | cross-version differential = 보류된 적대적 검증(D2)의 **macOS-friendly 저비용 1차 형태** | git worktree + 기존 테스트만으로 두 코드 상태 등가성 검증(survey differential testing) |
| **D8** | characterization 기대값 = **하이브리드**: 손계산 가능한 순수 수학 → (a) hand-derived assert / 불투명 회귀값 → (b) 현 HEAD 1회 snapshot. FP 비교는 `WithinRel(eps)`, crossver diff 는 same-toolchain bit-exact | (a)=계약 검증(현 버그도 잡음), (b)=불투명 값 회귀 잠금 |
| **D9** | **Graphics API Backend = 교체 가능 전제**(향후 RHI: GL→Metal/Vulkan). Track A=🟢backend-agnostic(교체 후 유효), Track B=⚙backend-volatile. GL 결합점 전수 ⚙ 마킹(§11). Track B 의 macOS 난점 자체가 백엔드 종속(Metal offscreen readback + Xcode Frame Debugger 가 개선) | 사용자 방향(2026-06-25): 백엔드 유연 교체 리팩토링 예정 |
| **D10** | 모든 handoff(특히 ★HANDOFF #1 Phase 종료)는 **`lossless-handoff` 스킬**로 생성(Artifact B = resume/context doc). 위치 = `doc/handoffs/2026-06-25/` (⚠ gitignore 로컬 — same-machine 세션 연속성용; cross-machine 인계 필요 시 tracked dir 로) | 사용자 지정(2026-06-25): 무손실 인계 표준화 |

**부속 확정:** 스펙 범위 = P1 full-detail(P2 별도 plan) · CI = 다음 페이즈 · 실행 vehicle = 5-에이전트 파이프라인(§10).

---

## 2. 범위 (IN / OUT)

**IN (이 스펙 + 후속 plan):**
- `test_smoke/` — 배선 first-green (Catch2+vcpkg+CTest+catch_discover_tests 경로 검증).
- `test/` — 모듈별 characterization 단위 테스트 (D3 5타겟).
- `<scripts>/crossver_verify.sh` — cross-version differential 하네스.
- single `verify` 흐름 (build → ctest → exit 0/1), render-test-debug 정합.

**OUT (별도 plan / 다음 페이즈):**
- Phase 2 Track B — 골든 이미지(헤드리스 GL fixture + FBO + glReadPixels + FLIP). support 헬퍼 3종(gl_test_fixture / spdlog_capture / gl_state_snapshot)도 여기 소속. **⚙ BACKEND-VOLATILE 집중 구역**(§11).
- CI (GitHub Actions).
- Mull 뮤테이션 게이트 (D2).
- 5-에이전트 `.md` 정식 갱신(Gate 5 Mull 보류·crossver 반영) — 후속 별건.

---

## 3. 아키텍처 / 컴포넌트

### 3.1 디렉토리 레이아웃 (폐기 `094362d` 구조 계승)

```
test_smoke/
└── CMakeLists.txt + smoke.cpp        (1 trivial TEST_CASE, SJH::common Deg2Rad)
test/
├── CMakeLists.txt                    (모듈별 add_executable + catch_discover_tests + tests 우산)
├── test_timer.cpp                    (신규)
├── test_transform.cpp               (094362d 복원+수리)
├── test_light.cpp                   (094362d 복원+수리)
├── test_sprite_uvrect.cpp           (094362d test_uniform_atlas 복원+수리 → ComputeUVRect 한정)
└── test_fsm.cpp                      (신규)
scripts/
└── crossver_verify.sh               (신규 — worktree 기반 differential)
```

루트 `CMakeLists.txt` 는 이미 `option(ENABLE_TESTING OFF)` + `EXISTS` 가드로 `test_smoke/`·`test/` 를 조건부 `add_subdirectory`(부재해도 통과). **디렉토리만 만들면 즉시 작동** — 루트 CMake 무수정.

### 3.2 타겟 구조 = C (per-module exe + smoke)

각 `test_<module>.cpp` → 독립 `add_executable` + `catch_discover_tests`. 각 exe 는 **그 모듈 헤더만** link.

```cmake
include(CTest)
include(Catch)   # vcpkg Catch2Config 가 CMAKE_MODULE_PATH 에 Catch.cmake 등록

add_executable(test_timer test_timer.cpp)
target_link_libraries(test_timer PRIVATE Catch2::Catch2WithMain SJH::timer)
catch_discover_tests(test_timer)
# ... 모듈별 반복 ...

add_custom_target(tests DEPENDS test_timer test_transform test_light test_sprite_uvrect test_fsm)
```

**왜 단일 exe(B) 가 아닌가:** 단일 `unit_tests` 가 `SJH::engine` 우산(19모듈) 전체를 link 하면, render/ 처럼 **한 모듈이라도 API churn 시**(예: `315269a` 가 render_bootstrap 통째 삭제·mesh_pass_processor 개명) **그 exe 가 다른 커밋에 컴파일 안 됨 → cross-version 테스트를 하나도 못 돌린다.** 모듈별 exe(C)는 안정 부분집합만 양쪽 실행, churn 모듈은 격리 가능.

### 3.3 contract-anchoring 원칙 (D6)

`test_<module>.cpp` 는 **공개 모듈 헤더(=안정 계약)에만** 바인딩. 내부 구현(.cpp 심볼·private 멤버)에 절대 손대지 않는다. → 모듈 내부 리팩토링에도 테스트가 살아남음.

**Phase 1 은 자동 충족** — 실측으로 D3 타겟 공개 헤더가 `7949a65`↔`315269a` byte-identical:
`timer.h`·`state_machine.h`·`transform.h`·`light.h` diff=0줄, `ComputeUVRect` 양 오버로드 무변경(uniform_atlas.h 의 유일 변경은 `GLuint`→`uint32_t TextureId()` + `GL/gl3w.h` include 제거). 🟢 *이 GL-decoupling(헤더에서 GL 타입 제거)은 사용자의 backend-abstraction 방향 early signal — contract-anchoring 테스트가 수혜자(§11).*

### 3.4 characterization 하이브리드 (D8)

- **(a) hand-derived** — 손계산 가능한 순수 수학. 예: `ComputeUVRect(0, 2, 16, 32, 32)` → 좌상단 타일 → `(0,0,0.5,0.5)` 를 *명세에서 계산해* assert. 현재 코드가 틀렸으면 이것도 잡음.
- **(b) snapshot** — 불투명 회귀값(예: `Light::GetAttenuationCoeff` 회귀식). 현 HEAD 1회 출력을 기대값으로 박고 "바뀌면 잡음".
- **FP:** `==` 금지 → `Catch::Matchers::WithinRel(expected, 1e-5f)` (수학) / `1e-4f`(회귀값). crossver diff 는 same-toolchain 이라 bit-exact 허용.
- **(a)에서 손계산값 ≠ 현재 출력 시** → 현 코드 버그 후보. "버그 기록 vs 내 계산 오류" 를 사람이 판단(자동 통과 강행 금지).

### 3.5 cross-version differential 하네스 (D6/D7)

`<scripts>/crossver_verify.sh <baseRef> <candRef>` (기본 `7949a65` vs `315269a`):
1. `git worktree add` 로 두 커밋 각각 펼침 → 엔진 빌드(사용자/CI 동일 toolchain).
2. 외부 `test/` 를 각 worktree 에 overlay(copy/symlink). **`315269a` 도 루트 CMakeLists 미변경**(실측: 델타에 root CMakeLists 없음) → 기존 `ENABLE_TESTING`/`EXISTS` 가드가 overlay 된 test/ 를 자동으로 집어듦. standalone test CMake 불필요.
3. 각 worktree 에서 `-DENABLE_TESTING=ON` 빌드 → `ctest` 실행 → 결과 캡처.
4. 두 결과 diff → **등가성 판정**. 다르면 "315269a 의 dead-code 제거가 공개 동작을 바꿈"(잠재 버그) 신호.

**fallback:** `315269a` 빌드 실패 시(사용자 보장하나 방어) → U5 를 **self-regression**(같은 커밋 2회 = 결정성 카나리아)로 축소, 315269a 대조는 빌드 수리 후. 인프라는 그대로.

---

## 4. Phase 1 타겟 명세

네임스페이스: `SJH::Timer` / `SJH::Object` / `SJH::Sprite` / `SJH::FSM` / `SJH::`(common).

| 타겟 | 출처 | 공개 표면(테스트) | 기대값 |
|------|------|------------------|--------|
| **smoke** | 신규 | `Deg2Rad`/`Rad2Deg` | (a) |
| **test_timer** | 신규 | `Tick(dt)`·`IsTimesUp`·`GetProgress`·`PollInterval`·`Pause`·`Resume`·`Reset` (누적/가속/인터벌 폴링/pause-resume 조합, 경계) | (a) |
| **test_transform** | 복원+수리 `094362d` | `GetLocalMatrix`·`GetRotationMatrix`·`GetRight/Up/Forward` (각도→행렬, TRS 합성, 6벡터 직교성) | (a) |
| **test_light** | 복원+수리 | `Pos/Ambient/Diffuse/Specular` POD + `static GetAttenuationCoeff(distance)`→(Kc,Kl,Kq) | POD=(a) / 감쇠=(b) |
| **test_sprite_uvrect** | 복원+수리 `test_uniform_atlas` → ComputeUVRect 한정 | 2 오버로드: `(frameIdx,cols,tileSize,atlasW,atlasH)` / `(...,tileW,tileH,...)` → vec4(uMin,vMin,uSize,vSize) (grid 변형·edge index·정사각/비정사각) | (a) |
| **test_fsm** | 신규 | `StateMachine<TState,TOwner>`: `RegisterState`·`TryTransit`·`ForceTransit`·`State` + mock `IFsmState<TOwner>`(GetStateFlag/GetTransitFlag) (유효/무효 전이·pending) | (a) |

복원 절차: `git show 094362d:test/test_<x>.cpp` → API drift 수리(현 헤더 시그니처 정합) → contract-anchoring 위반(내부 접근) 제거.

---

## 5. verify 흐름 (render-test-debug 정합)

```bash
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target tests
ctest --test-dir build_ninja --output-on-failure   # exit 0/1
```
- 모든 `test_*` 가 `catch_discover_tests` 로 자동 등록 → render-test-debug 1단계 그대로.
- crossver: `sh <scripts>/crossver_verify.sh 7949a65 315269a` → 등가 0/1.

---

## 6. ★ HANDOFF-UNIT 지도 (핵심 — 무손실 인계)

> 규칙(메모리 `plan-handoff-boundary-markup`): unit = 검증가능+커밋가능 green 체크포인트(≈1 컨텍스트). green 마다 partial commit(`git commit <경로>` — 메모리 `user-parallel-git`). ★=필수 handoff, ☆=컨텍스트 빠듯 시 보조 절단.

| unit | 내용 | green 기준 | commit | 출처 | ctx |
|------|------|-----------|--------|------|-----|
| **U0** | `test_smoke/` 1-case + 루트 가드 활성 | `ctest` 1 GREEN | `test_smoke/` | 신규 | 小 |
| **U1** | `test_timer` (8메서드 characterization) | GREEN | `test/test_timer*` | 신규 | 中 |
| **U2** | `test_transform` + `test_light` (복원+수리) | GREEN | `test/test_transform* test_light*` | 094362d | 中 |
| ☆ | *여기까지 보통 한 세션. 빠듯하면 U2 끝에서 절단* | | | | |
| **U3** | `test_sprite_uvrect` (복원+수리) | GREEN | `test/test_sprite*` | 094362d | 小中 |
| **U4** | `test_fsm` (template mock 구체화) + `tests` 우산 | GREEN | `test/test_fsm* test/CMakeLists` | 신규 | 中 |
| **U5** | `<scripts>/crossver_verify.sh` capstone: 두 worktree(7949a65/315269a) 빌드 GREEN 선결 → P1 모듈 테스트 5 exe(test_timer/transform/light/sprite_uvrect/fsm) 양쪽 실행 → diff 등가 | 두 커밋 결과 동일 | `<scripts>/crossver_verify.sh` | 신규 | 中 |
| **★ HANDOFF #1** | **Phase 1 종료.** P1 전체 GREEN + crossver 등가 증명 스냅샷 → **`lossless-handoff` 스킬로 Artifact B 생성** → `<doc>/handoffs/2026-06-25/...-test-harness-p1-resume-handoff.md`(맥락0 자기완결). **다음 = Phase 2 Track B 별도 plan**(성격 전환: GL fixture+FBO+FLIP) | | | | |

**day-0 선결(U5 전):** `315269a` worktree 빌드 GREEN 확인(사용자 보장, 방어적 체크).

**handoff 생성 규약 (D10):** ★(및 트리거되면 ☆) 지점의 인계 문서는 모두 **`lossless-handoff` 스킬(Artifact B = resume/context doc)** 로 작성 — Step 0 컨벤션 발견(빌드/커밋/테스트 정책) + 5원칙(self-containment·grounding·scope boundary·guardrails·hygiene) 적용. 위치 `doc/handoffs/2026-06-25/` ⚠ **gitignore 로컬**(same-machine 세션 연속성 용도 — 컨텍스트 compact 생존이 주 목적; 다른 머신 인계가 필요해지면 tracked dir 로 이동). grounding 원칙상 작성 시점에 `git log`/`git status`/`file:line` 재측정 필수.

---

## 7. 오라클 사각지대 / trade-off (명시)

- **characterization = 불변성 보장, 정확성 아님.** 현 코드 버그도 함께 잠긴다((b) 경로). 의도적 동작 변경 시 기대값을 **사람이 갱신**(자동 갱신 금지 — CITYWALK §5.4 오라클 문제와 동형).
- **Track A 미커버:** GL 상태 누수·렌더 출력·드로우 순서·성능/메모리. → Phase 2 Track B + (후속) 상태 스냅샷.
- crossver 는 **공개 동작 등가**만 증명. 내부 성능/메모리 회귀는 못 잡음.

---

## 8. 모듈 단위 명료성 (isolation)

각 `test_<module>` exe: **무엇을**(해당 모듈 공개 계약 characterization) · **어떻게**(Catch2WithMain + 모듈 헤더) · **의존**(그 `SJH::<module>` 하나). 서로 독립 빌드·실행·이해 가능. crossver 가 이 격리를 직접 활용(안정 부분집합만 양쪽 실행).

---

## 9. 미해결 / 후속 (별도 plan)

1. **Phase 2 Track B (골든 이미지)** ⚙ — 별도 spec+plan. macOS reference 환경 = Apple Silicon GL 4.1 + 숨김 GLFW 윈도우(Graphics-Testing-Prompt 기확정), glFinish+glReadPixels, FLIP ≤0.05 PASS/>0.10 FAIL. support 헬퍼 3종 부활. 골든 = 현 HEAD 캡처(D4). **착수 시점 결정 입력: 이 전체가 backend-volatile(§11) — 현 GL 로 즉시 갈지 vs 백엔드(Metal/Vulkan) 방향 잡힐 때까지 일부 대기할지 P2 plan 에서 재결정.**
2. **CI** — 다음 페이즈. 동일 GPU/toolchain 강제.
3. **Mull 뮤테이션** (D2 재결정).
4. **5-에이전트 `.md` 갱신** — Gate 5(Mull) 보류 표기 + crossver 단계 추가.

---

## 10. 5-에이전트 연계 (실행 vehicle)

- **render-test-debug** — 1단계 `ctest` + (추가) crossver 호출. 골든은 P2 까지 SKIPPED. ⚙ *GL 상태 snapshot diff 단계(2단계)는 backend-volatile(§11) — P2/백엔드 교체 시 상태 모델 치환.*
- **render-quality-gate** — Gate 1(빌드+Catch2) 유효, Gate 5(Mull) **보류/SKIP**(D2), crossver 등가를 보조 게이트로.
- **render-refactorer** — worktree 격리(crossver 와 동형). `test/golden/` 쓰기 차단(P2).
- 권한 매트릭스(`doc/testplan/Graphics-Testing-Prompt.md`)·anti-gaming(test≠code 격리) 유지.
- 정식 `.md` 갱신은 §9.4 후속.

---

## 11. Graphics API Backend 교체 여지 (backend-volatility map)

> **사용자 방향(2026-06-25):** 향후 **Graphics API Backend 를 유연 교체**하는 리팩토링 예정 — 현재 OpenGL, 목표 Metal/Vulkan (RHI 추상층 뒤). 이 절은 본 테스트 하네스에서 **백엔드 교체 시 바뀔 지점(⚙)** 과 **안 바뀔 지점(🟢)** 을 전수 마킹한다. **Phase 1 구현에는 영향 0** — 아래는 미래 변경점 가시화 + Track B(P2) plan 착수 시점 입력.

### 11.1 🟢 BACKEND-AGNOSTIC (교체 후에도 그대로 유효)

| 항목 | § | 비고 |
|------|---|------|
| **Track A 전체**(Timer/Transform/Light/ComputeUVRect/FSM) | §4 | 순수 CPU 수학·상태, GL 호출 0 → Metal/Vulkan 무관. **백엔드 교체 전·후 동일 유효한 안전망** |
| characterization 하이브리드 · FP epsilon | §3.4 | 값 비교 로직, 백엔드 무관 |
| cross-version differential 하네스 | §3.5 | build+run+diff — 엔진이 어느 백엔드로든 빌드되기만 하면 동작 |
| Catch2+CTest+catch_discover_tests 배선 · 타겟 구조 C | §3.1-3.2 | 빌드/테스트 프레임워크, 백엔드 무관 |
| contract-anchoring 원칙 | §3.3 | **오히려 백엔드 교체를 도움**(공개 계약만 의존, 내부 GL 호출 비의존) |
| spdlog_capture 헬퍼 | §2 | 로그 캡처, 백엔드 무관 |

→ **핵심: 이 스펙 본체(Track A)는 backend-agnostic.** OpenGL→Metal/Vulkan 교체 후에도 손 안 대고 회귀 안전망으로 계속 쓴다. (그래서 "CPU 안전망 먼저"(D1) 가 백엔드 리팩토링 관점에서도 옳은 선행 — 교체 중 흔들리지 않는 고정점.)

### 11.2 ⚙ BACKEND-VOLATILE (교체 시 변경 대상)

| 항목 | § | 현재(GL) | 교체 후(Metal/Vulkan) |
|------|---|---------|----------------------|
| "macOS headless 어렵다" 전제 | §0 | GL on macOS = EGL surfaceless 없음, NSGL/CGL 컨텍스트 필요 | **Metal: 오프스크린 MTLTexture 렌더 + `getBytes` readback = 윈도우 없이 깔끔**(GL보다 헤드리스 친화). Vulkan(MoltenVK): 헤드리스 + validation layers. → **이 난점 자체가 backend-volatile** |
| Track B 골든 fixture | §2·§9.1 | 숨김 GLFW 윈도우 + FBO + `glReadPixels` + `glFinish` | Metal: offscreen MTLTexture + GPU capture / Vulkan: `vkCmdCopyImageToBuffer` + RenderDoc |
| gl_test_fixture / gl_state_snapshot 헬퍼 | §2·§10 | GL 컨텍스트 · `glGetIntegerv` 상태 | 백엔드별 상태 스냅샷으로 치환 |
| render-test-debug GL 상태 diff 단계 | §10 | GL state snapshot diff | 백엔드별 상태 모델 |
| 이미지 비교 도구 FLIP/ImageMagick | §9.1 | 픽셀 diff(출력 PNG 면 백엔드 무관) | 동일 — 단 노이즈/결정성 기준은 백엔드별 재측정 |

### 11.3 Xcode Frame Debugger 가설 (사용자 제기) — 정직한 평가

사용자 직관은 **방향상 맞다**: Metal/Vulkan 백엔드는 macOS 헤드리스 골든 상황을 **실질 개선**한다. 단 역할 구분 필요 *(확신도: 중–높음 — 아키텍처 추론. Metal/MoltenVK API 세부는 Track B 착수 시 Context7 교차검증 권장)*:
- **자동화 enabler = Metal offscreen MTLTexture readback** — 윈도우 불요라 GL 의 macOS 헤드리스 난점을 해소. 이게 골든 *게이트* 를 가능케 하는 실제 요인.
- **Xcode Frame Debugger(Metal GPU Frame Capture) = 진단 보조** — 골든 *불일치 시* GPU 파이프라인을 시각적으로 스텝·검사하는 도구. 자동 게이트 자체는 아니지만 mismatch 디버깅을 크게 도움.
- Vulkan 경로면 validation layers + RenderDoc 캡처가 동급 역할.

→ **함의:** Track B(P2)의 macOS-GL 난점은 영구 제약이 아니라 **백엔드 결정에 종속**. Track B 별도 plan 은 **(i) 현 GL 로 즉시 진행 vs (ii) 백엔드 교체 방향이 잡힐 때까지 일부 대기** 중 택일 — P2 plan 착수 시 재결정(지금 결정 불필요).

### 11.4 早期 증거 (early signal)

`315269a` 가 이미 **GL-decoupling 방향**으로 움직였다: sprite `uniform_atlas.h` 에서 `#include "GL/gl3w.h"` 제거 + `GLuint`→`uint32_t TextureId()`(§3.3). dead-code 제거 리팩토링이 헤더의 GL 의존을 떼는 중 — 사용자 backend-abstraction 방향과 정합. **contract-anchoring(공개 헤더 의존) 테스트는 이 흐름의 직접 수혜자**: GL 타입이 헤더에서 빠질수록 테스트가 자동으로 더 backend-agnostic 해진다.

### 11.5 이 plan 에 대한 함의 (요약)

1. **Phase 1 (이 스펙) = 무영향.** Track A 는 backend-agnostic — 지금 짜고 백엔드 교체 후에도 그대로 쓴다.
2. **Track B (P2 별도 plan) = backend-volatile 집중.** 착수 시 "현 GL vs 백엔드 대기" 를 §11.3 기준으로 재결정.
3. **장기:** RHI 추상층이 생기면 Track B fixture 를 그 seam 뒤로 작성 → 백엔드별 readback 만 교체, 골든 시나리오/비교는 공유. (그 시점 별도 spec.)
