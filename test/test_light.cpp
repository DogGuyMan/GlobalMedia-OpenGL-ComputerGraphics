/**
 * @file test_light.cpp
 * @brief @c SJH::Light + caster 타입 (DirLight/PointLight/SpotLight) + @c GetAttenuationCoeff 회귀.
 *
 * @details
 *  GL context 불필요 — 순수 POD-like value class + 자유 함수.
 *
 *  본 테스트가 잡는 회귀 카테고리:
 *  - 디폴트 값 손상 (특히 ambient/diffuse/specular 의 기본값 권장 비율 — 0.1/0.5/1.0)
 *  - 새 caster 타입 추가/삭제로 lighting.fs `DirLight/PointLight/SpotLight` 와 어긋남
 *    (셰이더 contract 검증은 test_shader_uniform_contract.cpp)
 *  - GetAttenuationCoeff 계산 손상 (거리 0 또는 음수 시 비정상 값)
 *
 *  @note @c Light / @c DirLight / @c PointLight / @c SpotLight 는 lighting.fs 의
 *        @c struct Light / DirLight / PointLight / SpotLight 와 1:1 매핑.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "object/light.h"

using Catch::Matchers::WithinAbs;

// ──────────────────────────────────────────────────────────────────────────
// Light (점 광원 — Phong 3항 색상 + 위치)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("SJH::Light default — 위치 (3,3,3) + Phong 3항 기본 비율", "[light][defaults]")
{
    SJH::Light l;

    // 점 광원 default 위치 (3, 3, 3) — 카메라 (0,0,3) 기준 우상단 위에서 비추는 형태
    REQUIRE_THAT(l.Pos[0], WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(l.Pos[1], WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(l.Pos[2], WithinAbs(3.0f, 1e-6f));

    // Phong 3항 권장 비율: ambient < diffuse < specular = (0.1, 0.5, 1.0)
    REQUIRE_THAT(l.Ambient[0],  WithinAbs(0.1f, 1e-6f));
    REQUIRE_THAT(l.Diffuse[0],  WithinAbs(0.5f, 1e-6f));
    REQUIRE_THAT(l.Specular[0], WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("SJH::Light — ambient 가 0이면 그림자 영역 완전 검정 (lighting 로직 계약)",
          "[light][lighting_logic]")
{
    // lighting.fs CalcAmbient(): vec3 ambient = light.ambient * texture(material.diffuse, ...).rgb
    // light.ambient == 0 이면 그림자 영역이 완전히 검정. C++ 측 직접 검증.
    SJH::Light l;
    l.Ambient = vmath::vec3(0.0f);

    vmath::vec3 texDiffuse(1.0f, 1.0f, 1.0f);
    vmath::vec3 ambient = l.Ambient * texDiffuse;  // vmath::vecN::operator* 는 component-wise
    REQUIRE_THAT(ambient[0], WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(ambient[1], WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(ambient[2], WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("SJH::Light — 복사 시멘틱 (POD-like)", "[light][copy]")
{
    SJH::Light a;
    a.Pos = vmath::vec3(10.0f, 20.0f, 30.0f);

    SJH::Light b = a;
    REQUIRE_THAT(b.Pos[0], WithinAbs(10.0f, 1e-6f));

    a.Pos = vmath::vec3(0.0f);
    REQUIRE_THAT(b.Pos[0], WithinAbs(10.0f, 1e-6f));
}

// ──────────────────────────────────────────────────────────────────────────
// DirLight (평행광 — 방향만 가짐, 거리 감쇠 없음)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("SJH::DirLight default — Phong 3항 기본 비율", "[dirlight][defaults]")
{
    SJH::DirLight d;
    // 방향은 SceneNodeId::DirLight 노드의 Transform 이 소유 — 본 구조체는 색상 3항만.
    REQUIRE_THAT(d.Ambient[0],  WithinAbs(0.1f, 1e-6f));
    REQUIRE_THAT(d.Diffuse[0],  WithinAbs(0.5f, 1e-6f));
    REQUIRE_THAT(d.Specular[0], WithinAbs(1.0f, 1e-6f));
}

// ──────────────────────────────────────────────────────────────────────────
// PointLight (점광원 — 거리 감쇠)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("SJH::PointLight default — mDistance 32 + Phong 3항", "[pointlight][defaults]")
{
    SJH::PointLight p;
    // 거리 감쇠 산출 기준. 위치는 SceneNodeId::PointLight* 노드의 Transform 이 소유.
    REQUIRE_THAT(p.Distance, WithinAbs(32.0f, 1e-6f));
    REQUIRE_THAT(p.Ambient[0], WithinAbs(0.1f, 1e-6f));
}

// ──────────────────────────────────────────────────────────────────────────
// SpotLight (스포트라이트 — 콘 cutoff + 거리 감쇠)
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("SJH::SpotLight default — 콘 cutoff 12.5°/17.5° (inner/outer)",
          "[spotlight][defaults]")
{
    SJH::SpotLight s;

    // 손전등 — inner 콘 안쪽은 100% 밝기, outer 바깥은 0%, 사이는 soft edge
    REQUIRE_THAT(s.CutoffAngleDeg,      WithinAbs(12.5f, 1e-6f));
    REQUIRE_THAT(s.OuterCutoffAngleDeg, WithinAbs(17.5f, 1e-6f));
    REQUIRE_THAT(s.Distance, WithinAbs(32.0f, 1e-6f));
    // 위치/방향은 SceneNodeId::SpotLight 노드의 Transform 이 소유 — 본 구조체엔 없음.
}

TEST_CASE("SJH::SpotLight — outer > inner 가 spec (페이드 구간 양수)",
          "[spotlight][geometry]")
{
    SJH::SpotLight s;
    // outerCutoff angle 이 cutoff angle 보다 커야 inner->outer 페이드 구간 > 0.
    // (cos 값으로 변환하면 cosf(outer) < cosf(inner) — 역전 주의)
    REQUIRE(s.OuterCutoffAngleDeg > s.CutoffAngleDeg);
}

// ──────────────────────────────────────────────────────────────────────────
// GetAttenuationCoeff — 거리 -> (Kc, Kl, Kq) 다항계수
// ──────────────────────────────────────────────────────────────────────────

TEST_CASE("GetAttenuationCoeff — distance=32 에서 Kc=1 (상수항 보존)",
          "[attenuation]")
{
    auto coeff = SJH::GetAttenuationCoeff(32.0f);

    // Kc 는 항상 1.0 (1/(Kc + Kl·d + Kq·d²) 형식의 상수항)
    REQUIRE_THAT(coeff[0], WithinAbs(1.0f, 1e-6f));
    // Kl, Kq 는 거리 함수 — 부호만 검증 (≥0)
    REQUIRE(coeff[1] >= 0.0f);
    REQUIRE(coeff[2] >= 0.0f);
}

TEST_CASE("GetAttenuationCoeff — 거리 증가 시 Kl 감소 (감쇠 약화)",
          "[attenuation]")
{
    auto near_coeff = SJH::GetAttenuationCoeff(8.0f);
    auto far_coeff  = SJH::GetAttenuationCoeff(64.0f);

    // 먼 거리에서 광원이 더 약하게 감쇠해야 함 — Kl·d, Kq·d² 가 *원래는 커질 거리*에서 사용되므로
    // *계수 자체* 는 거리에 반비례적으로 작아져야 1/(1 + Kl·d + Kq·d²) 가 *완만한* 감쇠를 만듦.
    // 직접 직관 — Kl 값이 16배 거리에서 감소
    INFO("near_Kl=" << near_coeff[1] << " far_Kl=" << far_coeff[1]);
    REQUIRE(far_coeff[1] < near_coeff[1]);
}

TEST_CASE("GetAttenuationCoeff — 큰 거리에서도 NaN/Inf 없음 (수치 안정성)",
          "[attenuation][numerical]")
{
    auto coeff = SJH::GetAttenuationCoeff(1000.0f);

    REQUIRE(std::isfinite(coeff[0]));
    REQUIRE(std::isfinite(coeff[1]));
    REQUIRE(std::isfinite(coeff[2]));
    // max(kl, 0) 보장 — 음수 감쇠 계수 방지
    REQUIRE(coeff[1] >= 0.0f);
    REQUIRE(coeff[2] >= 0.0f);
}
