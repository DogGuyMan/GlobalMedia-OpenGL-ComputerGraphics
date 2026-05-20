/**
 * @file test_scene_graph.cpp
 * @brief SJH::SceneGraph<TId> — enum 인덱싱 파사드 (GL 컨텍스트 불요).
 */
#include "object/scene_graph.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vmath.h>
#include <cstddef>

using Catch::Matchers::WithinAbs;

namespace
{
    // SceneGraph 는 TId 가 Count 멤버를 가진 enum 이라는 점만 가정 — 테스트용 enum.
    enum class TestNode : std::size_t
    {
        Root = 0, A, B, C, Count
    };

    void RequireMatNear(const vmath::mat4 &a, const vmath::mat4 &b, float eps = 1e-5f)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                REQUIRE_THAT(a[c][r], WithinAbs(b[c][r], eps));
    }
}

TEST_CASE("SceneGraph 기본 — 모든 노드 World == identity", "[scene_graph]")
{
    SJH::SceneGraph<TestNode> graph;
    RequireMatNear(graph.World(TestNode::Root), vmath::mat4::identity());
    RequireMatNear(graph.World(TestNode::C), vmath::mat4::identity());
}

TEST_CASE("SceneGraph SetLocal — enum 으로 노드 변환 설정", "[scene_graph]")
{
    SJH::SceneGraph<TestNode> graph;
    SJH::Transform t;
    t.Translate = vmath::vec3(3.0f, 0.0f, 0.0f);
    graph.SetLocal(TestNode::A, t);
    RequireMatNear(graph.World(TestNode::A), t.GetLocalMatrix());
}

TEST_CASE("SceneGraph Attach — 계층 World 합성", "[scene_graph]")
{
    SJH::SceneGraph<TestNode> graph;
    graph.At(TestNode::Root).SetTranslate(vmath::vec3(10.0f, 0.0f, 0.0f));
    graph.At(TestNode::A).SetTranslate(vmath::vec3(0.0f, 2.0f, 0.0f));
    graph.At(TestNode::B).SetTranslate(vmath::vec3(0.0f, 0.0f, 1.0f));
    graph.Attach(TestNode::A, TestNode::Root);
    graph.Attach(TestNode::B, TestNode::A);

    RequireMatNear(graph.World(TestNode::B),
                   graph.World(TestNode::A) * graph.At(TestNode::B).Local().GetLocalMatrix());
}

TEST_CASE("SceneGraph 하향 dirty 전파 — 조상 변경이 자손 World 갱신", "[scene_graph]")
{
    SJH::SceneGraph<TestNode> graph;
    graph.At(TestNode::A).SetTranslate(vmath::vec3(0.0f, 2.0f, 0.0f));
    graph.Attach(TestNode::A, TestNode::Root);
    (void)graph.World(TestNode::A);

    graph.At(TestNode::Root).SetTranslate(vmath::vec3(50.0f, 0.0f, 0.0f));
    RequireMatNear(graph.World(TestNode::A),
                   graph.World(TestNode::Root) * graph.At(TestNode::A).Local().GetLocalMatrix());
}

TEST_CASE("SceneGraph Detach — enum 으로 분리", "[scene_graph]")
{
    SJH::SceneGraph<TestNode> graph;
    graph.At(TestNode::Root).SetTranslate(vmath::vec3(10.0f, 0.0f, 0.0f));
    graph.Attach(TestNode::A, TestNode::Root);
    graph.Detach(TestNode::A);
    REQUIRE(graph.At(TestNode::A).Parent() == nullptr);
}

TEST_CASE("SceneGraph WorldForward — enum 으로 노드 전방", "[scene_graph]")
{
    SJH::SceneGraph<TestNode> graph;
    graph.At(TestNode::A).SetEulerRot(vmath::vec3(0.0f, 90.0f, 0.0f));
    const vmath::vec3 f = graph.WorldForward(TestNode::A);
    REQUIRE_THAT(f[0], WithinAbs(-1.0f, 1e-5f));
    REQUIRE_THAT(f[2], WithinAbs(0.0f, 1e-5f));
}
