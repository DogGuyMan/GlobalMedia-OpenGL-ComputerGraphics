# METRICS — 게이트 정의와 임계값

> 본 PoC가 사용하는 모든 정량 임계값을 한 곳에 모은다. CLAUDE.md, agents, hooks 어디서든 임계값이 필요하면 본 문서를 단일 진실의 원천으로 한다.
>
> Phase 1 Day 3-4의 캘리브레이션 후 사용자 환경에 맞춰 직접 수정한다.

---

## 1. 골든 이미지 비교

### NVIDIA FLIP Weighted Median

| 값 | 판정 | Phase 1 게이트 동작 |
|---|---|---|
| ≤ 0.05 | PASS | 자동 머지 가능 |
| 0.05 < x ≤ 0.10 | WARNING | PR 코멘트 + 머지 가능 |
| > 0.10 | FAIL | 즉시 회귀로 보고, REVERT |

**근거**: NVIDIA FLIP 공식 문서의 기본 권장 + 보수적 조정. 게임의 시각 특성에 따라 캘리브레이션 필요.

### ImageMagick Pixel Diff (sanity)

| AE count | 판정 |
|---|---|
| < 1,000 | OK |
| 1,000 - 100,000 | OK + 사람 모니터링 |
| ≥ 100,000 | NEEDS_REVIEW (FLIP 결과 무관하게 escalate) |

**근거**: FLIP의 perceptual 모델이 큰 비-perceptual 차이(예: alpha 채널 통째로 0)를 놓칠 수 있는 케이스 보완.

---

## 2. Mutation Testing

### Mutation Score (살아남은 mutant 비율의 역수)

| MS | 판정 | 동작 |
|---|---|---|
| ≥ 60% | PASS | 머지 허용 |
| 40% ≤ MS < 60% | WARNING | 머지 허용 + 향후 테스트 추가 권고 |
| < 40% | FAIL | HOLD, 테스트 보강 필요 |

**근거**: A 논문의 SpecSuite-Core가 86-100% MS를 달성. 60%는 보수적 시작점. 신규 모듈은 80%+ 목표, 레거시는 점진 향상.

### A 논문 §8 caveat 준수

다음을 항상 함께 보고 (MS만 보고 안심 금지):
- 활성화된 mutant / 전체 mutant 비율 (A 논문에서 87%)
- non-activating mutant 수 (제외된 비율이 클수록 MS 신뢰도 낮음)
- 살아남은 mutant 각각의 의미 (어떤 blind spot인가)

---

## 3. 정적 분석

### clang-tidy

| 카테고리 | 임계값 | 동작 |
|---|---|---|
| `bugprone-*` | 0 errors | FAIL 시 머지 차단 |
| `cppcoreguidelines-*` | warnings 허용 | PR 코멘트만 |
| `performance-*` | warnings 허용 | PR 코멘트만 |
| `readability-*` | warnings 허용 | PR 코멘트만 |

**근거**: bugprone은 실제 버그 가능성. 나머지는 코딩 스타일이라 점진 개선.

### cppcheck

`--enable=warning,style --error-exitcode=1` — error 발견 시 FAIL.

---

## 4. 빌드·테스트

| 게이트 | 임계값 |
|---|---|
| `cmake --build` | exit code 0 |
| `ctest` | 100% PASS |

타협 없음. 빌드 또는 테스트 실패 = 즉시 REVERT.

---

## 5. iteration budget (A 논문 Algorithm 1)

| 값 | 의미 |
|---|---|
| Outer iterations max | **6** |
| Inner iterations max (FOCUSEDLOOP) | **8** |
| FOCUSEDLOOP 진입 임계값 (failures) | **< 10** |
| 같은 파일 수정 횟수 한도 | **3** (oscillation 감지) |

**근거**: A 논문 §6.1의 budget 정의. 평균 2-4 iter에서 수렴 (A §6 Discussion).

---

## 6. 비용 캡

| 단위 | hard cap | 동작 |
|---|---|---|
| PR 1개 | **$10** | 초과 시 SubagentStop |
| 일일 누적 | $50 | warning |
| 월 누적 | $500 | escalate |

**근거**: A 논문 §6.5의 $2-3/run 데이터에 C++ 빌드 시간 + 안전 마진 추가.

---

## 7. 결정성 검증

| 검증 | 임계값 |
|---|---|
| 골든 이미지 5회 캡처 | FLIP wm = 0.0 (정확히 일치) |
| 시드 시나리오 frame index | 명시적 매개변수 (시간 의존성 금지) |
| RNG seed | 고정 값 (예: 42) |

**근거**: CLAUDE.md §A.2.

---

## 8. Phase 1 완료 게이트 (Phase 2 진입 조건)

| 조건 | 측정 방법 |
|---|---|
| 최소 3개 리팩토링 PASS | git log + PR 통계 |
| FLIP 임계값 캘리브레이션 완료 | 본 문서 §1의 값이 실측에 맞춰 조정됨 |
| Mutation Score ≥ 60% 경험 | 최소 1개 PR에서 달성 |
| iteration budget 소진 비율 | < 30% |
| AGENTS_GUIDE §3 6가지 질문 답변 | 작성 완료 |

---

## 사용자가 캘리브레이션할 때

본 문서의 임계값은 **출발점**이다. Phase 1 Day 3-4 후 다음과 같이 조정:

1. **FLIP weighted median 0.05가 너무 엄격하면**
   - 0.08 또는 0.10으로 완화
   - 단, "왜 완화하는가"를 git commit message에 명시
   - 게임의 동적 효과(SSR, GI 등)가 많을 때 자연스러움

2. **Mutation Score 60%가 도달 불가능하면**
   - 30% → 45% → 60%의 점진 상향 일정 수립
   - 신규 모듈만 60% 강제, 레거시는 단계적

3. **PR당 $10 cap이 자주 초과되면**
   - iteration budget 축소 (6 → 4)
   - 또는 리팩토링 단위를 더 작게 분해

조정 시 본 문서를 직접 수정 + git commit으로 변경 이력 추적.
