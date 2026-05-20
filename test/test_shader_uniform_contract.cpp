/**
 * @file test_shader_uniform_contract.cpp
 * @brief @c resources/shader/lighting.fs / @c lighting.vs 소스 ↔ C++ Material / Light caster
 *        클래스의 *contract* 검증.
 *
 * @details
 *  ### 동기
 *  C++ 측 클래스가 셰이더의 struct 와 *필드 단위로 일치*해야 uniform 업로드가 정상.
 *  본 테스트는 셰이더 *소스 텍스트*를 읽어 정규식으로 검증 — GL 컴파일 불필요, 빠름.
 *
 *  ### 잡는 회귀 (bug-coverage-audit.md 카테고리)
 *  - A1 : `#version` 이 macOS GL 4.1 상한 초과
 *  - A3 : GLSL bool 비트 OR `|` (컴파일 실패 패턴)
 *  - A4 : FS interface block `out` 방향 오류
 *  - D3/4/5 : uniform 누락 / 값 누락 (실패 체인의 출발점)
 *
 *  ### 마이그레이션 노트 (2026-05-09)
 *  simple.fs 가 minimal 로 분리되고 light cube 전용으로 변경됨. Material / Light caster contract
 *  는 lighting.fs 가 source of truth — 본 테스트는 lighting.fs / lighting.vs 를 검증.
 *
 * @see [resources/shader/lighting.fs](../resources/shader/lighting.fs)
 * @see [resources/shader/lighting.vs](../resources/shader/lighting.vs)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <regex>
#include <sstream>
#include <string>

using Catch::Matchers::ContainsSubstring;

namespace
{
    std::string LoadFile(const std::string &relPath)
    {
        const std::string fullPath = std::string(SJH_RESOURCES_DIR) + "/" + relPath;
        std::ifstream fin(fullPath);
        if (!fin.is_open())
        {
            FAIL("cannot open shader file: " << fullPath);
        }
        std::stringstream ss;
        ss << fin.rdbuf();
        return ss.str();
    }
}

// ──────────────────────────────────────────────────────────────────────────
// macOS GL 4.1 상한 검증 — BugReport.md §3 회귀 방지 (lighting.fs + lighting.vs)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("lighting.fs — #version 이 410 이하인지 (macOS GL 4.1 상한)",
          "[shader_contract][version]")
{
    auto src = LoadFile("shader/lighting.fs");

    std::regex versionRe(R"(#version\s+(\d+)\s+core)");
    std::smatch m;
    REQUIRE(std::regex_search(src, m, versionRe));
    REQUIRE(std::stoi(m[1].str()) <= 410);
    REQUIRE(std::stoi(m[1].str()) >= 330);
}

TEST_CASE("lighting.vs — #version 이 410 이하", "[shader_contract][version]")
{
    auto src = LoadFile("shader/lighting.vs");
    std::regex versionRe(R"(#version\s+(\d+)\s+core)");
    std::smatch m;
    REQUIRE(std::regex_search(src, m, versionRe));
    REQUIRE(std::stoi(m[1].str()) <= 410);
}

// ──────────────────────────────────────────────────────────────────────────
// lighting.vs ↔ mesh.h Vertex struct (3 attribs: pos/normal/uv)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("lighting.vs — layout (location 0/1/2) = aPos/aNormal/aTexCoord",
          "[shader_contract][vs][attribute]")
{
    auto src = LoadFile("shader/lighting.vs");

    // mesh.h Vertex { position(vec3) / normal(vec3) / texCoord(vec2) } 와 일치
    REQUIRE_THAT(src, ContainsSubstring("layout(location = 0) in vec3 aPos"));
    REQUIRE_THAT(src, ContainsSubstring("layout(location = 1) in vec3 aNormal"));
    REQUIRE_THAT(src, ContainsSubstring("layout(location = 2) in vec2 aTexCoord"));
}

// ──────────────────────────────────────────────────────────────────────────
// Material (sampler-based) ↔ material.h
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("lighting.fs — Material 구조체 (sampler2D diffuse/specular + float shininess)",
          "[shader_contract][material]")
{
    auto src = LoadFile("shader/lighting.fs");

    REQUIRE_THAT(src, ContainsSubstring("struct Material"));
    REQUIRE_THAT(src, ContainsSubstring("uniform Material material"));

    // sampler-based (이전의 vec3 ambient/diffuse 와 다름!)
    REQUIRE_THAT(src, ContainsSubstring("sampler2D diffuse"));
    REQUIRE_THAT(src, ContainsSubstring("sampler2D specular"));
    REQUIRE_THAT(src, ContainsSubstring("float shininess"));
    REQUIRE_THAT(src, ContainsSubstring("material.shininess"));
}

// ──────────────────────────────────────────────────────────────────────────
// 3 Light Caster Types ↔ light.h
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("lighting.fs — DirLight (direction + ambient/diffuse/specular)",
          "[shader_contract][light][dirlight]")
{
    auto src = LoadFile("shader/lighting.fs");

    REQUIRE_THAT(src, ContainsSubstring("struct DirLight"));
    REQUIRE_THAT(src, ContainsSubstring("uniform DirLight dirLight"));
    REQUIRE_THAT(src, ContainsSubstring("vec3 direction"));
}

TEST_CASE("lighting.fs — PointLight (position + attenuation + 3 채널 + 배열)",
          "[shader_contract][light][pointlight]")
{
    auto src = LoadFile("shader/lighting.fs");

    REQUIRE_THAT(src, ContainsSubstring("struct PointLight"));
    REQUIRE_THAT(src, ContainsSubstring("uniform PointLight pointLights"));
    REQUIRE_THAT(src, ContainsSubstring("vec3 position"));
    REQUIRE_THAT(src, ContainsSubstring("vec3 attenuation"));
    // NUM_POINT_LIGHTS 매크로 — C++ 측과 동기화 필요
    REQUIRE_THAT(src, ContainsSubstring("NUM_POINT_LIGHTS"));
}

TEST_CASE("lighting.fs — SpotLight (cutoff/outerCutoff 콘 + attenuation)",
          "[shader_contract][light][spotlight]")
{
    auto src = LoadFile("shader/lighting.fs");

    REQUIRE_THAT(src, ContainsSubstring("struct SpotLight"));
    REQUIRE_THAT(src, ContainsSubstring("uniform SpotLight spotLight"));
    REQUIRE_THAT(src, ContainsSubstring("float cutoff"));
    REQUIRE_THAT(src, ContainsSubstring("float outerCutoff"));
}

TEST_CASE("lighting.fs — Phong 3항 helper 함수 (Ambient/Diffuse/Specular)",
          "[shader_contract][phong]")
{
    auto src = LoadFile("shader/lighting.fs");

    REQUIRE_THAT(src, ContainsSubstring("CalcAmbient"));
    REQUIRE_THAT(src, ContainsSubstring("CalcDiffuse"));
    REQUIRE_THAT(src, ContainsSubstring("CalcSpecular"));

    // 거리 감쇠 + 콘 soft edge
    REQUIRE_THAT(src, ContainsSubstring("CalcAttenuation"));
    REQUIRE_THAT(src, ContainsSubstring("CalcSoftEdge"));
}

TEST_CASE("lighting.fs — main() 이 3 caster 합산 (DirLight + PointLight 배열 + SpotLight)",
          "[shader_contract][entry]")
{
    auto src = LoadFile("shader/lighting.fs");

    REQUIRE_THAT(src, ContainsSubstring("void main()"));
    REQUIRE_THAT(src, ContainsSubstring("CalcDirLight"));
    REQUIRE_THAT(src, ContainsSubstring("CalcPointLight"));
    REQUIRE_THAT(src, ContainsSubstring("CalcSpotLight"));
    REQUIRE_THAT(src, ContainsSubstring("out vec4 fragColor"));
}

// ──────────────────────────────────────────────────────────────────────────
// 부정 — GLSL 컴파일 실패 패턴 회귀 방지
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("lighting.fs — bool 비트연산 `|` 없음 (STUDY_NOTE Ex6 §1-5)",
          "[shader_contract][negative]")
{
    auto src = LoadFile("shader/lighting.fs");

    std::regex badBitOr(R"(\)\s*\|\s*[a-zA-Z_])");
    REQUIRE_FALSE(std::regex_search(src, badBitOr));
}

TEST_CASE("lighting.fs — `out VS_OUT` 같은 FS interface block 방향 오류 없음",
          "[shader_contract][negative]")
{
    auto src = LoadFile("shader/lighting.fs");
    REQUIRE_FALSE(src.find("out VS_OUT") != std::string::npos);
}

TEST_CASE("simple.fs — minimal (light cube 전용, baseColor 만)",
          "[shader_contract][simple_minimal]")
{
    // simple.fs 가 마이그레이션 후 light cube 전용 minimal shader가 됐음을 박는다.
    // 이전 Material/Light contract 책임은 lighting.fs 로 이전됨.
    auto src = LoadFile("shader/simple.fs");

    REQUIRE_THAT(src, ContainsSubstring("uniform vec4 baseColor"));
    REQUIRE_THAT(src, ContainsSubstring("out vec4 fragColor"));
    // light/material struct 가 *없어야* 함
    REQUIRE_FALSE(src.find("struct Light") != std::string::npos);
    REQUIRE_FALSE(src.find("struct Material") != std::string::npos);
}
