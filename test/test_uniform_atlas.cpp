#include "sprite/uniform_atlas.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using SJH::Sprite::ComputeUVRect;
using Catch::Approx;

TEST_CASE("ComputeUVRect — 4x4 grid, 256x256 atlas, 64px tile", "[sprite][uniform_atlas]")
{
    SECTION("frame 0 (col=0, row=0)  (0, 0, 0.25, 0.25)") {
        auto uv = ComputeUVRect(0, /*cols=*/4, /*tile=*/64, /*atlasW=*/256, /*atlasH=*/256);
        REQUIRE(uv[0] == Approx(0.0f));
        REQUIRE(uv[1] == Approx(0.0f));
        REQUIRE(uv[2] == Approx(0.25f));
        REQUIRE(uv[3] == Approx(0.25f));
    }
    SECTION("frame 5 (col=1, row=1)  (0.25, 0.25, 0.25, 0.25)") {
        auto uv = ComputeUVRect(5, 4, 64, 256, 256);
        REQUIRE(uv[0] == Approx(0.25f));
        REQUIRE(uv[1] == Approx(0.25f));
        REQUIRE(uv[2] == Approx(0.25f));
        REQUIRE(uv[3] == Approx(0.25f));
    }
    SECTION("frame 15 (col=3, row=3)  (0.75, 0.75, 0.25, 0.25)") {
        auto uv = ComputeUVRect(15, 4, 64, 256, 256);
        REQUIRE(uv[0] == Approx(0.75f));
        REQUIRE(uv[1] == Approx(0.75f));
        REQUIRE(uv[2] == Approx(0.25f));
        REQUIRE(uv[3] == Approx(0.25f));
    }
}

TEST_CASE("ComputeUVRect — 8x4 non-square grid, 512x256 atlas, 64px tile", "[sprite][uniform_atlas]")
{
    SECTION("frame 0  (0, 0, 0.125, 0.25)") {
        auto uv = ComputeUVRect(0, 8, 64, 512, 256);
        REQUIRE(uv[0] == Approx(0.0f));
        REQUIRE(uv[1] == Approx(0.0f));
        REQUIRE(uv[2] == Approx(0.125f));
        REQUIRE(uv[3] == Approx(0.25f));
    }
    SECTION("frame 8 (col=0, row=1)  (0, 0.25, 0.125, 0.25)") {
        auto uv = ComputeUVRect(8, 8, 64, 512, 256);
        REQUIRE(uv[0] == Approx(0.0f));
        REQUIRE(uv[1] == Approx(0.25f));
    }
}

TEST_CASE("ComputeUVRect — 잘못된 입력 (zero/negative)  zero rect", "[sprite][uniform_atlas]")
{
    SECTION("cols=0") {
        auto uv = ComputeUVRect(0, 0, 64, 256, 256);
        REQUIRE(uv[0] == Approx(0.0f));
        REQUIRE(uv[2] == Approx(0.0f));
    }
    SECTION("atlasWidth=0") {
        auto uv = ComputeUVRect(0, 4, 64, 0, 256);
        REQUIRE(uv[2] == Approx(0.0f));
    }
}
