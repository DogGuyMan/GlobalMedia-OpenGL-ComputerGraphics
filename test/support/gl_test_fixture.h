/**
 * @file gl_test_fixture.h
 * @brief 테스트용 헤드리스 OpenGL 컨텍스트 RAII fixture.
 *
 * @details
 *  - 매 인스턴스 생성 시 *보이지 않는* GLFW 윈도우 + GL 3.3 core 컨텍스트 + glad 로드 보장.
 *  - 소멸 시 해당 윈도우 파괴. GLFW 자체는 프로세스 1회 init/terminate (atexit).
 *  - 테스트 격리 — 각 TEST_CASE 가 자체 fixture 인스턴스를 만들면 새 컨텍스트 / 새 GL 상태.
 *  - macOS GL 3.3 강제 (FORWARD_COMPAT) 적용.
 *
 *  ### 사용 예
 *  @code
 *  TEST_CASE("Init 후 VAO 가 바인딩되어 있다", "[context][gl]") {
 *      SJH::test::GLContextFixture ctx;
 *      auto context = SJH::Context::Create();
 *      REQUIRE(context != nullptr);
 *
 *      GLint boundVAO = 0;
 *      glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundVAO);
 *      REQUIRE(boundVAO != 0);
 *  }
 *  @endcode
 *
 *  ### 컨벤션
 *  - 본 fixture 는 *상태 쿼리* 검증을 위한 것 — pixel/FBO 캡처는 Phase B2 의 별도 헬퍼로.
 *  - 테스트 전략 결정은 [doc/testing-curriculum.md](../../doc/testing-curriculum.md) Phase B1 참조.
 */

#ifndef __SJH_TEST_GL_TEST_FIXTURE_H__
#define __SJH_TEST_GL_TEST_FIXTURE_H__

#pragma once

struct GLFWwindow;

namespace SJH::test
{
    class GLContextFixture
    {
    public:
        /// @brief hidden window 생성 + 컨텍스트 makeCurrent + glad 로드.
        /// @throws std::runtime_error GLFW init / window 생성 / glad 로드 실패 시.
        explicit GLContextFixture(int width = 256, int height = 256);
        ~GLContextFixture();

        GLContextFixture(const GLContextFixture &)            = delete;
        GLContextFixture &operator=(const GLContextFixture &) = delete;

        int width()  const { return mWidth; }
        int height() const { return mHeight; }

    private:
        GLFWwindow *mWindow{nullptr};
        int         mWidth;
        int         mHeight;
    };
}

#endif // __SJH_TEST_GL_TEST_FIXTURE_H__
