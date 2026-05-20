// Catch2 v3 통합 검증용 최소 smoke test.
// extern/Catch2 서브모듈 + add_subdirectory 통합이 동작하는지만 확인한다.
// 실제 모듈 테스트는 별도 디렉토리에서 작성.

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Catch2 v3 통합 — 기본 단언", "[smoke]")
{
    REQUIRE(1 + 1 == 2);
    CHECK(true);
}

TEST_CASE("Catch2 v3 통합 — SECTION 동작", "[smoke]")
{
    int x = 10;

    SECTION("증가")
    {
        x++;
        REQUIRE(x == 11);
    }

    SECTION("감소")
    {
        x--;
        REQUIRE(x == 9);
    }
}
