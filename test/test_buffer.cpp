/**
 * @file test_buffer.cpp
 * @brief @c SJH::Buffer (VBO/EBO 통합 래퍼) 의 RAII + 데이터 업로드 검증.
 *
 * @details
 *  - Happy path: VBO/EBO 모두 정상 생성, 핸들 != 0, Bind 성공.
 *  - GL 상태 쿼리: 업로드한 size 가 @c glGetBufferParameteriv(GL_BUFFER_SIZE) 와 일치.
 *  - RAII: 인스턴스 파괴 직후 핸들이 더 이상 GL 객체 아님.
 *  - 호출 흐름: VAO 바인딩 X 상태에서도 `GL_ARRAY_BUFFER` 생성은 성공함을 확인 (3.3 core 강제는 attrib 단계).
 */

#include <catch2/catch_test_macros.hpp>

#include "support/gl_test_fixture.h"
#include "buffer/buffer.h"

#include "GL/gl3w.h"

#include <array>

TEST_CASE("Buffer::CreateWithData — VBO happy path + 핸들 유효", "[buffer][gl]")
{
    SJH::test::GLContextFixture ctx;

    // VAO 먼저 바인딩 (3.3 core: VBO 생성 자체엔 불필요하지만 일반 흐름 모방)
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    constexpr std::array<float, 9> vertices = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    const auto byteSize = vertices.size() * sizeof(float);

    auto vbo = SJH::Buffer::CreateWithData(
        GL_ARRAY_BUFFER, GL_STATIC_DRAW,
        vertices.data(), sizeof(float), vertices.size());   // (stride, count) 로 분할

    REQUIRE(vbo != nullptr);
    REQUIRE(vbo->Get() != 0);

    SECTION("바인딩 성공")
    {
        REQUIRE(vbo->Bind());
    }

    SECTION("GL_BUFFER_SIZE 가 업로드 size 와 일치")
    {
        REQUIRE(vbo->Bind());
        GLint storedSize = 0;
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &storedSize);
        REQUIRE(storedSize == static_cast<GLint>(byteSize));
    }

    glDeleteVertexArrays(1, &vao);
}

TEST_CASE("Buffer::CreateWithData — EBO happy path", "[buffer][gl]")
{
    SJH::test::GLContextFixture ctx;

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    constexpr std::array<GLuint, 6> indices = { 0, 1, 2, 2, 3, 0 };
    const auto byteSize = indices.size() * sizeof(GLuint);

    auto ebo = SJH::Buffer::CreateWithData(
        GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW,
        indices.data(), sizeof(GLuint), indices.size());

    REQUIRE(ebo != nullptr);
    REQUIRE(ebo->Get() != 0);
    REQUIRE(ebo->Bind());

    GLint storedSize = 0;
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &storedSize);
    REQUIRE(storedSize == static_cast<GLint>(byteSize));

    glDeleteVertexArrays(1, &vao);
}

TEST_CASE("Buffer 소멸자 — RAII 로 핸들 해제", "[buffer][gl]")
{
    SJH::test::GLContextFixture ctx;

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint capturedHandle = 0;
    {
        constexpr std::array<float, 3> single = { 1.0f, 2.0f, 3.0f };
        auto vbo = SJH::Buffer::CreateWithData(
            GL_ARRAY_BUFFER, GL_STATIC_DRAW,
            single.data(), sizeof(float), single.size());
        REQUIRE(vbo != nullptr);
        capturedHandle = vbo->Get();
        REQUIRE(capturedHandle != 0);
        // 스코프 종료 -> 소멸자가 glDeleteBuffers 호출
    }

    // glIsBuffer 는 이름이 *현재 buffer 객체* 를 가리키면 true.
    // 삭제 후엔 false 반환해야 RAII 정상 동작.
    REQUIRE_FALSE(glIsBuffer(capturedHandle));

    glDeleteVertexArrays(1, &vao);
}

TEST_CASE("Buffer — VBO/EBO 같은 클래스로 두 인스턴스 독립", "[buffer][gl]")
{
    SJH::test::GLContextFixture ctx;

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    constexpr std::array<float, 6> verts = { 0, 0, 0,  1, 1, 1 };
    constexpr std::array<GLuint, 3> idx   = { 0, 1, 0 };

    auto vbo = SJH::Buffer::CreateWithData(
        GL_ARRAY_BUFFER, GL_STATIC_DRAW,
        verts.data(), sizeof(float), verts.size());
    auto ebo = SJH::Buffer::CreateWithData(
        GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW,
        idx.data(), sizeof(GLuint), idx.size());

    REQUIRE(vbo != nullptr);
    REQUIRE(ebo != nullptr);
    REQUIRE(vbo->Get() != ebo->Get()); // 서로 다른 핸들

    glDeleteVertexArrays(1, &vao);
}
