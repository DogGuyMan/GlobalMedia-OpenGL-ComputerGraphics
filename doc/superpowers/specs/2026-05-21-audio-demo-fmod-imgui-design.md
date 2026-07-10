# audio_demo — FMOD Studio + ImGui 파라미터 데모 — 설계 스펙

> 작성일: 2026-05-21

## 목표

신규 챕터 `apps/audio_demo/` 를 추가해 **FMOD Studio API 의 bank 로딩 + 파라미터 변경** 을 ImGui 슬라이더/콤보로 인터랙티브하게 시연한다. SDK 번들의 `Music.bank` 를 데모 소스로 사용, 이벤트 `event:/Music/Level 01` 의 파라미터(`Intensity`, `Progression` 등) 를 자동 발견해 타입별 위젯으로 노출한다.

학습 목적의 single-file 챕터 — sb7::application + FMOD + ImGui 한 파일 안에서 라이프사이클 완결. macOS(Ninja) + Windows(MSVC) 양쪽에서 빌드·실행되어야 한다.

## 배경 / 제약

- FMOD Core + Studio 는 `cmake/Dependency.cmake` 에서 이미 등록되어 `game_deps` INTERFACE 자동 합류 (조건부 — 헤더 존재 시). audio_demo 는 `project_deps + game_deps` 만 링크하면 fmod / fmodstudio 둘 다 전파받는다.
- macOS rpath `@loader_path` 는 `cmake/Dependency.cmake` 에서 전역 설정 — 챕터별 추가 작업 불필요.
- ImGui 는 `SJH::imgui` 정적 라이브러리로 `project_deps` 에 합류되어 있음 — 별도 링크 불필요. 챕터는 imguitest 의 통합 패턴(GLFW + OpenGL3 backend, GL 4.1 Core / GLSL 410) 을 그대로 따른다.
- FMOD 동적 라이브러리(`libfmod.dylib`/`libfmodstudio.dylib`, `fmod.dll`/`fmodstudio.dll`) 는 dynamic-only 라 실행 파일 옆으로 POST_BUILD copy 가 필수. macOS 의 dead_strip_dylibs 로 미사용 챕터(예: imguitest) 는 자동 prune 되므로 안전하지만, audio_demo 는 실제 호출하므로 copy 가 발동.
- 데모 bank 출처: `resources/installer/macos/FMOD Programmers API/api/studio/examples/media/` (SDK 추출 폴더 — `.gitignore` 로 차단). 챕터 안 `apps/audio_demo/resources/banks/` 로 복사된 산출물도 FMOD 라이선스상 재배포 금지 → 추가 `.gitignore` 등록.
- `Master.strings.bank` 의 strings 추출로 확인: `Music/Level 0`, `Intensity`, `Progression`, `Character` 등의 문자열 실재. 설계 가정 일치.

## 결정 사항

| 항목 | 결정 |
|---|---|
| 챕터 위치 | `apps/audio_demo/` (신규) |
| 챕터 패턴 | 패턴 B — single `main.cpp` + `DECLARE_MAIN` |
| FMOD 통합 | C++ 헤더 (`fmod.hpp`, `fmod_studio.hpp`) + `FMOD::Studio::System` 직접 사용 |
| ImGui 통합 | `SJH::imgui` 직접 사용, imguitest 의 init/shutdown 흐름 그대로 |
| 데모 이벤트 | `event:/Music/Level 01` 하드코드 + 실패 시 bank 안의 첫 이벤트 fallback |
| 파라미터 노출 | **자동 발견** — `getParameterDescriptionCount` + `getParameterDescriptionByIndex` |
| 위젯 매핑 | continuous → `SliderFloat`, labeled discrete → `Combo`, switch(0/1 discrete) → `Checkbox`, 그 외 discrete → `SliderInt` |
| Transport 컨트롤 | Play / Stop / Pause 버튼 + 마스터 볼륨 슬라이더 (별도 VCA 사용 X — 시스템 마스터 채널) |
| 에러 처리 | `FMOD_RESULT` 검사 → `FMOD_ErrorString` → stderr 출력 + 챕터 종료 (graceful degradation 안 함, 학습 예제) |
| GL 정책 | GL 4.1 Core / GLSL 410 (`init()` override 에서 강제) — 프로젝트 정책 [[glsl_410_project_policy]] |
| 활성화 | `apps/CMakeLists.txt` 에 `add_subdirectory(audio_demo)` 라인 추가 (한 번에 하나 활성화 원칙 — 기존 활성 챕터와 교체) |

## 파일 레이아웃

```
apps/audio_demo/
├── CMakeLists.txt
├── main.cpp                       ← sb7::application + FMOD + ImGui (단일 파일)
└── resources/
    └── banks/
        ├── Master.bank
        ├── Master.strings.bank
        └── Music.bank
```

`.gitignore` 추가:
```
# FMOD 샘플 bank — 라이선스상 재배포 제약 (doc/FMOD_Setup.md §7 와 동일 사유)
apps/*/resources/banks/
```

## application 클래스 — 라이프사이클

### 멤버 (개념)

```cpp
namespace SJH::AudioDemo {
class application : public sb7::application {
    // FMOD
    FMOD::Studio::System*      mSystem      = nullptr;
    FMOD::Studio::Bank*        mMasterBank  = nullptr;
    FMOD::Studio::Bank*        mStringsBank = nullptr;
    FMOD::Studio::Bank*        mMusicBank   = nullptr;
    FMOD::Studio::EventDescription* mEventDesc = nullptr;
    FMOD::Studio::EventInstance*    mInstance  = nullptr;

    // 파라미터 캐시 (자동 발견 결과)
    struct ParamCache {
        std::string name;
        FMOD_STUDIO_PARAMETER_ID id;
        enum Kind { Continuous, Labeled, Switch, DiscreteInt } kind;
        float minValue, maxValue;
        std::vector<std::string> labels;     // Labeled 일 때만
        float currentValue;                   // setParameter 직후 캐시
    };
    std::vector<ParamCache> mParams;

    // 마스터 버스 (볼륨 컨트롤용 — root bus path "bus:/")
    FMOD::Studio::Bus* mMasterBus = nullptr;

    // UI 상태
    float mMasterVolume = 1.0f;
    bool  mPaused = false;
};
}
```

### `init()` (sb7 hook — GL 옵션 설정)

GL 4.1 Core 강제 (`info.majorVersion=4; info.minorVersion=1; info.flags.core=1;`), 윈도우 타이틀, 사이즈 설정.

### `startup()`

1. `FMOD::Studio::System::create(&mSystem)` → `initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr)`
2. `mSystem->loadBankFile("resources/banks/Master.bank", FMOD_STUDIO_LOAD_BANK_NORMAL, &mMasterBank)`
3. `loadBankFile("resources/banks/Master.strings.bank", ..., &mStringsBank)`
4. `loadBankFile("resources/banks/Music.bank", ..., &mMusicBank)`
5. `mSystem->getEvent("event:/Music/Level 01", &mEventDesc)` — 실패 시 `mMusicBank->getEventList(...)` 의 첫 항목 사용
6. **파라미터 메타데이터 캐시** (아래 알고리즘) → `mParams` 채움
7. `mEventDesc->createInstance(&mInstance)` → `mInstance->start()`
8. `mSystem->getBus("bus:/", &mMasterBus)` — 마스터 버스 캐시 (볼륨 컨트롤용)
9. ImGui context + GLFW/OpenGL3 backend init (imguitest 와 동일)

### `render(double currentTime)`

1. `mSystem->update()` — FMOD 비동기 작업 처리 (매 프레임 필수)
2. ImGui new frame
3. **파라미터 위젯 루프** — `mParams` 각 항목마다 kind 별로 위젯 호출, 값 변화 감지 시 `mInstance->setParameterByID(p.id, newValue)` + `p.currentValue = newValue`
4. Transport 박스: Play(`mInstance->start()`) / Stop(`mInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT)`) / Pause 토글(`mInstance->setPaused(mPaused)`)
5. Master 볼륨 — `mMasterBus->setVolume(mMasterVolume)` (값 변화 시에만)
6. `glClear(GL_COLOR_BUFFER_BIT)` (검은 배경 — 시각 요소 없음)
7. ImGui render + GLFW swap

### `shutdown()`

ImGui backend/context shutdown → `mInstance->release()` → bank `unload()` 3개 → `mSystem->release()`.

## 파라미터 자동 발견 — 알고리즘

```cpp
int n = 0;
mEventDesc->getParameterDescriptionCount(&n);
for (int i = 0; i < n; ++i) {
    FMOD_STUDIO_PARAMETER_DESCRIPTION desc{};
    mEventDesc->getParameterDescriptionByIndex(i, &desc);

    ParamCache p;
    p.name      = desc.name;
    p.id        = desc.id;
    p.minValue  = desc.minimum;
    p.maxValue  = desc.maximum;

    bool labeled  = (desc.flags & FMOD_STUDIO_PARAMETER_LABELED);
    bool discrete = (desc.flags & FMOD_STUDIO_PARAMETER_DISCRETE);

    if (labeled) {
        p.kind = ParamCache::Labeled;
        // 0..max 범위로 label 문자열 수집
        for (int v = (int)desc.minimum; v <= (int)desc.maximum; ++v) {
            char buf[128]{};
            mEventDesc->getParameterLabelByID(desc.id, v, buf, sizeof(buf), nullptr);
            p.labels.emplace_back(buf);
        }
    } else if (discrete) {
        p.kind = (desc.minimum == 0.0f && desc.maximum == 1.0f)
                    ? ParamCache::Switch
                    : ParamCache::DiscreteInt;
    } else {
        p.kind = ParamCache::Continuous;
    }

    // 현재 값 시드 — 초기 query
    float current = desc.defaultvalue;
    p.currentValue = current;

    mParams.push_back(std::move(p));
}
```

UI 측 분기 (의사 코드):
```cpp
for (auto& p : mParams) {
    float v = p.currentValue;
    bool changed = false;
    switch (p.kind) {
        case Continuous: changed = ImGui::SliderFloat(p.name.c_str(), &v, p.minValue, p.maxValue); break;
        case Labeled: {
            int idx = (int)v;
            changed = ImGui::Combo(p.name.c_str(), &idx, /* string array from p.labels */);
            v = (float)idx;
        } break;
        case Switch: {
            bool b = v >= 0.5f;
            changed = ImGui::Checkbox(p.name.c_str(), &b);
            v = b ? 1.0f : 0.0f;
        } break;
        case DiscreteInt: {
            int iv = (int)v;
            changed = ImGui::SliderInt(p.name.c_str(), &iv, (int)p.minValue, (int)p.maxValue);
            v = (float)iv;
        } break;
    }
    if (changed) {
        mInstance->setParameterByID(p.id, v);
        p.currentValue = v;
    }
}
```

## CMakeLists.txt — `apps/audio_demo/`

```cmake
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
add_executable(${CHAPTER_NAME} main.cpp)

# fmod / fmodstudio / SJH::imgui 모두 project_deps + game_deps INTERFACE 전파
target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps game_deps)

# resources/ POST_BUILD copy (기존 패턴)
add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources)

# FMOD 동적 라이브러리 — 실행 파일 옆으로 (doc/FMOD_Setup.md §6)
if(TARGET fmod)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmod> $<TARGET_FILE_DIR:${CHAPTER_NAME}>)
endif()
if(TARGET fmodstudio)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmodstudio> $<TARGET_FILE_DIR:${CHAPTER_NAME}>)
endif()
```

## .bank 파일 배치 절차 (한 번)

1. `apps/audio_demo/resources/banks/` 디렉토리 생성
2. `resources/installer/macos/FMOD Programmers API/api/studio/examples/media/` 에서 3개 복사:
   - `Master.bank`
   - `Master.strings.bank`
   - `Music.bank`
3. `.gitignore` 에 `apps/*/resources/banks/` 추가 (FMOD 샘플 bank 재배포 금지)

Windows 빌드 시점도 같은 위치 — 헤더와 마찬가지로 mac 에서 복사한 게 그대로 통용 (`.bank` 는 플랫폼 무관 컨테이너).

## 에러 처리

학습 챕터이므로 graceful degradation 없이 첫 실패에서 종료:

```cpp
auto ck = [](FMOD_RESULT r, const char* where) {
    if (r != FMOD_OK) {
        std::fprintf(stderr, "[FMOD] %s: %s\n", where, FMOD_ErrorString(r));
        std::exit(1);
    }
};
ck(mSystem->loadBankFile(..., &mMusicBank), "loadBankFile Music");
```

`event:/Music/Level 01` 이 실패할 때만 fallback (bank 안의 첫 이벤트 사용) — 미래에 SDK 가 bank 구조를 바꿔도 데모가 죽지 않도록.

## 미적용 (YAGNI 컷)

- 3D positioning — 음악이라 불필요
- 멀티 이벤트 인스턴스 — 1개 고정
- Dialogue / VO bank — 다국어 처리 범위 밖
- bank 핫리로드 — 학습 예제 스코프 밖
- VCA 노출 — 마스터 채널 그룹 볼륨만
- 시각화 (스펙트럼/파형) — 오디오 + ImGui 최소 시연이 목적

## 검증 기준 (완료 정의)

1. `cmake --preset ninja -DENABLE_TESTING=OFF` configure 통과
2. `cmake --build --preset ninja --target audio_demo` 빌드 통과 (link 에러 X)
3. `cd build_ninja/apps/audio_demo && ./audio_demo` 실행
   - 창 열림 + ImGui 표시
   - 음악 재생 (Master.bank/Music.bank 로드 성공)
   - 슬라이더 조작 → 청각적으로 파라미터 변화 인지 가능
   - Stop → 음악 정지, Play → 재시작, Pause → 일시정지
   - 종료 시 crash 없음
4. macOS 에서 `otool -L ./audio_demo | grep fmod` 결과에 `@rpath/libfmod.dylib`, `@rpath/libfmodstudio.dylib` 둘 다 표시 (dead_strip 안 됨 — 실제 호출 있으니까)
5. Windows 빌드 — VS2022 Debug/Release 양쪽 link 통과 (실행 검증은 Windows 환경 분리)

## 후속 작업 (별도 spec)

- FMOD 호출을 `SJH::audio` 모듈로 일반화 (다른 챕터/엔진에서 재사용)
- `.bank` 라이브 핫리로드 (개발 편의)
- Audio 의존 게임 챕터 (ecs_demo 등) 연동
