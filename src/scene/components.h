#ifndef __SJH_SCENE_COMPONENTS_H__
#define __SJH_SCENE_COMPONENTS_H__

#include "scene/actor.h"

namespace SJH
{
    class Mesh;
    class Material;
}

namespace SJH::Scene
{
    /// @brief Unity MeshRenderer 식 통합 컴포넌트 — Mesh + Material + Visible + QueueLayer.
    /// @details RenderSystem 이 이 컴포넌트를 수집 → DrawCommand 빌드.
    class MeshRenderer : public Component
    {
    public:
        MeshRenderer() = default;
        MeshRenderer(const SJH::Mesh* mesh, const SJH::Material* material,
                     int queueLayer = 2000)
            : Mesh(mesh), Material(material), QueueLayer(queueLayer) {}

        // 모두 public Pascal — POD 식 데이터 (Unity convention).
        // 필드명이 클래스명과 같아 정의에서 SJH:: 한정자 사용.
        const SJH::Mesh*     Mesh       = nullptr;
        const SJH::Material* Material   = nullptr;
        bool                 Visible    = true;
        int                  QueueLayer = 2000;   // Unity: 2000=Opaque, 3000=Transparent
    };
}

#endif // __SJH_SCENE_COMPONENTS_H__
