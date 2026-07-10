# 세션 컨텍스트 이관 — 적 스폰(EnemyBuilder + WaveController 라이브 배선)

> 다른 Claude Code 세션이 이 작업을 손실 없이 이어받기 위한 컨텍스트 메모리.
> 아래 코드블록 전체를 새 세션에 붙여 사용. 자기완결.
> 작성 시점 스냅샷: 브랜치 `game/module/ingame/temp`, HEAD `53bc25e`, 2026-06-01.
> ⚠ 이 repo 는 **여러 에이전트가 병렬로 커밋 중**(vfx/timer/Fog/PostFX) — HEAD·git status 는 네가 받을 때 이미 더 전진했을 수 있다. 항상 `git log --oneline -5` + `git status --short` 로 실측 후 시작하라.

---

```
[ROLE / 인계받는 작업]
너는 C++17/CMake OpenGL 탑다운 슈터(/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics, 브랜치 game/module/ingame/temp)의 구현 에이전트다.
직전 세션에서 "적(enemy)이 화면에 안 보이는" 문제를 해결했다(물리+Life+AI만 있고 비주얼/스폰 트리거 없음 → 이제 스프라이트 + 웨이브 스폰 동작). 너는 이 작업의 **마무리(커밋 갭 닫기)** 와, 원하면 후속(Effekseer .efk / i-frame 등)을 이어받는다.

============================================================
[1] 직전 세션이 한 일 (요약)
============================================================
A. EnemyBuilder (적 스프라이트 조립) — PlayerBuilder 미러:
   - apps/_MyApp_/src/Bootstrap/EnemyBuilder.{h,cpp} (신규). Bootstrap::BuildEnemy(const EnemyDeps&) → SJH::Scene::Actor*.
     내부: Entity::Enemy::CreateEnemyActor(cfg) (물리+Life+AI+contact, EnemyFactory.h 무변경) 위에
     ENEMY_FRONT[variant%3] 스프라이트(SpriteRenderer) + 2프레임 walk(SpriteSequencePlayable, SetIsLoop(true)+Play()) 부착 후 spawnParent->AddChild.
   - 이 소스는 커밋 d3b31a5 "[dev] : wave demo" 에 포함됨(EnemyBuilder.{h,cpp} + WaveController.{h,cpp}).
B. WaveController 라우팅:
   - apps/_MyApp_/src/Stage/WaveController.{h,cpp} — SpawnEnemy() 가 inline CreateEnemyActor 대신 Bootstrap::BuildEnemy 호출.
     멤버 int mSpawnCount(variant 순환) 추가. 이것도 d3b31a5 에 포함.
C. WaveController 라이브 씬 배선(핵심 갭 해결, 옵션 A) — main.cpp:
   - 직전 세션이 발견한 STEP0 갭: WaveController 가 STATIC lib 에 컴파일만 되고 *어디서도 인스턴스화/씬부착 안 됨* → Update(dt) 안 불림 → 적 0마리.
   - 해결: apps/_MyApp_/main.cpp 의 startup() 에서 player 생성 직후 ~ dir.Enter() 이전에 WaveController 부착.
     라인 31: #include "Stage/WaveController.h"
     라인 277~278:
        auto *waveSpawner = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("WaveSpawner"));
        waveSpawner->AddComponent<Stage::WaveController>(&phys.World(), waveSpawner, mSpriteActor, 10.0f);
   - 근거: render() 의 Director::Get().Update(dt) 가 root 하위 모든 Component tick → root 에 달면 자동 동작. arenaHalfExtent 10.0f = StageConfig 기본과 일치(벽 안쪽 스폰).

============================================================
[2] 현재 repo 상태 스냅샷 (작성 시점 — 반드시 실측 재확인)
============================================================
HEAD: 53bc25e feat(vfx): main 로드 루프에 .efk 텍스처 검증 배선
브랜치: game/module/ingame/temp
미커밋(working tree, unstaged):
   M apps/_MyApp_/main.cpp                      ← C: WaveController 라이브 배선 (옵션 A)
   M apps/_MyApp_/src/Bootstrap/CMakeLists.txt  ← EnemyBuilder.cpp 소스 등록
   M apps/_MyApp_/src/Stage/CMakeLists.txt      ← MyApp::Bootstrap link 추가
   M doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition-execution.md  ← (다른 에이전트 것, 건드리지 마라)
Untracked(핸드오프 문서, 너의 코드 작업 아님):
   ?? doc/handoffs/2026-06-01/2026-06-01-enemy-builder-agent-prompt.md
   ?? doc/handoffs/2026-06-01/2026-06-01-timer-migration-agent-prompt.md
   ?? doc/handoffs/2026-06-01/2026-06-01-wave-controller-wiring-agent-prompt.md
   ?? doc/handoffs/2026-06-01/2026-06-01-wave-enemy-session-context-handoff.md (이 문서)

⚠⚠ 커밋 갭 — 가장 중요 ⚠⚠
커밋 d3b31a5 "wave demo" 는 EnemyBuilder/WaveController *소스만* 담았고,
**main.cpp 배선 + Bootstrap/CMakeLists.txt + Stage/CMakeLists.txt 3개는 아직 커밋 밖**이다.
→ 즉 d3b31a5 단독으로는 빌드 불가(EnemyBuilder.cpp 가 빌드에 등록 안 됨) + 적 스폰도 안 켜짐(main 배선 없음).
→ 지금 빌드가 green 인 건 이 3개 미커밋 변경이 working tree 에 살아있기 때문.
→ 네 첫 임무 = 이 3개를 한 커밋으로 묶어 갭을 닫는 것(사용자 승인 후). 권장 메시지:
   `[feat] : WaveController 라이브 배선 + EnemyBuilder CMake 등록 — 적 웨이브 스폰 완성`
   (Co-Authored-By 미사용. 사용자가 커밋하라고 명시할 때만 — 무단 커밋 금지.)

============================================================
[3] 검증 증거 (직전 세션, 재현 가능)
============================================================
- 빌드: cmake --build --preset ninja --target _MyApp_  → exit 0 (현재 incremental "no work to do").
- 실행(GUI 앱, 백그라운드 13초 후 kill + 로그 grep):
    cd build_ninja/apps/_MyApp_ && ( ./_MyApp_ >/tmp/run.log 2>&1 & p=$!; sleep 13; kill $p 2>/dev/null; wait $p 2>/dev/null )
    grep -iE "Wave|Enemy spawned|ENEMY[0-9]_FRONT" /tmp/run.log
  결과(검증됨):
    [Wave 1] Enemy spawned at (-7.0, 9.5)   ← ENEMY1_FRONT
    [Wave 1] Enemy spawned at ( 8.3, 9.5)   ← ENEMY2_FRONT
    [Wave 1] Enemy spawned at (-9.5,-8.8)   ← ENEMY3_FRONT
  → ~3초 간격 스폰, atlas load 실패 0, variant 3종 순환, 좌표 arena(±10) 내. WaveController 자동 tick 확인.

============================================================
[4] 미해결 / 후속 후보
============================================================
(a) 커밋 갭 닫기 — [2] 참조. (최우선)
(b) Effekseer .efk 로드 실패(적 스폰과 무관, 별도 에이전트가 이미 착수 중):
    실행 로그에 7종 Effect::Create 실패 — muzzle/dust/hit/laser/orbital_background/slash/summon.
    "[error] [ResourceRegistry::CreateEffect] Effekseer::Effect::Create 실패 (key=...)".
    init 은 OK([VFXSystem] init OK). 실패 지점이 Effect::Create 자체 → .efk 파일 부재/경로/파싱.
    관련 최근 커밋(다른 에이전트): 53bc25e .efk 텍스처 검증 배선 / 32db244 EffekseerPlayable lifecycle / 79e8902 EffekseerDiagnostics 신설.
    참고 메모리: efk-texture-basepath-trap (CreateEffect 가 materialPath 미전달 → 내장 상대경로가 resources/vfx/ 기준 해석).
(c) 적 피격/사망 처리 미구현(pre-Task6): i-frame·사망지연·디졸브 없음 → 적 접촉 시 플레이어 즉사(SetActive(false)) 가능. 이건 정상 상태(버그 아님). PlayerBehavior 분해 Task6 영역.

============================================================
[5] 가드레일 / 제약 (절대)
============================================================
- EnemyFactory.h (apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h) 수정 금지 — PlayerBehavior 분해 Task5 소유. EnemyBuilder 는 그 위에 스프라이트만 얹는다.
- main.cpp 는 Fog/PostFX 에이전트 경합 구역 — 추가 시 surgical(PostFX/Fog/stages/카메라 라인 미접근). 현재 배선은 startup() 끝 player~Enter 사이 *덧붙이기* 2줄+include 1줄.
- 단위 테스트/TDD 금지(no_auto_tests). 검증 = 빌드 + 육안/로그.
- 무단 git commit/add 금지 — 사용자 명시 승인 시에만.
- doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition-execution.md 는 다른 에이전트의 미커밋 변경 — 건드리지 마라.
- 주석 한국어. 헤더가드 __XXX_H__ (#pragma once 미사용). long 금지(int32_t/uint64_t), 경로 슬래시.

============================================================
[6] 핵심 파일 / 라인 레퍼런스
============================================================
- apps/_MyApp_/main.cpp:31 (include), :273 (mSpriteActor=player.SpriteActor), :277~278 (WaveController 배선), :303 (dir.Enter), :323 (Director::Update tick), :325 (Physics SyncToTransform root)
- apps/_MyApp_/src/Bootstrap/EnemyBuilder.{h,cpp} — BuildEnemy / EnemyDeps
- apps/_MyApp_/src/Stage/WaveController.{h,cpp} — SpawnEnemy()→BuildEnemy, mSpawnCount, ctor(b2World*, spawnParent, playerActor, arenaHalfExtent), Component(Update)
- apps/_MyApp_/src/Stage/StageBuilder.cpp / StageConfig.h — CreateStageActor (StageConfig{world,registry,arenaHalfExtent=10}). player 필드 없음(그래서 WaveController 를 여기서 못 만듦 → main 배선이 정답).
- apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h — CreateEnemyActor/EnemyConfig (무변경 대상)
- apps/_MyApp_/src/Playable/Constants.h:78~82 — const EntityTextureConfig ENEMY_FRONT[3] (RowCount=1, ColCount=2 → 2프레임 가로 스트립). 텍스처 resources/texture/enemy/ENEMY{1,2,3}_FRONT.png 존재.
- 빌드: cmake --build --preset ninja --target _MyApp_ / 실행: cd build_ninja/apps/_MyApp_ && ./_MyApp_

============================================================
[7] 관련 핸드오프 / spec 포인터
============================================================
- doc/superpowers/specs/2026-06-01-enemy-builder-sprite-design.md — EnemyBuilder 정본 spec.
- doc/handoffs/2026-06-01/2026-06-01-enemy-builder-agent-prompt.md — EnemyBuilder 구현 프롬프트(완료).
- doc/handoffs/2026-06-01/2026-06-01-wave-controller-wiring-agent-prompt.md — 옵션 A 배선 프롬프트(완료) + 오케스트레이터 결정(옵션 A 승인, PlayerBehavior 분해 충돌 0).
- (참고, 다른 작업) doc/handoffs/2026-06-01/2026-06-01-timer-migration-agent-prompt.md / *-playerbehavior-decomposition*.md.

[너의 첫 행동]
1. git log --oneline -5 + git status --short 로 실측(병렬 커밋으로 [2] 가 바뀌었을 수 있음).
2. main.cpp 배선([1].C) + Bootstrap/Stage CMakeLists 3개가 여전히 미커밋인지 확인.
3. 빌드 green + 적 스폰 로그 재확인.
4. 사용자에게 커밋 갭([2]) 보고 + 묶어서 커밋할지 질의. (무단 커밋 금지.)
```
