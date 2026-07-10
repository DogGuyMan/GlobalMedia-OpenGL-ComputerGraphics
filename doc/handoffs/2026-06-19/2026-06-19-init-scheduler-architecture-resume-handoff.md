# Handoff — InitScheduler T2 init-architecture 재개 컨텍스트 (2026-06-19, lossless)

> **이 문서 = 다음 세션의 단일 진입점.** render->rr 절단 brainstorm이 *Manager/startup 초기화 아키텍처(InitScheduler T2)* 설계로 진화했고, 그게 render->rr 절단(지점②) + 전역 싱글톤 정리(PostFXRegistry 흡수 / Manager 개명)까지 흡수했다.
> **현재 위치**: **구현 100% 완료 (Task 1~7, subagent-driven) · 빌드 GREEN · 미커밋(사용자 게이트).** 다음 = **GUI 육안 검증 + 사용자 path-scoped 커밋**.
> **구현 plan** = [`doc/superpowers/plans/2026-06-19-init-scheduler.md`](../superpowers/plans/2026-06-19-init-scheduler.md) (Task 1~7 + 완전 코드). 🔴 gitignore(로컬).
> **구현 완료 요약 (2026-06-20)**: 3 목표 전부 grep 검증 — ① `render->rr` 코드 의존 0(완전 DAG) ② PostFXRegistry 흡수(클래스/호출 0, D-7) ③ Manager->GameSystems 개명(클라 클래스 0, D-8). 각 Task spec+quality 리뷰 통과 + 최종 통합 리뷰 "Ready to commit". HEAD가 vcpkg 마이그레이션 커밋(`9123dae` 등)으로 이동했고 내 변경은 그 위 미커밋. **남은 것 = 사용자 GUI 회귀 + 커밋 뿐.** 신규=InitScheduler/EngineBootstrap/GameSystems/render_bootstrap, 삭제=Manager/PostFXRegistry/render_pipeline(구위치).
> **작성**: 2026-06-19. Branch `game/main`. HEAD `cadf80b`.
> **정본 설계** = [`doc/superpowers/specs/2026-06-19-init-scheduler-design.md`](../superpowers/specs/2026-06-19-init-scheduler-design.md) (이 핸드오프는 *meta 컨텍스트*; 설계 디테일은 spec).
> ⚠ 이 repo는 **사용자 병렬 작업 3종(webeditor / vcpkg-migration / 본 작업)** + 직접 커밋/staging 진행 중. 시작 시 `git log --oneline -4` + `git status --short` 재측정 필수.

---

## 0. 한눈에 (TL;DR)

- **무엇**: `_MyApp_`의 `game_application::startup()`(~40노드×10레벨 의존 DAG, [doc/diagrams/startup-init-topo.dot](../../doc/diagrams/startup-init-topo.dot))를 **데이터주도 topo-sort 초기화 시스템(InitScheduler)**으로. 동시에 render->rr 모듈 사이클 절단 + 전역 싱글톤 2건 정리.
- **왜**: ① 취약한 수동 40줄 startup() → 선언적 task 그래프. ② render->rr->sprite->render 3-사이클(유일 잔존) 제거 → 완전 DAG. ③ SIOF 회피. ④ 중복 전역(PostFXRegistry) 제거 + Director 명칭 충돌 해소.
- **진행**: brainstorm Section 1~3 합의 + **spec 작성·자체검토 완료(D-1~D-8 + §11 전역정리)**. 코드/커밋 0.
- **다음 행동**: plan(7 task) 실행 — subagent-driven(권장) 또는 inline. Task 7 개명 `GameSystems` 확정만 사용자 게이트.
- **방법**: brainstorming HARD-GATE 통과(설계 승인됨). 커밋 사용자 게이트. 구현은 path-scoped, 단계별 빌드 GREEN + GUI 육안.

## 1. State (작성 시점 재측정 — 재개 시 재측정 필수)

```
HEAD: cadf80b [refactor] 텍스쳐 이미지 의존 개선   (이 세션 중 커밋 0, 불변)
Branch: game/main
Uncommitted (MINE, tracked):
  M doxygen/pages/00-mainpage.md                          -- ModuleDeps 그래프 현재상태 갱신(render->rr 빨강)
  ?? doc/diagrams/2026-06-19-module-deps-current.*     -- 18모듈 의존 그래프
  ?? doc/diagrams/2026-06-19-global-singletons.*       -- 전역 싱글톤 5종 그래프
Uncommitted (MINE, gitignore 로컬 = doc/):
  doc/superpowers/specs/2026-06-19-init-scheduler-design.md          -- 정본 설계
  doc/handoffs/2026-06-19/2026-06-19-init-scheduler-architecture-resume-handoff.md  -- 본 문서
  doc/handoffs/2026-06-19/2026-06-19-render-rr-cycle-cut-design-handoff.md      -- 확장원(배너됨)
Uncommitted (NOT mine = 사용자 병렬):
  M README.md · doc/diagrams/*webeditor* · doc/handoff/*webeditor* · doc/handoff/*vcpkg-migration*
  m extern/Catch2, extern/assimp (ambient)
```

## 2. 결정/단계 status

| # | 내용 | 상태 |
|---|---|---|
| 설계 | brainstorm Section 1~3 + spec D-1~D-8 + §11 전역정리 | ✅ 완료(자체검토 포함) |
| 구현 plan | Task 1~7 완전 코드 + 검증절차 | ✅ 완료(`plans/2026-06-19-init-scheduler.md`) |
| 구현 | InitScheduler / render_bootstrap / 지점① / EngineBootstrap / startup 마이그레이션 / D-7 / D-8 | ⬜ 미착수(plan 실행 대기) |
| 산출물 | spec 1 + 그래프 2(module-deps, global-singletons) | ✅ (그래프 tracked, spec gitignore) |

## 3. 정본 문서 인덱스

> ⚠ **`doc/`(복수) = gitignored** (`.gitignore:10`) → spec/핸드오프는 **로컬 전용**. `doc/`(단수)만 tracked.

| 문서 | 경로 | git | 역할 |
|---|---|---|---|
| **정본 설계 spec** | `doc/superpowers/specs/2026-06-19-init-scheduler-design.md` | 🔴 gitignore(로컬) | D-1~D-8 + §11. 설계 SSOT |
| **본 핸드오프** | `doc/handoffs/2026-06-19/2026-06-19-init-scheduler-architecture-resume-handoff.md` | 🔴 gitignore(로컬) | 재개 진입점(meta) |
| 확장원(배너됨) | `doc/handoffs/2026-06-19/2026-06-19-render-rr-cycle-cut-design-handoff.md` | 🔴 gitignore(로컬) | render->rr 3-사이클 분석 reference |
| startup 위상 DAG | `doc/diagrams/startup-init-topo.dot` | ✅ tracked | 마이그레이션 대상(40노드×10레벨 Kahn) |
| 의존 그래프(18모듈) | `doc/diagrams/2026-06-19-module-deps-current.*` + `doxygen/pages/00-mainpage.md` | ✅ tracked(미커밋) | render->rr 빨강 |
| 전역 싱글톤 그래프 | `doc/diagrams/2026-06-19-global-singletons.*` | ✅ tracked | 5 싱글톤 + 흡수후보 |

## 4. Locked decisions (재론 금지 — 상세는 spec §3/§11)

- **D-1 지점① push**: SceneRenderer가 program 목록을 rr pull 안 함, 외부가 push. (per-frame, init task 아님)
- **D-2 지점② A**: 파이프라인 함수 *정의*를 신규 엔진 모듈 `src/render_bootstrap/`로 이주(클라로 내리는 trivial-C 아님). render->rr 절단.
- **D-3 scope T2**: 데이터주도 topo-sort + 직렬. 병렬=T3 게이트(GL 단일스레드).
- **D-4 Template Method + 변동성 분리**: 엔진 저변동 스켈레톤(`EngineBootstrap::Boot(IClientBootstrap&)`)이 hook 타이밍에 클라 InitScheduler 역호출.
- **D-5 검증→Kahn→F-2 fail-fast**: 그래프 오류(중복id/누락dep/사이클)=실행 전 하드에러, task 실패=즉시 중단.
- **D-6 fluent builder (C++17)**: `{.Id=}` designated init은 C++20/MSVC 미지원 → `Task("x").Needs({...}).Gl().Does([]{...})`.
- **D-7 PostFXRegistry → ResourceRegistry 흡수**: 중복 별칭(pass Material 이미 rr `mat_pass_<name>`). 연출 트랙은 `rr.FindMaterial("mat_pass_"+name)`.
- **D-8 Manager 개명**: 엔진 `Scene::Director`와 "Director" 명칭 충돌 해소(병합 아님 — 다른 도메인/레이어). 새 이름 미확정(GameSystems/GameApp 등 후보).
- ⚠ **정정**: "init task로 만든다"="함수를 클라로 옮긴다" 아님. 함수 정의=엔진(render_bootstrap), 클라 task는 호출만 감쌈.
- ⚠ **GL 단일스레드**: GL 객체생성=메인스레드 전용. 병렬은 CPU-only(audio/physics/decode)만, GL은 직렬. affinity는 T2에서 분류만.

## 5. 남은 작업 — 구현 plan 단계 (writing-plans 가 상세화/순서확정)

자연스러운 분할(독립 빌드 GREEN 단위):
1. **InitScheduler 유틸**(신규, 클라 `apps/_MyApp_/src/Bootstrap/`): `InitTask`(id/deps/affinity/`std::function<bool()>`) + fluent `InitTaskBuilder` + `Validate`(Kahn 중복/누락/사이클 하드에러) + `ExecuteInOrder`(등록순 tiebreak 결정성 + F-2 fail-fast). 독립 단위.
2. **render_bootstrap 모듈**(D-2): `src/render/render_pipeline.{h,cpp}`(+구조체 3종) → `src/render_bootstrap/`. CMake STATIC `SJH::render_bootstrap`(deps render/rr/object/material/scene/buffer) + umbrella(19) + 클라 include 2곳(`apps/_MyApp_/main.cpp:50`, `apps/_MyApp_/src/Playable/PostFXConstants.h:23`). => render->rr include 0.
3. **지점① push**(D-1): `SceneRenderer`에 `SetActivePrograms(vector<Program*>)` setter+멤버, `scene_renderer.cpp:123`의 `ResourceRegistry::Get().GetAllPrograms()` 제거, 클라 render 루프에서 push. => 잔존 3-사이클 소멸.
4. **EngineBootstrap + IClientBootstrap**(D-4): template-method 스켈레톤 + 3 hook. ⚠ spec §10 open: 엔진-고정 코드 얇으면 가치<비용 재검토(클라 단일 스케줄러로 단순화 가능).
5. **startup() 마이그레이션**: startup-init-topo.dot 간선 → InitTask `Deps` 전사(기계적). 누락 dep 주의. 빌드 GREEN + GUI 육안.
6. **D-7 PostFXRegistry 흡수**: 싱글톤 제거, `PostFXTweenPlayable`/`HpGrayscalePostFX`가 `rr.FindMaterial("mat_pass_"+name)` 직접 조회.
7. **D-8 Manager 개명**: 새 이름 사용자 확정 후 일괄 rename.

**검증 명령**: `cmake --preset ninja` → `cmake --build --preset ninja --target _MyApp_`(에러 0). 실행/GUI=사용자(`cd build_ninja/apps/_MyApp_ && ./_MyApp_`). ⚠ `-DENABLE_TESTING=ON`은 사전 breakage(test_smoke 디렉토리 부재) — 무관.

## 6. 병렬 트랙 conflict matrix

| 트랙 | 파일 | vs 내 작업 |
|---|---|---|
| **webeditor**(사용자) | `*webeditor*`(diagrams/handoff/specs), README.md | 0 충돌, 건드리지 말 것 |
| **vcpkg-migration**(사용자, 신규) | `doc/handoffs/2026-06-19/2026-06-19-vcpkg-migration-handoff.md` | 0 충돌. ⚠ 단 vcpkg 도입이 CMake/빌드에 영향 줄 수 있으니 구현 전 빌드 방식 재확인 |
| **init-scheduler**(나) | doc/(spec/handoff) + doc/diagrams/(graph 2) + doxygen/pages/00-mainpage.md + src/render_bootstrap/(예정) + apps/_MyApp_/src/Bootstrap/(예정) | 본인 |

> 커밋 시 path-scoped만(`git commit <경로>`), `git add -A` 금지(병렬 staging 휩쓺). 구현 영역 `src/render_bootstrap/`(신규)·`apps/_MyApp_/src/Bootstrap/`은 사용자 병렬과 무관.

## 7. 가드레일 / 컨벤션

- **커밋 사용자 게이트** + path-scoped + **`Co-Authored-By` 미사용**. 빌드는 사용자.
- **불가침**: `extern/sb7code` 수정 금지(application::run 템플릿메서드 → EngineBootstrap은 우리 코드로 startup() 안에서). stb `STB_IMAGE_IMPLEMENTATION` 단일 owner=`src/texture/image.cpp`. FMOD `SJH_HAS_FMOD` 가드. rr `game_deps` PUBLIC 불변. no_auto_tests.
- **코드 컨벤션**: 주석 한국어 + ASCII/한글만(특수문자 0, `->`만, 유니코드 화살표 금지). 헤더가드 `__SJH_<MODULE>_<NAME>_H__`(클라 `_TOPDOWNSHOOTER_*_H__`)·`#pragma once` 금지·`#endif` 주석 일치. Tab indent. `long` 금지. 멤버 `m`PascalCase / 지역 camelCase / bool `mIs*` / 포인터 `*Ptr`.
- **C++17**(MSVC 크로스): designated init·concepts·`<ranges>` 등 C++20 금지.

## 8. 설계 요약 (전체는 spec — 여기는 진입용 최소)

- **3층**: ① InitScheduler(제네릭 유틸, 클라 Bootstrap/) ② render_bootstrap 엔진 함수(D-2) ③ 클라 task 정의.
- **제어역전**: `EngineBootstrap::Boot(IClientBootstrap&)` 저변동 스켈레톤 + hook(OnResourcesReady/OnSceneSetup/OnBeforeFirstFrame), 클라가 hook 안에서 `InitScheduler.RunAll()`.
- **전역 싱글톤 5종**(spec §11): 엔진 3(DeviceContext/ResourceRegistry/Scene::Director) + 클라 2(Manager/PostFXRegistry). Godot Servers 정통. PostFXRegistry만 흡수(중복).
- **부수 발견**: Audio/VFX/Physics = 엔진 global 0(외부 lib=server + param-DI; Physics는 절차적 body라 가장 순수). WorldText만 rr, SceneRenderer만 Director/rr/DeviceContext. → init task affinity 분류에 유리.
- **정통 매핑(context7)**: Unreal ELoadingPhase / Unity RuntimeInitializeLoadType / Cocos Director+applicationDidFinishLaunching / sb7 run+startup. 병렬=에셋 로딩용. Godot=서버 분리, Cocos=Director+별도 subsystem(통짜 god 아님).

## 9. Known issues (내 작업 무관)

- 사용자 병렬 webeditor + vcpkg-migration = 별개. 쫓지 말 것. (vcpkg는 빌드 방식 바뀔 수 있어 구현 착수 시 재확인 권장.)
- `extern/Catch2`/`extern/assimp` dirty = ambient.
- `-DENABLE_TESTING=ON` configure 실패(test_smoke 부재) = 사전존재, 무관.

## 10. Change log

| 날짜 | 변경 |
|---|---|
| 2026-06-19 (1) | 최초: render->rr brainstorm → InitScheduler T2 피벗, Section 1 합의 |
| 2026-06-19 (2) | Section 2~3 합의 + spec 작성·자체검토(D-1~D-6) + gitignore 발견 정정 |
| 2026-06-19 (3) | 전역 싱글톤 전수조사(5종) + global-singletons 그래프 + D-7(PostFXRegistry 흡수)/D-8(Manager 개명) spec §11 추가 + Physics 격리 비대칭 발견. **설계 100% 완료, 다음=writing-plans.** 본 핸드오프 전면 갱신 |
