# src/ — CLAUDE.md
엔진 코어 — 18개 `SJH::<module>` STATIC 라이브러리 + 이를 묶은 INTERFACE 우산 `SJH::engine`.

## Purpose (owns / configures)
- 18개 독립 STATIC 라이브러리 모듈 (버퍼/셰이더/씬그래프/렌더/자원캐시 등) — 각 자체 `CMakeLists.txt`
- `SJH::engine` INTERFACE 우산 타겟 — 18개 모두를 한 줄 link 로 노출 (Cocos `libcocos2d.a` 정통)

## Quick commands
```bash
# 코어 전체는 데모 타겟이 끌어감 (직접 빌드 불필요)
cmake --preset ninja
cmake --build --preset ninja --target _MyApp_

# 특정 모듈만 컴파일 검증하고 싶을 때 (타겟명 = sjhopengl_<module>)
cmake --build --preset ninja --target sjhopengl_render
```

## Key files
| 모듈 | 책임 (한 줄) |
|---|---|
| `common` | 공통 유틸, GL 로더 비의존 |
| `diagnostics` | GL 진단 (cycle-exempt, Gotcha 참조) |
| `buffer` | VBO/EBO RAII |
| `shader` | 셰이더 컴파일 + InfoLog |
| `program` | 프로그램 링킹 + uniform 캐시 |
| `layout` | Vertex 레이아웃/VAO |
| `material` | Phong/PBR Material 값 클래스 |
| `object` | Mesh/Geometry 생성기 + Light/Transform POD |
| `texture` | Texture RAII + Image 픽셀로드 (STB_IMAGE 단일 owner) |
| `scene` | Actor + Component 씬 그래프 |
| `sprite` | 2D atlas + SpriteSequencePlayable |
| `fsm` | StateMachine\<TState,TOwner\> |
| `playable` | IPlayable + Composite (Sequence/Parallel) |
| `render` | DeviceContext + SceneRenderer |
| `input` | KeyboardInput/MouseInput 디스패치 |
| `resource_registry` | 자원 캐시 9종 (Texture/Material/Sound/Effect 등) |
| `timer` | 게임플레이 타이머 |
| `text` | 월드 공간 텍스트 렌더링 |

## Gotchas
- 주의: 새 모듈 추가 시 의존을 PUBLIC(헤더 노출)/PRIVATE(.cpp 전용) 명시 구분할 것 — Why: transitive leakage 는 self-contained 모듈 규율 위반 (`modular-build-discipline`).
- 주의: `diagnostics` 는 무순환 규율의 예외(cycle-exempt) — Why: 진단은 본질상 어느 계층이든 관측해야 해서 `render→diagnostics`↔`diagnostics→render` 순환이 허용됨. 다른 모듈에는 이 예외 적용 금지.
- 주의: 모듈 간 include 는 `"<모듈명>/<헤더>.h"` 형식(= `src/` 기준 상대경로) — Why: 벤더 헤더(`<vendor/...>`)와 구분해 모듈 이식성 유지.
- 주의: 이 README 는 18개 모듈로 갱신됨 — `.claude/CLAUDE.md` 등 일부 문서의 "17개" 서술은 stale(2026-06-11 `texture` 하위추출로 18번째 모듈 신설).

## Cross-module deps
- 의존: 각 모듈은 `project_deps`(GL/glfw/sb7) + 필요한 `SJH::<module>` 만 자기 `CMakeLists.txt` 에 명시 link.
- 피의존: 이 코어를 바꾸면 `apps/_MyApp_` 등 모든 데모가 흔들림 — API 변경 전 사용처 grep 필수.

## Common modification patterns
- 새 모듈 추가: `src/<module>/CMakeLists.txt` 작성 → `src/CMakeLists.txt` 의 `add_subdirectory()` + `sjhopengl_engine` 우산의 `SJH::<module>` 링크에 등록.
- 기존 모듈에 헤더/함수 추가: `"<모듈>/<헤더>.h"` include 컨벤션 유지 + 의존 PUBLIC/PRIVATE 재검토.
- 공개 API(헤더) 시그니처 변경: `apps/_MyApp_`, `test/` 등 소비 측 grep 으로 파급 범위 확인 후 진행.

## See also
- [ARCHITECTURE.md](../ARCHITECTURE.md) (HO-3 생성 중) · [결정 스토어](../doc/adr/README.md)
- [.claude/architecture.md](../.claude/architecture.md) · [doc/EngineAPI.md](../doc/EngineAPI.md)
