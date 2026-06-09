/**
 * @file sprite_component.h
 * @brief Sprite 전용 MeshRenderer - billboard plane + per-instance Material + 셰이더 uniform 자동 동기화.
 *
 * @details
 *  ### 책임
 *  - @c UniformAtlas* 를 주입받아 billboard plane + program + per-instance Material 을 최초 1회 자동 해결.
 *  - 매 @c Update 에서 atlas UV rect / tint / flipX / roll / 피격 / 디졸브 uniform 을 Material 에 동기화.
 *  - @c SpriteSequencePlayable (sibling Component) 이 @c frameIdx 를 갱신하면 다음 Update 에서 UV 재계산.
 *
 *  ### 비-책임
 *  - [X] frameIdx 시간 진행 - @c SpriteSequencePlayable 책임.
 *  - [X] 공유 자원(@c Mesh / @c Program / template @c Material) 수동 생성 - @c ResourceRegistry 경유 자동 해결.
 *  - [X] 디졸브 텍스처 로드 - 외부 sink(@c PlayerSpriteDirector 등)가 @c dissolveTex 주입.
 *
 * @note 내부 ResourceRegistry 키 컨벤션('_' 접두 - 사용자 namespace 격리):
 *       @c _sprite_plane / @c _sprite_billboard_program / @c _sprite_billboard / @c _sprite_inst_N.
 */

#ifndef __SJH_SPRITE_SPRITE_COMPONENT_H__
#define __SJH_SPRITE_SPRITE_COMPONENT_H__

#include "render/mesh_renderer.h"   // base class - Unity SpriteRenderer is_a MeshRenderer 정통
#include <vmath.h>

namespace SJH
{
    class Texture;   // forward - dissolveTex 핸들 참조 (resource_registry 거주, SJH 네임스페이스)
}

namespace SJH::Sprite
{
    class UniformAtlas;   // forward - 핸들 참조

    /**
     * @brief Sprite 전용 MeshRenderer - billboard plane + per-instance Material 자동 셋업.
     * @details
     *  ### 자동 해결되는 공유 자원 (ResourceRegistry 고정 키, '_' 접두로 사용자 namespace 격리)
     *  - @c "_sprite_plane"             - @c Mesh::CreatePlane() (sprite 전용 plane, 최초 1회)
     *  - @c "_sprite_billboard_program" - @c billboard_atlas.vs/fs program (최초 1회)
     *  - @c "_sprite_billboard"         - template SharedMaterial (program + AlphaTest)
     *  - @c "_sprite_inst_N"            - per-instance MaterialInstance (SpriteRenderer 마다 고유)
     *
     *  ### 책임 분할
     *  - 본 클래스 (MeshRenderer 상속): atlas / frameIdx / tint / flipX / 시각 효과 데이터
     *    + 매 @c Update 마다 uniform(@c uUvRect / @c uTint / @c uFlipX / @c uRoll / 피격 / 디졸브) 자동 송신.
     *  - @c SpriteSequencePlayable (sibling): 시간 따라 frameIdx 갱신 후 본 컴포넌트에 기록
     *    (M3.5 @c SpriteAnimator 폐기 - Playable 인터페이스 정통).
     *
     *  ### 사용 예
     *  @code
     *  auto* spr = actor->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
     *  spr->tint = vmath::vec4(1.0f, 0.5f, 0.5f, 1.0f);
     *  // SpriteSequencePlayable 부착 시 frameIdx 자동 진행
     *  SJH::SpriteSequence::SpriteFrameClip clip{0, atlas->FrameCount(), 4.0f};
     *  auto* seq = actor->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(spr, &clip);
     *  seq->SetIsLoop(true);
     *  seq->Play();
     *  @endcode
     *
     *  ### 1-frame lag 주의
     *  @c SpriteSequencePlayable::Update 가 본 컴포넌트보다 *나중에* 실행되면
     *  uniform 송신은 이전 frameIdx 기반. render() 에서 다음 frame 에 catch-up -
     *  4fps atlas 기준 무시 가능 (1/15 of a tick @ 60fps render).
     */
    class SpriteRenderer : public SJH::Scene::MeshRenderer
    {
    public:
        /// @brief atlas 주입 생성자 - plane + program + per-instance Material 자동 해결 (ctor 내 1회).
        /// @details @p atlas == @c nullptr 이면 Material 도 @c nullptr - 빈 SpriteRenderer (테스트/지연 셋업).
        /// @param atlas 사용할 @c UniformAtlas 포인터. @c nullptr 허용.
        explicit SpriteRenderer(UniformAtlas* atlas = nullptr);

        void OnEnter() override {}
        void OnExit()  override {}

        /// @brief 매 프레임 uniform 동기화 - uUvRect / uTint / uFlipX / uRoll / 피격 / 디졸브.
        /// @param dt 프레임 델타 타임 (초). @c mEffectClock 누적 및 디졸브 클럭에 사용.
        void Update(float dt) override;

        // =====================================================================
        // Sprite 데이터 - public 멤버 직접 접근 (POD-ish)
        // =====================================================================

        /// @brief 현재 atlas. @c nullptr 이면 @c Update 에서 uniform 송신 스킵.
        UniformAtlas* atlas    = nullptr;

        /// @brief 현재 재생 frame 인덱스 (0-based, row-major). @c SpriteSequencePlayable 이 갱신.
        int           frameIdx = 0;

        /// @brief 월드 단위 sprite 크기. 현재 미사용 - @c Transform::Scale 우선.
        vmath::vec2   size     = vmath::vec2(1.0f, 1.0f);

        /// @brief 색 곱셈 tint (RGBA, 0..1). 기본값 흰색 (효과 없음).
        vmath::vec4   tint     = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f);

        /// @brief @c true 이면 셰이더 @c uFlipX = -1 (좌우 반전). 기본값 @c false.
        bool          flipX    = false;

        // =====================================================================
        // 시각 효과 - 외부 sink (PlayerSpriteDirector 등) 가 구동
        // =====================================================================

        /// @brief 피격 깜빡임 활성화 (@c billboard_atlas.fs @c uEnableHit).
        /// @details sink 가 @c enableHit 을 on/off, @c uTime 은 본 컴포넌트 내부 clock 으로 자동 공급.
        bool enableHit = false;

        /// @brief 사망 디졸브 활성화 (@c billboard_atlas.fs @c uEnableDissolve).
        bool                enableDissolve           = false;

        /// @brief 디졸브 진행도 (0 = 원본, 1 = 완전 소멸). sink 가 0 -> 1 로 보간.
        float               dissolveThreshold        = 0.0f;

        /// @brief 디졸브 경계 외곽선 두께 (UV 단위).
        float               dissolveOutlineThickness = 0.05f;

        /// @brief 디졸브 경계 외곽선 색 (RGB, 0..1). 기본값 주황색.
        vmath::vec3         dissolveOutlineColor     = vmath::vec3(1.0f, 0.5f, 0.0f);

        /// @brief 디졸브 노이즈 텍스처 (@c unit=1). sink 가 @c resources/texture/dissolve.png 주입.
        /// @note @c nullptr 이면 @c unit0(@c uAtlas) 을 crude erode 로 사용 - sink 는 항상 주입 권장.
        const SJH::Texture* dissolveTex              = nullptr;

    private:
        float mEffectClock = 0.0f;   ///< @c uTime 용 free-running clock (Update 에서 += dt)
    };
} // namespace SJH::Sprite

#endif // __SJH_SPRITE_SPRITE_COMPONENT_H__
