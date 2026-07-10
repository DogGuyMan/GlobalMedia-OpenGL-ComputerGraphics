# WebEditor REST DB 서버 — 아키텍처 설계 브리프 (사용자 직접 설계용)

- **날짜**: 2026-06-19
- **목적**: 사용자가 **WebEditor의 데이터 백엔드(Web 기반 DB REST 서버) 아키텍처를 직접 설계**할 때 필요한 핵심 정보·시스템 분해·결정 체크리스트를 제공.
- **사용법**: 이 브리프로 설계 → 설계 결과(또는 의문점)를 가져오면, 내가 foundation 스펙의 `IDataStore` 포트(D4)에 *어댑터 교체*로 정합시키고 결정로그(D1~D15)와 충돌 검사 → `/writing-plans`.
- **연관**: 스펙 `doc/superpowers/specs/2026-06-19-webeditor-master-data-foundation-design.md`, 리서치 `doc/webeditor/`.

---

## 0. 이게 *어디에* 들어가는가 (경계부터)

> ⚠️ **이건 Phase 2 설계다. foundation 스펙은 안 바뀐다.** foundation의 dev 서버는 *cpp-httplib 정적+PUT 플랫파일*(D10)로 단순하게 간다. 네가 설계하는 **REST DB 서버는 그 dev 서버가 성장한 모습**이고, 둘은 **`IDataStore` 포트(D4)** 덕분에 에디터 코드 변경 없이 *어댑터 교체*로 갈아끼워진다.

```
[ Editor (WASM) ]
   └─ IDataStore (포트)              ← 에디터는 이 인터페이스만 안다 (불변)
        ├─ Fetch/PutAdapter   ─── Phase 1: cpp-httplib 플랫파일 dev 서버
        └─ RestDbAdapter      ─── Phase 2: ★네가 설계하는 REST DB 서버★
                                       └─ Storage (파일 | DB)
```

→ **네 설계의 자유도는 "포트 뒤쪽"에 있다.** 에디터가 기대하는 계약(load/save/list)만 만족하면, 서버 내부(엔드포인트/저장소/DB)는 마음대로 설계 가능.

---

## 1. 가장 중요한 정보 (이것만 알면 설계 가능 — 자립)

**무엇을 저장/편집하는가 = 게임 마스터 데이터.** 두 종류가 섞여 있다:
- **싱글톤 문서(document)**: 카테고리당 1개. 예 — Stage(웨이브 곡선/아레나), Physics(임펄스), Audio(이벤트 경로), Entity(플레이어 스탯), HUD, Bootstrap. → "한 문서 = 한 설정 객체".
- **컬렉션(collection)**: 같은 형태의 레코드 N개. 예 — 적 로스터(여러 적 타입), 아이템 DB, **VFX 카탈로그(이미 `TEST_EFFECTS[]` 배열로 존재)**, 웨이브 정의. → "한 컬렉션 = 레코드 배열, 각 레코드는 id 보유".
- **현재 foundation 범위(밸런스 4종)는 전부 싱글톤 문서**지만, **REST 설계는 컬렉션도 1급으로 수용**해야 미래(적/아이템/VFX)가 안 막힌다.

**스키마가 진실원천(SSOT).** 각 문서/레코드 타입마다 `*.schema.json`(JSON Schema 부분집합: type/properties/default/min/max/description)이 존재. 에디터는 스키마로 폼을 자동 생성, 서버는 같은 스키마로 검증. → **REST 서버는 스키마도 서빙해야 한다.**

**락된 제약 (재논의 금지 — 설계 전제)**:
- 게임은 이 데이터를 **런타임 JSON 로딩**(nlohmann/json)으로 소비(D2). → 서버 산출물의 *정본 표현은 JSON*이어야 게임이 그대로 읽는다.
- **"전부 C++" + 헤더온리 벤더링 + vcpkg 미사용**(프로젝트 정체성). dev/REST 서버 = **cpp-httplib**(D10, header-only, `include/`). JSON = nlohmann/json(D11).
- **브라우저 샌드박스**: 에디터(WASM)는 `fetch`만 가능. 로컬 FS 직접 접근 불가 → 모든 입출력이 HTTP. WASM 자체도 HTTP 서빙 필수(`file://` 불가, CORS).
- 플랫폼: macOS + Windows. (Windows는 `127.0.0.1` 권장 — IPv6 지연 회피.)
- 데이터 거주(게임 측): `SJH::ResourceRegistry`의 DataTable 자원(D6) — *서버 설계와 무관*, 참고용.

---

## 2. 어떤 시스템이 필요한가 (시스템 분해)

REST DB 서버를 설계할 때 최소 이 6개 시스템을 의식해야 한다:

| # | 시스템 | 책임 | 비고 |
|---|---|---|---|
| S-1 | **Schema Registry** | `*.schema.json` 보관·서빙·버전 | 에디터 폼 생성의 원천. 읽기 위주 |
| S-2 | **Data Store / Repository** | 문서·컬렉션 CRUD (저장소 추상화) | 백엔드 = 파일 \| DB (네 핵심 결정) |
| S-3 | **REST API 레이어** | 엔드포인트·라우팅·요청 검증·상태코드 | cpp-httplib `Get/Put/Post/Delete` |
| S-4 | **Validation** | PUT/POST 시 스키마 대조 + 거부(422) | 서버=방어선, 에디터=UX. **공유 스키마** |
| S-5 | **Versioning / Concurrency** | 동시편집 충돌 방지, 편집 이력 | ETag/If-Match, (파일이면 git이 이력) |
| S-6 | **Serving / Infra** | WASM 정적 서빙 + CORS + (미래)Auth | localhost=Auth 없음, 호스팅=토큰 |

추가로 **클라이언트 측**(에디터)에는 `RestDbAdapter`(IDataStore 구현) 하나만 새로 생긴다 — 나머지 에디터 코드는 불변.

---

## 3. REST DB 서버 구상 (출발점 — 네가 살 붙이면 됨)

### 3.1 리소스 모델 (문서 + 컬렉션 둘 다)
```
GET    /api/schema/{type}            # 스키마 1개 (폼 생성용)
GET    /api/schema                   # 스키마 목록

# 싱글톤 문서 (stage, physics, audio, player ...)
GET    /api/doc/{name}               # 문서 1개 읽기
PUT    /api/doc/{name}               # 문서 통째 저장 (검증 후)

# 컬렉션 (enemies, items, effects ...)  ← 미래 대비
GET    /api/coll/{name}              # 레코드 배열
POST   /api/coll/{name}              # 레코드 추가 (id 발급)
GET    /api/coll/{name}/{id}         # 레코드 1개
PUT    /api/coll/{name}/{id}         # 레코드 수정
DELETE /api/coll/{name}/{id}         # 레코드 삭제
```
- 응답: JSON. 검증 실패 = `422` + 에러 배열(field/message). 충돌 = `409`. 없음 = `404`.
- 게임이 읽는 *정본 파일*과 서버 표현을 **어떻게 일치시킬지**가 핵심(아래 3.2).

### 3.2 저장소(Storage) 스펙트럼 — ★가장 큰 결정★
| 옵션 | 장점 | 단점 | 적합 |
|---|---|---|---|
| **(A) 플랫 JSON 파일 (git 추적)** | git이 *공짜로* 이력/diff/PR리뷰/blame/롤백 제공. 게임이 파일 그대로 로드. 의존 0 | 관계 쿼리·대량 레코드 약함. 동시쓰기 직접 처리 | 마스터 데이터 정통(엔진들이 텍스트 에셋을 VCS에 둠) |
| **(B) 임베디드 SQLite** | 쿼리/인덱스/트랜잭션. 대량·교차참조 강함 | git diff 불가(바이너리). 게임 로드용 JSON export 단계 별도 필요. 마이그레이션 부담 | 레코드 수천+ / 관계형 질의 필요 시 |
| **(C) 문서 DB(외부)** | 확장성 | 무거움·호스팅·의존. 학습 프로젝트 과함 | 다중 사용자 SaaS 단계 |

> **내 의견(확정 아님, 네가 결정)**: 게임 마스터 데이터는 **(A) 플랫 JSON-in-git이 sweet spot.** 이유 = ① 게임이 런타임에 그 파일을 그대로 읽음(변환 0) ② "DB가 주는 이력/버전"을 git이 이미 더 잘 줌 ③ 헤더온리/의존0 철학과 일치. **(B) SQLite는 "적/아이템이 수백 종 + 서로 참조 + 쿼리 필요"가 실제로 닥쳤을 때** 전환(그때 JSON export 단계 추가). 즉 "DB REST 서버"의 *DB*를 처음부터 SQL로 볼 필요는 없고, **파일시스템을 DB처럼 다루는 Repository(S-2)** 로 시작해 나중에 백엔드만 교체하는 게 안전. ← 단 이건 네가 "쿼리/관계가 정말 필요한가"로 판단.

### 3.3 검증(S-4) / 버전(S-5)
- 검증: PUT/POST 본문을 `/api/schema/{type}`로 대조 → 위반 시 `422`. 서버가 *방어선*, 에디터는 *즉시 UX*. 스키마 한 벌 공유.
- 동시성: 단일 사용자 dev면 last-write-wins로 충분. 단 **ETag + `If-Match`** 한 겹 두면 미래 다중 편집 안전(읽을 때 버전 받고, 쓸 때 그 버전 일치해야 수락, 아니면 `409`).
- 이력/undo: **(A)면 git이 이력** → 별도 시스템 불요(파일 저장만 잘 하면 됨). (B)면 audit 테이블 직접.

### 3.4 기술 스택 (락된 것 위에서)
- 서버 = **cpp-httplib**(네이티브 C++ 타겟, `tools/webeditor-server/`) + **nlohmann/json** + (A면)`std::filesystem` / (B면 SQLite amalgamation 단일 .c 벤더링).
- 에디터 측 = `RestDbAdapter`(IDataStore 구현) — `fetch`로 위 엔드포인트 호출.

---

## 4. ★네가 설계 때 답해야 할 핵심 결정★ (이게 제일 중요)

이 8개에 답을 정하면 설계가 선다. 정답 없는 것엔 내 추천을 달았다(네가 뒤집어도 됨):

1. **배포 타깃**: localhost 전용 dev 도구인가, 언젠가 호스팅(원격 다중 사용자)인가? → *이게 Auth/CORS/동시성 난이도를 다 좌우*. (추천: localhost 우선, 호스팅은 인터페이스만 열어두기)
2. **저장소 백엔드**: (A)플랫JSON-in-git / (B)SQLite / (C)외부DB? (추천: A로 시작, Repository로 추상화)
3. **리소스 모델**: 싱글톤 문서만 / 컬렉션도? 컬렉션 id 발급 규칙(증분? slug? uuid?)? (추천: 둘 다 수용, id=slug)
4. **정본↔서버 일치**: 서버가 게임이 읽는 그 파일을 직접 쓰는가, 아니면 export 단계가 있는가? (A면 직접, B면 export)
5. **검증 권위**: 서버+에디터 양쪽? 위반 응답 포맷(422 바디 스키마)? (추천: 양쪽, 바디=`[{path,message}]`)
6. **동시성/이력**: last-write-wins / ETag 낙관락? 이력=git에 위임? (추천: ETag 한 겹 + git 이력)
7. **Auth**: dev=없음 / 호스팅=토큰? (추천: dev는 127.0.0.1 바인딩으로 충분)
8. **스키마 진화**: 스키마가 바뀔 때 기존 데이터 마이그레이션을 누가/언제? (추천: 관대한 로더+기본값 폴백 = 이미 D 결정, 서버는 그 위에)

---

## 5. 가드레일 (설계가 깨지면 안 되는 선)

- **`IDataStore` 계약을 깨지 마라**: 에디터는 load/save/list만 안다. 서버가 아무리 화려해도 이 포트 뒤에 있어야 *어댑터 교체*가 성립(D4).
- **게임 소비 = JSON 런타임 로딩(D2)**: 서버의 *정본 산출은 JSON*. DB를 쓰면 JSON export 경로가 반드시 있어야 게임이 안 깨진다.
- **전부 C++ / 헤더온리 / vcpkg 미사용**: 서버 라이브러리는 cpp-httplib(+필요시 SQLite amalgamation) 같은 *벤더링 가능한* 것만. 무거운 프레임워크(Drogon 등) 지양(리서치 compass 참조).
- **foundation 스펙(D1~D15) 불변**: 이 설계는 Phase 2. foundation의 cpp-httplib 플랫파일 dev 서버를 *대체할 미래형*일 뿐, 지금 스펙을 바꾸지 않는다.
- **브라우저 제약**: 에디터는 fetch만. 서버는 CORS 헤더(호스팅 시) + WASM 정적 서빙 책임.

---

## 6. 네 설계 산출물 (가져오면 내가 정합)

다음을 담아 오면 바로 `/writing-plans`로 연결 가능:
- §4의 8개 결정에 대한 *네 답* (특히 #1 배포타깃, #2 저장소).
- 엔드포인트 표(§3.1을 네 버전으로) + 요청/응답 예시 1~2개.
- 저장소 레이아웃(파일 트리 또는 테이블 스키마).
- (의문점이 있으면) 질문 목록 — 내가 답하거나 같이 결정.

→ 가져오면 내가 ① IDataStore 계약과의 정합 ② 결정로그 충돌 ③ 헤더온리/C++ 제약 위반을 체크하고, 필요하면 스펙에 Phase 2 섹션을 추가한 뒤 플랜으로 넘어간다.
