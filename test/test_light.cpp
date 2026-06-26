// SJH::Light(object/light.h) POD 기본값 + GetAttenuationCoeff 구조 property characterization.
// scene caster 라이트(DirLight/PointLight/SpotLight)는 SJH::scene 소속 -> 별도 테스트로 defer
// (contract-anchoring: 이 exe 는 SJH::object 만 link). spec D3/D8.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "object/light.h"
#include <cmath> // std::isfinite

using Catch::Matchers::WithinAbs;

TEST_CASE("Light: 기본값", "[light]")
{
    SJH::Light l;
    REQUIRE_THAT(l.Pos.x, WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(l.Pos.y, WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(l.Pos.z, WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(l.Ambient.x, WithinAbs(0.1f, 1e-6f));
    REQUIRE_THAT(l.Diffuse.x, WithinAbs(0.5f, 1e-6f));
    REQUIRE_THAT(l.Specular.x, WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("Light: GetAttenuationCoeff 구조 property", "[light]")
{
    // 반환 = vec3(Kc=1, max(Kl,0), max(Kq^2,0)). 정확 산술 대신 계약 property 검증(D8a).
    glm::vec3 c = SJH::GetAttenuationCoeff(32.0f);
    REQUIRE_THAT(c.x, WithinAbs(1.0f, 1e-6f)); // Kc 상수 1
    REQUIRE(c.y > 0.0f);                       // Kl > 0
    REQUIRE(c.z >= 0.0f);                      // Kq^2 >= 0
}

TEST_CASE("Light: 감쇠는 거리 증가에 단조 감소(Kl)", "[light]")
{
    float near = SJH::GetAttenuationCoeff(8.0f).y;
    float far = SJH::GetAttenuationCoeff(64.0f).y;
    REQUIRE(far < near); // 멀수록 Kl 작음
}

TEST_CASE("Light: 큰 거리에서 유한값(NaN/Inf 없음)", "[light]")
{
    glm::vec3 c = SJH::GetAttenuationCoeff(1000.0f);
    REQUIRE(std::isfinite(c.x));
    REQUIRE(std::isfinite(c.y));
    REQUIRE(std::isfinite(c.z));
}
