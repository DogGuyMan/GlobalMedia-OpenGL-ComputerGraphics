# 재개 핸드오프 — 엔진 테스트 하네스 Phase 1 (Track A) 완료

> **작성** 2026-06-25 · **재측정** branch `game/test-harness` @ `328048d`, working tree clean.
> **이 문서가 단일 진입점.** ⚠ `doc/` 는 .gitignore 로컬 — same-machine 세션 연속성용(cross-machine 인계 시 tracked dir 로 이동 필요).

## TL;DR + 다음 액션

**Phase 1 (Track A: CPU 순수 로직 characterization 안전망) 구현 완료.** 격리 worktree `game/test-harness` 에서 6 Task(0~5) + 최종 리뷰 전부 GREEN. **28/28 ctest PASS** + **cross-version differential EQUIVALENT**(7949a65 ≡ 315269a, 28 케이스 동일).

**다음 액션:**
1. **브랜치 통합 = 사용자가 직접 수행** (`game/test-harness` → `game/main`). 7커밋 전부 신규 파일만이라 충돌 0. worktree = `.claude/worktrees/engine-test-harness`. (메모리 `user-parallel-git` — git 은 사용자 직접 관례.)
2. **다음 substantive 작업 = Phase 2 (Track B 골든 이미지)** — **별도 spec+plan 신규 작성**. 성격 전환(GL fixture+FBO+glReadPixels+FLIP, ⚙backend-volatile). 착수 시 GL vs Metal/Vulkan 재결정([[backend-abstraction-direction]] / spec §11).

## State of the world (재측정)

- branch `game/test-harness` (base `7949a65` = game/main HEAD = oracle), tip `328048d`.
- 7 커밋 (전부 path-scoped, `[test]` 형식, Co-Authored-By 없음):
  | SHA | 내용 |
  |---|---|
  | `fdc1c72` | smoke 배선 first-green |
  | `0b9fc33` | test_timer (9) |
  | `ca28299` | test_transform(6) + test_light(4) — 094362d 복원, vmath→glm 수리 |
  | `f51b668` | test_sprite_uvrect(3) |
  | `d2a1045` | test_fsm(5) — StateMachine 최초 인스턴스화 검증 |
  | `df2c9a5` | crossver_verify.sh |
  | `328048d` | crossver_verify.sh — N/28 strip fix |
- working tree clean (미커밋 0).

## Task 상태 (전부 DONE + 2단계 리뷰 통과)

| Task | unit | 결과 | SHA |
|---|---|---|---|
| 0 | smoke 배선 | ctest 1 GREEN | fdc1c72 |
| 1 | test_timer | 9 GREEN, anti-gaming ✅ | 0b9fc33 |
| 2 | test_transform+test_light | 10 GREEN, anti-gaming ✅ | ca28299 |
| 3 | test_sprite_uvrect | 3 GREEN, anti-gaming ✅ | f51b668 |
| 4 | test_fsm | 5 GREEN, anti-gaming ✅ | d2a1045 |
| 5 | crossver capstone | **EQUIVALENT** (28 동일) | df2c9a5 + 328048d |
| 최종 | 홀리스틱 리뷰 | 머지 준비 완료 | — |

전체 재확인: `cmake --preset ninja -DENABLE_TESTING=ON && cmake --build --preset ninja --target tests --target smoke && ctest --test-dir build_ninja` → 28/28.

## Locked 결정 (정본 spec D1~D10)

- **D1** CPU 안전망 우선 / 골든=확정 2차 트랙. **D2** Mull 뮤테이션 보류. **D3** 타겟=common·timer·object(Transform·Light)·sprite(ComputeUVRect)·fsm. **D4** HEAD=oracle, characterization(현 동작 잠금). **D5** 구조 C(test_smoke + test/ 모듈별 exe). **D6** contract-anchoring + crossver differential. **D7** crossver=저비용 적대적검증(macOS-friendly). **D8** 기대값 하이브리드(손계산 / 구조 property). **D9** Graphics Backend 교체 전제(Track A=backend-agnostic). **D10** 모든 handoff=lossless-handoff 스킬.
- 정본 spec: `doc/superpowers/specs/2026-06-25-engine-test-harness-design.md` (⚠ gitignore 로컬).
- 정본 plan: `doc/superpowers/plans/2026-06-25-engine-test-harness.md` (⚠ gitignore 로컬).

## ⚠ Worktree 셋업 함정 (cross-machine/CI 필독)

- **`lib/macos/`(prebuilt sb7/glfw .a)는 gitignored** → worktree/CI 체크아웃에 자동 복제 안 됨. 테스트가 project_deps(sb7/glfw) 전이 링크하므로 **메인 working tree 의 `lib/macos` 를 worktree 에 심볼릭 링크** 필요(`ln -sfn <main>/lib/macos <wt>/lib/macos`). 현 worktree 엔 이미 박혀있음. `crossver_verify.sh` 는 임시 worktree 에 이 심볼릭 링크를 자동 처리.
- include/ 는 tracked(체크아웃됨) — 별도 처리 불요.

## Guardrails / 컨벤션

- **characterization 사이클**(작성→빌드→실행 PASS→커밋), red-first TDD 아님(메모리 `no_auto_tests`). 기대값을 통과시키려 조작 금지 — 불일치는 finding.
- 커밋 **path-scoped**(`git commit <경로>`), `add -A`/`-a` 금지(메모리 `user-parallel-git`). Co-Authored-By 미사용. 주석 한국어.
- 빌드 자율(VCPKG_ROOT 설정됨). 첫 빌드 heavy(vcpkg+엔진), 이후 incremental.
- clangd 거짓에러(catch2 헤더 not found 등)는 무시 — 빌드/ctest 가 진실(메모리 `clangd-imgui-cascade-false-errors`).

## 발견 (findings)

1. **`StateMachine` 최초 인스턴스화 성공** — production 사용 0 이던 `SJH::FSM::StateMachine<TState,TOwner>` 가 mock 으로 컴파일·전이 동작 정상. 하네스가 미실행 코드를 검증.
2. **315269a("의존 없는 코드 삭제") ≡ 7949a65** — cross-version differential 로 dead-code 제거 리팩토링이 28 케이스 공개 동작을 보존함을 증명.
3. characterization 관찰: FSM 은 startup 상태에 OnEnter 를 호출하지 않음(OnExit 는 호출). 현 동작으로 잠금(버그 여부는 미판정 — 필요시 사용자 검토).

## 범위 밖 (후속 별도 plan/세션)

- **Phase 2 Track B 골든**(별도 spec+plan), **CI**, **Mull 뮤테이션**(D2 재결정), **5-에이전트 .md 정식 갱신**(Gate5 Mull 보류·crossver 추가), **scene caster 라이트(DirLight/PointLight/SpotLight) 테스트**(SJH::scene 모듈, object 범위 밖이라 defer).

## Change log

- 2026-06-25: Phase 1 Track A 6 Task + 최종 리뷰 완료. 유일 plan 이탈 = crossver_verify.sh 의 ctest 정규화 sed 에 `N/28` 진행번호 strip 추가(`328048d`, plan 원본 sed 의 latent 버그 교정 — false-DIVERGENT 방지). 커밋된 스크립트가 정본. 브랜치 통합은 사용자가 직접.
