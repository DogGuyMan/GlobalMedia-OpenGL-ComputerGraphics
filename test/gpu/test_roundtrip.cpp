/**
 * @file test_roundtrip.cpp
 * @brief C-3 GL roundtrip - buffer / texture / program 의 입력->GPU->읽기 왕복 일치 검증.
 *
 * @details
 *  ### 잠그는 현 동작 (characterization)
 *  - **buffer**: @c Buffer::CreateWithData 로 업로드한 바이트를 @c glGetBufferSubData 로
 *    읽어 입력과 바이트 단위 일치.
 *  - **texture**: 절차 생성 @c Image (단색) -> @c Texture::CreateTexture -> @c glGetTexImage
 *    로 level 0 픽셀을 읽어 일치. (업로드 internalFormat 은 항상 GL_RGBA - texture.cpp.)
 *  - **program**: 정상 셰이더(성공 보장 - fail-fast 무관)로 Program 생성 후 알려진 uniform
 *    @c GetLocation != -1, 없는 uniform == -1.
 *
 *  ### fail-fast 주의
 *  Program/Shader 팩토리는 실패 시 abort/throw 한다. 본 테스트는 *성공 보장* 셰이더만 쓴다.
 */

#include "gl_context_fixture.h"

#include "buffer/buffer.h"
#include "texture/image.h"
#include "texture/texture.h"
#include "program/program.h"
#include "shader/shader.h"

#include <glm/glm.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <vector>

TEST_CASE("C-3 buffer roundtrip - CreateWithData 후 glGetBufferSubData 가 입력과 일치", "[gpu][roundtrip][buffer]")
{
    using namespace SJH;
    REQUIRE(Test::Gpu::GLContextFixture::IsValid());

    // 임의의 float 정점 데이터 (interleaved 흉내 - 12 개).
    const std::vector<float> input = {
        0.0f, 1.0f, 2.0f, 3.0f,
        -1.5f, 2.5f, 100.0f, 0.25f,
        42.0f, -42.0f, 7.0f, 9.0f};

    BufferUPtr buf = Buffer::CreateWithData(
        GL_ARRAY_BUFFER, GL_STATIC_DRAW,
        input.data(), sizeof(float), input.size());
    REQUIRE(buf != nullptr);
    REQUIRE(buf->Get() != 0u);
    REQUIRE(buf->GetCount() == input.size());

    // 읽기 - 같은 버퍼를 바인딩 후 glGetBufferSubData.
    REQUIRE(buf->Bind());
    std::vector<float> readback(input.size(), 0.0f);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0,
                       static_cast<GLsizeiptr>(sizeof(float) * input.size()),
                       readback.data());

    // 바이트(여기선 float) 단위 정확 일치.
    for (size_t i = 0; i < input.size(); ++i)
    {
        INFO("index " << i);
        CHECK(readback[i] == input[i]);
    }
}

TEST_CASE("C-3 texture roundtrip - 단색 Image 업로드 후 glGetTexImage 픽셀 일치", "[gpu][roundtrip][texture]")
{
    using namespace SJH;
    REQUIRE(Test::Gpu::GLContextFixture::IsValid());

    // 4x4 RGBA 단색 이미지 (각 채널 0~1 -> 0~255). 0.5f 는 SetSingleColorImage 가
    // static_cast<GLubyte>(color * 255) 로 변환할 것으로 가정 - 읽기값도 동일 규칙으로 기대.
    const int W = 4, H = 4;
    const glm::vec4 color(1.0f, 0.0f, 0.0f, 1.0f); // 순수 빨강, 불투명 (반올림 모호성 회피)
    ImageUPtr img = Image::Create("c3_solid", W, H, 4);
    REQUIRE(img != nullptr);
    img->SetSingleColorImage(color);

    TextureUPtr tex = Texture::CreateTexture(img.get());
    REQUIRE(tex != nullptr);
    REQUIRE(tex->GetTextureID() != 0u);
    REQUIRE(tex->GetWidth() == W);
    REQUIRE(tex->GetHeight() == H);

    // 읽기 - level 0 을 GL_RGBA / UNSIGNED_BYTE 로 읽는다 (업로드 internalFormat=GL_RGBA).
    tex->Bind();
    std::vector<std::uint8_t> readback(static_cast<size_t>(W) * H * 4, 0);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, readback.data());

    const std::uint8_t expR = static_cast<std::uint8_t>(color.r * 255.0f);
    const std::uint8_t expG = static_cast<std::uint8_t>(color.g * 255.0f);
    const std::uint8_t expB = static_cast<std::uint8_t>(color.b * 255.0f);
    const std::uint8_t expA = static_cast<std::uint8_t>(color.a * 255.0f);

    for (int px = 0; px < W * H; ++px)
    {
        INFO("pixel " << px);
        CHECK(readback[px * 4 + 0] == expR);
        CHECK(readback[px * 4 + 1] == expG);
        CHECK(readback[px * 4 + 2] == expB);
        CHECK(readback[px * 4 + 3] == expA);
    }
}

TEST_CASE("C-3 program uniform - 알려진 uniform GetLocation != -1, 없는 uniform == -1", "[gpu][roundtrip][program]")
{
    using namespace SJH;
    REQUIRE(Test::Gpu::GLContextFixture::IsValid());

    // 성공 보장 GLSL 410 셰이더 - uColor 를 *반드시 사용*해야 active uniform 으로 남는다
    //   (미사용 uniform 은 GL 이 최적화로 제거 -> location -1 이 될 수 있음).
    const std::string vsSrc =
        "#version 410 core\n"
        "layout(location=0) in vec3 aPos;\n"
        "void main() { gl_Position = vec4(aPos, 1.0); }\n";
    const std::string fsSrc =
        "#version 410 core\n"
        "uniform vec4 uColor;\n"
        "out vec4 fragColor;\n"
        "void main() { fragColor = uColor; }\n";

    // CreateFromSource 는 성공 보장(정상 소스) - fail-fast 무관. unique_ptr -> shared_ptr 이전.
    ShaderPtr vs = ShaderPtr(Shader::CreateFromSource(vsSrc, GL_VERTEX_SHADER));
    ShaderPtr fs = ShaderPtr(Shader::CreateFromSource(fsSrc, GL_FRAGMENT_SHADER));
    REQUIRE(vs != nullptr);
    REQUIRE(fs != nullptr);

    ProgramUPtr prog = Program::Create({vs, fs});
    REQUIRE(prog != nullptr);
    REQUIRE(prog->GetProgramAddr() != 0u);

    // 알려진 active uniform.
    CHECK(prog->GetLocation("uColor") != -1);
    // 존재하지 않는 uniform.
    CHECK(prog->GetLocation("uDoesNotExist") == -1);
}
