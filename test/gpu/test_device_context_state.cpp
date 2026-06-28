/**
 * @file test_device_context_state.cpp
 * @brief C-2 DeviceContext 상태캐시 - ApplyRenderStateBlock dirty-check + blend func
 *        캐시 기본값 불일치 회귀가드 (characterization).
 *
 * @details
 *  ### 잠그는 현 동작 (버그 수정 아님 - 현 동작 잠금)
 *  - **blend off -> on 전이** ([[blend-func-cache-default-mismatch]]):
 *    GL 의 glBlendFunc 기본값 = (GL_ONE, GL_ZERO) 인데 DeviceContext 캐시
 *    @c mLast.BlendSrc/Dst 기본값 = (SRC_ALPHA, ONE_MINUS_SRC_ALPHA) 라 불일치.
 *    off->on 전이서 func 을 *무조건* 강제하지 않으면 dirty-check 가 "이미 일치"로
 *    판단해 glBlendFunc 을 영영 skip -> GL 은 (ONE,ZERO) 잔재로 알파 무시.
 *    @c ApplyBlend 의 @c prevEnabled 가드가 이를 막는다 - 그 동작을 잠근다.
 *  - **InvalidateStateCache** 후 재적용이 first-call 처럼 GL 을 강제 갱신하는지.
 *
 *  ### 테스트 간 상태 오염 주의
 *  DeviceContext 는 Meyer 싱글톤(@c Get()) 이라 케이스 간 @c mLast 캐시가 공유된다.
 *  각 케이스 시작에 @c InvalidateStateCache() 로 캐시를 리셋하고, GL state 도
 *  raw GL 로 알려진 baseline 으로 직접 세팅해 결정성을 확보한다.
 */

#include "gl_context_fixture.h"

#include "render/device_context.h"
#include "material/pass.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    /// @brief GL_BLEND_SRC_RGB / GL_BLEND_DST_RGB 를 읽어 반환.
    void ReadBlendFunc(GLenum &outSrc, GLenum &outDst)
    {
        GLint src = 0, dst = 0;
        glGetIntegerv(GL_BLEND_SRC_RGB, &src);
        glGetIntegerv(GL_BLEND_DST_RGB, &dst);
        outSrc = static_cast<GLenum>(src);
        outDst = static_cast<GLenum>(dst);
    }

    /// @brief glIsEnabled(GL_BLEND).
    bool IsBlendEnabled()
    {
        return glIsEnabled(GL_BLEND) == GL_TRUE;
    }
}

TEST_CASE("C-2 blend off->on 전이서 glBlendFunc 이 강제 적용된다 (캐시 기본값 불일치 가드)", "[gpu][device_context]")
{
    using namespace SJH;
    REQUIRE(Test::Gpu::GLContextFixture::IsValid());

    DeviceContext &rc = DeviceContext::Get();

    // --- baseline: blend off + GL func 을 (ONE, ZERO) GL 기본으로 직접 세팅 ---
    //   캐시와 desync 된 상황(블렌딩 켜진 적 없음)을 재현한다.
    glDisable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ZERO);
    rc.InvalidateStateCache(); // 캐시를 first-call 상태로 리셋

    // off baseline(Opaque) 적용 - blend 는 여전히 off.
    rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Opaque));
    REQUIRE_FALSE(IsBlendEnabled());

    // --- off -> on 전이 (Transparent: BlendEnable=true, SRC_ALPHA / ONE_MINUS_SRC_ALPHA) ---
    rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Transparent));

    CHECK(IsBlendEnabled());
    GLenum src = 0, dst = 0;
    ReadBlendFunc(src, dst);
    // 핵심 단언: func 이 (ONE,ZERO) 잔재가 아니라 Transparent 기대값으로 강제 적용됐다.
    CHECK(src == static_cast<GLenum>(GL_SRC_ALPHA));
    CHECK(dst == static_cast<GLenum>(GL_ONE_MINUS_SRC_ALPHA));
}

TEST_CASE("C-2 InvalidateStateCache 후 재적용이 GL 을 강제 갱신한다", "[gpu][device_context]")
{
    using namespace SJH;
    REQUIRE(Test::Gpu::GLContextFixture::IsValid());

    DeviceContext &rc = DeviceContext::Get();

    // Transparent 를 한 번 적용해 캐시를 blend-on 상태로 만든다.
    rc.InvalidateStateCache();
    rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Opaque));
    rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Transparent));
    REQUIRE(IsBlendEnabled());

    // 외부(foreign) 소비자가 캐시 뒤에서 GL state 를 바꾼 상황을 모사 - blend off.
    glDisable(GL_BLEND);
    REQUIRE_FALSE(IsBlendEnabled());

    // InvalidateStateCache 없이 같은 Transparent 를 재적용하면 dirty-check 가
    // "이미 BlendEnable=true" 로 판단해 glEnable 을 skip -> GL 은 여전히 off (현 동작).
    rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Transparent));
    CHECK_FALSE(IsBlendEnabled()); // desync 잔존 (캐시 무효화 안 했으므로)

    // InvalidateStateCache 후 재적용하면 first-call 처럼 전체 강제 -> blend 다시 on.
    rc.InvalidateStateCache();
    rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Transparent));
    CHECK(IsBlendEnabled());
    GLenum src = 0, dst = 0;
    ReadBlendFunc(src, dst);
    CHECK(src == static_cast<GLenum>(GL_SRC_ALPHA));
    CHECK(dst == static_cast<GLenum>(GL_ONE_MINUS_SRC_ALPHA));
}

TEST_CASE("C-2 depth/cull state 가 RenderStateBlock 대로 GL 에 반영된다", "[gpu][device_context]")
{
    using namespace SJH;
    REQUIRE(Test::Gpu::GLContextFixture::IsValid());

    DeviceContext &rc = DeviceContext::Get();
    rc.InvalidateStateCache();

    // Opaque: DepthTest on / DepthWrite on / CullMode BACK.
    rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Opaque));
    CHECK(glIsEnabled(GL_DEPTH_TEST) == GL_TRUE);
    GLint depthWrite = 0;
    glGetIntegerv(GL_DEPTH_WRITEMASK, &depthWrite);
    CHECK(depthWrite == GL_TRUE);
    CHECK(glIsEnabled(GL_CULL_FACE) == GL_TRUE);

    // Transparent: DepthWrite off / CullMode 0(=face culling off).
    rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Transparent));
    glGetIntegerv(GL_DEPTH_WRITEMASK, &depthWrite);
    CHECK(depthWrite == GL_FALSE);
    CHECK(glIsEnabled(GL_CULL_FACE) == GL_FALSE);
}
