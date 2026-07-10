# 핸드오프 — JSON DoD Plan 1A 실행 (다른 Claude Agent 위탁)

- **날짜**: 2026-06-19
- **유형**: Artifact A (자립형 실행 프롬프트). 1개 묶음 작업 = Plan 1A(인프라 DataTable + Stage cutover) 실행.
- **정본 플랜**: `doc/superpowers/plans/2026-06-19-json-dod-system-plan1a-infra-stage.md` (Task 1~4 전체 코드 포함).
- **사용법**: 아래 ``` 펜스 블록을 통째 복사 → 새 Claude Code 세션에 붙여넣기. 펜스 밖 "Notes"는 오케스트레이터(너/사용자)용.

---

```
[ROLE]
너는 C++17 OpenGL 게임 프로젝트(/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics,
브랜치 game/slang-phase2-ubo)에서 "JSON DoD 시스템 Plan 1A"를 실행하는 구현 엔지니어다.
목표(한 문장): 게임 튜닝 상수를 런타임 JSON 으로 외부화하는 인프라(엔진 제네릭 DataTable 자원 + 로더)를
세우고 Stage 도메인 한 종을 끝까지 cutover 해 "재컴파일 없이 stage.json 수정 -> 게임 동작 변경"을 증명한다.

권위 있는 작업 목록 = `doc/superpowers/plans/2026-06-19-json-dod-system-plan1a-infra-stage.md`.
이 플랜의 Task 1~4 를 순서대로 따르라. 각 Task 에 **완전한 코드**가 들어있으니 그대로 적용한다.
아래 [Verified facts]/[Boundaries]/[Verify] 는 플랜을 안 읽어도 안전하게 시작하도록 핵심만 추린 것이다.

[Hard rules]
- 빌드 검증: `cmake --build --preset ninja --target _MyApp_` (exit 0 = 성공).
- 테스트: 단위테스트 작성 금지(프로젝트 no_auto_tests 규칙). 검증은 **빌드 + 실행 + 관찰**로만.
- 커밋: **하지 마라.** 구현 + 검증 + 보고까지만. 커밋은 사용자가 직접 한다(사용자가 같은 워킹트리에서
  병렬 git 작업 중). 만약 사용자가 명시적으로 커밋을 지시하면 반드시 path-scoped(`git commit <정확한 경로들>`,
  `git add -A` 금지) + 커밋메시지에 `Co-Authored-By` 트레일러 미사용.
- 코드 컨벤션: 주석은 한국어 + Doxygen 스타일(src/render 참조), ASCII/한글만(특수문자 0).
  명명: 멤버 mPascalCase / 지역 camelCase / 타입·함수 PascalCase / bool mIs* / 포인터 *Ptr.
  헤더가드 형식 __SCOPE_NAME_H__ (대문자+언더스코어, #pragma once 미사용).
- 빌드는 사용자가 직접 돌릴 수도 있다. 빌드 실패 시 추측 말고 에러 그대로 보고.

[Verified facts] (2026-06-19 재측정 - 단, 사용자 병렬 편집으로 표류 가능하니 시작 시 git status/해당 파일 재확인)
- nlohmann/json 은 이미 vcpkg 등록 + game_deps 합류: cmake/Dependency.cmake:131 (find_package) + :163.
  -> 추가 벤더링/별도 link 불필요. `#include <nlohmann/json.hpp>` 만.
- src/resource_registry 는 game_deps 를 PUBLIC link (CMakeLists.txt) -> nlohmann 헤더 가용.
- ResourceRegistry = Meyer 싱글톤 `SJH::ResourceRegistry::Get()`. 자원은 `std::unordered_map<std::string, XUPtr>`
  멤버(resource_registry.h:178-187, mEffects 가 마지막) + Create*/Find* 메소드 + Clear()(약 305-319, mEffects.clear() 마지막).
  CLASS_PTR(X) 매크로(common/common.h)가 XUPtr=unique_ptr 별칭 생성.
- WaveController.h:68 ctor 현재: `WaveController(b2World* world, SJH::Scene::Actor* spawnParent,
  SJH::Scene::Actor* playerActor, float arenaHalfExtent);`. WaveController.cpp:37-42 ctor 정의,
  :41 `mSpawnTimer(WAVE_SPAWN_INTERVAL)`, :114-116 웨이브 공식(WAVE_HP_BASE 등), :156 `LiveCount() < WAVE_MAX_ENEMIES`.
- StageConfig.h:41-48 struct(world, registry, arenaHalfExtent=ARENA_HALF_EXTENT, wallThickness, pickupPositions, startStatus).
- main.cpp:117 OnSceneSetup(), :119 `auto &reg = SJH::ResourceRegistry::Get();`.
  WaveController 생성 + CreateStageActor 호출 위치는 main.cpp 가 InitScheduler 로 리팩토링돼 줄번호 유동 ->
  `grep -n "AddComponent<Stage::WaveController>\|CreateStageActor\|StageConfig" apps/_MyApp_/main.cpp` 로 찾아라.
- 신규 예정(현재 없음 확인됨): src/resource_registry/datatable.{h,cpp}, apps/_MyApp_/src/Stage/StageData.h,
  apps/_MyApp_/resources/data/{stage.json, schema/stage.schema.json}.

[STEPS]
플랜 파일의 Task 1~4 를 그대로 실행:
- Task 1: 엔진 제네릭 DataTable 자원 — datatable.h(nlohmann::json 홀더 + static Load + template As<T>) + datatable.cpp
  (Load: ifstream 바이너리 + json::parse(allow_exceptions=false) + 실패 시 nullptr+spdlog) + resource_registry.h/.cpp
  (CreateDataTable/FindDataTable + mDataTables 멤버 + Clear) + CMakeLists.txt(datatable.cpp). 빌드.
- Task 2: 클라 StageData.h — struct(멤버 기본값 = Stage:: constexpr) + from_json(`s.X = j.value("x", s.X)` 폴백) +
  LoadStageData(reg) 헬퍼(find-or-create DataTable -> As<StageData>). (헤더온리, Task 4 와 함께 컴파일 검증.)
- Task 3: 데이터 파일 — resources/data/schema/stage.schema.json + resources/data/stage.json(현 constexpr 와 동일값).
- Task 4: Stage cutover — WaveController ctor 를 `const StageData&` 주입으로 변경(mData 멤버 추가, mSpawnTimer(data.WaveSpawnInterval),
  WAVE_* -> mData.*), main.cpp OnSceneSetup 에서 LoadStageData + StageConfig.arenaHalfExtent/wallThickness 주입 +
  WaveController 생성 인자 교체. 빌드 + 실행 + 데이터주도 검증.
각 Task 의 정확한 코드 블록은 플랜 파일에 있다 - 그대로 사용(요약으로 임의 작성 금지).

[Boundaries]
- OWN(수정 허용): src/resource_registry/{datatable.h,datatable.cpp,resource_registry.h,resource_registry.cpp,CMakeLists.txt},
  apps/_MyApp_/src/Stage/{StageData.h(신규),WaveController.h,WaveController.cpp},
  apps/_MyApp_/main.cpp(단 Stage 관련 배선부만 - StageData 로드 + WaveController/StageConfig 인자),
  apps/_MyApp_/resources/data/**(신규).
- NEVER TOUCH:
  * Slang/셰이더/render 관련 일체 (사용자가 이 브랜치에서 Slang 작업 병렬 진행 중 - 최근 커밋 c0388b3).
    apps/_MyApp_/shaders_slang/, scripts/slang_compile.py, src/render*, src/program/ 등 비접근.
  * 다른 도메인 Constants/사용처: Entity/Physics/Bootstrap/HUD/Audio/Playable/VFX = Plan 1B/1C 몫. 손대지 마라.
  * main.cpp 의 Stage 무관 영역(PostFX/UI/VFX/Camera 등).
- 사용자 병렬 작업과 충돌 회피: 커밋 안 함(위 Hard rules). 빌드 산출(build_ninja/) 편집은 검증용 임시만.

[Verify] (done & correct 의 정의)
1. `cmake --build --preset ninja --target _MyApp_` -> exit 0, 에러 0. (WAVE_*/ARENA_HALF_EXTENT 미정의 에러 없음 = cutover 완료.)
2. `cd build_ninja/apps/_MyApp_ && ./_MyApp_` -> 시작 로그에
   `[StageData] spawnInterval=3.00 maxEnemies=5 hpBase=20 arenaHalf=10.0` 출력. 게임이 기존과 동일하게 웨이브 스폰.
3. **데이터주도 증명(재컴파일 없음)**: `build_ninja/apps/_MyApp_/resources/data/stage.json` 의
   waveSpawnInterval 3.0->0.5, waveMaxEnemies 5->12 로 수정 후 `./_MyApp_` 재실행(빌드 X) ->
   로그 `spawnInterval=0.50 maxEnemies=12`, 적이 눈에 띄게 빨리/많이 스폰. 확인 후 stage.json 원복.
   (이 3번이 Plan 1 의 핵심 DoD - 반드시 수행/보고.)

[Self-review] (보고 전 체크)
- `grep -n "WAVE_\|ARENA_HALF_EXTENT" apps/_MyApp_/src/Stage/WaveController.cpp` -> 0 건(전부 cutover)?
- datatable.h 헤더가드/네임스페이스(SJH)/CLASS_PTR 적용? As<T> 템플릿이 헤더에 있어 클라가 인스턴스화?
- StageData from_json 이 j.value 폴백 패턴(누락 필드 = constexpr 기본값)?
- 엔진 DataTable 이 클라 타입(StageData)을 모르는 채로 유지(엔진->클라 의존 0)?
- Boundaries 위반 파일 수정 없었나?

[Report] 구조화 상태로 보고:
- 상태: DONE / DONE_WITH_CONCERNS / BLOCKED
- 변경 파일 목록(경로)
- [Verify] 1/2/3 각 결과(실제 로그 인용 - 특히 3번 데이터주도 증명)
- 보류/미결/우려 사항
- 커밋은 하지 않았음을 명시(사용자가 커밋).
```

---

## Notes (오케스트레이터용 - 펜스 밖)

- **브랜치**: `game/slang-phase2-ubo`. 사용자가 이 브랜치에서 Slang/render 리팩토링을 병렬 진행 중(최근 `c0388b3`). 수신 에이전트는 **혼자가 아님** - 그래서 커밋 금지 + Slang/render 비접근을 박았다.
- **권장 커밋(사용자 직접, path-scoped, Co-Authored-By 없음)**:
  - Task1: `git commit src/resource_registry/datatable.h src/resource_registry/datatable.cpp src/resource_registry/resource_registry.h src/resource_registry/resource_registry.cpp src/resource_registry/CMakeLists.txt -m "[feat] ResourceRegistry 제네릭 DataTable(JSON 마스터데이터) 자원"`
  - Task3: `git commit apps/_MyApp_/resources/data/schema/stage.schema.json apps/_MyApp_/resources/data/stage.json -m "[feat] Stage 마스터데이터 JSON + 스키마"`
  - Task4: `git commit apps/_MyApp_/src/Stage/StageData.h apps/_MyApp_/src/Stage/WaveController.h apps/_MyApp_/src/Stage/WaveController.cpp apps/_MyApp_/main.cpp -m "[feat] Stage cutover - WaveController 가 StageData 주입 소비"`
- **벤치마크 검증됨**: Plan 1A 코어는 BakingSheet 대비 누락 must-have 0(`doc/webeditor/06-bakingsheet-datatable-benchmark.md`). 1A 설계 무수정으로 실행해도 안전.
- **후속**: 1A GREEN 후 -> Plan 1B(나머지 Tier A 5종 + 벤치마크 채택분: TableData/MasterData/from_json ADL/char16_t) -> 1C(cross-ref/Validate) -> Plan 2(WebEditor).
- **재개 단일 진입점(메모리)**: `webeditor-master-data-effort` 메모리 + 정본 스펙 `doc/superpowers/specs/2026-06-19-webeditor-master-data-foundation-design.md`.
