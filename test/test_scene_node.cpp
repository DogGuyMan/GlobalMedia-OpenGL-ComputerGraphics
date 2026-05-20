/**
 * @file test_scene_node.cpp
 * @brief SJH::SceneNode — 월드 행렬 캐싱 + 계층 (GL 컨텍스트 불요).
 */
#include "object/scene_node.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vmath.h>

using Catch::Matchers::WithinAbs;

namespace
{
    void RequireMatNear(const vmath::mat4 &a, const vmath::mat4 &b, float eps = 1e-5f)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                REQUIRE_THAT(a[c][r], WithinAbs(b[c][r], eps));
    }
}

TEST_CASE("SceneNode 기본 — 부모 없는 노드의 World == 로컬 행렬", "[scene_node]")
{
    SJH::SceneNode node;
    RequireMatNear(node.World(), vmath::mat4::identity());

    SJH::Transform t;
    t.Translate = vmath::vec3(1.0f, 2.0f, 3.0f);
    node.SetLocal(t);
    RequireMatNear(node.World(), t.GetLocalMatrix());
}

TEST_CASE("SceneNode SetTranslate/SetScale — World 즉시 반영", "[scene_node]")
{
    SJH::SceneNode node;
    node.SetTranslate(vmath::vec3(5.0f, 0.0f, 0.0f));
    node.SetScale(vmath::vec3(2.0f, 2.0f, 2.0f));

    SJH::Transform expected;
    expected.Translate = vmath::vec3(5.0f, 0.0f, 0.0f);
    expected.Scale = vmath::vec3(2.0f, 2.0f, 2.0f);
    RequireMatNear(node.World(), expected.GetLocalMatrix());
}

TEST_CASE("SceneNode 캐시 — 변경 없이 두 번 호출하면 동일", "[scene_node]")
{
    SJH::SceneNode node;
    node.SetTranslate(vmath::vec3(1.0f, 1.0f, 1.0f));
    const vmath::mat4 first = node.World();
    const vmath::mat4 second = node.World();
    RequireMatNear(first, second);
}

TEST_CASE("SceneNode 계층 — World 는 부모 합성", "[scene_node]")
{
    SJH::SceneNode root, child;
    root.SetTranslate(vmath::vec3(10.0f, 0.0f, 0.0f));
    child.SetTranslate(vmath::vec3(0.0f, 5.0f, 0.0f));
    root.Attach(&child);

    REQUIRE(child.Parent() == &root);
    RequireMatNear(child.World(), root.World() * child.Local().GetLocalMatrix());
}

TEST_CASE("SceneNode 하향 dirty 전파 — 부모 변경이 자식 World 갱신", "[scene_node]")
{
    SJH::SceneNode root, child;
    child.SetTranslate(vmath::vec3(0.0f, 5.0f, 0.0f));
    root.Attach(&child);
    (void)child.World(); // 캐시 채움

    root.SetTranslate(vmath::vec3(100.0f, 0.0f, 0.0f)); // 부모 이동
    RequireMatNear(child.World(), root.World() * child.Local().GetLocalMatrix());
}

TEST_CASE("SceneNode Detach — 분리 후 자식은 부모 영향 제거", "[scene_node]")
{
    SJH::SceneNode root, child;
    root.SetTranslate(vmath::vec3(10.0f, 0.0f, 0.0f));
    child.SetTranslate(vmath::vec3(0.0f, 5.0f, 0.0f));
    root.Attach(&child);
    root.Detach(&child);

    REQUIRE(child.Parent() == nullptr);
    RequireMatNear(child.World(), child.Local().GetLocalMatrix());
}

TEST_CASE("SceneNode 재부착 — 새 부모로 옮기면 옛 부모 자식목록서 제거", "[scene_node]")
{
    SJH::SceneNode a, b, child;
    a.Attach(&child);
    b.Attach(&child); // child 를 b 로 이동
    REQUIRE(child.Parent() == &b);
    REQUIRE(a.Children().empty());
    REQUIRE(b.Children().size() == 1);
}

TEST_CASE("SceneNode 순환 가드 — 조상을 자식으로 부착 시 거부", "[scene_node]")
{
    SJH::SceneNode root, mid, leaf;
    root.Attach(&mid);
    mid.Attach(&leaf);
    leaf.Attach(&root); // leaf 아래에 조상 root 부착 시도 — 거부되어야 함
    REQUIRE(root.Parent() == nullptr);   // root 는 여전히 부모 없음
    REQUIRE(leaf.Children().empty());    // leaf 는 자식 없음
}

TEST_CASE("SceneNode WorldForward — 기본은 -Z", "[scene_node]")
{
    SJH::SceneNode node;
    const vmath::vec3 f = node.WorldForward();
    REQUIRE_THAT(f[0], WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(f[1], WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(f[2], WithinAbs(-1.0f, 1e-5f));
}

TEST_CASE("SceneNode WorldForward — yaw 90도면 -X", "[scene_node]")
{
    SJH::SceneNode node;
    node.SetEulerRot(vmath::vec3(0.0f, 90.0f, 0.0f));
    const vmath::vec3 f = node.WorldForward();
    REQUIRE_THAT(f[0], WithinAbs(-1.0f, 1e-5f));
    REQUIRE_THAT(f[1], WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(f[2], WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("SceneNode WorldForward — pitch -90도면 -Y", "[scene_node]")
{
    SJH::SceneNode node;
    node.SetEulerRot(vmath::vec3(-90.0f, 0.0f, 0.0f));
    const vmath::vec3 f = node.WorldForward();
    REQUIRE_THAT(f[0], WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(f[1], WithinAbs(-1.0f, 1e-5f));
    REQUIRE_THAT(f[2], WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("SceneNode WorldForward — 부모 회전 반영", "[scene_node]")
{
    SJH::SceneNode root, child;
    root.SetEulerRot(vmath::vec3(0.0f, 90.0f, 0.0f));
    root.Attach(&child);
    const vmath::vec3 f = child.WorldForward(); // child 로컬 identity, 부모 yaw 90
    REQUIRE_THAT(f[0], WithinAbs(-1.0f, 1e-5f));
    REQUIRE_THAT(f[2], WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("SceneNode TranslateBy — 증분 누적", "[scene_node]")
{
    SJH::SceneNode node;
    node.SetTranslate(vmath::vec3(1.0f, 0.0f, 0.0f));
    node.TranslateBy(vmath::vec3(2.0f, 3.0f, 0.0f));
    const vmath::mat4 w = node.World();
    REQUIRE_THAT(w[3][0], WithinAbs(3.0f, 1e-5f));
    REQUIRE_THAT(w[3][1], WithinAbs(3.0f, 1e-5f));
}

TEST_CASE("SceneNode TranslateBy — 자식 World 갱신 (dirty 전파)", "[scene_node]")
{
    SJH::SceneNode root, child;
    root.Attach(&child);
    (void)child.World();
    root.TranslateBy(vmath::vec3(5.0f, 0.0f, 0.0f));
    const vmath::mat4 cw = child.World();
    REQUIRE_THAT(cw[3][0], WithinAbs(5.0f, 1e-5f));
}
