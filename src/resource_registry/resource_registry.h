#ifndef __SJH_RESOURCE_REGISTRY_H__
#define __SJH_RESOURCE_REGISTRY_H__

/**
 * @file resource_registry.h
 * @brief Texture / Material / Model / Program / Mesh 자원의 lifecycle 중앙 관리 —
 *        이름 키 캐시 + 일괄 해제 + 싱글톤 접근.
 *
 * @details
 *  ### 동사 계약
 *  - `Create*`   — 새 자원을 *생성*하고 캐시에 등록. 이미 같은 키가 있으면 실패(nullptr).
 *  - `Register*` — *외부에서 만든 자원의 소유권 이전* (Mesh 처럼 Create 와 별개 factory 가 다수일 때).
 *  - `Find*`     — 캐시에서 *조회*만. 없으면 nullptr. 자원 생성/로드 없음.
 *
 *  Image 는 스코프 한정 — `CreateTexture` 가 GPU 업로드를 마치면 즉시 소멸. 매니저는 Image 를 캐시하지 않는다.
 *
 *  ### 접근
 *  Cocos `cc::Director::TextureCache` / Unity `Resources` / SP2 `DeviceContext::Get()` 정통 —
 *  Meyer's 싱글톤 `ResourceRegistry::Get()` 으로 전역 1 인스턴스. 챕터/app 의 자원 보유 컨벤션은
 *  `.claude/architecture.md §11.3` 참조.
 */

#include "buffer/framebuffer.h"
#include "common/common.h"
#include "image.h"
#include "material/material.h"
#include "object/mesh.h"
#include "object/model.h"
#include "program/program.h"
#include "sprite/uniform_atlas.h"
#include "texture.h"
#include <string>
#include <unordered_map>

namespace SJH
{
	CLASS_PTR(ResourceRegistry)
	/**
	 * @brief Texture / Material / Model / Program / Mesh 자원을 *논리 이름 키*로 캐시하고,
	 *        매니저 소멸 시 일괄 해제.
	 * @details
	 *  - 자원 소유권은 매니저 보유 (@c unique_ptr).
	 *  - 반환되는 raw 포인터는 *접근 전용* — 호출자는 매니저 수명 동안만 유효함을 가정.
	 *  - Image 는 스코프 한정 (`CreateTexture` 호출 스택 안에서만 유효) — 매니저가 보관하지 않음.
	 */
	class ResourceRegistry
	{
	  public:
		/// @brief 싱글톤 접근 — Meyer's. SP2 `DeviceContext::Get()` 패턴과 일관.
		/// @details 첫 호출 시 lazy 인스턴스화. thread-safe (C++11 static local).
		static ResourceRegistry &Get();

		/// @brief 보유 자원 일괄 해제 후 매니저 자체 소멸.
		~ResourceRegistry();

		/// @brief Image 로부터 GPU 텍스처를 *생성*하고 @p key 로 캐시. 이미 있으면 실패(nullptr).
		Texture *CreateTexture(const std::string &key, const Image *image);

		/// @brief @p key 로 캐시된 텍스처 *조회* (생성 안 함). 없으면 nullptr.
		Texture *FindTexture(const std::string &key);

		/// @brief 빈 Material 을 *생성*하고 @p key 로 캐시. 이미 있으면 실패(nullptr).
		/// @details 텍스처 독립 — 호출자가 이후 @c Material::SetResolvedTextures / @c SetProgram 으로 배선.
		///   *공유본* (Unity `sharedMaterial` 정통) — 모든 사용자가 같은 인스턴스. 결과 `IsInstance=false`.
		Material *CreateSharedMaterial(const std::string &key);

		/// @brief @p key 로 캐시된 머티리얼 *조회* (생성 안 함). 없으면 nullptr.
		Material *FindSharedMaterial(const std::string &key);

		/// @brief @p template_ 의 Clone + 인스턴스 등록 한 호출 — Owner 가 *항상* ResourceRegistry.
		/// @details
		///   - Clone() + RegisterMaterialInstance 의 2 단계를 *원자적으로* 묶음 —
		///     호출자가 MaterialUPtr 을 *지역 변수로 보유* 하는 실수 회피 (dangling 차단).
		///   - 결과는 `IsInstance=true` + `OriginalMaterial=template_` (Unreal `UMaterialInstanceDynamic` 정통).
		///   - Outline / 변형 사용 사례 — `mat->SetPass(Kind::OutlineVisible)` 등.
		Material *CreateMaterialInstanceFrom(const std::string &key, const Material *template_);

		/// @brief @p key 로 캐시된 *인스턴스* 머티리얼 조회 (생성 안 함). 없으면 nullptr.
		Material *FindMaterialInstance(const std::string &key);

		/// @brief 파일에서 Model 을 *로드*해 @p key 로 캐시. 이미 있으면 실패(nullptr).
		Model *CreateModel(const std::string &key, const std::string &filename);

		/// @brief @p key 로 캐시된 Model *조회* (생성 안 함). 없으면 nullptr.
		Model *FindModel(const std::string &key);

		/// @brief vs/fs 셰이더 파일에서 Program 을 *생성*해 @p key 로 캐시.
		///        이미 있거나 셰이더 로드 실패 시 nullptr.
		Program *CreateProgram(const std::string &key,
		                       const std::string &vertShaderFilename,
		                       const std::string &fragShaderFilename);

		/// @brief @p key 로 캐시된 Program *조회* (생성 안 함). 없으면 nullptr.
		Program *FindProgram(const std::string &key);

		/// @brief 외부에서 만든 Mesh 의 소유권을 이전해 @p key 로 캐시.
		/// @details Mesh 는 factory 가 여러 종류 (`CreateBox` / `CreatePlane` / 향후 더) — registry 가
		///          직접 Create 하지 않고 *위탁 (Register)* 패턴. 이미 있거나 @p mesh 가 nullptr 이면 실패.
		/// @return 캐시 내 비소유 핸들 (호출자가 추가 셋업 시).
		Mesh *RegisterMesh(const std::string &key, MeshUPtr mesh);

		/// @brief @p key 로 캐시된 Mesh *조회* (생성 안 함). 없으면 nullptr.
		Mesh *FindMesh(const std::string &key);

		/// @brief @p key 로 Framebuffer (FBO) 를 *생성*하고 캐시. 이미 있으면 실패(nullptr).
		/// @details SP-RTRegistry — 옛 `Framebuffer::Create()` 직접 호출 흐름의 위탁 패턴.
		///   `DefaultRenderTarget` (window backbuffer) 은 *Resource 가 아니라 Application 책임* — 본 매니저 대상 아님.
		Framebuffer *CreateFramebuffer(const std::string &key, int width, int height);

		/// @brief @p key 로 캐시된 Framebuffer *조회* (생성 안 함). 없으면 nullptr.
		Framebuffer *FindFramebuffer(const std::string &key);

		/// @brief PNG 로드 + grid 명시까지 한 호출로 UniformAtlas 를 *생성*하고 @p key 로 캐시.
		/// @details
		///   - CreateTexture / CreateModel 정통 — 호출자가 Fluent Builder 를 직접 들고 다니지 않고
		///     registry 가 `LoadFromPNG(pngPath).SetGrid(cols, rows)` 까지 일괄 수행.
		///   - 픽셀아트 NEAREST + CLAMP 는 `UniformAtlas::LoadFromPNG` 내부에서 적용.
		///   - 이미 있거나 PNG 로드/grid 검증 실패 (atlasW/H 가 cols/rows 로 나누어떨어지지 않는 경우 등) 시 nullptr.
		Sprite::UniformAtlas *CreateUniformAtlas(const std::string &key,
		                                         const std::string &pngPath,
		                                         int cols, int rows);

		/// @brief @p key 로 캐시된 UniformAtlas *조회* (생성 안 함). 없으면 nullptr.
		Sprite::UniformAtlas *FindUniformAtlas(const std::string &key);

		/// @brief 보유 모든 자원 일괄 해제 (매니저 인스턴스 자체는 유지).
		void Clear();

		// 싱글톤 — 복사/이동 금지.
		ResourceRegistry(const ResourceRegistry &) = delete;
		ResourceRegistry &operator=(const ResourceRegistry &) = delete;
		ResourceRegistry(ResourceRegistry &&) = delete;
		ResourceRegistry &operator=(ResourceRegistry &&) = delete;

	  private:
		ResourceRegistry() = default;

		std::unordered_map<std::string, TextureUPtr> mTextures;
		std::unordered_map<std::string, MaterialUPtr> mSharedMaterials;
		std::unordered_map<std::string, MaterialUPtr> mMaterialInstances;
		std::unordered_map<std::string, ModelUPtr> mModels;
		std::unordered_map<std::string, ProgramUPtr> mPrograms;
		std::unordered_map<std::string, MeshUPtr> mMeshes;
		std::unordered_map<std::string, FramebufferUPtr> mFramebuffers;
		std::unordered_map<std::string, Sprite::UniformAtlasUPtr> mAtlas;
	};
} // namespace SJH

#endif // __SJH_RESOURCE_REGISTRY_H__
