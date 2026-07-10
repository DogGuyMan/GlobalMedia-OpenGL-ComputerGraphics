잠깐 내 결정들을 처음부터 다시 한번 검토해 보자. 

Your questions have been answered: "SetupDefaultPipeline / BuildPostFXChain (현재 render 모듈 거주, 둘 다 _MyApp_ main.cpp 에서만 호출)의 의도는?"="엔진 공유 유틸". You can now continue with these answers in mind.

이 내용부터 다시 한번 말해줘 대신,
선택지에 대한 각각의 사이드 이펙트를 조사하고
각 핵심 A, B 선택지에 최대 3가지의 서브 선택지를 제안해줘
Show less

-------
-------
-------
-------

좋습니다 — 결정을 재검토하고, 각 선택지의 실제 사이드 이펙트를 코드로 조사한 뒤 A·B 각각에 서브 선택지를 제안하겠습니다. 그러려면 먼저 제가 아직 안 읽은 render_pipeline.cpp 실제 구현과 rr이 노출하는 API 표면을 grounding해야 정확합니다. (그리고 방금 IDE에서 여신 doc/diagrams/...dot — 제가 만든 걸 거기로 옮기신 듯하니 그 위치도 확인합니다.)

Read render_pipeline.cpp
render_pipeline.cpp 구현 확보 — rr API 표면이 정확히 3개(CreateProgram/RegisterMesh/CreateSharedMaterial)임을 확인(내가 앞서 4개라 한 GetProgram은 미사용). 나머지 의존(object/buffer/material/scene)은 render가 이미 갖는 하향 엣지라 무관. 정밀 사이드이펙트를 위해 rr 시그니처 + render_pipeline.h 포함자 + diagrams 위치를 확인합니다.

Bash Check diagram relocation, render_pipeline.h includers, rr signatures
IN
ROOT=/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
echo "=== doc/diagrams (user relocated my .dot here?) ===" && ls -la "$ROOT/doc/diagrams/" 2>/dev/null || echo "(no doc/diagrams dir)"
echo "=== doc/handoffs (my original location) ===" && ls -1 "$ROOT/doc/handoffs/" 2>/dev/null
echo "=== who #includes render_pipeline.h (not just calls) ===" && grep -rHn '#include .*render_pipeline.h' "$ROOT/src" "$ROOT/apps"
echo "=== rr method signatures for interface sizing ===" && grep -nE 'CreateProgram|RegisterMesh|CreateSharedMaterial|GetAllPrograms' "$ROOT/src/resource_registry/resource_registry.h"

OUT
=== doc/diagrams (user relocated my .dot here?) ===
total 5400
drwxr-xr-x@ 40 escatrgot  staff    1280 Jun 19 14:19 .
drwxr-xr-x@ 22 escatrgot  staff     704 Jun 15 03:37 ..
-rw-r--r--@  1 escatrgot  staff    6148 Jun 15 03:37 .DS_Store
-rw-r--r--@  1 escatrgot  staff    3515 Jun 19 14:17 2026-06-19-module-deps-current.dot
-rw-r--r--@  1 escatrgot  staff  193075 Jun 19 14:17 2026-06-19-module-deps-current.png
-rw-r--r--@  1 escatrgot  staff   23847 Jun 19 14:17 2026-06-19-module-deps-current.svg
-rw-r--r--@  1 escatrgot  staff    3011 Jun 15 03:37 actor-component-deps.dot
-rw-r--r--@  1 escatrgot  staff  394072 Jun 15 03:37 actor-component-deps.png
-rw-r--r--@  1 escatrgot  staff   26699 Jun 15 03:37 actor-component-deps.svg
-rw-r--r--@  1 escatrgot  staff    2203 Jun 15 03:37 physics-class-deps.dot
-rw-r--r--@  1 escatrgot  staff  105875 Jun 15 03:37 physics-class-deps.png
-rw-r--r--@  1 escatrgot  staff   10040 Jun 15 03:37 physics-class-deps.svg
-rw-r--r--@  1 escatrgot  staff    1142 Jun 15 21:05 playable-class.dot
-rw-r--r--@  1 escatrgot  staff   97890 Jun 15 21:05 playable-class.png
-rw-r--r--@  1 escatrgot  staff    8415 Jun 15 21:05 playable-class.svg
-rw-r--r--@  1 escatrgot  staff     830 Jun 15 21:05 playable-composite-tree.dot
-rw-r--r--@  1 escatrgot  staff   69700 Jun 15 21:05 playable-composite-tree.png
-rw-r--r--@  1 escatrgot  staff    5932 Jun 15 21:05 playable-composite-tree.svg
-rw-r--r--@  1 escatrgot  staff    5156 Jun 15 21:02 playable-module-deps.dot
-rw-r--r--@  1 escatrgot  staff  522638 Jun 15 21:03 playable-module-deps.png
-rw-r--r--@  1 escatrgot  staff   35323 Jun 15 21:03 playable-module-deps.svg
-rw-r--r--@  1 escatrgot  staff    3938 Jun 15 03:37 player-build-tree.dot
-rw-r--r--@  1 escatrgot  staff  196416 Jun 15 03:37 player-build-tree.png
-rw-r--r--@  1 escatrgot  staff   18142 Jun 15 03:37 player-build-tree.svg
-rw-r--r--@  1 escatrgot  staff    4001 Jun 15 03:37 render-multipass-deps.dot
-rw-r--r--@  1 escatrgot  staff   26686 Jun 15 03:37 render-multipass-deps.svg
-rw-r--r--@  1 escatrgot  staff    2727 Jun 15 03:37 render-multipass-seq.dot
-rw-r--r--@  1 escatrgot  staff   17544 Jun 15 03:37 render-multipass-seq.svg
-rw-r--r--@  1 escatrgot  staff    1546 Jun 15 03:37 stage-fsm.dot
-rw-r--r--@  1 escatrgot  staff   50493 Jun 15 03:37 stage-fsm.png
-rw-r--r--@  1 escatrgot  staff    7467 Jun 15 03:37 stage-fsm.svg
-rw-r--r--@  1 escatrgot  staff    2459 Jun 18 15:20 startup-init-sequence.dot
-rw-r--r--@  1 escatrgot  staff  164622 Jun 18 15:23 startup-init-sequence.png
-rw-r--r--@  1 escatrgot  staff   25858 Jun 18 15:23 startup-init-sequence.svg
-rw-r--r--@  1 escatrgot  staff    2224 Jun 18 15:20 startup-init-topo.dot
-rw-r--r--@  1 escatrgot  staff  277944 Jun 18 16:39 startup-init-topo.png
-rw-r--r--@  1 escatrgot  staff   39068 Jun 18 15:23 startup-init-topo.svg
-rw-r--r--@  1 escatrgot  staff    6644 Jun 15 03:37 stat-class-deps.dot
-rw-r--r--@  1 escatrgot  staff  308923 Jun 15 03:37 stat-class-deps.png
-rw-r--r--@  1 escatrgot  staff   19883 Jun 15 03:37 stat-class-deps.svg
=== doc/handoffs (my original location) ===
2026-05-26-M4-handoff.md
2026-05-26-M5-handoff.md
2026-05-26-render-refactor-session.md
2026-05-27-pass-component-2camera-session.md
2026-05-31-depth-based-fog-bloom-infra.md
2026-05-31-postfx-chain-resize-robustness.md
2026-06-01-decomposition-spec-plan-prompt.md
2026-06-01-enemy-builder-agent-prompt.md
2026-06-01-entity-hit-death-fx-agent-prompt.md
2026-06-01-player-4layer-sprite-composition.md
2026-06-01-player-wasd-4direction-sprite-switching.md
2026-06-01-playerbehavior-decomposition-execution.md
2026-06-01-playerbehavior-decomposition.md
2026-06-01-raycast-hand-vfx-handoff.md
2026-06-01-stage-policetape-mouse-picking-handoff.md
2026-06-01-timer-migration-agent-prompt.md
2026-06-01-wave-controller-wiring-agent-prompt.md
2026-06-01-wave-enemy-session-context-handoff.md
2026-06-02-directional-facing-cullfix-session-resume-handoff.md
2026-06-02-directional-facing-pose-agent-prompt.md
2026-06-02-directional-facing-pose-resume-handoff.md
2026-06-02-enemy-hit-death-fx-wiring_m6task9.md
2026-06-02-p4-enemy-presentation-port-p5-reconcile-agent-prompt.md
2026-06-02-pb-decomposition-foundation-done-resume-handoff.md
2026-06-02-pb-decomposition-task4-done-resume-handoff.md
2026-06-02-playable-director-foundation-DONE-resume-handoff.md
2026-06-02-playable-director-foundation-agent-prompt.md
2026-06-02-player-hit-fx-decision-response.md
2026-06-02-player-hit-fx-playable-design-handoff.md
2026-06-02-playerbehavior-decomposition-resume-handoff.md
2026-06-02-task4-impulse-agent-prompt.md
2026-06-02-task6-on-director-foundation-agent-prompt.md
2026-06-02-timer-migration-followup-agent-prompt.md
2026-06-03-entity-refactor-resume-handoff.md
2026-06-03-pb-decomposition-COMPLETE-resume-handoff.md
2026-06-03-stage-fsm-agent-prompt.md
2026-06-03-stage-fsm-fmod-handoff.md
2026-06-03-stage-fsm-resume-handoff.md
2026-06-04-statemachine-deferred-transition-agent-prompt.md
2026-06-19-render-rr-cycle-cut-design-handoff.md
=== who #includes render_pipeline.h (not just calls) ===
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/render_pipeline.cpp:19:#include "render/render_pipeline.h"
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/main.cpp:50:#include "render/render_pipeline.h"
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/Playable/PostFXConstants.h:23:#include "render/render_pipeline.h" // SJH::Render::PostFXStageConfig
=== rr method signatures for interface sizing ===
80:		Material *CreateSharedMaterial(const std::string &key);
104:		Program *CreateProgram(const std::string &key,
116:		std::vector<Program *> GetAllPrograms() const;
122:		Mesh *RegisterMesh(const std::string &key, MeshUPtr mesh);
조사 완료 — 정밀 사이드이펙트를 위한 사실을 모두 확보했습니다. 두 가지 새 발견:

diagram 위치 정정: 제가 만든 2026-06-19-module-deps-current.{dot,png,svg}를 doc/diagrams/로 옮기셨네요(기존 다이어그램 12종과 동거 — doc/handoffs/엔 .md만 남음). → 앞으로 다이어그램은 doc/diagrams/가 정본. 반영하겠습니다.
지점② 리플 정정: render_pipeline.h를 include하는 곳은 main.cpp뿐이 아니라 apps/_MyApp_/src/Playable/PostFXConstants.h:23도 (PostFXStageConfig 사용). 즉 이주 시 클라 include 2곳 수정.
아래가 재검토 + 사이드이펙트 + 서브옵션입니다.

1. 결정 체인 재검토 (처음부터)
#	결정	재확인 / 사이드이펙트
사실	부자연 엣지 = render→rr	render의 rr include는 딱 2곳: scene_renderer.cpp:32(지점①) + render_pipeline.cpp:26(지점②). 둘 다 처리하면 render→rr=0 (그 외 render→rr 경로 없음 — grep 재확인).
결정 2 (당신)	지점② = 엔진 공유 유틸	⚠ 이 선택이 A/B 복잡도를 만든다. 'app-specific'이었다면 trivial-C(2함수를 클라 Bootstrap/로 이동)로 끝, 새 구조물 0. 공유라서 클라 이주 탈락 → 공유 거처 A/B 필요. 재고 원하시면 trivial-C 부활 가능.
결정 3 (잠정)	지점① = IProgramRegistry	사실 A/B와 직교한 별도 축이라 아래 §1에서 재검토.
1. 공통 축 — 지점① (A/B와 직교, 핵심 사이드이펙트 = rr→render 엣지 생성 여부)
서브	방식	사이드이펙트	rr→render
①-interface (추천)	render가 IProgramRegistry{GetAllPrograms()} 정의 → rr 구현 → SceneRenderer가 IProgramRegistry* 1회 주입받아 호출	헤더 1개 + SceneRenderer 세터 + Manager init 1줄. DIP/OOP 정통. spec target과 일치	생성 (rr이 render 인터페이스 구현)
①-push	Manager(rr 보유)가 매 프레임 rr.GetAllPrograms() fetch → SceneRenderer에 push(또는 RenderWithCamera 인자)	인터페이스 0. SceneRenderer에 programs 세터/인자. 클라가 매 프레임 fetch+전달	미생성 (rr 완전 무관)
①-dispatcher	LightUniformDispatcher가 IProgramRegistry* 보유(SceneRenderer 우회)	①-interface의 granularity 변형. 과설계 기미	생성
핵심: rr은 현재 render에 의존 안 함. ①-interface/B는 rr→render를 새로 만든다(spec target이라 허용·정상). ①-push만 그 엣지조차 안 만든다(최대 분리).

1. Core A — 신규 render_bootstrap 모듈 이주 (지점②)
사이드이펙트(공통): 새 모듈 src/render_bootstrap/(CMake+ALIAS). deps = render·rr·object·material·scene·buffer(전부 하향). render는 render_pipeline.{h,cpp}+구조체 3종(Config/Result) 떠나보냄 → 지점② render→rr=0. 클라 include 2곳 수정 + 링크. umbrella +1 → 19모듈. 신규 사이클 0(render_bootstrap는 최상위, 아무도 역의존 안 함). rr→render는 안 만듦(모듈이 rr→render 아님).

서브	내용	추가 사이드이펙트
A-i (추천)	순수 이동 — free functions 그대로, 이름 유지	최소. 호출부 include 경로만.
A-ii	이동 + PipelineBuilder/PostFXChainBuilder 클래스화 (프로젝트 fluent Builder 정통)	main.cpp 호출부 재작성. API 표면 ↑
A-iii	이동 + 지점① 주입 와이어링까지 흡수 (앱 "셋업" 단일 거처)	모듈 책임이 파이프라인→셋업 전반으로 확장. 응집 ↑ 범위 ↑
3. Core B — render 유지 + DIP 인터페이스 (지점②)
사이드이펙트(공통): render_pipeline.{h,cpp} render에 잔류. ResourceRegistry& 인자 → render가 정의한 인터페이스로 교체, resource_registry.h include 제거. rr이 인터페이스 구현 → rr→render 생성(target). 클라 호출부는 reg(rr)를 인터페이스 인자로 그대로 전달(업캐스트, 수정 거의 0). 인터페이스 표면 = 3메서드(CreateProgram/RegisterMesh(MeshUPtr)/CreateSharedMaterial). 새 모듈 0.

서브	내용	추가 사이드이펙트
B-i	단일 IRenderResourceRegistry (①GetAllPrograms + ②Create*3 = 4메서드)	인터페이스 1개로 최소. ISP 위반(SceneRenderer가 Create*까지 봄)
B-ii (B중 추천)	ISP 분리 2개: IProgramRegistry(①,read) + IPipelineResourceFactory(②,write). rr이 둘 다 구현	헤더 2개. SceneRenderer는 전자만, pipeline funcs는 후자만 의존. 가장 깨끗
B-iii	②만 IPipelineResourceFactory, ①은 push	인터페이스 표면 최소 + rr→render는 ②경유만. ①축 push와 결합
4. rr→render 엣지 매트릭스 (조합별 — 최종 그래프 차별점)
조합	새 모듈	rr→render 새 엣지	비고
A-i + ①-interface	+1	생성(①)	추천 기본 — 정직한 레이어링 + DIP
A-i + ①-push	+1	0	최대 분리 — render·rr 상호 무지(rr→sprite→render 전이만)
B-ii + ①-interface	0	생성(①②)	모듈 안 늘림, render가 rr API 추상화 소유
B-iii (②interface + ①push)	0	생성(②만)	모듈 0 + 인터페이스 최소
trivial-C(클라 이주)	0	0	결정2 'engine shared' 철회 시에만
제 추천 + 다음
A-i + ①-interface 를 기본 추천합니다 — 파이프라인 조립을 정직한 별 모듈로 분리(render 코어 최소화)하고, SceneRenderer의 program 필요는 좁은 1메서드 DIP로 해결, spec target(rr→render)과 일치. 새 구조물은 모듈 1 + 인터페이스 1로 적정합니다.

다만 두 가지를 당신 취향에 맞춰 고르실 수 있습니다:

그래프 최소주의라면 A-i + ①-push(rr→render조차 0, 단 클라 매프레임 fetch).
모듈 안 늘리기라면 B-ii + ①-interface(render가 rr 생성API 추상화 소유 감수).
어느 조합으로 기우시는지(또는 결정2 'engine shared'를 재고해 trivial-C로 돌릴지) 알려주시면, 그 조합으로 설계 섹션을 구체화하겠습니다. 아직 구현은 안 들어갑니다(brainstorming HARD-GATE).

너가 사용한 각 클래스 디자인 패턴, 아키텍쳐 용어에 대해 설명해줘
좋습니다 — 제가 이번 render→rr 절단 분석에서 쓴 패턴·아키텍처 용어를 우리 실제 코드에 박힌 위치와 함께 설명하겠습니다. 추상 정의만이 아니라 "이 논의에서 왜 등장했는지"까지 묶습니다.

1. SOLID 원칙 — 이번 절단을 이끄는 원칙
SOLID = 객체지향 설계 5원칙(SRP·OCP·LRP·ISP·DIP). 이번에 직접 쓴 건 끝의 둘.

DIP — Dependency Inversion Principle (의존 역전 원칙)
정의: 고수준 모듈이 저수준 구상(concrete) 에 의존하지 말고, 둘 다 추상(interface) 에 의존하라. "의존 화살표를 뒤집는다."
우리 코드: 지금 SceneRenderer(고수준)가 ResourceRegistry(구상)를 직접 #include → 이게 부자연 엣지. ①-interface안은 render가 IProgramRegistry(추상)를 정의하고 rr이 그걸 구현 → render는 추상만 알고 rr 구상을 모름. 화살표가 render→rr에서 rr→render로 역전됩니다. 클라 사이클 때 Contracts/의 IPlayerCommand·ILivable도 같은 기법.
ISP — Interface Segregation Principle (인터페이스 분리 원칙)
정의: 클라이언트가 안 쓰는 메서드 에 의존하게 만들지 마라. 큰 인터페이스 하나보다 역할별 작은 인터페이스 여럿.
우리 코드: B-i(단일 IRenderResourceRegistry 4메서드)는 ISP 위반 — SceneRenderer는 읽기(GetAllPrograms)만 필요한데 쓰기(CreateProgram 등)까지 보게 됨. B-ii는 IProgramRegistry(읽기) + IPipelineResourceFactory(쓰기)로 분리 = ISP 준수.
2. GoF 디자인 패턴 — 코드에 박혀 있는 패턴
Facade (파사드)
정의: 복잡한 하위 시스템 묶음에 단일 진입 창구를 제공.
우리 코드: 제가 resource_registry를 "최상위 캐시 파사드"라 부른 이유 — Texture/Material/Program/Mesh/Sound/Effect 등 9종 자원의 생성·캐시를 한 클래스가 단일 창구로 감쌉니다. Cocos Director, Unity Resources 정통.
Factory / "Pure Factory" (팩토리)
정의: 객체 생성 책임을 별도 함수/메서드로 캡슐화 (호출자는 new 안 함).
우리 코드: render_pipeline.cpp의 SetupDefaultPipeline이 주석에 "Pure Factory" — ScreenQuadStage를 만들어 반환. rr의 CreateProgram/CreateSharedMaterial도 팩토리 메서드. "Pure"=내부 상태 없이 입력→산출만.
Builder (빌더, 특히 fluent/체이닝)
정의: 복잡한 객체를 단계적으로 조립. 메서드가 자기 자신을 반환해 .A().B().C() 체이닝(=fluent).
우리 코드: 서브옵션 A-ii의 PipelineBuilder 안. 이미 UniformAtlas.LoadFromPNG().SetGrid(c,r), Playable.Append().Join()(Tweeny/DOTween 정통)이 이 패턴. 프로젝트가 선호하는 스타일이라 후보로 올렸습니다.
Singleton (싱글턴)
정의: 전역에 인스턴스 단 하나 + 전역 접근점.
우리 코드: ResourceRegistry::Get(), Scene::Director::Get()(scene_renderer.cpp:111). ①-interface에서 주입이 쉬운 이유 = rr이 싱글턴이라 SetProgramRegistry(&ResourceRegistry::Get()) 한 줄.
(참고: SequencePlayable/ParallelPlayable의 Composite 패턴도 코드에 있지만 이번 절단엔 안 썼습니다.)

3. 의존성 주입 & 제어 흐름
Dependency Injection (DI, 의존성 주입)
정의: 객체가 필요한 협력자를 스스로 찾지(pull) 않고 밖에서 받음(inject). 종류: 생성자 주입 / 세터 주입.
우리 코드: ①-interface = 세터 주입(SetProgramRegistry). M5의 D8 SpriteRenderer DI = 생성자 주입(SpriteRenderer(atlas, mesh, mat) — 자기가 rr에서 안 찾고 받음). DI의 목적 = 결합도↓ + 테스트 시 가짜(mock) 주입 가능.
Push vs Pull (밀기 vs 당기기)
정의: 데이터를 받는 쪽이 가져오나(pull), 주는 쪽이 밀어넣나(push).
우리 코드: 현재 scene_renderer.cpp:123은 SceneRenderer가 rr에서 program을 pull(이게 부자연 엣지의 정체). ①-push안은 Manager가 매 프레임 push. pull은 받는 쪽이 출처(rr)를 알아야 해서 결합 발생, push는 출처를 모름.
Upcast (업캐스트)
정의: 파생 클래스 포인터/참조를 기반(인터페이스) 타입으로 암묵 변환 (안전, 항상 성립).
우리 코드: B안에서 클라가 SetupDefaultPipeline(reg, …)에 rr을 넘길 때, reg(구상 ResourceRegistry)가 IPipelineResourceFactory&로 자동 업캐스트 → 그래서 호출부 수정 거의 0.
4. 아키텍처 구조 용어
용어	정의	우리 코드
Interface (순수 가상 계약)	구현 없이 시그니처만. C++의 추상 클래스(순수 virtual ... = 0)	IProgramRegistry, IPlayable, IPlayerCommand
Registry (레지스트리)	키→객체 등록·조회하는 "물건 찾는 곳"	ResourceRegistry(키→자원), IProgramRegistry(전체 program 조회)
Layering (레이어링)	모듈을 층으로 쌓고 의존은 한 방향(위→아래) 만	leaf=common/texture, top=rr/text. 화살표는 하향이어야
Cohesion / Coupling (응집/결합)	응집=한 단위가 한 관심사에 집중 / 결합=단위 간 얽힘 정도	render_bootstrap 분리 = "파이프라인 조립"으로 응집↑, render→rr 끊어 결합↓
Module PUBLIC/PRIVATE	CMake 의존 전파: 헤더에 노출=PUBLIC, .cpp 전용=PRIVATE	그래서 "헤더는 forward-decl이라 PRIVATE 엣지=끊기 쉽다"고 말한 것
DAG · Cycle · Back-edge · Topological sort (그래프 용어)
DAG (Directed Acyclic Graph): 방향성 + 사이클 없음. 모듈 의존이 DAG여야 빌드/이해가 깨끗.
Cycle / Mutual dependency: 화살표가 한 바퀴 돎. 2개짜리(A↔B)=2-사이클(=mutual), 3개짜리(render→rr→sprite→render)=3-사이클.
Back-edge (역방향 엣지): 레이어를 거스르는 화살표. 위상정렬했을 때 유일한 back-edge가 render→rr 라는 게 핵심 발견(그래서 빨강).
Topological sort (위상정렬): DAG의 노드를 "의존 순서대로" 일렬 정렬. 사이클이 있으면 정렬 불가 → 제가 "back-edge 1개뿐"이라 한 건 이 정렬을 돌려본 결과.
5. 역할(role) 이름 — 클래스에 붙인 책임 명칭
GoF 패턴은 아니지만 엔진에서 관용적으로 쓰는 책임 라벨:

Orchestrator (총괄자): 직접 일 안 하고 수집→위임만. scene_renderer.h가 "High-level Orchestrator" — Camera 순회·Light 수집 후 Dispatcher/Processor에 위임, GL 호출은 직접 안 함. Unreal FSceneRenderer 정통.
Dispatcher (디스패처): 한 데이터를 여러 수신자에 일괄 송신. LightUniformDispatcher = 수집된 Light를 등록된 모든 Program uniform에 broadcast. ← 지점①의 GetAllPrograms()가 이 "모든 Program" 목록.
Aggregate Root (집합 루트, DDD 용어): 연관된 객체 묶음의 단일 진입점. scene_context.h = Camera/Light 컬렉션의 Aggregate Root. 제가 "program을 여기 넣으면 그 책임을 위반"이라 기각한 근거.
is-a / 상속: "B is-a A" = B가 A를 상속. SpriteRenderer : MeshRenderer(Unity SpriteRenderer 정통) → 그래서 sprite→render는 자연스러운 엣지(끊으면 안 됨).
Component: Actor에 붙여서 특수 능력 부여(상속 대신 합성). MeshRenderer/Camera/Light/PassComponent 전부 Component. 그래서 fsm/timer가 scene/actor.h를 의존(Component이려고).
정리하면, 이번 절단의 엔진 = DIP(화살표 역전) + ISP(인터페이스 분리)이고, 등장 장치 = Facade(rr)·Factory(파이프라인 함수)·DI(주입)·Registry(program 목록)이며, 판단 잣대 = DAG/back-edge(구조) + Cohesion(관심사 분리)입니다.

특정 용어를 더 깊게(예: DIP가 왜 "역전"인지 화살표 그림으로, 또는 ISP 위반의 구체 피해) 파고들까요, 아니면 아까 멈춘 A/B 조합 결정으로 돌아갈까요?

그래서 위 설명을 토대로 다시 결정을 풀어서 써줘
좋습니다 — 방금 정리한 어휘로 이 결정을 처음부터 다시 풀어서 서술하겠습니다. 옵션 내용은 그대로이고, 각 선택이 어떤 원칙/패턴의 적용인지를 드러내는 게 목적입니다.

문제를 용어로 다시 보기
우리가 끊으려는 render→rr은 레이어링 관점에서 back-edge입니다 — 중간층 render가 최상위 Facade인 rr을 역으로 가리켜, 그래프에 유일하게 남은 cycle(render→rr→sprite→render)을 만듭니다. 이 한 엣지만 없애면 의존 그래프가 DAG가 됩니다.

이 back-edge가 생긴 이유는 render 안의 두 결합 지점이고, 각각 성격이 다른 패턴 문제입니다:

지점① — scene_renderer.cpp:123: Orchestrator인 SceneRenderer가 라이트 송신을 위해 rr(Facade)에서 program 목록을 pull 해서 Dispatcher(LightUniformDispatcher)에 넘깁니다. → "받는 쪽이 출처를 안다" 는 결합.
지점② — render_pipeline.cpp: 파이프라인 조립용 Factory 함수 2개(SetupDefaultPipeline/BuildPostFXChain)가 render에 살면서 rr의 Factory 메서드(CreateProgram/RegisterMesh/CreateSharedMaterial)를 호출합니다. → "앱 조립 코드가 코어에 얹혀 있다" 는 결합.
두 지점은 직교라 따로 푼다는 게 핵심입니다.

지점① — pull을 없애기 = DI의 두 형태
본질은 같습니다: SceneRenderer가 협력자를 스스로 찾지(pull) 말고 밖에서 받게(Dependency Injection) 바꾼다. 다만 "무엇을 받느냐"가 갈립니다.

①-interface = DIP 적용. render가 IProgramRegistry(= program 목록을 주는 Registry의 Interface, 메서드 1개 GetAllPrograms)를 정의하고, rr이 그걸 구현합니다. 그러면 SceneRenderer는 추상만 알고 rr 구상을 모릅니다 — 의존 화살표가 render→rr에서 rr→render로 역전(이게 DIP의 "Inversion"). 주입은 세터 주입(SetProgramRegistry) 한 번, rr이 Singleton이라 간단.
①-push = 추상 없는 DI. 상위(Manager, rr 보유)가 매 프레임 rr.GetAllPrograms()를 가져와 SceneRenderer에 push. Interface를 안 만드니 새 엣지(rr→render)도 0. 대신 클라가 매 프레임 fetch+전달.
한 줄 차이: DIP를 쓰면(①-interface) rr→render 엣지가 새로 생기고, push를 쓰면 안 생깁니다. 이게 아래 매트릭스의 분기점.

지점② — Factory 함수의 거처 = 응집 수술 vs DIP 수술
먼저 당신의 결정2 = "엔진 공유 유틸" 이 여기서 작동합니다 — 이 함수들이 공유 자산이라, 가장 단순한 "클라로 이동"(trivial-C)은 탈락하고 공유 거처 A/B로 좁혀집니다. (공유가 아니었다면 trivial-C로 끝.)

A = 응집(Cohesion) · 레이어링 수술. 두 Factory 함수를 render 밖 새 모듈(render_bootstrap)로 옮깁니다. 근거: 이 함수들의 책임은 "파이프라인 조립"으로, SceneRenderer·스테이지 실행 이라는 render 코어와 관심사가 다릅니다(SRP/응집). 별 모듈로 빼면 render 코어가 한 관심사로 응집↑, 함수가 떠나니 render→rr이 자연 소멸. 비용 = 모듈 +1(텍스처처럼). rr→render는 안 생김(새 모듈이 render·rr를 내려다보는 최상위라 사이클 0).
B = DIP · ISP 수술. 함수는 render에 두고, 인자 ResourceRegistry&(구상)를 render가 정의한 Interface로 교체 → rr이 구현(DIP, 화살표 역전 rr→render). 비용 = 모듈 0 + 인터페이스 1~2개. 단점: render 코어가 rr의 자원생성 API를 추상화 소유 하게 되어, "앱 조립 코드가 코어에 잔류"가 인터페이스 형태로 남습니다.
서브옵션을 패턴으로 매핑
서브	한 줄	적용 패턴
A-i	순수 이동(free function 그대로)	응집/레이어링 최소 수술
A-ii	PipelineBuilder로 래핑	Builder 패턴(프로젝트 fluent 정통)
A-iii	새 모듈이 지점① 주입 와이어링까지 흡수	응집 확장(셋업 단일 거처, SRP 경계 ↑)
B-i	단일 인터페이스 4메서드	DIP O, ISP 위반(SceneRenderer가 Create*까지 봄)
B-ii	IProgramRegistry+IPipelineResourceFactory 분리	DIP + ISP 둘 다 준수
B-iii	②만 인터페이스, ①은 push	DIP(②) + 추상없는 DI(①) 혼합, 인터페이스 최소
rr→render는 "DIP의 부산물"이라는 시각
조합	새 모듈	rr→render	어휘로 본 성격
A-i + ①-interface	+1	생김(① DIP)	레이어링 분리 + DIP, spec target과 일치
A-i + ①-push	+1	0	레이어링 분리 + 추상없는 DI = render·rr 상호 무지(최대 분리)
B-ii + ①-interface	0	생김(①② DIP)	순수 DIP/ISP, 모듈 안 늘림
B-iii	0	생김(② DIP만)	DIP + push 혼합, 인터페이스 표면 최소
핵심 통찰: DIP를 적용한 자리마다 rr→render 화살표가 하나씩 생깁니다(역전의 대가/혜택). 그게 spec이 그린 목표 방향이라 "정상"이지만, 그래프를 더 빡빡하게 분리하고 싶으면 push로 그 역전조차 피하는 길(A-i + ①-push)이 있습니다.

추천 — 같은 내용을 어휘로
A-i + ①-interface 를 기본 추천합니다. 이유를 용어로 말하면: 지점②는 응집/레이어링 수술(파이프라인 조립을 정직한 별 모듈로)로, 지점①은 좁은 1메서드 DIP(Registry 추상)로 — 각 문제에 결이 맞는 도구를 씁니다. 결과 그래프는 spec target(rr→render)과 일치하고, 새 구조물은 모듈 1 + 인터페이스 1로 적정(과설계 아님).

축을 바꾸실 수 있는 지점:

그래프 최소주의(rr이 render를 직접 모르게) → A-i + ①-push.
모듈 안 늘리기(DIP만으로) → B-ii + ①-interface.
이 어휘 틀에서 어느 조합이 가장 납득되시는지 정해주시면, 그 조합으로 설계 섹션(인터페이스 시그니처·모듈 CMake·주입 지점)을 구체화하겠습니다. (구현은 설계 확정 후.)

먼저 ①-push = 추상 없는 DI. 
로 결정한다면
왜냐하면 IProgramRegistry는 오직 하나를 위한 인터페이스인거 같아서 과확장인것 같다 생각했는데.
Program 리스트를 모두 순회해서 가져와야 하는 코드의 빈도가 얼마나 되지?
다른 사례를 봐야할 것 같다.
좋은 직관입니다 — "1개 메서드·1개 구현·1개 소비자" 인터페이스는 다형성 가치가 없는 순수 seam이라, push로 사이클이 깨끗이 끊긴다면 과확장이 맞습니다. 다만 push가 정말 사이클을 끊는지(아니면 엣지를 CameraStage로 옮기기만 하는지)와, "전체 program 순회"가 라이트 말고도 재발하는 패턴인지가 결정적입니다. 데이터로 확인하겠습니다 — 호출 빈도 + program 생성 시점(staleness) + 코드베이스의 narrow interface 선례.

Bash Investigate GetAllPrograms frequency, program creation timing, broadcast patterns, narrow interface precedent
IN
ROOT=/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
echo "=== (1) GetAllPrograms 호출처 (전체 program 순회 빈도) ===" && grep -rHn 'GetAllPrograms' "$ROOT/src" "$ROOT/apps" --include='*.cpp' | grep -v '@c\|///\|//!'
echo ""
echo "=== (2) CreateProgram 호출처 (program 생성 시점 = staleness 판단) ===" && grep -rHn '\.CreateProgram\|->CreateProgram\|reg\.CreateProgram' "$ROOT/src" "$ROOT/apps" --include='*.cpp'
echo ""
echo "=== (3) 그 외 '모든 program 순회/일괄' 패턴 (라이트 외 broadcast 수요?) ==="
grep -rHn 'for.*[Pp]rogram\|GetAll\|EachProgram\|allPrograms\|mPrograms' "$ROOT/src" --include='*.cpp' --include='*.h' | grep -v '@c\|///' | head -30
echo ""
echo "=== (4) 선례: Contracts/ narrow interface 들 + 메서드 수 ===" && ls -1 "$ROOT/apps/_MyApp_/src/"*ontract* "$ROOT/apps/_MyApp_/src/Entity/"*ontract* 2>/dev/null; find "$ROOT/apps/_MyApp_/src" -iname '*contract*' -o -iname 'I*.h' 2>/dev/null | head -20

OUT
=== (1) GetAllPrograms 호출처 (전체 program 순회 빈도) ===
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/scene_renderer.cpp:123:		auto programs = ResourceRegistry::Get().GetAllPrograms();
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/resource_registry.cpp:150:	std::vector<Program *> ResourceRegistry::GetAllPrograms() const

=== (2) CreateProgram 호출처 (program 생성 시점 = staleness 판단) ===
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/render_pipeline.cpp:45:		auto* passthroughProg = reg.CreateProgram(
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/render_pipeline.cpp:98:			auto* prog = reg.CreateProgram(def.Name, def.VertFile, def.FragFile);
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/sprite_resources.cpp:41:				prog = reg.CreateProgram(
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp:116:			auto *skyboxProg = reg.CreateProgram(
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp:161:				prog = reg.CreateProgram("simple_texture",
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp:72:				prog = reg.CreateProgram("simple_texture",
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/Stage/StageBuilder.cpp:76:			return reg.CreateProgram(kPhongAlbedoProgKey, kPhongAlbedoVS, kPhongAlbedoFS);
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/Stage/StageBuilder.cpp:97:			return reg.CreateProgram(kTransparentProgKey, kTransparentVS, kTransparentFS);
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/HUD/HealthBarFactory.cpp:48:			prog = reg.CreateProgram(kProgramKey, kVsPath, kFsPath);

=== (3) 그 외 '모든 program 순회/일괄' 패턴 (라이트 외 broadcast 수요?) ===
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/device_context.h:26: *  | Uniform location 조회 | Program 직접      | `prog->GetLocation("uModel")`          |
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/screen_quad_stage.cpp:83:			Uniforms::SetInt(mProgram, "uScene", 0);
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/light_uniform_dispatcher.cpp:3: * @brief LightUniformDispatcher::Dispatch 구현 - Program 순회 + lighting sentinel + 일괄 uniform 송신.
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/light_uniform_dispatcher.cpp:76:	void LightUniformDispatcher::Dispatch(const std::vector<Program *> &programs,
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/light_uniform_dispatcher.cpp:90:		for (const auto *prog : programs)
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/mesh_pass_processor.cpp:155:                    Uniforms::SetMat4(*program, Const::UNI_VIEW, viewMat);
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/mesh_pass_processor.cpp:157:                    Uniforms::SetMat4(*program, Const::UNI_PROJ, projMat);
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/mesh_pass_processor.cpp:175:                Uniforms::SetMat4(*program, Const::UNI_MODEL, cmd.modelMatrix);
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/render/scene_renderer.cpp:123:		auto programs = ResourceRegistry::Get().GetAllPrograms();
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/resource_registry.cpp:128:		if (mPrograms.find(key) != mPrograms.end())
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/resource_registry.cpp:140:		auto insertedIt = mPrograms.emplace(key, std::move(program)).first;
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/resource_registry.cpp:146:		auto it = mPrograms.find(key);
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/resource_registry.cpp:147:		return (it != mPrograms.end()) ? it->second.get() : nullptr;
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/resource_registry.cpp:150:	std::vector<Program *> ResourceRegistry::GetAllPrograms() const
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/resource_registry.cpp:153:		out.reserve(mPrograms.size());
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/resource_registry.cpp:154:		for (const auto &[key, prog] : mPrograms)
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/resource_registry.cpp:313:		mPrograms.clear();
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/resource_registry.h:116:		std::vector<Program *> GetAllPrograms() const;
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/resource_registry/resource_registry.h:182:		std::unordered_map<std::string, ProgramUPtr> mPrograms;
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/common/constants.h:270:	// --- uniform struct 멤버 suffix (program_uniforms 가 prefix 와 결합) ---
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/diagnostics/uniform_diagnostics.cpp:32:    void UniformDiagnostics::NotifyMissing(GLuint program, const char *name)
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/diagnostics/uniform_diagnostics.cpp:36:            spdlog::warn("프로그램 {}에 uniform 누락: '{}'", program, name);
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/diagnostics/uniform_diagnostics.cpp:40:    void UniformDiagnostics::NotifyTypeMismatch(GLuint program, const char *name,
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/diagnostics/uniform_diagnostics.cpp:55:    void UniformDiagnostics::Invalidate(GLuint program)
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/diagnostics/gl_log.cpp:137:        const auto it = detail::uniformChecked.find(program);
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/diagnostics/gl_log.cpp:144:            if (glGetUniformLocation(program, name) < 0)
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/diagnostics/gl_log.cpp:152:                spdlog::warn("프로그램 {}에 기대 uniform 누락: [{}]", program, fmt::join(missing, ", "));
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/diagnostics/gl_log.cpp:158:        detail::uniformChecked[program] = allFound;
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/diagnostics/gl_log.cpp:194:        detail::uniformChecked.erase(program);
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/src/diagnostics/gl_validate.cpp:315:            glGetActiveUniform(program, static_cast<GLuint>(i), maxNameLen,

=== (4) 선례: Contracts/ narrow interface 들 + 메서드 수 ===
(eval):9: no matches found: /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/Entity/*ontract*
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/Contracts
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/UI/ImGuiLayerStack.h
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/UI/IImGuiLayer.h
/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/apps/_MyApp_/src/Contracts/EntityContracts.h
데이터가 명확합니다. EntityContracts(클라 사이클 절단 때 만든 인터페이스들)가 narrow interface 선례인지 확인하려 읽겠습니다 — 사용자의 "다른 사례" 질문에 직접 답.

Read EntityContracts.h
데이터가 당신 직관을 거의 뒷받침합니다. 두 질문에 정면으로 답하겠습니다.

Q1. "전체 program 순회" 빈도 — 매우 낮음 (사실상 1곳)
무엇	어디	빈도
전체 program 순회 (GetAllPrograms)	scene_renderer.cpp:123 → LightUniformDispatcher	소비자 1개, 카메라당 1회/프레임
그 외 글로벌 uniform(view/proj/model)	mesh_pass_processor.cpp:155-175	per-draw, 현재 그려지는 program 1개에만
핵심: view/proj/model 같은 다른 글로벌은 "모든 program"이 아니라 그 드로우의 active program 하나에만 씁니다. "전체 program 순회"가 필요한 건 오직 라이트 broadcast 하나 — 두 번째 소비자가 없습니다. 즉 재발하는 패턴이 아니라 라이트 전용 일회성이라, 이걸 추상화(인터페이스)로 일반화하는 건 speculative가 맞습니다.

Q2. 다른 사례 — 코드베이스는 1-메서드 인터페이스를 이미 많이 씀
Contracts/EntityContracts.h 메서드 수:

인터페이스	메서드	용도
IDieable / IDamageable / IMovable / IImpulsable	각 1개	능력 분리
ITimerOwner / IImpulseState / IFireTrigger	각 1개	C1/C2 사이클 절단용 DIP seam ← IProgramRegistry와 구조 동일
ILivable / IAttackable / IPlayerCommand	2~3개	
그래서 "1-메서드 인터페이스 = 과확장"이라는 일반 명제는 이 코드베이스에선 성립 안 합니다 — IFireTrigger(1메서드, 구현 1=PlayerHands, 소비 1=InputHandler)는 가상의 IProgramRegistry(1메서드, 구현 1=rr, 소비 1=SceneRenderer)와 판박이. 집style상 IProgramRegistry는 이상한 게 아닙니다.

그런데 결정적 차이가 있습니다:

무엇을 역전?	push 가능?
IFireTrigger 등 클라 시임	행위 호출(callback) — InputHandler가 PlayerHands를 호출	❌ 불가 — 호출은 클릭에 반응해 일어남. 미리 못 밀어넣음 → 인터페이스가 유일 한 seam
GetAllPrograms (지점①)	데이터 접근 — SceneRenderer가 목록을 읽음	✅ 가능 — 데이터는 필요 전에 건네줄 수 있음
→ 행위 콜백은 인터페이스가 사실상 강제, 데이터 접근은 push라는 더 싼 대안이 있음. 그래서 당신 직관("이건 push로 충분")은 단순 미니멀리즘이 아니라 원칙적으로 맞습니다 — 지점①은 데이터 접근이라 push가 적격.

한 가지 검증 — staleness (program이 런타임에 늘어나나?)
CreateProgram 호출처는 전부 빌드/셋업 단계(Bootstrap/Stage/HUD/pipeline). 런타임 스폰(적)도 EnemyBuilder.cpp:72가 "simple_texture" 기존 키 재사용(PlayerBuilder가 init에 이미 생성)이라 새 키가 런타임에 안 늘어남. 즉 program 집합은 사실상 init-stable.
→ 결론: push-per-frame(클라 렌더 루프에서 SceneRenderer().SetActivePrograms(reg.GetAllPrograms()) 한 줄)이면 미래에 런타임 셰이더가 생겨도 robust하고, 비용은 프레임당 ~10포인터 vector 하나(무시 가능).

평결 — ①-push 지지
당신 판단대로 ①-push 가 이 경우 맞습니다:

"전체 순회"는 소비자 1개·재발 없음 → 추상화는 speculative.
데이터 접근이라 push라는 싼 대안 존재(행위 콜백과 달리).
인터페이스도, 새 rr→render 엣지도 안 만듦 → render와 rr 최대 분리.
유일한 비용 = temporal coupling: 클라가 매 프레임 스테이지 실행 전에 SetActivePrograms를 불러줘야 함(잊으면 라이트 무업로드 — 단 GUI에서 즉시 보이고 dispatcher의 lighting-sentinel이 빈 목록을 안전 처리). 렌더 루프 한 곳이라 실무상 위험 낮음.

그럼 지점① = ①-push로 확정하겠습니다. 남은 건 지점② (A 신규모듈 / B DIP인터페이스) 인데, 여기선 push가 안 통합니다(②는 행위가 아니라 Factory 함수의 거처 문제라 push 무관). 지점②도 마저 정하실까요, 아니면 ①처럼 먼저 더 파볼까요?

GetAllPrograms를 접근하는 인터페이스를 만약 만들게 된다면
대체 어떤 특수 객체, 기능들이 추후 확장되면 이 인터페이스를 구현하게 될지 한번 
모범 렌더링엔진 Unity, Cocos, Unreal, Godot을 기준으로 /context7-mcp 으로 분석하여
많아도 최대 큰 축 5가지 정도만 모듈을 말해봐

왜냐하면 내 생각은 Shader Programs는 오직 렌더링 모듈에서만 접근하고 말 것 같아서.
좋은 검증 질문입니다 — "IProgramRegistry를 만든다면 미래에 누가 구현/소비하나"를 모범 엔진 기준으로 보면, 인터페이스의 정당성(다중 구현자 존재 여부)이 판가름납니다. 당신 가설("셰이더 프로그램은 렌더 모듈에서만 접근")을 context7로 교차검증하겠습니다. 먼저 context7 도구를 로드합니다.

context7 도구 로드 완료. 4개 엔진 중 셰이더-전역-접근 문서가 풍부한 3종(Unreal/Godot/Unity)을 resolve합니다(Cocos는 호출 예산상 본 프로젝트가 미러링하는 Cocos ProgramCache 지식 + 코드 주석으로 보완). 병렬 resolve:

Context7 [resolve-library-id]which subsystems access the global shade…
OUT
Available Libraries:



Title: Unreal Engine 5.7

Context7-compatible library ID: /websites/dev_epicgames_en-us_unreal-engine

Description: Unreal Engine is a powerful real-time 3D creation tool for developing games, visualizations, and interactive experiences across multiple platforms, supporting visual scripting, C++ programming, and various content creation workflows.

Code Snippets: 80411

Source Reputation: High

Benchmark Score: 66.28




Title: Unreal Engine 5.7 Documentation

Context7-compatible library ID: /websites/dev_epicgames_unreal-engine

Description: Comprehensive official documentation for Unreal Engine 5.7, covering game development, blueprints, C++ programming, rendering, animation, gameplay systems, and more.

Code Snippets: 145103

Source Reputation: High

Benchmark Score: 50.05




Title: Unreal Engine 5.8

Context7-compatible library ID: /websites/dev_epicgames_unreal-engine_unreal-engine-5-8-documentation

Description: This documentation provides complete resources for learning to use Unreal Engine 5.8, covering various aspects of game development and interactive application creation.

Code Snippets: 21

Source Reputation: High

Benchmark Score: 1




Title: Unreal Engine 5.7

Context7-compatible library ID: /websites/dev_epicgames_unreal-engine_5_7

Description: Unreal Engine 5.7 is a comprehensive game development and real-time 3D creation platform offering tools for graphics rendering, visual effects, gameplay systems, animation, and cross-platform deployment.

Code Snippets: 20

Source Reputation: High

Benchmark Score: 0.5




Title: Unreal Engine 5.7 Documentation

Context7-compatible library ID: /websites/dev_epicgames_unreal-engine_unreal-engine-5-7-documentation

Description: Complete documentation and resources for learning to use Unreal Engine 5.7, covering features, tools, and development practices.

Code Snippets: 22

Source Reputation: High

Benchmark Score: 1.33

Context7 [resolve-library-id]RenderingServer global shader parameters…
OUT
Available Libraries:



Title: Godot Engine

Context7-compatible library ID: /godotengine/godot-docs

Description: This repository contains the source files for the Godot Engine's documentation, written in reStructuredText and built with Sphinx.

Code Snippets: 33575

Source Reputation: High

Benchmark Score: 77.12

Versions: __branch__4.5




Title: Godot Engine

Context7-compatible library ID: /websites/godotengine_en_4_6

Description: Godot Engine is a free and open-source, all-inclusive game engine that provides a comprehensive set of tools for developing 2D and 3D games across multiple platforms.

Code Snippets: 133977

Source Reputation: High

Benchmark Score: 61.73




Title: Godot Engine

Context7-compatible library ID: /websites/godotengine_en_stable

Description: Godot Engine is a free, open-source game engine that supports 2D and 3D game development with features including rendering, physics, scripting, animation, and cross-platform export.

Code Snippets: 324954

Source Reputation: High

Benchmark Score: 66.19




Title: Godot Engine

Context7-compatible library ID: /websites/godotengine_en_4_5

Description: Godot Engine is a free and open-source game engine that provides a comprehensive suite of tools for developing 2D and 3D games across various platforms.

Code Snippets: 70779

Source Reputation: High

Benchmark Score: 63.39




Title: Godot Engine

Context7-compatible library ID: /websites/godotengine_en_4_4

Description: Godot Engine is a free and open-source game engine providing a comprehensive set of tools for creating 2D and 3D games and applications across various platforms.

Code Snippets: 128625

Source Reputation: High

Benchmark Score: 59.2

Context7 [resolve-library-id]global shader properties Shader.SetGloba…
OUT
Available Libraries:



Title: Unity Documentation

Context7-compatible library ID: /websites/unity

Description: The official documentation for Unity, a powerful real-time 3D development platform used for creating games, apps, and experiences. It covers the Unity Engine, Asset Store, and Unity Gaming Services.

Code Snippets: 22734

Source Reputation: High

Benchmark Score: 22.15




Title: Unity

Context7-compatible library ID: /websites/unity3d_manual

Description: Unity is a cross-platform game engine and development platform for creating 2D, 3D, and immersive interactive experiences.

Code Snippets: 31353

Source Reputation: High

Benchmark Score: 69.67




Title: Unity

Context7-compatible library ID: /websites/unity_en-us

Description: Unity is a comprehensive real-time 3D development platform used to create 2D and 3D games, apps, and experiences, supported by a full ecosystem of tools and services.

Code Snippets: 49497

Source Reputation: High

Benchmark Score: 13.24




Title: Unity Graphics

Context7-compatible library ID: /unity-technologies/graphics

Description: The Scriptable Render Pipeline (SRP) is a Unity feature that provides artists and developers with tools to create high-fidelity graphics, offering the Universal Render Pipeline (URP) and High Definition Render Pipeline (HDRP).

Code Snippets: 1245

Source Reputation: High

Benchmark Score: 58.62




Title: Unity Entities

Context7-compatible library ID: /websites/unity3d_packages_com_unity_entities_1_4

Description: The Unity Entities package is part of Unity's Data-Oriented Technology Stack (DOTS), providing a data-oriented implementation of the Entity Component System (ECS) architecture for high-performance game development.

Code Snippets: 10844

Source Reputation: High

Benchmark Score: 78.67

3종 모두 고품질 라이브러리 확보(Godot 77.12 / Unity manual 69.67 / Unreal 66.28). 핵심 질문 — "전역 셰이더 파라미터 broadcast + 전체 셰이더 집합을 접근하는 서브시스템"을 병렬 query합니다.

Context7 [query-docs]Which subsystems iterate or access ALL s…
OUT
GetGlobalShaderMap(EShaderPlatform)

Source: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/RenderCore/GetGlobalShaderMap


Accesses the global shader map for a specific shader platform. This is a global FGlobalShaderMap which contains an instance of each global shader type.



## GetGlobalShaderMap(EShaderPlatform)

### Description
Accesses the global shader map. This is a global FGlobalShaderMap which contains an instance of each global shader type.

### Method
GET

### Endpoint
`/websites/dev_epicgames_en-us_unreal-engine/api/runtime/rendercore/shadermap

### Parameters
#### Query Parameters
- **Platform** (EShaderPlatform) - Required - Specifies the shader platform for which to retrieve the global shader map.
- **bRefreshShaderMap** (boolean) - Optional - If true, the existing global shader map will be discarded and a new one will be created.

### Response
#### Success Response (200)
- **shaderMap** (FGlobalShaderMap *) - A reference to the global shader map.

#### Response Example
```json
{
  "shaderMap": {
    "//": "Represents the FGlobalShaderMap object"
  }
}



--------------------------------

### Access Global Shader Map by Shader Platform (C++)

Source: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/RenderCore/GetGlobalShaderMap

This C++ function retrieves the global shader map for a specified shader platform. It is part of the RenderCore module and requires including the GlobalShader.h header.

```cpp
FGlobalShaderMap * GetGlobalShaderMap(
    EShaderPlatform Platform
)


Access Global Shader Map by Feature Level (C++)

Source: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/RenderCore/GetGlobalShaderMap


This C++ function is an overload for GetGlobalShaderMap that accepts a feature level. It translates the feature level to the appropriate shader platform to access the global shader map. This is useful for platform-independent shader map retrieval.



FGlobalShaderMap * GetGlobalShaderMap(
    ERHIFeatureLevel::Type FeatureLevel
)


GetGlobalShaderMap (Feature Level)

Source: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/RenderCore


An overload of GetGlobalShaderMap that accepts a feature level and translates it to the appropriate shader platform. This allows for retrieving the shader map based on hardware capabilities.



FGlobalShaderMap * GetGlobalShaderMap ( ERHIFeatureLevel::Type FeatureLevel  
) | Overload for the above GetGlobalShaderMap which takes a feature level and translates to the appropriate shader platform | GlobalShader.h |   


C++ Global Shader Implementation: FLensDistortionUVGenerationShader

Source: https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-shaders-in-plugins-unreal-engine


Demonstrates how to define a global shader in C++ by inheriting from FGlobalShader. It includes methods for checking platform compatibility (ShouldCache), modifying compilation environments (ModifyCompilationEnvironment), constructor for parameter binding, serialization, and setting shader parameters from C++.



class FLensDistortionUVGenerationShader : public FGlobalShader
{
public:
	// This function determines whether or not the shader should be compiled for a given platform.
	// In this case, the shader will not be usable without SM4 support.
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	// Compile-time constants for shader can be defined in this function:
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GRID_SUBDIVISION_X"), kGridSubdivisionX);
		OutEnvironment.SetDefine(TEXT("GRID_SUBDIVISION_Y"), kGridSubdivisionY);
	}

	// Default constructor.
	FLensDistortionUVGenerationShader() {}

	// Constructor using an initializer object. We can bind parameters here so that our C++ code
	// can interface with USF, enabling us to set shader parameters from code.
	FLensDistortionUVGenerationShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PixelUVSize.Bind(Initializer.ParameterMap, TEXT("PixelUVSize"));
		RadialDistortionCoefs.Bind(Initializer.ParameterMap, TEXT("RadialDistortionCoefs"));
		TangentialDistortionCoefs.Bind(Initializer.ParameterMap, TEXT("TangentialDistortionCoefs"));
		DistortedCameraMatrix.Bind(Initializer.ParameterMap, TEXT("DistortedCameraMatrix"));
		UndistortedCameraMatrix.Bind(Initializer.ParameterMap, TEXT("UndistortedCameraMatrix"));
		OutputMultiplyAndAdd.Bind(Initializer.ParameterMap, TEXT("OutputMultiplyAndAdd"));
	}

	// All members must be serialized here. This function is run at load and save time, and is used
	// to put the shader into the DDC and pak files.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PixelUVSize << RadialDistortionCoefs << TangentialDistortionCoefs << DistortedCameraMatrix << UndistortedCameraMatrix << OutputMultiplyAndAdd;
		return bShaderHasOutdatedParameters;
	}

	// This function is an example of how shader parameters can be precomputed based on data specific
	// to the shader. In this case, the shader requires several matrices that can be computed from
	// a few parameters, and that would be inefficient to compute inside the shader itself. Note that
	// this function is not an override; it is custom to this class, and is called when this feature's
	// specific implementation requires it.
	template<typename TShaderRHIParamRef>
	void SetParameters(
		FRHICommandListImmediate& RHICmdList,
		const TShaderRHIParamRef ShaderRHI,
		const FCompiledCameraModel& CompiledCameraModel,
		const FIntPoint& DisplacementMapResolution)
	{
		FVector2D PixelUVSizeValue(
			1.f / float(DisplacementMapResolution.X), 1.f / float(DisplacementMapResolution.Y));
		FVector RadialDistortionCoefsValue(
			CompiledCameraModel.OriginalCameraModel.K1,
			CompiledCameraModel.OriginalCameraModel.K2,
			CompiledCameraModel.OriginalCameraModel.K3);
		FVector2D TangentialDistortionCoefsValue(
			CompiledCameraModel.OriginalCameraModel.P1,
			CompiledCameraModel.OriginalCameraModel.P2);

		SetShaderValue(RHICmdList, ShaderRHI, PixelUVSize, PixelUVSizeValue);
		SetShaderValue(RHICmdList, ShaderRHI, DistortedCameraMatrix, CompiledCameraModel.DistortedCameraMatrix);
		SetShaderValue(RHICmdList, ShaderRHI, UndistortedCameraMatrix, CompiledCameraModel.UndistortedCameraMatrix);
		SetShaderValue(RHICmdList, ShaderRHI, RadialDistortionCoefs, RadialDistortionCoefsValue);
		SetShaderValue(RHICmdList, ShaderRHI, TangentialDistortionCoefs, TangentialDistortionCoefsValue);
		SetShaderValue(RHICmdList, ShaderRHI, OutputMultiplyAndAdd, CompiledCameraModel.OutputMultiplyAndAdd);
	}

private:
	// Shader parameters.
	FShaderParameter PixelUVSize;
	FShaderParameter RadialDistortionCoefs;
	FShaderParameter TangentialDistortionCoefs;
	FShaderParameter DistortedCameraMatrix;
	FShaderParameter UndistortedCameraMatrix;
	FShaderParameter OutputMultiplyAndAdd;
};

// This macro exposes the shader to the Engine. Note the absolute virtual source file path.
IMPLEMENT_SHADER_TYPE(, FLensDistortionUVGenerationVS, TEXT("/Plugin/LensDistortion/Private/UVGeneration.usf"), TEXT("MainVS"), SF_Vertex)

Context7 [query-docs]Global shader uniforms broadcast to all …
OUT
Set Global Shader Parameter at Runtime in GDScript

Source: https://github.com/godotengine/godot-doc/blob/master/tutorials/shaders/shader_reference/shading_language.md


Shows how to modify the value of a global uniform at runtime using the RenderingServer API in GDScript. This operation has no performance impact as it doesn't require CPU-GPU synchronization. The global uniform must already exist in Project Settings.



RenderingServer.global_shader_parameter_set("my_color", Color(0.3, 0.6, 1.0))


Declare and Use Global Uniform in GLSL Shader

Source: https://github.com/godotengine/godot-doc/blob/master/tutorials/shaders/shader_reference/shading_language.md


Demonstrates how to declare a global uniform in a Godot shader using GLSL syntax and access it in the fragment function. The global uniform must be pre-defined in Project Settings before the shader is saved, or compilation will fail. Default values in shader code are ignored; the actual value comes from Project Settings.



shader_type canvas_item;

global uniform vec4 my_color;

void fragment() {
    COLOR = my_color.rgb;
}

Method Descriptions

Source: https://github.com/godotengine/godot-doc/blob/master/classes/class_renderingserver.md


The RenderingServer allows adding, setting, removing, and overriding global shader uniforms. Global shader parameter names are case-sensitive. Overriding a global shader uniform is equivalent to using the ShaderGlobalsOverride node.



Shaders > Global Uniforms > Concept

Source: https://github.com/godotengine/godot-doc/blob/master/tutorials/shaders/shader_reference/shading_language.md


Global uniforms are uniforms that are global to all shaders in the entire project, distinguishing them from regular uniforms which are only scoped to individual shaders. While regular uniforms are defined outside of shader functions and exist in the global scope of a single shader, global uniforms exist in the global scope across all shaders in the project. This distinction is important for understanding how shader data is shared and managed at the project level.



Shaders > Global Uniforms > Runtime Modification

Source: https://github.com/godotengine/godot-doc/blob/master/tutorials/shaders/shader_reference/shading_language.md


Global uniform values can be changed at runtime using the RenderingServer.global_shader_parameter_set() method in a script. This allows dynamic modification of shader parameters without recompiling shaders. Assigning global uniform values can be done as many times as desired without impacting performance, since setting data doesn't require synchronization between the CPU and GPU. Additionally, you can add or remove global uniforms at runtime using RenderingServer.global_shader_parameter_add() and RenderingServer.global_shader_parameter_remove() methods, though these operations have a performance cost compared to simply updating existing values.

Context7 [query-docs]Global shader properties Shader.SetGloba…
OUT
Edit Always Included Shaders list via C#

Source: https://doc.unity3d.com/Manual/web-optimization-graphics.html


Use this C# script to dynamically update the 'Always included shaders' list in Graphics Settings. Assign a new list of shaders to GraphicsSettings.alwaysIncludedShaders.ToArray().



GraphicsSettings.alwaysIncludedShaders = newShadersList.ToArray();



Set Global Texture in URP Render Graph

Source: https://doc.unity3d.com/Manual/urp/render-graph-create-global-texture.html


Allocate a global shader texture and set it using SetGlobalTextureAfterPass within the RecordRenderGraph method. This makes the texture available to all shaders and render passes.



// Allocate a global shader texture called _GlobalTexture
private int globalTextureID = Shader.PropertyToID("_GlobalTexture")

using (var builder = renderGraph.AddRasterRenderPass<PassData>("MyPass", out var passData)){

    // Set a texture to the global texture
    builder.SetGlobalTextureAfterPass(texture, globalTextureID);
}



Declare ShaderLab material properties

Source: https://doc.unity3d.com/Manual/SL-PropertiesInPrograms.html


Define material properties like color, vector, float, texture, and cubemap in a ShaderLab Properties block. These properties can then be accessed in Cg/HLSL code.



_MyColor ("Some Color", Color) = (1,1,1,1) 
_MyVector ("Some Vector", Vector) = (0,0,0,0) 
_MyFloat ("My float", Float) = 0.5 
_MyTexture ("Texture", 2D) = "white" {}
_MyCubemap ("Cubemap", CUBE) = "" {}


Access ShaderLab properties in Cg/HLSL

Source: https://doc.unity3d.com/Manual/SL-PropertiesInPrograms.html


Declare Cg/HLSL variables with the same names and matching types as your ShaderLab properties to access their values within shader programs. Low precision types are often sufficient for colors.



half4 _MyColor; // low precision type is usually enough for colors
float4 _MyVector;
float _MyFloat;
sampler2D _MyTexture;
samplerCUBE _MyCubemap;

Adding material properties to shaders > How property values are provided to shaders

Source: https://doc.unity3d.com/Manual/writing-shader-material-properties.html


Shader property values are supplied to shaders from per-renderer values set in MaterialPropertyBlock (for per-instance data), values set in the Material asset, and global shader properties set by Unity or scripts. The order of precedence is MaterialPropertyBlock, then Material, then global properties. If a property is not defined in any of these places, a default value is used.

4개 엔진 교차검증 완료. context7가 실제 API로 뒷받침한 결과 + 당신 가설 검증입니다.

context7가 보여준 것 (엔진별 근거)
엔진	"전체 셰이더" 접근 메커니즘 (context7 출처)
Unreal	GetGlobalShaderMap() → FGlobalShaderMap(= "각 global shader type의 인스턴스" 보유). bRefreshShaderMap=핫리로드. RenderCore 모듈 소관
Godot	RenderingServer.global_shader_parameter_set/add/remove (= "프로젝트 전 셰이더에 global") + ShaderGlobalsOverride 씬 노드로 override
Unity	Shader.SetGlobalVector/Float/Texture(전 셰이더), GraphicsSettings.alwaysIncludedShaders, ShaderVariantCollection warmup, URP SetGlobalTextureAfterPass. 우선순위 MaterialPropertyBlock > Material > global
"전체 셰이더 집합"을 만지는 큰 축 5가지
#	축	엔진 근거	성격
1	렌더 파이프라인 코어 (프레임당 전역 broadcast: light/view/fog/shadow)	Unreal RenderCore·Unity URP global texture·Godot RenderingServer	런타임/렌더
2	전역 셰이더 파라미터 시스템 (gameplay·씬이 set-global)	Godot global_shader_parameter+ShaderGlobalsOverride·Unity Shader.SetGlobalX	런타임/게임플레이
3	셰이더 컴파일·핫리로드·변형 관리	Unreal bRefreshShaderMap·Unity ShaderVariantCollection	에디터/에셋
4	빌드·로딩 자원 인클루전·워밍	Unity GraphicsSettings.alwaysIncludedShaders+warmup	빌드/로딩
5	진단·디버그·프로파일링 (frame debugger, shader stats)	(엔진 일반 — context7 직접근거 약함, 내 지식)	툴링
결정적 발견 — 모든 엔진이 "넣기(push), 안 꺼냄(pull)"
context7에서 일관된 패턴: 렌더 바깥 시스템(gameplay·씬·스크립트)은 SetGlobal(name, value) 로 값을 밀어넣지, "전체 program 목록을 꺼내오지 않습니다. Godot global_shader_parameter_set, Unity Shader.SetGlobalX 둘 다 세터입니다. "전체 셰이더 순회(iterate-all)"는 RenderingServer/RenderCore 내부에만 머뭅니다. 즉 정통 설계에서 GetAllPrograms 같은 목록 추출은 모듈 경계를 넘지 않습니다.

→ 이게 정확히 당신의 ①-push 직관이고, 4대 엔진이 전부 그렇게 합니다.

우리 프로젝트에 매핑 — 당신 가설은 맞습니다
축	우리 엔진에 존재?	iterate-all 소비?
1 렌더 코어	✅ SceneRenderer 라이트 디스패치	예 — 유일
2 전역 파라미터	△ 현재 라이트 디스패처에 흡수(게임플레이 전역-셰이더 시스템 없음)	생기더라도 정통은 SetGlobal 세터(render 내부 순회) — GetAll 노출 불필요
3 컴파일/핫리로드/변형	❌ 에디터·핫리로드·변형 없음(코스워크 OpenGL)	—
4 빌드/워밍	❌ PSO·변형 프리컴파일 없음	—
5 진단	✅ diagnostics 존재하나 per-program(NotifyMissing(program,…)), iterate-all 아님	아니오
결론: 5축 중 우리 엔진에 실재하며 iterate-all을 소비하는 건 #1 렌더 코어 하나뿐. #3·#4는 코스워크 엔진에 없고 안 생기며, #2는 (생겨도) 정통상 세터라 GetAll을 모듈 밖으로 노출 안 함, #5는 per-program이라 무관.

→ 따라서 IProgramRegistry는 구현자 ≈ 1(rr 단독) + 소비자 ≈ 1(render), 사실상 프로젝트 수명 내내 그대로입니다. 다형성·다중소비 가치가 없으니 speculative 인터페이스가 맞고, ①-push가 정답입니다. 게다가 push는 4대 엔진의 정통 패턴(값을 밀어넣기)과도 일치합니다.

①-push 확정으로 정리됩니다. 남은 **지점②(A 신규모듈 / B DIP인터페이스)**로 갈까요? (참고: ②는 행위/거처 문제라 이 push 논리가 안 걸리고, 별도로 A/B를 정해야 합니다.)