# apps/_MyApp_ — CLAUDE.md
탑다운 슈터 게임 데모 — `SJH::engine` 코어의 정통 소비자 실행 파일.

## Purpose (owns / configures)
- 게임 씬 조립(Actor/Component) + 진입점(`sb7::application` 상속)
- 게임 도메인 로직 — Stage FSM, Physics(Box2D 클라), Playable 연출, VFX/Audio leaf

## Quick commands
```bash
cmake --preset ninja
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_   # 리소스 상대경로 때문에 cd 필수

# 또는 개발 CLI 한 방 (구 shell helper 통합)
python3 scripts/dev.py all debug _MyApp_
```

## Key files
- `apps/_MyApp_/main.cpp` — 진입점(`sb7::application`+`DECLARE_MAIN`), 씬 조립 정통 참조
- `apps/_MyApp_/CMakeLists.txt` — 하위 STATIC 링크 + ImGui client-side 컴파일
- `apps/_MyApp_/src/GameSystems.h` — 게임 시스템 집약
- `apps/_MyApp_/src/Stage/` — Stage FSM (Title/CombatPlay/Pause/GameOver + WaveController)
- `apps/_MyApp_/src/Physics/` — Box2D 클라 (PhysicsComponent/ContactListener/PhysicsSystem)

## Gotchas
- 주의: ESC 키를 인게임 액션에 쓰지 말 것 — Why: `sb7.h` run 루프가 ESC 를 직접 폴링해 앱 종료로 하드와이어드(sb7 수정 불가). Pause 등은 다른 키/클릭 UI 사용.
- 주의: 실행은 반드시 `build_ninja/apps/_MyApp_` 로 `cd` 후 실행 — Why: 리소스를 상대경로로 로드하기 때문.
- 주의: `apps/CMakeLists.txt` 에서 데모 타겟은 한 번에 소수만 주석 해제 — Why: 컨벤션(현재 `_MyApp_` 단독 활성). 빌드 실패 시 가장 먼저 확인할 곳.
- 주의: `shaders_slang/`·메시·패스·GL상태를 건드렸으면 빌드 GREEN 으로 끝내지 말고 골든 게이트(`cmake --preset ninja-golden && cmake --build --preset ninja-golden --target tests && ctest --test-dir build_ninja-golden -R "골든"`)까지 돌릴 것 — Why: Slang varying 이름 불일치처럼 **빌드는 통과하고 런타임에만 터지는** 계열이 있고, 렌더 회귀는 컴파일러가 안 잡는다. 상세·함정은 [`test/CLAUDE.md`](../../test/CLAUDE.md).
- 주의: 게임 빌드(`build_ninja`)의 `_MyApp_` 는 **골든을 캡처하지 않는다** — Why: 캡처 진입점(`capture_application`)은 컴파일 정의 `SJH_GOLDEN_CAPTURE` 로만 선택되고, 그 정의는 프리셋 `ninja-golden` 전유다. `SJH_GOLDEN_CAPTURE=1 ./_MyApp_` 같은 **환경 변수 방식은 2026-07-26 폐기** — 실행해도 창만 뜨고 아무 PNG 도 안 생긴다(조용한 실패).

## Cross-module deps
- 의존: `project_deps` + `game_deps`(box2d/Effekseer/FMOD/assimp/spdlog) + `SJH::engine` 우산(PRIVATE).
- 피의존: 최종 실행 파일 — 다른 코드가 이 디렉토리에 의존하지 않음(leaf).

## Common modification patterns
- 새 데모 활성화/전환: `apps/CMakeLists.txt` 의 해당 `add_subdirectory` 주석 해제(동시 다중 활성 금지).
- 신규 게임 시스템 추가: `apps/_MyApp_/src/GameSystems.h` 집약 지점 등록 + Stage FSM/Physics 등 해당 하위 폴더에 배치.
- 신규 게임 전용 의존 필요 시: `game_deps` 구성 확인, 없으면 `cmake/Dependency.cmake` 에 추가.

## See also
- [ARCHITECTURE.md](../../ARCHITECTURE.md) (HO-3 생성 중) · [결정 스토어](../../doc/adr/README.md)
- [doc/topdown-shooter-progress.md](../../doc/topdown-shooter-progress.md)
