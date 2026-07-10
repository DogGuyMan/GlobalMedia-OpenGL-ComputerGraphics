# Directional Facing/Pose 스프라이트 연출 — 설계 (2026-06-02)

> 분해 Task6 의 directional 슬라이스. HitBlink/Dissolve(SpriteFxPlayable)·HP grayscale 은 이미 완료 — 본 spec 은 **방향 전환**만.
> 입력 핸드오프: `doc/handoffs/2026-06-02/2026-06-02-directional-facing-pose-agent-prompt.md`. **커밋 안 함**(사용자 관리). 브랜치 `game/module/ingame/temp`.

## 목표
탑다운 슈터(_MyApp_) 플레이어가 **이동/조준 방향에 따라 4방향(Front/Back/Left/Right) × 2포즈(Idle/Move) 스프라이트로 전환**된다. 방향 경계 각도는 사용자가 `Constants.h` 에서 튜닝.

## 아키텍처 판단 (clean-ddd-hexagonal)
표현/렌더 기능이라 full DDD 전술(Aggregate/Repository/CQRS/Event)은 **과함 → 미적용**. 적용되는 원칙만:
- **Dependency Rule (안쪽으로만)**: 링크 토폴로지가 강제 — `Entity→Playable` 은 순환이라 그룹 빌드는 PlayerActor(Entity)가 아닌 **PlayerBuilder(bootstrap=composition root, Entity+Playable 둘 다 link)** 에서.
- **Ports & Adapters**: `IActorPresentation` = 포트(연출 sink), `PlayableDirector` = 어댑터(유일 구현), `PlayerController`(입력 driver) 는 **포트 `IActorPresentation*` 로만** 대화(InputHandler 는 Playable 미링크라 강제).
- **Single-writer (SRP)**: facing/pose 계산·기록은 **PlayerController 단독(RD5)**, director 는 *적용*(가시성 토글)만 — velocity 안 읽음.

데이터 흐름(단방향): 입력 → PlayerController(계산) → 포트(SetFacing/SetPose) → PlayableDirector(상태) → Apply(SetActive 토글) → 화면. 게임로직(HP/물리) 무관.

---

## §1 책임 분리
```
PlayerController ──SetFacing/SetPose──▶ IActorPresentation(포트) ◀──impl── PlayableDirector ──SetActive──▶ 그룹 child actor
 (RD5 단일 작성자)                                                          (8그룹 Apply)        bPart 애니=child scene-tick([A])
```

## §2 PlayableDirector 추가분 (기존 Register/Play/Stop/Has/Update·"fire"/"hit"/"death"·React 매핑 **불변**)
PlayableDirector.h:
```cpp
#include <array>
namespace SJH::Sprite { class SpriteRenderer; }   // fwd (실타입 .cpp)

// public:
struct DirGroup { std::array<SJH::Sprite::SpriteRenderer*, 4> layers{}; };  // E·H·B·F 핸들만(애니 미보유)
void RegisterGroup(Entity::EFacing f, Entity::EPose p, const DirGroup& g);  // 빌드 시 1회
void RefreshDirectional();                                                   // = Apply (AddChild 후 1회)
void SetFacing(Entity::EFacing f) override;   // 빈 훅(47) → 본문 .cpp
void SetPose(Entity::EPose p) override;       // 빈 훅(48) → 본문 .cpp
// private:
static int idx(Entity::EFacing f); static int idx(Entity::EPose p);
void Apply();
DirGroup        mGroups[4][2]{};
Entity::EFacing mFacing = Entity::EFacing::Front;
Entity::EPose   mPose   = Entity::EPose::Idle;
```
PlayableDirector.cpp (`#include "scene/actor.h"` for GetOwner/SetActive):
```cpp
void SetFacing(f){ if(f==mFacing)return; mFacing=f; Apply(); }   // velocity 안 읽음
void SetPose  (p){ if(p==mPose)  return; mPose  =p; Apply(); }
void Apply(){
  for f in 0..3: for p in 0..1:
    bool active = (f==idx(mFacing) && p==idx(mPose));
    for layer in mGroups[f][p].layers:
      if (layer && layer->GetOwner()) layer->GetOwner()->SetActive(active);  // ★ SetActive 토글(Visible 아님)
}
```
**결정**: 숨김 = `SetActive(false)`(완전 비활성/CPU 절약; 2프레임 애니라 freeze/resume 무차별). DirGroup 은 렌더러 핸들만(애니 제외 — SetActive 가 actor 비활성으로 애니까지 정지하므로 핸들 불필요). bPart 애니는 child 의 `SpriteSequencePlayable`(scene-tick [A]) — director 가 Play/Stop 안 함.

## §3 PlayerBuilder — 8그룹 빌드 + RegisterGroup (PlayerActor 미접근)
1. **단일방향 무력화**: `pac.sprite.direction` 설정(:62) 제거 → nullptr 유지(PlayerActor 의 단일-4레이어 블록 비활성).
2. director 부착(:73) 직후, AddChild 전 — `kGroups`(IDLE 4 → MOVE 4) 순회하며 각 그룹의 레이어 child 빌드:
   - `FindUniformAtlas`(공유 PNG 중복키 회피) → 없으면 `CreateUniformAtlas`.
   - child Actor 생성 → `AddChild(spriteActor)` → `AddComponent<SpriteRenderer>(atlas)`; `flipX=t.Flip`, `QueueOffset=t.DrawOrder`. **초기 가시성 안 건드림**(32레이어 active 로 생성).
   - `t.ColCount>1` 이면 `SpriteSequencePlayable(spr, {0,ColCount,kFps})` + `SetIsLoop(true).Play()` ([A], child 소유 — DirGroup 미보유).
   - `dg.layers[li]=spr` → `director->RegisterGroup(f,p,dg)`.
3. **AddChild 후**(enter 로 32레이어 OnEnter/애니 시작 뒤): `director->RefreshDirectional()` → Front/Idle 외 28레이어 `SetActive(false)`. (inactive-at-enter 엣지 회피.)
- ⚠ PlayerResult 호환 스캔(첫 SpriteSequencePlayable)은 이제 Front/Move B파트를 잡음 — 무해(nullable). 보고 명시.

## §4 PlayerController — RD5 단일 작성자 (인터페이스 sink, flipX 핵 제거)
PlayerController.h (private): `IActorPresentation* mSink=nullptr; EFacing mLastFacing=Front; float mAttackWindowSec=0.15f; float mAttackTimer=0.0f;`
Update() — :171-177 교체(EulerRot[1]=mAimAngleY 손궤도 유지, flipX 핵 제거):
```cpp
if(!mSink) mSink = owner->GetComponent<E::IActorPresentation>();   // lazy(=director, 인터페이스 조회)
if(mSink){
  if(mAttackTimer>0) mAttackTimer-=dt; bool attacking=(mAttackTimer>0);
  vmath::vec2 velXZ(mInputValue[0], mInputValue[2]);               // ★ mInputValue 리셋(:161) 전
  bool moving = (dot(velXZ,velXZ) > 0.001f);
  vmath::vec2 aimXZ(mAimDirection[0], mAimDirection[2]);
  E::EFacing facing = attacking ? P::QuantizeByThreshold(aimXZ, P::PLAYER_FACING_THRESHOLD)
                    : moving    ? P::QuantizeByThreshold(velXZ, P::PLAYER_FACING_THRESHOLD)
                                : mLastFacing;
  E::EPose pose = (attacking||moving) ? E::EPose::Move : E::EPose::Idle;
  mSink->SetFacing(facing); mSink->SetPose(pose); mLastFacing=facing;
}
```
OnFirePressed(): `mAttackTimer = mAttackWindowSec;` (발사 후 0.15s facing=조준 — 하이브리드 발동).
정리: `mCachedSprite`(h:101)+forward(h:18)+(미사용 시)`sprite_component.h` include 제거.

## §5 스코프 / 커밋
- **OWN 3파일**: `PlayableDirector.{h,cpp}` · `PlayerBuilder.cpp` · `PlayerController.{h,cpp}`. + **§6 데이터** `apps/_MyApp_/src/Playable/Constants.h`(튜닝 const 추가 — 데이터만).
- **미접근**: `PlayerActor.{h,cpp}`(Entity→Playable 순환), foundation 시그니처/"fire"·"hit"·"death", 코어(`src/playable`·`src/timer`·셰이더·`property_block_setter`·`sprite_component`), `Spawns`·`EnemyFactory`·`EnemyBuilder`·`Life`·`main.cpp`, `Components.Interfaces.h`(읽기만 — Quantize4 미수정).
- **커밋 안 함**(사용자 관리). path-scoped, 무관 dirty 미접근.

## §6 Facing 임계 각도 config (Constants.h — 사용자 튜닝)
```cpp
// 방향 판정 임계 각도 (degree, [0,360), 0°=오른쪽(+X), 반시계 +). vec2={start,end}; start>end → 0° wrap.
// 매핑: Back=화면 위(Up,~90°), Front=화면 아래(Down,~270°). 360° 빈틈없이 덮는 게 결정적.
struct FacingThresholdConfig { vmath::vec2 Back, Front, Left, Right; };   // 순서 = Up,Down,Left,Right
const FacingThresholdConfig PLAYER_FACING_THRESHOLD = {
    { 45.0f,135.0f},   // Back  (Up)
    {225.0f,315.0f},   // Front (Down)
    {135.0f,225.0f},   // Left
    {315.0f, 45.0f},   // Right (wrap)
};
```
quantizer (PlayerController inline helper — EFacing in-scope; Constants.h 는 데이터만):
```cpp
// dir=(x,z) → θ = normalize360(degrees(atan2(-z, x)))  [기본 식 — screen-right=+X=0°, screen-up=-Z=90°(W=-Z)]
// → 4범위 중 θ 포함 필드 반환(EFacing 직결, wrap=start>end 처리). 빈틈/겹침(no-match) → fallback = mLastFacing.
// Quantize4(Components.Interfaces.h)는 미접근. atan2 부호가 자산과 안 맞으면 -z↔z 부호로 GUI 보정.
EFacing QuantizeByThreshold(vmath::vec2 dir, const FacingThresholdConfig& cfg);
```
- 기본값 = 90° 사분면(현 Quantize4 거동). 사용자가 숫자만 고쳐 경계 이동/확장.
- §4 하이브리드 구조 불변, `Quantize4` → `QuantizeByThreshold` 교체만.

## 검증
- 빌드: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_` → exit 0(sb7 gl3 #warning만). 링크 에러(Entity→Playable / InputHandler→Playable) 나면 구조 위반.
- 실행 육안: WASD 이동 → 몸통 Front/Back/Left/Right 전환 + 정지=Idle/이동=Move. 발사 시 0.15s 조준 방향 응시.
- ⚠ θ 0점/회전부호가 자산과 안 맞으면 → **Quantize4 본체 미수정**, `Constants.h` 임계 각도(또는 controller 의 atan2 부호)로 보정. muzzle.efk [error] 1건은 무관.

## 리스크 / 후속
- 좌표 부호: world XZ → θ 매핑은 GUI 1회 보정(임계 각도 튜닝으로 흡수). PhysicsMovement world XZ→b2Vec2(x,-z) 트랩 인지.
- PlayerResult 첫-애니 스캔 모호(Front/Move) — 무해.
- bPart 애니가 멀티프레임으로 늘면 SetActive freeze 가 위상 끊김 → 그땐 Visible 재고(현 2프레임은 무차별).
