/**
 * @file material.h
 * @brief 표면 머티리얼 — 디퓨즈/스페큘러 텍스처 관찰자 + sampler 유닛 + Phong shininess.
 *
 * @details
 *  ### 책임
 *  - 텍스처 *논리 이름* + *해석된 비소유 관찰자*(@c const @c Texture*) 보관.
 *  - uniform 전송 대상 셰이더 프로그램 참조 보관 (@c SetProgram 으로 주입).
 *  - @c Apply — 보유 프로그램에 sampler 슬롯/shininess uniform + 텍스처 바인딩 일괄 적용.
 *  - Phong shininess 지수 (specular highlight 집중도).
 *
 *  ### 모듈 위치 — 왜 `shader` 가 아니라 독립 `material` 모듈인가
 *  - @c Material 은 셰이더(@c SJH::shader)·프로그램(@c SJH::program) *위* 계층.
 *  - @c shader/material.h 에 두면 `shader ← program ← material` 이 `shader` 한 노드로 접혀
 *    `shader ↔ program` 순환이 됨. 독립 모듈로 분리해 단방향 DAG 유지.
 *
 *  ### 캡슐화 — 왜 getter/setter (직접 대입 차단)
 *  - 텍스처 *이름* 과 *해석된 관찰자* 는 *짝* — 이름을 함부로 바꾸면 관찰자가 stale.
 *    -> 이름 setter 가 *해석 캐시 무효화*(@c nullptr).
 *  - shininess 는 유효 범위 권장 @c [2, 256] — @c SetShininess 가 clamp.
 *
 *  ### 비-책임
 *  - ❌ 텍스처 *데이터 보유* — @c SJH::ResourceRegistry / @c Model 의 책임 (Material 은 관찰자만).
 *  - ❌ 셰이더 프로그램 *소유* — @c mProgram 은 비소유 관찰자, 외부가 수명 보장.
 */
#ifndef __SJH_MATERIAL_H__
#define __SJH_MATERIAL_H__

#include "common/common.h" // CLASS_PTR 매크로 + 스마트 포인터 별칭 alias
#include "GL/gl3w.h"
#include <string>

namespace SJH
{
    class Texture; // 비소유 관찰자 — 완전 정의는 material.cpp 만 필요 (Apply)
    class Program; // 비소유 관찰자 — 완전 정의는 material.cpp 만 필요 (Apply)

    CLASS_PTR(Material);
    /// @brief 텍스처 기반 Phong 머티리얼 (디퓨즈 + 스페큘러 맵 + shininess).
    class Material
    {
    public:
        static MaterialUPtr Create()
        {
            return MaterialUPtr(new Material());
        }
        // === 초기화 진입점 — 텍스처 이름 키 설정을 감싼다 ===

        /// @brief 디퓨즈 + 스페큘러 텍스처 이름 키를 한 번에 설정.
        void SetTextureNames(const std::string &diffuseName, const std::string &specularName)
        {
            SetDiffuseTextureName(diffuseName);
            SetSpecularTextureName(specularName);
        }

        /// @brief 디퓨즈 맵 이름 키 갱신 — *해석된 관찰자 무효화* (재 resolve 필요).
        /// @details 이름이 바뀌면 기존 @c mDiffuseTexture 포인터는 stale -> nullptr 로 되돌려 강제 재해석 유도.
        void SetDiffuseTextureName(const std::string &name)
        {
            mDiffuseTextureName = name;
            mDiffuseTexture = nullptr;
        }

        /// @brief 스페큘러 맵 이름 키 갱신 — *해석된 관찰자 무효화*.
        void SetSpecularTextureName(const std::string &name)
        {
            mSpecularTextureName = name;
            mSpecularTexture = nullptr;
        }

        /// @brief 이름으로부터 *해석된* 텍스처 관찰자 + sampler 유닛 설정.
        /// @details 텍스처 소유자(ResourceRegistry 또는 Model)가 Material 보다 오래 산다는 불변식 전제.
        void SetResolvedTextures(const Texture *diffuse, GLint diffuseUnit,
                                 const Texture *specular, GLint specularUnit)
        {
            mDiffuseTexture = diffuse;
            mDiffuseUnit = diffuseUnit;
            mSpecularTexture = specular;
            mSpecularUnit = specularUnit;
        }

        /// @brief 공유 템플릿을 per-use 가변 인스턴스로 복제 (Unreal MID / Unity renderer.material 패턴).
        MaterialUPtr Clone() const
        {
            return MaterialUPtr(new Material(*this));
        }

        /// @brief Phong shininess 지수 설정 — @c [2, 256] 으로 clamp (기본 @c 32).
        void SetShininess(float v)
        {
            mShininess = (v < 2.0f) ? 2.0f : (v > 256.0f ? 256.0f : v);
        }

        /// @brief uniform 을 전송할 셰이더 프로그램을 주입 (생성 후 셋업 시점 1회).
        /// @details 비소유 관찰자 — @p program 의 수명은 외부가 보장. @ref Apply 가 이 프로그램을 사용.
        void SetProgram(const Program *program) { mProgram = program; }

        /// @brief 보유 프로그램에 머티리얼 상태(텍스처 바인딩 + sampler/shininess uniform)를 일괄 적용.
        /// @details @c mProgram 이 nullptr 이면 no-op. 정의는 @c material.cpp —
        ///          @c Program / @c Texture 의 완전 정의가 필요해 헤더 inline 을 회피한다.
        void Apply() const;

        // === Getters — 모두 const, 읽기 전용 ===
        const std::string &GetDiffuseTextureName() const { return mDiffuseTextureName; }
        const std::string &GetSpecularTextureName() const { return mSpecularTextureName; }
        const Texture *GetDiffuseTexture() const { return mDiffuseTexture; }
        const Texture *GetSpecularTexture() const { return mSpecularTexture; }
        GLint GetDiffuseUnit() const { return mDiffuseUnit; }
        GLint GetSpecularUnit() const { return mSpecularUnit; }
        float GetShininess() const { return mShininess; }

        /// @brief 디퓨즈 텍스처가 해석된 상태인지 (nullptr 이면 미해석).
        bool IsResolved() const { return mDiffuseTexture != nullptr; }

    private:
        Material() = default;
        std::string mDiffuseTextureName;          ///< 디퓨즈 맵 이름 키 — @c ResourceRegistry::FindTexture 조회
        std::string mSpecularTextureName;         ///< 스페큘러 맵 이름 키 — 비어 있으면 셰이더 측 default 가정
        const Texture *mDiffuseTexture{nullptr};  ///< 이름으로부터 해석된 비소유 텍스처 관찰자
        const Texture *mSpecularTexture{nullptr}; ///< 스페큘러 맵 관찰자
        const Program *mProgram{nullptr};         ///< uniform 전송 대상 — 비소유 관찰자. @ref SetProgram 으로 주입.
        GLint mDiffuseUnit{0};                    ///< 디퓨즈 sampler2D 에 넣을 텍스처 이미지 유닛 번호
        GLint mSpecularUnit{1};                   ///< 스페큘러 sampler2D 의 유닛 번호
        float mShininess{32.0f};                  ///< Phong shininess — 셰이더 uniform `material.shininess`. 권장 [2,256]
    };
}

#endif // __SJH_MATERIAL_H__
