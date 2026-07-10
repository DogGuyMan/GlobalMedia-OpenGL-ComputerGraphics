# Effekseer API — 학습 / 인수인계 노트

> **대상 독자** = 본 프로젝트에 합류하는 다른 Claude Code Agent + 개발자.
> 이 저장소에서 Effekseer(파티클 엔진)를 어떻게 쓰는지, *검증된 실사용 호출* 만 모아 정리한다.
> `doc/api/Box2DAPI.md` · `doc/api/FMODAPI.md` 와 동일 성격의 *실사용 API 레퍼런스*.

**버전: Effekseer 1.7.3.0** (`extern/Effekseer`, `include/Effekseer.h` + `include/EffekseerRendererGL.h`).
심화 시그니처는 [Context7 `/effekseer/effekseer`](https://github.com/effekseer/effekseer/blob/master/doc/Help_Cpp/18x/Guide_Cpp_En.md) (공식 C++ Guide) 로 교차 검증함.

발췌 출처 (이 노트의 모든 코드):
- [apps/effekseer_demo/demo1/main.cpp](../apps/effekseer_demo/demo1/main.cpp) — 저수준 직접 사용 (Manager/Renderer 를 `main.cpp` 가 직접 보유). *교과서적 1 파일 데모.*
- [apps/_MyApp_/src/VFX/VFXSystem.h](../apps/_MyApp_/src/VFX/VFXSystem.h) · [.cpp](../apps/_MyApp_/src/VFX/VFXSystem.cpp) — Manager+Renderer **owner** 시스템 (Director 멤버 거주).
- [apps/_MyApp_/src/VFX/EffekseerPlayable.h](../apps/_MyApp_/src/VFX/EffekseerPlayable.h) · [.cpp](../apps/_MyApp_/src/VFX/EffekseerPlayable.cpp) — Effect 1개의 **재생 lifecycle** 을 감싼 leaf Playable (M5).
- [src/resource_registry/effect.h](../src/resource_registry/effect.h) — `Effekseer::EffectRef` 를 캐시 entry 로 보유하는 `SJH::Effect` wrap.

---

## 0. 큰 그림 — 책임 3분할 (이 저장소의 핵심 통합 패턴)

게임(`_MyApp_`) 은 Effekseer 를 **세 객체로 쪼개** 쓴다. 이게 데모(`demo1`) 의 1 파일 묶음과 가장 큰 차이이자 인수인계의 요점이다.

| 객체 | 보유물 | 책임 | 거주 |
|---|---|---|---|
| **`VFXSystem`** | `Effekseer::ManagerRef` + `EffekseerRendererGL::RendererRef` | 엔진 부트스트랩 / 매 프레임 `Update` / `Draw`. 파티클 1개당이 아니라 *전체 1개* | Director 멤버 (싱글턴급) |
| **`SJH::Effect`** | `Effekseer::EffectRef` | `.efkefc` 에셋 1개를 로드해 캐시. 재사용 가능한 *틀* | `SJH::ResourceRegistry` |
| **`EffekseerPlayable`** | `ManagerRef`(빌림) + `Effect*`(빌림) + `Effekseer::Handle` | 에셋 1개의 *한 번의 재생 인스턴스* lifecycle (Play→추적→정지) | Actor 의 Component |

> **관계**: `VFXSystem` 이 만든 `ManagerRef` 를, `EffekseerPlayable` 이 ctor 로 **빌려서**(소유 안 함) `Effect` 캐시의 `EffectRef` 를 `manager->Play(...)` 에 넘긴다.
> Cocos `ParticleSystem` 정통 — *엔진/에셋/인스턴스* 의 3-tier 분리.

```
ResourceRegistry ──CreateEffect──▶ SJH::Effect(EffectRef)   ┐
                                                            ├─▶ EffekseerPlayable.Play()
VFXSystem ──Manager::Create──────▶ Effekseer::ManagerRef ───┘        │
       │                                                            manager->Play(effect->Ref(), pos) → Handle
       └─ Update(dt)/Draw(view,proj)  ◀── 매 프레임 main loop ─────────┘
```

---

## 1. 두 라이브러리 계층 + 핵심 타입

| 헤더 | 역할 |
|---|---|
| `<Effekseer.h>` | 엔진 코어 — `Manager` / `Effect` / `Handle` / `Vector3D` / `Matrix44` |
| `<EffekseerRendererGL.h>` | OpenGL 백엔드 — `Renderer` / `CreateGraphicsDevice` / 5종 SubRenderer / 4종 Loader |

핵심 타입은 전부 **`Ref` = 내부 참조 카운트 스마트 포인터** (shared_ptr 류). `new`/`delete` 없음, 명시 release 불요 — *스코프 종료 또는 `Reset()` 이 정리*.

| 타입 | 정체 | 무효값 / 정리 |
|---|---|---|
| `Effekseer::ManagerRef` | 엔진 매니저 핸들 | `mManager.Reset()` / `mManager.Get() == nullptr` 로 유효성 검사 |
| `EffekseerRendererGL::RendererRef` | GL 렌더러 핸들 | `mRenderer.Reset()` / `mRenderer.Get() == nullptr` |
| `Effekseer::EffectRef` | 로드된 에셋(틀) | `shared_ptr` 류 — dtor 자동, `.Reset()` 가능 |
| `Effekseer::Handle` | **재생 인스턴스 ID** (단순 `int32_t`) | **`Ref` 아님!** `-1`(또는 본 코드 관례)로 invalid 표기. `StopEffect(h)` 로 정지 |

> ⚠ `Handle` 만 스마트 포인터가 아니라 **정수 ID**다. RAII 가 안 걸리므로 dtor/OnStop 에서 *직접* `StopEffect` 해줘야 leak/dangling 이 안 난다 ([EffekseerPlayable.cpp:19-26](../apps/_MyApp_/src/VFX/EffekseerPlayable.cpp#L19-L26) dtor 가 그 책임을 진다).

---

## 2. 부트스트랩 — Manager + Renderer 생성 (`VFXSystem::Init`)

엔진을 켜는 순서는 **고정 레시피**다. 데모/게임 어느 쪽이든 동일하다.

```cpp
// 1) GL 그래픽스 디바이스 → 렌더러 (maxSprites = 동시 스프라이트 상한)
auto graphicsDevice = ::EffekseerRendererGL::CreateGraphicsDevice(
    ::EffekseerRendererGL::OpenGLDeviceType::OpenGL3);          // macOS/Core 3.3+ 는 OpenGL3
mRenderer = ::EffekseerRendererGL::Renderer::Create(graphicsDevice, maxSprites);

// 2) 매니저 (인스턴스 풀 상한)
mManager  = ::Effekseer::Manager::Create(maxSprites);           // 본 프로젝트 기본 8000

// 3) 5종 SubRenderer 를 매니저에 주입 — 렌더러가 만들고 매니저가 소유
mManager->SetSpriteRenderer(mRenderer->CreateSpriteRenderer());
mManager->SetRibbonRenderer(mRenderer->CreateRibbonRenderer());
mManager->SetRingRenderer  (mRenderer->CreateRingRenderer());
mManager->SetTrackRenderer (mRenderer->CreateTrackRenderer());
mManager->SetModelRenderer (mRenderer->CreateModelRenderer());

// 4) 4종 Loader — 텍스처/모델/머티리얼/커브 파일 로드 위임
mManager->SetTextureLoader (mRenderer->CreateTextureLoader());
mManager->SetModelLoader   (mRenderer->CreateModelLoader());
mManager->SetMaterialLoader(mRenderer->CreateMaterialLoader());
mManager->SetCurveLoader(::Effekseer::MakeRefPtr<::Effekseer::CurveLoader>());  // 커브만 렌더러 무관
```

> 출처: [VFXSystem.cpp:11-30](../apps/_MyApp_/src/VFX/VFXSystem.cpp#L11-L30) ≡ [demo1/main.cpp:71-89](../apps/effekseer_demo/demo1/main.cpp#L71-L89) — **두 코드가 한 글자도 다르지 않다**. Context7 공식 가이드의 "Configure Drawing Modules" 와도 일치.
>
> 5종 SubRenderer 를 다 안 걸면 해당 종류의 파티클이 *조용히 안 그려진다*. 단발 muzzle 만 쓸 거라도 5종 다 거는 게 안전 관례.

---

## 3. Effect 에셋 로드 — `char16_t` 경로 (EFK_CHAR)

```cpp
// 경로는 반드시 u"..." (char16_t / UTF-16) — Effekseer EFK_CHAR 규약. "..." 일반 문자열 ✗
mEffect = Effekseer::Effect::Create(mManager, u"resources/Laser01.efkefc");
assert(mEffect.Get() != nullptr && "로드 실패 — resources/ 확인");
```

- 경로 리터럴에 **`u` 접두사 필수**. `EFK_CHAR` 는 Windows=`wchar_t`, 그 외=`char16_t` 로 분기되며 `u"..."` 가 양쪽 안전.
- 로딩은 무겁다 → **에셋당 1회만** 로드하고 `EffectRef` 를 재사용해 여러 번 `Play`. 본 프로젝트는 그 캐시를 `SJH::ResourceRegistry::CreateEffect` 가 맡고, 결과를 [`SJH::Effect`](../src/resource_registry/effect.h) 가 wrap 한다:

```cpp
// src/resource_registry/effect.h — EffectRef 를 캐시 entry 로 보관
class Effect {
    explicit Effect(::Effekseer::EffectRef ref) : mRef(ref) {}
    ::Effekseer::EffectRef Ref() const { return mRef; }   // Play 인자로 직접 전달
    // 복사/이동 전부 = delete (캐시 단일 소유)
};
```

- 확장자: **`.efkefc`** (1.6+ 신 포맷) 권장. ⚠ *에디터 export 버전 함정* → §7 참조.

---

## 4. 재생 lifecycle — `Play` → `Handle` → `StopEffect` (`EffekseerPlayable`)

게임의 leaf Playable 이 캡슐화한 한 인스턴스의 일생.

```cpp
// OnPlay — Effect 캐시의 Ref 를 좌표와 함께 재생. 반환 Handle 을 보관
mHandle = mManager->Play(mEffect->Ref(),
                         ::Effekseer::Vector3D(spawn[0], spawn[1], spawn[2]));

// OnStop / dtor — Handle 로 정지 후 invalid 표기 (정수 ID 라 수동 정리 필수)
mManager->StopEffect(mHandle);
mHandle = -1;                                  // 본 프로젝트 invalid 관례

// OnUpdate (TrackPolicy::FollowOwner) — 매 frame Actor 위치로 이동
mManager->SetLocation(mHandle, ::Effekseer::Vector3D(p[0], p[1], p[2]));

// 자연 종료 감지 — 파티클이 다 끝나면 Exists 가 false. 비루프면 finished 마킹
if (mHandle >= 0 && !mManager->Exists(mHandle) && !isLoop_) {
    mHandle = -1;  finished_ = true;
}
```
> 출처: [EffekseerPlayable.cpp:28-68](../apps/_MyApp_/src/VFX/EffekseerPlayable.cpp#L28-L68).

**Handle 조작 API 정리:**

| 호출 | 의미 | 사용처 |
|---|---|---|
| `Play(EffectRef, Vector3D)` | 재생 시작, `Handle` 반환 | OnPlay (좌표 객체 오버로드) |
| `Play(EffectRef, x, y, z)` | 〃 (float 3개 오버로드) | demo1 |
| `StopEffect(Handle)` | 해당 인스턴스 즉시 정지 | OnStop / dtor |
| `Exists(Handle)` → `bool` | 인스턴스가 아직 살아있나 | 자연 종료 감지 |
| `SetLocation(Handle, Vector3D)` | **절대 위치** 설정 | FollowOwner 추적 |
| `AddLocation(Handle, Vector3D)` | **상대 이동** (델타 누적) | demo1 좌→우 슬라이딩 |

> **두 가지 위치 추적 정책** ([EffekseerPlayable.h:12-16](../apps/_MyApp_/src/VFX/EffekseerPlayable.h#L12-L16) `enum class TrackPolicy`):
> - `Static` — OnPlay 시 1회만 좌표 지정 (단발 muzzle/폭발).
> - `FollowOwner` — OnUpdate 매 프레임 `SetLocation` 으로 Actor.Transform 추적 (오라/지속 이펙트).

**진단 훅** — Play 직후 [`SJH::Diagnostics::EffekseerDiagnostics`](../src/diagnostics/effekseer_diagnostics.h) (2026-06-02 신설) 가
`-1`(Play 실패) / Play 즉시 종료(빈 이펙트·텍스처 전무) 를 잡는다:
```cpp
SJH::Diagnostics::EffekseerDiagnostics::CheckPlayHandle(mHandle, "effekseer");
SJH::Diagnostics::EffekseerDiagnostics::CheckHandleAlive(mHandle, mManager->Exists(mHandle), "effekseer");
```

---

## 5. 매 프레임 — `Update` (시뮬레이션 전진)

```cpp
void VFXSystem::Update(float dt) {
    if (mManager.Get() == nullptr) return;
    mManager->Update(dt * 60.0f);   // ★ 초(dt) → 프레임. deltaFrame=1.0 이 "60fps 기준 1프레임"
}
```

> **★ deltaFrame 단위 함정** — Effekseer `Update` 의 인자는 *초가 아니라 프레임* 이다. 기준은 60fps → `deltaFrame = dt_seconds * 60`.
> 그냥 `Update(dt)` 로 초를 넘기면 파티클이 **60배 느리게** 흐른다. (tweeny `step(ms)` vs `step(ratio)` 오버로드 함정과 같은 부류의 *단위* 버그.)

**두 가지 Update 오버로드** (1.7 에 둘 다 존재):

| 형태 | 코드 | 비고 |
|---|---|---|
| `Update(float deltaFrame)` | [VFXSystem.cpp:36](../apps/_MyApp_/src/VFX/VFXSystem.cpp#L36) | 간이. 게임이 쓰는 형태 |
| `Update(const Manager::UpdateParameter&)` | [demo1/main.cpp:132-133](../apps/effekseer_demo/demo1/main.cpp#L132-L133) | 정밀 — 비동기/업데이트 횟수 제어 |

**LayerParameter** (LOD/컬링 기준점, 데모만 사용) — 뷰어 위치를 매니저에 알려준다:
```cpp
::Effekseer::Manager::LayerParameter layer;
layer.ViewerPosition = ::Effekseer::Vector3D(camX, camY, camZ);
mManager->SetLayerParameter(0, layer);          // demo1/main.cpp:124-129
```

---

## 6. 매 프레임 — `Draw` (렌더 패스)

Effekseer 렌더는 **반드시 view/proj 행렬을 렌더러에 먹인 뒤 `BeginRendering`→`Draw`→`EndRendering`** 으로 감싼다. 우리 씬의 GL draw 와 같은 프레임에, *씬을 그린 다음* 호출하는 게 정석(반투명 파티클이 위에 얹힘).

```cpp
void VFXSystem::Draw(const float* viewMat, const float* projMat) {
    ::Effekseer::Matrix44 view, proj;
    std::memcpy(view.Values, viewMat, sizeof(float) * 16);   // SJH 카메라 행렬 → Effekseer
    std::memcpy(proj.Values, projMat, sizeof(float) * 16);
    mRenderer->SetCameraMatrix(view);
    mRenderer->SetProjectionMatrix(proj);

    mRenderer->BeginRendering();
    mManager->Draw();             // 인자 없는 오버로드 (DrawParameter 기본값)
    mRenderer->EndRendering();
}
```
> 출처: [VFXSystem.cpp:39-52](../apps/_MyApp_/src/VFX/VFXSystem.cpp#L39-L52).

**행렬 변환 — `Effekseer::Matrix44`:**
- `Matrix44` 는 16 float 의 `Values` 멤버 (`float Values[4][4]`). 우리는 SJH/vmath 행렬을 **`memcpy` 로 그대로 부어** 쓴다 (VFXSystem) 또는 **element-wise 복사** (demo1 `ToEfkMat`, 주석 "index swap 제거").
- proj 를 Effekseer 가 직접 만들 수도 있다 — `efkProj.PerspectiveFovRH(fovRad, aspect, near, far)` ([demo1/main.cpp:105-110](../apps/effekseer_demo/demo1/main.cpp#L105-L110)). 이름 그대로 **RH(우수 좌표계)** 빌더. `LookAtRH` 도 동일 계열.
- ⚠ **좌표계 정합** — Effekseer 기본은 `SetCoordinateSystem(CoordinateSystem::RH)` 로 맞출 수 있고, 본 프로젝트는 RH 카메라(OpenGL 관례)를 쓰므로 *명시 호출 없이* RH 로 동작 중. LH 엔진과 통합한다면 `SetCoordinateSystem` + 행렬 핸드edness 를 먼저 의심.

**Draw 두 오버로드 + 데모의 정밀 버전:**
```cpp
// demo1 — DrawParameter 명시 (ZNear/ZFar/VP 직접 제어)
mRenderer->SetTime(static_cast<float>(mFrame) / 60.0f);   // 셰이더 애니메이션 시간(초)
mRenderer->SetProjectionMatrix(efkProj);
mRenderer->SetCameraMatrix(efkView);
mRenderer->BeginRendering();
::Effekseer::Manager::DrawParameter draw;
draw.ZNear = 0.0f;  draw.ZFar = 1.0f;
draw.ViewProjectionMatrix = mRenderer->GetCameraProjectionMatrix();
mManager->Draw(draw);
mRenderer->EndRendering();
```
> 출처: [demo1/main.cpp:139-151](../apps/effekseer_demo/demo1/main.cpp#L139-L151). `SetTime` 은 *초 단위* (frame/60) — deltaFrame(§5) 과 단위가 다르니 헷갈리지 말 것.

---

## 7. 종료 — `Reset` (순서 중요)

```cpp
void VFXSystem::Shutdown() {
    mManager.Reset();    // 매니저 먼저 (인스턴스/렌더러 참조 해제)
    mRenderer.Reset();   // 렌더러 나중
}
// demo1 shutdown 순서: mEffect.Reset() → mRenderer.Reset() → mManager.Reset()
```
- `Ref` 타입이라 `Reset()` 만으로 정리. 명시적 `delete` 없음.
- 살아있는 `Handle` 은 매니저 Reset 전에 `StopEffect` 하는 게 안전 (leaf Playable dtor 가 책임지지만, 시스템 단위 종료 시엔 매니저 Reset 이 일괄 정리).

---

## 8. 함정 / 인수인계 시 반드시 전달할 것 (gotchas)

이 저장소에서 *실제로 사람을 잡은* 문제들. 신규 작업자에게 **이 절을 가장 먼저** 읽혀라.

### 8.1 ⚠ `.efkefc` export 버전 — 신 포맷이면 `Create` 가 *조용히* 실패
- 런타임이 지원하는 바이너리 버전(`SupportBinaryVersion`)보다 **높은 버전으로 export 된 `.efk(efc)`** 는 `Effect::Create` 가 **에러 없이 `nullptr`** 을 돌려준다 (예외/로그 X).
- 본 프로젝트 런타임은 1.7.3.0 → **Effekseer 1.7 에디터로 export 필수** (포맷 > 1710 이면 막힘).
- 증상: "코드는 맞는데 아무것도 안 나옴" → `mEffect.Get() == nullptr` 부터 의심. `EffekseerDiagnostics` 가 Play 단계에서 잡아준다.

### 8.2 ⚠ `.efk` 내장 텍스처 base 경로 = `.efk` 파일이 있는 디렉토리
- 에디터가 .efk 에 박아둔 텍스처 상대경로는 **`.efk` 파일 위치 기준**으로 해석된다. `../foo.png` 는 .efk 의 *부모* 디렉토리.
- 리소스 디렉토리 구조를 옮길 때 텍스처가 깨지는 주범. resources/ 레이아웃을 에디터 기준과 맞춰라.

### 8.3 ⚠ Effekseer init 이 바인딩된 VAO 의 EBO 를 덮어쓴다 (GL 상태 오염)
- `EffekseerRendererGL` 초기화/렌더가 현재 바인딩된 VAO 의 `GL_ELEMENT_ARRAY_BUFFER` 바인딩을 *말없이 갈아끼울 수 있다* (Box2D debug-draw 도 동일).
- 본 프로젝트 대처: `ScreenQuadStage` 등 EBO 의존 패스가 **매 프레임 `ebo->Bind()` 로 재핀**. Effekseer Draw 전후로 자기 VAO/EBO 를 신뢰하지 말 것.

### 8.4 deltaFrame ≠ 초 (§5 재강조)
- `manager->Update(dt)` 에 *초* 를 넘기는 실수 = 파티클 60배 슬로우. 반드시 `dt * 60.0f`.

### 8.5 `Handle` 은 정수 ID — RAII 없음 (§1·§4 재강조)
- `ManagerRef`/`EffectRef` 와 달리 `Handle` 은 스마트 포인터가 아니다. dtor/OnStop 에서 `StopEffect` 를 *직접* 호출하지 않으면 파티클이 안 죽거나 dangling 참조가 된다.

### 8.6 데모(`demo1`) vs 게임(`_MyApp_`) 의 사용 결이 다르다
- `demo1` = Manager/Renderer 를 `main.cpp` 가 직접 들고 `LayerParameter`/`UpdateParameter`/`DrawParameter` 구조체까지 손으로 채우는 *교과서 모드*.
- `_MyApp_` = `VFXSystem`(owner) + `EffekseerPlayable`(leaf) + `SJH::Effect`(registry) 로 쪼갠 *프로덕션 모드* (§0).
- 신규 이펙트를 게임에 붙일 땐 **demo1 을 흉내내지 말고** §0 의 3분할을 따른다.

---

## 9. API 빠른 참조 (이 저장소에서 실제 호출된 것만)

| 분류 | 호출 | 출처 |
|---|---|---|
| 부트 | `EffekseerRendererGL::CreateGraphicsDevice(OpenGLDeviceType::OpenGL3)` | VFXSystem / demo1 |
| 부트 | `EffekseerRendererGL::Renderer::Create(device, maxSprites)` | 〃 |
| 부트 | `Effekseer::Manager::Create(maxSprites)` | 〃 |
| 부트 | `manager->Set{Sprite,Ribbon,Ring,Track,Model}Renderer(renderer->Create…Renderer())` | 〃 |
| 부트 | `manager->Set{Texture,Model,Material}Loader(renderer->Create…Loader())` | 〃 |
| 부트 | `manager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>())` | 〃 |
| 에셋 | `Effekseer::Effect::Create(manager, u"path.efkefc")` → `EffectRef` | effect.h / demo1 |
| 재생 | `manager->Play(EffectRef, Vector3D)` / `Play(ref, x,y,z)` → `Handle` | EffekseerPlayable / demo1 |
| 재생 | `manager->StopEffect(Handle)` | EffekseerPlayable / demo1 |
| 재생 | `manager->Exists(Handle)` → `bool` | EffekseerPlayable |
| 재생 | `manager->SetLocation(Handle, Vector3D)` / `AddLocation(Handle, Vector3D)` | EffekseerPlayable / demo1 |
| 갱신 | `manager->Update(float deltaFrame)` ★`dt*60` | VFXSystem |
| 갱신 | `manager->Update(Manager::UpdateParameter&)` + `SetLayerParameter(0, LayerParameter)` | demo1 |
| 렌더 | `renderer->SetCameraMatrix(Matrix44)` / `SetProjectionMatrix(Matrix44)` | VFXSystem / demo1 |
| 렌더 | `renderer->SetTime(seconds)` / `GetCameraProjectionMatrix()` | demo1 |
| 렌더 | `renderer->BeginRendering()` … `manager->Draw([DrawParameter])` … `renderer->EndRendering()` | VFXSystem / demo1 |
| 행렬 | `Matrix44::Values` (16 float) — `memcpy` / `PerspectiveFovRH` / `LookAtRH` | VFXSystem / demo1 |
| 종료 | `manager.Reset()` / `renderer.Reset()` / `effect.Reset()` | VFXSystem / demo1 |

---

### 관련 문서
- 설계/마일스톤: [doc/superpowers/specs/2026-05-26-m5-leaf-playables-design.md](../doc/superpowers/specs/2026-05-26-m5-leaf-playables-design.md) (M5 leaf Playable), [doc/topdown-shooter-progress.md](topdown-shooter-progress.md)
- 동류 API 노트: [doc/api/Box2DAPI.md](api/Box2DAPI.md) · [doc/api/FMODAPI.md](api/FMODAPI.md) · [doc/api/EngineAPI.md](api/EngineAPI.md)
- 의존성 등록(POST_BUILD dll copy 등): `cmake/Dependency.cmake` 의 `game_deps`, [doc/FMOD_Setup.md](FMOD_Setup.md) (FMOD 기준이나 패턴 동일)
