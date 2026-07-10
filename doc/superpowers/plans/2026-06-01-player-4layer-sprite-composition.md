# PlayerActor 4-레이어 스프라이트 합성 Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** PlayerActor 의 바디 스프라이트를 *단일 SpriteRenderer* → *4-레이어(H/E/B/F 파트) child 합성* 으로 확장하고, FRONT_MOVE 한 방향에서 B 파트(2프레임 스트립)만 시간 애니메이션한다.

**Architecture:** 엔진 `SpriteSequencePlayable` 에 값-소유 clip ctor 를 추가(footgun 제거)하고, `CreatePlayerActor` 를 헤더 inline → `.cpp` 로 옮기며 sprite 합성을 흡수한다. 파트마다 자기 PNG = 자기 UniformAtlas(애니 파트는 `SetGrid(ColCount,1)` 스트립), DrawOrder 별 child Actor 4개 + `QueueOffset=DrawOrder` painter 합성. Bootstrap `PlayerBuilder` 가 `FRONT_MOVE` 를 주입하고 단일-스프라이트 블록과 `clipStorage` 를 제거한다.

**Tech Stack:** C++17, CMake/Ninja, `SJH::sprite`(UniformAtlas/SpriteRenderer/SpriteSequencePlayable), `SJH::scene`(Actor/Component), Box2D, spdlog.

**Spec:** [`doc/superpowers/specs/2026-06-01-player-4layer-sprite-composition-design.md`](../specs/2026-06-01-player-4layer-sprite-composition-design.md)

> **참고 — 검증 방식**: 본 저장소는 [[no_auto_tests]] 컨벤션(단위 테스트는 사용자 요청 시에만). 각 Task 의 검증은 **빌드 통과 + 실행 관찰** (기존 M6 plan 과 동일 스타일). TDD red-green 강제하지 않음.
> **참고 — 커밋**: 각 Task 의 커밋 step 은 *사용자 승인 후* 실행(실행 스킬의 체크포인트가 게이트). `doc/` 는 gitignore 강제 로컬 전용이라 본 plan/spec 자체는 커밋 대상 아님 — 커밋은 `src/`·`apps/` 코드만.

---

## File Structure

| 파일 | 동작 | 책임 |
|---|---|---|
| `src/sprite/sprite_sequence_playable.h` | Modify | 값-소유 ctor 선언 + `ownedClip_` 멤버 |
| `src/sprite/sprite_sequence_playable.cpp` | Modify | 값-소유 ctor 정의 |
| `apps/_MyApp_/src/Entity/Player/PlayerActor.h` | Modify | `SpriteCfg` 추가 + `CreatePlayerActor` 선언화(inline 제거) + Constants/vector include |
| `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` | Create | `CreatePlayerActor` 본문 + 4-레이어 합성 |
| `apps/_MyApp_/src/Entity/CMakeLists.txt` | Modify | `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` 소스 추가 |
| `apps/_MyApp_/src/Bootstrap/PlayerBuilder.h` | Modify | `PlayerDeps.clipStorage` + 미사용 fwd 제거 |
| `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` | Modify | 단일-스프라이트 블록 제거 → FRONT_MOVE 주입 + PlayerResult 채움 |
| `apps/_MyApp_/main.cpp` | Modify | `&mWholeAtlasClip` 인자 + 멤버 제거 |

---

## Task 1: 엔진 — SpriteSequencePlayable 값-소유 ctor

**Files:**
- Modify: `src/sprite/sprite_sequence_playable.h`
- Modify: `src/sprite/sprite_sequence_playable.cpp`

- [ ] **Step 1: 헤더에 값-소유 ctor 선언 + `ownedClip_` 멤버 추가**

`src/sprite/sprite_sequence_playable.h` 의 기존 ctor 선언(아래) 바로 뒤에 오버로드를 추가한다.

기존:
```cpp
        SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                const SpriteFrameClip*       clip);
        ~SpriteSequencePlayable() override;
```
변경:
```cpp
        SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                const SpriteFrameClip*       clip);
        /// @brief 값-소유 ctor — clip 을 멤버(ownedClip_)에 복사 보관, clip_ 가 이를 가리킨다.
        ///        호출자가 외부 clip 저장소를 관리할 필요 없음 (비소유 포인터 footgun 제거).
        SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                SpriteFrameClip              ownedClip);
        ~SpriteSequencePlayable() override;
```

같은 헤더의 private 멤버에서 `clip_` 줄 바로 뒤에 `ownedClip_` 을 추가한다.

기존:
```cpp
        SJH::Sprite::SpriteRenderer* sprite_;
        const SpriteFrameClip*        clip_;   // 하위 호환 기본 클립 (clipIdx=0 fallback)
```
변경:
```cpp
        SJH::Sprite::SpriteRenderer* sprite_;
        const SpriteFrameClip*        clip_;       // 하위 호환 기본 클립 (clipIdx=0 fallback)
        SpriteFrameClip               ownedClip_{}; // 값 ctor 사용 시 clip 값 보관 — clip_ 가 이를 가리킴
```

- [ ] **Step 2: `.cpp` 에 값-소유 ctor 정의 추가**

`src/sprite/sprite_sequence_playable.cpp` 의 기존 ctor 정의(아래) 바로 뒤에 오버로드 정의를 추가한다.

기존:
```cpp
    SpriteSequencePlayable::SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                                    const SpriteFrameClip*       clip)
        : sprite_(spriteRef), clip_(clip)
    {
    }
```
변경 (뒤에 추가):
```cpp
    SpriteSequencePlayable::SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                                    const SpriteFrameClip*       clip)
        : sprite_(spriteRef), clip_(clip)
    {
    }

    // 값-소유 ctor — clip_ 는 ownedClip_ 의 주소만 취하고(스토리지는 객체와 함께 할당되어 안정),
    // ownedClip_ 에 값을 복사한다. *clip_ 의 값 읽기는 OnUpdate(나중)에서만 발생 → UB 아님.
    // 멤버 선언 순서(sprite_, clip_, ownedClip_)와 init 순서가 일치 → -Wreorder 없음.
    SpriteSequencePlayable::SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                                    SpriteFrameClip              ownedClip)
        : sprite_(spriteRef), clip_(&ownedClip_), ownedClip_(ownedClip)
    {
    }
```

- [ ] **Step 3: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 성공 (오버로드는 아직 미사용이나 컴파일됨). 경고 0.

> `build_ninja` 미생성 시 먼저 `cmake --preset ninja`.

- [ ] **Step 4: 커밋 (사용자 승인 후)**

```bash
git add src/sprite/sprite_sequence_playable.h src/sprite/sprite_sequence_playable.cpp
git commit -m "feat(sprite): SpriteSequencePlayable 값-소유 ctor — clip lifetime footgun 제거

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: PlayerActorConfig 확장 + CreatePlayerActor .cpp화 + 4-레이어 합성

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerActor.h`
- Create: `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp`
- Modify: `apps/_MyApp_/src/Entity/CMakeLists.txt`

- [ ] **Step 1: 헤더에 include 추가**

`apps/_MyApp_/src/Entity/Player/PlayerActor.h` 의 include 블록에서 `#include "<Physics>/physics_movement.h"` 줄 뒤에 Constants 를, `#include <string>` 뒤에 `<vector>` 를 추가한다.

기존:
```cpp
#include "apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h"
#include "<Physics>/physics_movement.h"
#include "scene/actor.h"
```
변경:
```cpp
#include "apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h"
#include "<Physics>/physics_movement.h"
#include "apps/_MyApp_/src/Playable/Constants.h"   // TopdownShooter::Playable::PlayerTextureConfig / FRONT_MOVE 등
#include "scene/actor.h"
```

기존:
```cpp
#include <memory>
#include <string>
#include <vmath.h>
```
변경:
```cpp
#include <memory>
#include <string>
#include <vector>
#include <vmath.h>
```

- [ ] **Step 2: 헤더에 `SpriteCfg` 추가**

`PlayerActorConfig` 의 멤버 선언 블록(아래)을 교체해 `SpriteCfg` 정의 + `sprite` 멤버를 넣는다.

기존:
```cpp
		LifeCfg       life;
		MovementCfg   movement;
		ControllerCfg controller;
		PhysicsCfg    physics;
	};
```
변경:
```cpp
		/// @brief 방향 텍스처 합성 설정 (spec §6.1). M6 Task7 의 SpriteCfg{atlas,clips} 를 대체.
		struct SpriteCfg
		{
			/// @brief 방향 텍스처 세트(예: Playable::FRONT_MOVE). nullptr → 스프라이트 없음(게임플레이-only).
			const std::vector<TopdownShooter::Playable::PlayerTextureConfig> *direction = nullptr;
			float fps = 6.0f; ///< 애니 파트(ColCount>1) 의 초당 프레임
		};

		LifeCfg       life;
		MovementCfg   movement;
		ControllerCfg controller;
		PhysicsCfg    physics;
		SpriteCfg     sprite;
	};
```

- [ ] **Step 3: 헤더의 inline 본문을 선언으로 교체**

`PlayerActor.h` 의 `inline std::unique_ptr<...> CreatePlayerActor(const PlayerActorConfig &cfg) { ... return actor; }` **전체 함수**(현재 `inline` 키워드부터 닫는 `}` 까지)를 아래 선언 한 줄로 교체한다. 함수 위의 `/// @brief ...` Doxygen 주석 블록은 **그대로 유지**한다.

교체 후 (주석 블록 다음 줄):
```cpp
	std::unique_ptr<SJH::Scene::Actor> CreatePlayerActor(const PlayerActorConfig &cfg);
```

- [ ] **Step 4: `PlayerActor.cpp` 생성**

Create `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp`:
```cpp
#include "apps/_MyApp_/src/Entity/Player/PlayerActor.h"

#include "resource_registry/resource_registry.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_frame_clip.h"
#include "sprite/sprite_sequence_playable.h"

#include <<box2d>/box2d.h>
#include <<spdlog>/spdlog.h>
#include <string>

namespace TopdownShooter::Entity::Player
{
	std::unique_ptr<SJH::Scene::Actor> CreatePlayerActor(const PlayerActorConfig &cfg)
	{
		auto actor = std::make_unique<SJH::Scene::Actor>(cfg.name);

		actor->AddComponent<Components::Life>(cfg.life.hp);

		if (cfg.physics.world != nullptr)
		{
			// Physics body 생성 — b2World 가 lifetime 소유.
			b2BodyDef bd;
			bd.type = b2_dynamicBody;
			bd.position.Set(cfg.physics.startPosition[0], cfg.physics.startPosition[1]);
			bd.linearDamping = cfg.physics.linearDamping;
			b2Body *body = cfg.physics.world->CreateBody(&bd);

			b2PolygonShape box;
			box.SetAsBox(cfg.physics.size[0] * 0.5f, cfg.physics.size[1] * 0.5f);

			b2FixtureDef fd;
			fd.shape = &box;
			fd.density = cfg.physics.density;
			fd.friction = cfg.physics.friction;
			fd.isSensor = cfg.physics.isSensor;
			fd.filter.categoryBits = cfg.physics.categoryBits;
			fd.filter.maskBits = cfg.physics.maskBits;
			body->CreateFixture(&fd);

			auto *pb = actor->AddComponent<Physics::Components::BoxBody>();
			pb->SetBody(body);
			pb->SetSensor(cfg.physics.isSensor);

			auto *pm = actor->AddComponent<Physics::PhysicsMovement>(cfg.movement.speed);

			if (cfg.controller.keyboard != nullptr)
			{
				auto *controller = actor->AddComponent<Controller::PlayerController>();
				controller->SetKeyboardInput(cfg.controller.keyboard);
				controller->SetMouseInput(cfg.controller.mouse);
				controller->SetWorldCamera(cfg.controller.camera);
				controller->SetMovableTarget(pm);
				controller->SetFireCallback(cfg.controller.onFire);
				controller->SetDamageCallback(cfg.controller.onDamage);
				controller->SetUp();
			}
		}
		else
		{
			// physics 미사용 — 기존 Movement (Transform 직접 조작).
			auto *movement = actor->AddComponent<Components::Movement>(cfg.movement.speed);

			if (cfg.controller.keyboard != nullptr)
			{
				auto *controller = actor->AddComponent<Controller::PlayerController>();
				controller->SetKeyboardInput(cfg.controller.keyboard);
				controller->SetMouseInput(cfg.controller.mouse);
				controller->SetWorldCamera(cfg.controller.camera);
				controller->SetMovableTarget(movement);
				controller->SetFireCallback(cfg.controller.onFire);
				controller->SetDamageCallback(cfg.controller.onDamage);
				controller->SetUp();
			}
		}

		// === 4-레이어 스프라이트 합성 (spec §6.3) — direction 지정 시에만 ===
		// 각 파트 PNG = 자기 UniformAtlas (정적=SetGrid(1,1), 애니 B파트=SetGrid(ColCount,1) 스트립).
		// DrawOrder 별 child Actor (local 0,0,0 → parent world 공유) + QueueOffset=DrawOrder painter 합성.
		if (cfg.sprite.direction != nullptr)
		{
			auto &reg = SJH::ResourceRegistry::Get();
			for (const auto &t : *cfg.sprite.direction)
			{
				auto child = std::make_unique<SJH::Scene::Actor>(
				    cfg.name + "_L" + std::to_string(t.DrawOrder));
				SJH::Scene::Actor *childPtr = actor->AddChild(std::move(child));

				// key=path → LEFT/RIGHT 가 같은 PNG 재사용 시 캐시 hit.
				auto *atlas = reg.CreateUniformAtlas(t.TexturePath, t.TexturePath, t.ColCount, t.RowCount);
				if (!atlas)
				{
					spdlog::error("[4layer] atlas load 실패: {}", t.TexturePath);
					continue; // 그 레이어만 skip (graceful)
				}

				auto *spr = childPtr->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
				spr->flipX = t.Flip;
				spr->QueueOffset = t.DrawOrder; // 2450+DrawOrder → distinct 층

				if (t.ColCount > 1) // 애니 파트 (가로 N프레임 스트립)
				{
					auto *seq = childPtr->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
					    spr, SJH::SpriteSequence::SpriteFrameClip{0, t.ColCount, cfg.sprite.fps});
					seq->SetIsLoop(true);
					seq->Play();
				}
			}
		}

		return actor;
	}
} // namespace TopdownShooter::Entity::Player
```

- [ ] **Step 5: Entity CMake 에 소스 추가**

`apps/_MyApp_/src/Entity/CMakeLists.txt` 의 `add_library(myapp_entity STATIC ...)` 목록에서 `<Player>/PlayerBehavior.cpp` 줄 위에 `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` 를 추가한다.

기존:
```cmake
add_library(myapp_entity STATIC
    BaseEntity.cpp
    <Player>/PlayerBehavior.cpp
```
변경:
```cmake
add_library(myapp_entity STATIC
    BaseEntity.cpp
    apps/_MyApp_/src/Entity/Player/PlayerActor.cpp
    <Player>/PlayerBehavior.cpp
```

> 추가 link 불필요 — `SJH::sprite`(이미 Entity PRIVATE link)가 `SJH::render`/`SJH::resource_registry` 를 PUBLIC 전이. spdlog 는 `game_deps`(PRIVATE) 경유.

- [ ] **Step 6: 빌드 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 성공. 이 시점 `PlayerBuilder` 는 아직 `sprite.direction` 미설정(기본 nullptr) → 합성 미발생, 기존 단일 `test_pattern` 스프라이트 동작 그대로 유지. **동작 무변화.**

- [ ] **Step 7: 커밋 (사용자 승인 후)**

```bash
git add apps/_MyApp_/src/Entity/Player/PlayerActor.h apps/_MyApp_/src/Entity/Player/PlayerActor.cpp apps/_MyApp_/src/Entity/CMakeLists.txt
git commit -m "feat(_MyApp_): CreatePlayerActor .cpp화 + SpriteCfg + 4-레이어 합성 로직

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: FRONT_MOVE 배선 — PlayerBuilder + main 정리

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/PlayerBuilder.h`
- Modify: `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp`
- Modify: `apps/_MyApp_/main.cpp`

- [ ] **Step 1: `PlayerBuilder.h` — clipStorage + 미사용 fwd 제거**

`PlayerDeps` 에서 `clipStorage` 필드와 그 주석을 제거한다.

기존:
```cpp
		SJH::Scene::Camera  *worldCamera  = nullptr; ///< raycast 카메라 + ActorFolower follow-target wiring.
		/// @brief SpriteFrameClip 의 lifetime 저장소 — main 이 멤버로 소유 (SpriteSequencePlayable 가
		///        clip 을 *포인터로만* 보유하므로 빌더 로컬에 두면 댕글링. caller-owned 필수).
		SJH::SpriteSequence::SpriteFrameClip *clipStorage = nullptr;
	};
```
변경:
```cpp
		SJH::Scene::Camera  *worldCamera  = nullptr; ///< raycast 카메라 + ActorFolower follow-target wiring.
	};
```

같은 헤더의 fwd 선언 블록에서 이제 미사용인 `struct SpriteFrameClip;` 을 제거한다(`SpriteSequencePlayable` 은 PlayerResult 가 쓰므로 유지).

기존:
```cpp
namespace SJH::SpriteSequence
{
	class SpriteSequencePlayable;
	struct SpriteFrameClip;
}
```
변경:
```cpp
namespace SJH::SpriteSequence
{
	class SpriteSequencePlayable;
}
```

- [ ] **Step 2: `PlayerBuilder.cpp` — Constants include + 상단 미사용 reg 제거**

include 블록에 Constants 를 추가한다(`"apps/_MyApp_/src/Entity/Player/PlayerActor.h"` 뒤).

기존:
```cpp
#include "apps/_MyApp_/src/Bootstrap/PlayerBuilder.h"

#include "apps/_MyApp_/src/Audio/AudioSystem.h"
```
변경:
```cpp
#include "apps/_MyApp_/src/Bootstrap/PlayerBuilder.h"

#include "apps/_MyApp_/src/Audio/AudioSystem.h"
#include "apps/_MyApp_/src/Playable/Constants.h" // TopdownShooter::Playable::FRONT_MOVE
```

함수 상단에서 더 이상 쓰지 않는 `reg` 지역참조를 제거한다(아래 atlas 블록 삭제로 미사용. `dir` 은 유지).

기존:
```cpp
		auto &reg = SJH::ResourceRegistry::Get();
		auto &dir = SJH::Scene::Director::Get();
```
변경:
```cpp
		auto &dir = SJH::Scene::Director::Get();
```

- [ ] **Step 3: `PlayerBuilder.cpp` — FRONT_MOVE 주입**

`CreatePlayerActor(pac)` 호출 **직전**(현재 `auto spriteActor = ...CreatePlayerActor(pac);` 줄 위)에 sprite 설정을 추가한다.

기존:
```cpp
		auto spriteActor = TopdownShooter::Entity::Player::CreatePlayerActor(pac);
```
변경:
```cpp
		pac.sprite.direction = &TopdownShooter::Playable::FRONT_MOVE; // FRONT_MOVE 4-레이어 (E·H·B·F)
		pac.sprite.fps       = 6.0f;

		auto spriteActor = TopdownShooter::Entity::Player::CreatePlayerActor(pac);
```

- [ ] **Step 4: `PlayerBuilder.cpp` — 단일-스프라이트 블록을 PlayerResult 채움으로 교체**

기존 atlas/단일 sprite/clip 블록 전체(아래)를 교체한다.

기존:
```cpp
		// Atlas — registry 가 LoadFromPNG + SetGrid 일괄.
		auto *atlas = reg.CreateUniformAtlas(
		    "test_pattern",
		    "resources/texture/TestPattern.png",
		    4, 4);
		if (!atlas)
		{
			spdlog::error("[M1] atlas load failed");
			return result; // SpriteActor=nullptr — caller early-return 보존.
		}

		result.Sprite = spriteActor->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);

		// clip 은 caller-owned 저장소에 기록 — SpriteSequencePlayable 가 포인터로만 보유 (lifetime 함정).
		*deps.clipStorage = SJH::SpriteSequence::SpriteFrameClip{0, atlas->FrameCount(), 4.0f};
		result.SpriteSeq = spriteActor->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
		    result.Sprite, deps.clipStorage);
		result.SpriteSeq->SetIsLoop(true);
		result.SpriteSeq->Play();
```
변경:
```cpp
		// 4-레이어 바디는 CreatePlayerActor 가 child 로 생성 (spec §6.3).
		// PlayerResult.Sprite/SpriteSeq 는 애니(B) 레이어의 컴포넌트를 가리킨다 (호환용 — 없으면 nullptr).
		for (const auto &child : spriteActor->GetChildren())
		{
			if (auto *seq = child->GetComponent<SJH::SpriteSequence::SpriteSequencePlayable>())
			{
				result.Sprite    = child->GetComponent<SJH::Sprite::SpriteRenderer>();
				result.SpriteSeq = seq;
				break;
			}
		}
```

> `deps.clipStorage` 참조가 사라지므로 Step 1 의 헤더 변경과 정합. `result` 의 나머지(SpriteActor)는 아래 기존 코드가 채운다.

- [ ] **Step 5: `main.cpp` — BuildPlayer 인자 + 멤버 제거**

`BuildPlayer` 호출에서 `&mWholeAtlasClip` 인자를 제거한다.

기존:
```cpp
			auto player = Bootstrap::BuildPlayer({&mKeyboard, &mMouse, &phys.World(), mCamera, &mWholeAtlasClip});
```
변경:
```cpp
			auto player = Bootstrap::BuildPlayer({&mKeyboard, &mMouse, &phys.World(), mCamera});
```

멤버 선언에서 `mWholeAtlasClip` 줄을 제거한다.

기존:
```cpp
		SJH::SpriteSequence::SpriteSequencePlayable *mSpriteSeq = nullptr;
		SJH::SpriteSequence::SpriteFrameClip mWholeAtlasClip{};
		SJH::Sprite::SpriteRenderer *mSprite = nullptr;
```
변경:
```cpp
		SJH::SpriteSequence::SpriteSequencePlayable *mSpriteSeq = nullptr;
		SJH::Sprite::SpriteRenderer *mSprite = nullptr;
```

- [ ] **Step 6: 빌드 + 실행 검증**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 성공, 경고 0.

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
Expected: 플레이어가 **FRONT_MOVE 4겹**(E·H·F 정적 + **B 파트 2프레임 워크 루프**)으로 표시. WASD 이동·카메라 follow 정상. (단일 `test_pattern` 격자무늬가 사라지고 캐릭터 스프라이트로 바뀜.)

> 실행 종료는 창 닫기. 리소스 상대경로 때문에 반드시 실행 파일 디렉토리에서(`cd`) 실행.

- [ ] **Step 7: 커밋 (사용자 승인 후)**

```bash
git add apps/_MyApp_/src/Bootstrap/PlayerBuilder.h apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp apps/_MyApp_/main.cpp
git commit -m "feat(_MyApp_): PlayerBuilder FRONT_MOVE 4-레이어 배선 + clipStorage 제거

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## 완료 기준

- [ ] FRONT_MOVE 4-레이어 합성이 `_MyApp_` 에서 시각 확인됨 (B 파트 2프레임 애니, 나머지 정적, DrawOrder 순 painter 합성).
- [ ] 엔진 `SpriteSequencePlayable` 값-소유 ctor 로 clip footgun 제거, `clipStorage`/`mWholeAtlasClip` 소멸.
- [ ] 3 커밋 (engine / entity / bootstrap+main) — 각각 빌드 통과.

## 범위 밖 (spec §13)

8방향/IDLE↔MOVE 런타임 전환 **구현**, `PlayerHand` orbit, M6 Task6/7 본체(PlayerBehavior+5 FX 슬롯), `BACK_MOVE_F_3` placeholder 투명도, 단위 테스트.
