# AI-Readiness 7카테고리 70% 달성 플랜 [승인됨 2026-07-10 — D-1 압축+보존강등 · D-2 작업패키지 7분할 사용자 확정]

> ⚠ 2026-07 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 목표: `doc/report/ai-readiness-score.json` 기준 18/100 (AI-Hostile) → **전 카테고리 ≥70%** (총점 환산 ~75+/100, AI-Ready 밴드).
> 계측기: **`doc/report/score_cpp.py` 고정** (C++ 적응 사본 — 원본 score.py로 재채점 금지, 비교 불가).
> 실행 형태: 단일 오케스트레이터가 핸드오프 7건(★)을 제작·디스패치. 각 핸드오프는 맥락0 자기완결 + green 체크포인트(카테고리 점수 delta) 보유.
> 제외 확정: godot_engine_reference 복구 안 함 (사용자 결정 2026-07-10) — 관련 깨진 링크는 "(원본 삭제됨)" 주석 처리만.

---

## 1. 현황 → 목표 매트릭스

| Cat | 현재 | 70% 목표 | 도달 수단 (요약) | 예상 도달 | 리스크 |
|---|---|---|---|---|---|
| A 내비게이션 | 0/15 | 10.5 | 모듈 CLAUDE.md 6장 (coverage 6/6 → 15) | **15** ✅ | 낮음 |
| B 컨텍스트 품질 | 9/20 | 14 | 신규 6장에 B2~B5 요소 내장 + **루트 CLAUDE.md 압축(D-1 결정 필요)** | 14~18 | 중 — D-1 미승인 시 B1이 낮아 14 경계 |
| C 암묵지 | 0/20 | 14 | 모듈 파일을 Five-Question 구조로 작성 + 결정 스토어 표준명 노출 | 14~18 | 낮음 (A와 동일 파일) |
| D 의존 매핑 | 2/15 | 10.5 | 루트 ARCHITECTURE.md + mermaid 모듈 그래프 + 모듈별 `## Cross-module deps` 섹션 | 10~15 | 중 — 10=67% 밴드 엣지. "What depends on X" 인덱스 표까지 넣어 15 밴드 정당화 |
| E 검증 게이트 | 5/15 | 10.5 | E1 잔여 14건 0화 + PR template/CODEOWNERS(E2) + evals/(E4). E3=4 기확보 | 13~15 | 낮음 |
| F 신선도 | 2/10 | 7 | audit_docs.py(P9) + pre-commit 훅 + CI docs-validation step | 6~9 | 중 — 훅+CI 둘 다 인식돼야 7+ |
| G 성과 계측 | 0/5 | 3.5 | evals/ + rollback-log.jsonl 시드 13건 + stats 스크립트(P11) | **3** ⚠ | **높음 — 정직 고지: 밴드가 0/2/3/5라 70%(3.5)는 이번 사이클 도달 불가. 3(측정 도입)까지가 인프라로 가능, 5는 before/after 종단 데이터 축적 필요 (분기 후 재평가)** |

---

## 2. 공정 흐름과 의존성 (선행/병렬 분석)

### 의존성 DAG (텍스트)

```
[Wave 0 — 선행 기반, 병렬 2]
 T0a. 모듈 CLAUDE.md 표준 템플릿 확정  ──┐  (A·B·C 3카테고리의 공통 선행 — 이것 없이 6장 병렬 착수 금지)
 T0b. 결정 스토어 표준명 확정(C-Q5)     ──┤  (템플릿의 See-also 링크 대상이므로 T0a와 한 몸)
 T0c. scripts/audit_docs.py (P9)      ──┘  (T0a와 병렬 가능 — E1 재발 방지 + F 훅의 엔진 + 모든 신규 문서의 수용 게이트)

[Wave 1 — 대량 병렬, T0 완료 후]
 W1-1~6. 모듈 CLAUDE.md 6장 (src/, apps/_MyApp_/, test/, cmake/, scripts/, vcpkg-overlay-ports/)
          → 파일 독립이라 완전 병렬. 단 전부 T0a 템플릿 + T0c 검증 통과 의무.
 W1-D.  ARCHITECTURE.md + mermaid 그래프 (독립 — 기존 doxygen 모듈 그래프·doc/pages 재활용.
          모듈 파일의 deps 섹션과 내용 정합 필요 → 템플릿의 deps 섹션 규약만 공유하면 병렬 OK)
 W1-E1. architecture.md 예시 경로 플레이스홀더화(`<app>/main.cpp`→`<app>/main.cpp` 류 — 문서 품질도 개선)
          + archival 플랜 문서 시점 헤더 + CLAUDE.md `doc/html/` 무확장 표기 (독립)
 W1-E2. .github/PULL_REQUEST_TEMPLATE.md(5렌즈 리뷰 체크리스트) + CODEOWNERS (독립, S)

[Wave 2 — Wave 0/1 산출물 의존]
 W2-F.  pre-commit 훅 + CI docs-validation step  ← T0c 의존
 W2-G.  evals/ 대표 task 세트 + rollback-log.jsonl 시드 13건 + rollback_stats.py  ← W1-1~6 약의존(대표 task가 모듈 컨텍스트 전제)
 W2-B.  루트 CLAUDE.md 압축(compass화)  ← W1-1~6 + W1-D 의존(내용 이관처가 먼저 존재해야) + **D-1 사용자 승인 필요**

[Wave 3 — 수렴]
 W3.  score_cpp.py 재채점 → 미달 카테고리 보정 1회전 → 대시보드 갱신 → 최종 보고
```

### 병렬성 요약
- **최대 병렬 폭 = Wave 1의 9개 작업** (모듈 6 + D + E1 + E2). 전부 소유 파일이 겹치지 않음.
- **직렬 강제 구간 = T0a→Wave1, W1→W2-B**. T0a(템플릿)를 건너뛰고 6장을 병렬 착수하면 구조 불일치로 재작업 — 이 플랜의 최대 함정.
- W2 3건은 상호 병렬.

---

## 3. 핸드오프 구획 (★ = 핸드오프 경계 / 각각 맥락0 자기완결 문서로 제작)

⚠ **구획 방식 결정 필요 (D-2)**: 사용자 요청은 "카테고리별 7건"이나, A·B·C는 동일 파일(모듈 CLAUDE.md 6장)을 공유해 **카테고리별 분할 시 병렬 에이전트 간 파일 충돌**이 발생한다. 권장은 아래 **작업패키지 7분할** (카테고리 커버리지는 매핑으로 보존, 겹치는 파일 소유는 1핸드오프 1소유):

| ★ | 핸드오프 | 커버 카테고리 | 선행 | 병렬 가능 대상 | green 체크포인트 |
|---|---|---|---|---|---|
| ★HO-0 | 템플릿+스토어 표준 (T0a+T0b) — 오케스트레이터 직접 또는 단일 에이전트 | (A·B·C 기반) | 없음 | HO-1 | 템플릿 1장 사용자 승인 |
| ★HO-1 | audit_docs.py (T0c) | E1·F 기반 | 없음 | HO-0 | 스크립트가 기존 문서에서 broken 14건 재현 검출 |
| ★HO-2 | 모듈 컨텍스트 팩토리 (W1-1~6) | **A + B(부분) + C(부분)** | HO-0, HO-1 | HO-3~5와 병렬 | 재채점 A≥10.5 + 신규 6장 audit 통과 0건 |
| ★HO-3 | 의존 지도 (W1-D) | D | HO-0(deps 규약만) | HO-2·4·5 | 재채점 D≥10.5 |
| ★HO-4 | E1 잔여 정화 + E2 인프라 (W1-E1+E2) | E | 없음 | HO-2·3·5 | 재채점 E1 broken 0건 · E≥10.5 |
| ★HO-5 | 신선도 자동화 (W2-F) | F | HO-1 | HO-2·3·4 | pre-commit 훅 실동작 + 재채점 F≥6 |
| ★HO-6 | 계측 인프라 (W2-G) + 루트 압축 (W2-B, D-1 승인 시) | G + B(잔여) | HO-2 완료 | 단독 | 재채점 G=3 · B≥14 |
| — | Wave 3 수렴 (재채점·보정·대시보드·보고) | 전체 | HO-2~6 | — | 전 카테고리 ≥70% (G는 3/5 예외 명시) |

각 핸드오프 문서 필수 요소 (lossless-handoff 규율 + 이번 개선안 P10 수신 게이트 선반영):
① 맥락0 배경 1문단 + 계측기 고정 명시(`score_cpp.py`) ② 소유 파일 목록(다른 HO와 배타) ③ 작업 사양(템플릿/예시 포함) ④ 수신 게이트(git status 실측 + audit_docs.py 실행) ⑤ green 체크포인트 = 재채점 명령과 통과 기준 ⑥ 금지 사항(다른 HO 소유 파일 불가침, 원본 score.py 사용 금지, [제안됨] 태그 없이 확정 기록 금지).

---

## 4. 카테고리별 작업 상세 (핸드오프 본문의 뼈대)

### A+B+C — 모듈 CLAUDE.md 6장 (HO-0·2)
- 템플릿 = **Five-Question 구조를 섹션으로 직접 사용** (C 자동채점 마커와 1:1 정합):
  `## Purpose (owns/configures)` / `## Quick commands`(bash fence — B2) / `## Key files`(3-5개 실경로 — B3) / `## Gotchas`(Why:/주의 마커 — B4·C-Q3) / `## Cross-module deps`(D 연동) / `## See also`(상대링크 — B5 + 결정 스토어 링크 — C-Q5). 각 장 30~80줄 (B1 밴드).
- src/는 17 하위 모듈 1행 표 + 모듈별 상세는 기존 `.claude/architecture.md`·`doc/EngineAPI.md` 링크로 위임 (중복 서술 금지 — 기존 정본 존중).
- C-Q5: `doc/adr/README.md` 1장 — "이 레포의 결정 스토어 = `doc/superpowers/specs/`(LOCKED D# 관행) + 세션 메모리" 포인터 문서 (신규 체계 발명 금지, 기존 관행의 표준명 노출만).

### D — 의존 지도 (HO-3)
- 루트 `ARCHITECTURE.md` (또는 `<doc>/architecture.md` — 스코어러는 양쪽 다 인식, 루트 권장): 17 모듈 mermaid 그래프(기존 doxygen `20-dependencies.md` 이식) + **"What depends on X?" 역인덱스 표** + project_deps/game_deps 계층 + diagnostics cycle-exempt 예외 명시. `.claude/architecture.md`와 역할 분리: 루트=지도(what), .claude=규율(why/how).

### E — 검증 게이트 (HO-4)
- E1: `.claude/architecture.md` 예시 경로 11건 → `<placeholder>` 표기 전환(regex 비매칭 + 가독성 동반 개선), archival 플랜 README 헤더에 「2026-05 시점 문서 — 경로 현행 불일치 가능」, CLAUDE.md `<doc>/html/index.html` → `doc/html/`(디렉토리 표기).
- E2: `.github/PULL_REQUEST_TEMPLATE.md` — code-design-review-lenses 5렌즈 + Self-review gate 체크박스. `.github/CODEOWNERS` 1줄.
- E4: `evals/` — 대표 에이전트 task 쿼리 5종(예: "SJH::render에 패스 추가", "새 데모 타겟 활성화") + 기대 결과. HO-6의 G 인프라와 디렉토리 공유.

### F — 신선도 (HO-1·5)
- `scripts/audit_docs.py`: 개선안 P9 사양 그대로 (E1 알고리즘 C++ 적응 + drift 검출 + SUPERSEDED 후보 제시만 + Auto/Heuristic 태그 + 3종 출력). `doc/report/score_cpp.py`의 검증 로직 재사용 가능.
- pre-commit 훅(.git/hooks 또는 .claude/hooks 연동) + `.github/workflows/`에 docs-validation step 추가.

### G — 계측 (HO-6)
- `doc/번복기록/rollback-log.jsonl` 시드 13건(B §2 11건 + A 2건 — 트리거/턴수는 推 표기), `scripts/rollback_stats.py`, `evals/results/` 초기 측정 1회. **G=3 도달이 이번 사이클 상한임을 핸드오프에 명시** — 5는 분기 축적 후.

---

## 5. 결정 — [확정됨 2026-07-10 사용자 승인]

- **D-1. 루트 CLAUDE.md 압축 = 승인 (압축 + 보존 강등)**: ~80줄 나침반으로 압축, 기존 전문은 `doc/CLAUDE-extended.md`로 이동 보존 (정보 무손실). HO-6 소관.
- **D-2. 핸드오프 구획 = 작업패키지 7분할**: §3 표 그대로 — 파일 소유 배타, 카테고리 커버리지는 매핑으로 보존.
- (선행 확정) godot_engine_reference 복구 안 함 / 계측기 = `doc/report/score_cpp.py` 고정.

## 6. 리스크 로그
1. G 70% 구조적 불가(밴드 0/2/3/5) — 3으로 마감하고 예외 명시. 2. D·F 밴드 엣지 — Wave 3 보정 1회전으로 커버. 3. 병렬 에이전트의 세션 한도(금일 2회 발생) — 핸드오프가 자기완결이므로 중단 시 해당 HO만 재개. 4. 사용자 병렬 git 작업과의 충돌 — 각 HO는 partial commit(`git commit <소유 경로>`)만.
