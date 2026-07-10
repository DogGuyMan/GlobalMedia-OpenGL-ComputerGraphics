# Resume Handoff — PlayerBehavior 분해 **구현 완료** + 연출/Cull (2026-06-03)

> ✅ **2026-06-03 갱신 (오케스트레이터 검증):** Task 5/7 이 본문 작성 시점의 "STAGED·미커밋"에서 **커밋됨 → `f45211c "[update] : deadcode 제거"`** (Carrier.h 도입 + PlayerBehavior.cpp 삭제 둘 다 이 커밋). **빌드 GREEN 재검증 완료**(`cmake --build --preset ninja --target _MyApp_` → **exit 0, error 0**, `[*/*] Linking CXX executable _MyApp_` 39.9MB, 허용된 sb7 `#warning` 1건만). 즉 **PlayerBehavior 분해 Task 0~7 = 코드 완료 + 빌드 GREEN**. **남은 것 = (1) GUI 육안 검증(§체크리스트) + (2) cull fix `src/material/pass.h` 단독 커밋(여전히 `M` 미커밋)** 둘뿐. ⚠ HEAD 가 검증 중 `8942e89→eb17c80→6c4d7c4→f45211c` 로 churning(사용자 병렬 git) — 재개 시 `git log -1`/`git status` 재측정 필수.

> **단일 진입점 (single entry point).** 이 문서 하나로 PlayerBehavior god-component 분해 전체(Task 0~7) + 연출 foundation + directional + cull fix 작업을 무손실 재개할 수 있다.
> spec/plan 은 **gitignored**(다른 머신에 안 따라감) — 임계 사실은 전부 인라인했다(§Pointers 참조).
>
> 작성: 2026-06-03 / 브랜치 `game/module/ingame/temp` / 작성 시점 HEAD `eb17c80`.
> **🔴 이 문서가 다음 둘을 supersede 한다** (배너 부착됨):
> - `doc/handoffs/2026-06-02/2026-06-02-pb-decomposition-foundation-done-resume-handoff.md` (Task5 부분·Task7 미착수 시점 — 이제 둘 다 구현 완료)
> - `doc/handoffs/2026-06-02/2026-06-02-directional-facing-cullfix-session-resume-handoff.md` (directional+cull 만 — Task5/7 미반영)

---

## TL;DR + 다음 행동

PlayerBehavior god-component 분해(Task 0~7)가 **전부 구현 완료**됐다. 남은 건 **커밋 + GUI 육안 검증**뿐.

- **Task 0~4 + 연출 foundation + hit-FX + Task 6 directional** = ✅ **커밋 완료** (HEAD 체인, 최신 directional=`a16eef0`).
- **Task 5 (Carrier 흡수) + Task 7 (PlayerBehavior 철거)** = ✅ **구현 완료·빌드 GREEN·5차원 적대검증 통과·미커밋** (작성 시점 = 사용자가 *staged* 함, 커밋은 미실행).
- **Cull fix (`src/material/pass.h` AlphaTest CullMode=0, D=오른쪽 투명)** = ✅ 적용·빌드 GREEN·**미커밋**(이전 스레드 — 사용자가 GUI 검증 완료 확인함).

**👉 다음 행동 (둘):**
1. `_MyApp_` 실행 → GUI 육안 검증(§아래 체크리스트): 총알 명중/벽 명중, 적 접촉 데미지(i-frame), 적 dissolve, WASD/방향스프라이트/발사 정상, 크래시 0.
2. 정상이면 사용자가 커밋(**커밋은 사용자 권한** — §Verbatim 의 path-scoped 명령). cull fix(`pass.h`)는 별도 커밋.

```bash
# 빌드 + 실행
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
cmake --build --preset ninja --target _MyApp_   # GREEN (sb7 #warning 1건만 허용)
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

> ⚠️ **재개 에이전트에게:** 이 브랜치는 사용자 + World Text 에이전트가 **병렬 커밋·staging·빌드**한다. 이 세션에도 HEAD 가 `e4f79e4`→`eb17c80` 로 움직였다. 재개 직전 반드시 `git log --oneline -8` + `git status --short` **재측정**. 메모리 [[user-parallel-git-and-builds]] 참조.

---

## State of the world (재측정 — 2026-06-03)

```
브랜치: game/module/ingame/temp   (PR 대상 보통 game/main)
HEAD:   eb17c80  [dev] : World Text — 다중행 atlas 폰트 수정(비정사각 타일 + 행 V-flip) + 데모 숫자
```

최근 커밋 (신→구) — ★=내 작업, 그 외=병렬 World Text 트랙:

| SHA | 제목 | 트랙 |
|---|---|---|
| `eb17c80` | World Text — 다중행 atlas 폰트 수정(비정사각 타일) | 병렬(World Text) |
| `e4f79e4` | World Text — 데모 트리거(마우스 클릭 데미지 텍스트) | 병렬 |
| ★`a16eef0` | **[dev] : Playable 4방향** (Task 6 directional) | **내 작업** |
| `39f8bc0`,`58c1ba7`,`6adaf02`,`e3c27df`,`d173298`,`10825cc` | World Text (SJH::text 17모듈) | 병렬 |
| ★`aa684f7` | **PlayableDirector 연출 통합 + sprite/PostFX 엔진지원** (foundation+hit-FX+P4적+GL_BOOL fix) | **내 작업** |
| ★`8f276a9` | **연출 디렉터 (MyApp::Playable 신규 lib)** | **내 작업** |
| (아래) `d44a4a3`(Task4 impulse)/`f1e1a23`(Task0~3+Timer) | PB분해 초기 | 내 작업 |

**Uncommitted (`git status --short`):**
```
# 내 Task 5/7 — 작성 시점 STAGED(사용자가 git add 함), 커밋 미실행:
A  apps/_MyApp_/src/Spawns/Carrier.h               ← 신규(5a/5b 핵심)
D  apps/_MyApp_/src/Spawns/Projectile.h            ← 구 스켈레톤 삭제
M  apps/_MyApp_/src/Entity/Bullet/bullet_factory.h ← BulletContactHandler→Projectile
D  apps/_MyApp_/src/Entity/Bullet/BulletContactHandler.{h,cpp}
M  apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h    ← EnemyContactHandler→ContactCarrier
D  apps/_MyApp_/src/Entity/Enemy/EnemyContactHandler.{h,cpp}
M  apps/_MyApp_/src/Entity/Enemy/EnemyDeathHandler.h ← PB 잔재 주석 일반화
M  apps/_MyApp_/src/Entity/CMakeLists.txt          ← 3 .cpp 줄 제거 + SJH::playable 주석
D  apps/_MyApp_/src/Entity/Player/PlayerBehavior.{h,cpp}
D  apps/_MyApp_/src/Entity/Player/PlayerMovementComponents.h
# 이전 스레드 cull fix — UNSTAGED:
 M src/material/pass.h                             ← AlphaTest CullMode 0 (D=오른쪽 투명 fix)
# 병렬/사용자 (미접근):
 m extern/Catch2 (서브모듈 포인터)  /  ?? doc/Radial*.shader, *HealthBar*.shader (HP바)
 M doc/handoff/...directional-facing-pose-resume-handoff.md (SUPERSEDED 배너)
 M doc/handoff/...pb-decomposition-task4-done-resume-handoff.md
```

**빌드: GREEN ✅** — clean reconfigure + 전체 빌드 `[8/8] Linking CXX executable apps/_MyApp_/_MyApp_` (40MB). 유일 출력 = sb7 의 허용된 `#warning gl.h+gl3.h`.

---

## Task 상태표 (PlayerBehavior 분해 + 연출 + cull)

| Task | 내용 | 상태 | 위치 |
|---|---|---|---|
| **0** | 엔진 `GetComponent<T>` 인터페이스 조회 (`if constexpr(is_base_of<Component,T>)` fast-path 가드) | ✅ 커밋 | `78ca9ee`/`1f6c23f` |
| **1** | `IActorPresentation`+`IImpulsable`+`EFacing`/`EPose`/`Quantize4` | ✅ 커밋 | `82735ed` |
| **2** | `Life` 확장(i-frame+DoDie+sink) → `optional<Timer>` 마이그레이션(계약 보존) | ✅ 커밋 | `1f6c23f`→`f1e1a23` |
| **3** | `EnemyContactHandler`→`IDamageable::DoDamaged` repoint | ✅ 커밋 | (이번 Task5 가 흡수) |
| **4** | `Impulse`(IImpulsable, Dash/Knockback, 미배선 ship) | ✅ 커밋 | `d44a4a3` |
| **foundation** | PlayableDirector + PostFXRegistry + hit-FX(SpriteHitFlash/Dissolve+PostFXTween+HpGrayscale) + GL_BOOL 코어fix + P4적 | ✅ 커밋 | `8f276a9`+`aa684f7` |
| **6** | directional 8그룹(4방향×2포즈) + RD5 + QuantizeByThreshold | ✅ 커밋 | `a16eef0` |
| **5** | `Carrier`(Projectile/ContactCarrier) — Bullet/적 흡수 + 콘택핸들러 2쌍 철거 | ✅ **커밋 `f45211c`** (검증완료) | Carrier.h tracked + 콘택핸들러/PB 삭제 확정 |
| **7** | `PlayerBehavior.{h,cpp}`+`PlayerMovementComponents.h` 철거 | ✅ **커밋 `f45211c`** (검증완료) | 디스크·git index 양쪽 부재 확인 |
| **cull** | `pass.h` AlphaTest CullMode=0 (D=오른쪽 투명) | 🟡 적용·**UNSTAGED·미커밋** | 빌드 GREEN. 단독 커밋만 남음 |

> **Task 0~7 의 *코드*는 전부 존재(구현 완료).** 남은 행정 작업 = Task5/7 + cull 커밋 + GUI 검증.

---

## 이번 세션이 한 일 (Task 5 + 7, 2026-06-03)

### Task 5 — Carrier 추상으로 데미지 배달 흡수

**신규 `apps/_MyApp_/src/Spawns/Carrier.h`** (header-only, §Verbatim 에 전문):
- `CarrierBase : SJH::Scene::Component, Physics::IContactable` — 얇은 배달 베이스. `Deliver(target, knockbackDir)` = `GetComponent<Entity::IDamageable>->DoDamaged` + `GetComponent<Entity::IImpulsable>->DoImpulse`(넉백) + `mOnHitFx`. self-damage 가드(`target==mOwnerEntity`) + `IsActive()` 가드.
- `Projectile : CarrierBase, Entity::IDieable` — 총알. `mAlive` 중복발화 가드 + `DoDie()`→`mPendingDisable`→Update 에서 지연 `SetActive(false)`(Box2D 콜백 중 b2Body 수정 금지 규약). BulletContactHandler 흡수.
- `ContactCarrier : CarrierBase` — 적 접촉. `OnCollisionEnter`/`OnTriggerEnter`→`Deliver(other, vec2(0))`. EnemyContactHandler 흡수(적 body 재사용, 동작 보존).
- **배선**: `bullet_factory.h` `AddComponent<Carrier::Projectile>(dmg)`+`SetOwnerEntity(nullptr)`. `EnemyFactory.h` `AddComponent<Carrier::ContactCarrier>(dmg)`.
- **삭제**: `BulletContactHandler.{h,cpp}`, `EnemyContactHandler.{h,cpp}`, 구 스켈레톤 `Spawns/Projectile.h`. `Entity/CMakeLists.txt` 에서 2 .cpp 줄 제거.

### Task 7 — dead god-component 철거
- **삭제**: `PlayerBehavior.{h,cpp}`(dead), `PlayerMovementComponents.h`(shadow 버그 — `DoForward(vec2)` 가 override 아님, 사용처 0). `Entity/CMakeLists.txt` 에서 `PlayerBehavior.cpp` 줄 제거 + SJH::playable 링크 주석 갱신(링크는 보수적 유지 — umbrella 전파 consumer 보호).
- `EnemyDeathHandler.h:13` "PlayerBehavior::Die 미러" 주석 일반화.
- `BulletSpawnPlayable.{h,cpp}` 는 **이미 `8f276a9` 에서 삭제됨** → Task 7 스킵.

### 구현 중 잡은 것
- **plan 코드 버그 교정**: `Carrier.h::HandleHit` 의 `GetTransform()`(bare) 은 `Component` 에 없음(Actor 전용) → `GetOwner()->GetTransform()`. clang 진단 즉시 포착.
- **헤더가드 컨벤션 교정**: `__TOPDOWNSHOOTER_SPAWN_CARRIER__` → `__TOPDOWNSHOOTER_SPAWNS_CARRIER_H__`(형제 Spawns 헤더 + 프로젝트 `__XXX_H__` 규약).

### 적대적 검증 (16-에이전트 워크플로 — `git show HEAD:<구파일>` old-vs-new 대조)
| 차원 | 확정 실제 이슈 |
|---|---|
| behavior-enemy | **0 (clean)** — ContactCarrier 가 적 접촉 동작 보존 |
| abstraction-knockback | **0 (clean)** — 넉백 dormant(적 IImpulsable 미부착·플레이어 DoImpulse(0,0)) → 보이는 변화 0 |
| lifetime-memory | **0 (clean)** — UAF/dangling/despawn-race 없음 |
| behavior-bullet, scope | 2건 (모두 nit, **zero live impact**) |

**blocker/important/회귀 0.** 확정 2건(전부 현재 런타임 영향 0):
1. **헤더가드 nit** → **수정 완료**.
2. **onHitFx seam (latent)** — `Deliver` 가 `mOnHitFx` 를 IDamageable 무관 무조건 발화하나, 구 `BulletContactHandler` 는 `if(life)` 게이트 안에서만 발화. **단 `SetOnHitFx` 호출처 0 라 dormant.** ⚠ **후속: 임팩트 FX 를 배선하면 벽 명중 시에도 FX 가 터진다(old=없음). 그 시점에 `Deliver` 의 onHitFx 를 damageable 게이트 안으로 넣을 것.** (plan 이 무조건 발화로 작성 — 임의 변경 안 함, 보고만.)

---

## Locked decisions (재논쟁 금지)
- **Task5 = "동작 보존"**: Carrier 는 인터페이스 배달(IDamageable/IImpulsable, Task0 게이트)로 전환하되 *동작* 은 보존. 적 자식 센서(hurtbox) 별도 fixture 토폴로지는 **후속**(적 body 재사용으로 보존).
- **C1**: Carrier `GetComponent<IDamageable>->DoDamaged` + `GetComponent<IImpulsable>->DoImpulse`(=Task4 Impulse 넉백 타겟).
- **Carrier 얇은 베이스**(확정 §6.4): 수명/센서/이동은 subtype 책임.
- **SJH::playable 링크**: PlayerBehavior 철거로 Entity 직접 사용 0 이 됐으나 **보수적 유지**(umbrella 전파 consumer 가 의존할 수 있음 — 제거 시 transitive break 위험).
- **onHitFx seam**: 현 dormant. FX 배선 시 damageable 게이트 권장(위).
- (이전) directional/cull/foundation 잠긴 결정은 supersede 된 `...directional-facing-cullfix...` + `...foundation-done...` 핸드오프 참조.

---

## 병렬 트랙 충돌 매트릭스 (재측정)
| 파일/트랙 | 소유 | 재개 행동 |
|---|---|---|
| `apps/_MyApp_/src/Spawns/Carrier.h` + bullet/enemy factory + 삭제들 | **내 Task5/7 (STAGED)** | ✅ 커밋 대상 |
| `src/material/pass.h` (cull fix, UNSTAGED) | **내 이전스레드** | ✅ 별도 커밋 |
| `apps/_MyApp_/src/Text/*`, `Manager.{h,cpp}`, `src/text/*`, `src/sprite/uniform_atlas*` | **병렬: World Text** | ⛔ 미접근 |
| `apps/_MyApp_/main.cpp` | 병렬(Fog/World Text) | ⛔ 미접근 |
| `apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp` | 병렬(IDE 오픈) | ⛔ Task5b 는 EnemyFactory 만 surgical — EnemyBuilder 미접근(달성) |
| `doc/Radial*.shader`, `*HealthBar*.shader` | 사용자(HP바) | ⛔ 미접근 |
| `extern/Catch2`(서브모듈) | — | ⛔ 미접근 |

**⚠ 사용자 병렬 작업 패턴 (메모리 [[user-parallel-git-and-builds]]):** 사용자가 같은 working tree 에서 staging/커밋/빌드한다(이 세션에 `eb17c80` 병렬 커밋 + 내 Task5/7 staging). **`git add -A`/`git commit`(인덱스 전체) 금지 → 반드시 `git commit <경로>` partial.** 탈락 커밋은 `git reflog` 복구.

---

## Guardrails & conventions
- **커밋 = 사용자 권한**(무단 금지). **path-scoped** 만(`git add -A`/`.` 금지 — 병렬 트랙 휩쓺).
- **`Co-Authored-By` 트레일러 미사용**(프로젝트 컨벤션).
- **테스트 자동 추가 금지**(`no_auto_tests`) — 검증 = 빌드 exit0 + GUI 육안.
- **주석 한국어**, 헤더가드 `__XXX_H__`(#pragma once 금지), `long` 금지(고정폭), 경로 슬래시.
- **미접근**: World Text(`src/Text`,`src/text`,`uniform_atlas`,`Manager`), `main.cpp`, `EnemyBuilder.cpp`, `Quantize4`(읽기전용), 코어 `src/*`(GL_BOOL fix·cull fix 외), `Timer 코어`, 셰이더.
- **빌드**: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_`. **실행**: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`.

---

## GUI 검증 체크리스트 (no_auto_tests → 빌드+육안)
- [ ] 좌클릭 발사 → 총알 적 명중 시 적 HP 감소 + 총알 소멸 / 벽 명중 시 총알 소멸 (`Carrier::Projectile`)
- [ ] 적 접촉 → 플레이어 HP 감소 (i-frame 정상, Task3 동작 유지 — `Carrier::ContactCarrier`)
- [ ] 적 사살 → dissolve 연출 정상 (`EnemyDeathHandler` despawn)
- [ ] WASD 이동 + 4방향 스프라이트 전환 + 발사 조준 응시 + i-frame 정상 (PB 삭제 동작변화 0 — PB 는 dead)
- [ ] D=오른쪽/우향 발사 시 스프라이트 투명 안 됨 (cull fix)
- [ ] 크래시 0

---

## Verbatim recovery — 미커밋 코드 (reset 으로도 안 잃도록)

### 1. 신규 `apps/_MyApp_/src/Spawns/Carrier.h` (전문 — 새 파일이라 reset 시 완전 소실)
```cpp
#ifndef __TOPDOWNSHOOTER_SPAWNS_CARRIER_H__
#define __TOPDOWNSHOOTER_SPAWNS_CARRIER_H__

#include "Entity/Components/Components.Interfaces.h" // IDamageable / IImpulsable
#include "Physics/Components.Interfaces.h"           // IContactable
#include "scene/actor.h"
#include <functional>
#include <utility>
#include <vmath.h>

namespace TopdownShooter::Spawn::Carrier
{
	/// @brief 데미지 배달 공통 베이스 (얇음 — 확정 §6.4). 수명/센서/이동은 subtype 책임.
	class CarrierBase : public SJH::Scene::Component,
	                    public Physics::IContactable
	{
	  public:
		using HitFx = std::function<void(const vmath::vec3 &)>;

		CarrierBase &SetOwnerEntity(SJH::Scene::Actor *o) { mOwnerEntity = o; return *this; }
		CarrierBase &SetDamage(int d) { mDamage = d; return *this; }
		CarrierBase &SetOnHitFx(HitFx fx) { mOnHitFx = std::move(fx); return *this; } // C4

	  protected:
		/// @brief C1 배달: target 의 IDamageable→DoDamaged + (있으면) IImpulsable→DoImpulse 넉백 + onHitFx.
		void Deliver(SJH::Scene::Actor *target, vmath::vec2 knockbackDir)
		{
			if (!target || target == mOwnerEntity || !target->IsActive())
				return;
			if (auto *dmg = target->GetComponent<Entity::IDamageable>())
				dmg->DoDamaged(mDamage);
			if (auto *imp = target->GetComponent<Entity::IImpulsable>())
				imp->DoImpulse(knockbackDir);
			if (mOnHitFx)
				mOnHitFx(target->GetTransform().Translate); // spawn-at-point seam 유지
		}

		SJH::Scene::Actor *mOwnerEntity = nullptr;
		int                mDamage      = 0;
		HitFx              mOnHitFx;
	};

	/// @brief flying + lifetime + self-despawn. BulletContactHandler 흡수 (mAlive 가드 + 지연 despawn).
	class Projectile : public CarrierBase, public Entity::IDieable
	{
	  public:
		explicit Projectile(int damage) { mDamage = damage; }
		void OnEnter() override {}
		void OnExit() override {}
		void Update(float /*dt*/) override
		{
			if (mPendingDisable && GetOwner()) GetOwner()->SetActive(false);
		}
		void DoDie() override { mPendingDisable = true; }
		void OnCollisionEnter(SJH::Scene::Actor *other) override { HandleHit(other); }
		void OnTriggerEnter(SJH::Scene::Actor *other) override { HandleHit(other); }
	  private:
		void HandleHit(SJH::Scene::Actor *other)
		{
			if (!mAlive || !other || !GetOwner()) return;
			mAlive = false;
			// 넉백 방향 = 소유자→타겟 XZ 차분. GetTransform 은 Actor — GetOwner() 경유.
			const vmath::vec3 d = other->GetTransform().Translate - GetOwner()->GetTransform().Translate;
			Deliver(other, vmath::vec2(d[0], -d[2]));
			DoDie();
		}
		bool mAlive         = true;
		bool mPendingDisable = false;
	};

	/// @brief sensor + persistent (적 접촉 데미지). EnemyContactHandler 흡수 — 적 body 재사용(동작 보존).
	class ContactCarrier : public CarrierBase
	{
	  public:
		explicit ContactCarrier(int damage) { mDamage = damage; }
		void OnEnter() override {}
		void OnExit() override {}
		void Update(float /*dt*/) override {}
		void OnCollisionEnter(SJH::Scene::Actor *other) override { Deliver(other, vmath::vec2(0.0f)); }
		void OnTriggerEnter(SJH::Scene::Actor *other) override { Deliver(other, vmath::vec2(0.0f)); }
	};
}; // namespace TopdownShooter::Spawn::Carrier
#endif // __TOPDOWNSHOOTER_SPAWNS_CARRIER_H__
```

### 2. 편집 요약 (삭제는 git 히스토리로 복구 가능 — 구파일은 HEAD `eb17c80` 에 존재)
- `bullet_factory.h`: `#include "Entity/Bullet/BulletContactHandler.h"` → `#include "Spawns/Carrier.h"`. `AddComponent<BulletContactHandler>(cfg.damage)` → `auto* proj = AddComponent<Spawn::Carrier::Projectile>(cfg.damage); proj->SetOwnerEntity(nullptr);` (BulletLifetime 유지).
- `EnemyFactory.h`: `#include "Entity/Enemy/EnemyContactHandler.h"` → `#include "Spawns/Carrier.h"`. `AddComponent<EnemyContactHandler>(cfg.damage)` → `AddComponent<Spawn::Carrier::ContactCarrier>(cfg.damage)`.
- `Entity/CMakeLists.txt`: source 목록에서 `Player/PlayerBehavior.cpp` `Bullet/BulletContactHandler.cpp` `Enemy/EnemyContactHandler.cpp` 3줄 제거. SJH::playable 주석을 "(PB철거 후 직접 사용 0 — consumer 보호 보수적 유지)" 로 갱신.
- `EnemyDeathHandler.h:13`: "PlayerBehavior::Die self-poll 패턴 미러" → "사망 self-poll — Life::IsAlive()==false 감지 시 death FX delegate + despawn".

### 3. `src/material/pass.h` cull fix (UNSTAGED — D=오른쪽 투명)
`DefaultPipelineStateOf(Kind::AlphaTest)` 에서 `/*CullMode*/ GL_BACK` → `/*CullMode*/ 0` (+ 주석 3줄: flipX winding 뒤집힘 + GL_BACK 컬링 투명 버그 방지, AlphaTest=sprite 전용 blast radius).

### 4. path-scoped 커밋 명령 (사용자용)
CMakeLists 가 5b+7 변경을 한 파일에 담아 hunk-split 불가(`git add -p` 미지원) → **Task5+7 단일 커밋 권장**:
```bash
git add apps/_MyApp_/src/Spawns/Carrier.h apps/_MyApp_/src/Spawns/Projectile.h \
  apps/_MyApp_/src/Entity/Bullet/bullet_factory.h \
  apps/_MyApp_/src/Entity/Bullet/BulletContactHandler.h apps/_MyApp_/src/Entity/Bullet/BulletContactHandler.cpp \
  apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h \
  apps/_MyApp_/src/Entity/Enemy/EnemyContactHandler.h apps/_MyApp_/src/Entity/Enemy/EnemyContactHandler.cpp \
  apps/_MyApp_/src/Entity/Enemy/EnemyDeathHandler.h \
  apps/_MyApp_/src/Entity/Player/PlayerBehavior.h apps/_MyApp_/src/Entity/Player/PlayerBehavior.cpp \
  apps/_MyApp_/src/Entity/Player/PlayerMovementComponents.h \
  apps/_MyApp_/src/Entity/CMakeLists.txt
git commit -m "[refactor] : Bullet/적 데미지배달 → Carrier(Projectile/ContactCarrier) + PlayerBehavior 철거 (PB분해 Task5+7)"
# cull fix 는 별도:
git commit src/material/pass.h -m "[fix] : sprite(AlphaTest) 패스 컬링 비활성 — flipX winding 투명 버그(D=오른쪽)"
```
(작성 시점 Task5/7 은 이미 staged 라 위 `git add` 는 재확인용 — `git status` 로 확인 후 commit.)

---

## Pointers (참조 깊이)
- **spec/plan (🔴 gitignored — 다른 머신에 안 따라감):** `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md`(§13 잠긴결정+§4 분해테이블) + `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md`(Task0~7 완전코드). → 임계 사실 위 §에 인라인.
- 추적 설계 핸드오프: `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md`(§13/§14 잠긴결정).
- **이 문서가 supersede:** `2026-06-02-pb-decomposition-foundation-done-resume-handoff.md`, `2026-06-02-directional-facing-cullfix-session-resume-handoff.md`. (둘 다 직전 PB/연출 진입점 — 이제 stale.)
- 메모리: [[next_work_playable]](현 활성), [[playable_director_foundation]], [[user-parallel-git-and-builds]], [[propertyblock_gl_bool_gap]].

---

## Change log (append-only)
- **2026-06-03 (본 문서 생성):** Task 5(Carrier 흡수)+Task 7(PB 철거) 구현 완료·빌드 GREEN·5차원 적대검증 통과(blocker/회귀 0). 신규 `Spawns/Carrier.h`. 검증 latent 1건=onHitFx seam(dormant). HEAD `eb17c80`(병렬 World Text 폰트 커밋). Task5/7 staged·미커밋, cull fix unstaged·미커밋. 직전 `...foundation-done...` + `...directional-cullfix...` 핸드오프 supersede.
- **2026-06-03 (오케스트레이터 검증·갱신):** Task 5/7 = staged→**커밋 `f45211c "deadcode 제거"`** 확정(`git log -- Spawns/Carrier.h`/`--diff-filter=D PlayerBehavior.cpp` 둘 다 `f45211c`). **빌드 GREEN 독립 재검증**(exit 0, error 0, `_MyApp_` 39.9MB 링크). → **분해 Task 0~7 코드 완료 + 빌드 GREEN.** 남은 것 = GUI 육안 검증 + `pass.h` cull 단독 커밋(`M`). 검증 중 HEAD churning `8942e89→eb17c80→6c4d7c4→f45211c`(사용자 병렬 git/reset). TL;DR 상단 배너 + Task표(5/7=커밋) 갱신.
