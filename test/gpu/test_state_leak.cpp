/**
 * @file test_state_leak.cpp
 * @brief C-4 GL 상태누수 - production 진단 SSOT(CaptureGLState/DiffStates)로 작업 전/후
 *        GL 상태 변화를 잠근다 (characterization - 버그 수정 아님).
 *
 * @details
 *  ### 오라클 = production diagnostics 재사용 ([[ubo-binding-point-semantic-slots]] 등 회귀가드 정신)
 *  raw glGet* 4종 스냅샷을 직접 짜지 않고 @c SJH::Diagnostics::CaptureGLState (이미 EBO /
 *  16 텍스처유닛 / 16 attribute / cull/colormask 캡처) + 신규 @c DiffStates 를 소비한다.
 *
 *  ### 범위 (정직한 축소)
 *  "패스 1회 풀 렌더 전/후" 의 *기대 변경 화이트리스트* 는 렌더 파이프라인 전체를 끌어와야 해
 *  본 GPU 단위테스트 범위에서 과하다. 대신 핵심 1~2 케이스로 잠근다:
 *    1. **오라클 sanity**: 아무 작업 없이 두 번 캡처 -> diff 비어야 함 (CaptureGLState 부수효과 0 + DiffStates 정합).
 *    2. **버퍼 바인딩 누수**: array buffer 바인딩 -> *array_buffer 단일 변경*만 보이고,
 *       원복 후 diff 비어야 함 (바인딩 누수 없음).
 *    3. **DeviceContext blend 작업**: blend off->on -> *픽셀 파이프라인(D) 변경만* 보이고
 *       바인딩(B)/attribute(C) 누수 0 (현 동작 잠금).
 *
 *  ⚠ "무엇이 기대 변경인가" 는 *현 동작* 기준으로 잠근다 (버그 수정 아님).
 */

#include "gl_context_fixture.h"

#include "buffer/buffer.h"
#include "diagnostics/gl_state_fields.h"
#include "render/device_context.h"
#include "material/pass.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace diag = SJH::Diagnostics;

namespace
{
    // 참고: 예전엔 VAO=0(default) 에서 CaptureGLState 가 glGetVertexAttribiv 로
    //       GL_INVALID_OPERATION 을 내는 quirk 가 있어 ScopedVAO 워크어라운드를 썼으나,
    //       2026-06-28 CaptureGLState 가 VAO=0 시 attribute query 를 skip 하도록 수정 →
    //       워크어라운드 제거. 이 테스트들이 VAO=0 에서 깨끗이 도는 것 자체가 그 수정의 검증.

    /// @brief diff 안에 지정 field 의 변경이 있는지.
    bool HasFieldChange(const std::vector<diag::FieldChange> &changes, const std::string &field)
    {
        return std::any_of(changes.begin(), changes.end(),
                           [&](const diag::FieldChange &c) { return c.field == field; });
    }

    /// @brief diff 안에 지정 category 변경이 하나라도 있는지.
    bool HasCategory(const std::vector<diag::FieldChange> &changes, char category)
    {
        return std::any_of(changes.begin(), changes.end(),
                           [&](const diag::FieldChange &c) { return c.category == category; });
    }
}

TEST_CASE("C-4 오라클 sanity - 무작업 시 CaptureGLState diff 가 비어 있다", "[gpu][state_leak]")
{
    using namespace SJH;
    REQUIRE(Test::Gpu::GLContextFixture::IsValid());
    const diag::GLStateFields a = diag::CaptureGLState();
    const diag::GLStateFields b = diag::CaptureGLState();

    const std::vector<diag::FieldChange> changes = diag::DiffStates(a, b);
    // 부수효과 0 캡처 + 그 사이 작업 0 -> diff 비어야 한다.
    CHECK(changes.empty());
}

TEST_CASE("C-4 array buffer 바인딩 -> array_buffer 단일 변경, 원복 후 누수 0", "[gpu][state_leak]")
{
    using namespace SJH;
    REQUIRE(Test::Gpu::GLContextFixture::IsValid());
    // 작업 대상 버퍼 1개 준비 (캡처 baseline 전에 생성 - 생성 자체는 측정 구간 밖).
    const std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
    BufferUPtr buf = Buffer::CreateWithData(GL_ARRAY_BUFFER, GL_STATIC_DRAW,
                                            data.data(), sizeof(float), data.size());
    REQUIRE(buf != nullptr);

    // baseline: array buffer 바인딩 해제 (0) 후 캡처.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    const diag::GLStateFields before = diag::CaptureGLState();

    // 작업: 버퍼 바인딩.
    glBindBuffer(GL_ARRAY_BUFFER, buf->Get());
    const diag::GLStateFields after = diag::CaptureGLState();

    const std::vector<diag::FieldChange> changes = diag::DiffStates(before, after);
    // 기대 변경 화이트리스트: array_buffer 만.
    CHECK(HasFieldChange(changes, "array_buffer"));
    CHECK_FALSE(HasFieldChange(changes, "vao"));
    CHECK_FALSE(HasFieldChange(changes, "program"));
    CHECK_FALSE(HasCategory(changes, 'D')); // 픽셀 파이프라인 누수 없음
    CHECK_FALSE(HasCategory(changes, 'C')); // attribute layout 누수 없음

    // 원복: 다시 0 으로 바인딩 -> before 대비 diff 비어야 함 (누수 0).
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    const diag::GLStateFields restored = diag::CaptureGLState();
    const std::vector<diag::FieldChange> restoreDiff = diag::DiffStates(before, restored);
    CHECK(restoreDiff.empty());
}

TEST_CASE("C-4 DeviceContext blend 작업은 픽셀 파이프라인(D)만 바꾸고 바인딩 누수 0", "[gpu][state_leak]")
{
    using namespace SJH;
    REQUIRE(Test::Gpu::GLContextFixture::IsValid());
    DeviceContext &rc = DeviceContext::Get();

    // baseline: Opaque 적용 (blend off) 후 캡처.
    rc.InvalidateStateCache();
    rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Opaque));
    const diag::GLStateFields before = diag::CaptureGLState();

    // 작업: Transparent 적용 (blend on, depth write off, cull off).
    rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Transparent));
    const diag::GLStateFields after = diag::CaptureGLState();

    const std::vector<diag::FieldChange> changes = diag::DiffStates(before, after);
    INFO("변경 필드 수: " << changes.size());
    // 픽셀 파이프라인(D) 변경은 있어야 한다 (blend/depth/cull).
    CHECK(HasCategory(changes, 'D'));
    CHECK(HasFieldChange(changes, "blend_enabled"));
    // 바인딩(B)/attribute(C) 누수는 없어야 한다 - blend state 전환은 그것들과 직교.
    CHECK_FALSE(HasCategory(changes, 'B'));
    CHECK_FALSE(HasCategory(changes, 'C'));
}
