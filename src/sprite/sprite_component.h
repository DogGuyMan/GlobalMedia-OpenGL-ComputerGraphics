#ifndef __SJH_SPRITE_SPRITE_COMPONENT_H__
#define __SJH_SPRITE_SPRITE_COMPONENT_H__

#include "render/mesh_renderer.h"   // base class — Unity SpriteRenderer is_a MeshRenderer 정통
#include <vmath.h>

namespace SJH
{
    class Texture;   // forward — dissolveTex 핸들 참조 (resource_registry 거주, SJH 네임스페이스)
}

namespace SJH::Sprite
{
    class UniformAtlas;   // forward — 핸들 참조

    /// @brief Sprite 전용 MeshRenderer — billboard plane + per-instance Material 자동 셋업.
    /// @details
    ///   ### 자동 해결되는 공유 자원 (ResourceRegistry 고정 키, '_' 접두로 user namespace 격리)
    ///     - "_sprite_plane"               — Mesh::CreatePlane() (sprite 전용 plane, 최초 1회)
    ///     - "_sprite_billboard_program"   — billboard_atlas.vs/fs program (최초 1회)
    ///     - "_sprite_billboard"           — template SharedMaterial (program + AlphaTest)
    ///     - "_sprite_inst_<N>"            — per-instance MaterialInstance (sprite 마다 unique)
    ///
    ///   ### 책임 분할
    ///     - 본 클래스 (MeshRenderer 상속): atlas / frameIdx / tint / flipX / size 데이터
    ///       + 매 Update 마다 uniform (uUvRect / uTint / uFlipX) 자동 송신
    ///     - SpriteSequencePlayable (sibling): 시간 따라 frameIdx 갱신  본 컴포넌트에 기록
    ///       (M3.5 SpriteAnimator 폐기 — Playable 인터페이스 정통)
    ///
    ///   ### 사용
    ///   @code
    ///   auto* spr = actor->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
    ///   spr->tint = vmath::vec4(1.0f, 0.5f, 0.5f, 1.0f);
    ///   // 옵션: SpriteSequencePlayable 부착 시 frameIdx 자동 진행
    ///   SJH::SpriteSequence::SpriteFrameClip clip{0, atlas->FrameCount(), 4.0f};
    ///   auto* seq = actor->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(spr, &clip);
    ///   seq->SetIsLoop(true);
    ///   seq->Play();
    ///   @endcode
    ///
    ///   ### 1-frame lag 주의
    ///   SpriteSequencePlayable.Update 가 본 컴포넌트보다 *나중에* 돌면 uniform 송신은 이전 frameIdx 기반.
    ///   render() 다음 frame 에 catch-up — 4fps atlas 기준 무시 가능 (1/15 of a tick @ 60fps render).
    class SpriteRenderer : public SJH::Scene::MeshRenderer
    {
    public:
        /// @brief atlas 주입 — plane + program + per-instance Material 자동 해결 (ctor 안 1회).
        /// @details atlas==nullptr 이면 Material 도 nullptr — 빈 SpriteRenderer (테스트/지연 셋업).
        explicit SpriteRenderer(UniformAtlas* atlas = nullptr);

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;

        // === 게임 무관 sprite 데이터 (public 멤버 직접 접근 — POD-ish) ===
        UniformAtlas* atlas    = nullptr;
        int           frameIdx = 0;
        vmath::vec2   size     = vmath::vec2(1.0f, 1.0f);   // 월드 단위 (현재 미사용 — Transform.Scale 우선)
        vmath::vec4   tint     = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        bool          flipX    = false;

        // === 피격 깜빡임 (billboard_atlas.fs uEnableHit/uTime) ===
        // sink(분해 Task 6 PlayerSpriteDirector)가 enableHit 만 on/off, uTime 은 본 컴포넌트 자체 clock.
        bool enableHit = false;

        // === 사망 디졸브 (billboard_atlas.fs uEnableDissolve/...) — sink 가 구동 ===
        bool                enableDissolve           = false;
        float               dissolveThreshold        = 0.0f;   // 0→1 (사라지는 정도)
        float               dissolveOutlineThickness = 0.05f;
        vmath::vec3         dissolveOutlineColor     = vmath::vec3(1.0f, 0.5f, 0.0f);
        const SJH::Texture* dissolveTex              = nullptr; // resources/texture/dissolve.png (sink 주입)

    private:
        float mEffectClock = 0.0f;   // uTime 용 free-running clock (Update 에서 += dt)
    };
} // namespace SJH::Sprite

#endif // __SJH_SPRITE_SPRITE_COMPONENT_H__
