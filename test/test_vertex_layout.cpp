/**
 * @file test_vertex_layout.cpp
 * @brief @c SJH::VertexLayout (VAO RAII + Vertex Attribute setter) 검증.
 *
 * @details
 *  - Create: VAO 1개 생성 + 자동 바인딩 -> @c GL_VERTEX_ARRAY_BINDING 으로 확인.
 *  - Bind: 다른 VAO 가 활성인 상황에서 본 인스턴스로 전환되는지 확인.
 *  - TrySetAttrib: VBO 바인딩 후 호출 시 성공 + 속성 상태 쿼리 일치.
 *  - 소멸자: 인스턴스 파괴 후 핸들이 GL 객체 아님 (`glIsVertexArray`).
 */

#include <catch2/catch_test_macros.hpp>

#include "support/gl_test_fixture.h"
#include "layout/vertex_layout.h"
#include "buffer/buffer.h"

#include "GL/gl3w.h"

#include <array>

namespace
{
    // 매 테스트 시작 시 GL 에러 큐 비우기.
    void DrainGLErrors()
    {
        while (glGetError() != GL_NO_ERROR) { /* discard */ }
    }
}

TEST_CASE("VertexLayout::Create — VAO 생성 + 자동 바인딩", "[vertex_layout][gl]")
{
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    auto vao = SJH::VertexLayout::Create();
    REQUIRE(vao != nullptr);
    REQUIRE(vao->GetVAOAddr() != 0);

    // 현재 컨텍스트에 본 인스턴스의 VAO 가 바인딩되어 있어야 함 (Create 가 자동 Bind 함).
    GLint boundVAO = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundVAO);
    REQUIRE(static_cast<GLuint>(boundVAO) == vao->GetVAOAddr());
}

TEST_CASE("VertexLayout::Bind — 다른 VAO 활성 상태에서 전환", "[vertex_layout][gl]")
{
    SJH::test::GLContextFixture ctx;

    // 외부 VAO 를 먼저 활성화.
    GLuint externalVao = 0;
    glGenVertexArrays(1, &externalVao);
    glBindVertexArray(externalVao);

    auto layout = SJH::VertexLayout::Create();
    REQUIRE(layout != nullptr);
    // Create 가 자동 바인딩하므로 이미 layout 의 VAO 가 활성.

    // 외부 VAO 로 다시 전환 -> layout 은 비활성.
    glBindVertexArray(externalVao);
    GLint boundExt = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundExt);
    REQUIRE(static_cast<GLuint>(boundExt) == externalVao);

    // Bind 호출 -> layout 의 VAO 로 다시 활성.
    REQUIRE(layout->Bind());
    GLint boundAfter = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundAfter);
    REQUIRE(static_cast<GLuint>(boundAfter) == layout->GetVAOAddr());

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &externalVao);
}

TEST_CASE("VertexLayout::TrySetAttrib — VBO 바인딩 후 attribute 설정 성공", "[vertex_layout][gl]")
{
    SJH::test::GLContextFixture ctx;

    auto layout = SJH::VertexLayout::Create();
    REQUIRE(layout != nullptr);

    // 정점 데이터 + VBO 생성 (Buffer 클래스 활용).
    constexpr std::array<float, 9> vertices = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    auto vbo = SJH::Buffer::CreateWithData(
        GL_ARRAY_BUFFER, GL_STATIC_DRAW,
        vertices.data(), sizeof(float), vertices.size());
    REQUIRE(vbo != nullptr);

    DrainGLErrors();

    // location=0, vec3 float, stride=3*sizeof(float), offset=0
    REQUIRE(layout->TrySetAttrib(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0));

    // GL 상태 쿼리: 0번 attribute 가 enabled 인지.
    GLint enabled = 0;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
    REQUIRE(enabled == GL_TRUE);

    GLint size = 0;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
    REQUIRE(size == 3);

    GLint type = 0;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
    REQUIRE(type == GL_FLOAT);

    GLint stride = 0;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
    REQUIRE(stride == static_cast<GLint>(sizeof(float) * 3));
}

TEST_CASE("VertexLayout 소멸자 — RAII 로 VAO 해제", "[vertex_layout][gl]")
{
    SJH::test::GLContextFixture ctx;

    GLuint capturedHandle = 0;
    {
        auto layout = SJH::VertexLayout::Create();
        REQUIRE(layout != nullptr);
        capturedHandle = layout->GetVAOAddr();
        REQUIRE(capturedHandle != 0);
        // 스코프 종료 -> 소멸자가 glDeleteVertexArrays 호출
    }

    REQUIRE_FALSE(glIsVertexArray(capturedHandle));
}
