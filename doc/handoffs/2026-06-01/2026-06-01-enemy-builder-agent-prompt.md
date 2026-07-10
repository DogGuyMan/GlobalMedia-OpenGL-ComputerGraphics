# 다른 Claude Code Agent 용 프롬프트 — EnemyBuilder (적 스프라이트 + 2프레임 애니)

> 아래 코드블록 전체를 새 Claude Code Agent 세션에 붙여 사용. 자기완결(별도 문서 안 읽어도 됨).
> 정본 spec(참고): `doc/superpowers/specs/2026-06-01-enemy-builder-sprite-design.md`.

---

```
[ROLE]
너는 C++17/CMake OpenGL 탑다운 슈터 프로젝트(/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics, 브랜치 game/module/ingame/temp)의 구현 에이전트다.
목표: 적(enemy)이 현재 비주얼이 없어(물리+Life+AI만) 눈으로 확인 불가하다. 플레이어처럼 스프라이트+2프레임 walk 애니를 적에게 붙여 "적 형태가 화면에 보이게" 한다. 이를 위해 PlayerBuilder 를 미러한 EnemyBuilder 를 새로 만들고, 라이브 스폰 경로(WaveController)를 EnemyBuilder 경유로 라우팅한다.

[중요 — 진행 중 작업과의 경계 (충돌 방지)]
이 저장소는 "PlayerBehavior god-component 분해"가 진행 중이고, 그 분해 Task 5 가 `apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h` 를 수정한다(EnemyContactHandler→Carrier). 충돌을 막기 위해:
- **너는 EnemyFactory.h / CreateEnemyActor 를 절대 수정하지 마라.** EnemyBuilder 는 CreateEnemyActor 가 만든 Actor *위에* 스프라이트만 얹는다.
- **main.cpp 수정 금지(원칙)** — 다른 에이전트가 Fog/PostFX 리팩토링 중(경합). 불가피하면 §라이브경로 참고해 최소 surgical, 사전에 사용자 확인.
- 그 외 너의 작업 파일: ① 신규 Bootstrap/EnemyBuilder.{h,cpp} ② Bootstrap/CMakeLists.txt ③ Stage/WaveController.cpp ④ Stage/CMakeLists.txt (+ 필요시 WaveController.h 에 카운터 멤버). 이 범위를 벗어나지 마라.

[절대 규칙]
- 단위 테스트/TDD 금지(no_auto_tests). 검증은 빌드 + 육안 실행.
- 커밋 금지(git commit/add 금지). 구현+빌드 후 보고만 — 사용자가 리뷰 후 커밋.
- 주석 한국어. 헤더가드 __XXX_H__ (#pragma once 미사용). long 금지, 경로 슬래시.

[배경 사실 — 검증됨]
- 적 텍스처: apps/_MyApp_/src/Playable/Constants.h 의 `const EntityTextureConfig ENEMY_FRONT[3]` (C배열). 각 = {DrawOrder=0, "./resources/texture/enemy/ENEMYn_FRONT.png", RowCount=1, ColCount=2, Flip=false}. PNG 3종 존재. ColCount=2 → 가로 2프레임 walk 스트립.
- EntityTextureConfig 필드: const int DrawOrder; const char* TexturePath; const int RowCount; const int ColCount; const bool Flip;
- CreateEnemyActor(EnemyConfig) [EnemyFactory.h]: EnemyConfig{b2World* world; vmath::vec2 pos; SJH::Scene::Actor* playerTarget; int hp=30; float speed=2.0f; int damage=10; EnemyDeathHandler::DeathFx onDeathFx;} → CircleBody(r=0.4)+Life+SimplePursueAI+EnemyContactHandler(+onDeathFx 시 EnemyDeathHandler) 부착한 std::unique_ptr<SJH::Scene::Actor> 반환(트리 미부착, 스프라이트 없음). EnemyDeathHandler::DeathFx = std::function<void(const vmath::vec3&)>.
- PhysicsSystem 이 매 프레임 b2Body→Actor.Transform 동기 → 적 actor 에 SpriteRenderer(billboard) 직접 부착하면 위치 따라옴. heightOffset 기본 0(ground).
- 플레이어 스프라이트 패턴(PlayerActor.cpp): reg.FindUniformAtlas(path) → 없으면 reg.CreateUniformAtlas(path, path, ColCount, RowCount) → actor->AddComponent<SJH::Sprite::SpriteRenderer>(atlas); spr->flipX=...; (ColCount>1) actor->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(spr, SJH::SpriteSequence::SpriteFrameClip{0, ColCount, fps}); seq->SetIsLoop(true); seq->Play();
- SpriteSequencePlayable ctor(값): (SJH::Sprite::SpriteRenderer*, SJH::SpriteSequence::SpriteFrameClip). SpriteFrameClip{int startFrame, int frameCount, float fps}.
- WaveController.cpp:43-51: inline EnemyConfig 채움(world/pos=RandomEdgePos()/playerTarget/hp=20+mWave*5/speed=1.5f+mWave*0.3f/damage=10) → CreateEnemyActor(cfg) → mSpawnParent->AddChild. 멤버: mWorld/mSpawnParent/mPlayerActor/mWave. onDeathFx 미바인딩.
- Bootstrap/CMakeLists.txt = 명시 소스목록 STATIC myapp_bootstrap. PUBLIC link 에 SJH::engine(sprite/scene/resource_registry 헤더) + MyApp::Entity(EnemyFactory) + MyApp::Physics(b2World) 이미 포함.
- 적은 단일 방향(SimplePursueAI facing 없음) → ENEMY_FRONT 단일방향·단일레이어로 충분. 플레이어의 8그룹/4레이어 패턴 적용하지 마라.

========================================================================
[STEP 0] 라이브 스폰 경로 확인 (먼저)
========================================================================
WaveController 가 실제로 tick(Update)되어 적을 스폰하는지 확인하라:
- grep "WaveController" apps/_MyApp_/src (특히 main.cpp / Stage/StageBuilder / StageStateComponent 에서 컴포넌트로 부착·Update 되는지).
- 적이 현재 화면에 스폰되는 경로를 파악. (a) WaveController 활성 → §STEP3 라우팅으로 충분. (b) main.cpp 수동 스폰 → 사용자에 보고 후 surgical 라우팅. (c) 어디서도 안 스폰 → §STEP3 라우팅 + (필요시) 사용자에 "스폰 트리거 없음" 보고.
파악 결과를 보고에 명시.

========================================================================
[STEP 1] Bootstrap/EnemyBuilder.h (신규)
========================================================================
파일 apps/_MyApp_/src/Bootstrap/EnemyBuilder.h:
```cpp
#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__

#include <functional>
#include <vmath.h>

class b2World;
namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Bootstrap
{
    /// @brief BuildEnemy 입력 의존 (PlayerDeps 미러 — 비싱글턴만).
    struct EnemyDeps
    {
        b2World*           world        = nullptr;
        SJH::Scene::Actor* spawnParent  = nullptr;   ///< 적 child 부착 부모
        SJH::Scene::Actor* playerTarget = nullptr;   ///< SimplePursueAI 추적 대상
        vmath::vec2        pos          = vmath::vec2(0.0f);
        int                hp           = 30;
        float              speed        = 2.0f;
        int                damage       = 10;
        int                variant      = 0;          ///< 0~2 → ENEMY_FRONT[variant % 3]
        float              spriteFps    = 6.0f;        ///< 2프레임 walk 애니 속도
        std::function<void(const vmath::vec3&)> onDeathFx; ///< 선택
    };

    /// @brief 적 1체 조립 — CreateEnemyActor + ENEMY_FRONT 스프라이트/애니 + spawnParent 부착.
    /// @return 트리 부착된 적 Actor* (실패 시 nullptr).
    SJH::Scene::Actor* BuildEnemy(const EnemyDeps& deps);
}

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__
```

========================================================================
[STEP 2] Bootstrap/EnemyBuilder.cpp (신규)
========================================================================
파일 apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp:
```cpp
#include <GL/gl3w.h> // 최상단 — resource_registry.h→framebuffer.h→...→gl3w.h 보다 먼저.

#include "Bootstrap/EnemyBuilder.h"

#include "Entity/Enemy/EnemyFactory.h"   // CreateEnemyActor / EnemyConfig (+box2d)
#include "Playable/Constants.h"           // ENEMY_FRONT
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_sequence_playable.h"

#include <spdlog/spdlog.h>

namespace TopdownShooter::Bootstrap
{
    SJH::Scene::Actor* BuildEnemy(const EnemyDeps& deps)
    {
        // 1) 물리+Life+AI+contact = 기존 factory (무변경)
        Entity::Enemy::EnemyConfig cfg;
        cfg.world        = deps.world;
        cfg.pos          = deps.pos;
        cfg.playerTarget = deps.playerTarget;
        cfg.hp           = deps.hp;
        cfg.speed        = deps.speed;
        cfg.damage       = deps.damage;
        cfg.onDeathFx    = deps.onDeathFx;
        auto enemy = Entity::Enemy::CreateEnemyActor(cfg);   // unique_ptr<Actor> (미부착)

        // 2) ENEMY_FRONT[variant] 스프라이트 + 2프레임 애니 (owner-direct, 단일 레이어)
        const auto& tex = Playable::ENEMY_FRONT[deps.variant % 3];
        auto& reg = SJH::ResourceRegistry::Get();
        auto* atlas = reg.FindUniformAtlas(tex.TexturePath);
        if (!atlas)
            atlas = reg.CreateUniformAtlas(tex.TexturePath, tex.TexturePath, tex.ColCount, tex.RowCount);
        if (atlas)
        {
            auto* spr = enemy->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
            spr->flipX = tex.Flip;
            if (tex.ColCount > 1)   // 2프레임 walk
            {
                auto* seq = enemy->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
                    spr, SJH::SpriteSequence::SpriteFrameClip{0, tex.ColCount, deps.spriteFps});
                seq->SetIsLoop(true);
                seq->Play();
            }
        }
        else
        {
            spdlog::error("[enemy] atlas load 실패: {}", tex.TexturePath);
        }

        // 3) 씬 트리 부착 (entry → 컴포넌트 OnEnter 캐스케이드)
        if (!deps.spawnParent) return nullptr;
        return deps.spawnParent->AddChild(std::move(enemy));
    }
}
```
주의: 네임스페이스 — Entity::Enemy / Playable / SJH::Sprite / SJH::SpriteSequence 가 TopdownShooter 하위인지 확인하고 필요시 정규화(예: TopdownShooter::Entity::Enemy). EnemyFactory.h 의 EnemyConfig 필드명이 위와 다르면 실제 헤더에 맞춰라(수정은 EnemyBuilder.cpp 에서만).

========================================================================
[STEP 3] Bootstrap/CMakeLists.txt — EnemyBuilder.cpp 추가
========================================================================
add_library(myapp_bootstrap STATIC ...) 의 소스 목록에 `EnemyBuilder.cpp` 한 줄 추가:
```cmake
add_library(myapp_bootstrap STATIC
    WorldSceneBuilder.cpp
    PlayerBuilder.cpp
    AudioWarmup.cpp
    EnemyBuilder.cpp
)
```
(link 의존은 이미 충분 — MyApp::Entity[EnemyFactory] + SJH::engine[sprite] + MyApp::Physics[b2World] PUBLIC. game_deps PRIVATE[box2d]. 추가 불필요.)

========================================================================
[STEP 4] WaveController 라우팅 (Stage)
========================================================================
apps/_MyApp_/src/Stage/WaveController.cpp 의 적 스폰부(EnemyConfig inline + CreateEnemyActor + AddChild)를 BuildEnemy 호출로 교체:
- 상단 include 추가: #include "Bootstrap/EnemyBuilder.h"
- (기존 EnemyFactory.h include 가 그 용도로만 쓰였다면 제거 가능 — 다른 데서 쓰면 유지)
- 스폰 코드:
```cpp
    Bootstrap::EnemyDeps d;
    d.world        = mWorld;
    d.spawnParent  = mSpawnParent;
    d.playerTarget = mPlayerActor;
    d.pos          = RandomEdgePos();
    d.hp           = 20 + mWave * 5;
    d.speed        = 1.5f + (mWave * 0.3f);
    d.damage       = 10;
    d.variant      = mSpawnCount % 3;   // 3종 순환
    Bootstrap::BuildEnemy(d);
    ++mSpawnCount;
```
- mSpawnCount 누적 카운터가 WaveController 에 없으면 WaveController.h 에 `int mSpawnCount = 0;` 멤버 추가(variant 순환용).
- **CMake**: Stage/CMakeLists.txt 의 target_link_libraries 에 `MyApp::Bootstrap` 추가(Stage→Bootstrap, acyclic — Bootstrap 은 Stage 미의존). 링크 에러 나면 PUBLIC/PRIVATE 위치 조정.

========================================================================
[검증]
========================================================================
1. 빌드: cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics && cmake --build --preset ninja --target _MyApp_  → exit 0.
2. 실행: cd build_ninja/apps/_MyApp_ && ./_MyApp_ → 적이 ENEMY_FRONT 스프라이트로 표시 + 2프레임 walk 애니 루프. 3종 순환. 플레이어 추적 이동 중 빌보드가 b2Body 따라옴.
3. 안 보이면: atlas 로드 로그 확인 / QueueOffset(벽·바닥에 가려짐 — spr->QueueOffset 조정) / heightOffset(FindPhysics(enemy)->SetHeightOffset) / WaveController 가 실제 tick 되는지(STEP0). 스폰 경로가 막혀 있으면 그것부터 해결(또는 사용자 보고).

[Self-review]
- EnemyFactory.h 를 안 건드렸는가? (분해 Task 5 경계)
- main.cpp 를 안 건드렸는가? (Fog 경합 — 불가피시 사전 보고했는가)
- 변경 파일 = EnemyBuilder.{h,cpp} + Bootstrap/CMakeLists.txt + WaveController.cpp(+.h) + Stage/CMakeLists.txt 범위 내인가?
- 빌드 exit 0 + 적 스프라이트 육안 확인?
- 네임스페이스/EnemyConfig 필드명이 실제 헤더와 정합?

[보고]
DONE / DONE_WITH_CONCERNS / BLOCKED — STEP0 스폰경로 파악 결과 + 변경 파일별 요약 + 빌드 마지막 줄 + 실행 시각 확인(적 보임?) + git status(네 파일). 커밋하지 마라.
```

---

## 사용 메모 (오케스트레이터/사용자용)
- EnemyBuilder 는 **EnemyFactory.h 를 안 건드림** → 내 분해 Task 5(ContactCarrier)와 **충돌 0**. 순서 무관(EnemyBuilder 먼저든 분해 먼저든 OK).
- 새 모듈 의존: **Stage→Bootstrap** 추가됨(acyclic). main.cpp 미접근(WaveController 가 활성일 때). WaveController 비활성이면 스폰 트리거를 먼저 풀어야 적이 보임(STEP0).
- 커밋 메시지(권장): `[feat] : EnemyBuilder — 적 ENEMY_FRONT 스프라이트 + 2프레임 walk 애니 (PlayerBuilder 미러)` (Co-Authored-By 미사용).
