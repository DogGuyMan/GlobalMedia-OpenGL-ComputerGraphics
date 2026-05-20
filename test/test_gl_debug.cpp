/**
 * @file test_gl_debug.cpp
 * @brief @c SJH::Diagnostics::GLDebug::CheckGL* 7개 함수의 동작 검증.
 *
 * @details
 *  - **Happy path** — context.cpp::Init() 의 VAO/VBO/Attribute 시퀀스를 그대로 반복하며
 *    7개 check 함수 모두 @c true 반환을 확인.
 *  - **Negative path** — 의도적으로 GL 에러를 발생시켜 check 함수가 @c false 반환을 확인.
 *  - 본 테스트의 가치: 진단 함수 자체의 회귀 (case 누락, 잘못된 enum 매핑) 를 잡음.
 */

#include <catch2/catch_test_macros.hpp>

#include "support/gl_test_fixture.h"
#include "diagnostics/gl_log.h"

#include "GL/gl3w.h"

namespace
{
    // 매 테스트 시작 시 GL 에러 큐 비우기 — 이전 테스트의 잔여 에러가 누설되지 않도록.
    void DrainGLErrors()
    {
        while (glGetError() != GL_NO_ERROR) { /* discard */ }
    }
}

TEST_CASE("GLDebug: VAO/VBO/Attribute 시퀀스 happy path", "[diagnostics][gl_debug]")
{
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    using SJH::Diagnostics::GLDebug;

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    REQUIRE(GLDebug::CheckGLGenVertexArrays());

    glBindVertexArray(vao);
    REQUIRE(GLDebug::CheckGLBindVertexArray(vao));

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    REQUIRE(GLDebug::CheckGLGenBuffers(vbo));

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    REQUIRE(GLDebug::CheckGLBindBuffer(vbo));

    const float vertices[] = { 0.0f, 0.0f, 0.0f };
    const GLint dataSize = static_cast<GLint>(sizeof(vertices));
    glBufferData(GL_ARRAY_BUFFER, dataSize, vertices, GL_STATIC_DRAW);
    REQUIRE(GLDebug::CheckGLBufferData(dataSize));

    glEnableVertexAttribArray(0);
    REQUIRE(GLDebug::CheckGLEnableVertexAttribArray(0));

    const GLuint stride = sizeof(float) * 3;
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    REQUIRE(GLDebug::CheckGLVertexAttribPointer({stride}));

    // cleanup
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

TEST_CASE("GLDebug::CheckGLBindVertexArray 는 invalid VAO 핸들을 잡는다", "[diagnostics][gl_debug]")
{
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    // glGenVertexArrays 가 절대 반환하지 않을 큰 핸들 -> GL_INVALID_OPERATION
    constexpr GLuint badVAO = 99999;
    glBindVertexArray(badVAO);
    REQUIRE_FALSE(SJH::Diagnostics::GLDebug::CheckGLBindVertexArray(badVAO));
}

TEST_CASE("GLDebug::CheckGLEnableVertexAttribArray 는 out-of-range index 를 잡는다", "[diagnostics][gl_debug]")
{
    SJH::test::GLContextFixture ctx;

    // 3.3 core: VAO 바인딩 필수 -> 미바인딩이면 GL_INVALID_OPERATION 이 됨.
    // 우리는 *index 범위* 를 테스트하고 싶으므로 VAO 먼저 바인딩.
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    DrainGLErrors();

    // GL_MAX_VERTEX_ATTRIBS 보다 훨씬 큰 index -> GL_INVALID_VALUE
    constexpr GLuint badIdx = 99999;
    glEnableVertexAttribArray(badIdx);
    REQUIRE_FALSE(SJH::Diagnostics::GLDebug::CheckGLEnableVertexAttribArray(badIdx));

    glDeleteVertexArrays(1, &vao);
}

TEST_CASE("GLDebug::CheckGLBufferData 는 unbound buffer 호출을 잡는다", "[diagnostics][gl_debug]")
{
    SJH::test::GLContextFixture ctx;

    // 명시적으로 0 (= no buffer) 바인딩 -> glBufferData 시 GL_INVALID_OPERATION
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    DrainGLErrors();

    const float dummy[] = { 0.0f };
    glBufferData(GL_ARRAY_BUFFER, sizeof(dummy), dummy, GL_STATIC_DRAW);
    REQUIRE_FALSE(SJH::Diagnostics::GLDebug::CheckGLBufferData(sizeof(dummy)));
}
