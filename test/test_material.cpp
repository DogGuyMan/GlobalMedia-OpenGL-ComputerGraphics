/**
 * @file test_material.cpp
 * @brief @c Material — SP6 Unity 정통 properties bag 의 회귀 검증.
 *
 * @details
 *  GL context 불필요 — Material 은 *데이터 컨테이너*. Texture* / Program* 은 저장/반환만,
 *  deref 안 함 -> 센티넬 포인터로 충분.
 *
 *  본 테스트 카테고리 (SP6):
 *  - Properties bag default 상태 (모든 map 비어 있음).
 *  - SetFloat/SetVec3/SetTexture 자유함수 — bag 에 store.
 *  - Material Instance (Unreal MID 정통) — `ResourceRegistry::CreateMaterialInstanceFrom` 으로 Clone.
 *    `Material::Clone` 은 private + friend ResourceRegistry — 외부 직접 호출 컴파일 차단.
 *  - 동일 키 overwrite (Unity 동작).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "material/material.h"
#include "material/material_uniforms.h"
#include "resource_registry/resource_registry.h"

// 센티넬 포인터 — Material 은 저장/반환만 하고 deref 안 함.
static const SJH::Texture* const kDiffuseA = reinterpret_cast<const SJH::Texture*>(0xD1FFA);
static const SJH::Texture* const kSpecB    = reinterpret_cast<const SJH::Texture*>(0x5EC8B);

using Catch::Matchers::WithinAbs;

TEST_CASE("Material default — properties bag 이 비어 있음", "[material][defaults]")
{
    auto m_uptr = SJH::Material::Create();
    auto& m = *m_uptr;

    REQUIRE(m.Properties.Floats.empty());
    REQUIRE(m.Properties.Ints.empty());
    REQUIRE(m.Properties.Vec3s.empty());
    REQUIRE(m.Properties.Vec4s.empty());
    REQUIRE(m.Properties.Mat4s.empty());
    REQUIRE(m.Properties.Textures.empty());
    REQUIRE(m.GetProgram() == nullptr);
    REQUIRE(m.GetCache()   == nullptr);
}

TEST_CASE("Uniforms::SetFloat — properties bag 에 store", "[material][properties][float]")
{
    auto m_uptr = SJH::Material::Create();
    auto& m = *m_uptr;

    SJH::Uniforms::SetFloat(m, "material.shininess", 64.0f);
    REQUIRE(m.Properties.Floats.size() == 1);
    REQUIRE_THAT(m.Properties.Floats["material.shininess"], WithinAbs(64.0f, 1e-6f));

    // 동일 키 overwrite — Unity 정통 동작.
    SJH::Uniforms::SetFloat(m, "material.shininess", 128.0f);
    REQUIRE(m.Properties.Floats.size() == 1);
    REQUIRE_THAT(m.Properties.Floats["material.shininess"], WithinAbs(128.0f, 1e-6f));
}

TEST_CASE("Uniforms::SetInt — properties bag 에 store", "[material][properties][int]")
{
    auto m_uptr = SJH::Material::Create();
    auto& m = *m_uptr;

    SJH::Uniforms::SetInt(m, "dirLightEnabled", 1);
    REQUIRE(m.Properties.Ints["dirLightEnabled"] == 1);
}

TEST_CASE("Uniforms::SetVec3 — properties bag 에 store", "[material][properties][vec3]")
{
    auto m_uptr = SJH::Material::Create();
    auto& m = *m_uptr;

    SJH::Uniforms::SetVec3(m, "tint", vmath::vec3(1.0f, 0.5f, 0.25f));
    REQUIRE(m.Properties.Vec3s.size() == 1);
    auto& v = m.Properties.Vec3s["tint"];
    REQUIRE_THAT(v[0], WithinAbs(1.0f,  1e-6f));
    REQUIRE_THAT(v[1], WithinAbs(0.5f,  1e-6f));
    REQUIRE_THAT(v[2], WithinAbs(0.25f, 1e-6f));
}

TEST_CASE("Uniforms::SetTexture — properties bag 에 TextureBinding store", "[material][properties][texture]")
{
    auto m_uptr = SJH::Material::Create();
    auto& m = *m_uptr;

    SJH::Uniforms::SetTexture(m, "material.diffuse",  kDiffuseA, /*unit*/ 0);
    SJH::Uniforms::SetTexture(m, "material.specular", kSpecB,    /*unit*/ 1);

    REQUIRE(m.Properties.Textures.size() == 2);

    const auto& diffuseBinding = m.Properties.Textures["material.diffuse"];
    REQUIRE(diffuseBinding.Tex  == kDiffuseA);
    REQUIRE(diffuseBinding.Unit == 0);

    const auto& specularBinding = m.Properties.Textures["material.specular"];
    REQUIRE(specularBinding.Tex  == kSpecB);
    REQUIRE(specularBinding.Unit == 1);
}

TEST_CASE("Material Instance via ResourceRegistry — properties 복사 + Unreal MID 메타 + 독립 mutation",
          "[material][instance][resource_registry]")
{
    auto& reg = SJH::ResourceRegistry::Get();
    reg.Clear(); // 다른 테스트와의 키 충돌 방지.

    // ── shared 원본 ─────────────────────────────────────
    auto *a = reg.CreateSharedMaterial("test_mat_shared");
    REQUIRE(a != nullptr);
    REQUIRE_FALSE(a->IsInstance);
    REQUIRE(a->OriginalMaterial == nullptr);

    SJH::Uniforms::SetFloat  (*a, "material.shininess",  64.0f);
    SJH::Uniforms::SetTexture(*a, "material.diffuse",    kDiffuseA, /*unit*/ 0);
    SJH::Uniforms::SetVec3   (*a, "tint", vmath::vec3(1.0f, 0.0f, 0.0f));

    // ── instance (Unreal `UMaterialInstanceDynamic` 정통) ─
    // `Material::Clone` 은 private — public 진입점은 ResourceRegistry::CreateMaterialInstanceFrom.
    auto *b = reg.CreateMaterialInstanceFrom("test_mat_instance", a);
    REQUIRE(b != nullptr);

    // Instance 메타 — Unreal `UMaterialInstanceDynamic::Parent` 검증.
    REQUIRE(b->IsInstance);
    REQUIRE(b->OriginalMaterial == a);
    REQUIRE(b->GetRootOriginal() == a); // direct parent 가 root.

    // Properties 복사 — 값 복제.
    REQUIRE(b->Properties.Floats["material.shininess"] == 64.0f);
    REQUIRE(b->Properties.Textures["material.diffuse"].Tex  == kDiffuseA);
    REQUIRE(b->Properties.Textures["material.diffuse"].Unit == 0);
    REQUIRE_THAT(b->Properties.Vec3s["tint"][0], WithinAbs(1.0f, 1e-6f));

    // b 변경이 a 에 영향 없음 (독립 mutation).
    SJH::Uniforms::SetFloat(*b, "material.shininess", 2.0f);
    SJH::Uniforms::SetVec3 (*b, "tint", vmath::vec3(0.0f, 1.0f, 0.0f));

    REQUIRE_THAT(a->Properties.Floats["material.shininess"], WithinAbs(64.0f, 1e-6f));
    REQUIRE_THAT(a->Properties.Vec3s["tint"][0],             WithinAbs(1.0f,  1e-6f));
    REQUIRE_THAT(b->Properties.Vec3s["tint"][1],             WithinAbs(1.0f,  1e-6f));

    // 중복 키 거부 — 같은 인스턴스 키 재요청은 nullptr.
    REQUIRE(reg.CreateMaterialInstanceFrom("test_mat_instance", a) == nullptr);

    // Find* 조회.
    REQUIRE(reg.FindSharedMaterial("test_mat_shared")   == a);
    REQUIRE(reg.FindMaterialInstance("test_mat_instance") == b);

    reg.Clear(); // 테스트 격리 — 다음 TEST_CASE 가 깨끗한 registry 보장.
}

TEST_CASE("Material — 4 타입 동시 store / 독립 map 검증", "[material][properties][multi]")
{
    auto m_uptr = SJH::Material::Create();
    auto& m = *m_uptr;

    SJH::Uniforms::SetFloat(m, "x", 1.0f);
    SJH::Uniforms::SetInt  (m, "y", 2);
    SJH::Uniforms::SetVec3 (m, "z", vmath::vec3(3.0f));
    SJH::Uniforms::SetVec4 (m, "w", vmath::vec4(4.0f, 4.0f, 4.0f, 4.0f));
    SJH::Uniforms::SetMat4 (m, "mvp", vmath::mat4::identity());

    REQUIRE(m.Properties.Floats.size() == 1);
    REQUIRE(m.Properties.Ints.size()   == 1);
    REQUIRE(m.Properties.Vec3s.size()  == 1);
    REQUIRE(m.Properties.Vec4s.size()  == 1);
    REQUIRE(m.Properties.Mat4s.size()  == 1);

    // 서로 다른 map — 같은 키로 type 다른 properties 공존 가능 (Unity 와 동일).
    SJH::Uniforms::SetFloat(m, "color", 0.5f);
    SJH::Uniforms::SetVec3 (m, "color", vmath::vec3(1.0f, 0.0f, 0.0f));
    REQUIRE(m.Properties.Floats["color"] == 0.5f);
    REQUIRE(m.Properties.Vec3s["color"][0] == 1.0f);
}
