# CPU 단위 테스트 통합 — Artifact A (자기완결 에이전트 프롬프트)

> **작성** 2026-06-27 · **출처** `/lossless-handoff` (원래 채팅 inline → 파일로 보존)
> **소스 브랜치** `game/test-harness` (base 7949a65=oracle). 7 테스트 파일 재확인 완료(아래 §Verified).
> ⚠ `doc/` = .gitignore 로컬. 단일 진입점.

## TL;DR

Track A CPU 안전망(28 ctest, characterization)이 격리 브랜치 `game/test-harness` 에만 있고 mainline 에 미통합. 이 핸드오프는 그 7개 파일을 **mainline 브랜치로 통합**하는 한정 작업. 루트 `CMakeLists.txt` 의 `ENABLE_TESTING`(EXISTS 가드) 배선은 이미 살아있어 **디렉토리만 가져다 놓으면 활성화** — 코어 코드/main.cpp 무변경.

> ✅ **2026-06-27 실행 완료** (game/remove-unused @ c741feb 기준): 7파일 checkout → `cmake --preset ninja -DENABLE_TESTING=ON` → `cmake --build --preset ninja --target tests smoke` → `ctest` = **28/28 PASS, 0 failed**. `test/golden/` 보존. 커밋은 사용자 게이트(미커밋, staged).

---

## ⛔ 추후 작업 (DEFERRED) — 본 통합과 **무관**, 절대 섞지 말 것

> **「// ! 모듈화 대상」(apps/_MyApp_/main.cpp 의 27 멤버 + 14 함수 마커)은 이 CPU 테스트 통합과 별개의 *미래 effort* 다.**

- **무엇:** `game_application` 의 capture 모드 + 부트/렌더루프를 `AppRunner` 로 추출하고, **capture 전용 상태(`mCaptureMode`/`mCaptureFrame`)만 `test/` 의 AppRunner subclass 로 분리**(main.cpp 무침투 골든 캡처). 마커 전수 + 근거는 main.cpp inline 주석에 박혀 있음.
- **왜 DEFERRED:**
  1. 본 통합(Track A CPU 테스트)은 `test/`·`test_smoke/` 신규 파일 배치뿐 — **main.cpp 를 건드리지 않는다**(핸드오프 [Boundaries] = NEVER TOUCH apps/_MyApp_/).
  2. main.cpp 는 현재 **미커밋 capture 작업 + 마커**로 사용자가 라이브 편집 중 → 손대면 충돌.
  3. 모듈화는 별도 설계(AppRunner 경계 확정 → PassIterator 확장 여부)가 선행돼야 하는 **독립 effort**.
- **상태:** 마킹만 완료(41개 마커 + 근거 inline). **실제 추출 미착수.** 별도 spec/plan 으로 진행할 것.
- **착수 시 게이트:** 이 CPU 테스트 통합이 mainline 에 커밋된 *후*, main.cpp capture 미커밋분 처리 방향(커밋 vs restore)이 정해진 *후* 시작.

---

## 복붙용 에이전트 프롬프트

```
[ROLE]
너는 OpenGL-ComputerGraphics(C++17/CMake/vcpkg, macOS, GL 4.1)의 테스트 통합 담당이다.
프로젝트 루트 = /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
목표 = Track A CPU 단위 테스트(28 ctest, characterization)를 격리 브랜치 game/test-harness 에서
       현재 mainline 브랜치로 "통합"하고 28/28 GREEN 을 재현하는 것. 단일 bounded 작업.

[Hard rules]
- 빌드/커밋은 사용자가 게이트한다. 너는 구현+검증+보고만. **커밋하지 말 것**(사용자 지시 없으면).
- 커밋이 허용될 때도 path-scoped(`git commit <경로>`), `git add -A` 금지(사용자 병렬 작업 휩쓺).
- Co-Authored-By 트레일러 미사용. 주석은 한국어.
- sb7(extern/sb7code) 절대 수정 금지. 코어 src/ 모듈 코드·apps/_MyApp_/main.cpp 무변경
  (이 작업은 test/ 신규 파일 배치 + 빌드 검증뿐).
- 테스트는 characterization(현 동작 잠금), red-first TDD 아님. 새 테스트를 임의 추가하지 말 것
  (요청된 7파일 통합만).

[Verified facts] (2026-06-27 재측정 — 그래도 시작 시 git 으로 재확인)
- game/test-harness 에 존재하는 7 파일(`git ls-tree -r --name-only game/test-harness -- test test_smoke`):
    test_smoke/CMakeLists.txt
    test_smoke/smoke.cpp
    test/CMakeLists.txt
    test/test_timer.cpp
    test/test_transform.cpp
    test/test_light.cpp
    test/test_sprite_uvrect.cpp
    test/test_fsm.cpp
- 루트 CMakeLists.txt 의 ENABLE_TESTING(option, 기본 OFF) 분기가 find_package(Catch2 3 CONFIG)
  + add_subdirectory(test_smoke)/add_subdirectory(test) 를 EXISTS 가드로 호출 → 디렉토리만 있으면 활성.
  (CLAUDE.md "테스트 실행" 섹션 참조.) → **루트 CMake 편집 불필요**.
- Catch2 v3 는 vcpkg manifest(vcpkg.json) 에 이미 의존성. find_package(Catch2 3 CONFIG REQUIRED) 동작.
- 커버: common(smoke)/timer/object(Transform·Light)/sprite(ComputeUVRect)/fsm. 전부 변경 안 된 코어
  모듈의 public 헤더에만 바인딩(contract-anchoring) → 리팩토링에 강함.

[STEP 1] 현재 브랜치 확인 + 7파일 가져오기
- `git -C <root> status` / `git branch --show-current` 로 통합 대상 브랜치 확인.
  (의도 = game/main 으로 갈 mainline. game/remove-unused 가 game/main 편입 예정이면 그 위에.)
- 작업 트리에 test/ 와 test_smoke/ 가 없으면:
    git checkout game/test-harness -- test test_smoke
  (인덱스에 7파일이 stage 됨 — 커밋은 사용자 게이트, 아래 Verify 후 보고만.)
- scripts/crossver_verify.sh 도 game/test-harness 에 있음. 필요시 같은 방식으로 가져오되
  이 작업의 필수는 아님(differential 은 별도). 가져왔다면 보고에 명시.

[STEP 2] 빌드 + ctest
- cmake --preset ninja -DENABLE_TESTING=ON
- cmake --build --preset ninja --target tests
- ctest --test-dir build_ninja --output-on-failure

[Boundaries]
- OWNS: test/, test_smoke/ (신규 배치), 그리고 (가져왔다면) scripts/crossver_verify.sh.
- NEVER TOUCH: src/ 코어 코드, apps/_MyApp_/(main.cpp 포함), extern/, 루트 CMakeLists.txt
  (ENABLE_TESTING 배선이 이미 맞으므로 수정 불필요 — 만약 add_subdirectory 가드가 없다면
   보고만 하고 멈출 것, 임의 편집 금지).
- 사용자가 같은 워킹트리에서 병렬 작업 가능 → git status 의 다른 변경은 건드리지 말 것.

[Verify] "done & correct" =
- ctest 출력에 **28/28 PASS**(또는 실제 케이스 수와 함께 100% passed).
- 빌드 에러 0. 코어/main 무변경(git status 로 test/·test_smoke/ 외 변경 없음 확인).

[Self-review]
- [ ] test/·test_smoke/ 외에 변경된 파일 없음(git status)
- [ ] 루트 CMakeLists.txt 무수정
- [ ] ctest 28/28(또는 실수치) GREEN, 출력 첨부
- [ ] 새 테스트 임의 추가 안 함(7파일만)

[Report] 구조화 보고:
- 상태: DONE / DONE_WITH_CONCERNS / BLOCKED
- 변경(배치)된 파일 목록
- ctest 결과(케이스 수 + pass/fail)
- 커밋 안 했음 명시(사용자 게이트) + 권장 커밋 메시지/경로
- deferred/우려 사항(있으면)
```

## Notes (오케스트레이터용 — fence 밖)

- **권장 커밋**(사용자 승인 후): `git commit test test_smoke -m "[test] : Track A CPU 단위 테스트 통합 (28 ctest characterization)"` — path-scoped, Co-Authored-By 없음.
- **통합 대상 브랜치 결정**: 현재 `game/remove-unused`(=315269a 리팩토링) 가 `game/main` 편입 예정. 테스트는 7949a65 기반이지만 cross-version EQUIVALENT(7949a65 ≡ 315269a) 검증됨 → remove-unused/main 어느 쪽에 올려도 GREEN 이어야 함. mainline 이 될 브랜치에 통합.
- **관련 메모리**: [[engine-test-harness-effort]](Track A 전모), [[no_auto_tests]](테스트 임의추가 금지 정합), [[user-parallel-git-and-builds]](path-scoped 커밋).
- **재개 진입점(전모)**: `doc/handoffs/2026-06-25/2026-06-25-test-harness-p1-resume-handoff.md`. 정본 spec/plan: `doc/superpowers/specs|plans/2026-06-25-engine-test-harness*`.
