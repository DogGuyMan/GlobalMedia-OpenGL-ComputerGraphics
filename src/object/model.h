/**
 * @file model.h
 * @brief Assimp 로 로드한 3D 모델 - 복수 Mesh + Material + Texture 의 수명 소유자.
 *
 * @details
 *  ### 책임
 *  - `Model::Load` - 파일에서 assimp 로 로드해 서브메시 트리를 @c RenderUnit 목록으로 변환.
 *  - @c mTextures / @c mMaterials 가 생존 기간 동안 GPU 자원 소유 (@c unique_ptr).
 *  - @c GetRenderUnits 로 ModelSpawner 등 Actor 펼침 호출자에게 @c RenderUnit 목록 노출.
 *
 *  ### 비-책임
 *  - [X] GL 드로우콜 직접 호출 - @c DeviceContext 가 담당 (Pattern Y 정통).
 *  - [X] 씬 트리 배치 - @c ModelSpawner / Actor 조립 코드가 담당.
 *  - [X] 애니메이션 처리 - 현재 정적 메시만 지원.
 *
 * @note 헤더가드는 @c __MODEL_H__ (SJH prefix 없음) - 레거시 명명.
 */

#ifndef __MODEL_H__
#define __MODEL_H__

#include "common/common.h"
#include "object/mesh.h"
#include "texture/texture.h"   // TextureUPtr - 모델이 보유하는 텍스처 lifetime
#include "material/material.h"
#include <vector>              // std::vector 직접 사용 - 구 mesh.h 전이 포함에 의존했음
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace SJH
{
    /**
     * @brief 1 메시 + 1 머티리얼 관찰자 쌍 - assimp 하위 메시 단위.
     * @details RenderUnit 이 @c mesh 를 유일 소유하고,
     *          @c material 은 @c Model::mMaterials 가 소유 (비소유 관찰자).
     */
    struct RenderUnit {
        MeshUPtr  mesh;                ///< 1 RenderUnit : 1 Mesh - RenderUnit 이 유일 소유
        Material* material{nullptr};   ///< 비소유 관찰자 - owner 는 Model::mMaterials
    };

    CLASS_PTR(Model);
    /**
     * @brief Assimp 로 로드한 3D 모델 - 복수 Mesh + Material 의 수명 소유자.
     * @details
     *  - 한 파일에서 여러 assimp 서브메시를 @ref RenderUnit 목록으로 변환.
     *  - @c mTextures / @c mMaterials 가 생존 기간 동안 GPU 자원 소유.
     *  - GL 드로우콜은 DeviceContext 게이트웨이가 담당 (Pattern Y 정통).
     */
    class Model
    {
    public:
        /// @brief assimp 로 파일을 로드해 Model 인스턴스 생성. 실패 시 @c nullptr.
        static ModelUPtr Load(const std::string &filename);

        /// @brief 서브메시(RenderUnit) 개수 반환.
        int GetMeshCount() const { return (int)mRenderUnit.size(); }
        /// @brief index 번째 메시의 비소유 관찰자. Model 보다 오래 보관 금지 - owner 는 RenderUnit.
        Mesh *GetMesh(int index) const { return mRenderUnit[(size_t)index].mesh.get(); }

        /// @brief 보유 머티리얼 개수.
        int GetMaterialCount() const { return (int)mMaterials.size(); }
        /// @brief index 번째 머티리얼의 비소유 관찰자 - 셋업 시 @c SetProgram 주입용.
        Material *GetMaterial(int index) const { return mMaterials[(size_t)index].get(); }

        /// @brief 모든 RenderUnit 의 const view - ModelSpawner 가 Actor 펼침 시 사용.
        const std::vector<RenderUnit>& GetRenderUnits() const { return mRenderUnit; }

    private:
        Model() = default;
        bool LoadByAssimp(const std::string &filename);
        void ProcessMesh(aiMesh *mesh, const aiScene *scene);
        void ProcessNode(aiNode *node, const aiScene *scene);

        std::vector<TextureUPtr>   mTextures;     ///< 로드한 텍스처 - Material 핸들의 lifetime owner. 관찰자보다 먼저 선언(나중 소멸).
        std::vector<MaterialUPtr>  mMaterials;    ///< Material 인스턴스 - 인덱스는 assimp mMaterialIndex.
        std::vector<RenderUnit>    mRenderUnit;   ///< 메시 목록 (Material 관찰자 보유).
    };

} // namespace

#endif // __MODEL_H__
