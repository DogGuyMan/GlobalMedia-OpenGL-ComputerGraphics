/**
 * @file test_material.cpp
 * @brief @c Material 값 클래스의 default value / setter 불변식 / copy 시멘틱 회귀.
 *
 * @details
 *  GL context 불필요 — 순수 value class (@c GLuint/@c GLint 는 타입 별칭일 뿐, GL 호출 없음).
 *
 *  본 테스트가 잡는 회귀 카테고리:
 *  - 디폴트 값 손상 (예: @c mShininess 32 -> 0 으로 잘못 변경 시 specular 무력화).
 *  - 이름 setter 의 *핸들 무효화* 누락 — 이름과 핸들이 어긋난 stale 상태.
 *  - @c SetShininess 의 clamp 범위 ([2, 256]) 손상.
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "material/material.h"

// Material 은 Texture* 를 저장/반환만 하고 deref 하지 않음 — 센티넬 포인터로 충분 (GL 불요).
static const SJH::Texture* const kDiffuseA = reinterpret_cast<const SJH::Texture*>(0xD1FFA);
static const SJH::Texture* const kSpecB    = reinterpret_cast<const SJH::Texture*>(0x5EC8B);

using Catch::Matchers::WithinAbs;

TEST_CASE("Material default 생성 — 기본값이 의도된 초기값과 일치", "[material][defaults]")
{
    auto m_uptr = SJH::Material::Create();   // private ctor 라 factory 경유.
    SJH::Material &m = *m_uptr;

    REQUIRE(m.GetDiffuseTextureName().empty());
    REQUIRE(m.GetSpecularTextureName().empty());
    REQUIRE(m.GetDiffuseTexture()  == nullptr);
    REQUIRE(m.GetSpecularTexture() == nullptr);
    REQUIRE(m.GetDiffuseUnit()  == 0);
    REQUIRE(m.GetSpecularUnit() == 1);
    REQUIRE_THAT(m.GetShininess(), WithinAbs(32.0f, 1e-6f));
    REQUIRE_FALSE(m.IsResolved());   // nullptr 이면 미해석
}

TEST_CASE("Material::SetTextureNames — 두 이름 동시 설정, 핸들은 미해석(nullptr) 유지",
          "[material][names]")
{
    auto m_uptr = SJH::Material::Create();   // private ctor 라 factory 경유.
    SJH::Material &m = *m_uptr;
    m.SetTextureNames("container2", "container2_specular");

    REQUIRE(m.GetDiffuseTextureName()  == "container2");
    REQUIRE(m.GetSpecularTextureName() == "container2_specular");
    // 이름만 설정 — 아직 resolve 안 함 -> nullptr.
    REQUIRE(m.GetDiffuseTexture()  == nullptr);
    REQUIRE(m.GetSpecularTexture() == nullptr);
    REQUIRE_FALSE(m.IsResolved());
}

TEST_CASE("Material — 이름 변경 시 해석된 핸들 *무효화*", "[material][names][invalidation]")
{
    auto m_uptr = SJH::Material::Create();
    SJH::Material &m = *m_uptr;
    m.SetTextureNames("container2", "container2_specular");
    m.SetResolvedTextures(kDiffuseA, /*diffUnit*/ 2, kSpecB, /*specUnit*/ 3);

    REQUIRE(m.IsResolved());
    REQUIRE(m.GetDiffuseTexture()  == kDiffuseA);
    REQUIRE(m.GetSpecularTexture() == kSpecB);

    m.SetDiffuseTextureName("wood");
    REQUIRE(m.GetDiffuseTextureName() == "wood");
    REQUIRE(m.GetDiffuseTexture()  == nullptr);   // 무효화됨
    REQUIRE(m.GetSpecularTexture() == kSpecB);    // 영향 없음
    REQUIRE_FALSE(m.IsResolved());

    m.SetSpecularTextureName("metal");
    REQUIRE(m.GetSpecularTexture() == nullptr);
}

TEST_CASE("Material::SetResolvedTextures — 관찰자 + 유닛 동시 설정", "[material][resolve]")
{
    auto m_uptr = SJH::Material::Create();
    SJH::Material &m = *m_uptr;
    m.SetResolvedTextures(kDiffuseA, /*diffUnit*/ 5, kSpecB, /*specUnit*/ 6);

    REQUIRE(m.GetDiffuseTexture()  == kDiffuseA);
    REQUIRE(m.GetSpecularTexture() == kSpecB);
    REQUIRE(m.GetDiffuseUnit()  == 5);
    REQUIRE(m.GetSpecularUnit() == 6);
    REQUIRE(m.IsResolved());
}

TEST_CASE("Material::SetShininess — [2, 256] 으로 clamp", "[material][shininess]")
{
    auto m_uptr = SJH::Material::Create();   // private ctor 라 factory 경유.
    SJH::Material &m = *m_uptr;

    SECTION("범위 내 값은 그대로")
    {
        m.SetShininess(64.0f);
        REQUIRE_THAT(m.GetShininess(), WithinAbs(64.0f, 1e-6f));
    }
    SECTION("하한 미만은 2 로 clamp")
    {
        m.SetShininess(0.0f);
        REQUIRE_THAT(m.GetShininess(), WithinAbs(2.0f, 1e-6f));
        m.SetShininess(-100.0f);
        REQUIRE_THAT(m.GetShininess(), WithinAbs(2.0f, 1e-6f));
    }
    SECTION("상한 초과는 256 으로 clamp")
    {
        m.SetShininess(1000.0f);
        REQUIRE_THAT(m.GetShininess(), WithinAbs(256.0f, 1e-6f));
    }
    SECTION("경계값은 통과")
    {
        m.SetShininess(2.0f);
        REQUIRE_THAT(m.GetShininess(), WithinAbs(2.0f, 1e-6f));
        m.SetShininess(256.0f);
        REQUIRE_THAT(m.GetShininess(), WithinAbs(256.0f, 1e-6f));
    }
}

TEST_CASE("Material — 복사 시멘틱 (자동 생성, 깊은 복사)", "[material][copy]")
{
    auto a_uptr = SJH::Material::Create();
    SJH::Material &a = *a_uptr;
    a.SetTextureNames("d", "s");
    a.SetResolvedTextures(kDiffuseA, 1, kSpecB, 2);
    a.SetShininess(128.0f);

    SJH::Material b = a;   // public copy ctor — value semantics
    REQUIRE(b.GetDiffuseTextureName()  == "d");
    REQUIRE(b.GetSpecularTextureName() == "s");
    REQUIRE(b.GetDiffuseTexture()  == kDiffuseA);
    REQUIRE(b.GetSpecularTexture() == kSpecB);
    REQUIRE_THAT(b.GetShininess(), WithinAbs(128.0f, 1e-6f));

    // 원본 변경이 복사본에 영향 없음.
    a.SetShininess(2.0f);
    a.SetDiffuseTextureName("changed");
    REQUIRE_THAT(b.GetShininess(), WithinAbs(128.0f, 1e-6f));
    REQUIRE(b.GetDiffuseTextureName() == "d");
}

TEST_CASE("Material::Clone — 독립 인스턴스, 원본 불변", "[material][clone]")
{
    auto base = SJH::Material::Create();
    base->SetTextureNames("brick", "brick_spec");
    base->SetResolvedTextures(kDiffuseA, 0, kSpecB, 1);
    base->SetShininess(64.0f);

    auto variant = base->Clone();   // MaterialUPtr
    REQUIRE(variant != nullptr);
    REQUIRE(variant.get() != base.get());                          // 다른 인스턴스
    REQUIRE(variant->GetDiffuseTextureName() == "brick");          // 값 복제
    REQUIRE(variant->GetDiffuseTexture() == kDiffuseA);
    REQUIRE_THAT(variant->GetShininess(), WithinAbs(64.0f, 1e-6f));

    // variant 의 텍스처 교체가 base 에 영향 없음.
    variant->SetDiffuseTextureName("brick_wet");
    REQUIRE(variant->GetDiffuseTextureName() == "brick_wet");
    REQUIRE(variant->GetDiffuseTexture() == nullptr);              // variant 핸들 무효화
    REQUIRE(base->GetDiffuseTextureName() == "brick");             // base 불변
    REQUIRE(base->GetDiffuseTexture() == kDiffuseA);
}
