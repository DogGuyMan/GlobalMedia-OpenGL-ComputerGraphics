// SJH::Sprite::ComputeUVRect characterization (094362d test_uniform_atlas 복원, drift 없음).
// 반환 vec4 = (uMin, vMin, uSize, vSize). spec D8a.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "sprite/uniform_atlas.h"

using SJH::Sprite::ComputeUVRect;
using Catch::Approx;

TEST_CASE("ComputeUVRect: 4x4 / 256 / 64px 정사각", "[sprite][uvrect]")
{
    SECTION("frame 0 = 좌상단")
    {
        glm::vec4 uv = ComputeUVRect(0, 4, 64, 256, 256);
        REQUIRE(uv.x == Approx(0.0f));  // uMin
        REQUIRE(uv.y == Approx(0.0f));  // vMin
        REQUIRE(uv.z == Approx(0.25f)); // uSize 64/256
        REQUIRE(uv.w == Approx(0.25f)); // vSize
    }
    SECTION("frame 5 = (col1,row1)")
    {
        glm::vec4 uv = ComputeUVRect(5, 4, 64, 256, 256);
        REQUIRE(uv.x == Approx(0.25f)); // 1*64/256
        REQUIRE(uv.y == Approx(0.25f));
        REQUIRE(uv.z == Approx(0.25f));
        REQUIRE(uv.w == Approx(0.25f));
    }
    SECTION("frame 15 = (col3,row3)")
    {
        glm::vec4 uv = ComputeUVRect(15, 4, 64, 256, 256);
        REQUIRE(uv.x == Approx(0.75f)); // 3*64/256
        REQUIRE(uv.y == Approx(0.75f));
    }
}

TEST_CASE("ComputeUVRect: 8x4 비정사각 (6-arg)", "[sprite][uvrect]")
{
    SECTION("frame 0")
    {
        glm::vec4 uv = ComputeUVRect(0, 8, 64, 64, 512, 256);
        REQUIRE(uv.x == Approx(0.0f));
        REQUIRE(uv.y == Approx(0.0f));
        REQUIRE(uv.z == Approx(0.125f)); // 64/512
        REQUIRE(uv.w == Approx(0.25f));  // 64/256
    }
    SECTION("frame 8 = (col0,row1)")
    {
        glm::vec4 uv = ComputeUVRect(8, 8, 64, 64, 512, 256);
        REQUIRE(uv.x == Approx(0.0f));
        REQUIRE(uv.y == Approx(0.25f)); // 1*64/256
        REQUIRE(uv.z == Approx(0.125f));
        REQUIRE(uv.w == Approx(0.25f));
    }
}

TEST_CASE("ComputeUVRect: 무효 입력은 zero rect", "[sprite][uvrect]")
{
    REQUIRE(ComputeUVRect(0, 0, 64, 256, 256) == glm::vec4(0.0f));   // cols=0
    REQUIRE(ComputeUVRect(0, 4, 64, 0, 256) == glm::vec4(0.0f));     // atlasW=0
}
