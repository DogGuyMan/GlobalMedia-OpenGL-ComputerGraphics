#ifndef __SJH_RESOURCE_REGISTRY_H__
#define __SJH_RESOURCE_REGISTRY_H__

/**
 * @file resource_registry.h
 * @brief Texture / Material / Model 자원의 lifecycle 중앙 관리 — 이름 키 캐시 + 일괄 해제.
 *
 * @details
 *  ### 동사 계약 (Task 2)
 *  - `Create*` — 새 자원을 *생성*하고 캐시에 등록. 이미 같은 키가 있으면 실패(nullptr).
 *  - `Find*`   — 캐시에서 *조회*만. 없으면 nullptr. 자원 생성/로드 없음.
 *
 *  Image 는 스코프 한정 — `CreateTexture` / `CreateMaterial` 이 GPU 업로드를 마치면 즉시 소멸.
 *  매니저는 Image 를 캐시하지 않는다.
 */

#include "image.h"
#include "texture.h"
#include "object/model.h"
#include "material/material.h"
#include "GL/gl3w.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace SJH
{
    CLASS_PTR(ResourceRegistry)
    /**
     * @brief Texture / Material / Model 자원을 *논리 이름 키*로 캐시하고, 매니저 소멸 시 일괄 해제.
     * @details
     *  - 자원 소유권은 매니저 보유 (@c unique_ptr).
     *  - 반환되는 raw 포인터는 *접근 전용* — 호출자는 매니저 수명 동안만 유효함을 가정.
     *  - Image 는 스코프 한정 (`Create*` 호출 스택 안에서만 유효) — 매니저가 보관하지 않음.
     */
    class ResourceRegistry
    {
    public:
        /// @brief 매니저 인스턴스 팩토리.
        static ResourceRegistryUPtr Create();

        /// @brief 보유 자원 일괄 해제 후 매니저 자체 소멸.
        ~ResourceRegistry();

        /// @brief Image 로부터 GPU 텍스처를 *생성*하고 @p key 로 캐시. 이미 있으면 실패(nullptr).
        Texture *CreateTexture(const std::string &key, const Image *image);

        /// @brief @p key 로 캐시된 텍스처 *조회* (생성 안 함). 없으면 nullptr.
        Texture *FindTexture(const std::string &key);

        /// @brief 빈 Material 을 *생성*하고 @p key 로 캐시. 이미 있으면 실패(nullptr).
        /// @details 텍스처 독립 — 호출자가 이후 @c Material::SetResolvedTextures / @c SetProgram 으로 배선.
        Material *CreateMaterial(const std::string &key);

        /// @brief @p key 로 캐시된 머티리얼 *조회* (생성 안 함). 없으면 nullptr.
        Material *FindMaterial(const std::string &key);

        /// @brief 파일에서 Model 을 *로드*해 @p key 로 캐시. 이미 있으면 실패(nullptr).
        Model *CreateModel(const std::string &key, const std::string &filename);

        /// @brief @p key 로 캐시된 Model *조회* (생성 안 함). 없으면 nullptr.
        Model *FindModel(const std::string &key);

        /// @brief 보유 모든 자원 일괄 해제 (매니저 인스턴스 자체는 유지).
        void Clear();

    private:
        std::unordered_map<std::string, TextureUPtr> mTextures;
        std::unordered_map<std::string, MaterialUPtr> mMaterials;
        std::unordered_map<std::string, ModelUPtr> mModels;
        ResourceRegistry() = default;
    };
}

#endif // __SJH_RESOURCE_REGISTRY_H__
