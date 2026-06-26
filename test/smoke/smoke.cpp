// smoke - 테스트 배선(Catch2+vcpkg+CTest+catch_discover_tests) 카나리아.
// 로직 검증이 아니라 "테스트 환경 자체가 작동한다"를 증명한다. (spec D5)
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "common/common.h" // SJH::Deg2Rad / Rad2Deg / FixedTime

using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

TEST_CASE("smoke: 테스트 배선 작동", "[smoke]")
{
    // Deg2Rad(180) = 180 * M_PI / 180 = M_PI
    REQUIRE_THAT(SJH::Deg2Rad(180.0f), WithinRel(3.14159274f, 1e-5f));
    // Rad2Deg 는 Deg2Rad 의 역
    REQUIRE_THAT(SJH::Rad2Deg(SJH::Deg2Rad(90.0f)), WithinRel(90.0f, 1e-5f));
    // FixedTime = 1/60
    REQUIRE_THAT(SJH::FixedTime(), WithinAbs(1.0f / 60.0f, 1e-7f));
}
