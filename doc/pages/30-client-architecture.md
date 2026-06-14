# Client 의존 그래프 (`apps/_MyApp_`) {#client-architecture}

탑다운 슈터 Client(`apps/_MyApp_/src/`) 서브시스템 간 + Client→SJH 엔진 모듈(`src/`) **실제 인스턴스 의존**만 표시한다.

> **노이즈 제외 규칙** — `A → B` 엣지를 그리지 *않는* 경우:
> ① **static 전용 클래스** 접근 (`diagnostics`, `Director::Get()`·`ResourceRegistry::Get()` 의 static 진입은 제외하되 반환 인스턴스의 비-static 멤버 호출은 KEEP)
> ② **namespace free function** (`Uniforms::Set*`, `Geometry::*`, `Physics::RaycastAll`, `Spawns::Spawn*`)
> ③ **Constant / Constexpr / enum 값** (`common/constants.h`, `Entity::ULTIMATE_*`, `Pass::Kind::*`, `PhysicsLayer::*`)
> ④ **forward-decl / 죽은 include / 포인터 passthrough** (멤버 호출·생성 없이 포인터만 전달)
>
> `A → B` = A 가 B 의 클래스 인스턴스를 소유·생성·보유하거나, 비-static 멤버 호출/상속. (4개 분석 에이전트가 `apps/_MyApp_/src/**` 코드를 읽어 분류.)

## 그래프

\dot
digraph ClientDeps {
  rankdir=TB;
  node [shape=box, style=rounded, fontname="Helvetica", fontsize=10];

  // ===== Client (apps/_MyApp_/src) =====
  subgraph cluster_client {
    label="Client 게임 코드 (apps/_MyApp_/src)"; labelloc=t;
    style=filled; fillcolor="#fffaf0";
    Manager [label="Manager\n(루트 owner)", style="rounded,filled", fillcolor="#ffe1a8"];
    Bootstrap; Stage; Entity; Physics; Spawns;
    VFX; Audio; Tween; HUD; UI; InputHandler;
    Algebraic [label="Algebraic\n(Stat — leaf)"];
    Playable  [label="Playable\n(client FX)"];
    Text      [label="Text\n(client)"];
  }

  // ===== Engine sink (src/) — Client 가 사용하는 모듈만 =====
  subgraph cluster_engine {
    label="SJH 엔진 모듈 (src/) — Client 사용분만 (모듈 내부 의존은 mainpage 참조)"; labelloc=b;
    style=filled; fillcolor="#eef3fb";
    node [style="rounded,filled", fillcolor="#d6e3f7"];
    scene; render; resource_registry; material; object;
    sprite; playable; timer; fsm; input; text;
  }

  // ── Client ↔ Client 상호 의존 (Entity 허브) — 빨강 양방향 ──
  edge [color="#c0392b", dir=both, penwidth=1.4];
  Entity -> Physics;
  Entity -> InputHandler;
  Entity -> Spawns;
  Entity -> Playable;

  // ── Client → Client (오케스트레이션 흐름) — 진회색 ──
  edge [color="#34495e", dir=forward, penwidth=1.0];
  Manager -> Audio; Manager -> VFX; Manager -> Physics; Manager -> Text;
  Bootstrap -> Manager; Bootstrap -> Entity; Bootstrap -> Audio; Bootstrap -> Spawns;
  Bootstrap -> Playable; Bootstrap -> Physics; Bootstrap -> Tween; Bootstrap -> HUD; Bootstrap -> InputHandler;
  Stage -> Bootstrap; Stage -> Manager; Stage -> Entity; Stage -> Physics;
  Stage -> VFX; Stage -> Audio; Stage -> UI; Stage -> Tween;
  Entity -> Algebraic; Entity -> Tween;
  Physics -> Algebraic;
  Spawns -> Physics; Spawns -> VFX; Spawns -> Audio; Spawns -> Tween;
  HUD -> Entity;

  // ── Client → Engine (엔진 API 인스턴스 사용) — 연회색 ──
  edge [color="#aebccc", dir=forward, penwidth=0.7];
  Manager -> render;
  Bootstrap -> scene; Bootstrap -> render; Bootstrap -> resource_registry; Bootstrap -> material; Bootstrap -> object;
  Stage -> scene; Stage -> render; Stage -> resource_registry; Stage -> material; Stage -> object; Stage -> fsm; Stage -> timer;
  Entity -> scene; Entity -> object; Entity -> material; Entity -> resource_registry; Entity -> timer; Entity -> sprite;
  Physics -> scene; Physics -> object; Physics -> timer;
  Spawns -> scene; Spawns -> playable; Spawns -> text; Spawns -> timer; Spawns -> resource_registry;
  VFX -> playable; VFX -> render; VFX -> scene; VFX -> resource_registry;
  Audio -> playable; Audio -> resource_registry;
  Tween -> playable;
  HUD -> scene; HUD -> material; HUD -> resource_registry; HUD -> render;
  UI -> resource_registry; UI -> render; UI -> material;
  InputHandler -> input; InputHandler -> scene; InputHandler -> timer; InputHandler -> object;
  Playable -> playable; Playable -> scene; Playable -> sprite; Playable -> resource_registry; Playable -> material;
  Text -> text; Text -> resource_registry;
}
\enddot

**범례**

| 표기 | 의미 |
|---|---|
| 빨강 ↔ (굵게) | Client 상호 의존 — **응집 핫스팟** (`Entity` 허브) |
| 진회색 → | Client → Client 오케스트레이션 (소유/생성/상속) |
| 연회색 → | Client → 엔진 모듈 (인스턴스 사용) |
| `Manager` (주황) | 루트 owner — `SceneRenderer` + `Audio/VFX/Physics/Text` 시스템 value 보유 |
| `Algebraic` | `Stat` 값 클래스 leaf (다른 Client 가 멤버로 보유, 자체 outgoing 없음) |

## Client × Engine 사용 행렬 (KEEP 만)

행=Client 서브시스템, 열=엔진 모듈. ● = 인스턴스 의존 (위 그래프의 연회색 엣지와 동일). 빈 칸 = 의존 없음 또는 노이즈(static/const/free-func)로 제외.

| Client \ Engine | scene | render | resrc_reg | material | object | sprite | playable | timer | fsm | input | text |
|---|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| Manager      |   | ● |   |   |   |   |   |   |   |   |   |
| Bootstrap    | ● | ● | ● | ● | ● |   |   |   |   |   |   |
| Stage        | ● | ● | ● | ● | ● |   |   | ● | ● |   |   |
| Entity       | ● |   | ● | ● | ● | ● |   | ● |   |   |   |
| Physics      | ● |   |   |   | ● |   |   | ● |   |   |   |
| Spawns       | ● |   | ● |   |   |   | ● | ● |   |   | ● |
| VFX          | ● | ● | ● |   |   |   | ● |   |   |   |   |
| Audio        |   |   | ● |   |   |   | ● |   |   |   |   |
| Tween        |   |   |   |   |   |   | ● |   |   |   |   |
| HUD          | ● | ● | ● | ● |   |   |   |   |   |   |   |
| UI           |   | ● | ● | ● |   |   |   |   |   |   |   |
| InputHandler | ● |   |   |   | ● |   |   | ● |   | ● |   |
| Playable     | ● |   | ● | ● |   | ● | ● |   |   |   |   |
| Text         |   |   | ● |   |   |   |   |   |   |   | ● |
| Algebraic    |   |   |   |   |   |   |   |   |   |   |   |

> 엔진 모듈 중 `buffer · common · diagnostics · layout · shader · **program**` 은 Client 의 KEEP 엣지가 **0** 이라 표/그래프에서 생략된다.
> 특히 `program` 은 Client 가 직접 인스턴스로 쓰지 않는다 — uniform 은 전부 `Material::Properties[...]` 맵으로 올리거나 `Program*` 를 `Material::SetProgram` 에 넘기기만 한다.

## 제외한 노이즈 + 발견된 이상 (각주)

- **`→ program` 전부 제외** — Client 어디서도 `SJH::Program` 인스턴스 멤버를 호출하지 않음 (포인터 passthrough or `Uniforms::` free-func). 엔진 `program` 노드는 Client 그래프에서 빠짐.
- **`VFX → diagnostics` 제외** — `EffekseerDiagnostics::CheckPlayHandle/CheckHandleAlive` static 호출뿐.
- **`InputHandler/PlayerController.cpp` 죽은 include 6종** — `material/`·`program/`·`render/mesh_renderer`·`object/mesh`·`resource_registry/` include 했으나 미사용. 유일 소비처 `SpawnGroundMarker(PlayerController.h:157 선언)` 가 **정의 없음**. → 정리 후보.
- **`Entity → render` 제외** — `bullet_factory.h` 의 `MeshRenderer AddComponent` 가 주석 처리됨 (dead include).
- **`Algebraic → scene` 제외** — `Stat.h` 가 `scene/actor.h` 를 include 하나 어떤 `Scene::` 타입도 미사용 (stale include).
- **상수/enum 만으로는 엣지 미생성** — `Pass::Kind::*`, `PhysicsLayer::*`, `Entity::ULTIMATE_*`, `HUD::HEALTHBAR_FILL_COLOR` 등은 const/enum 이라 제외 (해당 모듈에 인스턴스 사용이 따로 있으면 그쪽으로 KEEP).
