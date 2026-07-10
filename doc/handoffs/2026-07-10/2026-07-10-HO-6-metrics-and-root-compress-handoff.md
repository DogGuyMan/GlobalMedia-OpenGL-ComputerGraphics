# HO-6 — 계측 인프라(G) + 루트 CLAUDE.md 압축(D-1) (핸드오프, 맥락0 자기완결)

> AI-Readiness 70% 플랜의 콘텐츠 패키지 3건 중 3번(**단독 실행 — 병렬 파트너 없음, 선행 = HO-2 완료 필수**). 정본 플랜 = [`doc/superpowers/plans/2026-07-10-ai-readiness-70pct-plan.md`](../../superpowers/plans/2026-07-10-ai-readiness-70pct-plan.md) §3 HO-6 행 + §4 "G — 계측" 절 + §5 D-1 결정. 이 문서만 읽고 실행 가능.

## ① 배경

이 저장소는 2026-07-10 감사(`doc/report/ai-readiness-score.json`)에서 카테고리 G(성과 계측) **0/5**, B(컨텍스트 품질) **9/20**를 받았다. G는 `evals/`류 디렉토리·결과 파일·telemetry 단서가 전무해서 0점(evidence: `eval_dirs: [], metric_files: [], telemetry_hint: false`). B는 `.claude/CLAUDE.md`(281줄)·`.claude/architecture.md`(698줄)가 conciseness 상한(100줄)을 크게 넘겨 B1이 4점 만점에 2점으로 깎였다(evidence: `"conciseness 초과(>100 lines) context 2건"`). HO-6 은 (a) 번복 로그 + 통계 스크립트 + evals 초기 측정으로 G를 0→3으로, (b) 사용자 승인된 D-1(플랜 §5, "[확정됨 2026-07-10]")에 따라 루트 `.claude/CLAUDE.md`를 ~80줄 나침반으로 압축하고 전문을 `doc/CLAUDE-extended.md`에 무손실 보존해 B를 14+로 올린다.

**계측기 고정**: 재채점은 반드시 `python3 doc/report/score_cpp.py . --json <출력경로>`. 원본 `score.py` 사용 금지.

**⚠ G 밴드 상한 정직 고지 (플랜 §1 리스크 로그 인용)**: 이 사이클의 G 목표는 **3**이지 5가 아니다 — "5"는 before/after 종단 데이터 축적 후에나 정당화된다. `doc/report/score_cpp.py:598-616`을 보면 `eval_dirs` 존재만으로 +3, `metric_files`(정확한 파일명 `**/agent-results.json`·`**/.skill-eval.json`)로 +1, telemetry 언급으로 +1 — 이 HO는 **+3(eval_dirs)만 목표로 하고 나머지 두 보너스를 인위적으로 채우려 시도하지 않는다**(정직성 — 근거 없이 "성과 측정됨"이라 과대주장 금지).

## ② 소유 파일 목록

- `doc/번복기록/rollback-log.jsonl` (신규)
- `scripts/rollback_stats.py` (신규)
- `evals/results/` 디렉토리 + 그 안의 초기 측정 산출물 1개 (신규 — **`evals/` 최상위 자체와 대표 task 쿼리 5종은 HO-4 소유. `evals/`가 이미 있으면 `results/` 하위만 추가하고 다른 파일은 절대 건드리지 않는다. 없으면 `evals/results/`만 최소 생성한다 — HO-4 몫의 상위 파일을 대신 만들지 않는다.**)
- `.claude/CLAUDE.md` (기존 파일 **압축 수정** — 281줄 → ~80줄)
- `doc/CLAUDE-extended.md` (신규 — 이관된 전문 보존)

읽기 전용 참조(수정 금지): `doc/번복기록/2026-07-05-번복기록-분석보고서.md`, `doc/번복기록/AI설계판단오류-분석보고서.md`, HO-2가 만든 6개 모듈 `CLAUDE.md`(이관 목적지 확인용), `ARCHITECTURE.md`(HO-3 산출물, 있으면 참조).

## ③ 작업 사양

### (a) `doc/번복기록/rollback-log.jsonl` — 시드 13건

원천 = `doc/번복기록/2026-07-05-번복기록-분석보고서.md`의 **"## 2. 사용자의 진단이 옳았던 순간 — 전수 카탈로그 (11건)"** 표(11행, 열: `#`/`진단`/`문서`/`AI가 놓친 것`/`결과`/`심각도`) + `doc/번복기록/AI설계판단오류-분석보고서.md`의 **"## 2. 사용자의 반박 지점과 그 근거"** 절(2건: "반박 ① Model에 DrawOutline은 원칙 위배다", "반박 ② 왜 Model이 ITechnique을 소유하지"). 두 표 모두 열려 있으니 직접 읽고 옮길 것 — 아래는 스키마 + 워크된 예시 2건(그대로 형식 참고, 값은 원본 표 그대로).

**JSONL 스키마 (한 줄에 객체 하나)**:
```json
{"id": "R-01", "date": "2026-07-05", "date_confidence": "推(보고서 발행일 — 실제 사건일 아님)", "source_doc": "doc/번복기록/2026-07-05-번복기록-분석보고서.md", "source_ref": "[IR:1-13]", "trigger_type": "T1", "trigger_type_confidence": "推", "diagnosis": "능력 인터페이스 다중구현 = 다이아몬드 상속(자기 설계 자가진단)", "ai_missed": "애초에 그 설계가 논의 선상에 오르도록 방치", "result": "IPassable 하나 + 합성 멤버로 전환", "severity": "상", "turns_estimate": null, "turns_confidence": "推(원본에 턴수 기록 없음 — 대화록 회귀 조사 없이는 정확한 턴수 산출 불가, null로 남기고 이유 명시)"}
```

**트리거 4유형 매핑 기준** (원본 §1 정의 그대로, `trigger_type_confidence: "推"`로 표기 — 원본 표에 유형 라벨이 직접 붙어있지 않아 이 HO가 매핑 추정):
- T1 = 계획에 없던 구조물 등장 / T2 = 근거 없는 추천 / T3 = lock된 결정 재요동 / T4 = 코드베이스 사실 단정 오류

**필수 준수**: `severity`(상/중/하)와 `result`/`diagnosis`는 원본 표 값 **그대로 인용**(추정 아님, confidence 필드 불필요). `turns_estimate`는 원본에 없으므로 대화록(`[IR:...]`/`[GP:...]`/`[FA:...]`/`[번복:...]` 참조 태그의 대략적 줄 스팬)으로 개략 추정하거나, 근거 없이 숫자를 지어낼 바엔 `null` + confidence 사유 기재를 우선한다. 13행 전부 채운 뒤 `jsonlint` 또는 `python3 -c "import json;[json.loads(l) for l in open('doc/번복기록/rollback-log.jsonl')]"`로 파싱 검증.

### (b) `scripts/rollback_stats.py` (~80줄, pure stdlib, 한국어 docstring, `scripts/find_naming_violations.py` 관례 따름 — 신뢰도 등급 표기 + "코드 수정 안 함" 선언)

- 입력: `doc/번복기록/rollback-log.jsonl` (인자로 override 가능, default 이 경로)
- 집계: trigger_type 별 건수 / `turns_estimate` 평균(null 제외, 표본 수 명시) / 전체 대비 비율("번복률" = 해당 유형 건수 / 13, **세션 전체 대비 진짜 base rate 아님을 주석·출력 모두에 명시** — 아직 전체 세션 수를 추적하지 않으므로 proxy 지표임을 숨기지 않는다) / severity 분포.
- 출력: `--json <path>` 구조화 + 기본 콘솔 사람이 읽는 요약.
- `main()`: `argparse`, 위치 인자 없음, `--log`(default `doc/번복기록/rollback-log.jsonl`), `--json`.

### (c) `evals/results/` 초기 측정 1회

```bash
mkdir -p evals/results
python3 scripts/rollback_stats.py --json evals/results/2026-07-10-rollback-stats.json
```
이 JSON 파일 자체가 "초기 측정 1회" 산출물이다. 파일명을 `agent-results.json`/`.skill-eval.json`으로 짓지 않는다(§① 인용 — metric_files 보너스를 노려 G=3 캡을 넘기려 하지 않는다, 정직성 우선).

### (d) 루트 `.claude/CLAUDE.md` 압축 (D-1)

**순서 고정 — 반드시 이 순서**:
1. **먼저** 현재 `.claude/CLAUDE.md`(281줄) 전문을 **그대로** `doc/CLAUDE-extended.md`에 복사하고 맨 위에 헤더 추가: `# CLAUDE.md — 확장 전문 (2026-07-10 D-1 압축 이관 보존본)` + 1줄 설명("`.claude/CLAUDE.md`가 ~80줄 나침반으로 압축되며 이관된 상세 전문. 최신 나침반은 `.claude/CLAUDE.md`, 모듈별 상세는 `src/CLAUDE.md` 등 6개 모듈 CLAUDE.md + `ARCHITECTURE.md` + `doc/EngineAPI.md` 참조."). 이 단계가 무손실을 보장한다 — 이후 압축 단계에서 무엇을 빼도 여기 원문이 남는다.
2. **그다음** `.claude/CLAUDE.md`를 ~80줄로 재작성. 아래 골격을 따르되 각 항목은 1~3줄로 압축(전부 필수 — B2~B5 자동채점 마커 유지 목적):
   - 1줄: 프로젝트 정체성(C++17/OpenGL, Core/Client 분리, vcpkg manifest)
   - `## Quick commands` (bash 펜스): configure + `_MyApp_` 빌드/실행 + 테스트(`ENABLE_TESTING=ON` + ctest) — 나머지 셸 스크립트/프리셋 상세는 생략, "전체 목록은 아래 라우팅 표" 로 위임
   - Active Target 규칙 1문단(`apps/CMakeLists.txt` 주석 컨벤션 — CRITICAL 유지)
   - `## Key files`: `apps/_MyApp_/CLAUDE.md`, `src/CLAUDE.md`, `ARCHITECTURE.md`, `doc/EngineAPI.md`, `doc/adr/README.md` (5개, HO-2/HO-3 산출물 실존 확인 후 링크 — 아래 게이트 참조)
   - `## Gotchas` (`Why:`/`주의` 마커 유지): 헤더가드 `__CHAPTER_N_ENTRY_H__`(`#pragma once` 미사용) / stb_image 단일 owner / ESC sb7 하드와이어드 종료 / 한국어 주석 컨벤션
   - `## Cross-module deps` 또는 라우팅 섹션: "모듈별 상세 = 각 `<module>/CLAUDE.md`, 의존 그래프 = `ARCHITECTURE.md`, 결정 스토어 = `doc/adr/README.md`, 확장 전문 = `doc/CLAUDE-extended.md`"
   - `## See also`: 전역 Skill 9종 나열은 **1줄 요약 + `doc/CLAUDE-extended.md` 링크**로 축약(현재처럼 9개 전부 설명 나열 금지 — 나침반이 아니라 백과사전이 되는 원인).
3. 압축 후 `diff <(git show HEAD:.claude/CLAUDE.md) doc/CLAUDE-extended.md`로 이관본이 원본과 내용상 동일(헤더 추가분 제외)한지 확인.

## ④ 수신 게이트

```bash
git log --oneline -5
git status --short | wc -l
ls src/CLAUDE.md apps/_MyApp_/CLAUDE.md test/CLAUDE.md cmake/CLAUDE.md scripts/CLAUDE.md vcpkg-overlay-ports/CLAUDE.md 2>&1
# 위 6개 전부 존재해야 함(HO-2 선행 완료 조건) — 하나라도 없으면 착수 보류하고 오케스트레이터에 "HO-2 미완료" 보고
ls ARCHITECTURE.md doc/adr/README.md 2>&1   # HO-3/HO-2 산출물 — 없으면 (d) 3단계의 라우팅 링크에서 해당 항목만 "(예정)"으로 표기하고 계속 진행 가능(엄격 선행 아님)
ls doc/번복기록/rollback-log.jsonl scripts/rollback_stats.py evals/results 2026-07-10-rollback-stats.json 2>&1
ls scripts/audit_docs.py 2>&1   # HO-1 산출물 — 없어도 진행 가능
```

**작성 시점 상태 (2026-07-10 실측)**
- 브랜치: `refactor/pcb-to-worldscene`
- HEAD: `5590cc9` (`5590cc902d4774964c09d56bfa7ca5df81ced44d`, "[chore] : 주석 수정")
- `git status --short` 476줄(대부분 staged), untracked 13건. dirty — 파셜 커밋 필수.
- `.claude/CLAUDE.md` 281줄(2026-07-10 `wc -l` 실측), `doc/CLAUDE-extended.md`/`doc/번복기록/rollback-log.jsonl`/`scripts/rollback_stats.py`/`evals/` 전부 미존재 확인 완료.
- HO-2 산출물 6개는 **이 핸드오프 작성 시점에는 아직 없음**(병렬 디스패치 예정) — 착수 전 재확인 필수.

## ⑤ green 체크포인트

```bash
python3 doc/report/score_cpp.py . --json /tmp/rescan_ho6.json
python3 -c "
import json
d = json.load(open('/tmp/rescan_ho6.json'))
print('G =', d['categories']['G']['score'], d['categories']['G']['evidence'])
print('B =', d['categories']['B']['score'], d['categories']['B']['sub_scores'])
"
```

**통과 기준**: `G == 3`(evidence `eval_dirs: ["evals"]`, `metric_files: []` 허용 — +1/+1 보너스 없이도 3 달성이 정상) · `B ≥ 14`. B가 14 미만이면 원인은 대개 B1(conciseness) — `.claude/CLAUDE.md`가 여전히 80줄을 크게 넘겼는지, 또는 HO-2의 6개 모듈 파일이 아직 없어(병렬 미완료) 평균이 안 오른 것인지 구분해서 보고할 것.

## ⑥ 금지 사항

- ②의 5개 소유 파일/디렉토리 외 어떤 것도 수정하지 않는다. 특히 `evals/`의 HO-4 소유 상위 파일(대표 task 쿼리 5종), `src/CLAUDE.md` 등 HO-2 6종, `ARCHITECTURE.md`(HO-3)는 불가침 — 링크만 건다.
- 커밋은 `git commit doc/번복기록/rollback-log.jsonl scripts/rollback_stats.py evals/results .claude/CLAUDE.md doc/CLAUDE-extended.md` 처럼 **소유 경로만 파셜 커밋**. `git add -A` 금지.
- `.claude/CLAUDE.md` 압축 시 **내용을 삭제하지 않는다** — 반드시 (d) 1단계로 `doc/CLAUDE-extended.md`에 먼저 무손실 복사한 뒤에만 원본을 줄인다. 순서를 바꾸면 정보 유실.
- G 점수를 인위적으로 5까지 올리려 `metric_files`/telemetry 문구를 근거 없이 추가하지 않는다(①의 정직 고지 참조).
- `turns_estimate`를 확신 없이 구체적 숫자로 지어내지 않는다 — 모르면 `null` + 사유.
- 자동 테스트를 임의로 추가하지 않는다(`no_auto_tests` 관례).
- 소유 파일 밖에서 발견한 문제(예: 다른 HO 산출물의 오류)는 수정하지 말고 오케스트레이터에 보고만 한다.
