#ifndef __ENGINE_RESOURCE_MANAGEMENT_H__
#define __ENGINE_RESOURCE_MANAGEMENT_H__

#include "engine/material.h"
#include "engine/model_base.h"
#include "engine/transform.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Engine::ResourceManagement
{
	// GPU 자원 소유권은 ResourceManagement 가 보유. 외부(Model, Renderer) 는 raw 포인터로만 참조.
	class ResourceManagement
	{
	  public:
		// ─── Containers ───────────────────────────────────────────────
		std::unordered_map<std::string, std::unique_ptr<Model::ModelBase>> Models;
		std::unordered_map<std::string, std::unique_ptr<Material::Material>> Materials;

		// ─── 현재 기능 (Model 등록·조회) ──────────────────────────────
		void AddModel(const std::string &name,
		              std::unique_ptr<Model::ModelBase> model,
		              Transform::Transform *parent = nullptr);
		Model::ModelBase *GetModel(const std::string &name);
		bool HasModel(const std::string &name) const;
		void RemoveModel(const std::string &name);

		// ─── 현재 기능 (Material 등록·조회) ───────────────────────────
		Material::Material *AddMaterial(const std::string &name,
		                                std::unique_ptr<Material::Material> mat);
		Material::Material *GetMaterial(const std::string &name);

		// GL 컨텍스트 살아있는 동안 호출. Materials 가 먼저 (glDeleteTextures), 그 다음 Models (VAO/VBO).
		void TeardownGL();

		// ─── 엔진이라면 반드시 필요 — 미구현 placeholder ────────────
		// 자원 종류 확장 (현재는 Models/Materials 만 — Texture/Shader/Font/Audio 분리 필요)
		// std::unordered_map<std::string, std::unique_ptr<Texture>> Textures;
		// std::unordered_map<std::string, std::unique_ptr<Program::ShaderProgram>> Programs;
		// std::unordered_map<std::string, std::unique_ptr<Font>> Fonts;
		// std::unordered_map<std::string, std::unique_ptr<Sound>> Sounds;
		//
		// 캐시 / 중복 로드 방지
		// Texture *LoadTexture(const std::string &path);                 // 경로 기준 캐시 — 같은 파일 두 번 로드 방지
		// Program::ShaderProgram *LoadProgram(const std::string &vs, const std::string &fs);
		//
		// 참조 카운팅 + GC
		// void Acquire(const std::string &name);                         // 참조 카운트 ++
		// void Release(const std::string &name);                         // 참조 카운트 --, 0 되면 후보
		// void EvictUnused();                                            // 0 카운트 자원 일괄 정리
		//
		// 비동기 / 스트리밍 로드
		// std::future<Model::ModelBase *> AddModelAsync(const std::string &name, const std::string &path);
		// void Poll();                                                   // 매 프레임: 완료된 async 결과를 Models 에 흡수
		//
		// 핫 리로드 (파일 감시 → 변경 시 자동 reload)
		// void EnableHotReload(const std::string &asset_dir);
		// void RegisterDependency(const std::string &asset_path, const std::string &resource_name);
		//
		// 의존성 추적 (Material → Texture, Model → Material 등)
		// std::vector<std::string> GetDependencies(const std::string &resource_name) const;
		// void UnloadCascade(const std::string &resource_name);          // 자기 + 의존자 일괄 해제
		//
		// 그룹 단위 관리 (씬 전환 / 레벨 언로드 등)
		// void Tag(const std::string &name, const std::string &group);
		// void UnloadGroup(const std::string &group);
		//
		// 타입 안전 핸들 (raw 포인터 대신 weak handle)
		// template <typename T> class Handle { /* generation + index */ };
		// template <typename T> Handle<T> GetHandle(const std::string &name);
		//
		// 통계 / 디버그
		// size_t TotalGPUMemoryEstimate() const;
		// size_t TextureMemoryEstimate() const;
		// size_t VertexBufferMemoryEstimate() const;
		// void DumpStats(std::ostream &os) const;
		//
		// 직렬화 / 패킹
		// void SaveBundle(const std::string &path) const;                // 자원 묶음 저장
		// void LoadBundle(const std::string &path);                      // 자원 묶음 일괄 로드
	};
} // namespace Engine::ResourceManagement

#endif // __ENGINE_RESOURCE_MANAGEMENT_H__
