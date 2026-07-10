# HO-4 — E1 잔여 정화 + E2/E4 검증 게이트 인프라 (핸드오프, 맥락0 자기완결)

> AI-Readiness 70% 플랜의 인프라 패키지 3건 중 2번. 정본 플랜 = [`doc/superpowers/plans/2026-07-10-ai-readiness-70pct-plan.md`](../../superpowers/plans/2026-07-10-ai-readiness-70pct-plan.md) §3 HO-4 행 + §4 "E — 검증 게이트" 절.

## ① 배경

이 저장소는 2026-07-10 AI-Readiness 감사(`doc/report/ai-readiness-score.json`)에서 18/100(AI-Hostile)을 받았다. 카테고리 **E(검증 게이트)는 5/15** — hallucinated path 14건(E1, 5점 만점에 4점), CODEOWNERS/PR template 부재(E2, 0점), agent eval 디렉터리 부재(E4, 0점)가 원인이다. HO-4는 이 세 서브항목을 동시에 메운다 — 파일이 겹치지 않아 HO-1/2/3/5 와 완전 병렬 가능.

**계측기 고정**: 재채점은 반드시 `python3 doc/report/score_cpp.py . --json <출력경로>` 로 실행한다. cartography skill 의 원본 `score.py` 사용 금지(C++ 확장자 미인식 — 비교 불가).

## ② 소유 파일 목록

- `.claude/architecture.md` (수정 — E1)
- `.claude/CLAUDE.md` (수정 — E1, 딱 1줄)
- `doc/superpowers/plans/2026-05-24-M2-prompts/README.md` (수정 — E1)
- `.github/PULL_REQUEST_TEMPLATE.md` (신규 — E2)
- `.github/CODEOWNERS` (신규 — E2)
- `evals/` 디렉터리 신규(파일명 자유, 예: `evals/agent-task-queries.md`) (신규 — E4)

다른 경로는 절대 건드리지 않는다. 특히 `scripts/audit_docs.py`(HO-1), 모듈 `CLAUDE.md` 6장(HO-2), `ARCHITECTURE.md`(HO-3), pre-commit/CI(HO-5)는 타 HO 소유.

## ③ 작업 사양

### E1-a. `.claude/architecture.md` — 예시 경로 11건 → `<placeholder>` 전환

**원리(검증됨)**: 스코어러의 `RE_PATH_REF` 정규식은 매치 시작 위치 직전 문자가 `[A-Za-z0-9_/.]` 이면 안 된다는 lookbehind를 가진다. 따라서 **경로의 첫 세그먼트(첫 `/` 앞)만 `<...>`로 감싸면 그 뒤 세그먼트에서 재매칭이 절대 발생하지 않는다** (`/` 뒤에서 시작하는 매치는 lookbehind가 차단). 즉 `vendor/header.h` → `<vendor>/header.h` 로 충분하고 `<vendor>/<header>.h` 처럼 전부 감쌀 필요 없다.

아래 11개 문자열이 **파일 전체에서 나타나는 모든 위치**(중복 라인 포함, 최소 13개 지점)에 동일 치환을 적용한다. `grep -n -F "<검색어>" .claude/architecture.md` 로 각자 위치를 먼저 재확인할 것(문서가 향후 편집으로 라인이 밀렸을 수 있음 — 아래 라인 번호는 2026-07-10 시점 참고용).

| # | 원문 문자열 | 치환 후 | 발생 라인 (2026-07-10 기준) |
|---|---|---|---|
| 1 | `module/file.h` | `<module>/file.h` | 50, 468 |
| 2 | `app/main.cpp` | `<app>/main.cpp` | 191, 327, 429(md 링크 텍스트), 443(md 링크 텍스트), 450 |
| 3 | `app/config.h` (`app/config.h.in` 링크 텍스트 안의 부분 문자열) | `<app>/config.h` (→ 결과 `<app>/config.h.in`) | 427 |
| 4 | `doc/design/2026-05-19-input-module-design.md` (md 링크 **텍스트만** — href `../doc/design/...` 는 그대로 둔다, regex 비매칭 대상이 아님) | `<doc>/design/2026-05-19-input-module-design.md` | 298 |
| 5 | `vendor/header.h` | `<vendor>/header.h` | 49 |
| 6 | `vendor/file.h` | `<vendor>/file.h` | 468 |
| 7 | `src/renderer/renderer.h` | `<src>/renderer/renderer.h` | 389 |
| 8 | `spdlog/spdlog.h` | `<vendor>/spdlog.h` | 49, 311 |
| 9 | `glad/glad.h` | `<glad>/glad.h` | 251, 468 |
| 10 | `cmake/Config.cmake` (md 링크 텍스트만, href 그대로) | `<cmake>/Config.cmake` | 435 |
| 11 | `src/context/game_action.h` | `<src>/context/game_action.h` | 299 |

주의: 라인 468 한 줄에 #1·#6·#9 세 개가 함께 들어있다 — 한 줄 안에서 3곳 모두 치환. 라인 49 한 줄에 #5·#8 두 개가 함께 들어있다. md 링크(`[텍스트](href)`)는 **텍스트 쪽만** 바꾸고 `href`(`../doc/...`, `../cmake/...`)는 그대로 둔다 — href는 애초 regex 매칭 대상이 아니었고(상대경로 `../` 접두가 lookbehind에 걸림, 실측 확인됨) 링크 자체는 유효하므로 건드릴 필요 없다.

### E1-b. `doc/superpowers/plans/2026-05-24-M2-prompts/README.md` — archival 헤더 + 잔여 2건

⚠ **발견된 간극(정직 고지)**: 플랜 원문(§4 "E — 검증 게이트")은 이 파일에 "「2026-05 시점 archival — 경로 현행 불일치 가능」" 헤더 1줄 추가만 지시한다. 그러나 헤더 문구만으로는 `score_cpp.py`의 E1 broken 카운트가 줄지 않는다(스코어러는 텍스트 의미를 이해하지 못하고 경로 존재만 본다) — green 기준 "E1 broken 0건"을 달성하려면 **이 파일의 남은 2개 경로도 architecture.md 와 동일한 `<placeholder>` 화가 필요**하다. 아래는 그 간극을 메우는 권장 조치이며, 사용자가 다른 방식(예: 경로를 실제 최신 경로로 갱신)을 원하면 그쪽을 따르되 이 판단 변경은 [제안됨]으로 커밋 메시지/코멘트에 남길 것.

1. 파일 최상단(제목 바로 아래)에 1줄 추가: `> ⚠ 2026-05 시점 archival 문서 — 경로가 현재 코드베이스와 불일치할 수 있음 (예: \`test/\` 는 2026-06 이후 구조 재편, \`InputHandler/CameraController\` 는 리팩토링으로 개명/제거됨).`
2. 57행 `test/test_uniform_atlas.cpp` → `<test>/test_uniform_atlas.cpp`
3. 58행 `apps/_MyApp_/src/InputHandler/CameraController.h/.cpp` → `<apps>/_MyApp_/src/InputHandler/CameraController.h/.cpp` (이 표기는 애초 `.h`/`.cpp` 두 파일을 축약한 저자 관용구라 존재검증 불가능한 케이스 — HO-1 의 `audit_docs.py`는 이런 패턴을 "compound, 검증불가 skip"으로 별도 분류한다)

두 항목 모두 라인 번호는 2026-07-10 실측(`sed -n '55,60p'`로 재확인 권장 — plan 문서라 안정적이지만 확인 습관 유지).

### E1-c. `.claude/CLAUDE.md` — `doc/html/index.html` → `doc/html/`

80행(2026-07-10 실측, 이후 편집으로 이동 가능 — `grep -n "doc/html" .claude/CLAUDE.md` 로 재확인) 안의 `` `doc/html/index.html` `` 를 `` `doc/html/` `` 로 바꾼다(디렉토리 표기 — Doxygen 산출물은 여러 페이지이므로 `index.html` 단일 파일 지목보다 디렉토리 지목이 더 정확하기도 하다). 확장자가 사라지므로 regex 자동 비매칭.

### E2. PR 템플릿 + CODEOWNERS

`.github/PULL_REQUEST_TEMPLATE.md` 신규 — `.claude/skills/code-design-review-lenses/SKILL.md` 의 5렌즈(① Ownership model ② Coupling ③ ODR/header hygiene ④ SOLID/separation of responsibility ⑤ Consistency)를 체크박스로, 같은 스킬의 "Self-review gate"(구조 변경 제안 시 렌즈①·④ 자기 역적용) 섹션을 별도 체크박스+자유기술란으로 반영. Summary/Test plan 섹션 포함.

`.github/CODEOWNERS` 신규 — 1인 개발 저장소이므로 `* @<github-핸들>` 한 줄. **[제안됨]** — 이 저장소의 실제 GitHub 계정 핸들을 git 커밋 이력의 author(`git log -1 --format='%an <%ae>'`)나 `git remote -v` 의 origin 조직명으로 재확인 후 정확한 핸들을 넣을 것 (임시로 로컬 git user.name "DogGuyMan" 을 참고치로만 사용하되 확정 짓지 말 것).

### E4. `evals/` — 대표 에이전트 task 쿼리 5종

`evals/agent-task-queries.md` (또는 유사 파일명, 디렉터리명 `evals/` 만 스코어러 검출 조건 — 파일명은 자유) 신규. 각 쿼리에 "쿼리 / 기대 결과(참조해야 할 문서·컨벤션) / 실패 시 흔한 오판" 3필드로 5건 작성. 예시 5종(모두 `.claude/CLAUDE.md` 실측 컨벤션에 기반 — 그대로 사용해도 되고 대체해도 됨):

1. **"SJH::render 에 새 Pass 추가"** — 기대: `src/render/` CMakeLists 확인 + Pass 컨벤션(`doc/EngineAPI.md`) 참조. 오판: Pass를 카메라에 종속시키는 것(`camera_depth_postfx_misuse` memory 참조 — Camera.Depth 오용 금지).
2. **"새 데모 타겟 활성화 (예: migrate_demo)"** — 기대: `apps/CMakeLists.txt` 의 해당 `add_subdirectory` 주석 해제(Active Target Management 컨벤션). 오판: 여러 타겟을 동시에 활성화(컨벤션 위반 — 한 번에 하나/소수만).
3. **"신규 모듈에 game_deps 의존 추가"** — 기대: `cmake/Dependency.cmake` 확인 + PUBLIC/PRIVATE 명시적 구분(`modular-build-discipline` 스킬). 오판: 전이 의존에 기대어 명시 선언 생략.
4. **"Actor 에 새 속성 추가"** — 기대: `src/scene/actor.h`/`components.h` 확인, Actor 비상속 — 특수 속성은 Component 로만(`compound_actor_pattern` memory). 오판: Actor 를 상속해 서브클래스 신설.
5. **"텍스처/머티리얼 로딩 코드 추가"** — 기대: `SJH::ResourceRegistry` 위탁, `stb_image`/`glGenTextures` 직접 호출 금지(`.claude/architecture.md §11.3`, `stb_image_owner_resource_registry` memory). 오판: 데모 `main.cpp` 안에 리소스 객체를 직접 멤버로 보유.

## ④ 수신 게이트

착수 전 아래를 직접 실행해 실측하고 "작성 시점 상태"와 대조. 불일치 시 작업 전 보고.

```bash
git log --oneline -5
git status --short | wc -l
ls scripts/audit_docs.py    # HO-1 산출물 실존 확인 — 없어도 HO-4는 독립 진행 가능(의존 아님)하나 존재 여부는 기록
grep -n "doc/html" .claude/CLAUDE.md
grep -c '<vendor>\|<module>\|<app>\|<glad>\|<cmake>\|<doc>\|<src>' .claude/architecture.md   # 0이어야 아직 미착수 상태
```

**작성 시점 상태 (2026-07-10 실측)**
- 브랜치: `refactor/pcb-to-worldscene`, HEAD: `5590cc9` (`5590cc902d4774964c09d56bfa7ca5df81ced44d`, "[chore] : 주석 수정")
- `git status --short` 474줄(대부분 staged — `doc/` 재편 + `.claude/` 신설), untracked 11건. 매우 dirty — 사용자 병렬 작업 가능성(memory: `user-parallel-git-and-builds`). 파셜 커밋 필수(⑥).
- `.claude/architecture.md` 698줄, 11개 예시 경로 미치환 확인 완료.
- `.github/PULL_REQUEST_TEMPLATE.md`, `.github/CODEOWNERS`, `evals/` 모두 미존재 확인 완료.
- `scripts/audit_docs.py` 미존재(2026-07-10 시점) — HO-1과 병렬 진행 중이면 생겼을 수 있음, 있으면 그대로 두고 참고만.

## ⑤ green 체크포인트

```bash
python3 doc/report/score_cpp.py . --json /tmp/score_after_ho4.json
python3 -c "import json; d=json.load(open('/tmp/score_after_ho4.json')); print(d['categories']['E'])"
```

**통과 기준**: `categories.E.evidence.ref_broken == 0` (E1 broken 0건, 필수). `categories.E.score >= 10.5`(추정 도달치 ≈12 — E1=5 + E2=4[codeowners+pr_template] + E3=1[불변] + E4=2[evals] 합산, 실측으로 재확인). `evidence.codeowners == true`, `evidence.pr_template == true`, `evidence.evals_dir == true` 모두 확인.

## ⑥ 금지 사항

- 소유 파일 6개 외 어떤 파일도 수정하지 않는다. 특히 `scripts/audit_docs.py`(HO-1), 모듈 CLAUDE.md 6장(HO-2), `ARCHITECTURE.md`/mermaid(HO-3), `.git/hooks`·`.github/workflows/*`(HO-5) 불가침.
- `.claude/architecture.md` 안에서 이번 스코프 밖의 내용(예: `SJH::context` 모듈이 이미 폐기됐음에도 활성처럼 서술된 부분)을 발견해도 **수정하지 말고 보고만** — E1 플레이스홀더화 외 내용 수정은 이 HO의 범위가 아니다.
- 커밋은 `git commit .claude/architecture.md .claude/CLAUDE.md "doc/superpowers/plans/2026-05-24-M2-prompts/README.md" .github/PULL_REQUEST_TEMPLATE.md .github/CODEOWNERS evals/` 처럼 **소유 경로만 파셜 커밋**. `git add -A` 금지.
- CODEOWNERS 의 GitHub 핸들을 확인 없이 확정 기록하지 않는다 — `[제안됨]` 태그 유지, 사용자 확인 후 확정.
- 테스트를 임의로 새로 추가하지 않는다(`no_auto_tests`).
- placeholder 화 결과가 실제로 regex 비매칭인지 스스로 검증 없이 "완료"로 보고하지 않는다 — 가능하면 HO-1 완료분(`scripts/audit_docs.py`) 또는 위 ⑤ 명령으로 직접 확인 후 보고.
