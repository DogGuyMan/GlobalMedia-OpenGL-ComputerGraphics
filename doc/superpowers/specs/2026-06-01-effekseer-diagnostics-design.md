# Effekseer Diagnostics — `SJH::Diagnostics::EffekseerDiagnostics` 설계 (2026-06-01)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 대상 모듈: `src/diagnostics` (코어 STATIC `SJH::diagnostics`). 대상 데모: `_MyApp_`.
> 목적: `_MyApp_` 의 Effekseer(VFX) **생성·사용 디버깅**을 위한 진단 함수 신설.
> 정책: 의존성 0 접근(접근 A) — 사용자 승인 2026-06-01.

---

## 1. 배경 / 문제

`_MyApp_` 의 Effekseer 흐름(`ResourceRegistry::CreateEffect` → `VFXSystem`(Manager+Renderer) → `EffekseerPlayable`(Play→Handle) → `SpawnVfxInstance`)은 **조용히 깨지는** 지점이 둘 있다:

1. **자원 로드** — `Effekseer::Effect::Create` 는 텍스처가 없어도 *성공*한다(이펙트 객체는 만들어짐). 그래서 `CreateEffect` 반환 != null 만으로는 "텍스처가 실제 해석됐는지"를 알 수 없다. `.efk` 에 박힌 텍스처 상대경로는 `.efk` 디렉토리 기준으로 해석되는데(`CreateEffect` 가 materialPath 미전달, [resource_registry.cpp:271](../../../src/resource_registry/resource_registry.cpp#L271)), export 폴더명과 어긋나면 흰 사각형/비표시로 끝난다. (2026-06-01 에 6종 텍스처를 수동으로 고친 그 트랩.)
2. **런타임 핸들** — `Manager::Play()` 가 `-1`(실패)을 반환하거나, Play 직후 `Exists(handle)==false`(0 frame 만에 종료 = 빈 이펙트/텍스처 전무)인 경우를 호출부가 감지 못 한다.

본 설계는 이 둘을 잡는 **수동 호출형 진단 함수**(기존 `GLDebug::Check*` 패턴)를 추가한다.

## 2. 목표 / 비목표

**목표**
- `.efk` 가 참조하는 텍스처/모델/머티리얼이 base 기준 실존하는지 정적 검증.
- `Play()` 반환 핸들 유효성 + Play 직후 즉시 종료 감지.
- `src/diagnostics` 의 "저수준·의존 최소" 성격 유지 — **GL·Effekseer 둘 다 불의존**, blast radius 0.

**비목표(YAGNI)**
- 시스템/렌더러 상태 검증(`VFXSystem` Manager/Renderer null, view/proj 행렬 정합, Draw 전후 GL state 오염).
- 라이브 통계 dump(핸들 수/인스턴스/draw call).
- Effekseer 로더 introspection(`EffectRef->GetColorImage(i)` 기반 정밀 감지 = 접근 B). 필요 시 추후 Client(`apps/_MyApp_/src/VFX/`)에 별도 보강.

## 3. 핵심 결정

| # | 결정 | 근거 |
|---|---|---|
| D1 | **접근 A — 의존성 0** | `src/diagnostics` 가 Effekseer 헤더를 노출하면 game_deps PUBLIC 링크 → 21개 단위 테스트 포함 모든 consumer 로 전파. 두 기능 모두 Effekseer 타입 없이 구현 가능하므로 회피. |
| D2 | **호출자가 컨텍스트 제공** | handle lifecycle 의 `exists` 는 manager 를 가진 호출자(`EffekseerPlayable`)가 `mManager->Exists()` 로 계산해 넘긴다. 진단 함수는 해석·로깅만. `GLDebug`("호출자가 GL 호출 직후 호출") 패턴과 동일. |
| D3 | **정적 `.efk` 바이너리 파싱** | 로드 *전/후* 무관하게 디스크 존재 기준으로 검증. magic `SKFE` + UTF-16LE 경로 추출. 접근 B(로더 결과 기준)보다 덜 정확하지만 의존성 0 이고 트랩을 정확히 재현. |
| D4 | **static 메서드 클래스** | `GLObjectLog`/`GLDebug` 와 동일. 인스턴스 상태 불필요. |
| D5 | **path API = `std::string_view`(UTF-8)** | std 친화. VFX 호출부의 `u"..."`(char16_t) 경로는 호출 측이 ASCII 협소화(1줄)로 변환해 전달(경로가 ASCII 라 안전). |

## 4. 컴포넌트 & API

신규: `src/diagnostics/effekseer_diagnostics.h` + `effekseer_diagnostics.cpp`.
네임스페이스 `SJH::Diagnostics`. 헤더 가드 `__SJH_DIAGNOSTICS_EFFEKSEER_DIAGNOSTICS_H__`.
헤더 include: `<cstdint>`, `<string_view>` 만. (`.cpp` 가 `<fstream>`/`<filesystem>`/`<vector>`/`<string>`/`<<spdlog>/spdlog.h>`.)

```cpp
namespace SJH::Diagnostics
{
    /// @brief Effekseer 생성·사용 디버깅 진단 (의존성 0 — 파일 I/O + 정수/불리언만).
    class EffekseerDiagnostics
    {
    public:
        // ── 자원 로드 검증 (정적 .efk 파서) ──
        /// @brief .efk 가 참조하는 텍스처/모델/머티리얼이 base 기준 실제 존재하는지 검증.
        /// @param efkPath  .efk 파일 경로 (UTF-8). 슬래시 통일 권장.
        /// @param baseDir  텍스처 해석 기준 디렉토리. 비우면 efkPath 의 부모 디렉토리 사용
        ///                 (= Effekseer::Effect::Create 가 materialPath 없을 때 동작과 동일).
        /// @return 누락된 참조 수(0 = 전부 해석됨). 파일 없음 = -1, magic('SKFE') 불일치 = -2.
        /// @note 로드 전/후 무관 호출 가능 — Effekseer 비의존. 누락 ref 는 각각 spdlog::warn,
        ///       요약 1줄(총 ref 수 + 누락 수) 출력. 호출 측 권장: CreateEffect 직후.
        static int CheckEffectTextures(std::string_view efkPath, std::string_view baseDir = {});

        // ── 런타임 핸들 lifecycle ──
        /// @brief Manager::Play() 반환 핸들 검사 — Play 호출 직후.
        /// @param handle Play() 반환값 (Effekseer::Handle = int32_t). 음수 = 재생 실패.
        /// @param tag    로그 식별자(effect key 등). 비우면 생략.
        /// @return 유효(>=0) 시 true. 실패 시 spdlog::warn("manager/effect null 또는 maxSprites 초과 가능") 후 false.
        static bool CheckPlayHandle(int32_t handle, std::string_view tag = {});

        /// @brief Play 직후 인스턴스가 즉시 사라졌는지 감지.
        /// @param handle Play() 반환 핸들.
        /// @param exists 호출자가 mManager->Exists(handle) 로 계산해 전달.
        /// @param tag    로그 식별자.
        /// @return handle>=0 && exists 면 true. handle 유효한데 !exists 면
        ///         spdlog::warn("Play 직후 즉시 종료 — 빈 이펙트/텍스처 전무 의심") 후 false.
        ///         handle<0 면 조용히 false(CheckPlayHandle 이 이미 보고).
        static bool CheckHandleAlive(int32_t handle, bool exists, std::string_view tag = {});
    };
}
```

## 5. `CheckEffectTextures` 알고리즘 (`.cpp`)

1. `std::ifstream(efkPath, std::ios::binary)` 로 열기. 실패 → `spdlog::warn` + return **-1**.
2. 전체 바이트를 `std::vector<unsigned char>` 로 읽기.
3. 첫 4바이트가 `'S','K','F','E'`(0x53 0x4B 0x46 0x45) 인지 확인. 아니면 `spdlog::warn` + return **-2**.
4. UTF-16LE 런 스캔: `(byte ∈ [0x20,0x7e]) , 0x00` 쌍이 **3자 이상** 연속인 구간을 모아 narrow string 으로 디코드(저바이트만 취함).
5. 확장자 필터(대소문자 무시): `.png` / `.efkefc` / `.efkmat` / `.efkmodel` 로 끝나는 것만. `\` → `/` 정규화. 중복 제거(삽입 순서 보존).
6. base 결정: `baseDir` 비었으면 `std::filesystem::path(efkPath).parent_path()`.
7. 각 ref: `resolved = std::filesystem::path(base) / ref` → `std::filesystem::exists(resolved)`( `../` 자동 해석). 누락 시 `spdlog::warn("[EfkDiag] {efkName} 텍스처 누락: {ref} → {resolved}")`, `missing++`.
8. 요약: `missing==0` 이면 `spdlog::info("[EfkDiag] {efkName}: {n} refs 전부 OK")`, 아니면 `spdlog::warn("[EfkDiag] {efkName}: {missing}/{n} refs 누락")`. return `missing`.

> 이 동작은 2026-06-01 검증에 쓴 파이썬 스크립트와 1:1 대응(magic/UTF-16/base 해석/누락 집계).

## 6. 통합(호출) 지점 — 본 설계 범위 포함

1. **로드 검증** — [main.cpp](../../../apps/_MyApp_/main.cpp) 의 6종 VFX 로드 루프([:245-250](../../../apps/_MyApp_/main.cpp#L245-L250)) 안, 각 `CreateEffect` 성공 직후 `EffekseerDiagnostics::CheckEffectTextures(narrowPath)` 호출. `u"resources/vfx/dust.efk"`(char16_t) → ASCII 협소화 1줄(`std::string s; for (auto* p=path; *p; ++p) s.push_back(char(*p));`)로 narrow 경로 생성, 또는 `kTestVfx` 테이블에 narrow `const char*` 필드 1개 추가.
2. **핸들 lifecycle** — [EffekseerPlayable::OnPlay](../../../apps/_MyApp_/src/VFX/EffekseerPlayable.cpp#L27) 의 `mHandle = mManager->Play(...)` 직후:
   ```cpp
   EffekseerDiagnostics::CheckPlayHandle(mHandle, /*tag*/"effekseer");
   EffekseerDiagnostics::CheckHandleAlive(mHandle, mManager->Exists(mHandle), "effekseer");
   ```
   `EffekseerPlayable.cpp` 는 `#include "diagnostics/gl_log.h"` 형식대로 `#include "diagnostics/effekseer_diagnostics.h"` 추가. (Client 파일 수정 발생 — 승인됨.)

> 통합은 "디버그 가시성" 목적의 최소 배선. 추후 `SpawnVfxInstance` 등 다른 진입점에도 동일 패턴 적용 가능.

## 7. CMake / 빌드

- `src/diagnostics/CMakeLists.txt` 의 STATIC lib 소스 목록에 `effekseer_diagnostics.cpp` 1줄 추가.
- **새 link 의존 없음**: spdlog 는 이미 `PUBLIC` 링크, `<filesystem>` 는 clang(macOS)/MSVC v142·v143 의 C++17 에서 추가 라이브러리 불요. (GCC<9 의 `-lstdc++fs` 는 본 프로젝트 타겟 컴파일러에 해당 없음.)
- `EffekseerPlayable.cpp`(Client) 는 이미 `SJH::engine` 우산으로 `SJH::diagnostics` 를 링크하므로 추가 link 불요.

## 8. 크로스플랫폼 (CLAUDE.md 규칙 준수)

- 파일 I/O 바이너리 모드(`std::ios::binary`). `int32_t`(Handle). 경로 슬래시 통일(ref 정규화). `windows.h` 불필요.

## 9. 테스트 입장

- `no_auto_tests` 정책 — 자동 단위 테스트 추가 안 함. 검증 = **빌드 + `_MyApp_` 실행 후 콘솔 로그 확인**.
- (선택) `CheckEffectTextures` 는 순수 함수라 Catch2 단위 테스트가 쉽다 — 사용자 요청 시 `<test>/test_effekseer_diagnostics.cpp` 추가 가능(고정 .efk 픽스처 + missing 케이스). 기본은 미포함.

## 10. 파일 매니페스트

| 파일 | 변경 |
|---|---|
| `src/diagnostics/effekseer_diagnostics.h` | 신규 — 클래스 + 3 함수 선언 |
| `src/diagnostics/effekseer_diagnostics.cpp` | 신규 — 구현(.efk 파서 + 핸들 검사) |
| `src/diagnostics/CMakeLists.txt` | 소스 1줄 추가 |
| `apps/_MyApp_/main.cpp` | 로드 루프에 `CheckEffectTextures` 호출 + narrow 경로 |
| `apps/_MyApp_/src/VFX/EffekseerPlayable.cpp` | `OnPlay` 에 `CheckPlayHandle`+`CheckHandleAlive` + include |
