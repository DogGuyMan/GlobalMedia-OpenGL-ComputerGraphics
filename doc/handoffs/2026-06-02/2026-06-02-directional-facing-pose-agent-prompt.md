# 프롬프트 — Directional Facing/Pose 스프라이트 연출 (분해 Task6 잔여 슬라이스, 다른 Claude Code Agent 용)

> 🔴 **완료됨 — 재실행 금지 (2026-06-03):** 이 프롬프트의 작업(directional 8그룹 + RD5 + SetFacing/SetPose)은 이미 구현·**커밋됨 (`a16eef0 "Playable 4방향"`)**. 본 문서는 *설계 기준/리뷰 참조* 로만 보관. PlayerBehavior 분해 전체 현황 = `doc/handoffs/2026-06-03/2026-06-03-pb-decomposition-COMPLETE-resume-handoff.md`(단일 진입점).

> 아래 ``` 코드블록을 새 세션에 그대로 붙여 사용. 자기완결 — 다른 문서 안 읽어도 시작 가능.
> 정본(참고, 안 읽어도 됨): `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md` §3.4/§5.5/§5.7 + `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` Task6 + `doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md` §1.5.
> ⚠ 이 핸드오프는 구 `doc/handoffs/2026-06-02/2026-06-02-task6-on-director-foundation-agent-prompt.md` 의 **directional 슬라이스만** 대체/축소한다. 그 문서의 HitBlink/Dissolve 부분은 이미 완료(`SpriteFxPlayable`)되어 본 작업 범위 밖.
> 재개용 무손실 컨텍스트(전체 상태): `doc/handoffs/2026-06-02/2026-06-02-directional-facing-pose-resume-handoff.md`.

```
[ROLE]
너는 C++17 / CMake / OpenGL 프로젝트(/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics, 브랜치 game/module/ingame/temp)의 구현 에이전트다.
목표: 탑다운 슈터(_MyApp_)의 플레이어가 "이동/조준 방향에 따라 4방향(Front/Back/Left/Right) × 2포즈(Idle/Move) 스프라이트로 전환"되게 만든다.
구성 3-STEP: (1) PlayableDirector 에 8그룹 보유 + SetFacing/SetPose 빈 훅 본문(그룹 가시성 토글), (2) PlayerBuilder 가 8그룹 child 를 미리 빌드 + RegisterGroup, (3) PlayerController 가 매 프레임 velocity/aim → Quantize4 → SetFacing/SetPose push(RD5 단일 작성자).
선행 의존: 연출 foundation(PlayableDirector + PostFXRegistry + SpriteFxPlayable + HpGrayscalePostFX)의 *소스는 이미 커밋됨*(HEAD `8f276a9`); 빌드 와이어링 4파일(src·Bootstrap CMakeLists, PlayerBuilder.cpp, main.cpp)만 미커밋(M, working tree 존재→빌드 OK). 너는 그 위에 "등록/배선만" 추가한다. foundation 의 의미(verb→Play(key), Register/Play/Update 시그니처)는 절대 바꾸지 마라.

[절대 규칙]
- 커밋/git add 금지. 사용자 승인 전까지 구현 + 빌드 검증 + 육안(GUI) 검증 후 "보고"만. (이 브랜치는 병렬 에이전트 多 → 무관 변경이 쓸려 들어가면 복구 불가.)
- `git add -A` / `git commit -a` 금지 — path-scoped only. 무관 dirty 파일 미접근.
- Co-Authored-By 트레일러 미사용 (이 저장소 컨벤션 — 시스템 기본값 무시).
- 단위테스트 자동추가 금지(no_auto_tests). 검증 = 빌드 exit 0 + 실행 육안. (build-only 아님 — GUI 로 facing 전환을 눈으로 확인해야 함.)
- 주석 한국어. 헤더가드 `__XXX_H__` 형식(`#pragma once` 금지). 단 인접 파일은 두 형식 혼용 — PlayableDirector.h = `__TOPDOWNSHOOTER_..._H__`(앞2/뒤_H_), Constants.h/PlayerActor.h = `_TOPDOWNSHOOTER_..._`(앞1, H 없음). 신규 헤더를 둔다면 PlayableDirector.h 와 같은 `__TOPDOWNSHOOTER_..._H__` 권장(단 본 작업은 PlayableDirector 확장이라 신규 헤더가 거의 없음).
- `long` 금지 → `int32_t`/`uint64_t` 등 고정폭. 파일 경로 슬래시(`/`).
- 네임스페이스: 게임 코드는 `TopdownShooter::*`(예: `TopdownShooter::Playable`, `TopdownShooter::Entity`), 엔진 코어는 `SJH::*`. `MyApp::` 는 CMake alias 한정(코드 네임스페이스 아님).

[중요 — 경계 (충돌 방지)]
- OWN(네가 만지는 정확히 3파일 묶음):
  - `apps/_MyApp_/src/Playable/PlayableDirector.{h,cpp}` — DirGroup 보유 + RegisterGroup + SetFacing/SetPose 본문 + Apply (STEP1).
  - `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` — 8그룹 child 빌드 + RegisterGroup + 초기 가시성 (STEP2). ※ 공유 편집점(foundation/hit-FX 와 3자 공유) → surgical add only.
  - `apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}` — RD5 계산→push 배선 + flipX 핵 제거 (STEP3).
- NEVER(의미 변경/타 소유 — 절대 금지):
  - foundation 의미 변경: PlayableDirector 의 Register/Play/Stop/Has/Update 시그니처, "fire"/"hit"/"death" Register 블록(PlayerBuilder.cpp 의 Register("fire"/"hit"/"death") — grep 으로 위치 확인), ReactDamaged/ReactDied/ReactAttack 매핑. PostFXRegistry / PostFXTweenPlayable / HpGrayscalePostFX / SpriteFxPlayable 시그니처.
  - **`apps/_MyApp_/src/Entity/Player/PlayerActor.{h,cpp}` 미접근** — PlayerActor 는 myapp_entity 라이브러리이고 myapp_entity 는 MyApp::Playable 을 link 하지 않는다(아래 [검증된 사실] 링크 토폴로지). 여기서 PlayableDirector::DirGroup/RegisterGroup 을 참조하면 Entity→Playable 링크 사이클로 빌드가 깨진다. 그래서 그룹 빌드는 PlayerActor 가 아니라 **PlayerBuilder(bootstrap)** 에서 한다. PlayerActor 는 손대지 마라.
  - 코어 `src/playable/*`, `src/timer/*`, `*.fs`/`*.frag`/`*.vs` 셰이더, `src/buffer/framebuffer.h`, `src/material/pass.h`, `src/render/property_block_setter.cpp`, `src/sprite/sprite_component.cpp` (병렬 FX/렌더 코어 트랙 dirty).
  - 게임 도메인 타 소유: `apps/_MyApp_/src/Spawns/*`, `Entity/Enemy/EnemyFactory.h`, `Entity/Bullet/bullet_factory.h`, `Stage/*`, `EnemyDeathHandler`/`Carrier`, `Entity/Components/LifeComponents.h`(Life — 이미 sink 호출하므로 읽기만), `apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp`(적 스폰 병렬 트랙 — 이미 dirty, IDE 오픈 中).
  - `main.cpp`: PostFX/Fog/카메라 라인 미접근. (directional 작업은 main.cpp 를 건드릴 필요 0.)
  - `shell/CMakeExecute.sh` / `extern/Catch2` / 루트 `CMakeLists.txt` / `cmake/Doxygen.cmake` / `doxygen/direction/*` 미접근(병렬/서브모듈/Doxygen 잡음).
- `Components.Interfaces.h` 는 *읽기만* — EFacing/EPose/Quantize4/IActorPresentation 이미 정의됨(아래). 추가/수정 금지. (만약 Quantize4 부호가 틀리면 → [검증] §부호 트랩 우회 참조, 본체 수정 금지.)

[검증된 사실 — 현 인프라 (실측 2026-06-02, file:line 재확인됨. ⚠ 본인이 grep 으로 재검증할 것 — rename 8b5fc45 + 병렬커밋 진행 中)]
- ★ 링크 토폴로지(이 작업의 구조를 결정하는 핵심):
  - `myapp_playable`(MyApp::Playable) 는 `MyApp::Entity` 를 PUBLIC link (Playable/CMakeLists.txt:23 — IActorPresentation 참조). 즉 Entity→Playable 방향 link 추가 시 **순환**.
  - `myapp_bootstrap`(PlayerBuilder) 는 `MyApp::Entity` + `MyApp::Playable` **둘 다** link (Bootstrap/CMakeLists.txt:21,29). → 그룹 빌드 + RegisterGroup 은 여기서 안전.
  - `myapp_input_handler`(PlayerController) 는 `MyApp::Entity` link, **`MyApp::Playable` 미link** (InputHandler/CMakeLists.txt:32). → PlayerController 는 `PlayableDirector` 구체타입을 참조할 수 없다. sink 은 반드시 `Entity::IActorPresentation*` 인터페이스로 타이핑하고 `GetComponent<Entity::IActorPresentation>()` 로 받아라(엔진 GetComponent 가 인터페이스 조회 지원 — 분해 Task0). PlayerController.h 는 이미 `Entity/Components/Components.Interfaces.h` 를 include 하므로 IActorPresentation 가용.
- 채울 빈 훅 = `apps/_MyApp_/src/Playable/PlayableDirector.h:47-48`:
    void SetFacing(Entity::EFacing /*facing*/) override {}   // 47행
    void SetPose(Entity::EPose /*pose*/) override {}         // 48행
  같은 파일 43-45: ReactDamaged→Play("hit") / ReactDied(.cpp)→Play("death") / ReactAttack→Play("attack"). 이미 등록된 키 = "fire"/"hit"/"death" — 건드리지 마라.
- PlayableDirector 시그니처(무변경): `Register(const std::string&, std::unique_ptr<SJH::Playable::PlayableBase>) -> PlayableDirector&`(fluent, 같은 키=덮어쓰기) / `void Play(const std::string&)`(Stop()+Play()+슬롯 활성, 미등록=silent no-op) / `void Stop / bool Has / void Update(float dt) override`. 보유 Playable 은 AddComponent 안 하고 director 가 직접 Update — Play 된 슬롯만 tick.
- EFacing/EPose/Quantize4 (`apps/_MyApp_/src/Entity/Components/Components.Interfaces.h:85-95`, namespace TopdownShooter::Entity):
    enum class EFacing : int { Front = 0, Back, Left, Right };
    enum class EPose   : int { Idle  = 0, Move };
    inline EFacing Quantize4(vmath::vec2 v) {   // 부호: x>0=Right, z>0=Front (탑다운 W=-Z)
        if (std::abs(v[0]) > std::abs(v[1])) return v[0] > 0.0f ? EFacing::Right : EFacing::Left;
        return v[1] > 0.0f ? EFacing::Front : EFacing::Back;
    }
  IActorPresentation (같은 파일 100-118): defaulted no-op ReactDamaged/ReactDied/ReactAttack/FaceAim/SetFacing/SetPose.
- 8 directional 자산 상수 *전부 이미 존재* — `apps/_MyApp_/src/Playable/Constants.h`(namespace TopdownShooter::Playable): PLAYER_{FRONT,BACK,LEFT,RIGHT}_{IDLE,MOVE} 8개 + 구조체 `EntityTextureConfig`. 필드 순서 = {DrawOrder, TexturePath, RowCount, ColCount, Flip} (RowCount 가 ColCount 보다 먼저! ColCount>1 = 가로 N프레임 애니, RowCount=1 고정). RIGHT_* 는 LEFT_* PNG + Flip=true 재사용(디스크에 RIGHT PNG 없음). 예:
    const std::vector<EntityTextureConfig> PLAYER_FRONT_MOVE = {
        {-0, "./resources/texture/player/FRONT_MOVE_E_0.png", 1, 1},
        {-1, "./resources/texture/player/FRONT_MOVE_H_1.png", 1, 1},
        {-2, "./resources/texture/player/FRONT_MOVE_B_2.png", 1, 2},  // 애니 B파트 (ColCount=2)
        {-3, "./resources/texture/player/FRONT_MOVE_F_3.png", 1, 1},
    };
  ⚠ 애니(B) 파트의 DrawOrder 는 방향마다 다를 수 있음 → "애니=고정 인덱스" 가정 금지, `t.ColCount>1` 로만 판정.
- PlayerActor.h: SpriteCfg.direction = `const std::vector<TopdownShooter::Playable::EntityTextureConfig>*`(이미 Playable/Constants.h 를 #include — 헤더만이라 링크심볼 없음 → 컴파일 OK). PlayerActor.cpp 의 `if(cfg.sprite.direction!=nullptr)` 블록이 단일방향(FRONT_MOVE) 4-레이어 child 를 생성. **본 작업은 이 블록을 안 건드린다** — 대신 PlayerBuilder 가 direction 을 nullptr 로 둬서 이 블록을 무력화하고, 8그룹을 PlayerBuilder 에서 직접 빌드한다.
- PlayerBuilder.cpp 현행 라인(grep `director->Register(` / `AddComponent<.*PlayableDirector>` 로 재확인 — 아래는 실측): director 부착 = :73 `auto *director = spriteActor->AddComponent<TopdownShooter::Playable::PlayableDirector>();`(AddChild 보다 *먼저* — Life::OnEnter 가 sink 캐시). 단일방향 주입 = :62 `pac.sprite.direction = &TopdownShooter::Playable::PLAYER_FRONT_MOVE;`. Register("fire")=:106 / Register("hit")=:126 / Register("death")=:133(단일 SpriteDissolvePlayable, Composite 아님). 입력 콜백 SetFireCallback/SetDamageCallback = :140-141. ★ HP비율 화면 grayscale 은 :75-76 `AddComponent<HpGrayscalePostFX>("grayscale_vignetting","uGrayscaleAmount")`([A] 상시 바인더) — death 컴포지트에는 grayscale 없음. 건드리지 마라.
- 가시성 토글 메커니즘(중요): SpriteRenderer 는 `SJH::Sprite::SpriteRenderer`(SJH::Scene::MeshRenderer 상속). 가시성 = base 멤버 `bool Visible = true;`(`src/render/mesh_renderer.h:61`). SceneRenderer 가 `mr->IsEnabled() && mr->Visible && mr->Mesh && mr->Material` 로 게이트(`src/render/scene_renderer.cpp:128`). ★ 그룹 숨김은 `SpriteRenderer->Visible=false` 로 한다(SetActive 아님). 이유: `scene_renderer.cpp:119 if(!actor.IsActive())` + `actor.cpp:115 if(!mActive)` 때문에 SetActive(false) 는 렌더 *와* tick 을 둘 다 끈다 — 그러면 비활성 그룹의 걷기 애니(bPart 루프)가 멈춰서 재진입 시 프레임이 튄다. Visible 만 끄면 child 는 active 라 bPart 애니가 계속 돌아 재진입이 매끈하다([A] 패턴).
- 애니 컴포넌트: `SJH::SpriteSequence::SpriteSequencePlayable`(namespace = SJH::SpriteSequence). 값-소유 ctor `(SJH::Sprite::SpriteRenderer*, SJH::SpriteSequence::SpriteFrameClip)` → make_unique in-place. SetIsLoop(true).Play() 패턴. ★ directional bPart 는 child Actor 의 Component 라 scene tree 가 직접 tick([A]) — director.Update 슬롯과 무관. 빌드 시 한 번 Play 해두면 끝(비활성이어도 계속 tick). director 가 이를 Stop 하지 마라.
- 현재 PlayerController facing 처리 (`apps/_MyApp_/src/InputHandler/PlayerController.cpp:171-177`): Update() 안 `owner->GetTransform().EulerRot[1] = mAimAngleY`(손 궤도 — 유지) + `mCachedSprite = owner->GetComponent<SJH::Sprite::SpriteRenderer>()`(lazy 캐시, :175) + `mCachedSprite->flipX = (mAimDirection[0] < 0.0f)`(:177 구 좌우반전 핵 — 제거 대상). 멤버: `mInputValue`(vec3 XZ 입력)·`mAimDirection`(vec3 정규화 조준)·`mCachedSprite`(SpriteRenderer*, h:101 + h:18 forward). `mInputValue` 는 DoForward 직후(:161 근처) `vec3(0)` 으로 리셋됨.
- SetFacing/SetPose 호출자 현재 0건(grep). 너가 첫 writer.

========================================================================
[결정점 D1 — 작업 시작 전 사용자에게 확인] foundation 와이어링 커밋
========================================================================
★ 업데이트(2026-06-02 재측정): foundation 의 **Playable/ 소스 13파일은 이미 커밋됨**(HEAD `8f276a9` [dev] : 연출 디렉터 — PlayableDirector/PostFXRegistry/PostFXTweenPlayable/SpriteFxPlayable/HpGrayscalePostFX + CMake, BulletSpawnPlayable 삭제 fold). 즉 PlayableDirector.{h,cpp} 는 *커밋된* 파일을 편집(M 됨)하면 된다.
남은 미커밋(M) = **빌드 와이어링 4파일**: `src/CMakeLists.txt`(add_subdirectory(Playable)+link), `Bootstrap/CMakeLists.txt`(link), `Bootstrap/PlayerBuilder.cpp`(director 부착+Register), `main.cpp`(PostFXRegistry::Register). 이 4파일은 working tree 에 존재하므로 **빌드는 그대로 된다**(clean checkout 가 아닌 한). 두 경로 중 택1을 사용자에게 제안:
- (a) 권장: "빌드 와이어링 4 tracked-M 를 path-scoped 로 먼저 커밋" 요청 후 작업 → 네 directional 변경이 깨끗이 분리됨. 범위: `git add apps/_MyApp_/src/CMakeLists.txt apps/_MyApp_/src/Bootstrap/CMakeLists.txt apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp apps/_MyApp_/main.cpp`. ⚠ 단 PlayerBuilder.cpp 는 STEP2 에서 너도 편집하므로, 와이어링만 먼저 커밋하려면 네 STEP2 *전*에 해야 함(아니면 네 변경까지 섞임). EnemyBuilder.cpp / PlayerHand.{cpp,h} / src/{buffer,material,render,sprite}/* / billboard_atlas.vs / Entity·Stage / 루트 CMakeLists·Doxygen 은 절대 함께 stage 금지.
- (b) 미커밋 위에서 그대로 작업 → 빌드는 되지만 네 변경 + 와이어링이 working tree 에서 섞임. 보고 시 "내가 추가/변경한 파일·라인"을 명시.
어느 쪽이든 너는 커밋하지 않는다. (a) 를 먼저 제안하라.

========================================================================
[STEP 1] PlayableDirector — DirGroup 보유 + RegisterGroup + SetFacing/SetPose 본문 + Apply
========================================================================
PlayableDirector.h — public 에 DirGroup/RegisterGroup, private 에 그룹 멤버 추가. 기존 멤버/시그니처 불변. (별도 PlayerSpriteDirector 클래스 신설 금지 — 사용자 철학 "게임로직 외 모든 연출은 PlayableDirector 경유". plan 의 PlayerSpriteDirector 원설계는 폐기, 로직만 여기로 흡수.)
```cpp
#include <array>
// 헤더 상단 forward (실타입 include 는 .cpp 에서 — 헤더 의존 최소화):
namespace SJH::Sprite { class SpriteRenderer; }

// --- public: (기존 Register/Play/Stop/Has/Update 아래에 추가) ---
    /// @brief (facing,pose) 한 그룹 = 그 방향/포즈의 4-레이어 SpriteRenderer 핸들.
    struct DirGroup
    {
        std::array<SJH::Sprite::SpriteRenderer *, 4> layers{};   // E·H·B·F (널 가능 — 레이어 <4)
    };
    /// @brief 그룹 등록 (빌드 시 1회). idx = static_cast<int>(enum).
    void RegisterGroup(Entity::EFacing f, Entity::EPose p, const DirGroup &g) { mGroups[idx(f)][idx(p)] = g; }
    /// @brief 등록 끝난 뒤 PlayerBuilder 가 1회 호출 — 초기 (Front,Idle) 만 보이게.
    void RefreshDirectional() { Apply(); }

    // 기존 빈 훅 47-48 을 아래 선언으로 교체 (본문은 .cpp):
    void SetFacing(Entity::EFacing f) override;
    void SetPose(Entity::EPose p) override;

// --- private: ---
    static int idx(Entity::EFacing f) { return static_cast<int>(f); }
    static int idx(Entity::EPose p)   { return static_cast<int>(p); }
    void Apply();                                   // (mFacing,mPose) 그룹만 Visible=true

    DirGroup        mGroups[4][2]{};
    Entity::EFacing mFacing = Entity::EFacing::Front;
    Entity::EPose   mPose   = Entity::EPose::Idle;
```
PlayableDirector.cpp — SetFacing/SetPose/Apply 구현. **velocity 를 절대 읽지 않는다**(작성자는 PlayerController = RD5):
```cpp
#include "sprite/sprite_component.h"   // SJH::Sprite::SpriteRenderer (Visible)

void PlayableDirector::SetFacing(Entity::EFacing f)
{
    if (f == mFacing) return;
    mFacing = f;
    Apply();
}
void PlayableDirector::SetPose(Entity::EPose p)
{
    if (p == mPose) return;
    mPose = p;
    Apply();
}
void PlayableDirector::Apply()
{
    for (int f = 0; f < 4; ++f)
        for (int p = 0; p < 2; ++p)
        {
            const bool active = (f == idx(mFacing) && p == idx(mPose));
            for (auto *layer : mGroups[f][p].layers)
                if (layer) layer->Visible = active;   // ★ Visible 로만 — SetActive 금지(렌더+tick 둘 다 꺼짐)
        }
    // bPart 는 child 의 scene-Component([A])라 항상 tick — 여기서 Play/Stop 하지 않는다(재진입 프레임 점프 방지).
}
```
- bPart(걷기 애니)는 DirGroup 에 보유하지 않는다 — child Actor 의 SpriteSequencePlayable 가 scene tree 로 계속 tick 한다([A]). director 는 가시성만 토글.

========================================================================
[STEP 2] PlayerBuilder — 8그룹 child 빌드 + RegisterGroup (PlayerActor 아님!)
========================================================================
★ 그룹 빌드는 반드시 PlayerBuilder.cpp(bootstrap, Entity+Playable 둘 다 link)에서. PlayerActor.cpp(myapp_entity)에서 하면 Entity→Playable 링크 사이클.
PlayerBuilder.cpp — (1) `:62` 단일방향 주입을 무력화(direction 설정 제거 또는 nullptr), (2) director 부착(:73) *직후*, AddChild(scene 편입) *전*에 8그룹을 spriteActor 의 child 로 빌드 + RegisterGroup + 초기 가시성:
```cpp
namespace P = TopdownShooter::Playable;
using TopdownShooter::Entity::EFacing;
using TopdownShooter::Entity::EPose;

// (구 `pac.sprite.direction = &P::PLAYER_FRONT_MOVE;` (:62) 는 제거 — PlayerActor 단일방향 블록 비활성화)

struct DirGroupSrc { EFacing f; EPose p; const std::vector<P::EntityTextureConfig> *layers; };
static const std::vector<DirGroupSrc> kPlayerGroups = {
    {EFacing::Front, EPose::Idle, &P::PLAYER_FRONT_IDLE}, {EFacing::Back,  EPose::Idle, &P::PLAYER_BACK_IDLE},
    {EFacing::Left,  EPose::Idle, &P::PLAYER_LEFT_IDLE},  {EFacing::Right, EPose::Idle, &P::PLAYER_RIGHT_IDLE},
    {EFacing::Front, EPose::Move, &P::PLAYER_FRONT_MOVE}, {EFacing::Back,  EPose::Move, &P::PLAYER_BACK_MOVE},
    {EFacing::Left,  EPose::Move, &P::PLAYER_LEFT_MOVE},  {EFacing::Right, EPose::Move, &P::PLAYER_RIGHT_MOVE},
};

// --- director 부착(:73 `auto *director = spriteActor->AddComponent<P::PlayableDirector>();`) 직후 삽입 ---
auto &reg = SJH::ResourceRegistry::Get();
constexpr float kFps = 8.0f;   // 애니(ColCount>1) 초당 프레임 (SpriteCfg 기본값과 동일)
for (const auto &grp : kPlayerGroups)
{
    P::PlayableDirector::DirGroup dg{};
    int layerIdx = 0;
    const bool initiallyVisible = (grp.f == EFacing::Front && grp.p == EPose::Idle);
    for (const auto &t : *grp.layers)
    {
        auto *atlas = reg.FindUniformAtlas(t.TexturePath);                 // LEFT/RIGHT 같은 PNG → Find 먼저(중복키 nullptr 회피)
        if (!atlas) atlas = reg.CreateUniformAtlas(t.TexturePath, t.TexturePath, t.ColCount, t.RowCount);
        if (!atlas) { spdlog::error("[8layer] atlas load 실패: {}", t.TexturePath); continue; }

        auto child = std::make_unique<SJH::Scene::Actor>(
            "player_dir_" + std::to_string(static_cast<int>(grp.f)) + "_" +
            std::to_string(static_cast<int>(grp.p)) + "_L" + std::to_string(t.DrawOrder));
        SJH::Scene::Actor *childPtr = spriteActor->AddChild(std::move(child));

        auto *spr        = childPtr->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
        spr->flipX       = t.Flip;
        spr->QueueOffset = t.DrawOrder;
        spr->Visible     = initiallyVisible;                               // ★ 초기 가시성 — Front/Idle 만 true
        if (layerIdx < 4) dg.layers[layerIdx] = spr;

        if (t.ColCount > 1)                                                // 애니(B) 파트
        {
            auto *seq = childPtr->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
                spr, SJH::SpriteSequence::SpriteFrameClip{0, t.ColCount, kFps});
            seq->SetIsLoop(true);
            seq->Play();                                                   // [A] scene-tick — 비활성 그룹도 계속 돌게 둠
        }
        ++layerIdx;
    }
    director->RegisterGroup(grp.f, grp.p, dg);
}
director->RefreshDirectional();   // 등록 끝 — 초기 가시성 확정(멱등)
```
- spriteActor 가 이미 child 들을 갖고 있고 그 중 첫 애니 자식을 PlayerResult 로 노출하는 호환 블록이 있으면(grep `SpriteSeq`/`첫 애니`), 그룹 8개로 늘면서 "첫 애니 자식" 의미가 모호해진다 — 빌드만 통과하면 OK 로 두되 보고에 언급.
- `pac.sprite.groups` 같은 PlayerActor 쪽 신규 필드는 만들지 마라(PlayerActor 미접근). 그룹 소스 테이블은 PlayerBuilder 로컬(kPlayerGroups)로 충분.

========================================================================
[STEP 3] PlayerController — RD5 단일 작성자 (interface sink, flipX 핵 교체)
========================================================================
PlayerController 는 MyApp::Playable 을 link 하지 않으므로 PlayableDirector 구체타입 금지 → `Entity::IActorPresentation*` 인터페이스로만.
PlayerController.h — 멤버 추가(private). (`Entity/Components/Components.Interfaces.h` 는 이미 include 됨):
```cpp
TopdownShooter::Entity::IActorPresentation *mSink = nullptr;   // 방향 sink = PlayableDirector(인터페이스로 보유, lazy 캐시)
TopdownShooter::Entity::EFacing mLastFacing = TopdownShooter::Entity::EFacing::Front;
float mAttackWindowSec = 0.15f;                                // 확정 §6.3
float mAttackTimer     = 0.0f;
```
PlayerController.cpp Update() — 기존 :171-177 의 flipX 핵을 RD5 push 로 교체. EulerRot[1]=mAimAngleY(:171)는 유지. velXZ 는 mInputValue 리셋(:161) *전*에 확보:
```cpp
namespace E = TopdownShooter::Entity;
// (mCachedSprite/flipX 라인 :175-177 제거)

// === RD5: facing/pose 단일 작성자 (controller 계산 → sink 토글) ===
if (mSink == nullptr)
    mSink = owner->GetComponent<E::IActorPresentation>();   // lazy 캐시 (인터페이스 조회 — 엔진 GetComponent 지원)
if (mSink != nullptr)
{
    const bool attacking = (mAttackTimer > 0.0f);
    if (attacking) mAttackTimer -= dt;

    const vmath::vec2 velXZ(mInputValue[0], mInputValue[2]);                 // 이번 프레임 입력 (리셋 전!)
    const bool moving = (velXZ[0] * velXZ[0] + velXZ[1] * velXZ[1]) > 0.001f;
    const vmath::vec2 aimXZ(mAimDirection[0], mAimDirection[2]);

    E::EFacing facing = attacking ? E::Quantize4(aimXZ)
                      : moving    ? E::Quantize4(velXZ)
                                  : mLastFacing;
    E::EPose pose = (attacking || moving) ? E::EPose::Move : E::EPose::Idle;

    mSink->SetFacing(facing);
    mSink->SetPose(pose);
    mLastFacing = facing;
}
```
- ⚠ 순서: 위 블록은 `mInputValue` 가 살아있어야 하니 DoForward 직후(:161 리셋 *전*)에 두거나 리셋 전에 velXZ 를 떠둬라.
- 발사 시 attacking facing 을 aim 으로 쓰려면 OnFirePressed()에서 `mAttackTimer = mAttackWindowSec;` arm(선택 — 없어도 moving/lastFacing 으로 동작).
- mCachedSprite/flipX 제거 후 `mCachedSprite`(h:101) + forward(h:18) + (다른 곳에서 안 쓰면) `#include "sprite/sprite_component.h"` 가 미사용 → -Werror 경고. 미사용 확인 후 함께 정리(단 sprite_component.h 가 다른 심볼로 쓰이면 include 유지).

[검증]
- 빌드: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_` → exit 0 (sb7 gl.h/gl3.h #warning 만 — -Wno-error). 링크 에러(Entity→Playable / InputHandler→Playable)가 나면 구조 위반 — STEP2/STEP3 경계 재확인.
- 실행: `cd build_ninja/apps/_MyApp_ && ./_MyApp_` → 무크래시. 육안: WASD 이동 방향에 따라 플레이어 스프라이트가 Front/Back/Left/Right 전환 + 정지=Idle / 이동=Move 포즈 전환.
  - ⚠ 부호 트랩: Quantize4 는 W=-Z(Front=z>0) 전제. W 눌렀을 때 Front 가 아니라 Back 이 보이거나 좌우가 반대면 → **1차 조치 = Components.Interfaces.h(읽기전용) 를 건드리지 말고 controller 의 velXZ/aimXZ 구성에서 부호를 뒤집어 우회**(예: `vmath::vec2 velXZ(mInputValue[0], -mInputValue[2])`), 그 사실을 보고에 명시. Quantize4 본체 수정은 소유자 승인 후 후순위. (PhysicsMovement 가 world XZ→b2Vec2(x,-z) 매핑하는 트랩 존재 — 부호 확인 필수.)
  - 무관: muzzle / distortion.efk 로드 [error] 1건은 기존/별개 VFX 에이전트 영역 — 회귀 아님. fire FX 안 보여도 정상.

[Self-review]
- 그룹 빌드를 PlayerBuilder(bootstrap)에서 했나(PlayerActor.cpp 미접근 — Entity→Playable 사이클 회피)?
- PlayerController sink 을 `Entity::IActorPresentation*` 로 타이핑했나(PlayableDirector 구체타입 미참조 — InputHandler→Playable 미link)?
- SetFacing/SetPose 본문이 mGroups Visible 토글만 하고 velocity 를 안 읽나(RD5 단일 작성자)? 비활성 그룹 bPart 를 Stop 하지 않았나?
- 그룹 숨김에 SetActive 가 아니라 SpriteRenderer.Visible 을 썼나? 초기 가시성(Front/Idle)을 빌드 시 + RefreshDirectional()로 확정했나?
- "fire"/"hit"/"death" Register·Register/Play/Update 시그니처·PostFXRegistry/HpGrayscalePostFX 무변경인가?
- LEFT/RIGHT 같은 PNG → FindUniformAtlas 먼저(중복키 nullptr 회피)인가?
- OWN 3파일 묶음(PlayableDirector.{h,cpp} / PlayerBuilder.cpp / PlayerController.{h,cpp})만 변경했나? PlayerActor·EnemyBuilder·셰이더·Spawns·Stage·Life·main.cpp 미접근인가?
- 빌드 exit 0 + 육안 facing 전환 확인했나?

[보고]
DONE / DONE_WITH_CONCERNS / BLOCKED + 변경 파일별 요약 + 빌드 마지막 줄 + 실행 육안 관찰(방향 전환 됐나 / 부호 맞나) + 미처리·조율필요(예: Quantize4 부호 반대로 controller 우회함, PlayerResult 첫-애니 스캔 모호, D1 커밋 경로) + `git status --short`(네 파일). 커밋 금지.
```

---

## 사용 메모 (오케스트레이터/사용자용) — 코드블록 *밖*

- **이 작업의 성격**: 빌드 + 육안(GUI) 검증. 자산은 이미 디스크에 있고(player PNG 다수 + Constants.h 8그룹) 로딩 방식도 해결됨(개별 PNG → 각자 UniformAtlas) — 신규 로더/atlas 패킹 불필요. 순수 "8그룹 빌드 + Visible 토글 + RD5 push 배선".
- **구조 결정(critic 적대검수 반영)**: synth 초안은 그룹 빌드를 PlayerActor.cpp 에서 하고 sink 을 PlayableDirector 구체타입으로 받았으나 — 둘 다 **링크 사이클**(Entity→Playable / InputHandler→Playable)이라 컴파일 불가. 정정: 그룹 빌드 = PlayerBuilder(bootstrap, 둘 다 link), sink = `Entity::IActorPresentation*`(인터페이스). PlayerActor 는 미접근.
- **[선행 커밋 가드 — 2026-06-02 재측정]**: foundation 의 **Playable/ 소스 13파일은 이미 커밋됨**(HEAD `8f276a9`). 남은 미커밋 = **빌드 와이어링 4 tracked-M**(src/CMakeLists.txt, Bootstrap/CMakeLists.txt, Bootstrap/PlayerBuilder.cpp, main.cpp) — working tree 에 존재해 빌드는 됨. 깔끔한 분리 원하면 STEP2 *전*에 `git add` 이 4파일만(PlayerBuilder 는 곧 내가 편집하니 타이밍 주의). Playable/ 글롭은 더는 불필요(커밋됨). (death 컴포지트는 grayscale 제거됨 — HpGrayscalePostFX 가 HP비율 상시 구동.) 신규 병렬 dirty PlayerHand.{cpp,h} 함께 stage 금지.
- **경합점**: PlayerBuilder.cpp 는 foundation/directional(본Task)/hit-FX 3자 공유 — surgical add only. EnemyBuilder.cpp 는 적 스폰 병렬 트랙(IDE 오픈 中) — 충돌 0 이지만 절대 stage 금지. main.cpp 는 Fog 병렬과 경합 가능(directional 미접근). BulletSpawnPlayable.{cpp,h}(D) + Physics/filter.h(D)↔PhysicsLayer.h(신규)는 소속 모호 → 별도 확인.
- 결과(DONE+diff) 받으면 spec(§3.4 RD5 / §5.5) 대비 검증 후 커밋. 커밋 메시지(권장): `[dev] : directional facing/pose sprite switching`. **Co-Authored-By 미사용.**
- 정본 보강(선택): `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` Task6 에 원설계 코드(원 PlayerSpriteDirector 형태 — 재편으로 PlayableDirector 흡수, 로직 동형).
