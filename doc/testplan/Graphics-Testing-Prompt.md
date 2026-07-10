# Graphics Refactoring Guardrails

> 이 파일은 사용자의 기존 anti-hallucination CLAUDE.md에 **추가로 합쳐야 할** 그래픽스 도메인 특화 섹션이다. 기존 13개 체크포인트를 덮어쓰지 마라. 그 아래에 본 섹션을 덧붙인다.
>
> 빌드: CMake + Ninja (`build_ninja/`) + MSVC (`build_msvc/`) / 그래픽스: OpenGL 4.1 Core / 단위 테스트: Catch2 v3 / 헤드리스 GL: GLFW invisible window (`test/support/gl_test_fixture`)

---

## §A. 그래픽스 코드의 절대 규칙

### A.1 GL 호출 위치와 진단 래핑

본 프로젝트는 **직접 GL 호출을 허용**한다 (RHI 추상화 미사용). 그러나 디버그 빌드에서 회귀를 빨리 잡으려면 다음을 따른다:

- 셰이더/프로그램 객체 상태(`GL_COMPILE_STATUS`/`GL_LINK_STATUS`/`GL_VALIDATE_STATUS`) 는 `SJH::Diagnostics::GLObjectLog::Check*` 로 점검한다 — 호출 직후 무조건.
- 일반 GL 호출 직후 에러는 `SJH_GL_CHECK(x)` 매크로(`diagnostics/gl_log.h`) 로 폴링. NDEBUG 빌드에선 no-op.
- KHR_debug 가능 환경에서는 `SJH::Diagnostics::GLDebug::Init()` 를 챕터/컨텍스트 초기화에서 1회 호출 (macOS GL 4.1 Core 는 KHR_debug 미지원 → no-op).
- uniform 누락 warn-once 는 `SJH::Diagnostics::UniformDiagnostics` 사용 — 같은 uniform 을 매 프레임 경고하는 노이즈 방지.
- GL 상태 차이는 `test/support/gl_state_snapshot` (`GLStateSnapshot::Capture()` + `Diff()`) 로 캡처 — 회귀 디버깅 시.

> ⚠️ "RHI/`IRHIDevice`/`IRHICommandList` 인터페이스 경유" 같은 표현이 본 저장소 다른 곳에 남아있다면 **outdated** — 본 프로젝트엔 그런 추상화가 없다.

### A.2 결정적 시드 시나리오만 사용

테스트가 깨지는 가장 흔한 원인은 비결정성이다. 다음을 강제한다:

- 카메라 위치·시간·RNG 시드 모두 고정 (예: `Camera` 의 view 행렬은 씬 노드 Transform 으로 고정값 주입)
- 골든 이미지 캡처 시 `glFinish()` 호출 후 `glReadPixels` 로 readback
- 시간 기반 효과(애니메이션, 파티클)는 `frame_index` 매개변수로 강제 결정성

### A.3 OpenGL 드라이버 차이 회피

A 논문이 "stochastic outputs"를 핵심 한계로 명시. 그래픽스에서는 드라이버 차이가 stochastic 의 주범이다.

- **진실의 원천(source of truth)** — 본 프로젝트는 헤드리스 백엔드(Mesa llvmpipe 등) 를 별도 구축하지 않는다. 대신 **CI 매트릭스의 가장 결정적인 환경 하나**를 reference 로 고정한다:
  - macOS: 로컬 dev 의 Apple Silicon GL 4.1 Core
  - Windows: `build-msvc.yml` GitHub Actions 의 MSVC + Windows-latest (`exercise7_win` 패턴)
- 실 사용자 머신의 GPU 골든은 신뢰 불가 — reference 환경에서 캡처한 이미지만 commit.

---

## §B. 골든 이미지 비교 — 결정 임계값

본 프로젝트는 골든 이미지 디렉토리를 아직 만들지 않았다 (Track B 미구현). 도입 시 다음 규약을 따른다:

### B.1 디렉토리 / 도구

- 골든 위치: `test/golden/<scene>.png` (test/ 단수 — 디렉토리 이름 통일)
- 캡처 출력: `build_ninja/test/render_output/<scene>.png`
- 비교 도구: 1순위는 NVIDIA **FLIP**, 없으면 ImageMagick `compare -metric AE` + per-pixel tolerance.

```bash
# 도구가 있다면 (FLIP — 별도 설치)
flip --reference test/golden/<scene>.png \
     --test build_ninja/test/render_output/<scene>.png \
     --basename build_ninja/test/diff_<scene> --output-csv

# weighted median ≤ 0.05 → PASS
# weighted median ≤ 0.10 → WARNING (PR 코멘트만)
# weighted median > 0.10 → FAIL
```

FLIP 미설치 환경의 sanity check:
```bash
compare -metric AE \
    test/golden/<scene>.png \
    build_ninja/test/render_output/<scene>.png /dev/null 2>&1
```

### B.2 절대 금지

- `memcmp` 로 픽셀 비교 — 부동소수점 누적으로 거의 항상 깨짐
- 단일 글로벌 임계값 — 장면별 baseline + tolerance 를 메타데이터로 관리
- 골든 갱신 PR 을 다른 변경 PR 과 묶기 — **반드시 별도 PR**

---

## §C. 5-에이전트 운영 분리 — 권한 매트릭스 (이 프로젝트 강제 설정)

> 🔵 *원칙*(역할별 권한 분리·테스트≠코드·3+라운드·mutation blind spot·직무모사 분해 금지)은 전역 Skill **`agent-orchestration-anti-gaming`** 에 있다. 아래는 *이 프로젝트의 render-* 에이전트에 그 원칙을 강제한 구체값*.

각 에이전트의 권한은 `.claude/hooks/hooks.json` 의 PreToolUse 매처로 강제된다.

| 에이전트 | Read | Edit | Bash | Write | 비고 |
|---|---|---|---|---|---|
| **render-architect** | ✓ | ✗ | ✗ | ✗ | 코드 수정 권한 절대 없음 (분석만) |
| **render-refactorer** | ✓ | ✓ | ✓ | ✓ | git worktree 격리 필수 (`.worktrees/` 경유), `test/golden/` 쓰기 차단 |
| **render-test-debug** | ✓ | ✗ | ✓ | ✗ | 테스트 실행만, 코드 수정 안 함 |
| **render-quality-gate** | ✓ | ✗ | ✓ | ✗ | clang-tidy / FLIP / mutation 측정만 |
| **render-pm** | ✓ | ✗ | ✓ | ✗ | git merge / revert 결정만 |

> 설계 근거 상세: `doc/testplan/AGENTS_GUIDE.md`.

### 이 프로젝트의 hooks 강제 (Skill 원칙 → 구체 enforcement)
- **테스트≠코드**: `render-test-debug` 코드 변경 권한 없음 + `render-refactorer` 골든 디렉토리 쓰기 차단.
- **3+ 라운드**: PR 게이트는 No-feedback(단발+즉시 사람 개입) 또는 3+ rounds 만. 1·2 라운드 금지.
- **mutation blind spot**: `render-quality-gate` 가 활성/전체 mutant 수 + killed + 각 생존 mutant 의미 동시 보고.
- **oscillation 가드**: `render-refactorer` 가 같은 파일 3회 이상 수정 시 hooks 자동 정지 → 사람 escalate.
- **5분해 고정**: 직무 모사 7-agent 대신 운영 흐름 5-agent default (D 논문 73.0% < 77.5%).

---

## §E. 비용·시간 견적 (PR 단위)

A 논문 §6.5 데이터 기준:

- 1 spec version 컴파일: 30-60분 wall-clock + $2-3 API
- 18 successful runs 총합: $45.15
- C++ 빌드 시간을 더하면 **PR당 약 $5 / 1-2시간** (보수적 추정)

월 PR 30개 가정 → **약 $150/월 + 30-60시간의 자동화된 검증 시간**.

이 비용을 hooks 의 hard cap 으로 강제한다. 초과 시 SubagentStop 으로 종료.

---

## §F. 사용자가 직접 진화시키는 영역

이 가드레일은 **출발점**이다. 본 저장소의 컨텍스트(SuperBible 7 학습 + 직접 GL 호출 + Catch2 단위 테스트)가 다음과 같이 바뀌면 본 섹션을 직접 수정한다:

| 본 가이드 가정 | 다를 경우 수정 |
|---|---|
| 직접 GL 호출 + `SJH::Diagnostics` 래핑 | RHI 추상화 도입 시 §A.1 의 진단 규칙을 RHI 경유로 갱신 |
| GLFW invisible window 헤드리스 fixture | Mesa llvmpipe 등 도입 시 §A.3 갱신 |
| FLIP weighted median ≤ 0.05 | 장면 시각적 임계값에 맞춰 캘리브레이션 |
| PR당 $5 견적 | 실제 측정 후 hooks 의 hard cap 갱신 |
| 5-에이전트 운영 분리 | D §5 권고를 따르되, 추가 게이트 필요 시 6번째 에이전트보다 **5번째 에이전트의 도구 확장** 우선 |

**원칙**: 에이전트를 추가하기 전에 기존 5개의 도구를 강화하라. D 논문의 정량 결과가 "더 많은 에이전트가 더 안전하다" 는 것을 반증했다.
