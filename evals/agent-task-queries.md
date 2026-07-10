# Agent Task Queries — 에이전트 readiness eval

이 문서의 목적: 새 에이전트/세션이 이 저장소에서 작업할 때 **올바른 문서·컨벤션을 참조하는지** 점검하기 위한 대표 task 5종 모음이다. 각 항목은 실제로 이 저장소에서 자주 발생하는 요청을 흉내 낸 쿼리이며, 기대 결과(참조해야 할 문서·컨벤션)와 흔한 오판 사례를 함께 기록해 채점 기준으로 쓸 수 있게 했다.

---

## 1. "SJH::render 에 새 Pass 추가"

- **쿼리**: SJH::render 모듈에 새로운 렌더 Pass 를 추가하고 싶다.
- **기대 결과 (참조해야 할 문서·컨벤션)**: `src/render/` 의 `CMakeLists.txt` 로 기존 의존성 선언 방식 확인 + Pass 컨벤션은 `doc/EngineAPI.md` 참조.
- **흔한 오판**: Pass 를 카메라에 종속시킴 — `Camera.Depth` 오용 금지(폐기된 패턴, MEMORY `camera_depth_postfx_misuse` 참조). PostFX 는 비-카메라 ordered pass list 이며 `addChild` 순서가 자동 렌더 순서를 결정한다(4대 엔진 정통 패턴, `PassComponent` + Screen Ortho Camera 2-Camera 구조).

## 2. "새 데모 타겟 활성화 (예: migrate_demo)"

- **쿼리**: `migrate_demo` 를 빌드하고 싶다. 어떻게 활성화하나?
- **기대 결과 (참조해야 할 문서·컨벤션)**: `apps/CMakeLists.txt` 에서 해당 `add_subdirectory(migrate_demo)` 줄의 주석을 해제한다 (CLAUDE.md "Active Target Management — CRITICAL" 섹션).
- **흔한 오판**: 여러 데모 타겟을 동시에 활성화함. 컨벤션은 "한 번에 하나/소수만 활성화" — 다른 데모(`box2d_demo`/`effekseer_demo`/`audio_demo`/`tweeny_demo`)까지 같이 주석 해제하면 빌드 충돌·의도치 않은 링크 확장을 유발할 수 있다.

## 3. "신규 모듈에 game_deps 의존 추가"

- **쿼리**: 새로 만드는 `src/<module>/` 이 box2d 또는 Effekseer/FMOD 를 써야 한다. 어떻게 의존을 추가하나?
- **기대 결과 (참조해야 할 문서·컨벤션)**: `cmake/Dependency.cmake` 에서 `game_deps` INTERFACE 타겟 구성 확인 + 전역 Skill `modular-build-discipline` 의 PUBLIC/PRIVATE 명시 구분 원칙(헤더 노출 의존=PUBLIC, .cpp 내부 전용=PRIVATE) 적용.
- **흔한 오판**: 다른 모듈이 이미 `game_deps` 를 링크하고 있다는 이유로 전이 의존(transitive dependency)에 기대어 자기 모듈 `CMakeLists.txt` 에 명시적 `target_link_libraries` 선언을 생략함 — self-contained 모듈 원칙 위반.

## 4. "Actor 에 새 속성 추가"

- **쿼리**: Actor 에 새로운 속성(예: 체력, 태그 등)을 추가하고 싶다.
- **기대 결과 (참조해야 할 문서·컨벤션)**: `src/scene/actor.h` / `src/scene/components.h` 확인. Actor 는 비상속 — 특수 속성은 반드시 Component 로만 추가한다(MEMORY `compound_actor_pattern`, CLAUDE.md `SJH::scene` 모듈 설명).
- **흔한 오판**: Actor 를 상속해 서브클래스(예: `PlayerActor : public Actor`)를 신설해 속성을 얹으려 함 — 이 저장소의 Actor/Component 씬 그래프 설계 원칙과 정면으로 어긋난다.

## 5. "텍스처/머티리얼 로딩 코드 추가"

- **쿼리**: 새 데모/모듈에서 텍스처나 머티리얼을 로딩하는 코드를 추가하고 싶다.
- **기대 결과 (참조해야 할 문서·컨벤션)**: `SJH::ResourceRegistry` 에 위탁(`.claude/architecture.md §11.3` 자원 보유 컨벤션 필독). `stb_image` 직접 호출·`glGenTextures` 직접 호출 금지 — `SJH::Image::Load` + `SJH::Texture::CreateTexture` 위임(MEMORY `stb_image_owner_resource_registry`, `uniform_atlas_delegates_image_texture`).
- **흔한 오판**: 데모 `main.cpp` 안에 `Texture*`/`Material` 등 자원 객체를 직접 멤버로 보유함 — CLAUDE.md 는 데모 멤버를 "씬과 시스템만"으로 제한하고 자원 객체는 ResourceRegistry 로 위탁하는 것을 Cocos `cc::Director` / Unity `Resources.Load` 정통 패턴으로 명시한다.
