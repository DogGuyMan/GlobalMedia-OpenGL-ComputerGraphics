# 의존 사이클 → DAG 리팩토링 마스터 로드맵 (후보 ①②③④ 전체)

> **단일 진입점.** 엔진+클라이언트 의존 그래프의 **양방향 사이클 10건을 0으로** 만들어 완전 DAG 화하는 전체 작업을 한 화면에서 추적한다. 상세는 각 문서 참조(여기선 중복 안 함). **상태: 클라 ④ Task 1~4 구현 완료(C4 절단=커밋 `f98fc81` · C3·C1·C2 절단=빌드 GREEN·미커밋) → Task 5(통합검증)+커밋만 잔여 · 엔진 ①②③ 미착수.**

## 0. 후보 → 사이클 → 문서 → 상태 (커버리지 매트릭스)

| 후보 | 끊는 mutual | 레이어 | 상세 문서 | 종류 | 상태 |
|---|---|---|---|---|---|
| **① Texture/Image 하위추출** | E3·E5·E6 | 엔진 | 엔진 plan 슬라이스 3 / 엔진 spec §4 | 설계합의(D1·D8) | 🟢 **완료**(빌드 GREEN·미커밋, E3/E5/E6 절단증명·texture 18모듈·D8 DI) · ⚠ 사전존재 3-cycle `render→rr→sprite→render` 잔존(별도결정) |
| **② RenderTarget+Light** | E1·E4 | 엔진 | 엔진 plan 슬라이스 1 / 엔진 spec §2 | 설계합의(D2·D3) | 🟢 **완료**(빌드 GREEN·미커밋, E1·E4 절단증명) |
| **③ scene 팩토리 상위이동** | E2 | 엔진 | 엔진 plan 슬라이스 2 / 엔진 spec §3 | 설계합의(D4) | 🟢 **완료**(빌드 GREEN·미커밋, E2 절단증명) |
| **④ Entity 인터페이스 DIP** | C1·C2·C3·C4 | 클라 | 클라 plan Task 1~5 | D-1~D-4 확정 | 🟢 **Task 1~4 구현 완료** (C4 `f98fc81` 커밋 · C3/C1/C2 빌드 GREEN·미커밋) · Task 5(검증)+커밋 잔여 |

**합계: 엔진 6 + 클라 4 = 10 mutual → 0 목표.** 둘 다 완료 시 의존 그래프 완전 DAG.

## 1. 문서 지도

| 역할 | 경로 |
|---|---|
| 사이클 전수조사(근거) | `doc/handoffs/2026-06-11/2026-06-11-dependency-cycle-refactor-handoff.md` |
| 엔진 설계 정본(D1~D8) | `doc/superpowers/specs/2026-06-11-engine-dependency-cycle-refactor-design.md` |
| 엔진 구현 plan(Task) | `doc/superpowers/plans/2026-06-11-engine-cycle-e1-e6-plan.md` |
| 클라 구현 plan(Task) | `doc/superpowers/plans/2026-06-11-client-cycle-c1-c4-dip-plan.md` |
| **본 마스터 로드맵** | `doc/superpowers/plans/2026-06-11-dependency-cycle-master-roadmap.md` |
| 그래프(현황) | `doxygen/pages/00-mainpage.md` (엔진 ModuleDeps) · `doxygen/pages/30-client-architecture.md` (ClientDeps) |

## 2. 실행 순서 · 독립성

- **엔진 ①②③ ⊥ 클라 ④ 는 독립** — 엔진은 `src/`, 클라는 `apps/_MyApp_/src/` 만 건드림. 코드 충돌 없음. **순서 무관, 병렬 가능.** (엔진이 신설하는 `src/texture/`·`scene/light` 등은 클라가 안 건드림.)
- **엔진 내부 순서(고정):** 슬라이스 ② → ③ → ① (spec D5, 작은 것부터). 슬라이스 3 에 D6(program_uniforms)·D8(SpriteRenderer DI) 함정 포함.
- **클라 내부 순서:** Task 1(시임 이동, C4 동시) → 2(C3) → 3(C1) → 4(C2) → 5(검증).
- **권장 착수:** 클라 ④가 코어 GL 미접촉이라 위험이 낮음 → 먼저. 단 클라(`Entity/` 등)는 사용자 병렬 편집 영역이라 직전 `git status` 필수.

```
[엔진 트랙]  슬라이스② ──▶ 슬라이스③ ──▶ 슬라이스①        (E1,E4 → E2 → E3,E5,E6)
[클라 트랙]  Task1 ─▶ Task2 ─▶ Task3 ─▶ Task4 ─▶ Task5     (C4 → C3 → C1 → C2 → 검증)
두 트랙 독립 — 직렬/병렬 자유. 둘 다 끝 = 10 mutual → 0 = 완전 DAG.
```

## 3. 통합 검증 (완전 DAG 증명)

각 슬라이스/Task 는 자체 빌드 GREEN. 전체 완료 후:
- **엔진:** `grep -rn '#include "<상위모듈>/"' src/<하위모듈>` 0 — 6 mutual 소멸 확인. (rr↔buffer/object/sprite, scene↔object/render, buffer↔render)
- **클라:** `grep -rn '#include "Entity/"' apps/_MyApp_/src/{Physics,InputHandler,Spawns,Playable}` → const-noise(§out-of-scope) 외 0.
- **빌드:** `_MyApp_` + `-DENABLE_TESTING=ON` configure GREEN, MSVC(무-FMOD) CI 통과.
- **GUI 육안(사용자):** 라이팅/스프라이트/월드텍스트/대시/발사/피격 정상.
- **그래프 갱신:** ModuleDeps mutual 6→0 + texture 18모듈, ClientDeps mutual 4→0.

## 4. 열린 의사결정 (착수 전 확정 필요)

| ID | 문서 | 사안 | 상태 |
|----|------|------|------|
| 클라 D-4 | 클라 plan §1 | 액션 인터페이스 입도 | ✅ **D4-a 확정** (IPlayerCommand 통합 + IFireTrigger, 2026-06-11) |
| 착수 순서 | 본 문서 | 엔진/클라 어느 트랙 먼저, 직렬 vs 병렬 | 🔵 **클라 ④ 먼저 진행 중** (Task 1 완료 `f98fc81`). 엔진 트랙 독립 — 미착수 |
| 문서 후속 시점 | 엔진 spec §6 | mainpage 그래프 정정·CLAUDE.md 18모듈 = 코드 GREEN 후 (사용자 uncommitted 파일이라 조율) | 보류 |

## 5. 가드레일 (전 트랙 공통)

- **구현 미착수** — 슬라이스/Task 별 사용자 승인 후 진행, 각 단계 빌드 GREEN 후 보고(커밋 없음).
- 커밋: 사용자 path-scoped 게이트, `git add -A` 금지, **`Co-Authored-By` 미사용**, 빌드는 사용자.
- 병렬 편집: 클라 `Entity/Physics/Spawns/InputHandler/Playable` + 엔진 D8 의 `text_renderer.cpp`·클라 2파일 = 활성 영역 → 직전 `git status`.
- 컨벤션: 한국어 Doxygen(특수문자 0), `__SJH_*_H__` 가드(`#pragma once` 금지), Tab indent, `long` 금지, sb7code 불가침, FMOD `SJH_HAS_FMOD` 가드·rr game_deps PUBLIC 불변, no_auto_tests.

## 6. 설계 검토 결과 (4-문서 렌즈 전수조사 — 2026-06-11)

`improve-codebase-architecture`(depth/deletion-test/seam) + `architecture.md`(§11 패턴·13 암묵합의) + `architecture-design-agent`(ddd 14 rules) 렌즈로 두 plan 재점검. **놓친 사이클·블로킹 갭 0** (F6) — 나머지는 문서 보강으로 닫음.

| ID | 발견 | 처리 |
|----|------|------|
| F1 | D6가 `SJH::Uniforms` family 분리(광원 setter 3종→render dispatcher) — architecture.md §11.1 정합 | 엔진 plan Task 1.1 노트 + §11.1 갱신을 문서후속(§ 엔진 spec §6)에 포함 |
| F2 | D8 DI 주입 `Mesh*`/`Material*`는 rr 세션수명 소유분이어야 dangling-safe(§11.2) | 엔진 plan Task 3.3 불변식 명문화 |
| F3 | 클라 ④ 신규 4 인터페이스 = 단일 adapter "hypothetical seam"(shallow, 순수 레이어링 DIP) | 클라 plan §1 D-3 노트 강화 (기지 trade-off, 완전절단 채택 유지) |
| F4 | `SpriteResources`/`actor_factory` 명 generic 경계 (ddd) | 💭 경미 — 승인 spec 명칭 유지, 추후 재검토 여지만 기록 |
| F5 | 신규 `src/texture/` §7 체크리스트(CLASS_PTR·가드·팩토리·build-verify) 준수 명시 | 엔진 plan Task 3.1 노트 |
| F6 | **신규 사이클 0 확인** (D6 4-사이클 차단·E3 단방향·Contracts 무루프) | ✅ 확인 — 조치 불요 |
| F7 | 💚 **deepening 정합(긍정)**: E3 Texture 추출 = rr(leaf+facade 혼재 shallow → 순수 캐시 facade deep) + texture deep leaf. 클라 ④ shallow seam도 testability(Contracts mock) 충족 — improve-codebase 목표와 일치 | ✅ 확인 — 설계 방향 타당 |

> EngineDesign.md 는 🛑 역사적 스냅샷(구 `src/engine/*`, 17모듈 분리 전) — 본 사이클 작업과 직교, 추가 발견 없음.

## Change log
- 2026-06-11 (4) — 클라 ④ **Task 2~4 구현 완료**(빌드 GREEN, 미커밋). Task 2(C3): `apps/_MyApp_/src/Spawns/Carrier.h` → `Entity::ILivable`. Task 3(C1): `apps/_MyApp_/src/Physics/PhysicsImpulse.h` → `Entity::ITimerOwner`(+`timer/multiple_timer.h` 직접포함=transitive 손실 보강). Task 4(C2): `InputHandler/PlayerController` → `ITimerOwner`/`IImpulseState`/`IPlayerCommand`/`IFireTrigger` 라우팅 + **`PlayerEntity:IPlayerCommand`·`PlayerHands:IFireTrigger` 부착(override)** + 죽은 Entity/ include 4종 제거. 절단 증명: `grep '#include "Entity/"' {Physics,InputHandler,Spawns,Playable}` = const-noise(`UltimateLaser.cpp`→`apps/_MyApp_/src/Entity/Constants.h`) 외 0. **남은 것 = Task 5(통합검증) + 사용자 path-scoped 커밋(6파일).**
- 2026-06-11 (3) — 클라 ④ **Task 1 구현·커밋 `f98fc81`** (시임→Contracts/, C4 절단, 빌드 GREEN). 발견 정정: 소비처 14파일(3 include 형태) · override 함정. 클라 plan §0.5 핸드오프 진입점 신설. 다음 = Task 2(C3).
- 2026-06-11 (2) — 4문서 렌즈 전수조사: F1~F6. 착수 순서 = 보류(계획만 더). F1/F2/F5/F3 각 plan 노트 보강.
- 2026-06-11 — 최초 작성. 4후보(엔진 ①②③ + 클라 ④) 통합 인덱스. 엔진 plan + 클라 plan + 엔진 spec + 핸드오프 연결. 10 mutual→0 목표·독립 2트랙·통합 검증·열린 결정(클라 D-4) 정리.
