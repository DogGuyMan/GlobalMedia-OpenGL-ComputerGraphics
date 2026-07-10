# 벤치마크 #6 — BakingSheet 데이터테이블 아키텍처 (우리 DataTable 설계 검증)

- **날짜**: 2026-06-19
- **방식**: 다중 에이전트(anti-gaming 역할분리) — ① C# 아키텍처 분석자(Sonnet, BakingSheet clone 분석 + 3-ref) → ② C#→C++ 마이그레이터(Sonnet, 우리 Plan 1A에 매핑) → ③ 적대검증·종합(Opus, 본 문서).
- **레퍼런스**: 주=[cathei/BakingSheet](https://github.com/cathei/BakingSheet)(C#/Unity), 교차=[Cysharp/MasterMemory](https://github.com/Cysharp/MasterMemory), Unity ScriptableObject.
- **목적**: C++ 게임의 JSON 마스터데이터 "DataTable 시스템"(Plan 1A) 설계가 모범구현 대비 타당한지 + 무엇을 더 채택할지.

---

## 0. 헤드라인 결론 (적대검증 후)

> **벤치마크가 Plan 1A 코어를 검증했다.** "제네릭 `nlohmann::json` 홀더(`DataTable`) + 타입별 명시 `from_json`(`StageData` 등)"은 BakingSheet의 Sheet/Row/ValueConverter 모델을 *C++엔 없는 리플렉션을 제거하고* 옮긴 정확한 번역이다. **Plan 1A에 누락된 must-have 없음.**

BakingSheet 풍부함의 대부분은 우리 규모(단일 게임 + 소량 마스터데이터)에서 **1B/1C로 이연하거나 skip**된다. 즉 이번 벤치마크는 "지금 더 만들 것"보다 "지금 안 만들어도 됨을 확인 + 미래 채택 지도"의 가치가 크다.

## 1. 핵심 번역 (C# 리플렉션 → C++ 명시)

BakingSheet 우아함의 8할은 **C# 런타임 리플렉션**(`PropertyMap`/`ContractResolver`/`Activator.CreateInstance`/`[SheetValueConverter]` attribute)에 의존한다 — 셀↔필드 컬럼매핑을 자동화. **C++엔 리플렉션이 없으므로 재현 시도는 금지.** 등가물:

| BakingSheet (C# 리플렉션) | C++ 등가 (우리 채택) |
|---|---|
| `PropertyMap` 자동 컬럼매핑 | 타입별 명시 `from_json`(수동 또는 `NLOHMANN_DEFINE_TYPE_INTRUSIVE`) ← **Plan 1A 이미** |
| `ISheetValueConverter`(Enum/Nullable/커스텀) | nlohmann ADL `from_json` 오버로드(`glm::vec2`, `char16_t` 경로 등) |
| `SheetContainer` 프로퍼티 리플렉션 열거 | 클라가 *명시 열거*(집약 struct, 1B) — 리플렉션 열거 불가 |

## 2. 채택 판정표 (적대검증 반영)

| 개념 | C++ 등가 | Plan 1A에 있나 | 판정 | 근거 |
|---|---|---|---|---|
| 키기반 Row 데이터모델 | `*Data` struct + `from_json` | ✓ | **이미** (must-have 충족) | 3/3 공유 must-have |
| ValueConverter 확장점 | 커스텀 타입 `from_json` ADL 오버로드 | 암묵적 | **adopt-now(명문화)** | glm/enum/char16_t 에 필요. C++ 관용구 |
| Sheet(id→row 컬렉션) | `As<std::vector<Record>>()` + 클라 인덱스, 또는 `TableData<K,V>`(클라 정의, `from_json` 제공) | ✗ | **adopt-1B** | Tier B(VFX/스프라이트)서 필요. **`RawJson()` 추가 불필요 — `As<>` 로 충분** |
| SheetContainer(집약) | 클라 `MasterData{...}` 명시 열거 struct | ✗ | **adopt-1B** | 테이블 2개+ 시. 1A(Stage 단일)엔 YAGNI |
| PostLoad 훅 | 클라 `PostLoad(MasterData&)` 자유함수 | ✗ | **adopt-1B** | cross-ref resolve/파생값. (※ UV계산은 게임 런타임 sprite 몫 — PostLoad에 넣지 말 것) |
| Reference(cross-ref) | `DataRef<T>{id; const T* ptr;}` + eager resolve 패스 | ✗ | **adopt-later(1B, 참조 2개+ 생기면)** | 현재 테이블간 참조 사례 0. YAGNI 경계 |
| Verify(무결성) | 클라 `ValidateMasterData()` 자유함수(런타임 미호출, CI/툴) | ✗ | **adopt-1C** | 게임=관대한 파서 결정과 정합. WebEditor/CI 게이트 |
| 중간 베이크(Excel→JSON) | JSON이 이미 최종 포맷 | N/A | **skip** | 저작 파이프라인 없음(미래 WebEditor가 JSON 직접 씀) |
| 다중소스 컨버터(Excel/Google/CSV) | — | ✗ | **skip(YAGNI)** | JSON 단일소스. 1/3 optional |
| 코드젠+다중인덱스(MasterMemory) | `find_if`/`unordered_map` 단일키 | ✗ | **skip(YAGNI)** | 소량 데이터. 1/3 optional |
| async/Task, HashCode, Unity SO 통합, SheetRowArray | — | ✗ | **skip** | C++ 동기로드 / 핫리로드 없음 / 엔진 에셋시스템 없음 |

## 3. 우리 플랜으로의 반영 (수정안)

- **Plan 1A (현재)** — **변경 없음.** 벤치마크가 현 설계를 검증. 유일한 명문화: 커스텀 타입(`glm::vec2` 등)은 `from_json` ADL 오버로드로 — 이는 이미 1A 패턴 안.
- **Plan 1B (Tier B 컬렉션 — VFX/스프라이트)**:
  - 컬렉션은 `As<std::vector<Record>>()`(단순) 또는 클라 `TableData<K,V>`(id 조회 필요 시, `from_json` 제공 → `As<TableData<K,V>>()`). **엔진 `DataTable` 무수정**(RawJson 불필요).
  - 모든 컬렉션 Record 에 `std::string Id`(또는 기존 `key`) 컨벤션.
  - 테이블 2개+ 되면 클라 `MasterData` 집약 struct + `LoadMasterData()` + (필요시) `PostLoad()`.
  - **char16_t VFX 경로**: JSON 은 UTF-8 → 런타임 `std::u16string` 변환 유틸 필요(엔진에 있는지 미확인 — 1B 착수 시 확인. 없으면 소형 UTF-8→UTF-16 변환 추가 또는 Effekseer 호출 직전 변환).
- **Plan 1C / 미래**: `DataRef<T>` cross-ref(참조 발생 시), `ValidateMasterData()`(CI/WebEditor 게이트), 코드젠(테이블 20+ 로 수동 from_json 부담 시 — 현재 미지수).

## 4. 적대검증이 마이그레이터 설계에서 교정한 것

1. **`RawJson()` 접근자 추가 거부** — `As<std::vector<Record>>()` / `As<TableData<K,V>>()`(from_json 제공)로 충분. 엔진 캡슐화 유지.
2. **`MasterData`+`PostLoad`를 1A→1B 이연** — Stage 단일 테이블엔 과설계.
3. **PostLoad의 `ComputeUVRect` 예시 폐기** — UV는 게임 런타임 sprite 계산 몫. PostLoad는 cross-ref/게임실사용 파생값 한정.
4. **`shared_ptr` 스케치 → `DataTableUPtr`(unique) + Find/Create raw `DataTable*`** (Plan 1A 실제와 일치).

## 5. 확정 사실 (출처) vs 의견

**확정(분석자 인용)**: `Sheet<TKey,TValue>:KeyedCollection`(Sheet.cs:18) / 코어는 `Microsoft.Extensions.Logging.Abstractions`만 의존(BakingSheet.csproj) / Excel=ExcelDataReader·JSON=Newtonsoft 컨버터 분리(.csproj) / 코드젠 없음·런타임 리플렉션(`Activator.CreateInstance`) / cross-ref=`Sheet<,>.Reference{Id,Ref}` eager resolve(SheetReference.cs) / Verify 별도 단계(SheetVerifier.cs).
**의견(💭)**: 우리 규모엔 컬렉션/cross-ref/validate가 1B/1C면 충분하고 코드젠은 십중팔구 불필요 / Plan 1A는 무수정이 옳다.

## 6. 검증 한계 (정직)

- BakingSheet clone = `--depth 1` HEAD(정확 커밋 미기록). MasterMemory/Unity SO = 문서 수준만(소스 미clone).
- `HashCode` 사용처 미확인. 일부 CultureInfo/formatter 경로 미추적.
- 우리 측 가정 미검증 2건: ① VFX 가 id 조회를 실제 요구하는지(아니면 `vector` 순회로 충분) ② char16_t UTF-8→u16 변환 유틸 엔진 존재 여부. → 1B 착수 시 확인.
- 에이전트 권한은 hook 강제 대신 프롬프트 분리(분석/마이그레이션이 우리 repo 무수정). 한계 인지.
