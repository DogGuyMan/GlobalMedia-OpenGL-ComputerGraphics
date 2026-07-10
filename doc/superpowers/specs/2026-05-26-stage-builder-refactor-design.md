# Stage Builder 리팩토링 — 스테이지 형성 로직 응집 (M4 선행)

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

**날짜**: 2026-05-26
**대상 데모**: `apps/_MyApp_/` (탑다운 슈터)
**관련 모듈**: `apps/_MyApp_/src/Stage/`, `apps/_MyApp_/src/Physics/`, `src/resource_registry/`
**선행 조건**: 없음 — M4 (PlayerStateMachine + Projectile + Enemy) 의 *선행* 작업
**후속 작업**: M4 본격 + M7 Stage FSM 활성화

---

## 1. 동기

현재 `apps/_MyApp_/main.cpp` 의 `startup()` 이 *씬 형성* 책임 (plane mesh / wall + pickup material / 벽 4개 / Pickup Sensor 1개) 을 직접 수행한다. 이로 인해:

- main.cpp 가 30+ 줄의 boilerplate 보유 — Player/Camera/Physics 시스템 셋업과 *섞임*.
- `<apps>/_MyApp_/src/Physics/wall_factory.h` / `pickup_factory.h` 가 *물리 모듈* 디렉토리에 있지만 실제로는 *스테이지 형성 전용* — 의미 경계 불일치.
- 향후 M4 Enemy spawn / M7 Stage FSM (Title/Combat/Boss) 진입 시 *추가 형성 로직* 의 거주 위치 부재.

`apps/_MyApp_/src/Stage/` 디렉토리가 이미 존재 (Stage.h + State/ stub 3개) 하지만 빈 껍데기. 이번 리팩토링이 그 경계를 *실제로* 채운다.

## 2. 핵심 결정 5종 (brainstorming Q&A 결과)

| # | 결정 | 채택 | 거부 |
|---|---|---|---|
| D1 | Stage 의 형태 | **Builder 자유함수 + `Components::StageState`** (옵션 C) | Actor 상속 (현재 `Stage.h` 모양) — `compound_actor_pattern` 컨벤션 위반; Component 단일 흡수 (옵션 B) — Component 가 비대 |
| D2 | factory 정리 | **`Stage/Factories/` 하위 분리 유지** (옵션 B) | StageBuilder.cpp 인라인 (옵션 A) — M4 추가 spawn factory 응집 어려움; 하이브리드 (옵션 C) |
| D3 | StageConfig 시그니처 | **Stage Builder 가 mesh/material 전부 생성** (옵션 A) | main.cpp 가 재료 전달 (옵션 B) — boilerplate 잔존; 하이브리드 (옵션 C) |
| D4 | Plane Mesh owner | **기존 `ResourceRegistry::RegisterMesh` 사용** (옵션 A1, 정정) | Stage Actor 자체 보유 (옵션 A2); file-static UPtr (옵션 A3) — leak 외관 + TU init 순서 위험 |

> **정정 (2026-05-26 plan 작성 직전)**: 본 spec 초안은 `SJH::ResourceRegistry::CreateMesh` 신규 도입을 가정했으나, plan 작성 단계에서 [src/resource_registry/resource_registry.h:100](../../src/resource_registry/resource_registry.h#L100) 의 `RegisterMesh(key, MeshUPtr)` 가 *이미 존재* 함을 확인. 헤더 §13 line 12 가 *동사 계약* 으로 `Register*` 를 "외부에서 만든 자원의 소유권 이전 (Mesh 처럼 Create 와 별개 factory 가 다수일 때)" 명시. 따라서 §5 코어 API 확장은 폐기 — Stage Builder 가 `SJH::Mesh::CreatePlane()` 직접 호출 후 `RegisterMesh("stage_plane", std::move(planeUPtr))` 한 줄로 위탁. CLAUDE.md §167 의 "Program / Mesh 는 미지원" 표기 자체가 outdated — 별건으로 후속 정정 (§10 참조).
| D5 | `EStageStatus` / FSM 통합 | **EStageStatus 멤버만 도입, FSM 활성은 후속** (옵션 B) | 이번에 FSM 완전 활성 (옵션 A) — Title/Combat/Boss 동작 미정의; StageState Component 미도입 (옵션 C) — M4 진입 시 EStageStatus 다시 도입 부담 |

배경: 사용자는 *"스테이지 형성 로직이 `Stage.h` 의 책임이 되도록"* 명시 — Builder + Component 분리가 *Actor 비상속* 컨벤션과 *형성 책임 응집* 둘 다 충족하는 유일한 형태.

## 3. 모듈 레이아웃

### 신설/이동 파일

```
apps/_MyApp_/src/Stage/
  CMakeLists.txt                   # 신설 — MyApp::Stage STATIC 라이브러리 (Physics/CMakeLists.txt 패턴 따름)
  Stage.h                          # EStageStatus enum 만 (기존 stub 정리)
  StageConfig.h                    # PoD: b2World*, ResourceRegistry*, arena/wall/pickup 파라미터
  StageBuilder.h                   # CreateStageActor(StageConfig) 선언
  StageBuilder.cpp                 # CreateStageActor 구현 (Composition Root)
  Components/
    StageStateComponent.h          # Components::StageState — EStageStatus current_ 보유 (Component)
  Factories/
    wall_factory.h                 # ← <Physics>/wall_factory.h 에서 이동 (네임스페이스 변경)
    pickup_factory.h               # ← apps/_MyApp_/src/Bootstrap/pickup_factory.h 에서 이동 (PickupTriggerLogger 동반)
  State/                           # 기존 stub 3개 — 이번 변경 없음
    StageFSMState.h
    StageStateMachine.h
    StageState.Impl.h              # 빈 파일 — M4 또는 M7 에서 채움
```

### CMake wire 갱신

```
apps/_MyApp_/src/CMakeLists.txt    # add_subdirectory(Stage) 추가 + myapp_client INTERFACE 에 MyApp::Stage 합류
```

> Stage 디렉토리는 *현재 빌드에 미합류* 상태 — Stage/CMakeLists.txt 자체 부재. 본 리팩토링이 Stage 의 *첫 활성화*.

### 삭제 파일

```
<apps>/_MyApp_/src/Physics/wall_factory.h     # Stage/Factories/ 로 이동
apps/_MyApp_/src/Bootstrap/pickup_factory.h   # Stage/Factories/ 로 이동
```

### 유지 파일

- `<apps>/_MyApp_/src/Physics/filter.h` — `PhysicsLayer` enum 은 *전역* (Player/Enemy/Bullet 모두 사용) 이라 Physics/ 그대로.
- `apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h` — `Components::BoxBody/CircleBody` 도 전역.

### 네임스페이스 정리

- 기존 `TopdownShooter::Physics::CreateWallActor` → `TopdownShooter::Stage::Factories::CreateWallActor`
- 기존 `TopdownShooter::Physics::CreatePickupActor` → `TopdownShooter::Stage::Factories::CreatePickupActor`
- 기존 `TopdownShooter::Physics::PickupTriggerLogger` → `TopdownShooter::Stage::Factories::PickupTriggerLogger`
- 신설 `TopdownShooter::Stage::CreateStageActor`, `TopdownShooter::Stage::StageConfig`, `TopdownShooter::Stage::EStageStatus`
- 신설 `TopdownShooter::Stage::Components::StageState`

## 4. 책임 경계

| 책임 | 위치 | 비고 |
|---|---|---|
| arena 형성 (walls 4개 + Pickup Sensor) | `Stage::CreateStageActor()` | Composition Root |
| 벽 1개 생성 (b2Body + Components::BoxBody + MeshRenderer) | `<Stage>/Factories/wall_factory.h` (`CreateWallActor`) | 위치만 이동, 시그니처 유지 |
| pickup sensor 생성 (b2Body + PickupTriggerLogger + MeshRenderer) | `apps/_MyApp_/src/Bootstrap/pickup_factory.h` (`CreatePickupActor`) | 위치만 이동, 시그니처 유지 |
| `PickupTriggerLogger` Component | `apps/_MyApp_/src/Bootstrap/pickup_factory.h` 안 (현재와 동일) | factory 와 한 파일 — 재사용 가능성 낮음 |
| `EStageStatus` (Title/Combat/Boss) 보유 | `Stage::Components::StageState` (Component) | setter/getter 만, FSM 미통합 |
| plane mesh 생성 + owner | `SJH::Mesh::CreatePlane()` → `SJH::ResourceRegistry::RegisterMesh("stage_plane", std::move(planeUPtr))` | 기존 API (코어 변경 없음) |
| wallMat / pickupMat 생성 + owner | `SJH::ResourceRegistry::CreateSharedMaterial("stage_wall" / "stage_pickup")` | 기존 API |
| simple.vs/fs Program | `SJH::ResourceRegistry::CreateProgram("stage_solid_plane", ...)` | 기존 API, key 만 stage_ prefix |
| `b2World` lifecycle | `main.cpp` 의 `PhysicsSystem` (변경 없음) | Stage 는 raw ptr 만 |

## 5. 코어 API — 변경 없음 (기존 `RegisterMesh` 사용)

> **본 섹션은 초안에서 제거됨.** [src/resource_registry/resource_registry.h:100](../../src/resource_registry/resource_registry.h#L100) 의 `RegisterMesh(key, MeshUPtr)` + [:103](../../src/resource_registry/resource_registry.h#L103) 의 `FindMesh(key)` 가 이미 헤더에 정착 (Texture/Program/Material 과 동등 캐시 + `Clear()` 도 mMeshes clear 처리 완료). Stage Builder 는 한 줄로 위탁:

```cpp
// <Stage>/StageBuilder.cpp 안
auto planeUPtr = SJH::Mesh::CreatePlane();
SJH::Mesh* planeRaw = cfg.registry->RegisterMesh("stage_plane", std::move(planeUPtr));
// 이후 wall/pickup factory 가 planeRaw 를 MeshRenderer 에 주입
```

> CLAUDE.md §167 의 "Program / Mesh 는 미지원" 표기는 **outdated** — 별건으로 후속 정정 (§10 참조). 본 plan 범위 외.

## 6. `StageConfig` + 호출 시그니처

```cpp
// apps/_MyApp_/src/Stage/Stage.h — EStageStatus 만 정의 (기존 stub Stage 클래스는 제거)
namespace TopdownShooter::Stage {
    enum class EStageStatus : uint64_t {
        Title  = 1ull << 0,
        Combat = 1ull << 1,
        Boss   = 1ull << 2,
    };
}
```

```cpp
// apps/_MyApp_/src/Stage/StageConfig.h
namespace TopdownShooter::Stage {
    struct StageConfig {
        b2World*               world           = nullptr;   // 필수 — null 시 abort
        SJH::ResourceRegistry* registry        = nullptr;   // 필수 — null 시 abort
        float                  arenaHalfExtent = 10.0f;     // 벽 안쪽 절반 크기
        float                  wallThickness   = 0.5f;
        std::vector<vmath::vec2> pickupPositions = { vmath::vec2(0.0f, 3.0f) };
        EStageStatus           startStatus     = EStageStatus::Title;
    };
}
```

```cpp
// <Stage>/StageBuilder.h
namespace TopdownShooter::Stage {
    /// @brief Stage Actor = walls + pickups + StageState Component 가 child/component 로 매단 일반 Actor.
    ///        plane mesh 와 material 은 ResourceRegistry 에 자동 등록 (key: stage_plane / stage_wall / stage_pickup).
    /// @param cfg StageConfig — world + registry 필수, 나머지는 default.
    /// @return Stage Actor (호출자가 Director::Root().AddChild 로 위탁)
    std::unique_ptr<SJH::Scene::Actor> CreateStageActor(const StageConfig& cfg);
}
```

```cpp
// apps/_MyApp_/src/Stage/Components/StageStateComponent.h
namespace TopdownShooter::Stage::Components {
    class StageState : public SJH::Scene::Component {
    public:
        EStageStatus Current() const { return current_; }
        void SetCurrent(EStageStatus s) { current_ = s; }
    private:
        EStageStatus current_ = EStageStatus::Title;
    };
}
```

## 7. main.cpp 변경 (대규모 축소)

### 제거 대상 멤버

- `SJH::MeshUPtr mPlane;` — Stage 가 registry 에 등록한 stage_plane 사용
- 기존 startup() 안 변수 — `solidProg`, `wallMat`, `pickupMat`, `spawnWall` 람다, walls 4 호출, Pickup 1 호출

### 추가 호출 (한 줄)

```cpp
// startup() 안 — Physics::Init 직후
auto stage = TopdownShooter::Stage::CreateStageActor({
    .world    = &mPhysics.World(),
    .registry = &reg,
});
dir.Root().AddChild(std::move(stage));
```

### 보유 유지

- `PlayerActor` 셋업 (M3 도메인 — 별개)
- `mPhysics`, `mKeyboard`, `mMouse`, `mCamera*`, `mSprite*` 등 (Stage 와 무관)

## 8. 작업 순서 (PR 1개 = 1 commit)

> 초안의 2 commit 분할 (commit A `CreateMesh` 확장) 은 §5 정정으로 폐기. 단일 commit 으로 통합.

| 단계 | 작업 |
|---|---|
| T1 | `<Stage>/Factories/wall_factory.h` 생성 — `<Physics>/wall_factory.h` 내용 + 네임스페이스 변경 |
| T2 | `apps/_MyApp_/src/Bootstrap/pickup_factory.h` 생성 — `apps/_MyApp_/src/Bootstrap/pickup_factory.h` 내용 + 네임스페이스 변경 (PickupTriggerLogger 포함) |
| T3 | `apps/_MyApp_/src/Stage/Stage.h` 갱신 — EStageStatus enum 만 (기존 Stage 클래스 제거) |
| T4 | `apps/_MyApp_/src/Stage/StageConfig.h` 신설 |
| T5 | `apps/_MyApp_/src/Stage/Components/StageStateComponent.h` 신설 |
| T6 | `<Stage>/StageBuilder.h` + `StageBuilder.cpp` 신설 |
| T7 | `Stage/CMakeLists.txt` 신설 — `MyApp::Stage` STATIC (`Physics/CMakeLists.txt` 패턴 따름) |
| T8 | `apps/_MyApp_/src/CMakeLists.txt` 갱신 — `add_subdirectory(Stage)` + `myapp_client` INTERFACE 에 `MyApp::Stage` 합류 |
| T9 | `apps/_MyApp_/main.cpp` 축소 — `#include "<Stage>/StageBuilder.h"`, mPlane / wall/pickup / wallMat/pickupMat/solidProg 셋업 제거, `CreateStageActor` 1 호출로 치환 |
| T10 | `<Physics>/wall_factory.h` 삭제, `apps/_MyApp_/src/Bootstrap/pickup_factory.h` 삭제 |
| T11 | 빌드 + 시각 회귀 검증 (사용자 직접 — 게임 실행 후 wall 4개 + pickup + Player WASD 동작 확인) |

빌드/실행 명령:
```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

## 9. 시각 회귀 체크리스트 (사용자 검증)

- [ ] arena 4 벽 표시 (회색, 10×10 단위)
- [ ] Pickup Sensor 노란색 사각형 (0, +3 위치)
- [ ] Player 캐릭터 sprite 정상 (TestPattern atlas, 16-frame loop)
- [ ] Player WASD 이동 (벽 충돌 시 정지)
- [ ] Pickup Sensor 진입 시 `[Pickup] OnTriggerEnter` spdlog 출력
- [ ] Camera follow 정상 (Player 추적)
- [ ] 변경 *전* 과 동일 결과 — 행위 동등성

## 10. 비범위 (Out of Scope)

이 spec 은 다음을 *포함하지 않음*:

| 항목 | 이유 / 후속 위치 |
|---|---|
| `Stage/State/` FSM 활성화 (TitleState/CombatState/BossState 구현) | D5 결정 — 후속 작업 (M4 또는 M7). 현재 stub 유지 |
| Enemy/Bullet spawn 책임 Stage 흡수 | M4 작업. spawn 책임이 Stage 또는 Spawns/ 어디로 갈지는 M4 brainstorming |
| `Stage` 가 `Director::ActiveStage` 로 등록되는 시스템 | 후속 — 현재는 main.cpp 가 stage Actor 의 라이프타임 owner (Director Root 의 child) |
| `apps/_MyApp_/src/Stage/Stage.h` 의 기존 Stage *클래스* (Actor 상속) | 제거 — D1 결정. 사용자 IDE 가 열어둔 `StageState.Impl.h` 와 함께 후속 작업 시 다시 의미 부여 |
| 단위 테스트 추가 | memory `no_auto_tests.md` |
| spawnable 패턴 일반화 (Stage/Spawns/ 합치기 등) | M4 진입 시 결정 |
| CLAUDE.md §167 "Program / Mesh 는 미지원" 표기 정정 (현재 outdated) | 별건 — CreateProgram + RegisterMesh 둘 다 이미 정착. 본 plan 후 별도 CLAUDE.md 갱신 PR |

## 11. 제약 (memory + CLAUDE.md 컨벤션 준수)

| 제약 | 출처 | 적용 |
|---|---|---|
| Actor 비상속 — 특수 속성은 Component 로만 | memory `compound_actor_pattern` | Stage 도 동일 — `CreateStageActor` Builder + `Components::StageState` |
| 단위 테스트 자발 추가 금지 | memory `no_auto_tests.md` | commit A 의 CreateMesh 도 테스트 없음 |
| stb_image 단일 owner | memory `stb_image_owner_resource_registry` | Stage 코드는 stbi_* 직접 호출 없음 — 영향 0 |
| 주석 한국어 | CLAUDE.md Conventions | 신규 파일 모두 한국어 주석 |
| 헤더 가드 `__XXX_H__` 형식 | CLAUDE.md Conventions | `__MYAPP_STAGE_STAGE_BUILDER_H__` 류 |
| `windows.h` 분기 | CLAUDE.md 크로스 플랫폼 | Stage 코드는 windows.h 미사용 — 영향 0 |

---

## 부록 A — 자가 검증 (spec self-review)

- [x] **Placeholder scan** — TBD/TODO 없음. arena/wallThickness/pickupPositions default 값 명시.
- [x] **Internal consistency** — D1~D5 결정과 §3~§7 의 코드 시그니처 일치. 모듈 레이아웃 (§3) 과 책임 경계 (§4) 매핑 1:1.
- [x] **Scope check** — commit A + commit B 두 단계로 PR 1개 안에 완결. M4/M7 후속은 §10 으로 분리.
- [x] **Ambiguity check** — `StageConfig` 의 `world` / `registry` null 시 정책 = abort 명시. `CreateMesh` idempotent 명시.
