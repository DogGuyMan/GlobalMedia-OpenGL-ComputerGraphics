# M5 Leaf Playable 통합 설계 (Effekseer + FMOD Core/Studio + Tweeny)

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **작성일**: 2026-05-26
> **브랜치**: `game/module/rendertarget`
> **관련 spec**:
> - [`2026-05-26-playable-component-interface-design.md`](2026-05-26-playable-component-interface-design.md) — IPlayable / PlayableBase / Composite 정본 (M3.5)
> - [`2026-05-24-topdown-shooter-design.md`](2026-05-24-topdown-shooter-design.md) §5 (Effekseer) / §7 (Tweeny) / §8 (FMOD)
> **인수인계**: [`doc/handoffs/2026-05-26/2026-05-26-M5-handoff.md`](../../../doc/design/2026-05-26-M5-handoff.md)
> **진행 보고서**: [`doc/topdown-shooter-progress.md`](../../../doc/topdown-shooter-progress.md) M5
> **결정 횟수**: 7건 (Phase 1~6 + Phase 6-tris 컨벤션)

---

## §0 Goal

M3.5 에서 정착된 `IPlayable` / `PlayableBase` / Composite 위에, **세 가지 외부 라이브러리 (FMOD Core·Studio / Effekseer / Tweeny) 를 wrap 하는 leaf Playable 4종** 을 *Client 거주* 로 도입한다. 동시에 `_MyApp_` 의 game-runtime subsystem 들 (Physics + Audio + VFX) 을 `TopdownShooter::Director` 라는 Client-side 싱글톤 한 곳으로 집계해 `main.cpp` 비대화를 막는다.

본 spec 은 *그 도입* 의 6 결정 + 구체 시그니처 + 시각 검증 시나리오 + 구현 순서를 정한다.

**비고 — Engine 코어 변경 범위 확장 (Phase 2 결정의 함의):**
M5 는 원래 *Engine 코어 변경 0* 으로 계획되었으나, Phase 2 결정 ("ResourceRegistry::CreateSound/CreateEffect 를 Engine 에 추가") 으로 인해 `src/resource_registry/` 가 game_deps 에 PUBLIC link 된다. 즉 **`SJH::engine` 우산을 link 하는 모든 consumer (test 포함) 가 FMOD/Effekseer 를 자동 합류**. 이는 의도된 architecture shift (resource caching 중앙화) 이며 §5.1 + §6.1 에 위험과 후속 작업 명시.

---

## §1 핵심 결정 (6건)

| # | Phase | 항목 | 결정 |
|---|---|---|---|
| 1 | Phase 1 | **subsystem 보유 위치** | **Client 전용 `TopdownShooter::Director`** 신설 (`apps/_MyApp_/src/Director.{h,cpp}`). PhysicsSystem (기존 `mPhysics`) + AudioSystem + VFXSystem 3 멤버 보유 + Init/Update/Shutdown 일괄. `SJH::Scene::Director` (engine) 는 변경 0 — 이름 충돌이지만 namespace 분리로 충돌 0. |
| 2 | Phase 2 | **자원 캐시 위치** | **Engine `SJH::ResourceRegistry`** 에 `CreateSound` / `CreateEffect` 신설. `src/resource_registry/sound.{h,cpp}` + `src/resource_registry/effect.{h,cpp}` 추가. `src/resource_registry/CMakeLists.txt` 에 `target_link_libraries(... PUBLIC game_deps)` 한 줄 추가 — ⚠ 모든 engine consumer 가 game_deps 자동 합류. |
| 3 | Phase 3 | **FmodPlayable.Stop fade** | **즉시 stop** (no fade). `channel_->stop()` + `channel_ = nullptr`. Studio 측도 동일 (`instance_->stop(FMOD_STUDIO_STOP_IMMEDIATE)`). 향후 fade 가 필요해지면 별도 마일스톤. |
| 4 | Phase 4 | **EffekseerPlayable 3D 추적** | **ctor enum `TrackPolicy { Static, FollowOwner }`** 으로 선택. `Static` (default) = OnPlay 시 `spawnPos_` 로 1회 `SetLocation`. `FollowOwner` = OnUpdate 매 frame `owner->Transform.Translate` 읽어 `manager_->SetLocation(handle_, ...)` 호출. |
| 5 | Phase 5 | **TweenPlayable 도입 시기** | **본 M5 에 포함** — skeleton 의 template ctor + OnUpdate 채움. `int32_t dtMs = static_cast<int32_t>(dt * 1000.0f); tween_.step(dtMs);` 명시. 키 G 입력 시 카메라 shake 로 다형 검증. (memory `tweeny_step_overload_trap` 강제 준수) |
| 6 | Phase 6 | **시각 검증 리소스** | demo 에서 직접 복사 — Master.bank + Master.strings.bank (이미 복사됨 — `apps/_MyApp_/resources/banks/`), Laser.wav (effekseer_demo/demo1 → `apps/_MyApp_/resources/audio/`), distortion.efk (effekseer_demo/demo1 → `apps/_MyApp_/resources/vfx/`). |
| 7 | Phase 6-tris | **Client 파일명 컨벤션** | **PascalCase (`XxxxYyyy.h`)** — Algebraic/Entity/InputHandler/Stage 다수 컨벤션 따름. M5 가 만지는 모든 파일 (skeleton 4 + 신설 3 subsystem + Director) 에 적용. Engine (`src/`) 은 기존 snake_case 유지 — 무관. 기존 Physics/* snake_case 는 §6.6 cleanup 으로 분리. |

---

## §2 모듈 레이아웃 변경

### §2.1 Engine 측 — `src/resource_registry/` 확장

```
src/resource_registry/
├─ CMakeLists.txt                  # ⚠ target_link_libraries(... PUBLIC game_deps) 한 줄 추가
├─ resource_registry.h             # CreateSound / CreateEffect 2 메소드 + 캐시 멤버 추가
├─ resource_registry.cpp           # 위 2 메소드 구현 (path-key unordered_map cache)
├─ sound.h                         # (신설) SJH::Sound — FMOD::Sound* opaque wrap (Release in dtor)
├─ sound.cpp                       # (신설)
├─ effect.h                        # (신설) SJH::Effect — Effekseer::EffectRef opaque wrap
├─ effect.cpp                      # (신설)
├─ image.{h,cpp}                   # (기존 — 변경 0)
├─ texture.{h,cpp}                 # (기존 — 변경 0)
├─ material.{h,cpp}                # (기존 — 변경 0)
└─ model.{h,cpp}                   # (기존 — 변경 0)
```

### §2.2 Client 측 — `apps/_MyApp_/src/` 신설/확장

**파일명 컨벤션**: Client 측은 **PascalCase (`XxxxYyyy.h`)** 통일 (저장소 다수 컨벤션 — Algebraic/Entity/InputHandler/Stage). M5 가 만지는 모든 파일은 이 컨벤션. M5 skeleton 의 snake_case (`fmod_playable.h` 등) 는 본 spec 시행 시점에 *함께 rename*. Engine 측 (`src/`) 은 기존 snake_case 유지 — 무관.

```
apps/_MyApp_/src/
├─ CMakeLists.txt                  # MyApp::Client 우산에 Director 합류
├─ Director.h                      # (신설) TopdownShooter::Director — Physics+Audio+VFX 집계 싱글톤
├─ Director.cpp                    # (신설)
│
├─ Audio/
│   ├─ CMakeLists.txt              # MyApp::Audio STATIC, game_deps + SJH::engine PRIVATE link
│   ├─ AudioSystem.h               # (신설) AudioSystem — FMOD::System + Studio::System owner + EventDescription 캐시
│   ├─ AudioSystem.cpp             # (신설)
│   ├─ FmodPlayable.{h,cpp}        # (skeleton rename + 활성화) Core .wav 재생 — Pause override 필수
│   │                              #   ← 기존 fmod_playable.{h,cpp} 에서 rename
│   └─ FmodStudioPlayable.{h,cpp}  # (skeleton rename + 활성화) Studio event 재생
│                                  #   ← 기존 fmod_studio_playable.{h,cpp} 에서 rename
│
├─ VFX/
│   ├─ CMakeLists.txt              # MyApp::VFX STATIC
│   ├─ VFXSystem.h                 # (신설) VFXSystem — Effekseer::Manager + EffekseerRendererGL owner
│   ├─ VFXSystem.cpp               # (신설)
│   └─ EffekseerPlayable.{h,cpp}   # (skeleton rename + 활성화) TrackPolicy enum 추가
│                                  #   ← 기존 effekseer_playable.{h,cpp} 에서 rename
│
├─ Tween/
│   ├─ CMakeLists.txt              # MyApp::Tween INTERFACE (header-only template)
│   └─ TweenPlayable.h             # (skeleton rename + 활성화) tweeny<T> + step(int32 ms)
│                                  #   ← 기존 tween_playable.h 에서 rename
│
└─ Physics/                         # (기존 — 코드 변경 0)
                                    # ※ 내부 snake_case (physics_system.{h,cpp}, contact_listener.*,
                                    #   pickup_factory.h, wall_factory.h) 의 PascalCase rename 은
                                    #   본 M5 범위 외 — §6.6 cleanup 항목으로 분리
```

---

## §3 공개 시그니처

### §3.1 Engine — `SJH::ResourceRegistry` 확장

```cpp
// src/resource_registry/sound.h
namespace SJH {
    class Sound {
      public:
        explicit Sound(::FMOD::Sound* raw) : raw_(raw) {}
        ~Sound();                            // raw_->release()
        Sound(const Sound&)            = delete;
        Sound& operator=(const Sound&) = delete;
        ::FMOD::Sound* Raw() const { return raw_; }
      private:
        ::FMOD::Sound* raw_ = nullptr;
    };
}

// src/resource_registry/effect.h
namespace SJH {
    class Effect {
      public:
        explicit Effect(Effekseer::EffectRef ref) : ref_(ref) {}
        ~Effect() = default;                 // EffectRef = shared_ptr 류 — auto release
        Effect(const Effect&)            = delete;
        Effect& operator=(const Effect&) = delete;
        Effekseer::EffectRef Ref() const { return ref_; }
      private:
        Effekseer::EffectRef ref_;
    };
}

// src/resource_registry/resource_registry.h (추가)
class ResourceRegistry {
  public:
    // 기존 Image/Texture/Material/Model + 신설 Sound/Effect
    Sound*  CreateSound (::FMOD::System* sys, const std::string& path);     // path-key cache
    Effect* CreateEffect(::Effekseer::ManagerRef mgr, const std::u16string& path);
    //                                                ^^^^^^^^^^^^^^^^^^^^^^^ Effekseer 는 utf-16 path 표준
  private:
    std::unordered_map<std::string,     std::unique_ptr<Sound>>  soundCache_;
    std::unordered_map<std::u16string,  std::unique_ptr<Effect>> effectCache_;
};
```

### §3.2 Client — `TopdownShooter::Director`

```cpp
// <apps>/_MyApp_/src/Director.h
namespace TopdownShooter {
class Director {
  public:
    static Director& Get();
    void Init();                  // Audio.Init + VFX.Init + Physics.Init 순
    void Update(float dt);        // Audio.Update + VFX.Update + Physics.Step
    void Shutdown();              // 역순 release

    Audio::AudioSystem&   Audio()   { return audio_; }
    VFX::VFXSystem&       VFX()     { return vfx_;   }
    Physics::PhysicsSystem& Physics() { return phys_; }

    Director(const Director&)            = delete;
    Director& operator=(const Director&) = delete;
  private:
    Director() = default;
    ~Director() = default;
    Audio::AudioSystem    audio_;
    VFX::VFXSystem        vfx_;
    Physics::PhysicsSystem phys_;
};
}
```

### §3.3 Client — `Audio::AudioSystem`

```cpp
// apps/_MyApp_/src/Audio/AudioSystem.h
namespace TopdownShooter::Audio {
class AudioSystem {
  public:
    void Init();                  // FMOD::System::create + initialize + Studio::System::create + initialize
    void Update(float dt);        // studio_->update() (Core 도 함께 진행)
    void Shutdown();              // studio_->release() + system_->release()

    ::FMOD::System*          GetSystem()       { return system_; }
    ::FMOD::Studio::System*  GetStudioSystem() { return studio_; }

    // bank 로드 (path = "resources/banks/Master.bank") + EventDescription 캐시
    void LoadBank(const std::string& path);
    ::FMOD::Studio::EventDescription* LoadEvent(const std::string& eventPath);

  private:
    ::FMOD::System*          system_  = nullptr;
    ::FMOD::Studio::System*  studio_  = nullptr;
    std::vector<::FMOD::Studio::Bank*> banks_;
    std::unordered_map<std::string, ::FMOD::Studio::EventDescription*> eventCache_;
};
}
```

### §3.4 Client — `Audio::FmodPlayable` (Core, T1)

```cpp
namespace TopdownShooter::Audio {
class FmodPlayable : public SJH::Playable::PlayableBase {
  public:
    FmodPlayable(::FMOD::System* sys, SJH::Sound* sound);
    ~FmodPlayable() override;

    void Pause() override;        // base.Pause + channel_->setPaused(true)
  protected:
    void OnPlay() override;       // sys_->playSound(sound_->Raw(), nullptr, false, &channel_)
    void OnStop() override;       // if (channel_) channel_->stop(); channel_ = nullptr
    void OnUpdate(float dt) override;   // isPlaying false + !isLoop_ → finished_ = true
  private:
    ::FMOD::System*  sys_     = nullptr;   // 외부 owner (AudioSystem)
    SJH::Sound*      sound_   = nullptr;   // 외부 owner (ResourceRegistry cache)
    ::FMOD::Channel* channel_ = nullptr;   // 자체 owner (Play 시 받음)
};
}
```

### §3.5 Client — `Audio::FmodStudioPlayable` (T3)

```cpp
namespace TopdownShooter::Audio {
class FmodStudioPlayable : public SJH::Playable::PlayableBase {
  public:
    explicit FmodStudioPlayable(::FMOD::Studio::EventDescription* desc);
    ~FmodStudioPlayable() override;

    void Pause() override;        // base.Pause + instance_->setPaused(true)
  protected:
    void OnPlay() override;       // desc_->createInstance(&instance_); instance_->start();
    void OnStop() override;       // instance_->stop(FMOD_STUDIO_STOP_IMMEDIATE); instance_->release(); = nullptr
    void OnUpdate(float dt) override;   // getPlaybackState == STOPPED + !isLoop_ → finished_ = true
  private:
    ::FMOD::Studio::EventDescription* desc_     = nullptr;   // 외부 owner (AudioSystem 캐시)
    ::FMOD::Studio::EventInstance*    instance_ = nullptr;   // 자체 owner
};
}
```

### §3.6 Client — `VFX::VFXSystem`

```cpp
// apps/_MyApp_/src/VFX/VFXSystem.h
namespace TopdownShooter::VFX {
class VFXSystem {
  public:
    void Init(int maxSprites = 8000);
    void Update(float dt);        // manager_->Update(dt * 60.0f) — Effekseer 기준 frame
    void Draw(const float* viewMat, const float* projMat);   // main.render() 안에서 호출
    void Shutdown();

    Effekseer::ManagerRef           GetManager()  { return manager_; }
    EffekseerRendererGL::RendererRef GetRenderer() { return renderer_; }
  private:
    EffekseerRendererGL::RendererRef renderer_;
    Effekseer::ManagerRef            manager_;
};
}
```

### §3.7 Client — `VFX::EffekseerPlayable` (T2)

```cpp
namespace TopdownShooter::VFX {
enum class TrackPolicy { Static, FollowOwner };

class EffekseerPlayable : public SJH::Playable::PlayableBase {
  public:
    EffekseerPlayable(Effekseer::ManagerRef manager,
                      SJH::Effect*          effect,
                      const vmath::vec3&    spawnPos = vmath::vec3(0.0f),
                      TrackPolicy           track    = TrackPolicy::Static);
    ~EffekseerPlayable() override;

  protected:
    void OnPlay() override;       // handle_ = manager_->Play(effect_->Ref(), spawnPos as EfkVec3)
    void OnStop() override;       // if (handle valid) manager_->StopEffect(handle_); handle_ = invalid
    void OnUpdate(float dt) override;
        // (1) FollowOwner 면 GetOwner()->Transform.Translate → manager_->SetLocation(handle_, ...)
        // (2) !manager_->Exists(handle_) && !isLoop_ → finished_ = true
  private:
    Effekseer::ManagerRef manager_;
    SJH::Effect*          effect_   = nullptr;
    int                   handle_   = -1;          // Effekseer::Handle == int, -1=invalid
    vmath::vec3           spawnPos_{0.0f};
    TrackPolicy           track_    = TrackPolicy::Static;
};
}
```

### §3.8 Client — `Tween::TweenPlayable<T>` (T4)

```cpp
// apps/_MyApp_/src/Tween/TweenPlayable.h
#include <<tweeny>/tweeny.h>

namespace TopdownShooter::Tween {
template <typename T>
class TweenPlayable : public SJH::Playable::PlayableBase {
  public:
    TweenPlayable(tweeny::tween<T> tween, std::function<void(T)> onStep)
        : tween_(std::move(tween)), onStep_(std::move(onStep)) {}
    ~TweenPlayable() override = default;

  protected:
    void OnUpdate(float dt) override {
        // ⚠ memory tweeny_step_overload_trap — step(float) 에 dt(초) 넘기면 폭주.
        //    int32_t (ms) 오버로드 강제 사용.
        int32_t dtMs = static_cast<int32_t>(dt * 1000.0f);
        T value = tween_.step(dtMs);
        if (onStep_) onStep_(value);
        if (tween_.progress() >= 1.0f && !isLoop_) finished_ = true;
    }
  private:
    tweeny::tween<T>       tween_;
    std::function<void(T)> onStep_;
};
}
```

---

## §4 시각 검증 시나리오 (T5 / M3.5 잔여 흡수)

| 트리거 | 부착 Playable | 검증 항목 |
|---|---|---|
| `_MyApp_::startup()` | `actor->AddComponent<FmodStudioPlayable>(audio.LoadEvent("event:/BGM"))` + `SetIsLoop(true).Play()` | BGM 무한 loop 재생, shutdown 시 정상 정지 (release count 0) |
| `onMouseButton(LEFT, PRESS)` | `Sequence().Append(EffekseerPlayable(vfx.GetManager(), reg.CreateEffect(mgr, u"resources/vfx/distortion.efk"), playerPos, Static)).Append(Parallel().Join(FmodPlayable(audio.GetSystem(), reg.CreateSound(sys, "resources/audio/Laser.wav"))).Join(FmodStudioPlayable(audio.LoadEvent("event:/Slash"))))` | muzzle(VFX) → 발사음(Core .wav) + Slash(Studio one-shot) 동시. Sequence + Parallel 계층 중첩 시각 검증 |
| `onKey(G, PRESS)` | `Sequence().Append(Parallel().Join(TweenPlayable<float>(tweeny::from(0).to(1).during(100).via(tweeny::easing::sinusoidalInOut), [&](float v){ camera.shakeOffset.x = std::sin(v*8*M_PI)*5.0f; })).Join(FmodStudioPlayable(audio.LoadEvent("event:/Damaged"))))` | 카메라 shake 100ms + Damaged 동시. TweenPlayable 다형 동작 + finished_ 종료 검증 |
| `_MyApp_::shutdown()` | (Director.Shutdown → 역순 Audio.Shutdown / VFX.Shutdown / Physics.Shutdown) | FMOD/Effekseer dll 정상 release, macOS leaks 시 누수 0 |

**검증 항목 체크리스트** (사용자 확인 시점):
- [ ] BGM event:/BGM 이 loop=true 로 영원 재생 + 종료 시 정지
- [ ] 마우스 좌클릭 시 muzzle + Laser.wav + Slash 동시 발생 (Parallel)
- [ ] muzzle → 발사음 의 순서 (Sequence) 가 시각/청각으로 분리됨
- [ ] G 키 시 카메라 100ms shake + Damaged 동시
- [ ] Pause 호출 후 Play 시 같은 위치 재개. Stop 후 Play 시 처음부터 (PlayableBase 정의)
- [ ] 종료 시 FMOD dll / Effekseer dll release count 0 (leaks)

---

## §5 구현 순서 (의존 그래프)

```
[Engine 단계]
SP1. src/resource_registry/CMakeLists.txt 에 game_deps PUBLIC link 추가
SP2. src/resource_registry/sound.{h,cpp} + effect.{h,cpp} 신설
SP3. src/resource_registry/resource_registry.{h,cpp} 에 CreateSound/CreateEffect 추가
SP4. 검증: cmake --build --preset ninja --target _MyApp_  → 전체 정상 통과

[Client 단계 — Rename + Subsystem]
RN1. PascalCase rename (단순 git mv + #include 경로 수정):
       fmod_playable.{h,cpp}        → FmodPlayable.{h,cpp}
       fmod_studio_playable.{h,cpp} → FmodStudioPlayable.{h,cpp}
       effekseer_playable.{h,cpp}   → EffekseerPlayable.{h,cpp}
       tween_playable.h             → TweenPlayable.h
       + Audio/CMakeLists.txt, VFX/CMakeLists.txt, Tween/CMakeLists.txt 의 add_library 소스 목록 갱신
       + 헤더 가드 매크로 _TOPDOWNSHOOTER_*_FMOD_PLAYABLE_H__ 등도 동시 갱신 권장
CL1. apps/_MyApp_/src/Audio/AudioSystem.{h,cpp} 신설 (FMOD Core+Studio init/update/release + LoadBank/LoadEvent)
CL2. apps/_MyApp_/src/VFX/VFXSystem.{h,cpp} 신설 (Effekseer Manager+Renderer init/update/draw/release)
CL3. apps/_MyApp_/src/Director.{h,cpp} 신설 (Audio+VFX+Physics 집계 + Init/Update/Shutdown)

[Client 단계 — Leaf Playable 활성화]
LP1. T3 FmodStudioPlayable.{h,cpp} 활성화 + main.cpp startup 에 BGM 부착 → 빌드/실행 검증
LP2. T1 FmodPlayable.{h,cpp} 활성화 + main.cpp onMouseButton 에 Laser 부착 → 검증
LP3. T2 EffekseerPlayable.{h,cpp} 활성화 (TrackPolicy enum 포함) + main.cpp onMouseButton 에 distortion 부착 → 검증
LP4. T4 TweenPlayable.h 활성화 (template) + main.cpp onKey 에 카메라 shake 부착 → 검증

[Composite 통합]
CO1. main.cpp onMouseButton 을 Sequence(Effekseer → Parallel(FmodCore + FmodStudio)) 로 교체 → 검증
CO2. main.cpp onKey(G) 를 Parallel(TweenShake + FmodStudio Damaged) 로 교체 → 검증

[POST_BUILD 자원 복사 확인]
PB1. apps/_MyApp_/CMakeLists.txt 의 resources/ POST_BUILD copy 가 banks/audio/vfx/ 하위 모두 잡는지 확인
PB2. cd build_ninja/apps/_MyApp_ && ls resources/{banks,audio,vfx} — 모두 정상

[정리]
END1. doc/topdown-shooter-progress.md M5 섹션 업데이트 (commits + 검증 항목 체크)
END2. .claude/CLAUDE.md 의 SJH::ResourceRegistry 캐싱 항목 갱신 (Sound/Effect 추가 명시)
END3. memory MEMORY.md 갱신 (필요 시 — 예: "ResourceRegistry game_deps 합류")
```

---

## §6 리스크 & 후속 작업

### §6.1 game_deps 가 Engine 으로 누출 (Phase 2 함의)

**현상**: `src/resource_registry/CMakeLists.txt` 에 `target_link_libraries(... PUBLIC game_deps)` 추가 시 `SJH::resource_registry` 의 모든 consumer (= `SJH::engine` 우산 link 하는 모든 데모/테스트) 가 FMOD/Effekseer/Box2D/...를 자동 합류한다.

**영향**:
- `migrate_demo`, `audio_demo` 같은 _MyApp_ 외 데모들도 game_deps 합류
- `test/` 활성 21개 단위 테스트 + `test_smoke` 도 game_deps 합류
- macOS 의 경우 POST_BUILD 의 `$<TARGET_FILE:fmod>` copy 가 모든 데모/테스트 실행 파일 옆에 dll 복사 필요 가능성

**완화**:
- 활성 데모만 영향 받음 (apps/CMakeLists.txt 의 주석 해제 3종 = `_MyApp_`/`migrate_demo`/`audio_demo`)
- `migrate_demo` 는 game_deps 가 추가되어도 link 만 되고 사용 안 함 — 빌드 OK
- `audio_demo` 는 이미 game_deps 사용 중 — 무영향
- **test wiring 확인 필요** — `test/CMakeLists.txt` 의 각 `test_*` 가 `SJH::engine` 을 link 한다면 FMOD/Effekseer 헤더 추가 합류. 컴파일 영향 측정 필요 (SP4 단계에서)

**후속 작업** (M5 완료 시 별도 commit):
- `.claude/CLAUDE.md` 의 *§Src 모듈 레이아웃* 표에서 `SJH::resource_registry` 의 *PUBLIC 의존* 행에 `+ game_deps` 명시
- *§자원 보유 컨벤션* 섹션에 "Sound/Effect 도 ResourceRegistry 캐시 지원" 추가

### §6.2 `SJH::Scene::Director` ↔ `TopdownShooter::Director` 이름 충돌

namespace 분리로 *컴파일 충돌 0* 이지만 *대화/문서 시 혼동* 위험. spec 첫 페이지 + Director.h doc-comment 에 "engine 의 동명 클래스와 무관 — Cocos cc::Director 정통 *별도 인스턴스*" 명시.

### §6.3 Effekseer Renderer 의 draw 호출 위치

`VFXSystem::Update(dt)` 는 main 의 update 단계, `VFXSystem::Draw(view, proj)` 는 main 의 render 단계 (불투명 메시 그린 후, post-process 전) 에 호출. apps/_MyApp_/main.cpp 의 render() 안에 한 줄 추가 필요. (현재 main.cpp 가 SceneRenderer.Draw 후 다른 호출 없음 → 자연 위치)

### §6.4 utf-8 / utf-16 path 경계

Effekseer 는 path 가 `char16_t*` (utf-16) 필수 — `u"resources/vfx/distortion.efk"` 식 리터럴 사용. ResourceRegistry::CreateEffect 의 시그니처가 `const std::u16string&` 이라 호출자가 변환 책임. 자동 변환 helper 추가는 후속 (필요 시 `ToU16(const std::string&)` 자유 함수).

### §6.5 FMOD Studio bank 부재 시 graceful fallback (out of scope)

Master.bank 가 없으면 `getEvent` 가 nullptr 반환 → AudioSystem::LoadEvent 에서 spdlog::warn + nullptr 반환. FmodStudioPlayable 은 nullptr desc 로 생성되면 OnPlay no-op (paused_=true 유지). 이는 M5 의 명시 검증 항목 *아님* — spec 명시만 하고 robust 검증은 M6+ 로.

### §6.6 Physics/* snake_case rename (out of scope — 후속 cleanup)

`apps/_MyApp_/src/Physics/` 안에 snake_case 파일이 남아 있다 — `physics_system.{h,cpp}`, `contact_listener.{h,cpp}`, `filter.h`, `physics_movement.h`, `pickup_factory.h`, `wall_factory.h`. Stage/Factories/ 안에도 `pickup_factory.h` / `wall_factory.h`. 본 M5 는 PhysicsSystem 을 *Director 멤버로 이동만* 하므로 내부 파일명은 변경 0. 일관된 Client PascalCase 통일을 위해 *별도 cleanup commit* 권장:

```
physics_system.{h,cpp}   → PhysicsSystem.{h,cpp}
contact_listener.{h,cpp} → ContactListener.{h,cpp}
physics_movement.h       → PhysicsMovement.h
filter.h                 → Filter.h     (또는 PhysicsFilter.h — 의미 명확화)
pickup_factory.h         → PickupFactory.h
wall_factory.h           → WallFactory.h
```

M5 완료 후 별도 PR/commit 으로 분리 — 본 spec 의 의존 그래프에 영향 0.

---

## §7 정의되지 않은 것 (Out of Scope)

| 항목 | 이유 |
|---|---|
| FMOD fade / DSP / 3D positional audio | Phase 3 결정 = 즉시 stop. 3D audio 는 탑다운 카메라 + 2D 게임 plane 이라 정통 mono mix 로 충분 |
| Effekseer 의 2D vs 3D mode 분기 | _MyApp_ 는 3D camera (탑다운) → Effekseer 의 3D 기본 mode 그대로 사용 |
| TweenPlayable 의 loop / yoyo / sequence-of-tweens | tweeny 자체가 fluent 라 PlayableBase.isLoop_ 만 노출. yoyo 는 사용처 등장 시 별도 |
| AudioSystem 의 volume bus / mixer | Studio 의 VCA/Bus 는 향후 작업. 현재 = 이벤트 단위 setVolume 만 |
| VFXSystem 의 culling / max effects 동적 조정 | Init(maxSprites=8000) 고정. 성능 이슈 등장 시 별도 |
| Bullet ECS 연동 / FmodStudio 의 dynamic param | M4 (PlayerStateMachine) + 본 M5 가 통합되는 시점 별도 |
| 자동 ToU16 helper | utf-8↔16 변환 사용처 1군데 (Effekseer path) — 직접 리터럴 (u"...") 권장 |

---

## §8 검증 명령 (인수자가 실행)

```bash
# Engine + Client 전체 빌드
cmake --build --preset ninja --target _MyApp_

# 실행 (리소스 상대경로 때문에 cd 필수)
cd build_ninja/apps/_MyApp_ && ./_MyApp_

# clean + 빌드 + 실행 (shell helper)
sh <shell>/CMakeALL.sh debug _MyApp_

# macOS leaks 검증 — FMOD/Effekseer dll release count 검증
sh <shell>/CMakeExecute.sh debug _MyApp_ leaks

# 테스트가 game_deps 합류로 영향 받는지 검증 (SP4)
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target tests
ctest --test-dir build_ninja --output-on-failure
```

---

**spec end.**
