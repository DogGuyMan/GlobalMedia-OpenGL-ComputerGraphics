#ifndef __MODEL_H__
#define __MODEL_H__

#include "common/common.h"
#include "object/mesh.h"
#include "resource_registry/texture.h"   // TextureUPtr — 모델이 보유하는 텍스처 lifetime
#include "material/material.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace SJH
{
    /**
     * @brief 1 메시 + 1 머티리얼 관찰자 쌍 — assimp 하위 메시 단위.
     * @details RenderUnit 이 @c mesh 를 유일 소유하고,
     *          @c material 은 @c Model::mMaterials 가 소유 (비소유 관찰자).
     */
    struct RenderUnit {
        MeshUPtr  mesh;                ///< 1 RenderUnit : 1 Mesh — RenderUnit 이 유일 소유
        Material* material{nullptr};   ///< 비소유 관찰자 — owner 는 Model::mMaterials
    };

    CLASS_PTR(Model);
    /**
     * @brief Assimp 로 로드한 3D 모델 — 복수 Mesh + Material 의 수명 소유자.
     * @details
     *  - 한 파일에서 여러 assimp 서브메시를 @ref RenderUnit 목록으로 변환.
     *  - @c mTextures / @c mMaterials 가 생존 기간 동안 GPU 자원 소유.
     *  - @ref Draw 는 모든 RenderUnit 의 메시를 순서대로 드로우.
     */
    class Model
    {
    public:
        /// @brief assimp 로 파일을 로드해 Model 인스턴스 생성. 실패 시 @c nullptr.
        static ModelUPtr Load(const std::string &filename);

        /// @brief 서브메시(RenderUnit) 개수 반환.
        int GetMeshCount() const { return (int)mRenderUnit.size(); }
        /// @brief index 번째 메시의 비소유 관찰자. Model 보다 오래 보관 금지 — owner 는 RenderUnit.
        Mesh *GetMesh(int index) const { return mRenderUnit[index].mesh.get(); }

        /// @brief 보유 머티리얼 개수.
        int GetMaterialCount() const { return (int)mMaterials.size(); }
        /// @brief index 번째 머티리얼의 비소유 관찰자 — 셋업 시 @c SetProgram 주입용.
        Material *GetMaterial(int index) const { return mMaterials[index].get(); }

        /// @brief 모든 RenderUnit 을 순서대로 — 머티리얼 Apply 후 메시 드로우.
        void Draw() const;

    private:
        Model() = default;
        bool LoadByAssimp(const std::string &filename);
        void ProcessMesh(aiMesh *mesh, const aiScene *scene);
        void ProcessNode(aiNode *node, const aiScene *scene);

        std::vector<TextureUPtr>   mTextures;     ///< 로드한 텍스처 — Material 핸들의 lifetime owner. 관찰자보다 먼저 선언(나중 소멸).
        std::vector<MaterialUPtr>  mMaterials;    ///< Material 인스턴스 — 인덱스는 assimp mMaterialIndex.
        std::vector<RenderUnit>    mRenderUnit;   ///< 메시 목록 (Material 관찰자 보유).
    };

} // namespace

#endif // __MODEL_H__
