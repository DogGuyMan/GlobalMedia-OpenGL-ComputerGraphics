# Directional Facing/Pose 스프라이트 전환 — 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development(권장) 또는 superpowers:executing-plans 로 task 단위 구현.
> ⚠ 프로젝트 규약: `no_auto_tests`(단위테스트 자동추가 금지) → 검증=빌드 exit0 + GUI 육안. **커밋 안 함**(사용자 관리) → commit step 없음.
> 정본 spec: `doc/superpowers/specs/2026-06-02-directional-facing-pose-design.md`. 브랜치 `game/module/ingame/temp`.

**Goal:** 플레이어가 이동/조준 방향에 따라 4방향(Front/Back/Left/Right) × 2포즈(Idle/Move) 스프라이트로 전환.

**Architecture:** PlayerController(RD5 단일 작성자, 인터페이스 sink)가 velocity/aim → 튜닝 가능 각도 임계(Constants.h) → `IActorPresentation::SetFacing/SetPose` push. PlayableDirector(어댑터)가 8그룹 가시성을 `SetActive` 토글. 그룹 빌드는 PlayerBuilder(composition root).

**Tech Stack:** C++17, CMake/Ninja, SJH 엔진(Scene/Sprite/Playable), vmath.

---

## File Structure (OWN — 정확히 이 묶음만)
- `apps/_MyApp_/src/Playable/Constants.h` — **Modify**: `FacingThresholdConfig` + `PLAYER_FACING_THRESHOLD`(데이터만, §6 튜닝 지점).
- `apps/_MyApp_/src/Playable/PlayableDirector.{h,cpp}` — **Modify**: DirGroup/RegisterGroup/RefreshDirectional/SetFacing/SetPose/Apply(§2).
- `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` — **Modify**: 8그룹 빌드 + RegisterGroup + RefreshDirectional + 단일방향 무력화(§3).
- `apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}` — **Modify**: QuantizeByThreshold + RD5 push + attack window + flipX/mCachedSprite 제거(§4).

**미접근**: `PlayerActor.{h,cpp}`(Entity→Playable 순환), `Components.Interfaces.h`(Quantize4 읽기만), 코어(`src/*`)·셰이더·`Spawns`·`EnemyFactory`·`EnemyBuilder`·`Life`·`main.cpp`. foundation 시그니처/"fire"·"hit"·"death" 불변.

---

### Task 1: §6 Facing 임계 각도 config (Constants.h, 데이터)

**Files:** Modify `apps/_MyApp_/src/Playable/Constants.h`

- [ ] **Step 1:** `namespace TopdownShooter::Playable` 안, `EntityTextureConfig` 구조체 정의 *뒤*(예: 줄 16 `};` 다음)에 추가:

```cpp
	// 방향 판정 임계 각도 (degree, [0,360), 0°=오른쪽(+X), 반시계 +). 각 vec2 = {start, end};
	// start>end 면 0°를 가로질러 wrap (Right). 360° 를 빈틈없이 덮어야 결정적.
	// 매핑: Back=화면 위(Up, ~90°), Front=화면 아래(Down, ~270°). 순서 = Up,Down,Left,Right.
	struct FacingThresholdConfig
	{
		vmath::vec2 Back;  // Up   (~90°)
		vmath::vec2 Front; // Down (~270°)
		vmath::vec2 Left;  // (~180°)
		vmath::vec2 Right; // (~0°, wrap)
	};
	const FacingThresholdConfig PLAYER_FACING_THRESHOLD = {
	    {45.0f, 135.0f},   // Back  (Up)
	    {225.0f, 315.0f},  // Front (Down)
	    {135.0f, 225.0f},  // Left
	    {315.0f, 45.0f},   // Right (0° wrap)
	};
```

- [ ] **Step 2:** 빌드 — 헤더만 추가라 컴파일 가드만 확인.

Run: `cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics && cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -3`
Expected: `=== ` 빌드 exit 0 (sb7 gl3 `#warning` 만 허용).

---

### Task 2: §2 PlayableDirector — DirGroup + 가시성 토글

**Files:** Modify `apps/_MyApp_/src/Playable/PlayableDirector.{h,cpp}`

- [ ] **Step 1 (헤더 상단 include):** PlayableDirector.h 의 `#include <map>` 위/근처에 추가:

```cpp
#include <array>
```
그리고 기존 include 블록 아래(네임스페이스 열기 전)에 forward 추가:
```cpp
namespace SJH::Sprite { class SpriteRenderer; }   // DirGroup 핸들 (실타입 .cpp)
```

- [ ] **Step 2 (public API 추가):** `bool Has(const std::string&) const;` 다음 줄에 추가:

```cpp
		/// @brief (facing,pose) 한 그룹 = 그 방향/포즈 4-레이어 SpriteRenderer 핸들(애니 미보유).
		struct DirGroup { std::array<SJH::Sprite::SpriteRenderer *, 4> layers{}; };
		/// @brief 그룹 등록 (빌드 시 1회).
		void RegisterGroup(Entity::EFacing f, Entity::EPose p, const DirGroup &g) { mGroups[idx(f)][idx(p)] = g; }
		/// @brief AddChild(enter) 후 1회 — 초기 (Front,Idle) 만 활성.
		void RefreshDirectional() { Apply(); }
```

- [ ] **Step 3 (빈 훅 → 선언 교체):** 기존 SetFacing/SetPose 빈 훅(현재 `void SetFacing(Entity::EFacing /*facing*/) override {}` / `void SetPose(Entity::EPose /*pose*/) override {}`)을 선언으로 교체:

```cpp
		void SetFacing(Entity::EFacing f) override; // 본문 .cpp
		void SetPose(Entity::EPose p) override;     // 본문 .cpp
```

- [ ] **Step 4 (private 멤버 추가):** 기존 private 의 `std::map<std::string, Slot> mPlayables;` 아래에 추가:

```cpp
		static int idx(Entity::EFacing f) { return static_cast<int>(f); }
		static int idx(Entity::EPose p) { return static_cast<int>(p); }
		void Apply();
		DirGroup        mGroups[4][2]{};
		Entity::EFacing mFacing = Entity::EFacing::Front;
		Entity::EPose   mPose   = Entity::EPose::Idle;
```

- [ ] **Step 5 (.cpp include):** PlayableDirector.cpp 상단(`#include "apps/_MyApp_/src/Playable/PlayableDirector.h"` 아래)에 추가:

```cpp
#include "scene/actor.h"             // Actor::GetOwner/SetActive
#include "sprite/sprite_component.h" // SJH::Sprite::SpriteRenderer (Component 완전형 — GetOwner 호출)
```

- [ ] **Step 6 (.cpp 본문):** `ReactDied` 정의 위(또는 namespace 끝 직전)에 추가:

```cpp
	void PlayableDirector::SetFacing(Entity::EFacing f)
	{
		if (f == mFacing) return;   // velocity 안 읽음 — 계산은 PlayerController(RD5)
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
		// (mFacing,mPose) 그룹만 활성. SetActive 토글 → 렌더+tick 동시 게이트(2프레임 애니라 freeze 무차별).
		for (int f = 0; f < 4; ++f)
			for (int p = 0; p < 2; ++p)
			{
				const bool active = (f == idx(mFacing) && p == idx(mPose));
				for (auto *layer : mGroups[f][p].layers)
					if (layer && layer->GetOwner())
						layer->GetOwner()->SetActive(active);
			}
	}
```

- [ ] **Step 7:** 빌드.

Run: `cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics && cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5`
Expected: 빌드 exit 0. 링크 에러 시 구조 위반.

---

### Task 3: §3 PlayerBuilder — 8그룹 빌드 + RegisterGroup

**Files:** Modify `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp`
(전제: `#include "apps/_MyApp_/src/Playable/Constants.h"`, `#include "apps/_MyApp_/src/Entity/Components/LifeComponents.h"`(EFacing/EPose 경유), `#include "scene/actor.h"`, `#include "sprite/sprite_component.h"`, `#include "sprite/sprite_sequence_playable.h"`, `#include "resource_registry/resource_registry.h"` 이미 존재 — grep 으로 확인, 없으면 추가.)

- [ ] **Step 1 (단일방향 무력화):** `pac.sprite.direction = &TopdownShooter::Playable::PLAYER_FRONT_MOVE;` 줄을 찾아(grep `pac.sprite.direction`) 삭제하고 주석으로 대체:

```cpp
		// 단일방향 주입 제거 — 8그룹을 아래에서 직접 빌드(PlayerActor 의 if(direction!=nullptr) 단일블록 비활성).
		// pac.sprite.direction 기본값 nullptr 유지.
```

- [ ] **Step 2 (8그룹 빌드 블록):** director 부착(grep `AddComponent<TopdownShooter::Playable::PlayableDirector>` → `auto *director = ...`) *직후*, AddChild 전에 삽입:

```cpp
		// ─── directional 8그룹(4방향×2포즈) child 빌드 + RegisterGroup (분해 Task6) ───
		{
			namespace P = TopdownShooter::Playable;
			using TopdownShooter::Entity::EFacing;
			using TopdownShooter::Entity::EPose;
			struct GrpSrc { EFacing f; EPose p; const std::vector<P::EntityTextureConfig> *layers; };
			const GrpSrc kGroups[] = {
			    {EFacing::Front, EPose::Idle, &P::PLAYER_FRONT_IDLE}, {EFacing::Back, EPose::Idle, &P::PLAYER_BACK_IDLE},
			    {EFacing::Left, EPose::Idle, &P::PLAYER_LEFT_IDLE},   {EFacing::Right, EPose::Idle, &P::PLAYER_RIGHT_IDLE},
			    {EFacing::Front, EPose::Move, &P::PLAYER_FRONT_MOVE}, {EFacing::Back, EPose::Move, &P::PLAYER_BACK_MOVE},
			    {EFacing::Left, EPose::Move, &P::PLAYER_LEFT_MOVE},   {EFacing::Right, EPose::Move, &P::PLAYER_RIGHT_MOVE},
			};
			auto           &reg  = SJH::ResourceRegistry::Get();
			constexpr float kFps = 8.0f; // 애니(ColCount>1) 초당 프레임 (SpriteCfg 기본과 동일)
			for (const auto &grp : kGroups)
			{
				P::PlayableDirector::DirGroup dg{};
				int                           li = 0;
				for (const auto &t : *grp.layers)
				{
					auto *atlas = reg.FindUniformAtlas(t.TexturePath); // 공유 PNG 중복키 nullptr 회피
					if (!atlas)
						atlas = reg.CreateUniformAtlas(t.TexturePath, t.TexturePath, t.ColCount, t.RowCount);
					if (!atlas)
					{
						spdlog::error("[8layer] atlas 실패: {}", t.TexturePath);
						continue;
					}
					auto  child = std::make_unique<SJH::Scene::Actor>(
					    "player_dir_" + std::to_string(static_cast<int>(grp.f)) + "_" +
					    std::to_string(static_cast<int>(grp.p)) + "_L" + std::to_string(t.DrawOrder));
					auto *cp = spriteActor->AddChild(std::move(child));
					auto *spr = cp->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
					spr->flipX       = t.Flip;
					spr->QueueOffset = t.DrawOrder; // 초기 가시성 안 건드림 — RefreshDirectional 이 처리
					if (li < 4) dg.layers[li] = spr;
					if (t.ColCount > 1) // 걷기 애니(B) — t.ColCount 로만 판정
					{
						auto *seq = cp->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
						    spr, SJH::SpriteSequence::SpriteFrameClip{0, t.ColCount, kFps});
						seq->SetIsLoop(true);
						seq->Play(); // [A] scene-tick (child 소유, DirGroup 미보유)
					}
					++li;
				}
				director->RegisterGroup(grp.f, grp.p, dg);
			}
		}
```

- [ ] **Step 3 (RefreshDirectional — AddChild 후):** `result.SpriteActor = ...AddChild(std::move(spriteActor));` + 기존 PlayerHands/카메라 follow 라인들 *뒤*, `return result;` *전*에 삽입:

```cpp
		// 초기 가시성 확정 — 32레이어가 active 로 enter(OnEnter/애니 시작)된 *뒤* Front/Idle 외 SetActive(false).
		director->RefreshDirectional();
```

- [ ] **Step 4:** 빌드.

Run: `cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics && cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -6`
Expected: 빌드 exit 0.

---

### Task 4: §4+§6 PlayerController — RD5 push + QuantizeByThreshold + attack window

**Files:** Modify `apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}`

- [ ] **Step 1 (h 멤버 추가):** PlayerController.h private 섹션에 추가:

```cpp
	TopdownShooter::Entity::IActorPresentation *mSink = nullptr; // = PlayableDirector(인터페이스로만, lazy)
	TopdownShooter::Entity::EFacing             mLastFacing = TopdownShooter::Entity::EFacing::Front;
	float                                       mAttackWindowSec = 0.15f; // 발사 후 "조준 응시" 윈도
	float                                       mAttackTimer     = 0.0f;
```

- [ ] **Step 2 (h — mCachedSprite 제거):** grep `mCachedSprite`, `SpriteRenderer` in PlayerController.h. `SJH::Sprite::SpriteRenderer *mCachedSprite ...` 멤버 + 그 forward 선언(`namespace SJH::Sprite { class SpriteRenderer; }`)을 **삭제**(아래 .cpp 정리와 함께). 다른 곳에서 mCachedSprite 를 쓰면 그 사용처도 제거.

- [ ] **Step 3 (.cpp include + 헬퍼):** PlayerController.cpp 상단에 추가:

```cpp
#include "apps/_MyApp_/src/Playable/Constants.h" // FacingThresholdConfig / PLAYER_FACING_THRESHOLD (헤더-only 데이터)
#include <cmath>                 // std::atan2
```
이어서 익명 네임스페이스에 헬퍼 추가(파일 상단, 함수들 위):
```cpp
namespace
{
	// [start,end](degree)에 deg 포함? start>end 면 0° wrap.
	bool InRange(float deg, const vmath::vec2 &r)
	{
		return (r[0] <= r[1]) ? (deg >= r[0] && deg < r[1]) : (deg >= r[0] || deg < r[1]);
	}
	// dir=(x,z) → θ=normalize360(deg(atan2(-z,x))) → 4범위 중 포함 필드. no-match=fallback.
	TopdownShooter::Entity::EFacing QuantizeByThreshold(
	    vmath::vec2 dir, const TopdownShooter::Playable::FacingThresholdConfig &cfg,
	    TopdownShooter::Entity::EFacing fallback)
	{
		namespace E = TopdownShooter::Entity;
		float deg = std::atan2(-dir[1], dir[0]) * 57.29578f; // rad→deg, screen-up=-Z
		if (deg < 0.0f) deg += 360.0f;
		if (InRange(deg, cfg.Back)) return E::EFacing::Back;
		if (InRange(deg, cfg.Front)) return E::EFacing::Front;
		if (InRange(deg, cfg.Left)) return E::EFacing::Left;
		if (InRange(deg, cfg.Right)) return E::EFacing::Right;
		return fallback;
	}
}
```

- [ ] **Step 4 (.cpp Update — flipX 핵 → RD5 블록):** grep `mCachedSprite` / `flipX` in Update(). `EulerRot[1] = mAimAngleY` 라인은 **유지**. 그 아래 `mCachedSprite` lazy 캐시 + `mCachedSprite->flipX = (mAimDirection[0] < 0.0f)` 두 줄을 **삭제**하고, **`mInputValue` 리셋(grep `mInputValue` 가 vec3(0) 으로 리셋되는 지점) 전에** 아래 블록 삽입:

```cpp
		// === RD5: facing/pose 단일 작성자 (controller 계산 → sink 토글) ===
		if (mSink == nullptr)
			mSink = owner->GetComponent<TopdownShooter::Entity::IActorPresentation>(); // lazy(=director, 인터페이스 조회)
		if (mSink != nullptr)
		{
			namespace E = TopdownShooter::Entity;
			if (mAttackTimer > 0.0f) mAttackTimer -= dt;
			const bool        attacking = (mAttackTimer > 0.0f);
			const vmath::vec2 velXZ(mInputValue[0], mInputValue[2]); // ★ 리셋 전
			const bool        moving = (velXZ[0] * velXZ[0] + velXZ[1] * velXZ[1]) > 0.001f;
			const vmath::vec2 aimXZ(mAimDirection[0], mAimDirection[2]);

			E::EFacing facing = attacking ? QuantizeByThreshold(aimXZ, TopdownShooter::Playable::PLAYER_FACING_THRESHOLD, mLastFacing)
			                  : moving    ? QuantizeByThreshold(velXZ, TopdownShooter::Playable::PLAYER_FACING_THRESHOLD, mLastFacing)
			                              : mLastFacing;
			E::EPose pose = (attacking || moving) ? E::EPose::Move : E::EPose::Idle;
			mSink->SetFacing(facing);
			mSink->SetPose(pose);
			mLastFacing = facing;
		}
```
> `owner` / `dt` 변수명은 Update() 의 실제 이름에 맞춰 사용(grep 으로 확인).

- [ ] **Step 5 (.cpp OnFirePressed — attack 윈도 arm):** `OnFirePressed()` 본문(grep `OnFirePressed`)에 추가:

```cpp
		mAttackTimer = mAttackWindowSec; // 발사 후 0.15s 동안 facing=조준 (하이브리드)
```

- [ ] **Step 6 (미사용 include 정리):** mCachedSprite 제거 후 `#include "sprite/sprite_component.h"` 가 PlayerController.cpp/h 에서 더 안 쓰이면 제거(-Werror 회피). grep `SpriteRenderer`/`sprite_component` 로 잔여 사용 확인 후 판단.

- [ ] **Step 7:** 빌드.

Run: `cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics && cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -6`
Expected: 빌드 exit 0. 링크 에러(InputHandler→Playable) 나면 = PlayableDirector 구체타입 참조한 것 → sink 을 IActorPresentation 으로만 썼는지 재확인.

---

### Task 5: 통합 빌드 + GUI 육안 검증 + 부호 보정

**Files:** 없음(검증). 필요 시 Task1 의 임계 각도 또는 Task4 의 atan2 부호 조정.

- [ ] **Step 1:** clean 재구성 + 빌드.

Run: `cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics && cmake --preset ninja >/dev/null 2>&1 && cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -4`
Expected: 빌드 exit 0.

- [ ] **Step 2:** 실행 + 육안.

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
관찰:
- WASD 이동 → 몸통 스프라이트가 Front/Back/Left/Right 로 전환되는가.
- 정지 → Idle 포즈, 이동 → Move(걷기 애니).
- 좌클릭 발사 → 0.15s 동안 몸통이 조준(마우스) 방향을 향하는가.
- 크래시 0. (muzzle/distortion.efk `[error]` 1건은 무관 — fire FX 별개 트랙.)

- [ ] **Step 3 (부호 보정 — 필요 시만):** W 가 Back(위) 대신 Front 로 보이거나 좌우 반대면:
  - **1차**: `Constants.h` 의 `PLAYER_FACING_THRESHOLD` 각도 범위 조정(예: Back↔Front 범위 스왑).
  - **또는**: Task4 의 `std::atan2(-dir[1], dir[0])` 의 `-dir[1]` 부호(±) 또는 `dir[0]` 부호 조정.
  - **Quantize4(Components.Interfaces.h) 본체는 미수정.** 보정 사실을 보고에 명시.

- [ ] **Step 4 (보고):** DONE/DONE_WITH_CONCERNS/BLOCKED + 변경 파일 요약 + 빌드 마지막 줄 + 육안 관찰(방향/포즈 전환·부호) + (있으면)부호 보정 내용 + `git status --short`(내 파일). **커밋 안 함.**

---

## 자기검토 (작성자 체크리스트)
- **Spec 커버리지**: §1(책임)→아키텍처 흐름, §2→Task2, §3→Task3, §4→Task4(RD5/attack), §5(스코프)→File Structure 미접근 목록, §6→Task1+Task4(QuantizeByThreshold). 모든 절에 대응 task 있음. ✓
- **타입 일관성**: `DirGroup`/`RegisterGroup`/`RefreshDirectional`/`Apply`/`mGroups`(Task2) ↔ 사용(Task3 `RegisterGroup`/`RefreshDirectional`). `FacingThresholdConfig`/`PLAYER_FACING_THRESHOLD`(Task1) ↔ `QuantizeByThreshold`(Task4). `mSink`/`mLastFacing`/`mAttackTimer`/`mAttackWindowSec`(Task4 h) ↔ Update/OnFirePressed(Task4 cpp). EFacing 순서(Front=0,Back=1,Left=2,Right=3)와 idx 캐스팅 일관. ✓
- **플레이스홀더**: 모든 코드 step 에 완전 코드. 단 line 번호는 병렬 드리프트로 grep 재확인 지시(하드코딩 안 함). ✓
- **무접근 경계**: PlayerActor/Quantize4/코어/Spawns/enemy/main 미접근 — Task 들이 OWN 4파일만 수정. ✓
