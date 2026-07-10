# InitScheduler — 데이터주도 topo-sort 초기화 아키텍처 설계 (2026-06-19)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **상태**: 설계 합의 완료(brainstorm Section 1~3), 구현 미착수. 다음 = writing-plans.
> **재개 진입점**: [`doc/handoffs/2026-06-19/2026-06-19-init-scheduler-architecture-resume-handoff.md`](../../handoffs/2026-06-19-init-scheduler-architecture-resume-handoff.md)
> **브랜치/HEAD**: `game/main` / `cadf80b` (10 mutual 사이클 절단 완료 직후)

## 1. 배경 / 문제

`apps/_MyApp_`의 `game_application::startup()`은 현재 **~40노드 x 10레벨 깊이의 의존 DAG**를 *손으로 위상정렬해 직렬로 나열*한 코드다 (위상 분석은 [`doc/diagrams/startup-init-topo.dot`](../../../doc/diagrams/startup-init-topo.dot)에 Kahn-레벨로 박제됨). 세 가지 문제:

1. **취약성** — 의존 순서가 코드 줄 순서에 암묵적으로 인코딩. 노드 하나 추가/이동 시 순서를 사람이 다시 추적해야 함.
2. **잔존 모듈 사이클** — 의존 그래프에 유일하게 남은 `render -> resource_registry -> sprite -> render` 3-사이클(부자연 엣지 = `render -> rr`). 파이프라인 조립 함수(`SetupDefaultPipeline`/`BuildPostFXChain`)가 `src/render/`에 살며 rr을 직접 include하는 게 원인.
3. **SIOF 위험** — 싱글톤 lazy-construction 순서에 의존하면 static init 순서 미정의(Cocos가 명시 경고). 명시적·강제된 init 순서가 필요.

## 2. 목표 / 비목표

**목표**
- `startup()`을 **데이터주도 선언적 task 그래프**로: 각 init 단계가 `(id, deps, affinity, fn)`을 선언, 스케줄러가 topo-sort해 실행.
- **엔진(저변동)과 클라(고변동)의 변동성 분리** + 엔진이 init 순서를 소유(제어역전).
- 파이프라인 조립 함수를 `src/render/` 밖으로 빼 **`render -> rr` 절단 → 완전 DAG**.
- **동반 전역 정리(D-7/D-8)**: `PostFXRegistry` 흡수 + `Manager` 개명 — 전역 싱글톤 5종 중 중복 제거 + Director 명칭 충돌 해소 (상세 §11).

**비목표 (이번 범위 밖)**
- **병렬 실행(T3)** — OpenGL 단일 컨텍스트 제약(아래 §6)으로 이득<위험. 미래 확장으로 게이트. 이번은 **직렬(T2)**.
- 스케줄러의 엔진 모듈 승격 — 우선 클라 거주(YAGNI), 재사용 시 추출.

## 3. 확정 결정 (재론 금지)

| ID | 결정 | 근거 |
|----|------|------|
| **D-1** | **지점① = push** — SceneRenderer가 라이트용 program 목록을 rr에서 *pull*하지 않고, 외부가 *push* | `GetAllPrograms` 소비자 1개·재발 0, 4엔진 정통(외부는 *값 push*, *목록 pull* 안 함; Cocos는 `GetAllPrograms` 부재 = per-pass 라이트), 1-메서드 인터페이스는 다형성 0 speculative seam |
| **D-2** | **지점② = A** — 파이프라인 함수 *정의*를 신규 엔진 모듈 `src/render_bootstrap/`로 이주(render core 밖). 클라로 내리는 trivial-C 아님 | "engine shared util"(재사용 의도), 관심사 분리(조립 != 실행), Cocos `Director` init / Unity `ScriptableRendererFeature` 정통, rr 청결 유지 |
| **D-3** | **scope = T2** — 데이터주도 topo-sort + **직렬** 실행. 병렬은 T3로 게이트 | 깊은 startup() DAG라 topo-sort 정당. GL 단일스레드로 병렬 이득 작음 |
| **D-4** | **변동성 분리 + Template Method** — 엔진=저변동 고정 스켈레톤이 hook 타이밍에 클라 InitScheduler를 *역호출* | Unreal 페이즈/Unity load type/Cocos `applicationDidFinishLaunching`/sb7 `application::run`+`startup` 전부 동일 패턴 |
| **D-5** | **검증→Kahn 직렬→F-2 fail-fast** | 그래프 오류는 실행 전 하드에러, 런타임 task 실패는 즉시 중단(init 실패는 보통 치명) |
| **D-6** | **등록 API = fluent builder (C++17)** | designated initializer `{.Id=}`는 C++20 → MSVC 미지원. fluent는 프로젝트 정통(Tweeny/UniformAtlas) |
| **D-7** | **`PostFXRegistry` → `ResourceRegistry` 흡수** — 중복 별칭 제거(pass Material 은 이미 rr 의 `mat_pass_<name>`). 연출 트랙은 `rr.FindMaterial("mat_pass_"+name)` 직접 조회 | 싱글톤 -1. PostFXRegistry 자기 문서가 "서비스 로케이터" 인정 + dangling 경고. Godot/Cocos 관심사분리 정통 |
| **D-8** | **`Manager` 개명**(예: `GameSystems`/`GameApp`) — 엔진 `Scene::Director` 와 "Director" 명칭 충돌 해소 | 병합 아님(다른 도메인/레이어). 명칭만 |

## 4. 아키텍처 — 3층 (변동성 분리)

```
[엔진 저변동]  EngineBootstrap::Boot(IClientBootstrap&)   <- Template Method, init 순서 소유
                  |  고정 단계(DeviceContext/ResourceRegistry/SetupDefaultPipeline...)
                  |  + hook 지점(OnResourcesReady / OnSceneSetup / OnBeforeFirstFrame)
                  v  (제어역전: 엔진이 클라를 역호출)
[클라 고변동]  game_application : IClientBootstrap
                  |  각 hook 안에서 InitScheduler에 task 등록 + RunAll()
                  v
[제네릭 유틸]  InitScheduler  <- Validate -> Kahn topo-sort -> 직렬 실행(F-2 fail-fast)
[엔진 공유]    src/render_bootstrap/  <- SetupDefaultPipeline/BuildPostFXChain (D-2, task가 호출)
```

- **엔진 스켈레톤(저변동)** = 거의 안 바뀌므로 *고정 시퀀스(Boot() 직선 본문)*. topo-sort 불필요(YAGNI). 복잡해지면 같은 `InitScheduler` 재사용 가능.
- **클라 스케줄러(고변동)** = 40노드 자주 변함 → `InitScheduler`가 값을 함.
- **거주**: `InitScheduler`/task = 클라 `apps/_MyApp_/src/Bootstrap/`. 파이프라인 함수 = 엔진 `src/render_bootstrap/`.

> `EngineBootstrap`은 우리 코드다. `extern/sb7code`의 `application::run()`(최상위 템플릿메서드)은 불가침이라, `EngineBootstrap::Boot`는 우리가 소유한 `game_application::startup()`(sb7가 부르는 hook) 안에서 시작된다.

## 5. 컴포넌트

### 5.1 InitTask (데이터 모델)

```cpp
enum class EAffinity { Cpu, Gl };          // 분류만 (T2 직렬; 미래 T3 병렬 분기용)

struct InitTask
{
    std::string              Id;           // "reg" / "sceneFB" / "screenQuadStage" (topo.dot 노드명)
    std::vector<std::string> Deps;         // 선행 task id (topo.dot 간선)
    EAffinity                Affinity;     // Cpu(기본) | Gl
    std::function<bool()>    Run;          // 실제 init. 성공=true (F-2: false면 fail-fast)
};
```

### 5.2 InitScheduler + Fluent 등록 (C++17)

```cpp
class InitScheduler
{
  public:
    InitTaskBuilder Task(std::string id);   // 등록 빌더 반환
    void            RunAll();               // Validate -> ExecuteInOrder

  private:
    friend class InitTaskBuilder;
    void AddTask(InitTask task);            // 빌더 Does()가 확정 호출
    void Validate() const;                  // 중복id/누락dep/사이클 -> 하드에러
    void ExecuteInOrder();                  // Kahn 직렬 + F-2

    std::vector<InitTask> mTasks;
};

class InitTaskBuilder                       // 댕글링 방지: task를 값 보유, Does()에서 commit
{
    InitScheduler &mSched;
    InitTask        mTask;
  public:
    InitTaskBuilder(InitScheduler &s, std::string id) : mSched(s) { mTask.Id = std::move(id); }
    InitTaskBuilder &Needs(std::vector<std::string> deps) { mTask.Deps = std::move(deps); return *this; }
    InitTaskBuilder &Gl()  { mTask.Affinity = EAffinity::Gl;  return *this; }
    InitTaskBuilder &Cpu() { mTask.Affinity = EAffinity::Cpu; return *this; }
    void             Does(std::function<bool()> run) { mTask.Run = std::move(run); mSched.AddTask(std::move(mTask)); }
};
```

사용:
```cpp
sched.Task("screenQuadStage").Needs({"reg", "sceneRenderer", "mSceneFB"}).Gl()
     .Does([&] { sqStage = SJH::Render::SetupDefaultPipeline(reg, mgr.SceneRenderer(), sceneFB, cfg);
                 return sqStage != nullptr; });
```

### 5.3 EngineBootstrap + IClientBootstrap (Template Method / 제어역전)

```cpp
class IClientBootstrap                       // 제어역전 seam (다형성용 아님 = sb7 startup/render와 동일 정당성)
{
  public:
    virtual ~IClientBootstrap() = default;
    virtual void OnResourcesReady()   = 0;   // 자원 준비 후
    virtual void OnSceneSetup()       = 0;   // 씬 구성 타이밍
    virtual void OnBeforeFirstFrame() = 0;   // 첫 프레임 직전
};

class EngineBootstrap
{
  public:
    void Boot(IClientBootstrap &client)
    {
        InitDeviceContext();                 // 엔진 고정(저변동) - 진짜 엔진 전용 단계만
        client.OnResourcesReady();           // === HOOK: 클라가 자원 task 실행(reg 채우기 등) ===
        client.OnSceneSetup();               // === HOOK: 클라가 씬/파이프라인 task 실행 ===
        client.OnBeforeFirstFrame();         // === HOOK ===
    }
};
```

> 엔진 고정 단계(예: DeviceContext)와 hook 경계의 정확한 분할은 plan에서 실제 `startup()` 분석으로 확정.
> ⚠ 주의: `SetupDefaultPipeline`은 클라 `mSceneFB` 의존(topo.dot L3, deps=reg/sceneRenderer/mSceneFB)이라 **엔진 고정이 아니라 클라 task**(`OnSceneSetup` hook에서 render_bootstrap 함수 호출). *함수 정의=엔진(D-2), 호출=클라 task* 로 D-2와 일관.

### 5.4 render_bootstrap 모듈 (지점② fold = render→rr 절단)

| 변경 | 내용 |
|------|------|
| 이주 | `src/render/render_pipeline.{h,cpp}` (+구조체 `DefaultPipelineConfig`/`PostFXStageConfig`/`PostFXChainResult`) → `src/render_bootstrap/` |
| 신규 CMake | `SJH::render_bootstrap` STATIC, deps = `render rr object material scene buffer` |
| render | render_pipeline 드롭 → `render -> rr` include **0** |
| 클라 include 2곳 | `apps/_MyApp_/main.cpp:50`, `<apps>/_MyApp_/src/Playable/PostFXConstants.h:23`: `<render>/render_pipeline.h` → `<render_bootstrap>/render_pipeline.h` |
| umbrella | `SJH::engine`에 render_bootstrap 추가(19모듈), 클라는 우산 link라 부담 0 |

파이프라인 함수가 호출하는 rr API = `CreateProgram`/`RegisterMesh`/`CreateSharedMaterial` 3종(불변, render_bootstrap이 rr을 구상 의존). 신규 사이클 0(render_bootstrap는 최상위, 역의존 없음).

## 6. 에러 처리 + 결정성

**검증(실행 전, 하드에러 = 프로그래머 버그, engine_diagnostics 사이클 가드 정책):**

| 검사 | 처리 |
|------|------|
| 중복 id | abort "duplicate task id 'X'" |
| 누락 dep | abort "task 'X' depends on unknown 'Y'" |
| 사이클 | Kahn으로 전부 못 비움 → abort + 사이클 노드 출력 |

**실행(Kahn 직렬):** in-degree 0부터, **등록 순서로 tiebreak**(같은 레벨 다수 시) → 직렬 실행이 **재현 가능**(GL 상태/init 결정성, Graphics-Testing-Prompt 가치). 정렬·해시 순서 금지.

**Task 실패(F-2 fail-fast):** `Run()`이 false 반환 시 즉시 중단 + "task 'X' 실패로 init 중단". init 실패는 보통 복구 불가(셰이더 경로 오타 등)라 fail-fast가 디버깅상 가장 명확.

**OpenGL 단일스레드 제약(병렬 비목표 근거):** GL 객체 생성(텍스처/셰이더/FBO)은 GL 컨텍스트가 current인 *메인스레드 전용*(GLFW 1 컨텍스트). 같은 Kahn 레벨이라도 GL을 만지면 병렬 불가. CPU-only task(audio/physics/PNG 디코드)만 병렬 가능. → `Affinity`는 T2에서 *분류만*, 실행은 직렬. T3는 이 제약 위에서 별도 설계.

## 7. 마이그레이션 — topo.dot이 곧 명세

`startup-init-topo.dot`이 이미 전 노드의 deps를 인코딩(간선=의존) → 마이그레이션은 *기계적 전사*:

```
topo.dot:  {reg sceneRenderer mSceneFB} -> screenQuadStage
task:      sched.Task("screenQuadStage").Needs({"reg","sceneRenderer","mSceneFB"}).Gl().Does([&]{...});
```

- **점진 가능** — 40노드를 한 번에 안 바꿔도 됨.
- **누락 dep 위험** — 스케줄러는 "썼는데 미선언"은 못 잡음(너무 일찍 실행 → 크래시). topo.dot이 dep 분석의 정본이므로 전사 시 누락만 주의.
- **검증** — 빌드 GREEN + **GUI 육안**(라이팅/스프라이트/스테이지/발사/PostFX 정상).

## 8. 지점① — 별도 처리 (per-frame, init task 아님)

지점①(SceneRenderer 라이트용 program 목록 push)은 **매 프레임** 일이라 InitScheduler와 직교. `render()`에서 `Manager::Get().SceneRenderer().SetActivePrograms(reg.GetAllPrograms())`. render 모듈: SceneRenderer에 setter+멤버 추가, rr include 제거.
- program 집합이 init-stable이라 "마지막 init task로 1회 push"도 가능하나 robust = per-frame. **이 선택은 plan에서 확정.**

## 9. 정통 매핑 (context7 검증)

| 엔진 | 엔진 고정 스켈레톤 | 클라 hook | 병렬 |
|------|------------------|-----------|------|
| Unreal | `ELoadingPhase`(EarliestPossible→…→PostEngineInit) JSON 선언 + `Collection.InitializeDependency<T>()` | 게임 모듈 `StartupModule` | TaskGraph=에셋 |
| Unity | `RuntimeInitializeLoadType`(SubsystemRegistration→…→AfterSceneLoad) | 클라 메서드 hook | Addressables async=에셋 |
| Cocos | `Director::init()` 고정 + **SIOF 경고** | `applicationDidFinishLaunching` | AsyncTaskPool=에셋 |
| sb7(우리) | `application::run`(window→GL→loop) 불가침 | `startup`/`render`/`shutdown` | — |

공통: 자원 레지스트리/서버 = 싱글톤(소유), 실행 컨텍스트 = 주입(사용). 병렬 = 전부 *에셋 로딩*용이지 subsystem 배선용 아님.

## 10. 열린 항목 (plan에서 확정 / 미래)

- 엔진 고정 vs 클라 hook의 *정확한 노드 분할* (실제 startup() 분석).
- **EngineBootstrap 엔진-고정 코드량** — 실제 분석 시 엔진 전용 단계가 얇으면(sb7이 window/GL 담당, 나머지 대부분 게임별) `EngineBootstrap`의 주 가치는 *hook 타이밍 구조*(변동성 경계 강제). 가치 < 비용이면 클라 단일 스케줄러 + phase 그룹으로 단순화 검토(D-4 의도는 보존).
- 지점① push 위치: per-frame vs 마지막 init task 1회.
- `InitScheduler` 엔진 승격(`SJH::init`): 재사용 demo 생기면.
- T3 병렬(CPU task 스레드풀 + GL 큐): GL 제약 위에서 별도 설계.

## 11. 전역 싱글톤 정리 (동반 작업 — D-7/D-8)

> 그래프: [`doc/diagrams/2026-06-19-global-singletons.{dot,png,svg}`](../../../doc/diagrams/2026-06-19-global-singletons.png) (tracked).

전역 싱글톤 **5종** = 엔진 3(저변동) + 클라 2(고변동). Godot 'Servers' 관심사분리 정통:

| 싱글톤 | 레이어 | 관심사 |
|--------|--------|--------|
| `DeviceContext` | 엔진 | GL 상태 facade |
| `ResourceRegistry` | 엔진 | 자원 캐시 9종 |
| `Scene::Director` | 엔진 | 씬 그래프 + SceneContext |
| `Manager` | 클라 | 게임 시스템 허브 |
| `PostFXRegistry` | 클라 | passName->Material* (중복 별칭) |

**정리 (병합 X, 흡수/개명 O):**
- **D-7 PostFXRegistry 흡수** -> ResourceRegistry. 유일 흡수 후보. 시스템 4종(Audio/VFX/Physics/Text)은 이미 Manager 값멤버로 집약됨(추가 불요).
- **D-8 Manager 개명** -> Director 명칭 충돌 해소.
- **병합 금지**: Manager(클라/시스템) + Director(엔진/씬)는 같은 *Director 패턴*의 두 인스턴스일 뿐 책임 중복 아님. 병합 = god-object(Godot 'Servers' / Cocos 정통 위반 + DIP 가치 위반).

**부수 발견 — 시스템 격리 비대칭 (init 설계에 유리, grep 확정):**
- **Audio/VFX/Physics = 엔진 global 의존 0**. 이유: ① 각자 외부 lib(FMOD/Effekseer/Box2D)가 그 도메인 *server* 역할 -> 우리 엔진 global 불요. ② per-frame 데이터를 *param 주입*(`SetListener(pos)` / `Draw(view,proj)` / `SyncToTransform(root)`)으로 받음 = 깨끗한 DI. ③ Physics 가 가장 순수 — body 가 *절차적*(`b2CircleShape::m_radius` / `SetAsBox`)이라 로드할 에셋 0.
- **WorldTextSystem 만 rr 의존**(font atlas 가 SJH 자원이라 시스템이 직접 로드). **SceneRenderer** 는 코어 orchestrator라 Director/rr/DeviceContext 당김(rr 은 D-1 으로 절단=push).
- **함의**: Audio/VFX/Physics 가 이미 param-DI로 잘 격리됨 -> InitScheduler task 의 `Affinity=Cpu` 후보(GL 무관), `Deps` 선언 자연스러움. GL/rr 의존은 SceneRenderer/WorldText 뿐.

## 12. Decision Log

| 날짜 | 결정 | 비고 |
|------|------|------|
| 2026-06-19 | D-1 지점① push | 인터페이스 거부(speculative), 4엔진 검증 |
| 2026-06-19 | D-2 지점② A (render_bootstrap) | B(인터페이스) 대비 rr 청결·관심사 정직 |
| 2026-06-19 | D-3 T2 직렬 (병렬 게이트) | GL 단일스레드 |
| 2026-06-19 | D-4 Template Method + 변동성 분리 | 사용자 제안, 4엔진 정통 |
| 2026-06-19 | D-5 검증→Kahn→F-2 fail-fast | — |
| 2026-06-19 | D-6 fluent builder (C++17) | designated init=C++20 정정 |
| 2026-06-19 | D-7 PostFXRegistry -> rr 흡수 | 중복 별칭 제거, 싱글톤 -1 |
| 2026-06-19 | D-8 Manager 개명 | Director 명칭 충돌 해소 (병합 아님) |
