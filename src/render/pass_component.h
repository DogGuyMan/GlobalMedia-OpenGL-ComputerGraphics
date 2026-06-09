/**
 * @file pass_component.h
 * @brief 화면 공간 렌더 패스 컴포넌트 - PostFX 체인의 단일 스테이지 배선.
 *
 * @details
 *  ### 책임
 *  - @c MeshRenderer 의 화면 공간 대응 - ScreenQuad + PostFX 셰이더 한 단계.
 *  - @c InputFB / @c OutputFB 포인터 공유로 파이프라인 경계 표현:
 *    @c Pass[N].OutputFB == @c Pass[N+1].InputFB (같은 포인터).
 *  - OpenGL 피드백 루프 방지 계약: 읽는 FB(@c InputFB) != 쓰는 FB(@c OutputFB) 항상.
 *
 *  ### 비-책임
 *  - [X] Framebuffer 소유 - @c ResourceRegistry / @c PostFXChainResult 가 소유.
 *  - [X] GL 상태 직접 조작 - @c Material::SetPass(Kind) 와 SceneRenderer 의 DrawCommand 가 담당.
 *
 *  ### @c QueueOffset 컨벤션
 *  기본값 @c 9000 - 월드 지오메트리 DrawCommand(0~8999) 가 모두 처리된 이후 실행 보장.
 *  @c BuildPostFXChain 으로 생성된 패스는 이 순서를 그대로 사용.
 *
 * @note @c MeshRenderer 와 본질적으로 동일 구조 - 단지 "화면 공간의 MeshRenderer".
 *       Actor::AddComponent<PassComponent>(prevFB, fb, mat) 로 주입되며,
 *       소유 Actor 는 @c Scene::Layer::Screen 레이어에 속해야 SceneRenderer 가 올바르게 수집.
 */
#ifndef __SJH_PASS_COMPONENT_H__
#define __SJH_PASS_COMPONENT_H__

#include "scene/actor.h"  // Component 베이스 (render/ 가 scene/ 에 의존 - MeshRenderer 동일 패턴)

namespace SJH
{
    class Framebuffer;
    class Material;
} // namespace SJH

namespace SJH::Scene
{
	/**
	 * @brief 화면 공간 렌더 패스 컴포넌트 - PostFX 체인 단일 스테이지 배선.
	 * @details
	 *  @c InputFB 를 샘플링해 PostFX 셰이더를 적용 후 @c OutputFB 에 쓰는 한 단계.
	 *  SceneRenderer 가 @c Layer::Screen 의 Actor 에서 이 컴포넌트를 수집해 DrawCommand 빌드.
	 *
	 *  피드백 루프 방지 계약:
	 *  - @c InputFB -> 이전 패스(또는 sceneFB) 의 OutputFB (읽기 전용).
	 *  - @c OutputFB -> 다음 패스의 InputFB 혹은 최종 합성 소스 (쓰기 대상).
	 *  - 두 포인터는 항상 달라야 함 (@c assert 또는 caller 보장).
	 */
    class PassComponent : public Component
    {
      public:
		/// @brief InputFB / OutputFB / Material 주입 생성자.
		/// @param inputFB  읽기 소스 FBO (비소유). 이전 패스의 @c OutputFB 와 포인터 공유.
		/// @param outputFB 쓰기 대상 FBO (비소유). 다음 패스의 @c InputFB 와 포인터 공유.
		/// @param mat      PostFX 셰이더 + GL state 담당 머티리얼 (비소유).
        PassComponent(SJH::Framebuffer *inputFB, SJH::Framebuffer *outputFB, SJH::Material *mat)
            : InputFB(inputFB), OutputFB(outputFB), mMaterial(mat)
        {
        }

        SJH::Framebuffer *const InputFB;   ///< 읽기 소스 FBO - 이전 패스 @c OutputFB 와 포인터 공유 (비소유).
        SJH::Framebuffer *const OutputFB;  ///< 쓰기 대상 FBO - 다음 패스 @c InputFB 와 포인터 공유 (비소유).
        SJH::Material          *mMaterial; ///< PostFX 셰이더 + GL state (비소유). nullptr 시 SceneRenderer skip.
        bool                    Enabled     = true;  ///< false 시 SceneRenderer DrawCommand 제외.
        int                     QueueOffset = 9000;  ///< 월드 지오메트리(0~8999) 이후 실행 보장.

        virtual void OnEnter() override {}
        virtual void OnExit() override  {}
        virtual void Update(float) override {}
    };
} // namespace SJH::Scene

#endif // __SJH_PASS_COMPONENT_H__
