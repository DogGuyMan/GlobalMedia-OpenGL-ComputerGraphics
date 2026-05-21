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
    ///          true 일 때 RenderQueue::Flush 가 매 DrawCommand 전에 glStencil* 적용.
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

    /// @brief Unity MeshRenderer 식 통합 컴포넌트 — Mesh + Material + Visible + QueueLayer + 추가 GL 상태.
    /// @details RenderSystem 이 이 컴포넌트를 수집 -> DrawCommand 빌드.
    ///          Stencil/DepthTest/DepthWrite/CullFace 는 *override* — 기본값 = 표준 opaque 동작.
    class MeshRenderer : public Component
    {
    public:
        MeshRenderer() = default;
        MeshRenderer(SJH::Mesh* const mesh, SJH::Material* const material,
                     int queueLayer = 2000)
            : Mesh(mesh), Material(material), QueueLayer(queueLayer) {}

	virtual void OnEnter() override {}
        virtual void OnExit() override {}
        virtual void Update(float dt) override {}

        // 모두 public Pascal — POD 식 데이터 (Unity convention).
        // 필드명이 클래스명과 같아 정의에서 SJH:: 한정자 사용.
        SJH::Mesh*     const Mesh       = nullptr;
        SJH::Material* const Material   = nullptr;
        bool                 Visible    = true;
        int                  QueueLayer = 2000;   // Unity: 2000=Opaque, 3000=Transparent

        // ── 추가 per-actor GL 상태 (override) — RenderQueue::Flush 가 적용 ──
        StencilState         Stencil;             ///< 기본 disabled — 활성 시 stencil pass 흐름.
        bool                 DepthTest  = true;   ///< false 면 glDisable(GL_DEPTH_TEST) per-draw.
        bool                 DepthWrite = true;   ///< false 면 glDepthMask(GL_FALSE) per-draw.
    };
}

#endif // __SJH_SCENE_COMPONENTS_H__
