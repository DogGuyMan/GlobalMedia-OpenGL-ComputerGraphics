# HANDOFF -- 성능 회귀 시스템 분류 (orphan/runaway 프로세스 + 시스템 데몬 부하)

> **수신자:** 신규 Claude Code 세션/에이전트 (이 대화의 컨텍스트 없음 가정).
> **작성:** 2026-06-21 · **상태:** 본 세션 case study (Slang Phase 2 T5 검증) 기반 방법론 정착.
> 사용자가 "느려졌다"/"잔랙"/"프레임 떨어짐" 보고 시 코드 의심 *전에* 시스템 부하 격리하는 5단계 파이프라인.

---

## 0. TL;DR + 다음 액션

사용자가 코드 변경 직후 "성능이 떨어졌다" 보고하면 *코드를 의심하기 전에* 시스템 부하 격리 먼저. 5단계 파이프라인으로 진행:

1. **키워드 디스앰비귀에이션** -- "컴퓨터 자체 잔랙" vs "게임 fps만 떨어짐" (시스템 부하 vs 코드 부하)
2. **코드 변경의 *진짜* 매 프레임 비용** 정확 측정 (사용자 추측 부인/확인)
3. **CPU top N 식별** -- `ps -axo %cpu,%mem,command | sort -rn | head` (현재 시점 부하 1~3순위)
4. **의심 PID 엄밀 조사** -- `ps -p PID -o user,ppid,start,etime,vsz` + `lsof -p PID` (정체 확정, **특히 stdout 경로로 *어떤 도구*가 spawn 했는지 추적**)
5. **조치** -- kill 안전성 검증 후 SIGTERM->SIGKILL, 시스템 데몬은 SIP 검토 후 GUI fallback

**⚠ 본 핸드오프의 결정적 발견 (case study)**: *과거 Claude Code 세션이 leak 한 Python 프로세스* 가 *2일 21시간* 동안 CPU 97% 점유하며 사용자 컴퓨터를 느리게 만들 수 있다. 본인 도구가 self-induce 한 부하를 의심 1순위 후보로 명시.

---

## 1. State of the world -- case study (2026-06-21 Slang Phase 2 T5 직후)

### 증상
- 사용자: "게임의 프레임이 너무 느려졌다. 프레임별로 json 파싱이나, 그러한것들이 작동하는건가?"
- *직전 변경*: Slang Phase 2 T5 (bullet_factory.h:132-133 주석 해제, UBO 셰이더 PoC 활성화).
- 의심 1순위 = "내가 추가한 UBO 코드가 매 프레임 무거운 작업?".

### 진단 결과
- **Phase 2 코드는 무죄** -- draw 당 +3 GL 호출 (UBO update) 만 추가. 시스템 전반 부하 못 일으킴.
- **진짜 원인 2종**:
  - **PID 72877 Python (97.5% CPU, 2일 21시간 실행)** -- *이전 Claude Code 세션* 이 `Bash(python -)` 으로 띄운 stdin-piped Python. 세션 종료 후에도 orphan 으로 남아 무한 루프. lsof 의 stdout 경로 `/tmp/claude-501/.../e5798cc0-.../tasks/` 가 결정적 단서.
  - **macOS Apple Intelligence 데몬 ~54%** -- `modelcatalogd` (36%) + `generativeexperiencesd` (18%). *주기적 작업* 패턴이라 측정 시점 우연히 부하.
- **해소** -- PID 72877 SIGTERM 정상 종료. Apple Intelligence 는 SIP 차단으로 CLI 불가, 시스템 설정 GUI deep link 안내.

### 학습
- "컴퓨터 자체 잔랙" = *시스템 전역* 자원 점유 신호. 단일 게임 draw +3 GL 호출은 시스템 부하 못 일으킴.
- Claude Code 의 `Bash(tool)` 가 stdin-piped Python/Node/shell 을 띄우고 *세션 종료 시 명시 cleanup 없으면* orphan leak. 반복 누적 가능성.

---

## 2. Locked decisions (재논의 금지 -- 본 세션 방법론 lock)

| # | 결정 | 근거 |
|---|---|---|
| T1 | **사용자 보고 키워드 디스앰비귀에이션 우선** -- "컴퓨터 자체 잔랙" vs "게임만 느림" 구분 | 시스템 부하 vs 코드 부하 분기 신호. 잘못 인식 시 코드 격리 실험에 시간 낭비. |
| T2 | **자기 도구 self-induced leak 가능성 의심 1순위 후보** | Claude Code 의 `Bash` 가 띄운 long-running 프로세스가 세션 종료 후 orphan 으로 남는 패턴. case study 에서 결정적. |
| T3 | **`lsof -p PID` 의 stdout/stderr 경로가 정체 확정 단서** | `/tmp/claude-<uid>/.../tasks/<task-id>.output` = Claude Code task output. cwd + PIPE 도 보조 단서. |
| T4 | **SIP 보호 시스템 데몬은 CLI bootout 불가** -- GUI deep link 로 우회 | `launchctl bootout` 이 `Operation not permitted while System Integrity Protection is engaged` 로 차단. sudo 도 불가 (SIP). |
| T5 | **코드 격리 실험은 *마지막* 수단** -- 시스템 부하 정상화 후에도 회귀 남으면 진행 | 시스템 부하 시 격리 실험 결과 noise. 의미 있는 비교 불가. |

---

## 3. 5단계 진단 파이프라인 (paste-ready)

### Step 1 -- 사용자 보고 키워드 디스앰비귀에이션 (대화만)

사용자 보고에서 다음 키워드 찾기. *적극 명시 질의*:

| 키워드 | 의미 | 분기 |
|---|---|---|
| "컴퓨터 *자체* 잔랙" / "시스템 전반" / "다른 앱도 느림" | 시스템 부하 | Step 3 (CPU top 측정) 즉시 진입 |
| "게임만 느림" / "내 앱만 느려짐" / "다른 앱은 정상" | 코드 부하 | Step 2 (코드 비용 분석) 진입 |
| "시점부터 느려짐" / "특정 액션 후 느려짐" | 누적 / 트리거 | Step 2 + Step 3 병행 |
| "점점 느려짐" | 메모리 누수 / GL 자원 누수 | Step 4 + 장시간 모니터링 |

질의 예시:
> "느려진 게 **게임 fps만** 떨어지는 거야, 아니면 **컴퓨터 전체** 가 잔랙 생기는 느낌이야?"

### Step 2 -- 코드 변경의 *진짜* 매 프레임 비용 정확히 측정

직전 코드 변경의 *매 프레임 추가 호출* 수치화. 사용자 추측 (예: "JSON 매 프레임 파싱?") 에 *직답*:

- 신규 함수의 *호출 빈도* (매 프레임 / 매 program 전환 / 매 draw / 1회만) 분류.
- 매 프레임 호출 = 직접 측정 대상. 1회만 호출 = 무관 후보.
- GL 호출 추가 수 = 정확히 N 개 (`glBindBuffer * X + glBufferSubData * Y + ...`).
- 일반적으로 *수십~수백 추가 GL 호출/frame* 까지는 시스템 전반 부하 못 일으킴.

### Step 3 -- CPU top N + 의심 후보 식별

```bash
# 현재 시점 CPU 부하 top 7 + Claude Code 도구 관련 (clangd/python/node)
echo "=== Claude Code 도구 ===" ; ps -axo pid,%cpu,%mem,rss,etime,command 2>/dev/null | awk 'NR==1 || /clangd|claude|python|node/' | head -15
echo ; echo "=== 전체 CPU top 7 ===" ; ps -axo %cpu,%mem,command 2>/dev/null | sort -rn | head -8
```

**의심 우선순위** (큰 부하 → 작은 부하):
1. **Python/Node *오랜 etime + 높은 CPU*** (>60%, etime > 1시간) -- *self-induced leak 1순위*
2. **`modelcatalogd` / `generativeexperiencesd` / `mlhostd`** -- macOS Apple Intelligence (주기적 부하)
3. **`mds` / `mds_stores` / `mdworker`** -- Spotlight indexing
4. **`backupd`** -- Time Machine
5. **`ContextStoreAgent` / `siri`** -- Apple 검색/Siri
6. **clangd indexing** -- 우리 도구의 정통 부하 (코드 변경 직후)
7. **Chrome/VS Code Renderer 다중** -- 브라우저/IDE 자체 부하

### Step 4 -- 의심 PID 엄밀 조사

```bash
# 의심 PID 엄밀 조사 (PID 자리에 실제 번호)
PID=72877  # 예시
echo "=== 1. ps detail ===" ; ps -p $PID -o user,pid,ppid,pgid,sess,lstart,etime,%cpu,%mem,rss,vsz,command 2>&1
echo ; echo "=== 2. 부모 프로세스 ===" ; PPID=$(ps -p $PID -o ppid= 2>/dev/null | tr -d ' ') ; ps -p "$PPID" -ww -o pid,ppid,user,etime,command 2>&1
echo ; echo "=== 3. lsof: cwd + 텍스트 + stdin/stdout/stderr ===" ; lsof -p $PID 2>&1 | head -25
echo ; echo "=== 4. stdout/stderr 경로 (정체 확정 단서) ===" ; lsof -p $PID 2>&1 | awk '$4 ~ /^[0-9]+[uwr]$/' | head -10
```

**정체 확정 단서 우선순위**:

| 단서 | 해석 |
|---|---|
| **PPID = 1 (launchd)** | orphan -- 원래 부모가 죽음. 자기 도구가 자식을 안 reaped 한 leak 신호. |
| **stdout/stderr = `/tmp/claude-<uid>/...`** | 🎯 Claude Code task output. 본인 도구가 spawn. |
| **stdout/stderr = `/tmp/cursor-<uid>/...`** | Cursor 등 다른 AI 코딩 도구. |
| **cwd = 알고리즘/스터디 디렉토리** | 사용자 *과거* 학습 세션 흔적. |
| **stdin = `/private/tmp/zsh*`** | zsh heredoc -- `cat <<EOF \| python` 패턴. |
| **VSZ 거대 (>100GB) + RSS 작음** | mmap 패턴 -- ML 모델 가중치 / 거대 데이터 매핑. |
| **명령줄 = `python -` / `node -e` / `sh -c`** | stdin-piped 스크립트 (스크립트 이름 없음). 자기 도구 spawn 의심. |
| **etime > 1일 + CPU > 80%** | runaway 무한 루프. |

### Step 5 -- 조치

#### 5-A. orphan/runaway 프로세스 kill

```bash
# 안전 순서: SIGTERM (정상 종료 시도) → SIGKILL (강제)
PID=72877
kill $PID 2>&1 ; sleep 2
if ps -p $PID > /dev/null 2>&1; then
  kill -9 $PID 2>&1 ; sleep 1
  ps -p $PID > /dev/null 2>&1 && echo "❌ SIGKILL 도 실패 (권한/SIP)" || echo "✅ SIGKILL"
else
  echo "✅ SIGTERM 정상 종료"
fi
```

#### 5-B. 시스템 데몬 (macOS Apple Intelligence 류)

```bash
# 1. user-domain bootout 시도 (sudo 불요)
UID=$(id -u)
launchctl bootout "gui/$UID/com.apple.generativeexperiencesd" 2>&1
# 실패 시: "Operation not permitted while System Integrity Protection is engaged"
#   → SIP 차단. CLI 불가, GUI fallback.

# 2. 시스템 설정 GUI deep link
open "x-apple.systempreferences:com.apple.Siri-Settings.extension"  # Apple Intelligence & Siri 패널
# 또는
open "/System/Applications/System Settings.app"
```

사용자 안내 (macOS Tahoe 기준):
> 시스템 설정 → Apple Intelligence & Siri → Apple Intelligence 토글 OFF

#### 5-C. 후속 모니터링

```bash
# 조치 직후 top CPU 재측정 -- 부하 사라졌는지
ps -axo %cpu,%mem,command 2>/dev/null | sort -rn | head -5
```

조치 후 사용자가 게임/앱 재실행 → fps 정상화 보고 받으면 *원인 확정*.

---

## 4. Skill 화 후보 (추후 변환 시)

### Skill 명: `system-perf-triage` 또는 `process-leak-detector`

**Description 초안** (`description` frontmatter):
> Apply when user reports performance regression -- "느려졌다", "잔랙", "fps 떨어짐", "컴퓨터가 멈춤" 등 -- *especially after code changes that don't obviously match the symptom scale*. Use whenever symptoms suggest system-wide load (other apps slow too) vs single-app regression. Performs 5-step triage: keyword disambiguation → code cost analysis → CPU top identification → suspect PID forensics → kill/GUI fallback. Critically: **always considers Claude Code self-induced process leaks as suspect #1** (orphan Python/Node from past `Bash(tool)` invocations). Trigger on "왜 이렇게 느려졌어?", "프레임 떨어졌어", "system slow after my change", etc.

### Trigger 키워드 (Korean + English)
- "느려졌다" / "느려" / "잔랙" / "프레임 떨어짐" / "버벅거림"
- "slow" / "lag" / "frame drop" / "system slow" / "machine slow"
- "컴퓨터 자체" / "전체적으로 느림" / "다른 앱도"

### 필요 도구 권한
- `Bash`: `ps`, `lsof`, `launchctl`, `kill`, `open` -- 모두 user-level (sudo 불요)
- 읽기 전용: `Read`, `Grep`
- **sudo 권한 요구 금지** -- 시스템 보호 유지

### 안티게이밍 / 위험
- ⚠ **임의 kill 위험** -- *사용자 실행 중인 작업* 의 프로세스 kill 시 데이터 손실. **PID 정확히 명시 + 사용자 동의 받기** 필수.
- ⚠ **시스템 데몬 임의 종료 금지** -- macOS 시스템 안정성 위협. SIP 차단은 *Apple 의 안전망*, 우회 시도 금지.
- ⚠ **다른 사용자/root 소유 프로세스** -- user-level kill 권한 없음. 권한 에러 시 사용자에게 보고.
- ⚠ **자기 부인 함정** -- Claude Code 가 *자기 자신의 leak* 을 의심하지 않으려는 편향 회피. 항상 의심 1순위.

### 파이프라인 통합 (CI/CD style 후보)
- Pre-commit hook 후보: `ps -axo etime,command | grep -E 'python -|node -e' | awk '$1 > "1-00:00"'` -- 1일 이상 stdin-piped 스크립트 감지
- Claude Code 세션 종료 시: 자기 spawn 한 자식 PID 추적 + 명시 reap (Anthropic 측 도구 개선 영역)

---

## 5. Anti-patterns (절대 하지 말 것)

### A1. 코드 격리 실험 *먼저*
잘못된 순서:
```
"느려졌다" → 코드 주석 처리 → 재빌드 → 측정
```
시스템 부하 상태에서 측정하면 격리 결과 *noise*. 의미 없음.

올바른 순서:
```
"느려졌다" → 키워드 디스앰비귀에이션 → CPU top 측정 → 시스템 부하 정상화 → 코드 격리 (필요 시)
```

### A2. "내 코드는 무죄" 단정
*증거 없이* 무죄 단정 금지. case study 처럼 매 프레임 비용 *수치화* 한 후 시스템 부하와 *크기 비교* 한 뒤 단정.

### A3. SIP 우회 시도
`launchctl bootout` SIP 차단 → sudo / SIP 비활성화 시도 금지. macOS 안전망 우회는 사용자 시스템 위험.

### A4. 의심 키워드 무시
사용자가 "JSON 파싱?" 같은 *명시 추측* 했을 때 무시하고 다른 방향 가지 말고 *직답* (긍정/부정 + 근거) 한 뒤 진행.

### A5. orphan PID kill 전 정체 미확정
`lsof -p PID` 안 보고 kill 시 *사용자 작업* 죽일 위험. **항상 정체 확정 → 사용자 동의 → kill**.

---

## 6. Case study artifacts (재현 가능)

### 사용자 보고 (원문)
> "게임의 프레임이 너무 느려졌다. 프레임별로 json 파싱이나, 그러한것들이 작동하는건가?"
> (직후 질의에 답) "그냥 컴퓨터 자체가 잔랙이 생김"

### 핵심 명령 + 출력 (case study)

```bash
# Step 3 결과
$ ps -axo %cpu,%mem,command | sort -rn | head -7
86.2  0.0 .../Python.framework/Versions/3.13/.../Python -
41.4  3.6 Google Chrome Helper (Renderer)
36.4  0.1 modelcatalogd
32.6  0.6 WindowServer
24.4  2.5 Code Helper (Renderer)
18.0  0.1 generativeexperiencesd

# Step 4 결과 (의심 PID = Python 86%)
$ ps -p 72877 -o user,ppid,lstart,etime,%cpu,vsz
USER       PPID  STARTED       ELAPSED  %CPU      VSZ
escatrgot  1     Thu Jun 18    02-21:27 88.7      435268544   # PPID=1 orphan!

$ lsof -p 72877 | head
cwd  ... /Users/escatrgot/Library/Mobile Documents/com~apple~CloudDocs/Markdown/SelfStudy/Algorithm/.../SWEA_5644
0r   ... /private/tmp/zshxZnsf4                                   # stdin = zsh heredoc
1w   ... /private/tmp/claude-501/.../tasks/beadkncpb.output       # 🎯 Claude Code task!
```

### 정체 확정
- `/tmp/claude-501/...` = Claude Code (UID 501) task output 경로
- session UUID `e5798cc0-e3d0-4379-ac56-d97f22498ab8` = 과거 다른 Claude Code 세션
- 사용자가 SWEA 5644 알고리즘 풀던 과거 세션에서 `Bash(python -)` stdin piping → 무한 루프 → 세션 종료 후 orphan 으로 2일 21시간 leak.

### 조치 결과
- ✅ PID 72877 SIGTERM 정상 종료
- ❌ generativeexperiencesd bootout SIP 차단 → 시스템 설정 GUI deep link 안내
- ✅ 사용자 게임 재실행 후 fps 정상화 ("꽤 부드러워졌어")
- ✅ 원인 확정 = Python orphan 부하 (Apple Intelligence 는 보조)

---

## 7. Pointers

- 본 case study 가 발생한 상위 작업: [`doc/handoffs/2026-06-20/2026-06-20-slang-phase2-engine-ubo-handoff.md`](./2026-06-20-slang-phase2-engine-ubo-handoff.md) Phase 2 T5 R1 ✅ 직후.
- 관련 전략 핸드오프: [`doc/handoffs/2026-06-20/2026-06-20-slang-post-phase2-strategy-handoff.md`](./2026-06-20-slang-post-phase2-strategy-handoff.md).
- macOS Apple Intelligence 시스템 설정 deep link:
  - `x-apple.systempreferences:com.apple.Siri-Settings.extension` (Apple Intelligence & Siri 패널)
  - `x-apple.systempreferences:com.apple.preference.security?Privacy_AppleIntelligenceReport` (개인정보 보호 리포트)
- launchctl domain syntax: `gui/<uid>/<label>` (user GUI), `user/<uid>/<label>` (user background), `system/<label>` (system, sudo + SIP).
- lsof FD 컬럼 의미: `cwd`=working dir, `txt`=executable/lib, `0r`=stdin read, `1w`=stdout write, `2w`=stderr write, `Nu/Nr/Nw`=numbered fd.

---

## Change log

- 2026-06-21: 최초 작성. Slang Phase 2 T5 R1 ✅ 직후 사용자 "느려졌다" 보고 → 5단계 진단 파이프라인으로 *과거 Claude Code 세션이 leak 한 Python 프로세스* (PID 72877, CPU 97%, 2일 21시간) 정체 확정 + SIGTERM 처리 → fps 정상화. 본 방법론을 Skill `system-perf-triage` 후보로 정착.
