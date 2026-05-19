/**
 * @file resource_registry.cpp
 * @brief Create* — 캐시-미스 경로에서 새 자원을 생성.
 *        Find*   — 캐시-히트 경로에서 기존 인스턴스를 즉시 반환.
 *
 * @details emplace 결과의 iterator 로 raw 포인터를 꺼내 반환 — 매니저 보관 인스턴스를 가리키므로
 *          호출자에게 노출되는 lifetime 은 매니저 자신의 lifetime 과 동일하다.
 *          Image 는 스코프 한정 — GPU 업로드 후 Create* 스택 프레임을 벗어나면 즉시 소멸.
 */
#include "resource_registry.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace SJH
{
    ResourceRegistryUPtr ResourceRegistry::Create()
    {
        auto resourceManager = std::unique_ptr<ResourceRegistry>(new ResourceRegistry());
        return std::move(resourceManager);
    }

    ResourceRegistry::~ResourceRegistry()
    {
        Clear();
    }

    Texture *ResourceRegistry::CreateTexture(const std::string &key, const Image *image)
    {
        if (mTextures.find(key) != mTextures.end())
        {
            spdlog::warn("CreateTexture: 키 '{}' 가 이미 존재 — Find 를 먼저 호출하라", key);
            return nullptr;
        }
        auto texture = Texture::CreateTexture(image);
        if (texture == nullptr)
        {
            spdlog::error("CreateTexture: GPU 텍스처 생성 실패 — key '{}'", key);
            return nullptr;
        }
        auto insertedIt = mTextures.emplace(key, std::move(texture)).first;
        return insertedIt->second.get();
    }

    Texture *ResourceRegistry::FindTexture(const std::string &key)
    {
        auto it = mTextures.find(key);
        return (it != mTextures.end()) ? it->second.get() : nullptr;
    }

    Material *ResourceRegistry::CreateMaterial(const std::string &key)
    {
        // 텍스처 독립 — 빈 Material 만 생성·캐시. 텍스처/프로그램 배선은 호출자 책임.
        if (mMaterials.find(key) != mMaterials.end())
        {
            spdlog::warn("CreateMaterial: 키 '{}' 가 이미 존재 — Find 를 먼저 호출하라", key);
            return nullptr;
        }
        auto insertedIt = mMaterials.emplace(key, Material::Create()).first;
        return insertedIt->second.get();
    }

    Material *ResourceRegistry::FindMaterial(const std::string &key)
    {
        auto it = mMaterials.find(key);
        return (it != mMaterials.end()) ? it->second.get() : nullptr;
    }

    Model *ResourceRegistry::CreateModel(const std::string &key, const std::string &filename)
    {
        if (mModels.find(key) != mModels.end())
        {
            spdlog::warn("CreateModel: 키 '{}' 가 이미 존재 — Find 를 먼저 호출하라", key);
            return nullptr;
        }
        auto model = Model::Load(filename);
        if (model == nullptr)
        {
            spdlog::error("CreateModel: 모델 로드 실패 — key '{}', file '{}'", key, filename);
            return nullptr;
        }
        auto insertedIt = mModels.emplace(key, std::move(model)).first;
        return insertedIt->second.get();
    }

    Model *ResourceRegistry::FindModel(const std::string &key)
    {
        auto it = mModels.find(key);
        return (it != mModels.end()) ? it->second.get() : nullptr;
    }

    void ResourceRegistry::Clear()
    {
        mTextures.clear();
        mMaterials.clear();
        mModels.clear();
    }

}
