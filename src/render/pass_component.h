#ifndef __SJH_PASS_COMPONENT_H__
#define __SJH_PASS_COMPONENT_H__

#include "scene/actor.h"  // Component 베이스 (render/ 가 scene/ 에 의존 — MeshRenderer 동일 패턴)

namespace SJH
{
    class Framebuffer;
    class Material;
} // namespace SJH

namespace SJH::Scene
{
    /// @brief 화면 공간 MeshRenderer — ScreenQuad + PostFX 셰이더 조합.
    /// @details
    ///   MeshRenderer 와 본질적으로 동일 — 단지 "화면 공간의 MeshRenderer".
    ///   포인터 공유 = 파이프라인 경계: Pass1.OutputFB == Pass2.InputFB (같은 포인터).
    ///   OpenGL 피드백 루프 없음 보장: 읽는 FB != 쓰는 FB 항상.
    class PassComponent : public Component
    {
      public:
        PassComponent(SJH::Framebuffer *inputFB, SJH::Framebuffer *outputFB, SJH::Material *mat)
            : InputFB(inputFB), OutputFB(outputFB), mMaterial(mat)
        {
        }

        SJH::Framebuffer *const InputFB;   ///< readonly — 이전 패스의 OutputFB 와 포인터 공유
        SJH::Framebuffer *const OutputFB;  ///< writable — 다음 패스의 InputFB 와 포인터 공유
        SJH::Material          *mMaterial;
        bool                    Enabled     = true;
        int                     QueueOffset = 9000;  ///< 월드 지오메트리(0~8999) 이후

        virtual void OnEnter() override  {}
        virtual void OnExit() override   {}
        virtual void Update(float) override {}
    };
} // namespace SJH::Scene

#endif // __SJH_PASS_COMPONENT_H__
