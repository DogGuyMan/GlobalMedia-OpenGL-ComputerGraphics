# 프로젝트 인수인계: UTM Windows 11 ARM64 + MSVC OpenGL 빌드 환경

> 이 문서는 다른 Claude Code Agent가 사용자의 현재 작업 맥락을 이어받아 작업할 수 있도록 정리한 것이다. 마지막 업데이트: 2026-05-10.

---

## 사용자 프로필

- **이름**: 상준 (한국어 사용자)
- **학교/전공**: 숭실대학교 (Soongsil University), 글로벌미디어학부
- **배경**: 게임 서버 개발, 시스템 프로그래밍, Unity, Blender, OpenGL, C/C++
- **선호 스타일**: 표면적 해결보다 근본 원리·메커니즘 이해를 선호. 한국어로 소통.
- **개발 환경**: macOS (Apple Silicon), iTerm2, VS Code + clangd

---

## 현재 프로젝트 맥락

### 프로젝트 정체

학교 컴퓨터 그래픽스 강의용 **OpenGL SuperBible 7th Edition 학습 프로젝트**. Mac 한 대에서 macOS 네이티브 빌드와 Windows 크로스 빌드(또는 Windows VM 내 MSVC 빌드) 모두 수행하여 양 플랫폼의 컴파일·런타임 동작을 비교 학습하는 것이 목적.

### 프로젝트 위치

- **Mac 원본**: `~/WindowShare/Midterm-GlobalMedia-ComputerGraphics/` (SMB 공유 폴더 내)
- **Windows 접근**: `Z:\Midterm-GlobalMedia-ComputerGraphics\` (SMB 네트워크 드라이브)
- **프로젝트 제출 대상**: 교수 (이식성 중요. vcpkg 미사용, CMake만으로 완결되는 구조)

### CMakePresets.json 구조 (확정됨)

```json
{
    "version": 6,
    "configurePresets": [
        { "name": "ninja",      "generator": "Ninja", "Linux/macOS 전용" },
        { "name": "msvc",       "generator": "Visual Studio 16 2019", "toolset": "v142" },
        { "name": "msvc-2022",  "generator": "Visual Studio 17 2022", "toolset": "v143" }
    ]
}
```

프로젝트 원본 CLAUDE.md 참고 (별도 첨부):
- 챕터별 `apps/chapterN/` 구조
- sb7 프레임워크 + GLFW3 + OpenGL
- `lib/{macos,windows}/` 에 사전 빌드된 정적 라이브러리 체크인됨
- `extern/sb7code/` 는 Git 서브모듈

---

## 지금까지 해결된 문제 (시간순)

### 1. UTM Windows 11 ARM64 VM 구축

- Apple Silicon Mac에 UTM으로 Windows 11 ARM64 설치 완료
- 설치 과정에서 겪은 이슈 및 해결:
  - **EFI Shell 떨어짐**: `fs0:` → `bootaa64.efi` 수동 실행
  - **TPM 2.0 / Secure Boot 요구 에러**: `Shift+F10` → regedit → `HKLM\SYSTEM\Setup\LabConfig` DWORD 3개 추가 (BypassTPMCheck, BypassSecureBootCheck, BypassRAMCheck = 1)
  - **설치 중 ISO 재부팅 루프**: VM 강제 종료 → UTM에서 Windows ISO 분리 → NVMe에서 부팅

### 2. UTM QEMU ROM 파일 접근 차단 문제

- **증상**: `failed to find romfile "efi-virtio.rom"` 에러로 VM 실행 실패
- **원인**: macOS Gatekeeper의 `com.apple.quarantine` 플래그가 UTM 앱 번들 내부 ROM 파일에 남아있어 QEMU Helper가 접근 차단됨
- **해결**: `sudo xattr -cr /Applications/UTM.app` (모든 확장 속성 재귀 제거)
- **재발 방지**: Homebrew로 UTM 관리하면 cask 설치 시 자동으로 quarantine 제거됨

### 3. Mac ↔ Windows 파일 공유

- **시도 1 (실패)**: UTM SPICE WebDAV
  - `http://127.0.0.1:9843/` 접근 에러 0x80070043
  - **원인 판명**: UTM WebDAV는 virtio-serial 채널 기반. ARM Windows에는 SPICE Guest Tools WebDAV 클라이언트가 제공되지 않아 구조적으로 작동 불가
  - 진단 근거:
    - `netstat -ano | findstr "9843"` 결과 없음
    - Mac에서 `sudo lsof -iTCP -sTCP:LISTEN | grep -i qemu` 결과 없음
    - `ps` 명령에서 QEMU 명령줄에 `webdav|spice|chardev` 인자 없음

- **시도 2 (성공)**: Mac SMB 파일 공유
  - **설정**:
    - Mac: 시스템 설정 → 일반 → 공유 → 파일 공유 ON → `~/WindowShare` 추가 → 옵션에서 SMB 활성화 + 본인 계정 체크 + 비밀번호
    - UTM 네트워크: `공유 네트워크` 모드, `virtio-net-pci` 어댑터
    - Windows IP: `192.168.64.14`, Mac IP: `192.168.64.1`
  - **Windows 접근**: `\\192.168.64.1\WindowShare` (Z: 드라이브로 마운트 완료)
  - **WebClient 서비스**: `Set-Service -Name WebClient -StartupType Automatic` + `Start-Service WebClient` (WebDAV 시도 당시 켰음, 현재 SMB라 불필요하지만 그대로 둠)

### 4. 네트워크 어댑터 ROM 에러 연쇄

- **첫 시도 (e1000e)**: `failed to find romfile "efi-e1000e.rom"` — 같은 quarantine 문제
- **두 번째 시도 (e1000)**: `Intel Gigabit Ethernet (e1000)` 선택 후 VM 부팅 크래시 발생
- **최종**: `virtio-net-pci` 유지. `sudo xattr -cr /Applications/UTM.app` 적용 후 재시도 시 정상 동작
- **UTM 네트워크 설정 세부값 (현재 구성)**:
  ```
  네트워크 모드:              공유 네트워크
  네트워크 카드 에뮬레이션:   virtio-net-pci
  MAC 주소:                  5E:DC:C7:1C:A4:BB
  게스트 네트워크 (IPv4):    10.0.2.0/24 (표시값, 실제는 vmnet이 192.168.64.0/24 사용)
  게스트 네트워크 (IPv6):    fec0::/64
  DHCP 할당 시작:            10.0.2.15
  DHCP 할당 종료:            10.0.2.254
  호스트로부터 게스트 격리:   체크 해제 (확인됨)
  ```

### 5. DHCP 초기 실패 → 정상화

- **초기 증상**: Windows의 Default Gateway가 IPv6 링크로컬만 잡히고 IPv4 없음 (`fe80::c889:f3ff:fe1d:5364%11`)
- **최종 상태** (현재):
  ```
  IPv4 Address:      192.168.64.14
  Subnet Mask:       255.255.255.0
  Default Gateway:   192.168.64.1
  DHCP Server:       192.168.64.1
  Description:       Red Hat VirtIO Ethernet Adapter
  ```
- 과정 중 시도됐으나 필수는 아니었던 명령들:
  ```powershell
  netsh winsock reset
  netsh int ip reset
  ipconfig /flushdns
  ipconfig /release
  ipconfig /renew
  ```

### 6. Visual Studio Build Tools 2022 ARM64 설치

- **설치 완료 상태**:
  - Visual Studio **Build Tools 2022** (IDE 아님)
  - `cl.exe` 버전: `14.44.3...` (최신)
  - 위치: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\`
  - Host: ARM64 네이티브 (`$env:PROCESSOR_ARCHITECTURE = ARM64`)

- **설치 과정의 주요 이슈**:
  - 초기에 VS 2026 Build Tools 시도 시 "ARM-powered device is not supported" 경고
  - 부트스트래퍼(`vs_BuildTools.exe`)는 x86 32비트가 정상 (이건 문제 아님)
  - 해결: VS 2022 Build Tools 다운로드 (`https://aka.ms/vs/17/release/vs_BuildTools.exe`)
  - VS 2022 17.4+부터 ARM64 네이티브 공식 지원, 프로젝트의 `msvc-2022` 프리셋과 호환
  - 경고가 떠도 진행했고 정상 설치됨

- **Developer PowerShell 사용 중**: 시작 메뉴 → "Developer PowerShell for VS 2022"

---

## 현재 막혀있는 문제 (Active Issue)

### 증상

Developer PowerShell에서 `cmake --preset msvc-2022` 실행 시 에러:

```
CMake Error at CMakeLists.txt:5 (project):
  Failed to run MSBuild command:
    C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/arm64/MSBuild.exe
  to get the value of VCTargetsPath:
    ...
    error : The BaseOutputPath/OutputPath property is not set for project 'VCTargetsPath.vcxproj'.
    Please check to make sure that you have specified a valid combination of Configuration and Platform.
    Configuration='Debug'  Platform='ARM64'.
    ...
```

### 원인 분석

- 호스트가 ARM64 Windows이므로 CMake가 **기본 타겟 플랫폼을 ARM64로 선택**
- VS 2022 Build Tools의 MSBuild가 ARM64 플랫폼에 대한 C++ 프로젝트 구성을 찾지 못함
- 가능성:
  1. **"MSVC v143 - VS 2022 C++ ARM64/ARM64EC build tools" 구성 요소가 설치되지 않았거나**
  2. **x64 타겟으로 빌드하면 해결됨** (lib/windows/*.lib가 x64라면 오히려 이게 맞음)

### 해결 방향 (미확인)

**방향 A (빠름, 추천): x64 타겟으로 Configure**
```powershell
Remove-Item -Recurse -Force build_msvc -ErrorAction SilentlyContinue
cmake --preset msvc-2022 -A x64
```

**방향 B: ARM64 빌드 도구 구성 요소 설치**
- VS Installer → Build Tools 2022 → Modify → Individual components
- 검색 "ARM64" → `MSVC v143 - VS 2022 C++ ARM64/ARM64EC build tools` 체크
- 설치 후 Developer PowerShell 재시작

**결정 근거**: `lib/windows/glfw3.lib`, `lib/windows/sb7.lib`의 실제 아키텍처를 확인해야 함:
```powershell
dumpbin /headers lib\windows\glfw3.lib | Select-String "machine" | Select-Object -First 3
```
- `(x64)` → 방향 A
- `(ARM64)` → 방향 B
- 에러 / 인식 불가 → mingw 빌드 가능성 → `python scripts\dev.py extern` 재빌드 필요

---

## 다음 작업 (To-Do, 우선순위 순)

### 1. 라이브러리 아키텍처 확인

```powershell
cd Z:\Midterm-GlobalMedia-ComputerGraphics
dumpbin /headers lib\windows\glfw3.lib | Select-String "machine" | Select-Object -First 3
dumpbin /headers lib\windows\sb7.lib | Select-String "machine" | Select-Object -First 3
```

### 2. 결과에 따라 Configure 재시도

x64인 경우:
```powershell
Remove-Item -Recurse -Force build_msvc -ErrorAction SilentlyContinue
cmake --preset msvc-2022 -A x64
```

ARM64인 경우: VS Installer에서 ARM64 빌드 도구 추가 설치 후
```powershell
Remove-Item -Recurse -Force build_msvc -ErrorAction SilentlyContinue
cmake --preset msvc-2022
```

MSVC 호환 안 될 때 (mingw 빌드):
```powershell
python scripts\dev.py extern
# Windows 는 build_extern/output/ 를 lib\windows\ + include\ 로 자동 복사
```

### 3. 빌드 테스트

```powershell
cmake --build --preset msvc-2022 --target chapter1
```

- 활성 타겟 확인: `apps/CMakeLists.txt`에서 `add_subdirectory()` 주석 해제 상태 확인
- 빌드 성공 시 실행:
  ```powershell
  cd build_msvc\apps\chapter1\Debug
  .\chapter1.exe
  ```

### 4. (선택) 로컬 디스크로 이동 권장

Z:\ (SMB 네트워크 드라이브)에서 직접 빌드하면:
- I/O 느림
- MSBuild가 일부 UNC 경로 문제 일으킬 수 있음
- 파일 잠금 문제 (Windows와 Mac 동시 접근)

로컬 복사 후 빌드 권장:
```powershell
robocopy Z:\Midterm-GlobalMedia-ComputerGraphics C:\Dev\Midterm-GlobalMedia-ComputerGraphics /E /XD build_ninja build_msvc build_extern
cd C:\Dev\Midterm-GlobalMedia-ComputerGraphics
cmake --preset msvc-2022 -A x64
```

### 5. 장기 작업

- **Git 기반 동기화 고려**: GitHub private repo 사용해서 Mac ↔ Windows 동기화 (학교 제출 시 커밋 히스토리도 자연스럽게 생성됨)
- **네트워크 어댑터 등 안정적인 현재 상태 스냅샷 백업**:
  ```bash
  # Mac에서 VM 종료 후
  cp -R ~/Library/Containers/com.utmapp.UTM/Data/Documents ~/Desktop/UTM-working-backup/
  ```
- **프로젝트 확장**: ninja (macOS) ↔ msvc-2022 (Windows) 양쪽 빌드 결과 비교하여 크로스플랫폼 OpenGL 학습 진행

---

## 환경 확정 요약

### Mac 호스트

| 항목 | 값 |
|------|----|
| OS | macOS (Apple Silicon) |
| UTM | 설치 완료, quarantine 플래그 제거됨 (`sudo xattr -cr`) |
| 공유 폴더 | `~/WindowShare/` |
| SMB 공유 | 활성화, 사용자 `escatrgot` SMB 비밀번호 설정됨 |
| Mac IP (vmnet) | 192.168.64.1 |

### Windows VM

| 항목 | 값 |
|------|----|
| OS | Windows 11 ARM64 |
| Host Name | WIN-FENLMUEIQEC |
| 사용자명 | escatrgot |
| IPv4 | 192.168.64.14 |
| MAC | 5E:DC:C7:1C:A4:BB |
| 네트워크 어댑터 | Red Hat VirtIO Ethernet Adapter (virtio-net-pci) |
| Z: 드라이브 | `\\192.168.64.1\WindowShare` (영구 마운트) |
| VS Build Tools | 2022, `cl.exe` 14.44.3... (ARM64 호스트) |
| 프로젝트 위치 | `Z:\Midterm-GlobalMedia-ComputerGraphics\` |

### UTM 네트워크 설정 (재확인용)

```
네트워크 모드:              공유 네트워크
네트워크 카드 에뮬레이션:   virtio-net-pci
호스트로부터 게스트 격리:   해제
게스트 네트워크 (IPv4):    10.0.2.0/24
DHCP 시작/끝:              10.0.2.15 ~ 10.0.2.254
```

---

## 중요 관습 및 규칙

### 사용자 확정 선호 사항

- **vcpkg 미사용**: 교수 환경 호환성 위해 CMake만으로 완결되는 구조
- **VS 2022 고정**: 프로젝트 `CMakePresets.json`에 `msvc-2022` 프리셋 명시됨
- **x64 타겟 빌드 권장**: 다른 PC에서도 실행 가능해야 함 (교수 제출 대상)
- **ARM64 Windows에서 x64 크로스 컴파일**: VS 2022 Build Tools가 자동 지원

### 코드 규칙 (프로젝트 CLAUDE.md 참고)

- 주석: 한국어
- 헤더 가드: `__CHAPTER_N_ENTRY_H__` 형식
- 빌드 디렉토리: `build_ninja`, `build_ninja-release`, `build_msvc`
- VSCode: `build_ninja/compile_commands.json` 기준 clangd 동작
- 패턴 A (entry 분리) vs 패턴 B (main.cpp 단일) — 최근 챕터는 패턴 B 기본

### 크로스 플랫폼 코딩 규칙

- `windows.h`는 `#ifdef _WIN32` 안에서만 include
- `WIN32_LEAN_AND_MEAN` + `NOMINMAX` 필수
- `long` 금지 → `int32_t`, `uint64_t` 등 고정 크기 타입
- 파일 경로는 슬래시(`/`) 통일
- 파일 I/O는 바이너리 모드
- `_WIN32` 매크로: mingw에서 자동 정의, clang++에서는 정의 안 됨

---

## Wine 검증 실행 양식 (Reference Pattern)

GitHub Actions (`build-msvc.yml`) 로 빌드한 `.exe` 산출물을 macOS 에서 `wine` 으로 실행 검증하기 위한 표준 양식. **레퍼런스 검증 통과**: `<apps>/exercise7/` (검증일 2026-05-09, 당시 실존하던 챕터 디렉토리 — 이후 데모 정리로 현재 저장소엔 부재). 새 챕터/연습을 wine 으로 돌리고 싶으면 아래 4가지를 모두 따른다.

### 왜 필요한가 (원리)

wine 의 OpenGL 백엔드는 보통 **GL 4.1까지만 안정적**이다. sb7 의 기본 컨텍스트 요청은 환경에 따라 4.5+ 가 잡혀서, 그대로 두면 wine 에서 컨텍스트 생성 실패로 즉시 죽는다. 또 MSVC 산출물을 GUI 앱(`WIN32` subsystem) 으로 빌드해야 wine 에서 진입점이 정상 매칭된다 (콘솔 subsystem 으로 빌드되면 wine 윈도우가 안 뜸). 리소스는 작업 디렉토리 기준 상대경로로 로드해야 wine prefix 안에서도 동일하게 풀린다.

### 1. main.cpp — wine 호환 필수 골격

```cpp
#include "GL/gl3w.h"
#include <sb7.h>
#include <shader.h>
#include <vmath.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define MAC_WINE_TEST          // wine 분기 플래그 (GL 4.1 강제용)

class my_application : public sb7::application {
  public:
    virtual void init() {
        sb7::application::init();
#ifdef MAC_WINE_TEST
        info.majorVersion = 4;  // wine 호환 마지노선
        info.minorVersion = 1;
#endif
    }
    // startup() / render() / shutdown() ...
};
DECLARE_MAIN(my_application)
```

- 패턴 B (단일 `main.cpp` + `DECLARE_MAIN`) 사용
- 셰이더/텍스처 경로는 **작업 디렉토리 기준 상대경로**: `<shaders>/xxx.glsl`, `<textures>/xxx.jpg`
- `STB_IMAGE_IMPLEMENTATION` 은 한 컴파일 단위에서만 (중복 시 링크 에러)
- **하지 말 것**: `glViewport(0, 0, info.windowWidth, info.windowHeight)` — macOS Retina 에서 `info.windowWidth/Height` 는 포인트 단위라 framebuffer 픽셀의 절반 → 좌하단 1/4 만 그려진다. sb7 가 자동으로 framebuffer 크기로 viewport 잡아주므로 호출 자체를 생략

### 2. apps/<name>/CMakeLists.txt — Windows GUI 앱 양식

```cmake
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

if(WIN32)
    add_executable(${CHAPTER_NAME} WIN32 main.cpp)   # GUI 앱 (콘솔 없음)
else()
    add_executable(${CHAPTER_NAME} main.cpp)
endif()

target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps)
target_include_directories(${CHAPTER_NAME} PRIVATE ${CMAKE_CURRENT_DIR})

if(MSVC)
    target_compile_definitions(${CHAPTER_NAME} PRIVATE WIN32 _WINDOWS)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources
        COMMENT "${CHAPTER_NAME} 빌드 후 리소스를 build 디렉터리에 복사")
endif()
```

핵심: `WIN32` 옵션이 빠지면 wine 에서 윈도우 안 뜸. MSVC 한정 `WIN32 _WINDOWS` 매크로로 sb7 의 win32 분기 활성화.

### 3. 리소스 구조

```
apps/<name>/resources/
├── shaders/
│   ├── basic_lighting_{vs,fs}.glsl
│   ├── basic_texturing_{vs,fs}.glsl
│   └── simple_color_{vs,fs}.glsl
└── textures/
    └── *.{jpg,png}
```

POST_BUILD 단계에서 `build_msvc/apps/<name>/<Config>/resources/` 로 복사. 실행 시 `cd build_msvc/apps/<name>/<Config>` 한 후 `.exe` 호출 (또는 wine 호출) — 그래야 `./shaders/...` 상대경로가 풀린다.

### 4. .github/workflows/build-msvc.yml — CI 양식 (그대로 사용)

```yaml
name: Build (MSVC)
on:
  push:         { branches: [main] }
  pull_request: { branches: [main] }
  workflow_dispatch:

jobs:
  build:
    runs-on: windows-latest
    strategy:
      fail-fast: false
      matrix:
        include:
          - { configuration: Debug,   build_preset: msvc-2022 }
          - { configuration: Release, build_preset: msvc-2022-release }
    steps:
      - uses: actions/checkout@v4
        with: { submodules: true }
      - run: cmake --preset msvc-2022
      - run: cmake --build --preset ${{ matrix.build_preset }}
      - uses: actions/upload-artifact@v4
        with:
          name: build-msvc-${{ matrix.configuration }}
          path: build_msvc/apps/**/${{ matrix.configuration }}/
```

- `submodules: true` 필수 (`extern/sb7code/` 가 서브모듈)
- 새 챕터 추가 시 — `apps/CMakeLists.txt` 의 활성 타겟 주석 해제만 하면 CI 가 자동으로 새 산출물 업로드

### macOS 에서 wine 으로 검증하는 절차

1. CI artifact 다운로드 (또는 로컬 mingw-w64 빌드)
2. `unzip build-msvc-Debug.zip -d /tmp/wine-test/`
3. `cd /tmp/wine-test/apps/<name>/Debug`
4. `wine ./<name>.exe`
   - GL 컨텍스트 생성 로그 확인 (`majorVersion=4, minorVersion=1`)
   - 윈도우가 뜨고 정상 렌더링되면 OK

### 함정 체크리스트 (디버깅 용)

| 증상 | 원인 | 해결 |
|------|------|------|
| wine 에서 즉시 죽음 / 컨텍스트 생성 실패 | `MAC_WINE_TEST` + `init()` GL 4.1 강제 누락 | 1번 골격 적용 |
| 윈도우 안 뜨고 콘솔만 깜빡 | `add_executable` 에 `WIN32` 옵션 빠짐 | 2번 양식 적용 |
| 좌하단 1/4 만 렌더링 (macOS native 도) | `glViewport(..., info.windowWidth, info.windowHeight)` 호출 | 그 라인 제거 |
| 셰이더/텍스처 로드 실패 | 절대경로 사용 또는 cd 안 하고 실행 | 상대경로 (`./shaders/...`) + 실행 디렉토리에서 호출 |
| 링크 단계에서 `stbi_*` 중복 정의 | `STB_IMAGE_IMPLEMENTATION` 을 여러 컴파일 단위에서 정의 | 단일 `.cpp` 에서만 정의 |

### 검증된 레퍼런스 위치

- 코드: `<apps>/exercise7/main.cpp`, `<apps>/exercise7/CMakeLists.txt`, `<apps>/exercise7/resources/` (당시 실존, 이후 데모 정리로 현재 저장소엔 부재)
- CI: `.github/workflows/build-msvc.yml`
- 메모리: `<memory>/wine_verified_pattern_exercise7.md` (요약 버전 — Claude 전역 auto-memory 저장소 경로, 이 레포 파일 아님)

---

## 다른 Agent에게 전달 사항

### 우선순위

1. **현재 막혀있는 MSBuild ARM64 에러 해결**이 최우선 (위 "To-Do 1~3" 참고)
2. 해결 후 `chapter1` 빌드 성공 확인 → 다른 챕터로 확장
3. 필요 시 Git repo로 전환해서 장기 유지보수 용이하게

### 주의 사항

- **Z:\ 에서 빌드 시도 중**. 문제 발생 시 로컬 C:\Dev\ 로 이동할 준비 되어있음
- **사용자는 한국어로 소통**. 답변도 한국어로
- **원리 설명 선호** — 에러가 왜 났는지 알려주고 나서 해결책 제시
- **Developer PowerShell** 사용 중 (일반 PowerShell 아님 주의)
- **`cl` vs `c1` 혼동** 주의 — 소문자 L 사용해야 함

### 피해야 할 함정

- UTM WebDAV 재시도: 구조적으로 ARM Windows에서 불가능. SMB로 확정됨.
- 네트워크 카드를 e1000/e1000e로 변경: virtio-net-pci에서 정상 동작 중, 건드리지 말 것
- VS 2026 설치 시도: VS 2022로 확정됨 (프리셋 호환성)
- 부트스트래퍼 아키텍처로 판단: `vs_BuildTools.exe`는 x86 32비트가 정상

### 도움이 되는 추가 컨텍스트

사용자는 이전 대화에서 다음 관련 프로젝트/학습 경험을 보유:
- OpenGL (legacy + modern pipeline) 실무 경험
- CMake Superbuild 패턴 이해
- macOS + mingw-w64 + wine-stable 크로스 컴파일 환경 구축 경험 있음 (별도 프로젝트)
- Unity, C++, C#, Rust+wgpu, Slang 등 GPU 프로그래밍 전반

즉, CMake와 C++ 빌드 시스템 관련 대화는 중급~고급 수준으로 진행 가능.

---

## 이 문서 사용 방법

다른 Claude Code Agent가 이 프로젝트를 이어받을 때:

1. 이 문서를 먼저 읽어 전체 맥락 파악
2. 프로젝트 루트의 `CLAUDE.md` (프로젝트별 규칙)도 함께 참고
3. **현재 막혀있는 문제부터 해결**: `dumpbin /headers lib\windows\glfw3.lib` 부터 실행
4. 새로운 문제 해결 시 이 문서의 "지금까지 해결된 문제" 섹션에 업데이트하여 전달

---

**마지막 성공 단계**: Developer PowerShell에서 `cl` 정상 인식, `cmake --preset msvc-2022` 시도 중
**다음 단계**: 라이브러리 아키텍처 확인 → `-A x64` 플래그 추가 Configure 재시도
