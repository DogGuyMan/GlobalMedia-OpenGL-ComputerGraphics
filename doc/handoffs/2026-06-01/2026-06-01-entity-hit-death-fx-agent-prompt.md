# 다른 Claude Agent 용 프롬프트 — Entity 피격/사망 FX (Layer A+B)

> 아래 코드블록 전체를 새 Claude Agent 세션에 그대로 붙여 사용. (자기완결 — 별도 문서 안 읽어도 됨.)
> 정본 spec(참고): `doc/superpowers/specs/2026-06-01-entity-hit-death-fx-design.md`.

---

```
[ROLE]
너는 C++17/CMake OpenGL 탑다운 슈터 프로젝트(/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics, 브랜치 game/module/ingame/temp)의 구현 에이전트다.
"Entity 피격 깜빡임 + 사망 디졸브 연출" 기능의 **엔진(Layer A) + 게임플레이(Layer B)** 레이어를 구현한다.
연출 셰이더(billboard_atlas.fs)는 이미 완성돼 있고, CPU 측 uniform 송신 + 사망 타이밍 지연만 없다. 그 두 가지를 만든다.

[중요 — 진행 중 작업과의 경계]
이 저장소는 동시에 "PlayerBehavior god-component 분해"(Task 0~7)가 진행 중이다 (Task 0·1·2 커밋됨, Task 3 미커밋 워킹트리). 너의 작업은 그것과 충돌하지 않도록 아래 경계를 엄수한다:
- **너는 PlayerSpriteDirector 를 만들지 않는다.** 그 파일(apps/_MyApp_/src/Entity/Player/PlayerSpriteDirector.{h,cpp})은 분해 Task 6 소유다. 연출의 "구동"(ReactDamaged/ReactDied 가 효과를 켜고 끄는 로직)도 거기서 한다 — 너의 범위 아님.
- **너의 범위 = 딱 3개 파일**: ① src/sprite/sprite_component.h ② src/sprite/sprite_component.cpp ③ apps/_MyApp_/src/Entity/Components/LifeComponents.h. 그 외 어떤 파일도 수정 금지.
- **main.cpp 절대 수정 금지** (다른 에이전트가 Fog/PostFX 리팩토링 중 — 경합).
- **billboard_atlas.{vs,fs} 셰이더 수정 금지** (이미 완성).
- 워킹트리에 너와 무관한 미커밋 변경(EnemyContactHandler 등)이 있을 수 있다 — 건드리지 마라.

[절대 규칙]
- 단위 테스트/TDD 금지 (프로젝트 no_auto_tests 정책). 검증은 빌드 + 육안.
- 커밋 금지 (git commit/add 금지). 구현 후 보고만 — 사용자가 리뷰 후 커밋한다.
- 주석은 한국어. #pragma once 미사용(헤더가드 유지). long 금지(고정폭). 경로 슬래시.

[배경 사실 — 검증됨]
- 셰이더 billboard_atlas.fs 에 연출 uniform 이 이미 선언/구현돼 있다(너는 송신만): uEnableHit(bool), uTime(float) → step(0,sin(uTime*30)) White/Red 깜빡임 / uEnableDissolve(bool), uDissolveTex(sampler2D), uDissolveThreshold(float 0→1), uDissolveOutlineColor(vec3), uDissolveOutlineThickness(float) → threshold 미만 discard + 경계 아웃라인.
- SpriteRenderer::Update 는 현재 uUvRect/uFlipX/uTint 3개만 송신. 신규 uniform 송신처 0개.
- uniform API: SJH::Uniforms::SetFloat/SetInt/SetVec3/SetVec4/SetTexture(Material&, "name", ...). bool 은 SetInt(0/1), sampler2D 는 SetTexture(mat, "name", const Texture*, unit). 헤더 src/material/material_uniforms.h. (store-only — draw 시 송신.)
- SpriteRenderer 는 per-instance MaterialInstance 를 가져 sprite 별 uniform 격리됨. 게임 무관 POD-ish 데이터 홀더(atlas/frameIdx/tint/flipX 공개 멤버 + Update 송신).
- 디졸브 노이즈 자산: apps/_MyApp_/resources/texture/dissolve.png 존재(로드는 분해 Task 6 의 sink 가 함 — 너는 SpriteRenderer 에 const Texture* dissolveTex 필드만 둔다, 기본 nullptr).
- SJH::Texture 타입은 resource_registry 에 있다. SpriteRenderer.h 에서는 전방 선언(namespace SJH { class Texture; })으로 충분.

========================================================================
[LAYER A] src/sprite/sprite_component.{h,cpp} — SpriteRenderer FX uniform
========================================================================

(A-1) src/sprite/sprite_component.h:
- 상단 forward 선언에 Texture 추가: 기존 `class UniformAtlas;` 옆(또는 namespace SJH 안)에 `class Texture;` 추가. (UniformAtlas 는 SJH::Sprite, Texture 는 SJH — namespace 주의: 파일 상단 `namespace SJH { class Texture; }` 또는 적절 위치.)
- SpriteRenderer 클래스의 기존 공개 멤버(atlas/frameIdx/size/tint/flipX) 아래에 추가:

    // === 피격 깜빡임 (billboard_atlas.fs uEnableHit/uTime) ===
    // sink(분해 Task 6 PlayerSpriteDirector)가 enableHit 만 on/off, uTime 은 본 컴포넌트 자체 clock.
    bool enableHit = false;

    // === 사망 디졸브 (billboard_atlas.fs uEnableDissolve/...) — sink 가 구동 ===
    bool                enableDissolve           = false;
    float               dissolveThreshold        = 0.0f;   // 0→1 (사라지는 정도)
    float               dissolveOutlineThickness = 0.05f;
    vmath::vec3         dissolveOutlineColor     = vmath::vec3(1.0f, 0.5f, 0.0f);
    const SJH::Texture* dissolveTex              = nullptr; // resources/texture/dissolve.png (sink 주입)

- private(클래스 맨 아래 private: 추가) 에 uTime 용 자체 clock:

    private:
        float mEffectClock = 0.0f;   // uTime 용 free-running clock (Update 에서 += dt)

(A-2) src/sprite/sprite_component.cpp:
- 상단 include 는 이미 material/material_uniforms.h, resource_registry/resource_registry.h 포함(SetTexture/Texture 사용 가능 — 부족하면 추가).
- SpriteRenderer::Update(float dt) 의 시그니처에서 dt 를 사용하도록 바꾸고(현재 /*dt*/ 주석처리됨), 기존 3 uniform 송신 뒤에 추가:

    void SpriteRenderer::Update(float dt)
    {
        if (!atlas || !Material)
            return;
        mEffectClock += dt;   // uTime free-running clock

        Uniforms::SetVec4(*Material, "uUvRect", atlas->GetUVRect(frameIdx));
        Uniforms::SetFloat(*Material, "uFlipX", flipX ? -1.0f : 1.0f);
        Uniforms::SetVec4(*Material, "uTint", tint);

        // === 피격 ===
        Uniforms::SetInt(*Material, "uEnableHit", enableHit ? 1 : 0);
        Uniforms::SetFloat(*Material, "uTime", mEffectClock);

        // === 디졸브 ===
        Uniforms::SetInt(*Material, "uEnableDissolve", enableDissolve ? 1 : 0);
        Uniforms::SetFloat(*Material, "uDissolveThreshold", dissolveThreshold);
        Uniforms::SetFloat(*Material, "uDissolveOutlineThickness", dissolveOutlineThickness);
        Uniforms::SetVec3(*Material, "uDissolveOutlineColor", dissolveOutlineColor);
        if (dissolveTex)
            Uniforms::SetTexture(*Material, "uDissolveTex", dissolveTex, /*unit=*/1); // uAtlas=unit0
    }

- backward-compat: 기본 enableHit=false/enableDissolve=false → 셰이더 분기 skip → 기존 모든 sprite 비주얼 변화 0 이어야 한다.

========================================================================
[LAYER B] apps/_MyApp_/src/Entity/Components/LifeComponents.h — 사망지연
========================================================================
현재 Life 클래스(분해 Task 2 에서 확장됨)는 다음을 이미 가진다: mMaxHp/mCurHp/mIFrameSeconds/mInvincibleTimer/mDeathFxFired/mOnDeathFx/mSink, ctor 3개, SetIFrameSeconds/SetOnDeathFx/IsInvincible, OnEnter(sink 캐시)/OnExit/Update/IsAlive/GetHp/GetMaxHp/DoDamaged/DoDie. **이 중 Update 와 DoDie 만 수정 + 멤버 3개·setter 1개 추가.** 나머지 무변경.

(B-1) 멤버 추가 (mSink 아래):
        float mDeathDelaySeconds = 0.0f;   // 0 = 즉시(현행). >0 = 사망 연출(디졸브) 동안 SetActive 지연
        bool  mDying             = false;
        float mDeathTimer        = 0.0f;

(B-2) setter 추가 (SetOnDeathFx 옆):
        Life &SetDeathDelaySeconds(float s) { mDeathDelaySeconds = s; return *this; }

(B-3) Update 교체 (현재: i-frame 감산 + death poll):
        void Update(float dt) override
        {
            if (mInvincibleTimer > 0.0f) mInvincibleTimer -= dt;
            if (mDying)   // 사망 연출 진행 중 — 타이머 만료 시 비활성
            {
                mDeathTimer -= dt;
                if (mDeathTimer <= 0.0f && GetOwner()) GetOwner()->SetActive(false);
                return;
            }
            if (!mDeathFxFired && !IsAlive()) DoDie();   // 안전망
        }

(B-4) DoDie 교체 (현재: one-shot → ReactDied → onDeathFx → 즉시 SetActive):
        void DoDie() override
        {
            if (mDeathFxFired) return;
            mDeathFxFired = true;
            const vmath::vec3 pos = GetOwner() ? GetOwner()->GetTransform().Translate : vmath::vec3(0.0f);
            if (mSink) mSink->ReactDied(pos);            // 디졸브 시작 (sink 가 구동 — 분해 Task 6)
            if (mOnDeathFx) mOnDeathFx(pos);
            if (mDeathDelaySeconds <= 0.0f)
            {
                if (GetOwner()) GetOwner()->SetActive(false);   // 즉시 (기본/현행)
            }
            else
            {
                mDying      = true;                       // 지연 — Update 가 만료 시 비활성
                mDeathTimer = mDeathDelaySeconds;
            }
        }

- backward-compat: 기본 mDeathDelaySeconds=0 → 즉시 SetActive(false) = 현행 동작(적·미설정 플레이어). 회귀 0. 플레이어 0.5s 주입은 분해 Task 6 가 함 — 너는 캐퍼빌리티만 추가(주입하지 마라).
- gameplay→presentation 읽기 금지: Life 는 sink 의 디졸브 완료를 묻지 않는다. 사망지연 길이는 후일 sink 디졸브 길이(~0.5s)와 독립 매칭.

========================================================================
[검증]
========================================================================
1. 빌드 (필수): cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics && cmake --build --preset ninja --target _MyApp_
   기대: exit 0, Linking CXX executable apps/_MyApp_/_MyApp_. (sprite_component.h 변경이라 다수 TU 재컴파일.)
2. backward-compat: 기본값(enableHit/enableDissolve=false, deathDelay=0)에서 기존 비주얼·동작 변화 0 이어야 함(육안 — 평소처럼 실행). cd build_ninja/apps/_MyApp_ && ./_MyApp_ → 플레이어/스프라이트 정상.
3. (선택) FX 시각 스모크: 효과가 셰이더에 닿는지 확인하려면 *임시로* 플레이어 SpriteRenderer 에 enableHit=true (또는 enableDissolve=true; dissolveThreshold 를 0.5 정도; dissolveTex 를 ResourceRegistry 로 dissolve.png 로드 후 대입) 강제 → White/Red 깜빡임 / 디졸브 렌더 육안 확인 → **임시코드 반드시 revert**. 단 main.cpp 는 건드리지 말 것(임시 토글은 PlayerBuilder 등 비경합 위치, 확인 후 원복). full 통합(피격→깜빡, 사망→디졸브)은 분해 Task 6 이 담당하니 여기선 "셰이더가 uniform 에 반응한다"만 확인하면 충분.

[Self-review]
- 변경 파일 정확히 3개(sprite_component.h/.cpp, LifeComponents.h)인가? PlayerSpriteDirector/main.cpp/셰이더 미수정?
- SpriteRenderer 신규 필드 6개 + mEffectClock + Texture 전방선언 추가? Update 가 dt 사용 + 신규 uniform 7개 송신?
- Life Update/DoDie 만 수정 + 멤버3·setter1 추가? 나머지(DoDamaged/OnEnter 등) 무변경? 기본값에서 회귀 0?
- 빌드 exit 0? git status --short 에 너의 3파일만(+무관한 기존 미커밋은 미관여로 구분)?

[보고]
DONE / DONE_WITH_CONCERNS / BLOCKED 중 하나로 — 변경 요약(file 별) + 빌드 마지막 줄 + self-review 결과 + git status(네 3파일 명시). 사람용이 아닌 구조화 보고. 커밋하지 마라.
```

---

## 사용 메모 (오케스트레이터/사용자용)

- 위 프롬프트는 **Layer A+B 만** (충돌 0, 지금 진행 가능). **Layer C(연출 구동)는 내 분해 Task 6** — 다른 에이전트에게 시키지 말 것(PlayerSpriteDirector 병렬 생성 = 충돌).
- 다른 에이전트 완료·커밋 후 → 내 분해 Task 4→7 재개. **Task 6 이 Layer A(SpriteRenderer FX 필드)+B(Life 사망지연)에 의존**하므로 그 전에 A·B 가 들어와 있어야 함.
- 커밋 메시지(권장): `[feat] : Entity 피격/사망 FX 엔진 레이어 — SpriteRenderer FX uniform + Life 사망지연` (Co-Authored-By 미사용, 프로젝트 컨벤션).
