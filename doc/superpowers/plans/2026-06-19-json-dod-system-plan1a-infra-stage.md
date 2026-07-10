# JSON DoD 시스템 — Plan 1A (인프라 + Stage 파일럿) 구현 플랜

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 게임 튜닝 상수를 런타임 JSON 으로 외부화하는 *인프라*(제네릭 DataTable 자원 + 로더)를 세우고, **Stage 도메인 한 종**을 끝에서 끝까지 cutover 해 "재컴파일 없이 JSON 수정 → 게임 동작 변경"을 증명한다.

**Architecture:** 엔진 측 `SJH::ResourceRegistry` 에 *타입 비의존* 제네릭 `DataTable`(= 검증된 `nlohmann::json` 문서 홀더)를 추가한다. 클라이언트 측은 타입드 `StageData` struct + `from_json`(ADL)로 그 JSON 을 자기 타입으로 역직렬화한다(엔진→클라 타입 의존 0). `from_json` 은 `j.value(field, 기본값)` 패턴이라 누락 필드는 기존 `constexpr` 기본값으로 폴백(관대한 파서). Stage 의 소비처(`WaveController` ctor)를 `StageData` 주입으로 cutover.

**Tech Stack:** C++17, CMake(ninja preset), nlohmann/json(이미 vcpkg `find_package` + `game_deps`), spdlog, ResourceRegistry(Meyer's singleton).

---

## 이 플랜의 위치 (Plan 1 시퀀스)

- **Plan 1A (이 문서)** = 인프라(DataTable 자원 + 로더) + **Stage 파일럿** cutover. 인프라/패턴을 한 도메인으로 검증.
- **Plan 1B (후속)** = 나머지 Tier A 5종(Entity/Physics/Bootstrap/HUD/Audio) — 1A 패턴 복제.
- **Plan 1C (후속)** = Tier B(Playable 스프라이트 테이블, VFX) — 중첩배열/특수타입.
- **Plan 2 (한참 후)** = WebEditor. Plan 1 전체 동작 보장 후.

정본 스펙: [doc/superpowers/specs/2026-06-19-webeditor-master-data-foundation-design.md](../specs/2026-06-19-webeditor-master-data-foundation-design.md) §0.

**벤치마크 검증 (2026-06-19)**: [doc/webeditor/06-bakingsheet-datatable-benchmark.md](../../webeditor/06-bakingsheet-datatable-benchmark.md) — BakingSheet(C#/Unity) 대비 다중에이전트 분석 결과, **본 Plan 1A 코어(제네릭 json `DataTable` + 타입별 명시 `from_json`)가 검증됨 — 누락 must-have 0**. C# 리플렉션 컬럼매핑의 C++ 등가가 정확히 명시 `from_json` 이라 1A 설계 **무수정**. 추가 개념(컬렉션/cross-ref/Validate)은 1B/1C 로 매핑(아래 후속 참조).

## 가드레일 (프로젝트 규칙 — 엄수)

- **검증 = 빌드 + 실행 + 관찰** (프로젝트 `no_auto_tests` 규칙 — 단위테스트는 사용자 요청 시에만 추가). TDD red-green 강제 안 함.
- **커밋 = path-scoped** `git commit <정확한 경로들>` (사용자가 같은 워킹트리에서 병렬 작업 — `git add -A`/인덱스 전체 커밋 금지). **`Co-Authored-By` 트레일러 미사용.** 커밋은 사용자 승인 후 또는 사용자가 직접 — 임의 커밋 금지.
- **빌드는 사용자가 직접 하거나 명시 커맨드로**: `cmake --build --preset ninja --target _MyApp_`.
- 주석은 한국어 + Doxygen 스타일(`src/render` 컨벤션), ASCII/한글만(특수문자 0).
- 명명: 멤버 `mPascalCase` / 지역 `camelCase` / 타입·함수 `PascalCase`.
- `apps/_MyApp_/src/Stage/` 와 `src/resource_registry/` 외 파일 비접근(Stage 파일럿 범위).

## File Structure

| 파일 | 책임 | 상태 |
|---|---|---|
| `<src>/resource_registry/datatable.h` | 제네릭 `DataTable` 자원 — `nlohmann::json` 문서 홀더 + `Load(path)` + `As<T>()` | 신규 |
| `<src>/resource_registry/datatable.cpp` | `DataTable::Load` 구현(파일 읽기 + 파싱 + 관대한 실패) | 신규 |
| `src/resource_registry/resource_registry.h` | `CreateDataTable`/`FindDataTable` 선언 + `mDataTables` 멤버 + include | 수정 |
| `src/resource_registry/resource_registry.cpp` | 두 메소드 구현 + `Clear()` 에 `mDataTables.clear()` | 수정 |
| `src/resource_registry/CMakeLists.txt` | `datatable.cpp` 소스 추가 | 수정 |
| `<apps>/_MyApp_/src/Stage/StageData.h` | 타입드 `StageData` struct + `from_json` + `LoadStageData` 헬퍼 | 신규 |
| `<apps>/_MyApp_/resources/data/schema/stage.schema.json` | Stage 스키마(JSON Schema 부분집합) | 신규 |
| `<apps>/_MyApp_/resources/data/stage.json` | Stage 값 | 신규 |
| `apps/_MyApp_/src/Stage/WaveController.h` | ctor 를 `const StageData&` 주입으로 변경 + `mData` 멤버 | 수정 |
| `apps/_MyApp_/src/Stage/WaveController.cpp` | `WAVE_*` 상수 참조 → `mData.*` 로 cutover | 수정 |
| `apps/_MyApp_/main.cpp` | `OnSceneSetup` 에서 StageData 로드 + WaveController/StageConfig 배선 | 수정 |

---

## Task 1: 엔진 — 제네릭 DataTable 자원

**Files:**
- Create: `<src>/resource_registry/datatable.h`
- Create: `<src>/resource_registry/datatable.cpp`
- Modify: `src/resource_registry/resource_registry.h` (include + 메소드 선언 + 멤버)
- Modify: `src/resource_registry/resource_registry.cpp` (구현 + Clear)
- Modify: `src/resource_registry/CMakeLists.txt` (소스 추가)

- [ ] **Step 1: `datatable.h` 작성**

```cpp
/**
 * @file datatable.h
 * @brief 타입 비의존 마스터데이터 자원 - 검증된 JSON 문서 1개를 보유하고 As<T> 로 역직렬화.
 *
 * @details
 *  ### 책임
 *  - 파일에서 JSON 을 로드해 nlohmann::json 으로 보유 (논리 키로 ResourceRegistry 캐시).
 *  - As<T>() 로 호출자(클라이언트) 타입으로 역직렬화 - 엔진은 클라 타입을 모른다(템플릿이라 호출처 인스턴스화).
 *
 *  ### 비-책임
 *  - [X] 타입드 struct 정의 - 클라이언트(예: TopdownShooter::Stage::StageData) 책임.
 *  - [X] JSON Schema 검증 - 본 자원은 관대한 파서. 스키마 대조는 WebEditor(Plan 2) 책임.
 *
 * @note 로드 실패(파일 부재/JSON 파싱 에러)는 예외 대신 nullptr 반환 + spdlog warn (게임 비중단).
 */
#ifndef __SJH_RESOURCE_REGISTRY_DATATABLE_H__
#define __SJH_RESOURCE_REGISTRY_DATATABLE_H__

#include "common/common.h"
#include <<nlohmann>/json.hpp>
#include <string>

namespace SJH
{
	CLASS_PTR(DataTable)

	/// @brief JSON 문서 1개를 보유하는 타입 비의존 마스터데이터 자원.
	class DataTable
	{
	  public:
		/// @brief 파일에서 JSON 을 로드해 DataTable 생성. 실패 시 nullptr(예외 없음).
		/// @param filename 실행 파일 디렉토리 기준 상대경로(예: <.>/resources/data/stage.json).
		static DataTableUPtr Load(const std::string &filename);

		/// @brief 보유 JSON 문서 접근 (읽기 전용).
		const nlohmann::json &Json() const { return mData; }

		/// @brief 보유 JSON 을 호출자 타입 T 로 역직렬화 (T 는 from_json(ADL) 을 제공해야 함).
		/// @details 템플릿이라 클라이언트 호출 지점에서 인스턴스화 - 엔진은 T 를 모른다.
		template <class T>
		T As() const
		{
			return mData.get<T>();
		}

	  private:
		explicit DataTable(nlohmann::json data) : mData(std::move(data)) {}

		nlohmann::json mData; ///< 로드된 JSON 문서 (owner).
	};
} // namespace SJH

#endif // __SJH_RESOURCE_REGISTRY_DATATABLE_H__
```

- [ ] **Step 2: `datatable.cpp` 작성**

```cpp
/**
 * @file datatable.cpp
 * @brief DataTable::Load 구현 - 파일 읽기 + JSON 파싱 + 관대한 실패(nullptr).
 */
#include "datatable.h"

#include <fstream>
#include <<spdlog>/spdlog.h>

namespace SJH
{
	DataTableUPtr DataTable::Load(const std::string &filename)
	{
		std::ifstream stream(filename, std::ios::binary); // 바이너리 모드 - 크로스플랫폼 규칙
		if (!stream.is_open())
		{
			spdlog::warn("[DataTable] 파일 열기 실패 - '{}' (기본값으로 폴백)", filename);
			return nullptr;
		}

		nlohmann::json data = nlohmann::json::parse(stream, nullptr, /*allow_exceptions=*/false);
		if (data.is_discarded())
		{
			spdlog::error("[DataTable] JSON 파싱 실패 - '{}' (기본값으로 폴백)", filename);
			return nullptr;
		}

		// private ctor 라 make_unique 불가 - new + raw 로 wrap.
		return DataTableUPtr(new DataTable(std::move(data)));
	}
} // namespace SJH
```

- [ ] **Step 3: `resource_registry.h` 에 include + 선언 + 멤버 추가**

`#include "sprite/uniform_atlas.h"` 등 기존 include 묶음(약 30-42행)에 한 줄 추가:

```cpp
#include "datatable.h"
```

`FindEffect` 선언 다음(약 164행, `void Clear();` 앞)에 메소드 2개 추가:

```cpp
		/// @brief 파일에서 JSON 마스터데이터를 *로드*해 @p key 로 캐시. 이미 있거나 로드 실패 시 nullptr.
		/// @details 타입 비의존 - 호출자가 FindDataTable(key)->As<T>() 로 타입드 역직렬화.
		DataTable *CreateDataTable(const std::string &key, const std::string &filename);

		/// @brief @p key 로 캐시된 DataTable *조회* (생성 안 함). 없으면 nullptr.
		DataTable *FindDataTable(const std::string &key);
```

멤버 맵 묶음(약 178-187행, `mEffects` 다음)에 추가:

```cpp
		std::unordered_map<std::string, DataTableUPtr> mDataTables; // Plan 1A - 마스터데이터 JSON
```

- [ ] **Step 4: `resource_registry.cpp` 에 구현 + Clear 추가**

`CreateEffect`/`FindEffect` 구현 근처(파일 내 다른 Create/Find 패턴 옆)에 추가:

```cpp
	DataTable *ResourceRegistry::CreateDataTable(const std::string &key, const std::string &filename)
	{
		if (mDataTables.find(key) != mDataTables.end())
		{
			spdlog::warn("CreateDataTable: 키 '{}' 가 이미 존재 - Find 를 먼저 호출하라", key);
			return nullptr;
		}
		auto table = DataTable::Load(filename);
		if (table == nullptr)
			return nullptr; // Load 가 이미 warn/error 로깅
		auto insertedIt = mDataTables.emplace(key, std::move(table)).first;
		return insertedIt->second.get();
	}

	DataTable *ResourceRegistry::FindDataTable(const std::string &key)
	{
		auto it = mDataTables.find(key);
		return (it != mDataTables.end()) ? it->second.get() : nullptr;
	}
```

`Clear()` 본문(약 305-319행)의 `mEffects.clear();` 다음 줄에 추가:

```cpp
		mDataTables.clear(); // Plan 1A
```

- [ ] **Step 5: `CMakeLists.txt` 에 소스 추가**

`src/resource_registry/CMakeLists.txt` 의 `add_library(sjhopengl_resource_registry STATIC ...)` 목록(약 1-6행)에 `effect.cpp` 다음 줄 추가:

```cmake
    datatable.cpp            # Plan 1A - 마스터데이터 JSON 자원
```

- [ ] **Step 6: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 컴파일/링크 성공(exit 0). DataTable 심볼 미해결 없음. (game_deps PUBLIC 경유 nlohmann 헤더 가용 — 별도 link 불필요.)

- [ ] **Step 7: 커밋 (path-scoped)**

```bash
git commit <src>/resource_registry/datatable.h <src>/resource_registry/datatable.cpp \
           src/resource_registry/resource_registry.h src/resource_registry/resource_registry.cpp \
           src/resource_registry/CMakeLists.txt \
           -m "[feat] ResourceRegistry 에 제네릭 DataTable(JSON 마스터데이터) 자원 추가"
```

---

## Task 2: 클라이언트 — StageData 타입드 struct + from_json

**Files:**
- Create: `<apps>/_MyApp_/src/Stage/StageData.h`

- [ ] **Step 1: `StageData.h` 작성**

`j.value("field", s.field)` 패턴 = 누락 필드는 멤버 기본값(= `Stage::` constexpr)으로 폴백 → 관대한 파서. 멤버 기본값이 `Constants.h` 를 참조하므로 단일 진실원천 유지.

```cpp
/**
 * @file StageData.h
 * @brief Stage 마스터데이터 런타임 타입 - DataTable JSON 을 역직렬화하는 타입드 struct.
 *
 * @details
 *  ### 책임
 *  - WAVE_* / ARENA_* constexpr 의 *런타임 대응물*. nlohmann from_json 으로 JSON -> struct.
 *  - 누락 필드는 멤버 기본값(= Stage::Constants.h 의 constexpr)으로 폴백 (관대한 파서).
 *  - LoadStageData 헬퍼: ResourceRegistry 에 DataTable 적재 + As<StageData> 한 호출.
 *
 * @note from_json 은 ADL 로 nlohmann 이 자동 호출 (DataTable::As<StageData> 가 트리거).
 *       엔진 SJH::DataTable 은 본 타입을 모른다 - 의존 방향 클라->엔진 단방향 유지.
 */
#ifndef __TOPDOWNSHOOTER_STAGE_STAGE_DATA_H__
#define __TOPDOWNSHOOTER_STAGE_STAGE_DATA_H__

#include "apps/_MyApp_/src/Stage/Constants.h"
#include "resource_registry/resource_registry.h"
#include <<nlohmann>/json.hpp>
#include <<spdlog>/spdlog.h>
#include <string>

namespace TopdownShooter::Stage
{
	/// @brief Stage 웨이브/아레나 튜닝값의 런타임 보유 struct. 멤버 기본값 = Constants.h constexpr.
	struct StageData
	{
		float WaveSpawnInterval = WAVE_SPAWN_INTERVAL;
		int   WaveMaxEnemies    = WAVE_MAX_ENEMIES;
		int   WaveHpBase        = WAVE_HP_BASE;
		int   WaveHpPerWave     = WAVE_HP_PER_WAVE;
		float WaveSpeedBase     = WAVE_SPEED_BASE;
		float WaveSpeedPerWave  = WAVE_SPEED_PER_WAVE;
		int   WaveContactDamage = WAVE_CONTACT_DAMAGE;
		float ArenaHalfExtent   = ARENA_HALF_EXTENT;
		float WallThickness     = WALL_THICKNESS;
	};

	/// @brief JSON -> StageData (ADL). 누락 키는 기존 멤버값(기본값) 유지 - 관대한 파서.
	inline void from_json(const nlohmann::json &j, StageData &s)
	{
		s.WaveSpawnInterval = j.value("waveSpawnInterval", s.WaveSpawnInterval);
		s.WaveMaxEnemies    = j.value("waveMaxEnemies", s.WaveMaxEnemies);
		s.WaveHpBase        = j.value("waveHpBase", s.WaveHpBase);
		s.WaveHpPerWave     = j.value("waveHpPerWave", s.WaveHpPerWave);
		s.WaveSpeedBase     = j.value("waveSpeedBase", s.WaveSpeedBase);
		s.WaveSpeedPerWave  = j.value("waveSpeedPerWave", s.WaveSpeedPerWave);
		s.WaveContactDamage = j.value("waveContactDamage", s.WaveContactDamage);
		s.ArenaHalfExtent   = j.value("arenaHalfExtent", s.ArenaHalfExtent);
		s.WallThickness     = j.value("wallThickness", s.WallThickness);
	}

	/// @brief DataTable 적재(find-or-create) 후 StageData 로 역직렬화. 실패 시 전 필드 기본값.
	/// @param reg ResourceRegistry (마스터데이터 캐시 owner).
	/// @param key 캐시 키 (기본 "stage").
	/// @param path JSON 경로 (실행 디렉토리 기준 상대).
	inline StageData LoadStageData(SJH::ResourceRegistry &reg,
	                               const std::string     &key  = "stage",
	                               const std::string     &path = "<.>/resources/data/stage.json")
	{
		StageData s; // 전 필드 = Constants.h 기본값
		SJH::DataTable *dt = reg.FindDataTable(key);
		if (dt == nullptr)
			dt = reg.CreateDataTable(key, path);
		if (dt != nullptr)
			s = dt->As<StageData>(); // from_json - 누락 필드는 위 기본값 유지
		else
			spdlog::warn("[StageData] DataTable '{}' 미적재 - 전 필드 Constants.h 기본값 사용", key);
		return s;
	}
} // namespace TopdownShooter::Stage

#endif // __TOPDOWNSHOOTER_STAGE_STAGE_DATA_H__
```

- [ ] **Step 2: 헤더 단독 컴파일 확인은 Task 4(사용처)와 함께 — 여기선 파일만 생성. 커밋은 Task 4 와 묶음.**

(StageData.h 는 헤더온리라 단독 빌드 산출 없음. Task 4 에서 main.cpp/WaveController 가 include 하며 컴파일 검증.)

---

## Task 3: 데이터 파일 — 스키마 + 값

**Files:**
- Create: `<apps>/_MyApp_/resources/data/schema/stage.schema.json`
- Create: `<apps>/_MyApp_/resources/data/stage.json`

POST_BUILD 가 `resources/` 를 실행 파일 디렉토리로 복사하므로(기존 컨벤션), 게임은 런타임에 `<.>/resources/data/stage.json` 으로 읽는다.

- [ ] **Step 1: `stage.schema.json` 작성**

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "Stage",
  "type": "object",
  "properties": {
    "waveSpawnInterval": { "type": "number",  "default": 3.0,  "minimum": 0.1, "description": "적 스폰 간격(초)" },
    "waveMaxEnemies":    { "type": "integer", "default": 5,    "minimum": 1,   "description": "동시 생존 최대 수" },
    "waveHpBase":        { "type": "integer", "default": 20,   "minimum": 1,   "description": "웨이브1 기본 HP" },
    "waveHpPerWave":     { "type": "integer", "default": 5,    "minimum": 0,   "description": "웨이브당 HP 증가" },
    "waveSpeedBase":     { "type": "number",  "default": 1.5,  "minimum": 0.0, "description": "웨이브1 기본 속도" },
    "waveSpeedPerWave":  { "type": "number",  "default": 0.3,  "minimum": 0.0, "description": "웨이브당 속도 증가" },
    "waveContactDamage": { "type": "integer", "default": 10,   "minimum": 0,   "description": "접촉 데미지" },
    "arenaHalfExtent":   { "type": "number",  "default": 10.0, "minimum": 1.0, "description": "아레나 벽 안쪽 절반" },
    "wallThickness":     { "type": "number",  "default": 0.5,  "minimum": 0.1, "description": "물리 벽 두께(반-크기)" }
  }
}
```

- [ ] **Step 2: `stage.json` 작성 (현재 constexpr 값과 동일 = cutover 후 동작 불변 보장)**

```json
{
  "waveSpawnInterval": 3.0,
  "waveMaxEnemies": 5,
  "waveHpBase": 20,
  "waveHpPerWave": 5,
  "waveSpeedBase": 1.5,
  "waveSpeedPerWave": 0.3,
  "waveContactDamage": 10,
  "arenaHalfExtent": 10.0,
  "wallThickness": 0.5
}
```

- [ ] **Step 3: 커밋 (path-scoped)**

```bash
git commit <apps>/_MyApp_/resources/data/schema/stage.schema.json \
           <apps>/_MyApp_/resources/data/stage.json \
           -m "[feat] Stage 마스터데이터 JSON + 스키마 추가"
```

---

## Task 4: Stage cutover — WaveController 주입 + main 배선

**Files:**
- Modify: `apps/_MyApp_/src/Stage/WaveController.h` (ctor 시그니처 + `mData` 멤버)
- Modify: `apps/_MyApp_/src/Stage/WaveController.cpp` (`WAVE_*` → `mData.*`)
- Modify: `apps/_MyApp_/main.cpp` (StageData 로드 + WaveController/StageConfig 배선)

- [ ] **Step 1: `WaveController.h` — include + ctor + 멤버 변경**

include 묶음(약 35-38행)에 추가:

```cpp
#include "<Stage>/StageData.h"  // StageData 주입
```

ctor 선언(현재 약 68-69행)을 교체:

```cpp
        /// @brief 생성자 - 필수 의존 + Stage 마스터데이터 주입.
        /// @param world       Box2D 월드 (Enemy 물리 생성 위탁).
        /// @param spawnParent Enemy Actor 를 AddChild 할 부모 Actor.
        /// @param playerActor Player Actor 비소유 포인터 (사망 observer 등록 대상).
        /// @param data        Stage 웨이브/아레나 마스터데이터(런타임 JSON 로드값).
        WaveController(b2World* world, SJH::Scene::Actor* spawnParent,
                       SJH::Scene::Actor* playerActor, const StageData& data);
```

멤버 선언부에서 `float mArenaHalfExtent;`(약 115행)는 유지하고, 그 근처(예: `mArenaHalfExtent` 다음 줄)에 추가:

```cpp
        StageData          mData;            ///< Stage 마스터데이터(웨이브 곡선/아레나) - JSON 런타임 로드값.
```

- [ ] **Step 2: `WaveController.cpp` — ctor + 상수 참조 cutover**

ctor(약 37-42행)를 교체:

```cpp
    WaveController::WaveController(b2World* world, SJH::Scene::Actor* spawnParent,
                                    SJH::Scene::Actor* playerActor, const StageData& data)
        : mWorld(world), mSpawnParent(spawnParent),
          mPlayerActor(playerActor), mArenaHalfExtent(data.ArenaHalfExtent),
          mData(data),
          mSpawnTimer(data.WaveSpawnInterval)
    {}
```

`SpawnEnemy()` 의 웨이브 공식(약 114-116행)을 교체:

```cpp
        d.hp           = mData.WaveHpBase + mWave * mData.WaveHpPerWave;
        d.speed        = mData.WaveSpeedBase + static_cast<float>(mWave) * mData.WaveSpeedPerWave;
        d.damage       = mData.WaveContactDamage;
```

`Update`/스폰 가드의 `WAVE_MAX_ENEMIES` 참조(약 156행, `LiveCount() < WAVE_MAX_ENEMIES`)를 교체:

```cpp
        if (mSpawnTimer.IsTimesUp() && LiveCount() < mData.WaveMaxEnemies)
```

(주의: `#include "apps/_MyApp_/src/Stage/Constants.h"` 는 다른 상수가 남아있지 않으면 제거 가능하나, 안전하게 유지해도 무방. `WAVE_*` 참조가 본 파일에서 모두 사라졌는지 grep 으로 확인: `grep -n "WAVE_" apps/_MyApp_/src/Stage/WaveController.cpp` → 0 건이어야 함.)

- [ ] **Step 3: `main.cpp` — StageData 로드 + 배선**

`main.cpp` 상단 include 에 추가:

```cpp
#include "<Stage>/StageData.h"
```

`OnSceneSetup()` 의 시작부(`auto &reg = SJH::ResourceRegistry::Get();` 직후, 약 119행)에 로드 추가:

```cpp
			// Plan 1A - Stage 마스터데이터 런타임 로드 (재컴파일 없이 stage.json 으로 튜닝).
			TopdownShooter::Stage::StageData stageData = TopdownShooter::Stage::LoadStageData(reg);
			spdlog::info("[StageData] spawnInterval={:.2f} maxEnemies={} hpBase={} arenaHalf={:.1f}",
			             stageData.WaveSpawnInterval, stageData.WaveMaxEnemies,
			             stageData.WaveHpBase, stageData.ArenaHalfExtent);
```

`CreateStageActor` 호출 지점(`StageConfig` 구성처)을 찾아(`grep -n "CreateStageActor\|StageConfig" apps/_MyApp_/main.cpp`), `StageConfig` 에 데이터 주입:

```cpp
			// 기존: cfg 의 arenaHalfExtent/wallThickness 는 constexpr 기본값.
			// cutover: JSON 로드값으로 덮어쓴다.
			cfg.arenaHalfExtent = stageData.ArenaHalfExtent;
			cfg.wallThickness   = stageData.WallThickness;
```

`WaveController` 생성 지점을 찾아(`grep -n "AddComponent<Stage::WaveController>\|WaveController(" apps/_MyApp_/main.cpp`), 마지막 인자 `Stage::ARENA_HALF_EXTENT` 를 `stageData` 로 교체:

```cpp
			// 기존: AddComponent<Stage::WaveController>(&phys.World(), waveSpawner, mSpriteActor, Stage::ARENA_HALF_EXTENT);
			waveSpawner->AddComponent<Stage::WaveController>(&phys.World(), waveSpawner, mSpriteActor, stageData);
```

(주의: `stageData` 가 `OnSceneSetup` 지역 변수라 `WaveController` 생성 지점이 같은 함수 스코프 안인지 확인. 만약 `WaveController` 가 `OnBeforeFirstFrame` 에서 생성되면, `stageData` 를 멤버(`mStageData`)로 승격해 두 phase 간 공유. grep 결과의 실제 위치에 맞춰 둘 중 하나 선택.)

- [ ] **Step 4: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 성공(exit 0). `WAVE_*`/`ARENA_HALF_EXTENT` 미정의 에러 없음(전부 cutover).

- [ ] **Step 5: 런타임 동작 검증 (재컴파일 없음 = 데이터 주도 증명)**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
Expected (로그): 시작 시 `[StageData] spawnInterval=3.00 maxEnemies=5 hpBase=20 arenaHalf=10.0`. 게임이 기존과 *동일하게* 웨이브 스폰(stage.json 이 constexpr 와 동일값이므로 동작 불변).

그 다음 **재컴파일 없이** 데이터만 수정해 검증:
```bash
# 실행 디렉토리의 복사본을 직접 수정 (POST_BUILD 복사본).
# spawnInterval 3.0 -> 0.5, maxEnemies 5 -> 12 로 편집 후 재실행.
```
`<build_ninja>/apps/_MyApp_/resources/data/stage.json` 의 `waveSpawnInterval` 을 `0.5`, `waveMaxEnemies` 를 `12` 로 수정 → `./_MyApp_` 재실행(빌드 X).
Expected: 로그가 `spawnInterval=0.50 maxEnemies=12`, 적이 눈에 띄게 빨리/많이 스폰됨. → **재컴파일 없이 밸런스 변경 = Plan 1 핵심 동작 증명.** (검증 후 `stage.json` 원복.)

- [ ] **Step 6: 커밋 (path-scoped) + StageData.h 동봉**

```bash
git commit <apps>/_MyApp_/src/Stage/StageData.h \
           apps/_MyApp_/src/Stage/WaveController.h apps/_MyApp_/src/Stage/WaveController.cpp \
           apps/_MyApp_/main.cpp \
           -m "[feat] Stage cutover - WaveController 가 StageData(런타임 JSON) 주입 소비"
```

---

## Self-Review (작성자 점검 결과)

- **스펙 커버리지**: 본 플랜은 스펙 §0 Plan 1 의 *인프라 + Stage 파일럿* 슬라이스만 다룬다(의도된 범위). 나머지 Tier A 5종 = Plan 1B, Tier B = Plan 1C.
- **타입 일관성**: `DataTable::As<T>` / `from_json(json, StageData&)` / `LoadStageData` / `WaveController(..., const StageData&)` / `mData` 멤버명 전 Task 일관.
- **No placeholder**: 모든 코드 블록 실 내용. main.cpp 의 일부 지점은 "grep 으로 위치 확인 후 교체"인데, 이는 main.cpp 가 InitScheduler 리팩토링으로 줄번호가 유동적이기 때문(symbol 기반 지시 + before/after 스니펫 제공으로 보완).
- **검증 가능**: 각 Task 빌드 커맨드 + Task 4 의 "JSON 수정 → 재실행 → 동작 변경" 이 Plan 1 의 DoD("재컴파일 없이 밸런스 패치") 직접 증명.

## 잔여 리스크 / 후속 (Plan 1B 진입 전 확인)

- `WaveController` 생성 phase(OnSceneSetup vs OnBeforeFirstFrame) 확인 후 `stageData` 의 스코프(지역 vs 멤버) 확정 — Step 3 주의 참조.
- `DataTable::Load` 의 private ctor + `new` wrap 은 의도적(외부 생성 차단). clang-tidy `make_unique` 권고가 뜨면 무시(의도된 패턴).
- Plan 1B 는 본 패턴(제네릭 DataTable + 타입드 *Data + from_json + Load*Data 헬퍼 + 소비처 cutover)을 Entity/Physics/Bootstrap/HUD/Audio 에 복제. HUD 는 `glm::vec4`/`vec2` 의 from_json(배열 [r,g,b,a]) 추가 필요.
- **벤치마크 채택 지도(1B/1C, 출처 [06-bakingsheet-datatable-benchmark.md](../../webeditor/06-bakingsheet-datatable-benchmark.md))**:
  - 커스텀 타입(`glm::vec2`/`vec4`, enum)은 nlohmann **ADL `from_json` 오버로드**(= BakingSheet `ISheetValueConverter` 등가)로 처리.
  - 컬렉션(Tier B: VFX `EffectAsset[]`, 스프라이트 테이블)은 `As<std::vector<Record>>()` 또는 클라 `TableData<K,V>`(id 조회 시, 자체 `from_json` 제공 → `As<TableData<K,V>>()`). **엔진 `DataTable` 무수정 — `RawJson()` 추가 금지.**
  - 테이블 2개+ 되면 클라 `MasterData` 집약 struct + `LoadMasterData()` + (필요시) `PostLoad()`. ※ PostLoad 는 cross-ref resolve/게임 실사용 파생값 한정 — 스프라이트 UV 계산은 넣지 말 것(런타임 `SJH::sprite` 몫).
  - **char16_t VFX 경로**: JSON UTF-8 → 런타임 `std::u16string` 변환 유틸 필요(엔진 존재 여부 1B 착수 시 확인).
  - cross-ref(`DataRef<T>{id; const T* ptr;}` + eager resolve)·`ValidateMasterData()`(런타임 미호출, CI/에디터 게이트) = **Plan 1C**. 다중소스 컨버터/코드젠/인덱스 = **skip(YAGNI)**.
