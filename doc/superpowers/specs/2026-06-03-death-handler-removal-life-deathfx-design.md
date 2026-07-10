# Death Handler 제거 + Life 사망 일원화 + 위치기반 DeathFx Playable — 설계 (2026-06-03)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 브랜치 `game/module/ingame/temp` / 상태: **설계 승인됨, 구현 대기**.
> 출처 작업지시: `doc/최종으로해야하는작업들.md` lines 1-23 (Death Handler 섹션 + DeathFx Playable).

---

## 1. 배경 / 문제

현재 사망 처리는 **두 메커니즘이 중복**되어 있고, 위치 정보가 새고 있다.

- **`EnemyDeathHandler`** (`Entity/Enemy/EnemyDeathHandler.{h,cpp}`) — `Update(dt)`에서 매 프레임 `Life::IsAlive()`를 **폴링**해 사망을 감지하고 `mOnDeathFx(pos)` + `SetActive(false)`를 호출하는 컴포넌트. 그러나 **현재 미부착**: `EnemyFactory.h:49`의 `if (cfg.onDeathFx)` 가드가 false (WaveController가 `onDeathFx` 미설정). 즉 죽은 코드.
- **`Life`** (`apps/_MyApp_/src/Entity/Components/LifeComponents.h`) — 이미 사망 전 로직을 보유: `DoDamaged→DoDie` 주경로 + `Update` 안전망(`if(!mDeathFxFired && !IsAlive()) DoDie()`). `DoDie()`가 `ReactDied(pos)`(디졸브) + `mOnDeathFx(pos)`(폭발 seam) + `mDieTimer` 지연을 구동. 즉 **EnemyDeathHandler의 역할은 이미 Life가 전담**.
- **위치 누수**: `Life::DoDie`가 `pos`를 계산해 `mSink->ReactDied(pos)`로 넘기지만, `PlayableDirector::ReactDied(pos)`는 `pos`를 **버리고** `Play("death")`만 호출. 사망 위치 폭발이 director를 통해 못 흐름.
- `mOnDeathFx` std::function seam은 **아무도 set 안 함** → death 폭발이 현재 안 터짐.

### 목표
1. `EnemyDeathHandler` 삭제 (Update 폴링 제거). 사망 구동은 Life 단일.
2. 사망 시 **물리 트리거 즉시 off** (디졸브 중 사망체가 안 밀치고 안 때림).
3. death 폭발을 **director Playable("deathfx")** 로 일원화하고, `ReactDied(pos)`의 위치가 실제로 폭발 위치로 흐르게 한다.

---

## 2. 결정 요약

| # | 결정 | 근거 |
|---|---|---|
| D1 | EnemyDeathHandler 삭제 (Update 폴링 제거) | 미부착 redundant 폴러. Life가 이미 전담 → **런타임 변화 0** |
| D2 | physics-off 주체 = **Life::DoDie 안 lazy `FindPhysics`** | 사용자 결정. 캐시 의존 추가 없이 사망 1회 조회 |
| D3 | physics-off 수단 = **`Physics::SetSensor(true)`** | 컴포넌트의 런타임 isTrigger 레버 (PhysicsComponent.h:71). 충돌 응답 차단 |
| D4 | DeathFx 위치 = **`PlayableDirector::Play(key, pos)` 확장** | 사용자 결정. `ReactDied(pos)`의 버려지던 pos를 director 통해 폭발로 전달 |
| D5 | director→VFX 역의존 회피 = **Client 인터페이스 `IPositionedPlayable`** | `PlayableBase`(SJH 코어) 무수정 + director가 VFX에 의존 안 함 |
| D6 | **`mOnDeathFx` seam 제거** (`SetOnDeathFx`/`EnemyConfig::onDeathFx`/`EnemyDeps::onDeathFx` 동반) | death 폭발이 "deathfx" Playable로 일원화 → seam은 미사용 dead code. *(B 결정 — 검토 가능)* |
| D7 | Life box2d 의존 격리 = **`LifeComponents.cpp` 신설**, `DoDie`를 .cpp로 이동 | header-only `LifeComponents.h`에 box2d 안 끌어옴. Entity는 이미 game_deps PRIVATE 링크 |
| D8 | "deathfx" 등록 = **EnemyBuilder** (VFX/Manager 접근 자연) | AttachEntityPresentation은 generic 유지. `explosion` 미등록 시 안전 skip |

### 범위 밖 (후속)
- WaveController 생성/파괴 **Observer Register/Unregister** (`doc/...작업들.md` line 25).
- Player deathfx 등록 (이번엔 enemy 한정).
- **`explosion` .efk warmup 등록** — deathfx visual의 전제. 현재 미등록으로 보임(E 미해결 — §6 참조).

---

## 3. 구현 (코드 예제)

### A. EnemyDeathHandler 삭제 — 동작 변화 0

**삭제:** `<Entity>/Enemy/EnemyDeathHandler.h`, `<Entity>/Enemy/EnemyDeathHandler.cpp`

**`Entity/CMakeLists.txt`** — 소스 목록에서 제거 + LifeComponents.cpp 추가:
```cmake
add_library(myapp_entity STATIC
    BaseEntity.cpp
    apps/_MyApp_/src/Entity/Components/WeaponComponents.cpp
    <Components>/LifeComponents.cpp        # <- 신설 (DoDie 물리 접촉부 격리, D7)
    apps/_MyApp_/src/Entity/Player/PlayerActor.cpp
    apps/_MyApp_/src/Entity/Player/PlayerEntity.cpp
    apps/_MyApp_/src/Entity/Player/PlayerHand.cpp
    apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.cpp
    # <Enemy>/EnemyDeathHandler.cpp        <- 삭제
)
```

**`apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h`** — include + EnemyConfig 필드 + 부착 제거:
```cpp
// 삭제: #include "<Entity>/Enemy/EnemyDeathHandler.h"

struct EnemyConfig
{
    b2World*           world;
    vmath::vec2        pos;
    SJH::Scene::Actor* playerTarget;
    int   hp     = ENEMY_HP;
    float speed  = ENEMY_SPEED;
    int   damage = ENEMY_DAMAGE;
    // 삭제: EnemyDeathHandler::DeathFx onDeathFx;   ← 타입 출처가 사라짐 + seam 폐기(D6)
};

// CreateEnemyActor 본문에서 삭제:
//   if (cfg.onDeathFx)
//       actor->AddComponent<EnemyDeathHandler>(cfg.onDeathFx);
```

**`apps/_MyApp_/src/Bootstrap/EnemyBuilder.h`** — `EnemyDeps::onDeathFx` 제거 (D6):
```cpp
// 삭제: std::function<void(const vmath::vec3&)> onDeathFx;
```
EnemyBuilder.cpp의 `cfg.onDeathFx = deps.onDeathFx;` 복사 라인도 동반 제거.

> **왜 안전한가**: EnemyDeathHandler는 현재 미부착(가드 false)이라 Update 폴링이 안 돈다. `mOnDeathFx`는 아무도 set 안 한다. → 삭제해도 **런타임 변화 0**.

---

### B. Life::DoDie — 물리 off + .cpp 격리

**`LifeComponents.h`** — `DoDie` 인라인 구현 → 선언으로, `mOnDeathFx`/`SetOnDeathFx` 제거:
```cpp
// 멤버에서 제거: std::function<void(const vmath::vec3 &)> mOnDeathFx;
// public 에서 제거: Life &SetOnDeathFx(...) { ... }

void DoDie() override;   // <- 구현은 LifeComponents.cpp (box2d 의존 격리, D7)
```
(`DoDamaged`/`Update`의 `DoDie()` 호출부는 무변경 — 선언만 보고 링커가 해소.)

**`<Entity>/Components/LifeComponents.cpp`** — 신설:
```cpp
#include "apps/_MyApp_/src/Entity/Components/LifeComponents.h"
#include "apps/_MyApp_/src/Physics/PhysicsComponent.h"   // FindPhysics + Physics::SetSensor (box2d — .cpp 한정 격리)

namespace TopdownShooter::Entity::Components
{
	void Life::DoDie()
	{
		if (mDeathFxFired) return;               // one-shot
		mDeathFxFired = true;
		const vmath::vec3 pos = GetOwner() ? GetOwner()->GetTransform().Translate : vmath::vec3(0.0f);

		if (mSink) mSink->ReactDied(pos);        // 디졸브("death") + 사망위치 폭발("deathfx", pos) — §D 참조

		// 물리 트리거 즉시 off — 디졸브 지연 동안 사망체가 안 밀치고 안 때림 (lazy, 사망 1회).
		if (auto *phys = Physics::Components::FindPhysics(GetOwner()))
			phys->SetSensor(true);

		if (mDieTimer)
			mDieTimer->Reset();                  // 지연 발동 — Update 가 만료 시 SetActive(false)
		else if (GetOwner())
			GetOwner()->SetActive(false);        // 즉시 (지연 없을 때)
	}
}
```

> `pos` 계산이 `SetSensor` 전에 오는 순서 보존(센서 off가 transform에 영향 없지만 명료성). `mOnDeathFx(pos)` 라인은 제거 — 폭발은 이제 `ReactDied(pos)→Play("deathfx",pos)` 경로.

---

### C. PlayableDirector::Play(key, pos) + IPositionedPlayable

**`<Playable>/IPositionedPlayable.h`** — 신설 (Client 표식 인터페이스, D5):
```cpp
#ifndef __TOPDOWNSHOOTER_PLAYABLE_IPOSITIONED_PLAYABLE_H__
#define __TOPDOWNSHOOTER_PLAYABLE_IPOSITIONED_PLAYABLE_H__

#include <vmath.h>

namespace TopdownShooter::Playable
{
	/// @brief Play(key, pos) 가 spawn 위치를 주입할 수 있는 Playable 표식.
	///        EffekseerPlayable 등 위치를 갖는 leaf 가 구현. director 는 이 인터페이스로만 통신
	///        (director→VFX 역의존 회피 + SJH 코어 PlayableBase 무수정).
	class IPositionedPlayable
	{
	  public:
		virtual ~IPositionedPlayable() = default;
		virtual void SetSpawnPosition(vmath::vec3 pos) = 0;
	};
}

#endif // __TOPDOWNSHOOTER_PLAYABLE_IPOSITIONED_PLAYABLE_H__
```

**`apps/_MyApp_/src/Playable/PlayableDirector.h`** — 오버로드 선언:
```cpp
void Play(const std::string &key);                    // 기존
void Play(const std::string &key, vmath::vec3 pos);   // <- 신규 (위치 주입 후 재생)
```

**`apps/_MyApp_/src/Playable/PlayableDirector.cpp`** — 구현 + ReactDied 갱신:
```cpp
#include "<Playable>/IPositionedPlayable.h"   // dynamic_cast 대상

void PlayableDirector::Play(const std::string &key, vmath::vec3 pos)
{
	auto it = mPlayables.find(key);
	if (it == mPlayables.end() || !it->second.playable) return; // 미등록 = silent no-op
	// 위치를 받는 Playable(EffekseerPlayable 등)이면 주입. 아니면 위치 무시(예: SpriteDissolve).
	if (auto *positioned = dynamic_cast<IPositionedPlayable *>(it->second.playable.get()))
		positioned->SetSpawnPosition(pos);
	it->second.playable->Stop();
	it->second.playable->Play();
	it->second.playing = true;
}

void PlayableDirector::ReactDied(vmath::vec3 pos)   // 시그니처 동일, pos 더는 안 버림
{
	Play("death");          // 스프라이트 디졸브 (actor-bound, 위치 불필요)
	Play("deathfx", pos);   // 사망 위치 폭발 (위치기반, 미등록이면 no-op)
}
```

> `Play(key)`/`Play(key,pos)` 코드 중복은 작아 그대로 둔다(위치 주입 한 줄 차이). 필요 시 후속에 private 헬퍼로 합칠 수 있으나 YAGNI.

---

### D. EffekseerPlayable — IPositionedPlayable 구현

**`apps/_MyApp_/src/VFX/EffekseerPlayable.h`** — 다중 상속 + override:
```cpp
#include "<Playable>/IPositionedPlayable.h"

class EffekseerPlayable : public SJH::Playable::PlayableBase,
                          public TopdownShooter::Playable::IPositionedPlayable
{
  public:
	EffekseerPlayable(::Effekseer::ManagerRef manager, SJH::Effect *effect,
	                  const vmath::vec3 &spawnPos = vmath::vec3(0.0f),
	                  TrackPolicy track = TrackPolicy::Static);

	void SetSpawnPosition(vmath::vec3 pos) override { mSpawnPos = pos; }  // <- Play(key,pos) 가 호출
	// ...
};
```
`OnPlay()`는 무변경 — 이미 `mSpawnPos`를 읽어 `mManager->Play(effect, Vector3D(mSpawnPos))` 한다(EffekseerPlayable.cpp:31). `SetSpawnPosition`이 Play 직전에 `mSpawnPos`를 갱신하므로 OnPlay가 갱신값 사용.

---

### E. "deathfx" 등록 (EnemyBuilder)

**`apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp`** — AttachEntityPresentation 반환 캡처 후 등록:
```cpp
auto *director = AttachEntityPresentation(*enemy, pres);   // 기존: 반환 무시 → 캡처

// 사망 위치 폭발 — director "deathfx" Playable. ReactDied(pos) 가 Play("deathfx", pos) 로 구동.
// explosion 미등록(warmup 안 됨)이면 FindEffect=nullptr → skip(안전 no-op).
if (auto *exp = SJH::ResourceRegistry::Get().FindEffect("explosion"))
	director->Register("deathfx", std::make_unique<VFX::EffekseerPlayable>(
	    TopdownShooter::Manager::Get().VFX().GetManager(), exp,
	    vmath::vec3(0.0f), VFX::TrackPolicy::Static));
```
필요 include 추가: `"Manager.h"`, `"apps/_MyApp_/src/VFX/EffekseerPlayable.h"`, `"apps/_MyApp_/src/Playable/PlayableDirector.h"`(이미 있음). Bootstrap은 MyApp::VFX/MyApp::Manager 링크 보유.

> 라인 84-93의 기존 주석 "[B] 폭발/spark 는 delegate(onDeathFx)가 별도 트리거(유지)"는 갱신 — 이제 director "deathfx"가 담당.

---

## 4. 영향 / 동작 보존 분석

| 변경 | 분류 | 근거 |
|---|---|---|
| A: EnemyDeathHandler 삭제 + onDeathFx 플러밍 제거 | **런타임 0** | 미부착·미설정 dead code |
| B: `mOnDeathFx` 제거 | **런타임 0** | 아무도 set 안 함 |
| B: physics-off (`SetSensor(true)`) | **신규(의도)** | 사망체가 디졸브 중 충돌 응답 정지 |
| C/D: `Play(key,pos)` + IPositionedPlayable | **신규 메커니즘** | 기존 `Play(key)` 무변경 (오버로드) |
| C: `ReactDied(pos)` 가 `Play("deathfx",pos)` 추가 | **신규(의도)** | "deathfx" 미등록이면 no-op → 등록 전까진 동작 0 |
| E: "deathfx" 등록 | **신규(의도)** | `explosion` 미등록이면 skip |

**핵심**: A/B(제거)는 동작 보존(dead code), B(physics-off)/C/D/E는 의도된 신규 동작. `Play("deathfx")`는 미등록 시 silent no-op이라 단계적 안전.

---

## 5. 검증

`no_auto_tests` — 단위테스트 없음. 검증 = **빌드 GREEN + GUI 육안**:
```bash
cmake --build --preset ninja --target _MyApp_      # exit 0 (sb7 #warning 1건만 허용)
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
- 빌드: EnemyDeathHandler 참조 잔존 없음(EnemyFactory/CMake), LifeComponents.cpp 컴파일, IPositionedPlayable/Play(pos) 링크.
- GUI: 적 사망 시 ① 디졸브 정상 ② **디졸브 중 적이 플레이어를 안 밀치고 안 때림**(SetSensor) ③ `explosion` 등록 시 사망 위치 폭발.

---

## 6. 미해결 / 리스크

- **E `explosion` warmup**: deathfx visual의 전제. 현재 `explosion`/`spark` 키는 `apps/_MyApp_/src/Spawns/CombatSequences.cpp`에서 `FindEffect`로 *쓰이지만* `CreateEffect` 등록 위치 미발견 → 미등록 가능성. 미등록이면 deathfx는 안전 no-op(폭발 안 보임). **이번 범위 밖**(자산 배선 별도)이나, visual 확인하려면 `explosion` .efk warmup(예 AudioWarmup류 VFX warmup)이 선행돼야 함. → spec 리뷰 시 "이번에 포함할지" 결정.
- **D6 `mOnDeathFx` 제거**: pasted `DoDie`와 유일하게 달라지는 점. 미사용이라 안전하나, 향후 비-Playable death seam이 필요하면 되살려야 함. director "deathfx"로 충분하다는 전제.
- **SetSensor 의미**: `SetSensor(true)`는 충돌 *응답*을 끄지만 contact *이벤트*는 계속 보고됨(센서). 접촉 데미지(`ContactCarrier`)가 사망 후에도 이벤트로 데미지를 줄 수 있으면, 핸들러에 `IsAlive()` 게이트 추가가 후속 필요. (현 ContactCarrier 동작 확인은 구현 단계 점검 항목.)
- **병렬 git**: 사용자 활발 편집 중. 구현 전 `git status`/관련 파일 재측정 + path-scoped 커밋.

---

## 7. 변경 파일 목록

**신규(2)**: `<Entity>/Components/LifeComponents.cpp`, `<Playable>/IPositionedPlayable.h`
**수정(8)**: `LifeComponents.h`, `apps/_MyApp_/src/Playable/PlayableDirector.h`, `apps/_MyApp_/src/Playable/PlayableDirector.cpp`, `apps/_MyApp_/src/VFX/EffekseerPlayable.h`, `apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h`, `apps/_MyApp_/src/Bootstrap/EnemyBuilder.h` (EnemyDeps::onDeathFx 제거), `apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp` (deathfx 등록 + onDeathFx 복사 제거), `Entity/CMakeLists.txt`
**삭제(2)**: `<Entity>/Enemy/EnemyDeathHandler.h`, `<Entity>/Enemy/EnemyDeathHandler.cpp`
