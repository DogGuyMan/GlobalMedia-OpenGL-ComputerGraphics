# 다른 Claude Code Agent 용 프롬프트 — WaveController 라이브 씬 배선 (적 스폰 트리거)

> 아래 코드블록 전체를 새 Claude Code Agent 세션에 붙여 사용. 자기완결(별도 문서 안 읽어도 됨).
> 선행 작업: EnemyBuilder(적 스프라이트 + 2프레임 애니)는 **이미 구현·빌드 통과**. 정본 spec(참고): `doc/superpowers/specs/2026-06-01-enemy-builder-sprite-design.md`.
> 이 프롬프트가 다루는 건 **"적이 화면에 안 나오는" 잔여 갭 = WaveController 가 라이브 씬에 부착되지 않아 tick 이 안 되는 문제** 하나뿐.

---

```
[ROLE]
너는 C++17/CMake OpenGL 탑다운 슈터 프로젝트(/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics, 브랜치 game/module/ingame/temp)의 구현 에이전트다.

[현재 상태 — 검증됨]
- 적 조립 파이프라인(EnemyBuilder + WaveController 라우팅)은 이미 완성·빌드 통과(exit 0):
  - apps/_MyApp_/src/Bootstrap/EnemyBuilder.{h,cpp} (신규) — CreateEnemyActor + ENEMY_FRONT 스프라이트/2프레임 애니 조립 후 spawnParent->AddChild. Bootstrap::BuildEnemy(const EnemyDeps&) → Actor*.
  - apps/_MyApp_/src/Stage/WaveController.{h,cpp} — SpawnEnemy() 가 Bootstrap::BuildEnemy 를 호출하도록 라우팅 완료. WaveController 는 SJH::Scene::Component (Update(float dt) 보유). 생성자 = WaveController(b2World* world, SJH::Scene::Actor* spawnParent, SJH::Scene::Actor* playerActor, float arenaHalfExtent). 멤버 mSpawnCount(variant 순환) 추가됨.
  - Bootstrap/CMakeLists.txt 에 EnemyBuilder.cpp 추가, Stage/CMakeLists.txt 에 MyApp::Bootstrap link 추가 — 둘 다 반영됨.
  - 적 텍스처 resources/texture/enemy/ENEMY{1,2,3}_FRONT.png 3종 존재(빌드 디렉토리로 복사됨).

[문제 — 적이 화면에 안 나온다]
WaveController 는 STATIC 라이브러리(myapp_stage)에 컴파일만 되고, **어디에서도 인스턴스화되어 씬 트리에 Component 로 부착되지 않는다** → Update(dt) 가 호출되지 않음 → SpawnEnemy() 가 영원히 안 불림 → 적 0마리. (13초 실행 로그에 "[Wave ...]" / "Enemy spawned" 0건으로 확정.)

[핵심 사실 — main.cpp 구조 (검증됨)]
- apps/_MyApp_/main.cpp 의 game_application::startup() 안:
  - 라인 254: `dir.Root().AddChild(Stage::CreateStageActor({ &phys.World(), &reg }));` — 스테이지(벽+PCB)만 생성. StageConfig 는 {world, registry} 만 전달 → arenaHalfExtent 는 **기본값 10.0f** 사용. StageConfig 에 player 필드 없음.
  - 라인 259: `mFxRoot = dir.Root().AddChild(make_unique<Actor>("FxRoot"));`
  - 라인 261~264: `auto player = Bootstrap::BuildPlayer({...}); mSpriteActor = player.SpriteActor;` — **플레이어는 스테이지보다 뒤에 생성된다.** (그래서 StageBuilder 안에서는 player 가 아직 없어 WaveController 를 못 만든다 — playerTarget=nullptr 가 됨.)
  - 라인 289: `dir.Enter();`
- render() 라인 323: `SJH::Scene::Director::Get().Update(dt);` — dir.Root() 하위 **모든** Component 의 Update(dt) 를 매 프레임 호출. 즉 WaveController 를 root 하위 actor 에 Component 로 달기만 하면 자동 tick 된다.
- render() 라인 325: `Physics().SyncToTransform(Director::Get().Root());` — root 전체 트리 재귀. 적 actor 가 root 어딘가에 있으면 b2Body→Transform 동기 + 빌보드 추종 OK.
- WaveController 생성자 인자: world=&phys.World(), playerActor=mSpriteActor(라인 262 이후 사용 가능), arenaHalfExtent=10.0f(스테이지 기본과 일치 필수 — 안 맞으면 적이 벽 밖에서 스폰), spawnParent=아래 선택.
- main.cpp 는 이미 "Stage/StageBuilder.h" 를 include + CreateStageActor 호출 → executable 이 MyApp::Stage 를 link 함. 따라서 "Stage/WaveController.h" 추가에 **CMake 변경 불필요**.

[제약 — 매우 중요]
- **main.cpp 는 Fog/PostFX 리팩토링 에이전트의 경합 구역이다.** 이 작업도 main.cpp 를 건드려야 하므로(아래 [DECIDE] 참고), **반드시 최소 surgical** 로 하고, PostFX/Fog/stages/카메라 관련 라인은 절대 건드리지 마라. 추가는 startup() 끝(player 생성 이후 ~ dir.Enter() 이전)에 *덧붙이기*만.
- **EnemyFactory.h / EnemyBuilder.{h,cpp} / WaveController.{h,cpp} 는 수정하지 마라** — 이미 완성. (EnemyFactory.h 는 별도 "PlayerBehavior 분해 Task 5" 소유.)
- 단위 테스트/TDD 금지(no_auto_tests). 검증 = 빌드 + 육안 실행.
- 커밋 금지. 구현+빌드+보고만.
- 주석 한국어. 헤더가드 __XXX_H__ (#pragma once 미사용). long 금지, 경로 슬래시.

========================================================================
[DECIDE] 배선 방식 선택 (먼저 결정하고 보고에 근거 명시)
========================================================================
적을 라이브로 띄우는 데 현실적 경로는 main.cpp 의 player 생성(라인 264) 이후에 WaveController 보유 actor 를 root 에 추가하는 것뿐이다(player 가 거기서야 존재하므로). 권장안 = 옵션 A.

옵션 A (권장 — main.cpp surgical 6줄):
  - 상단 include 블록(다른 "Stage/..." include 옆, 예: 라인 29 근처)에 추가:
        #include "Stage/WaveController.h"
  - startup() 의 player 생성(라인 261~264) 직후, dir.Enter()(라인 289) 이전에 추가:
        // 적 웨이브 스폰 트리거 — player(mSpriteActor) 생성 이후라야 SimplePursueAI 타깃 유효.
        // root 하위 Component 라 Director::Update(dt) 가 자동 tick. arenaHalfExtent 는 StageConfig 기본(10.0f)과 일치.
        auto *waveSpawner = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("WaveSpawner"));
        waveSpawner->AddComponent<Stage::WaveController>(&phys.World(), waveSpawner, mSpriteActor, 10.0f);
    (spawnParent=waveSpawner 자기 자신 → 적이 WaveSpawner 의 child 로 붙음. dir.Enter() 가 뒤에 오므로 OnEnter 캐스케이드 정상. 런타임 스폰분은 AddChild 가 entered 부모에 캐스케이드 — 플레이어/적 동형.)
  - 네임스페이스 확인: main.cpp 는 namespace TopdownShooter → WaveController 는 Stage::WaveController. phys = Manager::Get().Physics(), dir = Director::Get() 는 startup 에서 이미 선언됨(라인 154~155). mSpriteActor 는 멤버.

옵션 B (대안 — main.cpp 무수정, 단 변경량 더 큼, 비권장):
  StageConfig 에 SJH::Scene::Actor* playerTarget 필드 추가 + StageBuilder 가 WaveController 부착 + main.cpp 에서 CreateStageActor 호출을 player 생성 *뒤로* 이동(reorder). → main.cpp reorder 가 옵션 A 의 6줄보다 Fog 경합 위험이 크다. 권장 안 함.

옵션 C (임시 시각 확인용): main.cpp 에 player 직후 1~2체 즉시 BuildEnemy 직접 호출(WaveController 없이)로 형태만 육안 확인 → 확인 후 옵션 A 로 대체. (시간 절약용, 최종 아님.)

[빌드 가능성 메모]
- Actor::AddComponent<T>(args...) 는 가변 인자 perfect-forward (EnemyFactory.h 가 AddComponent<SimplePursueAI>(target, body, speed) 로 검증됨).
- WaveController.h 는 class b2World; forward decl + "scene/actor.h" 만 의존. main.cpp 에 추가 link 불필요(MyApp::Stage 이미 link).

========================================================================
[검증]
========================================================================
1. 빌드: cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics && cmake --build --preset ninja --target _MyApp_  → exit 0.
2. 실행: cd build_ninja/apps/_MyApp_ && ./_MyApp_  (약 8~13초). 로그에 "[Wave 1] Enemy spawned at (x,y)" 가 ~3초 간격으로 찍히고, 화면에 적이 ENEMY_FRONT 스프라이트(2프레임 walk 루프)로 표시 + 플레이어 추적 이동. 3종(variant) 순환.
   - 로그 grep 예: `grep -iE "Wave|Enemy spawned|ENEMY[0-9]_FRONT" 로그`.
3. 안 보이면 점검: (a) WaveController 가 root 하위에 실제로 붙었나(Update 호출되나) (b) atlas 로드 실패 로그("[enemy] atlas load 실패") (c) arenaHalfExtent 불일치로 벽 밖 스폰 (d) QueueOffset/heightOffset 로 다른 빌보드·바닥에 가려짐.

[Self-review]
- main.cpp 수정이 player 생성 이후 ~ dir.Enter() 이전의 *덧붙이기* 6줄 + include 1줄 범위인가? PostFX/Fog/stages/카메라 라인은 안 건드렸나?
- EnemyFactory.h / EnemyBuilder / WaveController 소스는 무수정인가?
- arenaHalfExtent 가 스테이지(StageConfig 기본 10.0f)와 일치하나?
- 빌드 exit 0 + "[Wave] Enemy spawned" 로그 + 적 스프라이트 육안 확인?

[보고]
DONE / DONE_WITH_CONCERNS / BLOCKED — 택한 옵션(A/B/C)과 근거 + main.cpp 변경 diff 요약(추가 라인만) + 빌드 마지막 줄 + 실행 로그의 "[Wave ...]" 발췌 + 적 육안 확인(보임?) + git status. 커밋하지 마라. main.cpp 를 건드렸으면 Fog/PostFX 에이전트와의 잠재 충돌(추가한 라인 위치)을 보고에 명시.
```

---

## 사용 메모 (오케스트레이터/사용자용)
- **갭의 본질**: EnemyBuilder(적 스프라이트 조립)는 완성·빌드 통과지만, 그것을 호출하는 WaveController 가 *라이브 씬에 안 붙어* tick 이 안 됨 → 적 0마리. 부착 지점이 main.cpp(player 가 거기서 생성되므로)라서 Fog/PostFX 에이전트와 경합.
- **권장 = 옵션 A** (main.cpp surgical 6줄 + include 1줄). CMake 변경 불필요(MyApp::Stage 이미 link).
- **충돌 회피**: 추가는 startup() 의 player 생성(라인 264) 이후 ~ dir.Enter()(라인 289) 이전 *덧붙이기*만. PostFX 체인/stages/카메라/fog 라인 미접근.
- 커밋 메시지(권장): `[feat] : WaveController 라이브 씬 배선 — 적 웨이브 스폰 활성화` (Co-Authored-By 미사용).

## 🟢 오케스트레이터 결정 (PlayerBehavior 분해 담당, 2026-06-01)
- **승인 = 옵션 A.** main.cpp surgical append (player 생성 이후 ~ dir.Enter() 이전).
- **PlayerBehavior 분해와 충돌 0**: 분해의 잔여 Task 4~7 은 **main.cpp 도 WaveController.{h,cpp} 도 건드리지 않는다** (Task 6 도 PlayerSpriteDirector/PlayerActor/PlayerBuilder/PlayerController 로만 라우팅). main.cpp 경합은 **Fog/PostFX 에이전트와만** — PostFX/camera/fog/stages 라인을 피해 startup() 끝에 *덧붙이기*만 하면 안전. WaveController 가 쓰는 `mSpriteActor`(player) 는 분해 Task 6 이 BuildPlayer 내부를 바꿔도 반환 포인터로 유지되므로 영향 없음.
- **⚠ 검증 시 정상 동작 주의 (오해 방지)**: 현재 분해 Task 3(적 접촉→`Life::DoDamaged`)은 **이미 커밋**되어 접촉 데미지가 라이브다. 그러나 **i-frame·사망지연·디졸브 sink 는 분해 Task 6 미구현** 상태라, 스폰된 적이 플레이어에 닿으면 **즉시 연속 데미지 → 플레이어 즉사(SetActive(false)→사라짐), 디졸브 없음**. 이건 정상(pre-Task6) 상태이지 버그 아님. **검증 초점 = "적이 스폰되어 ENEMY_FRONT 스프라이트 + 2프레임 walk + 플레이어 추적"** 이고, 플레이어가 접촉 시 사라지는 현상은 무시(적이 닿기 전에 스폰/스프라이트/추적을 육안 확인).
