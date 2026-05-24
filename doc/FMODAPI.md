# FMOD Studio API — 학습 노트

`apps/audio_demo/demo1`, `apps/audio_demo/demo2` 에서 실제로 사용한 FMOD API 호출 정리.
모든 예시는 두 데모의 코드에서 발췌 — 검증된 호출.

설치/CMake 통합은 [doc/FMOD_Setup.md](FMOD_Setup.md) 참조.

---

## 1. 개요 — Core vs Studio

FMOD 는 두 계층:

| 계층 | 헤더 | 역할 |
|---|---|---|
| **Core** | `<fmod/fmod.h>`, `<fmod/fmod.hpp>` | 저수준 — 직접 wav/mp3/스트림 재생, DSP, 채널 그룹 |
| **Studio** | `<fmod/fmod_studio.hpp>` | 고수준 — `.bank` 파일 기반 이벤트/믹서/파라미터 시스템 |

본 데모들은 **Studio 만** 사용 (Core 는 Studio 가 내부에서 가져옴 — `initialize` 인자 분리).

```cpp
#include <fmod/fmod_common.h>          // FMOD_RESULT 등 공용 타입
#include <fmod/fmod_errors.h>          // FMOD_ErrorString
#include <fmod/fmod_studio.hpp>        // C++ Studio API (System, Bank, Event...)
#include <fmod/fmod_studio_common.h>   // Studio 공용 enum/flag
```

---

## 2. 에러 처리 — `FMOD_RESULT` + `FMOD_ErrorString`

거의 모든 함수가 `FMOD_RESULT` 반환. 성공은 `FMOD_OK`, 그 외는 에러 코드.

```cpp
void ck(FMOD_RESULT r, const char* where) {
    if (r != FMOD_OK) {
        std::fprintf(stderr, "[FMOD] %s: %s\n", where, FMOD_ErrorString(r));
        std::exit(1);
    }
}

// 사용 — 학습 챕터는 첫 실패에서 즉시 종료
ck(mSystem->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr),
   "System::initialize");
```

`FMOD_ErrorString` 은 enum 값을 사람이 읽을 수 있는 문자열로 변환. 예: `ERR_EVENT_NOTFOUND`, `ERR_FILE_NOTFOUND`, `ERR_INVALID_HANDLE` …

프로덕션 코드는 graceful degradation 권장 (오디오 실패해도 게임은 진행).

---

## 3. System 라이프사이클

### 생성 + 초기화

```cpp
FMOD::Studio::System* mSystem = nullptr;
ck(FMOD::Studio::System::create(&mSystem), "System::create");

// (maxchannels, studio_flags, core_flags, extradriverdata)
ck(mSystem->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr),
   "System::initialize");
```

- `maxchannels=512` — 동시 가상 채널 한도 (실제 하드웨어 채널은 더 적음, FMOD 가 가상→실제 매핑)
- 두 flag 인자 — Studio 와 Core 각각 분리
- `nullptr` 마지막 인자 — 플랫폼별 추가 드라이버 데이터 (보통 nullptr)

### 매 프레임 호출 — `update()`

비동기 작업(bank 로딩, 이벤트 시작/정지, 파라미터 보간) 을 메인 스레드에서 처리.
**누락하면 사운드가 끊기거나 아예 안 들림.**

```cpp
void render(double /*currentTime*/) override {
    if (mSystem) {
        ck(mSystem->update(), "System::update");
    }
    // ... GL clear / draw ...
}
```

### 종료 — `release()`

```cpp
void shutdown() override {
    // ... ImGui shutdown 먼저, 이벤트 인스턴스 release, bank unload ...
    if (mSystem) {
        ck(mSystem->release(), "System::release");
        mSystem = nullptr;
    }
}
```

`release()` 는 모든 하위 객체(이벤트 인스턴스 잔여분, 채널 등) 도 함께 정리.

---

## 4. Bank 로드 / 언로드

`.bank` 는 FMOD Studio 저작 도구에서 export 된 컨테이너 — 이벤트, 샘플, 믹서, 파라미터 메타데이터를 패킹.

### 로드 순서 (중요)

```cpp
FMOD::Studio::Bank* mMasterBank  = nullptr;
FMOD::Studio::Bank* mStringsBank = nullptr;
FMOD::Studio::Bank* mMusicBank   = nullptr;

ck(mSystem->loadBankFile("resources/banks/Master.bank",
                         FMOD_STUDIO_LOAD_BANK_NORMAL, &mMasterBank),
   "loadBankFile Master");
ck(mSystem->loadBankFile("resources/banks/Master.strings.bank",
                         FMOD_STUDIO_LOAD_BANK_NORMAL, &mStringsBank),
   "loadBankFile Strings");
ck(mSystem->loadBankFile("resources/banks/Music.bank",
                         FMOD_STUDIO_LOAD_BANK_NORMAL, &mMusicBank),
   "loadBankFile Music");
```

- **Master.bank** — 믹서/버스 구조 (베이스)
- **Master.strings.bank** — 이벤트/파라미터의 **문자열 경로 룩업 테이블**. 이게 없으면 `getEvent("event:/...")` 같은 문자열 lookup 이 전부 실패.
- **\<컨텐츠\>.bank** — 실제 이벤트와 샘플 (Music.bank 등)

플래그:
- `FMOD_STUDIO_LOAD_BANK_NORMAL` — 동기 로드 (헤더 + 메타데이터 즉시, 샘플은 lazy)
- `FMOD_STUDIO_LOAD_BANK_NONBLOCKING` — 비동기, `getLoadingState()` 폴링 필요
- `FMOD_STUDIO_LOAD_BANK_DECOMPRESS_SAMPLES` — 메모리 사용 ↑, CPU 비용 ↓

### 언로드 — 역순

```cpp
// 로드 순: Master → Strings → Music
// 언로드 순: Music → Strings → Master
if (mMusicBank)   { mMusicBank->unload();   mMusicBank   = nullptr; }
if (mStringsBank) { mStringsBank->unload(); mStringsBank = nullptr; }
if (mMasterBank)  { mMasterBank->unload();  mMasterBank  = nullptr; }
```

**왜 역순**: 파생 bank(Music) 이 base bank(Master) 의 믹서/문자열을 참조한다. 베이스를 먼저 내리면 dangling reference.

### Bank 안의 이벤트 열거 (demo1 의 fallback)

```cpp
int count = 0;
ck(mMusicBank->getEventCount(&count), "Bank::getEventCount");

std::vector<FMOD::Studio::EventDescription*> events(static_cast<size_t>(count));
ck(mMusicBank->getEventList(events.data(), count, nullptr), "Bank::getEventList");
// events[0] ~ events[count-1] 사용 가능
```

세 번째 인자 (`nullptr` 자리) 는 *실제로 채워진 개수* 출력용 — 보통 `count` 와 같지만 안전 차원.

---

## 5. Event 모델 — Description vs Instance

FMOD Studio 의 핵심 추상:

- **`EventDescription`** — 이벤트의 *템플릿* (메타데이터, 파라미터 정의). Bank 가 소유. **release 하지 않는다** — 그냥 포인터만 nullify.
- **`EventInstance`** — 실제 재생 단위. Description 으로부터 생성. **반드시 release 해야 함**.

```cpp
FMOD::Studio::EventDescription* mBgmDesc     = nullptr;  // bank 소유, 빌려 쓰기만
FMOD::Studio::EventInstance*    mBgmInstance = nullptr;  // 우리가 소유, release 책임
```

### 경로로 lookup

```cpp
ck(mSystem->getEvent("event:/BGM",     &mBgmDesc),     "getEvent BGM");
ck(mSystem->getEvent("event:/Damaged", &mDamagedDesc), "getEvent Damaged");
ck(mSystem->getEvent("event:/Slash",   &mSlashDesc),   "getEvent Slash");
```

- 경로는 항상 `"event:/<group>/<name>"` 또는 `"event:/<name>"`
- 공백 그대로 — 예: `"event:/Music/Level 01"`
- 대소문자 구분
- **`.strings.bank` 가 로드되어 있어야** 동작

### 인스턴스 생성 + 재생

```cpp
ck(mBgmDesc->createInstance(&mBgmInstance), "BGM createInstance");
ck(mBgmInstance->start(),                   "BGM start");
```

### 정지 / 일시정지 / 재시작

```cpp
// 즉시 정지 (셧다운 시)
mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);

// fade-out 정지 (UI 의 Stop 버튼)
mInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);

// 일시정지 토글
mInstance->setPaused(true);   // 또는 false

// 다시 시작 (이미 멈춘 인스턴스는 start() 호출로 처음부터 재생)
mInstance->start();
```

### 해제

```cpp
if (mInstance) {
    mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
    mInstance->release();
    mInstance = nullptr;
}
mEventDesc = nullptr;  // bank 소유 — release X, 포인터만 null
```

---

## 6. One-shot 이벤트 패턴 (demo2 Damaged / Slash)

총 발사음, 발자국, UI 효과음 등 *짧고 일회성* 이벤트는 인스턴스 핸들을 들고 있을 필요 없다. **create → start → release** 한 줄 호출.

```cpp
void play_one_shot(FMOD::Studio::EventDescription* desc, const char* label) {
    FMOD::Studio::EventInstance* inst = nullptr;
    ck(desc->createInstance(&inst), "one-shot createInstance");
    ck(inst->start(),               "one-shot start");
    ck(inst->release(),             "one-shot release");
}
```

`release()` 의 동작:
- 즉시 파괴하지 않는다
- 인스턴스에 "재생 끝나면 자동 해제" 마크
- 재생 도중에는 정상 동작, 끝나면 FMOD 가 알아서 청소

→ description 만 캐시해두면 버튼 클릭당 새 인스턴스 생성. 동시 다중 재생도 가능 (총 8발 누르면 8개 인스턴스 동시 재생됨).

---

## 7. Parameter API — 4가지 축

FMOD Studio 의 파라미터는 두 가지 *스코프* × 두 가지 *식별 방식* 조합으로 4가지 사용법.

### 스코프

| 스코프 | 호출 대상 | 예 |
|---|---|---|
| **Event-instance** | `EventInstance::setParameter*` | 자동차 RPM (각 차량마다 다름) |
| **Global** | `System::setParameter*` | 게임 전체 Health (모든 이벤트 공유) |

### 식별

| 방식 | 호출 | 비고 |
|---|---|---|
| **By Name** | `setParameterByName("Health", 0.5f)` | 매 호출 문자열 → ID lookup. 편하지만 매번 비용 |
| **By ID** | `setParameterByID(id, 0.5f)` | description 조회 시 한 번 ID 캐시 후 사용. **권장** |

### demo2 — Event-instance + By Name

```cpp
// BGM_STATE 는 BGM 이벤트의 instance parameter (Labeled: Title/Combat/Boss)
ck(mBgmInstance->setParameterByName("BGM_STATE", static_cast<float>(idx)),
   "setParameterByName BGM_STATE");
```

### demo2 — Global + By Name

```cpp
// Health 는 global parameter — System 에 직접 설정
ck(mSystem->setParameterByName("Health", mHealth), "setParameterByName Health");
```

⚠️ **Global 은 `System` 에**, instance 는 `EventInstance` 에 — 헷갈리면 `ERR_EVENT_NOTFOUND` 류.

### demo1 — Event-instance + By ID (자동 발견)

```cpp
// startup 시 description 조회로 ID 캐시
FMOD_STUDIO_PARAMETER_DESCRIPTION desc{};
mEventDesc->getParameterDescriptionByIndex(i, &desc);
FMOD_STUDIO_PARAMETER_ID id = desc.id;  // 캐시

// 매 프레임 / 슬라이더 변경 시
ck(mInstance->setParameterByID(id, value), "setParameterByID");
```

---

## 8. 파라미터 메타데이터 자동 발견 (demo1)

이벤트가 어떤 파라미터를 가졌는지 *런타임에* 알아내기.

### 개수 + 인덱스 순회

```cpp
int paramCount = 0;
ck(mEventDesc->getParameterDescriptionCount(&paramCount),
   "getParameterDescriptionCount");

for (int i = 0; i < paramCount; ++i) {
    FMOD_STUDIO_PARAMETER_DESCRIPTION desc{};
    ck(mEventDesc->getParameterDescriptionByIndex(i, &desc),
       "getParameterDescriptionByIndex");
    // desc 사용...
}
```

### `FMOD_STUDIO_PARAMETER_DESCRIPTION` 필드

| 필드 | 타입 | 의미 |
|---|---|---|
| `name` | `const char*` | 파라미터 이름 (예: "Intensity", "BGM_STATE") |
| `id` | `FMOD_STUDIO_PARAMETER_ID` | 캐시 가능한 ID (data1/data2 두 uint32 묶음) |
| `minimum` | `float` | 최소값 (Labeled/Discrete 도 float 으로 저장) |
| `maximum` | `float` | 최대값 |
| `defaultvalue` | `float` | 디폴트 |
| `flags` | `int` | 비트마스크 — `FMOD_STUDIO_PARAMETER_LABELED`, `FMOD_STUDIO_PARAMETER_DISCRETE`, ... |

### 타입 분기 (demo1 의 UI 자동 매핑)

```cpp
const bool labeled  = (desc.flags & FMOD_STUDIO_PARAMETER_LABELED) != 0;
const bool discrete = (desc.flags & FMOD_STUDIO_PARAMETER_DISCRETE) != 0;

if (labeled) {
    // Labeled discrete (Title/Combat/Boss 같은 enum)
    // → ImGui::Combo
} else if (discrete) {
    if (desc.minimum == 0.0f && desc.maximum == 1.0f) {
        // 0/1 switch → ImGui::Checkbox
    } else {
        // 정수 슬라이더 → ImGui::SliderInt
    }
} else {
    // 연속값 → ImGui::SliderFloat
}
```

### Labeled 의 라벨 문자열 조회

```cpp
char buf[128]{};
int retrieved = 0;
mEventDesc->getParameterLabelByID(desc.id, valueIndex, buf, sizeof(buf), &retrieved);
// buf 에 라벨 문자열 (예: "Title", "Combat", "Boss")
// valueIndex 는 minimum..maximum 정수 범위
```

---

## 9. Bus API — 그룹별 볼륨 제어

Bus 는 FMOD Studio 의 *믹서 채널 그룹*. 이벤트는 출력 시 특정 bus 로 라우팅됨 (저작 도구에서 설정).

### 경로

| 경로 | 의미 |
|---|---|
| `"bus:/"` | **Master bus** (모든 출력의 최상위) |
| `"bus:/BGM Bus"` | 사용자가 명명한 자식 bus — **공백 그대로** 사용 |
| `"bus:/Music"` | 다른 명명 예시 |

### 캐시 + 볼륨 설정

```cpp
FMOD::Studio::Bus* mBgmBus = nullptr;
FMOD::Studio::Bus* mSfxBus = nullptr;

ck(mSystem->getBus("bus:/BGM Bus", &mBgmBus), "getBus BGM Bus");
ck(mSystem->getBus("bus:/SFX Bus", &mSfxBus), "getBus SFX Bus");

// 슬라이더 변경 시
ck(mBgmBus->setVolume(0.5f), "BGM Bus setVolume");
```

- Bus 는 System 이 소유 — **release 하지 않는다** (Description 과 동일).
- `setVolume(v)` — `v=0.0` 무음, `v=1.0` 원본 그대로, `v>1.0` 부스트 (clipping 위험).
- `getVolume(&v, &finalv)` — 현재 + 페이드/automation 반영 최종값.

---

## 10. 셧다운 순서 — 의존 역순

```cpp
void shutdown() override {
    // 1. ImGui 먼저 — GLFW 콜백이 FMOD 보다 lifecycle 길어야 함
    if (mImGuiCtx) {
        ImGui_ImplGlfwGL3_Shutdown();
        ImGui::DestroyContext(mImGuiCtx);
    }

    // 2. EventInstance — 우리가 소유한 자원 release
    if (mInstance) {
        mInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
        mInstance->release();
    }
    // EventDescription / Bus 는 bank/system 소유 — 포인터만 nullify
    mEventDesc = nullptr;

    // 3. Bank — 로드 역순
    if (mMusicBank)   mMusicBank->unload();
    if (mStringsBank) mStringsBank->unload();
    if (mMasterBank)  mMasterBank->unload();

    // 4. System — 최후
    if (mSystem) mSystem->release();
}
```

규칙: **참조하는 쪽 먼저, 참조되는 쪽 나중에**.

---

## 11. 자주 만나는 함정

### Q. `getEvent("event:/Foo")` 가 `ERR_EVENT_NOTFOUND` 반환

→ `.strings.bank` 가 로드 안 됐거나, 경로 오타 (대소문자/공백/슬래시), 또는 해당 이벤트가 있는 컨텐츠 bank 가 미로드.

### Q. 사운드가 한 두 번만 나오고 안 나옴

→ `mSystem->update()` 누락. 매 프레임 한 번 필수.

### Q. macOS 에서 bank 로드 시 `ERR_FILE_NOTFOUND`

→ macOS GLFW 3.0.4 의 `_GLFW_USE_CHDIR` 가 `glfwInit()` 시 CWD 를 앱 번들 Resources 로 변경. 본 데모들은 `init()` 안에서 `_NSGetExecutablePath` + `chdir(dirname(...))` 로 실행 파일 디렉토리 복귀.

### Q. 같은 SFX 를 빠르게 여러 번 누르면 안 들리는 게 있음

→ `maxchannels` (initialize 인자) 가 너무 작거나, 이벤트의 *max instances* 설정(저작 도구) 이 1 로 잠겨있음.

### Q. one-shot 인스턴스를 release() 했는데 소리가 끝까지 나옴

→ 정상. `release()` 는 "끝나면 정리" 마크일 뿐, 즉시 종료 아님. 즉시 끊으려면 `stop(FMOD_STUDIO_STOP_IMMEDIATE)` 먼저.

### Q. `setParameterByName("Health", ...)` 가 `ERR_EVENT_NOTFOUND`

→ Health 가 global 파라미터인데 EventInstance 에 호출했을 가능성. **Global 은 `mSystem->setParameterByName`**, instance 는 `mInstance->setParameterByName`.

---

## 12. 본 프로젝트 코드 위치

| 데모 | 보여주는 패턴 |
|---|---|
| [apps/audio_demo/demo1/main.cpp](../apps/audio_demo/demo1/main.cpp) | SDK 샘플 Music.bank — 파라미터 메타데이터 **자동 발견** + ImGui 위젯 자동 매핑 + Master bus 볼륨 |
| [apps/audio_demo/demo2/main.cpp](../apps/audio_demo/demo2/main.cpp) | 사용자 제작 bank — **명시적** 이벤트 (BGM/Damaged/Slash) + **one-shot** 패턴 + global parameter (Health) + 2개 명명 bus (BGM/SFX) |
