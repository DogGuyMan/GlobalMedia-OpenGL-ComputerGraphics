// SJH::Transform characterization - vmath->glm 수리본 (094362d 복원). spec D8a.
// 합성: GetLocalMatrix = T * R * S, R = Rz*Ry*Rx (각 glm::radians(EulerRot[i]), EulerRot=degree).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "object/transform.h"
#include <glm/gtc/matrix_transform.hpp>

using Catch::Matchers::WithinAbs;

// glm mat4 근사 비교 (column-major, [col][row])
static void RequireMatNear(const glm::mat4 &a, const glm::mat4 &b, float eps = 1e-5f)
{
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            REQUIRE_THAT(a[c][r], WithinAbs(b[c][r], eps));
}

static void RequireVec3Near(const glm::vec3 &a, const glm::vec3 &b, float eps = 1e-5f)
{
    REQUIRE_THAT(a.x, WithinAbs(b.x, eps));
    REQUIRE_THAT(a.y, WithinAbs(b.y, eps));
    REQUIRE_THAT(a.z, WithinAbs(b.z, eps));
}

TEST_CASE("Transform: 기본값은 단위행렬", "[transform]")
{
    SJH::Transform t;
    RequireMatNear(t.GetLocalMatrix(), glm::mat4(1.0f));
}

TEST_CASE("Transform: 이동만", "[transform]")
{
    SJH::Transform t;
    t.Translate = glm::vec3(1.0f, 2.0f, 3.0f);
    RequireMatNear(t.GetLocalMatrix(),
                   glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f)));
}

TEST_CASE("Transform: 스케일만", "[transform]")
{
    SJH::Transform t;
    t.Scale = glm::vec3(2.0f, 3.0f, 4.0f);
    RequireMatNear(t.GetLocalMatrix(),
                   glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 3.0f, 4.0f)));
}

TEST_CASE("Transform: 기본 방향 벡터", "[transform]")
{
    SJH::Transform t;
    RequireVec3Near(t.GetRight(), glm::vec3(1.0f, 0.0f, 0.0f));
    RequireVec3Near(t.GetUp(), glm::vec3(0.0f, 1.0f, 0.0f));
    RequireVec3Near(t.GetForward(), glm::vec3(0.0f, 0.0f, -1.0f)); // -col2
}

TEST_CASE("Transform: Y축 90도 회전 방향", "[transform]")
{
    SJH::Transform t;
    t.EulerRot = glm::vec3(0.0f, 90.0f, 0.0f); // degree
    // Y+90: col0(Right)=(0,0,-1), col2=(1,0,0) -> Forward=-col2=(-1,0,0)
    RequireVec3Near(t.GetRight(), glm::vec3(0.0f, 0.0f, -1.0f));
    RequireVec3Near(t.GetForward(), glm::vec3(-1.0f, 0.0f, 0.0f));
    RequireVec3Near(t.GetUp(), glm::vec3(0.0f, 1.0f, 0.0f)); // 불변
}

TEST_CASE("Transform: TRS 합성 순서 (T*Rz*S)", "[transform]")
{
    SJH::Transform t;
    t.Translate = glm::vec3(1.0f, 0.0f, 0.0f);
    t.EulerRot = glm::vec3(0.0f, 0.0f, 30.0f); // Z 30도
    t.Scale = glm::vec3(2.0f, 2.0f, 2.0f);

    // impl 미러: T * (Rz*Ry*Rx) * S, 여기선 Rx=Ry=0
    glm::mat4 expected =
        glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 2.0f));
    RequireMatNear(t.GetLocalMatrix(), expected);
}
