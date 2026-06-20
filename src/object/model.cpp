/**
 * @file model.cpp
 * @brief Model::Load / LoadByAssimp / ProcessNode / ProcessMesh 구현.
 * @details
 *  ### 로드 흐름
 *  1. `Load` - `ModelUPtr(new Model())` 생성 후 `LoadByAssimp` 위임.
 *  2. `LoadByAssimp` - `Assimp::Importer::ReadFile` (`aiProcess_Triangulate | aiProcess_FlipUVs`).
 *  3. 씬의 `mNumMaterials` 순회 - diffuse/specular 텍스처 추출 + `Material::Create` 조합.
 *     텍스처 없는 머티리얼은 `aiColor_Diffuse` -> `material.albedo` (vec3) 로 fallback.
 *  4. `ProcessNode` 재귀 (DFS) -> `ProcessMesh` 에서 `Geometry::FromAssimp` + `Mesh::Create`.
 *
 *  ### 람다 `_lambdaLoadTexture` 설계 이유
 *  - `[&]` 캡처로 `dirname` / `mTextures` 를 공유 - `LoadByAssimp` 스코프 내에서만 유효.
 *  - 멤버 함수로 분리하면 `model.h` 노출 + 헤더 재컴파일 비용 -> 스코프 한정 람다 우선.
 *
 * @note `mTextures` 는 `mMaterials` 보다 먼저 선언되어 나중에 소멸 -
 *       머티리얼의 `Texture*` 관찰자가 소멸 전 dangling 되지 않는 것을 보장.
 */
#include "model.h"
#include "object/geometry.h"
#include "texture/texture.h"
#include "material/material.h"
#include <assimp/material.h>
#include <spdlog/spdlog.h>
#include <vmath.h>

namespace SJH
{
    ModelUPtr Model::Load(const std::string &filename)
    {
        auto model = ModelUPtr(new Model());
        if (!model->LoadByAssimp(filename))
            return nullptr;
        return std::move(model);
    }

    // scene->mRootNode부터 재귀적으로 처리
    bool Model::LoadByAssimp(const std::string &filename)
    {
        Assimp::Importer importer;
        auto scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            spdlog::error("failed to load model: {}", filename);
            return false;
        }

        // [&] : 외부 변수 전부를 참조로 캡처. dirname,mTextures 접근에 사용.
        //        수명은 LoadByAssimp 스코프 안으로 한정 -> 댕글링 위험 없음.
        //
        // 람다 캡처 리스트 종류:
        // +--------------+----------------------------+------------------------------+---------------------------+
        // | 문법         | 가능한 것                  | 불가능한 것                  | 대표 용도                 |
        // +--------------+----------------------------+------------------------------+---------------------------+
        // | []           | 람다 내부 지역 변수만       | 외부 변수 접근 일체          | stateless 비교자          |
        // | [&]          | 외부 변수 전부 참조 읽기,쓰기| 람다가 스코프보다 오래 살기 | 이 코드처럼 단명 헬퍼     |
        // | [=]          | 외부 변수 전부 값 복사 읽기 | 복사본 수정(기본 const)      | 스레드,비동기 캡처        |
        // | [x]          | x 값 복사 읽기             | 다른 외부 변수,x 수정        | 특정 값 스냅샷            |
        // | [&x]         | x 참조 읽기,쓰기           | 다른 외부 변수 접근          | 하나만 수정, 나머지 격리  |
        // | [=, &x]      | 전체 복사 + x만 참조 수정  | -                            | 대부분 복사, x만 out-param|
        // | [&, x]       | 전체 참조 + x만 값 고정    | x 수정                       | 루프 인덱스 고정          |
        // | [this]       | 멤버 변수,함수 접근(포인터) | 객체 수명 보장               | 멤버 함수 내 람다         |
        // | [*this] C++17| 객체 전체 값 복사          | 복사 비용,원본 수정          | 비동기 시 수명 독립       |
        // +--------------+----------------------------+------------------------------+---------------------------+
        auto dirname = filename.substr(0, filename.find_last_of("/"));
        // 핵심 동기 (1) - LoadByAssimp 한 곳에서만 쓰는 헬퍼. 멤버 함수로 빼면 model.h 에
        //               노출돼 클래스 전역 가시화 + 헤더 재컴파일. 람다는 함수 스코프에 가둠.
        auto _lambdaLoadTexture = [&](aiMaterial *material, aiTextureType type) -> Texture *
        {
            if (material->GetTextureCount(type) <= 0)
                return nullptr;
            aiString filepath;
            material->GetTexture(type, 0, &filepath); //  type 인자 사용 (이전 코드는 DIFFUSE 하드코딩 버그)
            // 핵심 동기 (2) - dirname: 이 호출만의 지역값을 [&] 가 캡처. 멤버 승격도, 인자 반복 전달도 회피.
            const auto fullPath = fmt::format("{}/{}", dirname, filepath.C_Str());
            auto image = Image::Load(filepath.C_Str(), fullPath); //  새 API: (image_name, filepath)
            if (!image)
                return nullptr;
            auto tex = Texture::CreateTexture(image.get()); // TextureUPtr
            if (!tex)
                return nullptr;
            mTextures.push_back(std::move(tex)); // Model 이 lifetime owner
            return mTextures.back().get();       // 비소유 관찰자 반환
        };

        // 씬 안에 있는 머티리얼 만큼 반복
        for (uint32_t i = 0; i < scene->mNumMaterials; i++)
        {
            auto aiMat = scene->mMaterials[i];
            auto glMaterial = Material::Create();

            const auto diffuse = _lambdaLoadTexture(aiMat, aiTextureType_DIFFUSE);
            const auto specular = _lambdaLoadTexture(aiMat, aiTextureType_SPECULAR);

            // SP-Material-Split - Material 의 MaterialPropertyBlock (Unity 정통) 에 텍스처 슬롯 store.
            // 셰이더의 sampler uniform 이름 (lighting.fs 의 material.diffuse / material.specular) 과 1:1.
            if (diffuse)  glMaterial->Properties.Textures["material.diffuse"]  = { diffuse,  /*unit*/ 0 };
            if (specular) glMaterial->Properties.Textures["material.specular"] = { specular, /*unit*/ 1 };
            glMaterial->Properties.Floats["material.shininess"] = 32.0f;

            // 텍스처 없는 머티리얼(Albedo 전용 FBX 등)을 위해 aiColor_Diffuse 를 항상 추출.
            // phong.slang (구 phong_albedo.fs) 가 sampler2D 대신 material.albedo (vec3) 로 Phong diffuse 계산.
            aiColor4D aiDiffColor(0.8f, 0.8f, 0.8f, 1.0f);
            aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, aiDiffColor);
            glMaterial->Properties.Vec3s["material.albedo"] =
                vmath::vec3(aiDiffColor.r, aiDiffColor.g, aiDiffColor.b);

            mMaterials.push_back(std::move(glMaterial)); //  m_materials -> mMaterials
        }

        ProcessNode(scene->mRootNode, scene);
        return true;
    }

    // Node는 Tree 형태로 구성되어 있음.
    void Model::ProcessNode(aiNode *node, const aiScene *scene)
    {
        // 현 계층 Sibling 처리
        for (uint32_t i = 0; i < node->mNumMeshes; i++)
        {
            auto meshIndex = node->mMeshes[i];
            auto mesh = scene->mMeshes[meshIndex];
            ProcessMesh(mesh, scene);
        }

        for (uint32_t i = 0; i < node->mNumChildren; i++)
        {
            // 재귀적으로 호출중.
            ProcessNode(node->mChildren[i], scene);
        }
    }

    void Model::ProcessMesh(aiMesh *mesh, const aiScene *scene)
    {
        spdlog::info("process mesh: {}, #vert: {}, #face: {}",
                     mesh->mName.C_Str(), mesh->mNumVertices, mesh->mNumFaces);

        // aiMesh -> MeshData 변환은 Geometry::FromAssimp 책임 (절차적 빌더와 동일한 출력 형식).
        auto data = Geometry::FromAssimp(mesh);
        auto glMesh = Mesh::Create(data.vertices, data.indices, GL_TRIANGLES);

        Material *mat = nullptr;
        if (mesh->mMaterialIndex < mMaterials.size())
            mat = mMaterials[mesh->mMaterialIndex].get();
        mRenderUnit.push_back({std::move(glMesh), mat});
    }

}
