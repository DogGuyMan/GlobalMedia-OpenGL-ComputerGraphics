# Handoff — Task 9: 적 피격/사망 FX 배선 (enemy hit/death FX)

> **수신자**: PlayerBehavior 분해(spec `2026-06-01-playerbehavior-decomposition-design`/plan 동명)를 끝낸 *다음* Claude Agent, 또는 그와 병행하는 Agent.
> **목적**: 이미 완성·커밋된 M6 단발 FX 함수(`SpawnHitSpark`/`SpawnEnemyDeathFX`)를 **게임플레이에 트리거 배선** — 적이 총알에 맞으면 spark+피격음, 죽으면 폭발+사망음+despawn.
> **작성 시점**: 2026-06-01. 브랜치 `game/module/ingame/temp`. clean-ddd-hex Workflow(5 에이전트) 검증.
> **진행 단계**: 설계·검증 완료, **코드 0줄**. 분해 §13/§14 seam 과 정합. (정본 = `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md`.)
> **산출 경로**: `doc/`(추적). `doc/`는 gitignore.

---

## 0. 한 줄 요약 + 핵심 통찰

M6 FX 함수는 **존재·커밋·작동하나 호출자가 0**. 진짜 막힌 것은 **(블로커) 적 스폰 루프 `WaveController` 가 아예 인스턴스화 안 됨 → 런타임 적 0마리** + FX 트리거 미배선. **분해와 독립적인 최소 슬라이스가 즉시 가능**(아래 §8 권고).

---

## 1. 검증된 현 상태 (this-pass 코드 정독)

1. **`WaveController` = dead code, 인스턴스화 0회** (블로커). `Stage/WaveController.{h,cpp}` 컴파일되나 `new`/`AddComponent<WaveController>`/`make_unique` 어디에도 없음. ctor `(b2World*, Actor* spawnParent, Actor* playerActor, float arenaHalfExtent)` — **audio/vfx/reg/fxRoot/onDeathFx/SequenceContext 멤버 없음**. `SpawnEnemy()` 가 `EnemyConfig{...,damage=10}` 빌드+`AddChild(CreateEnemyActor)` 하지만 **`cfg.onDeathFx` 안 set**. → 런타임 적 0.
2. **M6 FX 함수 = 존재·커밋·호출자 0**. `Spawns/CombatSequences.{h,cpp}`: `SpawnHitSpark(const SequenceContext&, const vmath::vec3&)` / `SpawnEnemyDeathFX(...)` / `SpawnPickupChime(...)`. 전부 `if(!ctx.fxRoot) return;` + vfx/reg/audio null-check → **미존재 시 안전 no-op**. 유일한 live 호출 = `main.cpp:285` `SpawnVfxInstance`(ImGui VFX-테스트, **전투 아님**). 3 함수 모두 caller 0.
3. **R1a 잔여 = 전부 커밋·intact, 단 injector 없음(미배선)**. `EnemyConfig.onDeathFx`[EnemyFactory.h:25] + `if(cfg.onDeathFx) AddComponent<EnemyDeathHandler>`[:55-56]. `EnemyDeathHandler` self-poll[EnemyDeathHandler.cpp:6-16]: `Update()`가 `GetComponent<Life>()` 폴, `!IsAlive()`면 `mOnDeathFx(pos)` 1회(mDead latch)+`SetActive(false)`. `BulletContactHandler::SetOnHitFx`[BulletContactHandler.h:30-31] live; `HandleHit`가 `DoDamaged` 후 `if(mOnHitFx) mOnHitFx(other->Transform.Translate)`. **`SetOnHitFx` caller 0, `BulletConfig`에 onHitFx 필드 없음**.
4. **분해 seam — Step 0 DONE, Step 1~7 미완**. Step 0 landed: `src/scene/actor.h:94-95` `GetComponent<T>` gate = `is_polymorphic_v<T>` → **`GetComponent<IDamageable>()`/`<IActorPresentation>()` 이제 작동**. 미완: `Life::DoDie()` 빈[LifeComponents.h:41-43], Life에 i-frame/`SetOnDeathFx`/sink 없음; `EnemyContactHandler.cpp:13-14` 여전히 dead `PlayerBehavior::Hit` 호출; `Spawns/Projectile.h` Carrier = 빈 스켈레톤.
5. **main.cpp Composition Root에 SequenceContext deps 전부 scope 내**: `reg`[153] / `Manager::Get().Audio()`,`.VFX()`[156] / `phys.World()`[155] / `dir.Root()` / `mFxRoot`[생성 259, sweep 324]. 단 **전투용 SequenceContext 조립은 어디에도 없음**(AudioWarmup이 BGM용 local ctx만). 자원 키 `'spark'`/`'explosion'` **미등록**(muzzle[227] + VFX-테스트 6키만); FMOD `event:/Hit`·`/EnemyDeath`·`/Pickup` **bank에 없음**(BGM/Damaged/Slash만).

---

## 2. 갭 (의존 순서)

- **GAP A (블로커 — 적 없음)**: `WaveController` 미인스턴스화 → 적 스폰 루프 전체 죽음. **붙일 대상이 없음.** Task 9는 WaveController를 **생성** + onDeathFx delegate(또는 SequenceContext)를 **운반하도록 확장**해야 함(현 ctor에 FX 훅 없음).
- **GAP B (트리거 — 적 사망 FX)**: `SpawnEnemyDeathFX` + `EnemyConfig.onDeathFx` 주입 seam + (post)`Life::SetOnDeathFx` 다 있으나 **람다 바인딩하는 자 없음**. → 적 스폰 시 `onDeathFx = [ctx](const vmath::vec3& p){ Spawns::SpawnEnemyDeathFX(ctx, p); }` 주입(ctx = Composition Root에서 값-복사).
- **GAP C (트리거 — 피격/임팩트 FX)**: `SpawnHitSpark` + 임팩트 seam(pre: `BulletContactHandler::SetOnHitFx` / post: Carrier 임팩트) 있으나 미바인딩. 총알에 onHitFx 스레딩 필요(`BulletConfig`에 필드 추가 or 발사처에서 `SetOnHitFx`).
- **SECONDARY (자산, 코드 아님)**: 트리거 다 배선해도 `'spark'`/`'explosion'` Effekseer 미등록(`resources/vfx/hit.efk`가 'spark' alias 가능) + `event:/Hit`·`/EnemyDeath` FMOD 미저작 → **무음·무시각**. 코드는 안전 no-op이라 ship OK, 단 수용("spark+sound")은 자산-블록.

---

## 3. pre vs post 분해 seam (델리게이트 SHAPE 동일 — owner만 이동)

Task 9는 분해 *후* 사용이 전제 → **post seam 이 authoritative**. 단 `std::function<void(const vmath::vec3&)>` 람다는 동일·portable, host 클래스만 바뀜.

| | PRE (오늘 작동, 분해 독립) | POST (authoritative) |
|---|---|---|
| **적 사망 FX** | `EnemyConfig.onDeathFx`→`EnemyDeathHandler` self-poll(`IsAlive()`)→`onDeathFx(pos)`+`SetActive(false)`. Step 의존 0 | `Life::DoDie()`가 `Life::SetOnDeathFx` 1회 발화+`SetActive(false)` (player+enemy 단일 death 기구). 주입이 `EnemyConfig.onDeathFx`→`Life::SetOnDeathFx(...)`로 이동. **Step 2 필요 + Step 7(EnemyDeathHandler 흡수)=OPTIONAL·승인필수** |
| **피격/임팩트 FX** | `BulletContactHandler::SetOnHitFx`(피격체 Transform.Translate). Step 의존 0 | Carrier가 `SetOnHitFx` 흡수(§13.5/§14-Step5). Bullet→Projectile-Carrier, 적 몸통→child-sensor ContactCarrier. `target->GetComponent<IDamageable>()->DoDamaged`(Step 0 게이트로 가능). **Step 5 필요** |

> **재확인 규칙**: Task 9 착수 시 `LifeComponents.h`(`SetOnDeathFx`/non-empty `DoDie`?) + `Projectile.h`(Carrier 구현?) + `EnemyContactHandler.cpp`(repoint?)를 **다시 읽어** 어느 Step이 landed인지 보고 경로 분기. (현 핸드오프 시점 = Step 0만.)

---

## 4. 배선 단계 (S0~S5)

| S | 작업 | 파일 | 검증 |
|---|---|---|---|
| **S0** | **분해 상태 재확인**(가정 금지) — Life::SetOnDeathFx/DoDie? EnemyContactHandler repoint? Carrier 구현? gate(이미 done). 나머지 Task를 landed에 따라 분기 | LifeComponents.h, EnemyContactHandler.cpp, Projectile.h, actor.h:94-95 | grep(빌드 불요, read 게이트) |
| **S1** | **전투 SequenceContext 조립** (Composition Root). POD-of-ptr, 람다에 값-복사 | `main.cpp` startup (mFxRoot 생성 259 + 플레이어 빌드 264 *이후*) | `Spawns::SequenceContext combatCtx{&Manager::Get().Audio(), &Manager::Get().VFX(), &reg, &phys.World(), &dir.Root(), mFxRoot};` → 빌드 |
| **S2** | **WaveController 생성 + 사망 FX delegate 운반** (확장) | WaveController.{h,cpp}(멤버+setter), main.cpp(플레이어 *이후* 생성) | WaveController에 `std::function<void(const vmath::vec3&)> mOnEnemyDeathFx` + `SetOnEnemyDeathFx(...)`(Spawns include 회피=inward). `SpawnEnemy()`에서 `cfg.onDeathFx = mOnEnemyDeathFx;`. main.cpp **264 이후**(player=mSpriteActor 필요)에 `dir.Root()` 자식 host actor에 WaveController 부착(playerActor=mSpriteActor, world=&phys.World(), arenaHalfExtent=StageConfig 벽), delegate=`[combatCtx](const vmath::vec3& p){ Spawns::SpawnEnemyDeathFX(combatCtx, p); }`. **빌드+실행: 적이 ~3s마다 가장자리 스폰(`[Wave N]` 로그), 쏴서 HP0→despawn** (FX는 자산 전까지 no-op, 크래시 없음 확인) |
| **S3** | **피격 FX 배선** (pre: bullet / post: Carrier) | PRE: `bullet_factory.h`(`BulletConfig.onHitFx` 필드 + `CreateBulletActor`에서 `handler->SetOnHitFx`) + 발사처(Weapon::UseWeapon/PlayerController). POST(Step5시): Carrier 임팩트 seam | PRE: `if(cfg.onHitFx) handler->SetOnHitFx(cfg.onHitFx);`, 발사처에서 `[combatCtx](const vmath::vec3& p){ Spawns::SpawnHitSpark(combatCtx,p); }` 스레딩(PlayerBuilder onFire 패턴). **빌드+실행: 적 명중 시 총알 despawn(기존)+크래시 없음** (spark는 자산 후 S5) |
| **S4** | (post-Step3 의존) 적 몸통박치기 데미지 경로 확인 — Step 3 repoint 됐으면 player-hit 작동. **bullet→enemy hit/death FX엔 무관** | EnemyContactHandler.cpp | player-hit FX가 scope일 때만 |
| **S5** | **(자산 — 수용 필수, 코드 독립)** `'spark'`/`'explosion'` Effekseer 등록 + `event:/Hit`·`/EnemyDeath` FMOD 저작 | main.cpp VFX블록(227-252) + `resources/banks/` | `reg.CreateEffect(vfxs.GetManager(),"spark",u"resources/vfx/hit.efk")`(alias 가능) + explosion .efk 소싱 + FMOD Studio 이벤트 저작. **빌드+실행: 명중→spark+음, 사망→폭발+음+despawn (진짜 수용 관찰)**. 콘텐츠 작업 — 사용자에 flag |

---

## 5. 미결정 (사용자 질의)
- **WaveController 인스턴스화 OWNER**: `dir.Root()` 자식 전용 GameController/spawner actor(권고) vs Stage Actor vs Player. ⚠ **제약: WaveController는 플레이어 actor 필요(mSpriteActor, main.cpp:261 이후) + Stage는 player 전(254) 빌드 → CreateStageActor 안에 못 넣음.** 264 이후 신규 child actor 권고.
- **WaveController ctx 스레딩**: full `SequenceContext` 멤버(Stage→Spawns 헤더 결합) vs **bare `std::function<void(vec3)>` delegate(권고 — inward, PlayerBuilder onFire 관용)**.
- **사망 기구**: `EnemyConfig.onDeathFx`+`EnemyDeathHandler`(오늘 작동, 독립) vs `Life::SetOnDeathFx`(Step 2 + **Step 7 승인필수**). **Step 7(EnemyDeathHandler 흡수)는 §14 OPTIONAL·승인필수 — 무단 폐기 금지.**
- **피격 seam**: pre `BulletContactHandler::SetOnHitFx`(BulletConfig.onHitFx 추가) vs post Carrier(Step 5 필요, 현 빈 스켈레톤).
- **combatCtx가 발사처에 도달하는 법**: BuildPlayer/weapon config로 스레딩 vs capture-free 람다가 내부 `Manager::Get()` 접근(PlayerBuilder onFire 선례). 발사 경로(Weapon::UseWeapon→bullet_factory) 추적 필요.
- **자산 존재**: spark/explosion/event 미존재 → alias/소싱/저작 vs 코드만 ship(no-op 안전)하고 자산은 후속.
- **접촉점 근사**: 피격 FX가 피격체 `Transform.Translate`(진짜 contact-manifold 아님 — `IContactable::OnCollisionEnter(other)`가 manifold 미노출). BulletContactHandler 기존 관례와 일치 — 수용 가능?

---

## 6. 절대 하지 말 것
- `PlayerBehavior`/FX 슬롯 부활 금지(분해 A2/§10, Step 7 삭제 예정). FX = onFire/SetOnHitFx/Life::SetOnDeathFx delegate만.
- **Entity/Stage 코드가 `Spawns/CombatSequences.h` include / `Spawns::` 직접 호출 금지.** Entity→Spawns는 `std::function` delegate로만 inward: 람다는 **Composition Root(main.cpp)**에서 combatCtx와 함께 빌드해 주입. (PlayerBuilder onFire/onDamage 선례.)
- 적↔플레이어 직접 도달 금지. Carrier가 `target->GetComponent<IDamageable>()->DoDamaged`로 배달(Step 0 게이트). concrete `GetComponent<PlayerBehavior>` 도달 금지(Step 3 고치는 load-bearing 버그).
- **WaveController를 `CreateStageActor`(main.cpp:254) 안에 생성 금지** — player(261) 전 실행, WaveController는 player 필요. **264 이후** 생성.
- spawn-at-point delegate(onHitFx/onDeathFx)를 `IActorPresentation` OnXXX sink로 접지 말 것(§13.2 마지막 bullet — spawn-at-point는 별개).
- 자산 존재 가정 금지(컴파일·실행은 안전 no-op이나 자산 전엔 무시각·무음 — 수용 caveat 명시).
- `doc/`(gitignore)에 추적문서 금지 — `doc/`(단수).
- 단위 테스트 자발 추가 금지([[no_auto_tests]]) — 빌드+실행(스폰/despawn/FX 관찰) 검증.
- **Step 7(EnemyDeathHandler→Life::SetOnDeathFx 흡수) 사용자 승인 없이 실행 금지**(§14 OPTIONAL).

---

## 7. 진입점 파일 (verbatim)
- `Stage/WaveController.h:20-21`(ctor — FX/ctx 멤버 없음, 확장 대상), `WaveController.cpp:39-54`(SpawnEnemy — `cfg.onDeathFx` 미set = 주입 지점), `:56-80`(Update 스폰 루프)
- `Entity/Enemy/EnemyFactory.h:25`(`EnemyConfig.onDeathFx`), `:55-56`(조건부 `AddComponent<EnemyDeathHandler>`)
- `Entity/Enemy/EnemyDeathHandler.{h:19-20, cpp:6-16}`(self-poll: `mOnDeathFx(Translate)`+`SetActive(false)`)
- `Entity/Components/LifeComponents.h:41-43`(`DoDie` 빈 — Step 2 미완), `:45-54`(`DoDamaged→DoDie` Template-Method, SetOnDeathFx/i-frame 미존재)
- `Entity/Bullet/BulletContactHandler.{h:30-31, cpp:31-32}`(`SetOnHitFx`/HandleHit), `bullet_factory.h:15-23`(`BulletConfig` — onHitFx 필드 없음), `:50`(SetOnHitFx 미호출)
- `Entity/Enemy/EnemyContactHandler.cpp:13-14`(dead `PlayerBehavior::Hit` — Step 3 대상, bullet→enemy FX엔 무관)
- `Spawns/CombatSequences.{h:10-16, cpp:9-32}`(3 시그니처; SpawnHitSpark→`FindEffect("spark")`+`event:/Hit`, SpawnEnemyDeathFX→`"explosion"`+`event:/EnemyDeath`; 전부 ctx.fxRoot 가드+null-check no-op)
- `Spawns/SequenceContext.h:15-23`(6 raw-ptr POD)
- `Spawns/Projectile.h:17-80`(Carrier 스켈레톤 — Step 5 대상)
- `main.cpp:151-156`(deps scope), `:254-257`(CreateStageActor=player 전), `:259`(mFxRoot), `:261-264`(BuildPlayer, mSpriteActor), `:279-287`(SpawnVfxInstance 선례), `:324`(sweep); `reg.CreateEffect 'muzzle':227` (spark/explosion 없음)
- `src/scene/actor.h:94-95`(gate=is_polymorphic — Step 0 DONE), `:182-194`(typeid/dynamic_cast)
- `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md` §13.2(sink+spawn-at-point seam), §13.5(Carrier C1), §14(Step 0~7), §14 Step7(EnemyDeathHandler→Life OPTIONAL·승인)

---

## 8. 의존성 + 권고 (★ 최소 슬라이스)

**분해 Step 의존**:
- Step 0(GetComponent gate): **이미 DONE**. Carrier IDamageable 배달 가능.
- 적 사망 FX: `Life::SetOnDeathFx` 경로 = Step 2(+Step 7 승인) 필요 / **`EnemyConfig.onDeathFx`+`EnemyDeathHandler` 경로 = 의존 0(오늘 작동)**.
- 피격 FX: `BulletContactHandler::SetOnHitFx` = 의존 0 / Carrier 경로 = Step 5 필요(현 스켈레톤).
- 몸통박치기 player-hit = Step 3/5(scope일 때만).

**★ 권고 — 최소 Task 9 슬라이스 (분해 Step 1~7 무관, 즉시 가능)**:
> `WaveController 인스턴스화` + `EnemyConfig.onDeathFx`/`EnemyDeathHandler`로 적 사망 FX + `BulletContactHandler::SetOnHitFx`로 피격 FX. **Step 0(완료)만 의존.** 분해와 병행/선행 가능.
>
> 분해가 landed되면(Step 2 Life::SetOnDeathFx / Step 5 Carrier) **S0에서 재확인 후 authoritative seam(Life/Carrier)으로 스위치**. 델리게이트 람다는 동일하므로 이전 비용 낮음.

자산(S5)은 별도 콘텐츠 작업 — 코드 배선(S1~S3)은 자산 없이도 안전(no-op)하게 ship 가능, 단 수용 관찰은 자산 후.

---

## 9. 변경 기록
| 일자 | 변경 |
|---|---|
| 2026-06-01 | Task 9 핸드오프 작성 — clean-ddd-hex Workflow(5 에이전트) 검증. WaveController 미인스턴스화(블로커) + M6 FX 함수 caller 0 + R1a 커밋·미배선 + 분해 Step 0만 landed 확인. pre/post 분해 seam 매핑 + S0~S5 배선 + 미결정 + 최소 슬라이스 권고(분해 독립). 코드 미변경. |
