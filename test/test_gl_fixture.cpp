/**
 * @file test_gl_fixture.cpp
 * @brief @c GLContextFixture 자가검증 — fixture 자체가 동작함을 보장.
 *
 * @details
 *  - 본 fixture 가 깨지면 Phase 1+ 모듈의 GL 상태 쿼리 테스트가 모두 무의미해지므로
 *    *fixture 자체* 의 회귀를 잡는 게 우선.
 */

#include <catch2/catch_test_macros.hpp>
#include "support/gl_test_fixture.h"
#include "GL/gl3w.h"
#include <string>

TEST_CASE("GLContextFixture: glad-loaded GL 3.3 core context", "[fixture][gl]")
{
    SJH::test::GLContextFixture ctx;

    SECTION("GL_VERSION 문자열 조회 가능")
    {
        const auto *version = glGetString(GL_VERSION);
        REQUIRE(version != nullptr);
        const std::string v = reinterpret_cast<const char *>(version);
        INFO("GL_VERSION = " << v);
        REQUIRE_FALSE(v.empty());
    }

    SECTION("초기 상태에서 GL_NO_ERROR")
    {
        // 컨텍스트 생성 + glad 로드 직후엔 에러 큐가 비어있어야 함.
        REQUIRE(glGetError() == GL_NO_ERROR);
    }

    SECTION("fixture width/height 반영")
    {
        REQUIRE(ctx.width()  == 256);
        REQUIRE(ctx.height() == 256);
    }
}

TEST_CASE("GLContextFixture: 두 인스턴스 순차 생성/파괴 안전", "[fixture][gl]")
{
    // GLFW 1회 init / 다중 윈도우 가드가 깨지면 두 번째 인스턴스 생성 시 크래시 가능.
    {
        SJH::test::GLContextFixture ctx1(64, 64);
        REQUIRE(ctx1.width() == 64);
    }
    {
        SJH::test::GLContextFixture ctx2(128, 128);
        REQUIRE(ctx2.width()  == 128);
        REQUIRE(ctx2.height() == 128);
        // 새 컨텍스트도 정상 동작
        REQUIRE(glGetString(GL_VERSION) != nullptr);
        REQUIRE(glGetError() == GL_NO_ERROR);
    }
}
