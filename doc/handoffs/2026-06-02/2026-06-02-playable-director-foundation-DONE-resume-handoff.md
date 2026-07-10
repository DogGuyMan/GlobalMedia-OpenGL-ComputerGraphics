# Resume Handoff — PlayableDirector 연출 foundation **구현 완료** (커밋 미승인)

> 2026-06-02 작성. 이 문서는 [`2026-06-02-playable-director-foundation-agent-prompt.md`](2026-06-02-playable-director-foundation-agent-prompt.md) 프롬프트를 실행한 **결과 보고 + 무손실 컨텍스트**다.
> 받는 에이전트는 이 대화 맥락이 전혀 없다 — 이 문서만으로 자족적으로 이어갈 수 있게 작성했다.
> **이 문서가 단일 진입점.** 원본 프롬프트는 입력(완료됨), 이 문서가 현재 상태.

---

## TL;DR + 다음 액션

- **무엇**: 게임 로직(HP/물리/입력판정) 제외, 모든 연출/사운드를 `PlayableDirector`의 named Playable(`verb→Play(key)`)로만 구동하는 **공유 foundation**을 신설하고, 기존 플레이어 onFire/onDamage를 이 위로 **회귀0** 마이그레이션했다.
- **상태**: 빌드 exit 0, 런타임 무크래시, 4-에이전트 adversarial review = 3 PASS + 1 CONCERN(의도된 설계 델타). **커밋 안 함**(사용자 승인 대기).
- **다음 액션(둘 중 택1, 병렬 가능)**:
  1. **hit-FX 트랙** — `"hit"`/`"death"` 키에 Vignette/Grayscale/Damaged Composite 등록(PostFXTweenPlayable이 `PostFXRegistry::Get().Material("grayscale_vignetting")`로 도달).
  2. **Task6(PlayerBehavior 분해)** — `PlayableDirector::SetFacing/SetPose` 빈 훅을 채우고 directional sprite Playable + HitBlink/Dissolve를 키로 등록. → 전용 프롬프트: [`2026-06-02-task6-on-director-foundation-agent-prompt.md`](2026-06-02-task6-on-director-foundation-agent-prompt.md)
- **먼저 할 일**: 사용자에게 path-scoped 커밋 승인받기(아래 §커밋). 그 전엔 추가 작업이 내 미커밋 변경과 섞인다.

---

## State of the world — 재측정 (2026-06-02 작성 시점)

- **브랜치**: `game/module/ingame/temp`
- **HEAD**: `d44a4a3 [dev] : impulse` (내 세션 중 **커밋 0건** — HEAD 불변)
- `git log --oneline -3`: `d44a4a3 impulse` / `8b5fc45 rename class & file` / `b4e5eb8 effekseer update and postFX update`
- **내 변경 = 미커밋**(아래 §파일). 다른 dirty는 **병렬 에이전트 소유**(§충돌 매트릭스).

---

## Task / Step 상태

| 단계 | 상태 | 비고 |
|---|---|---|
| PostFXRegistry.{h,cpp} 신설 | ✅ DONE | Meyer's 싱글톤 `passName→SJH::Material*` |
| PlayableDirector.{h,cpp} 신설 | ✅ DONE | Component+IActorPresentation, Register/Play/Stop/Has + 중앙 tick + SetSpawnContext |
| MyApp::Playable CMake lib + 우산/Bootstrap 배선 | ✅ DONE | Entity↔Spawns 사이클 회피 위해 전용 lib |
| onFire/onDamage → director 마이그레이션 | ✅ DONE | `"fire"`/`"damaged_test"` 등록 + 콜백 라우팅, 회귀0 |
| main.cpp PostFXRegistry 등록 1줄 | ✅ DONE | `startup()` (init() 아님 — 아래 결정 #4) |
| 빌드 exit 0 + 런타임 무크래시 | ✅ DONE | clean rebuild 30/30 재확인 |
| enemy/bullet onDeathFx/SetOnHitFx delegate 제거 | ❌ 미착수 | **Task5/7 소유 — 본 foundation 미접근**(조율 필요) |
| 커밋 | ⏸ 대기 | 사용자 승인 필요 |

---

## 변경 파일 (내가 작성 — path-scoped, 이것만 커밋)

**신설 — `apps/_MyApp_/src/Playable/` (신규 STATIC lib `MyApp::Playable`)**
- `PlayableDirector.h` / `.cpp` — 디렉터 본체.
- `PostFXRegistry.h` / `.cpp` — PostFX Material 레지스트리.
- `CMakeLists.txt` — `myapp_playable` STATIC.

**편집(M)** — `apps/_MyApp_/main.cpp`, `apps/_MyApp_/src/Bootstrap/{CMakeLists.txt, PlayerBuilder.cpp}`, `apps/_MyApp_/src/CMakeLists.txt`.

---

## 검증된 사실 — `file:line` (작성 시점 재확인, 받는 쪽도 재검증할 것)

### 코어(읽기 전용, 수정 금지)
- `src/playable/playable_base.h` — `PlayableBase : public IPlayable, public SJH::Scene::Component`. `Play()`={paused_=false;finished_=false;OnPlay()} / `Stop()`={...elapsed_=0;OnStop()} / `Update(float) final`={if(!IsEnabled()||paused_||finished_)return; elapsed_+=dt; OnUpdate(dt)}. **OnPause 훅 없음, public Reset() 없음**(Stop이 리셋). 필수 override = `OnUpdate(float)`만. `SetIsLoop(bool)` 추가 setter.
- `src/playable/composite_playable.{h,cpp}` — `SequencePlayable`(Append/Insert/AppendInterval) / `ParallelPlayable`(**Join만**). children = `vector<unique_ptr<IPlayable>>`. **child는 Actor 미부착이라 composite OnUpdate가 `dynamic_cast<Component*>(child)->Update(dt)`로 수동 디스패치**(composite_playable.cpp:65-66, 103-104). → director가 composite 1개만 tick해도 전체 서브트리 구동됨.
- `src/scene/actor.h:47` — `Component::mEnabled = true`(기본). `IsEnabled()`=mEnabled. → **미부착 PlayableBase도 Update가 통과**(director 직접 tick의 근거). `GetComponent<T>` is_polymorphic 게이트 → 인터페이스 조회는 dynamic_cast(자식 actor 비재귀).
- `apps/_MyApp_/src/Entity/Components/Components.Interfaces.h` — `IActorPresentation`(net `TopdownShooter::Entity`, **`::Components` 아님**): 전부 defaulted no-op `ReactDamaged(int)/ReactDied(vec3)/ReactAttack(vec2)/FaceAim(vec2)/SetFacing(EFacing)/SetPose(EPose)`. `enum class EFacing{Front,Back,Left,Right}; EPose{Idle,Move};`. copy/move 삭제+protected 기본 ctor. `Quantize4(vec2)→EFacing` 자유함수 존재(x>0=Right,z>0=Front).
- `apps/_MyApp_/src/Entity/Components/LifeComponents.h:23,68-73,98,112` — `Life`가 `mSink=GetOwner()->GetComponent<IActorPresentation>()`를 OnEnter에서 1회 캐시; `DoDamaged→mSink->ReactDamaged`, `DoDie→mSink->ReactDied(pos)` **이미 호출**. Life **수정 불필요/금지**.
- `apps/_MyApp_/src/Spawns/SequenceContext.h` — `struct SequenceContext{ Audio::AudioSystem* audio; VFX::VFXSystem* vfx; SJH::ResourceRegistry* reg; b2World* world; SJH::Scene::Actor* sceneRoot; SJH::Scene::Actor* fxRoot; }`(전부 nullptr 기본). `Spawns/CombatSequences.h` — `SpawnEnemyDeathFX(const SequenceContext&, const vmath::vec3& worldPos)`(vec3!).
- leaf Playable 생성자: `VFX::EffekseerPlayable(ManagerRef, SJH::Effect*, vec3 spawnPos=0, TrackPolicy=Static)` / `Audio::FmodPlayable(FMOD::System*, SJH::Sound*)` / `Audio::FmodStudioPlayable(FMOD::Studio::EventDescription*, optional<vec3>=nullopt)`(explicit) / `Tween::TweenPlayable<T>(tweeny::tween<T>, function<void(T)> onStep)`.

### 내가 만든 것 (현재 라인)
- `apps/_MyApp_/src/Playable/PlayableDirector.h:25` — `class PlayableDirector : public SJH::Scene::Component, public Entity::IActorPresentation`. API: `:29 Register` / `:32 Play` / `:34 Stop` / `:35 Has` / `:38 SetSpawnContext`. 훅: `:47 ReactDamaged→Play("hit")` / `:48 ReactDied`(→.cpp) / `:49 ReactAttack→Play("attack")` / `:51 SetFacing{}` / `:52 SetPose{}` **빈 훅(Task6 자리)**. `:56 struct Slot{unique_ptr<PlayableBase> playable; bool playing=false;}`.
- `PlayableDirector.cpp:17-24 Play`=Stop()+Play()+playing=true / `:20 미등록=silent no-op` / `:39-54 Update`=playing&&!IsFinished()인 슬롯만 tick(**autoplay 방지** = 슬롯 `playing` 플래그) / `:56-62 ReactDied`=Play("death")+`if(mCtx.fxRoot)SpawnEnemyDeathFX`.
- `apps/_MyApp_/src/Playable/PostFXRegistry.h` — `static Get()`(Meyer) / `Register(passName, SJH::Material*)` / `Material(passName)→nullptr if absent`. `SJH::Material` 전방선언(헤더 경량).
- `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp:72` director 부착(루트 액터=Life sink, **AddChild 전**) / `:84 SetSpawnContext` / `:98 if(shot&&muzzle&&slashEvt)` / `:110 Register("fire", …)` / `:130 Register("damaged_test", …)` / `:136-137 SetFire/DamageCallback([director]{director->Play("fire"/"damaged_test");})`.
- `apps/_MyApp_/main.cpp:26 #include "Playable/PostFXRegistry.h"` / `:215 PostFXRegistry::Get().Register("grayscale_vignetting", FindPassMaterial(...))`.

---

## Locked decisions (재논의 금지 — 근거 포함)

1. **CMake 홈 = 신규 `MyApp::Playable` STATIC lib**(Entity 아님). 이유: PlayableDirector가 `MyApp::Spawns`(SpawnEnemyDeathFX) + `MyApp::Entity`(IActorPresentation) 둘 다 필요한데 Spawns가 이미 Entity를 PUBLIC link → Entity에 넣으면 **Entity↔Spawns 사이클**. 전용 lib가 사이클 회피. `src/CMakeLists.txt`에서 `add_subdirectory(Playable)`(Spawns/Entity 뒤) + `MyApp::Client` 우산 합류. 실행파일은 무변경(Client 경유).
2. **director는 플레이어 루트 액터에 1개 부착**(= 유일 IActorPresentation). PlayerBuilder에서 `CreatePlayerActor` 반환 직후·`AddChild` **전** 부착 → Life::OnEnter가 sink로 캐시. `CreatePlayerActor`(Entity) 안에 넣으면 Entity→Playable 사이클이라 **Bootstrap에서 부착**.
3. **`Play(key)=Stop()+Play()`(named 재사용)**. 핸드오프 명세. 단발은 원본과 동일, 원본의 per-click actor leak 제거. → **CONCERN**: 연사 시 이전 인스턴스 끊고 재생(원본은 레이어링). 의도된 트레이드오프.
4. **PostFXRegistry 등록은 `startup()`(line 215), `init()` 아님**. 이유: `mPassComponents`/`FindPassMaterial`이 `init()`(GL 플래그만)엔 미존재, `startup()`의 pass material 유효지점에만 존재. 핸드오프가 "FindPassMaterial 사용 가능 지점"으로 한정한 의도 그대로 — **정정사항**.
5. **autoplay 버그 수정**: 핸드오프의 "`!IsFinished()`인 것 tick"을 직역하면 등록만 된(=finished_ false) Composite를 frame 1에 자동재생시킨다. 슬롯별 `playing` 플래그로 Play된 것만 tick. 경험적 검증: startup 로그 `[shake]` 0건.

---

## 병렬-트랙 충돌 매트릭스 (이 브랜치는 멀티 에이전트)

| 파일/영역 | 소유 | 받는 쪽 행동 |
|---|---|---|
| `apps/_MyApp_/src/Playable/*` | **이 foundation** | hit-FX/Task6가 **소비**(director에 Register / SetFacing·SetPose 채움). PlayableDirector.h에 키 라우팅 추가는 OK |
| `PlayerBuilder.cpp` (player 조립) | foundation 편집함 | hit-FX/Task6가 추가 Register 시 **director 핸들 재사용**(`spriteActor->GetComponent<PlayableDirector>()`). 3자 공유 편집점 — surgical |
| `main.cpp` | foundation = PostFX 1줄만 | **Fog/PostFX 체인/카메라 라인 미접근**(별도 Fog/PostFX 에이전트 경합) |
| `EnemyBuilder.cpp` (적 idle tween) | **병렬 에이전트** | 미접근. 커밋서 제외 |
| `enemy_factory.h / EnemyDeathHandler / Carrier` | **Task5/7** | onDeathFx/SetOnHitFx delegate 제거 = 그쪽. 미접근 |
| `LifeComponents.h / Components.Interfaces.h` | 코어(읽기) | 수정 금지(Life는 이미 sink 호출) |
| `src/playable, src/timer, *.fs/*.frag` | 코어/셰이더 | 수정 금지 |
| `shell/CMakeExecute.sh, src/buffer/framebuffer.h, src/material/pass.h` | **병렬 에이전트**(이모지 치환) | 미접근. 커밋서 제외 |

---

## Guardrails & 컨벤션 (Step 0)

- **빌드/실행**: `cmake --preset ninja` → `cmake --build --preset ninja --target _MyApp_` → `cd build_ninja/apps/_MyApp_ && ./_MyApp_`. Debug `-Wall -Werror`(sb7 `#warnings`만 `-Wno-error`).
- **커밋 정책**: 사용자 승인 후에만. **`git add -A` 금지**(병렬 dirty 쓸려 들어감) → path-scoped. `Co-Authored-By` **미사용**(이 저장소 컨벤션).
- **테스트**: 요청 시에만(no_auto_tests). 본 작업 단위테스트 미추가.
- **코드**: 주석 한국어. 헤더가드 `__XXX_H__`(#pragma once 금지). `long` 금지(고정폭). 경로 슬래시. 네임스페이스: 게임코드 `TopdownShooter::*`(MyApp::은 CMake alias 한정), 엔진 `SJH::*`.

---

## 커밋 (승인 시) — path-scoped

```
git add -- apps/_MyApp_/src/Playable/ apps/_MyApp_/main.cpp \
  apps/_MyApp_/src/Bootstrap/CMakeLists.txt apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp \
  apps/_MyApp_/src/CMakeLists.txt
# 제외(병렬 dirty): EnemyBuilder.cpp, shell/CMakeExecute.sh, src/buffer/framebuffer.h,
#                   src/material/pass.h, doc/handoff/* (task4/pb-decomposition)
```
권장 메시지: `[feat] : PlayableDirector 연출 foundation + PostFXRegistry — onFire/onDamage 마이그레이션`

---

## 후속 / 알려진 한계

1. **fire FX 육안 미확인** — `muzzle`(distortion.efk) Create 실패(별개 VFX 에이전트, `.efk` 버전 트랩). 가드가 원본과 동일해 **회귀0은 코드 동등성으로 입증**(원본·신본 둘 다 muzzle null이면 무출력). muzzle 고쳐지면 자동 재생.
2. **SpawnContext.fxRoot = Director 루트** — PlayerBuilder가 main의 `mFxRoot` 미접근(main 1줄 제약). ReactDied 월드점 death FX가 `SweepFinishedChildren(mFxRoot)` 대상 아님. 후속: mFxRoot 주입.
3. **enemy/bullet delegate 제거 미처리** — Task5/7 조율.

---

## 검증 명령 (받는 쪽 재확인용)

```
cmake --build --preset ninja --target _MyApp_   # 기대: exit 0 (sb7 #warnings만)
cd build_ninja/apps/_MyApp_ && ./_MyApp_         # 기대: 무크래시, enemy wave 스폰, [error] 1건=muzzle뿐
# 좌클릭/G키는 GUI 상호작용 — 사람 육안. G키 누르면 stdout에 [shake] v=… 로그.
```

---

## Verbatim recovery — at-risk 미커밋 코드 (PlayerBuilder.cpp 는 tracked-M → `git checkout` 시 소실)

> 신규 `Playable/*` 5파일은 untracked라 `git checkout`엔 안전(`git clean -fd`엔 소실). 아래는 reset 시 복구용 핵심 블록.

**PlayerBuilder.cpp — `CreatePlayerActor` 반환 직후, `AddChild`(line ~155) 전 삽입 블록:**
```cpp
auto *director = spriteActor->AddComponent<TopdownShooter::Playable::PlayableDirector>();
{   // 월드점 FX ctx (fxRoot=Director 루트 — main mFxRoot 미접근, 후속)
    TopdownShooter::Spawns::SequenceContext fxCtx;
    fxCtx.audio=&TopdownShooter::Manager::Get().Audio(); fxCtx.vfx=&TopdownShooter::Manager::Get().VFX();
    fxCtx.reg=&SJH::ResourceRegistry::Get(); fxCtx.world=deps.physicsWorld;
    fxCtx.sceneRoot=&dir.Root(); fxCtx.fxRoot=&dir.Root();
    director->SetSpawnContext(fxCtx);
}
{   // "fire" = Sequence( Effekseer muzzle → Parallel( Fmod.shot ∥ FmodStudio.Slash ) )
    auto &reg=SJH::ResourceRegistry::Get(); auto &audio=TopdownShooter::Manager::Get().Audio();
    auto &vfx=TopdownShooter::Manager::Get().VFX();
    auto *shot=reg.FindSound("shot"); auto *muzzle=reg.FindEffect("muzzle"); auto *slashEvt=audio.LoadEvent("event:/Slash");
    if (shot && muzzle && slashEvt) {
        auto seq=std::make_unique<SJH::Playable::SequencePlayable>();
        seq->Append(std::make_unique<TopdownShooter::VFX::EffekseerPlayable>(vfx.GetManager(), muzzle, vmath::vec3(0.0f), TopdownShooter::VFX::TrackPolicy::Static));
        auto par=std::make_unique<SJH::Playable::ParallelPlayable>();
        par->Join(std::make_unique<TopdownShooter::Audio::FmodPlayable>(audio.GetSystem(), shot));
        par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(slashEvt));
        seq->Append(std::move(par));
        director->Register("fire", std::move(seq));
    }
}
{   // "damaged_test" = Parallel( TweenShake ∥ (if) FmodStudio.Damaged )
    auto &audio=TopdownShooter::Manager::Get().Audio(); auto *damagedEvt=audio.LoadEvent("event:/Damaged");
    auto par=std::make_unique<SJH::Playable::ParallelPlayable>();
    auto tween=tweeny::from(0.0f).to(1.0f).during(100).via(tweeny::easing::sinusoidalInOut);
    par->Join(std::make_unique<TopdownShooter::Tween::TweenPlayable<float>>(std::move(tween),
        [](float v){ float o=std::sin(v*8.0f*3.14159f)*5.0f; spdlog::info("[shake] v={:.3f} offset={:.3f}", v, o); }));
    if (damagedEvt) par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(damagedEvt));
    director->Register("damaged_test", std::move(par));
}
if (auto *controller = spriteActor->GetComponent<Controller::PlayerController>()) {
    controller->SetFireCallback([director]{ director->Play("fire"); });
    controller->SetDamageCallback([director]{ director->Play("damaged_test"); });
}
```
**main.cpp**: `#include "Playable/PostFXRegistry.h"`(line 26) + `startup()` 의 uVignetteColor 블록 직후 `TopdownShooter::Playable::PostFXRegistry::Get().Register("grayscale_vignetting", FindPassMaterial("grayscale_vignetting"));`(line 215).

---

## Pointers

- 원본 입력 프롬프트: [`2026-06-02-playable-director-foundation-agent-prompt.md`](2026-06-02-playable-director-foundation-agent-prompt.md) (✅ 실행 완료 — 이 문서가 결과).
- 정본 spec(선택 심화): `doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md`, `2026-05-26-playable-component-interface-design.md`(IPlayable).
- 다음 슬라이스 프롬프트(Task6): [`2026-06-02-task6-on-director-foundation-agent-prompt.md`](2026-06-02-task6-on-director-foundation-agent-prompt.md).
- 메모리: `playable_director_foundation.md` (병렬 트랙 key 규약 + 후속 요약).

---

## Change log
- 2026-06-02 — 작성. foundation 구현 완료, 빌드/런타임/리뷰 검증, 커밋 미승인. HEAD `d44a4a3`.
