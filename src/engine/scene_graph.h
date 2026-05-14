#ifndef __ENGINE_SCENE_GRAPH_H__
#define __ENGINE_SCENE_GRAPH_H__

#include "engine/transform.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Engine::SceneGraph
{
	// 씬 트리(계층)는 SceneGraph 가 소유. Model 의 실제 GPU 자원은 ResourceManagement 가 갖고
	// 여기에는 Transform 노드만 부모-자식 포인터로 엮여 들어간다.
	class SceneGraph
	{
	  public:
		// ─── State ───────────────────────────────────────────────────
		std::unordered_map<std::string, std::unique_ptr<Transform::Transform>> hierarchies;
		const char *name_of_WorldRoot_transform = "WorldRoot";

		// ─── 현재 기능 ───────────────────────────────────────────────
		Transform::Transform *CreateRootTransform(const char *root_name);

		// 모든 root 트리 정리. GL 자원은 없으므로 단순 map 비우기.
		void Clear();

		// ─── 엔진이라면 반드시 필요 — 미구현 placeholder ────────────
		// 트리 탐색
		// Transform::Transform *Find(const std::string &path);            // "/WorldRoot/cube/leg" 식 경로 검색
		// void Traverse(Transform::Transform *root,
		//               const std::function<void(Transform::Transform *)> &visitor); // DFS / BFS visitor
		// std::vector<Transform::Transform *> FindByTag(const std::string &tag);
		//
		// 부모-자식 조작
		// void Reparent(Transform::Transform *child, Transform::Transform *new_parent); // cycle 가드 + Children 갱신
		// void Detach(Transform::Transform *node);                       // 부모에서 분리 (자식은 유지)
		// void Destroy(Transform::Transform *node);                      // 노드 + 서브트리 통째 제거
		//
		// 월드 매트릭스 캐시 — 매 프레임 매트릭스 재계산 비용 절감
		// void InvalidateWorldMatrix(Transform::Transform *node);        // 자기 + 서브트리 dirty 표시
		// const vmath::mat4 &GetWorldMatrix(const Transform::Transform *node); // dirty 일 때만 재계산
		//
		// 활성 / 가시 상태 (계층 전파)
		// void SetActive(Transform::Transform *node, bool active);
		// bool IsVisibleInHierarchy(const Transform::Transform *node) const;
		//
		// 태그 / 레이어
		// void SetTag(Transform::Transform *node, const std::string &tag);
		// void SetLayer(Transform::Transform *node, int layer);
		//
		// 라이프사이클 훅
		// std::function<void(Transform::Transform *)> OnAttachCallback;
		// std::function<void(Transform::Transform *)> OnDetachCallback;
		//
		// 디버그 / 직렬화
		// void DumpTree(std::ostream &os, const Transform::Transform *root = nullptr) const;
		// void Serialize(std::ostream &os) const;
		// void Deserialize(std::istream &is);
	};
} // namespace Engine::SceneGraph

#endif // __ENGINE_SCENE_GRAPH_H__
