#ifndef __SJH_SCENE_COMPONENTS_H__
#define __SJH_SCENE_COMPONENTS_H__

#include "scene/actor.h"
#include "GL/gl3w.h"

namespace SJH
{
    class Mesh;
    class Material;
}

namespace SJH::Scene
{
    /// @brief 스텐실 상태 — Unity ShaderLab Stencil 블록 정통.
    /// @details Enabled=false 가 기본 — 모든 기존 MeshRenderer 동작 보존.
    ///          true 일 때 MeshPassProcessor::Process 가 매 DrawCommand 전에 glStencil* 적용.
    struct StencilState
    {
        bool   Enabled   = false;        ///< 본 상태 활성 여부.
        GLenum Func      = GL_ALWAYS;    ///< glStencilFunc 비교 연산.
        GLint  Ref       = 1;            ///< 참조값 (GL_REPLACE 시 stencil buffer 에 쓰이는 값).
        GLuint TestMask  = 0xFFu;        ///< 비교 마스크 (& stencilValue & ref).
        GLenum SFail     = GL_KEEP;      ///< stencil test fail 시 op.
        GLenum DpFail    = GL_KEEP;      ///< stencil pass + depth fail 시 op.
        GLenum DpPass    = GL_KEEP;      ///< 둘 다 pass 시 op (GL_REPLACE 가 stencil 도장).
        GLuint WriteMask = 0xFFu;        ///< glStencilMask — write 시 비트마스크 (0 = read-only).
    };

    /// @brief Unity MeshRenderer 식 통합 컴포넌트 — Mesh + Material + Visible + QueueOffset + 추가 GL 상태.
    /// @details
    ///   SceneRenderer 이 이 컴포넌트를 수집 -> DrawCommand 빌드.
    ///   Stencil/DepthTest/DepthWrite/CullFace 는 *override* — 기본값 = 표준 opaque 동작.
    ///
    ///   ### Queue 결정 모델 (Unity 정통, 직교 축)
    ///   - **절대 queue** = `Material.PassKind` (Material 측 — "어떤 종류" 의도 선언)
    ///   - **per-renderer 미세 조정** = `MeshRenderer.QueueOffset` (Renderer 측 — "같은 종류 내 순서")
    ///   - 최종 queue = `Pass::QueueOf(material.PassKind, mr.QueueOffset)`
    ///
    ///   예:
    ///   - Box: Material.SetPass(Opaque) + QueueOffset=0  -> 2000
    ///   - Outline (Box 직후): Material.SetPass(Opaque) + QueueOffset=5  -> 2005
    ///   - Window: Material.SetPass(Transparent) + QueueOffset=0  -> 3000
    ///   - Skybox: Material.SetPass(Skybox) + QueueOffset=0  -> 2500
    ///
    ///   Unity 매핑: `Material.renderQueue` ↔ Material.PassKind / `Renderer.sortingOrder` ↔ MeshRenderer.QueueOffset
    class MeshRenderer : public Component
    {
    public:
        MeshRenderer() = default;
        /// @param queueOffset  Material.PassKind 의 queue 에 더해질 *정수 offset* (Unity Renderer.sortingOrder).
        ///                     기본 0 = Material 의 queue 그대로 (정통 경로).
        ///                     Outline 등 *같은 Pass 내 미세 순서* 필요 시 양수 (예: +5).
        MeshRenderer(SJH::Mesh* const mesh, SJH::Material* const material,
                     int queueOffset = 0)
            : Mesh(mesh), Material(material), QueueOffset(queueOffset) {}

	virtual void OnEnter() override {}
        virtual void OnExit() override {}
        virtual void Update(float dt) override {}

        // 모두 public Pascal — POD 식 데이터 (Unity convention).
        // 필드명이 클래스명과 같아 정의에서 SJH:: 한정자 사용.
        SJH::Mesh*     const Mesh       = nullptr;
        SJH::Material* const Material   = nullptr;
        bool                 Visible    = true;
        /// @brief Material.PassKind 의 queue 에 더해질 offset (Unity Renderer.sortingOrder 정통).
        ///        같은 Pass::Kind 안에서 *미세 순서 조정* 용 (예: Outline = +5).
        int                  QueueOffset = 0;

        // ── 추가 per-actor GL 상태 (override) — MeshPassProcessor::Process 가 적용 ──
        StencilState         Stencil;             ///< 기본 disabled — 활성 시 stencil pass 흐름.
        bool                 DepthTest  = true;   ///< false 면 glDisable(GL_DEPTH_TEST) per-draw.
        bool                 DepthWrite = true;   ///< false 면 glDepthMask(GL_FALSE) per-draw.
    };
}

#endif // __SJH_SCENE_COMPONENTS_H__
