// SJH::Material characterization - Pass->Queue 매핑 / Properties typed bag / Instance metadata 계약 잠금.
// 핵심: Material 은 GL 호출 0 (uniform 송신은 draw 시점 MeshPassProcessor 책임) 이라 순수 CPU 로 구성/조회 가능.
//       Clone() 은 private+friend ResourceRegistry 라 직접 호출 불가 -> IsInstance/OriginalMaterial
//       public mutable 필드를 수동 배선해 GetRootOriginal 경로압축만 잠근다(contract-anchoring).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "material/material.h"
#include "material/material_uniforms.h"
#include "material/pass.h"

#include <glm/glm.hpp>

using Catch::Matchers::WithinAbs;
using SJH::Material;
namespace Pass = SJH::Pass;
namespace U    = SJH::Uniforms;

// === Pass::QueueOf - 순수 함수 매핑 잠금 ==================================
// underlying value = Unity Render Queue 정수. enum 값이 곧 queue 정수.
TEST_CASE("Pass: QueueOf 는 enum underlying + offset", "[material][pass]")
{
    REQUIRE(Pass::QueueOf(Pass::RenderQueue::Opaque) == 2000);
    REQUIRE(Pass::QueueOf(Pass::RenderQueue::AlphaTest) == 2450);
    REQUIRE(Pass::QueueOf(Pass::RenderQueue::Skybox) == 2500);
    REQUIRE(Pass::QueueOf(Pass::RenderQueue::Transparent) == 3000);
    REQUIRE(Pass::QueueOf(Pass::RenderQueue::Opaque, 5) == 2005); // offset
}

TEST_CASE("Pass: IsTransparentQueue 는 2500 임계값 기준", "[material][pass]")
{
    REQUIRE_FALSE(Pass::IsTransparentQueue(2000)); // Opaque
    REQUIRE(Pass::IsTransparentQueue(2500));       // 임계값 경계 포함
    REQUIRE(Pass::IsTransparentQueue(3000));       // Transparent
}

// === Material 기본 상태 ===================================================
TEST_CASE("Material: 기본 생성은 Opaque + Program 미설정", "[material]")
{
    auto mat = Material::Create();
    REQUIRE(mat->GetPass() == Pass::RenderQueue::Opaque);
    REQUIRE(mat->GetQueueLayer() == 2000); // Pass::QueueOf(Opaque)
    REQUIRE(mat->GetProgram() == nullptr);
    REQUIRE_FALSE(mat->IsInstance); // Create() 결과 = 공유 원본
}

// === SetPass -> GetQueueLayer + GetRenderStateBlock SSoT ==================
TEST_CASE("Material: SetPass 가 queue + RenderStateBlock 을 일괄 결정", "[material]")
{
    auto mat = Material::Create();

    mat->SetPass(Pass::RenderQueue::Transparent);
    REQUIRE(mat->GetPass() == Pass::RenderQueue::Transparent);
    REQUIRE(mat->GetQueueLayer() == 3000);

    // GetRenderStateBlock 은 SetPass 시 seed 된 ROP - DefaultRenderStateBlockOf 와 동일.
    const auto &rs = mat->GetRenderStateBlock();
    REQUIRE(rs.QueueLayer == 3000);
    REQUIRE(rs.BlendEnable == true);   // Transparent = alpha blend on
    REQUIRE(rs.DepthWrite == false);   // Transparent = depth write off
    REQUIRE(rs.CullMode == 0);         // Transparent = 양면 그리기 (cull off)
}

TEST_CASE("Material: SetPass(Opaque) RenderStateBlock 기본값", "[material]")
{
    auto mat = Material::Create();
    mat->SetPass(Pass::RenderQueue::Opaque);
    const auto &rs = mat->GetRenderStateBlock();
    REQUIRE(rs.DepthTest == true);
    REQUIRE(rs.DepthWrite == true);
    REQUIRE(rs.BlendEnable == false);
    REQUIRE(rs.QueueLayer == 2000);
}

// === SetProgram (nullptr 경유 - GL 호출 없음) =============================
TEST_CASE("Material: SetProgram(nullptr) 은 read back 가능 + fluent", "[material]")
{
    auto mat = Material::Create();
    // nullptr 주입 시 EagerBuild prune 루프는 prog 없이 no-op (GL 미접촉).
    Material &ref = mat->SetProgram(nullptr);
    REQUIRE(&ref == mat.get()); // fluent - *this 반환
    REQUIRE(mat->GetProgram() == nullptr);
}

// === Properties typed bag - store / readback (Uniforms::Set* 경유) ========
TEST_CASE("Material: Uniforms::Set* 는 typed map 에 store 만 (GL 미접촉)", "[material]")
{
    auto mat = Material::Create();

    U::SetFloat(*mat, "uShininess", 32.0f);
    U::SetInt(*mat, "uMode", 3);
    U::SetVec2(*mat, "uTile", glm::vec2(2.0f, 4.0f));
    U::SetVec3(*mat, "uColor", glm::vec3(0.1f, 0.2f, 0.3f));
    U::SetVec4(*mat, "uTint", glm::vec4(1.0f, 0.0f, 0.0f, 0.5f));

    REQUIRE_THAT(mat->Properties.Floats.at("uShininess"), WithinAbs(32.0f, 1e-6f));
    REQUIRE(mat->Properties.Ints.at("uMode") == 3);
    REQUIRE_THAT(mat->Properties.Vec2s.at("uTile").y, WithinAbs(4.0f, 1e-6f));
    REQUIRE_THAT(mat->Properties.Vec3s.at("uColor").g, WithinAbs(0.2f, 1e-6f));
    REQUIRE_THAT(mat->Properties.Vec4s.at("uTint").a, WithinAbs(0.5f, 1e-6f));
}

TEST_CASE("Material: 같은 키 재설정은 덮어쓰기 (map upsert)", "[material]")
{
    auto mat = Material::Create();
    U::SetFloat(*mat, "uK", 1.0f);
    U::SetFloat(*mat, "uK", 9.0f);
    REQUIRE(mat->Properties.Floats.size() == 1);
    REQUIRE_THAT(mat->Properties.Floats.at("uK"), WithinAbs(9.0f, 1e-6f));
}

TEST_CASE("Material: SetTexture(nullptr) 도 binding 을 store (unit 보존)", "[material]")
{
    auto mat = Material::Create();
    U::SetTexture(*mat, "uAlbedo", nullptr, 2); // 비소유 포인터 - GL 바인딩은 draw 시점
    REQUIRE(mat->Properties.Textures.count("uAlbedo") == 1);
    const auto &bind = mat->Properties.Textures.at("uAlbedo");
    REQUIRE(bind.Tex == nullptr);
    REQUIRE(bind.Unit == 2);
}

// === Instance metadata + GetRootOriginal 경로압축 ========================
// Clone() 은 private+friend ResourceRegistry 라 직접 호출 불가.
// IsInstance / OriginalMaterial 은 public mutable 필드라 수동 배선해 root 추적만 잠근다.
TEST_CASE("Material: root(원본)는 자기 자신을 root 로 반환", "[material]")
{
    auto root = Material::Create();
    // OriginalMaterial == nullptr 인 진짜 root - 자기 자신 반환.
    REQUIRE(root->GetRootOriginal() == root.get());
}

TEST_CASE("Material: 체인 root 추적 + 경로압축 캐시", "[material]")
{
    // root <- mid <- leaf 체인을 public 필드로 수동 배선 (Clone 대체).
    auto root = Material::Create();
    auto mid  = Material::Create();
    auto leaf = Material::Create();

    mid->IsInstance       = true;
    mid->OriginalMaterial = root.get();
    leaf->IsInstance      = true;
    leaf->OriginalMaterial = mid.get(); // 직접 부모 = mid

    // GetRootOriginal 은 OriginalMaterial 슬롯을 root 까지 거슬러 올라간다.
    REQUIRE(leaf->GetRootOriginal() == root.get());

    // 경로압축 - 호출 후 leaf 의 OriginalMaterial 슬롯이 root 로 갱신(mutable 캐시).
    REQUIRE(leaf->OriginalMaterial == root.get());
}
