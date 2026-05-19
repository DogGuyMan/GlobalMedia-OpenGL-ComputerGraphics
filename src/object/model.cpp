#include "model.h"
#include "resource_registry/texture.h"
#include "material/material.h"
#include <spdlog/spdlog.h>

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

        // [&] : 외부 변수 전부를 참조로 캡처. dirname·mTextures 접근에 사용.
        //        수명은 LoadByAssimp 스코프 안으로 한정 -> 댕글링 위험 없음.
        //
        // 람다 캡처 리스트 종류:
        // ┌──────────────┬────────────────────────────┬──────────────────────────────┬───────────────────────────┐
        // │ 문법         │ 가능한 것                  │ 불가능한 것                  │ 대표 용도                 │
        // ├──────────────┼────────────────────────────┼──────────────────────────────┼───────────────────────────┤
        // │ []           │ 람다 내부 지역 변수만       │ 외부 변수 접근 일체          │ stateless 비교자          │
        // │ [&]          │ 외부 변수 전부 참조 읽기·쓰기│ 람다가 스코프보다 오래 살기 │ 이 코드처럼 단명 헬퍼     │
        // │ [=]          │ 외부 변수 전부 값 복사 읽기 │ 복사본 수정(기본 const)      │ 스레드·비동기 캡처        │
        // │ [x]          │ x 값 복사 읽기             │ 다른 외부 변수·x 수정        │ 특정 값 스냅샷            │
        // │ [&x]         │ x 참조 읽기·쓰기           │ 다른 외부 변수 접근          │ 하나만 수정, 나머지 격리  │
        // │ [=, &x]      │ 전체 복사 + x만 참조 수정  │ —                            │ 대부분 복사, x만 out-param│
        // │ [&, x]       │ 전체 참조 + x만 값 고정    │ x 수정                       │ 루프 인덱스 고정          │
        // │ [this]       │ 멤버 변수·함수 접근(포인터) │ 객체 수명 보장               │ 멤버 함수 내 람다         │
        // │ [*this] C++17│ 객체 전체 값 복사          │ 복사 비용·원본 수정          │ 비동기 시 수명 독립       │
        // └──────────────┴────────────────────────────┴──────────────────────────────┴───────────────────────────┘
        auto dirname = filename.substr(0, filename.find_last_of("/"));
        // 핵심 동기 ① — LoadByAssimp 한 곳에서만 쓰는 헬퍼. 멤버 함수로 빼면 model.h 에
        //               노출돼 클래스 전역 가시화 + 헤더 재컴파일. 람다는 함수 스코프에 가둠.
        auto _lambdaLoadTexture = [&](aiMaterial *material, aiTextureType type) -> Texture *
        {
            if (material->GetTextureCount(type) <= 0)
                return nullptr;
            aiString filepath;
            material->GetTexture(type, 0, &filepath); //  type 인자 사용 (이전 코드는 DIFFUSE 하드코딩 버그)
            // 핵심 동기 ② — dirname: 이 호출만의 지역값을 [&] 가 캡처. 멤버 승격도, 인자 반복 전달도 회피.
            const auto fullPath = fmt::format("{}/{}", dirname, filepath.C_Str());
            auto image = Image::Load(filepath.C_Str(), fullPath); //  새 API: (image_name, filepath)
            if (!image)
                return nullptr;
            auto tex = Texture::CreateTexture(image.get()); // TextureUPtr
            if (!tex)
                return nullptr;
            // 핵심 동기 ③ — [&] 가 this 도 캡처 -> 멤버 mTextures 직접 접근.
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

            // 새 Material API — 이름은 비워두고 *해석된 관찰자 + sampler 유닛* 만 설정.
            glMaterial->SetResolvedTextures(
                /*diffuse */ diffuse, /*diffuseUnit*/ 0,
                /*specular*/ specular, /*specularUnit*/ 1);

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

        // 1. Vertex 정보 수집
        std::vector<Vertex> vertices;
        vertices.resize(mesh->mNumVertices);
        for (uint32_t i = 0; i < mesh->mNumVertices; i++)
        {
            auto &v = vertices[i];
            v.position = vmath::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            v.normal = vmath::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            v.texCoord = vmath::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        }

        // 2. Indices 정보 수집
        std::vector<uint32_t> indices;
        indices.resize(mesh->mNumFaces * 3);
        for (uint32_t i = 0; i < mesh->mNumFaces; i++)
        {
            indices[3 * i] = mesh->mFaces[i].mIndices[0];
            indices[3 * i + 1] = mesh->mFaces[i].mIndices[1];
            indices[3 * i + 2] = mesh->mFaces[i].mIndices[2];
        }

        // 우리가 만들었던 Mesh 코드 호출
        auto glMesh = Mesh::Create(vertices, indices, GL_TRIANGLES);
        Material *mat = nullptr;
        if (mesh->mMaterialIndex < mMaterials.size()) //  m_materials -> mMaterials, unsigned 범위 안전
            mat = mMaterials[mesh->mMaterialIndex].get();
        mRenderUnit.push_back({std::move(glMesh), mat});
    }

    void Model::Draw() const
    {
        for (auto &unit : mRenderUnit)
        {
            // 머티리얼이 uniform/텍스처를 자기 프로그램에 적용 -> 그 다음 메시 기하 드로우.
            if (unit.material)
                unit.material->Apply();
            unit.mesh->Draw();
        }
    }
}
