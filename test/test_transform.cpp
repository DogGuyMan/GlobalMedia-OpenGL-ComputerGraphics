/**
 * @file test_transform.cpp
 * @brief SJH::Transform — GetLocalMatrix TRS 합성 검증 (GL 컨텍스트 불요).
 */
#include "object/transform.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vmath.h>

using Catch::Matchers::WithinAbs;

namespace
{
    // 두 mat4 가 성분별로 근사 동일한지 검사.
    void RequireMatNear(const vmath::mat4 &a, const vmath::mat4 &b, float eps = 1e-5f)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                REQUIRE_THAT(a[c][r], WithinAbs(b[c][r], eps));
    }
}

TEST_CASE("Transform 기본값 — identity 로컬 행렬", "[transform]")
{
    SJH::Transform t;
    RequireMatNear(t.GetLocalMatrix(), vmath::mat4::identity());
}

TEST_CASE("Transform translate 전용", "[transform]")
{
    SJH::Transform t;
    t.Translate = vmath::vec3(1.0f, 2.0f, 3.0f);
    RequireMatNear(t.GetLocalMatrix(),
                   vmath::translate(vmath::vec3(1.0f, 2.0f, 3.0f)));
}

TEST_CASE("Transform scale 전용", "[transform]")
{
    SJH::Transform t;
    t.Scale = vmath::vec3(2.0f, 3.0f, 4.0f);
    RequireMatNear(t.GetLocalMatrix(),
                   vmath::scale(vmath::vec3(2.0f, 3.0f, 4.0f)));
}

TEST_CASE("Transform EulerRot — degree 입력이 radians 로 변환 (Y축 90도)", "[transform]")
{
    SJH::Transform t;
    t.EulerRot = vmath::vec3(0.0f, 90.0f, 0.0f);
    // vmath::rotate 는 degree 직접 수용 (glm::rotate 와 달리 radians 변환 불요).
    const vmath::mat4 expected =
        vmath::rotate(90.0f, vmath::vec3(0.0f, 1.0f, 0.0f));
    RequireMatNear(t.GetLocalMatrix(), expected);
}

TEST_CASE("Transform TRS 합성 순서 — T·Rz·Ry·Rx·S", "[transform]")
{
    SJH::Transform t;
    t.Translate = vmath::vec3(1.0f, 0.0f, 0.0f);
    t.EulerRot  = vmath::vec3(0.0f, 0.0f, 30.0f);
    t.Scale     = vmath::vec3(2.0f, 2.0f, 2.0f);
    const vmath::mat4 expected =
        vmath::translate(vmath::vec3(1.0f, 0.0f, 0.0f)) *
        vmath::rotate(30.0f, vmath::vec3(0.0f, 0.0f, 1.0f)) *
        vmath::scale(vmath::vec3(2.0f, 2.0f, 2.0f));
    RequireMatNear(t.GetLocalMatrix(), expected);
}
