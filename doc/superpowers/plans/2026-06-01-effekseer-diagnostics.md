# Effekseer Diagnostics Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `src/diagnostics` 에 의존성 0(Effekseer·GL 불의존) 진단 클래스 `EffekseerDiagnostics` 를 신설하고 `_MyApp_` 의 VFX 로드/재생 흐름에 배선해, 텍스처 누락과 핸들 실패를 콘솔 로그로 즉시 드러낸다.

**Architecture:** 기존 `GLDebug`/`GLObjectLog` 와 동일한 수동 호출형 static 메서드 클래스. 자원 검증은 `.efk` 바이너리(magic `SKFE` + UTF-16LE 경로)를 직접 파싱해 `std::filesystem::exists` 로 해석(Effekseer 불필요). 핸들 검증은 호출자(`EffekseerPlayable`)가 `mManager->Exists()` 결과를 넘기고 진단 함수는 해석·로깅만 한다.

**Tech Stack:** C++17, `<filesystem>`, `<fstream>`, spdlog (이미 `SJH::diagnostics` 가 PUBLIC 링크). 새 link 의존 없음.

**정책:** `no_auto_tests` — 자동 단위 테스트 미작성. 검증 = 빌드 + `_MyApp_` 실행 후 콘솔 `[EfkDiag]` 로그 확인.

**정본 spec:** `doc/superpowers/specs/2026-06-01-effekseer-diagnostics-design.md`

---

## File Structure

| 파일 | 책임 |
|---|---|
| `src/diagnostics/effekseer_diagnostics.h` (신규) | `EffekseerDiagnostics` 3 static 함수 선언. include = `<cstdint>`,`<string_view>` 만. |
| `src/diagnostics/effekseer_diagnostics.cpp` (신규) | `.efk` 파서 + 핸들 검사 구현. |
| `src/diagnostics/CMakeLists.txt` (수정) | STATIC lib 소스에 `.cpp` 1줄 추가. |
| `apps/_MyApp_/src/VFX/EffekseerPlayable.cpp` (수정) | `OnPlay` 에 핸들 검사 2줄 + include. |
| `apps/_MyApp_/main.cpp` (수정) | 6종 VFX 로드 루프에 텍스처 검증 + include. |

---

## Task 1: 진단 모듈 신설 (`EffekseerDiagnostics`) + 컴파일 검증

**Files:**
- Create: `src/diagnostics/effekseer_diagnostics.h`
- Create: `src/diagnostics/effekseer_diagnostics.cpp`
- Modify: `src/diagnostics/CMakeLists.txt:1-7`

- [ ] **Step 1: 헤더 작성** — `src/diagnostics/effekseer_diagnostics.h`

```cpp
#ifndef __SJH_DIAGNOSTICS_EFFEKSEER_DIAGNOSTICS_H__
#define __SJH_DIAGNOSTICS_EFFEKSEER_DIAGNOSTICS_H__

#include <cstdint>
#include <string_view>

namespace SJH::Diagnostics
{
    /// @brief Effekseer 생성·사용 디버깅 진단 (의존성 0 — 파일 I/O + 정수/불리언만).
    /// @details GLDebug/GLObjectLog 와 동일한 수동 호출형 static 진단. GL·Effekseer 불의존.
    ///          모든 출력은 spdlog::warn/info — 빌드/실행을 막지 않는 진단 신호.
    class EffekseerDiagnostics
    {
    public:
        /// @brief .efk 가 참조하는 텍스처/모델/머티리얼이 base 기준 실제 존재하는지 검증.
        /// @param efkPath  .efk 파일 경로 (UTF-8). 슬래시 통일 권장.
        /// @param baseDir  텍스처 해석 기준 디렉토리. 비우면 efkPath 의 부모 디렉토리 사용
        ///                 (= Effekseer::Effect::Create 가 materialPath 없을 때 동작과 동일).
        /// @return 누락된 참조 수(0 = 전부 해석됨). 파일 없음 = -1, magic('SKFE') 불일치 = -2.
        /// @note 로드 전/후 무관 호출 가능 — Effekseer 비의존. 누락 ref 는 각각 spdlog::warn.
        static int CheckEffectTextures(std::string_view efkPath, std::string_view baseDir = {});

        /// @brief Manager::Play() 반환 핸들 검사 — Play 호출 직후.
        /// @param handle Play() 반환값 (Effekseer::Handle = int32_t). 음수 = 재생 실패.
        /// @param tag    로그 식별자(effect key 등). 비우면 생략.
        /// @return 유효(>=0) 시 true. 실패 시 spdlog::warn 후 false.
        static bool CheckPlayHandle(int32_t handle, std::string_view tag = {});

        /// @brief Play 직후 인스턴스가 즉시 사라졌는지 감지.
        /// @param handle Play() 반환 핸들.
        /// @param exists 호출자가 mManager->Exists(handle) 로 계산해 전달.
        /// @param tag    로그 식별자.
        /// @return handle>=0 && exists 면 true. handle 유효한데 !exists 면 warn 후 false.
        ///         handle<0 면 조용히 false(CheckPlayHandle 이 이미 보고).
        static bool CheckHandleAlive(int32_t handle, bool exists, std::string_view tag = {});
    };
}

#endif // __SJH_DIAGNOSTICS_EFFEKSEER_DIAGNOSTICS_H__
```

- [ ] **Step 2: 구현 작성** — `src/diagnostics/effekseer_diagnostics.cpp`

```cpp
#include "diagnostics/effekseer_diagnostics.h"

#include <<spdlog>/spdlog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace SJH::Diagnostics
{
    namespace
    {
        /// 확장자(소문자, ASCII) 일치 검사 — 텍스처/모델/머티리얼 참조만 추림.
        bool HasResourceExtension(const std::string &s)
        {
            std::string lower = s;
            for (char &c : lower)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            static const char *kExts[] = {".png", ".efkefc", ".efkmat", ".efkmodel"};
            for (const char *e : kExts)
            {
                const std::string ext = e;
                if (lower.size() >= ext.size() &&
                    lower.compare(lower.size() - ext.size(), ext.size(), ext) == 0)
                    return true;
            }
            return false;
        }
    } // namespace

    int EffekseerDiagnostics::CheckEffectTextures(std::string_view efkPath, std::string_view baseDir)
    {
        const std::string pathStr(efkPath);
        std::ifstream ifs(pathStr, std::ios::binary);
        if (!ifs)
        {
            spdlog::warn("[EfkDiag] .efk 열기 실패: {}", pathStr);
            return -1;
        }
        const std::vector<unsigned char> data((std::istreambuf_iterator<char>(ifs)),
                                              std::istreambuf_iterator<char>());

        // magic 'SKFE' (Effekseer 바이너리)
        if (data.size() < 4 || data[0] != 'S' || data[1] != 'K' || data[2] != 'F' || data[3] != 'E')
        {
            spdlog::warn("[EfkDiag] magic 'SKFE' 불일치 (.efk 아님?): {}", pathStr);
            return -2;
        }

        // UTF-16LE 런 스캔 — (ascii, 0x00) 쌍이 3자 이상 연속인 구간.
        std::vector<std::string> refs;
        std::string cur;
        auto flush = [&]() {
            if (cur.size() >= 3 && HasResourceExtension(cur))
            {
                for (char &c : cur)
                    if (c == '\\') c = '/'; // 슬래시 통일
                if (std::find(refs.begin(), refs.end(), cur) == refs.end())
                    refs.push_back(cur);
            }
            cur.clear();
        };
        for (std::size_t i = 0; i + 1 < data.size(); i += 2)
        {
            const unsigned char lo = data[i];
            const unsigned char hi = data[i + 1];
            if (hi == 0x00 && lo >= 0x20 && lo <= 0x7e)
                cur.push_back(static_cast<char>(lo));
            else
                flush();
        }
        flush();

        const fs::path base = baseDir.empty()
                                  ? fs::path(pathStr).parent_path()
                                  : fs::path(std::string(baseDir));
        const std::string efkName = fs::path(pathStr).filename().string();

        int missing = 0;
        for (const std::string &ref : refs)
        {
            const fs::path  resolved = base / ref;
            std::error_code ec;
            if (!fs::exists(resolved, ec))
            {
                spdlog::warn("[EfkDiag] {} 텍스처 누락: {} → {}", efkName, ref, resolved.string());
                ++missing;
            }
        }
        if (missing == 0)
            spdlog::info("[EfkDiag] {}: {} refs 전부 OK", efkName, refs.size());
        else
            spdlog::warn("[EfkDiag] {}: {}/{} refs 누락", efkName, missing, refs.size());
        return missing;
    }

    bool EffekseerDiagnostics::CheckPlayHandle(int32_t handle, std::string_view tag)
    {
        if (handle < 0)
        {
            spdlog::warn("[EfkDiag] Play 실패 (handle={}) tag={} — manager/effect null 또는 maxSprites 초과 가능",
                         handle, tag);
            return false;
        }
        return true;
    }

    bool EffekseerDiagnostics::CheckHandleAlive(int32_t handle, bool exists, std::string_view tag)
    {
        if (handle < 0)
            return false; // CheckPlayHandle 이 이미 보고
        if (!exists)
        {
            spdlog::warn("[EfkDiag] handle={} tag={} 가 Play 직후 즉시 종료 — 빈 이펙트/텍스처 전무 의심",
                         handle, tag);
            return false;
        }
        return true;
    }
}
```

- [ ] **Step 3: CMakeLists 소스 추가** — `src/diagnostics/CMakeLists.txt`

기존 `add_library(...)` 블록(1-7행)에 `effekseer_diagnostics.cpp` 를 추가:

```cmake
add_library(sjhopengl_diagnostics STATIC
    gl_log.cpp
    uniform_diagnostics.cpp
    gl_state_fields.cpp
    gl_state_log.cpp
    gl_validate.cpp
    effekseer_diagnostics.cpp
)
```

- [ ] **Step 4: 진단 모듈 단독 컴파일 검증**

Run: `cmake --build --preset ninja --target sjhopengl_diagnostics`
Expected: `CMakeLists.txt` 변경으로 자동 재configure 후 빌드 성공. 에러/경고(-Werror) 없음. `effekseer_diagnostics.cpp.o` 생성.

- [ ] **Step 5: 커밋**

```bash
git add src/diagnostics/effekseer_diagnostics.h src/diagnostics/effekseer_diagnostics.cpp src/diagnostics/CMakeLists.txt
git commit -m "feat(diagnostics): Effekseer 진단 EffekseerDiagnostics 신설 (의존성 0)"
```

---

## Task 2: 핸들 lifecycle 배선 (`EffekseerPlayable::OnPlay`)

**Files:**
- Modify: `apps/_MyApp_/src/VFX/EffekseerPlayable.cpp:1-4` (include), `:27-32` (OnPlay)

- [ ] **Step 1: 진단 헤더 include 추가**

`EffekseerPlayable.cpp` 상단의 기존 include 블록:

```cpp
#include "EffekseerPlayable.h"

#include "scene/actor.h"        // GetOwner() (FollowOwner 정책에서 Transform 조회)
#include <<spdlog>/spdlog.h>
```

을 다음으로 교체(진단 헤더 1줄 추가):

```cpp
#include "EffekseerPlayable.h"

#include "diagnostics/effekseer_diagnostics.h"   // Effekseer 핸들 lifecycle 진단
#include "scene/actor.h"        // GetOwner() (FollowOwner 정책에서 Transform 조회)
#include <<spdlog>/spdlog.h>
```

- [ ] **Step 2: `OnPlay` 에 핸들 검사 2줄 추가**

기존 `OnPlay`:

```cpp
    void EffekseerPlayable::OnPlay()
    {
        if (mManager.Get() == nullptr || mEffect == nullptr) return;
        mHandle = mManager->Play(mEffect->Ref(),
                                 ::Effekseer::Vector3D(mSpawnPos[0], mSpawnPos[1], mSpawnPos[2]));
    }
```

을 다음으로 교체:

```cpp
    void EffekseerPlayable::OnPlay()
    {
        if (mManager.Get() == nullptr || mEffect == nullptr) return;
        mHandle = mManager->Play(mEffect->Ref(),
                                 ::Effekseer::Vector3D(mSpawnPos[0], mSpawnPos[1], mSpawnPos[2]));
        // 진단 — Play 실패(-1) / Play 직후 즉시 종료(빈 이펙트·텍스처 전무) 감지.
        SJH::Diagnostics::EffekseerDiagnostics::CheckPlayHandle(mHandle, "effekseer");
        SJH::Diagnostics::EffekseerDiagnostics::CheckHandleAlive(
            mHandle, mManager->Exists(mHandle), "effekseer");
    }
```

- [ ] **Step 3: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 빌드 성공. (`EffekseerPlayable.cpp` 는 `SJH::engine` 우산을 통해 이미 `SJH::diagnostics` 링크 — 추가 CMake 불요.) 증분 빌드 stale lib 의심 시 `touch apps/_MyApp_/src/VFX/EffekseerPlayable.cpp` 후 재빌드.

- [ ] **Step 4: 커밋**

```bash
git add apps/_MyApp_/src/VFX/EffekseerPlayable.cpp
git commit -m "feat(vfx): EffekseerPlayable::OnPlay 핸들 lifecycle 진단 배선"
```

---

## Task 3: 텍스처 로드 검증 배선 (`main.cpp` 6종 로드 루프)

**Files:**
- Modify: `apps/_MyApp_/main.cpp:23-27` (include), `:245-251` (로드 루프)

- [ ] **Step 1: 진단 헤더 include 추가**

`main.cpp` 의 기존 include(23행 부근):

```cpp
#include "apps/_MyApp_/src/VFX/ParticleStage.h"

#include "apps/_MyApp_/src/Spawns/OneShotSweeper.h"
#include "apps/_MyApp_/src/Spawns/VfxInstance.h"
#include "apps/_MyApp_/src/UI/VfxSpawnLayer.h"
```

을 다음으로 교체(진단 헤더 1줄 추가):

```cpp
#include "apps/_MyApp_/src/VFX/ParticleStage.h"

#include "diagnostics/effekseer_diagnostics.h"   // VFX 텍스처 로드 검증
#include "apps/_MyApp_/src/Spawns/OneShotSweeper.h"
#include "apps/_MyApp_/src/Spawns/VfxInstance.h"
#include "apps/_MyApp_/src/UI/VfxSpawnLayer.h"
```

- [ ] **Step 2: 로드 루프에 텍스처 검증 추가**

기존 루프(245-251행):

```cpp
				for (const auto &v : kTestVfx)
				{
					if (auto *eff = reg.CreateEffect(vfxs.GetManager(), v.key, v.path))
						vfxEntries.push_back({v.key, eff});
					else
						spdlog::warn("[vfx-test] load failed: {}", v.key);
				}
```

을 다음으로 교체(로드 성공 시 텍스처 경로 검증):

```cpp
				for (const auto &v : kTestVfx)
				{
					if (auto *eff = reg.CreateEffect(vfxs.GetManager(), v.key, v.path))
					{
						vfxEntries.push_back({v.key, eff});
						// 진단 — .efk 가 참조하는 텍스처가 실제 해석되는지 검증 (u16 경로 → ASCII narrow).
						std::string narrow;
						for (const char16_t *p = v.path; *p; ++p)
							narrow.push_back(static_cast<char>(*p));
						SJH::Diagnostics::EffekseerDiagnostics::CheckEffectTextures(narrow);
					}
					else
						spdlog::warn("[vfx-test] load failed: {}", v.key);
				}
```

- [ ] **Step 3: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 빌드 성공.

- [ ] **Step 4: 커밋**

```bash
git add apps/_MyApp_/main.cpp
git commit -m "feat(vfx): main 로드 루프에 .efk 텍스처 검증 배선"
```

---

## Task 4: 실행 검증 (콘솔 `[EfkDiag]` 로그 관찰)

**Files:** (없음 — 실행만)

- [ ] **Step 1: 실행**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
(GLFW 창이 뜸. 시작 직후 stdout 의 `[EfkDiag]` 로그를 확인. 창은 GUI 라 에이전트가 직접 못 보면 사용자가 관찰하거나, 시작 로그만 보려면 짧은 timeout 으로 stdout 캡처.)

- [ ] **Step 2: 로드 검증 로그 확인**

Expected (텍스처는 2026-06-01 에 이미 정위치로 이동됨 → 전부 OK):
```
[EfkDiag] dust.efk: 1 refs 전부 OK
[EfkDiag] hit.efk: 13 refs 전부 OK
[EfkDiag] laser.efk: 6 refs 전부 OK
[EfkDiag] orbital_background.efk: 1 refs 전부 OK
[EfkDiag] slash.efk: 2 refs 전부 OK
[EfkDiag] summon.efk: 1 refs 전부 OK
```
(만약 누락이 있으면 `[EfkDiag] <name> 텍스처 누락: <ref> → <resolved>` 가 ref 별로 출력 → 텍스처 배치 트랩 재발 신호.)

- [ ] **Step 3: 핸들 lifecycle 로그 확인 (인터랙티브)**

마우스 좌클릭(바닥) 또는 "VFX Test" 창에서 이펙트 선택 후 클릭 → `SpawnVfxInstance` → `EffekseerPlayable::OnPlay` 발동.
Expected: 정상 재생 시 `[EfkDiag]` 경고 **없음**. 빈 이펙트/실패 시 `Play 실패` 또는 `즉시 종료` 경고 출력.

- [ ] **Step 4: (검증 완료 — 코드 변경 없음, 커밋 불요)**

---

## Self-Review (작성자 점검 결과)

- **Spec coverage:** §4 API(3함수)=Task1, §5 파서 알고리즘=Task1 Step2, §6 통합(EffekseerPlayable)=Task2·(main)=Task3, §7 CMake=Task1 Step3, §9 검증=Task4. 누락 없음.
- **Placeholder scan:** 없음 — 모든 코드 블록 완전.
- **Type consistency:** `CheckEffectTextures(string_view,string_view)→int`, `CheckPlayHandle(int32_t,string_view)→bool`, `CheckHandleAlive(int32_t,bool,string_view)→bool` — 선언(Task1)·호출(Task2/3) 시그니처 일치. `mHandle`(int32_t), `mManager->Exists()`(bool) 타입 정합.
- **비고:** muzzle(`distortion.efk`, main.cpp:227)은 의도된 부재라 검증 배선 제외(범위 외).
