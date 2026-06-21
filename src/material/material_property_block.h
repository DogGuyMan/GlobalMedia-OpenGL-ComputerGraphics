/**
 * @file material_property_block.h
 * @brief Material 의 *셰이더 무관 properties bag* - Unity `MaterialPropertyBlock` 정통.
 *
 * @details
 *  ### 책임
 *  - Float / Int / Vec2 / Vec3 / Vec4 / Mat4 / Texture 7종 typed map 으로 uniform 값 보관.
 *  - `TextureBinding` nested struct 로 sampler unit 과 비소유 텍스처 포인터를 묶어 보관.
 *
 *  ### 비-책임
 *  - [X] GL uniform 송신 - draw 시점 `MeshPassProcessor` (값=UBO 멤버, sampler=BindSamplers) 가 책임.
 *  - [X] 텍스처 GPU 바인딩 - `DeviceContext::BindTexture` 가 책임.
 *  - [X] Program schema 검증 - `Material::SetProgram` 의 EagerBuild 가 책임.
 *
 *  ### 정통 매핑
 *  Unity `UnityEngine.MaterialPropertyBlock`:
 *  - 데이터 컨테이너 (값 보유) - *applier 아님*
 *  - `block.SetFloat(name, value)` / `SetTexture(name, tex)` 로 typed store
 *  - `Renderer.SetPropertyBlock(block)` 으로 *renderer 별 override* 전달
 *
 *  본 클래스는 Unity 정통의 *typed map 7 묶음 (Float/Int/Vec2/Vec3/Vec4/Mat4/Texture)* 만 분리.
 *  *적용* (uniform 송신 + 텍스처 바인딩) 은 draw 시점 `MeshPassProcessor` 책임 (값=UBO 멤버, sampler=BindSamplers).
 *
 *  ### 분리 동기 (Material 의 책임 분할)
 *  - **`Material`** = Program 참조 + Pass.Kind + `MaterialPropertyBlock` 1 개 보유 (큰 분류 + 메타)
 *  - **`MaterialPropertyBlock`** = *셰이더 무관 typed properties* (이 클래스)
 *  - **`MeshPassProcessor`** = *block + Program -> GL 송신* (값=UBO 멤버 업로드, sampler=BindSamplers)
 *
 *  세 책임 분리로 Separation of Concerns + 진실의 원천 단일화 만족.
 *
 * @note type erasure(`std::any` / `std::variant`) 대신 *typed 분리* - Unity URP/HDRP / Cocos Material 정통.
 *       iterator 비용은 map size (보통 3~10 항목) 대비 무시 가능.
 */
#ifndef __SJH_MATERIAL_PROPERTY_BLOCK_H__
#define __SJH_MATERIAL_PROPERTY_BLOCK_H__

#include "GL/gl3w.h"
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace SJH
{
    class Texture; // 비소유 관찰자.

    /**
     * @brief Unity MaterialPropertyBlock 정통 - typed properties bag.
     * @details 7 typed map (`Floats` / `Ints` / `Vec2s` / `Vec3s` / `Vec4s` / `Mat4s` / `Textures`) +
     *          `TextureBinding` nested struct.
     *          type erasure 대신 *typed 분리* - Unity URP/HDRP / Cocos Material 정통.
     */
    struct MaterialPropertyBlock
    {
        /**
         * @brief 텍스처 바인딩 - sampler unit 과 비소유 텍스처 관찰자.
         * @details Apply 시점에:
         *          (a) `Uniforms::SetInt(prog, name, Unit)` 으로 sampler slot 번호 송신,
         *          (b) `DeviceContext::BindTexture(Unit, Tex->ID())` 로 실제 텍스처 바인딩.
         */
        struct TextureBinding
        {
            const Texture *Tex = nullptr; ///< 비소유 텍스처 포인터 - owner 는 ResourceRegistry.
            GLint Unit = 0;               ///< GL 텍스처 unit 번호 (GL_TEXTURE0 + Unit).
        };

        /// @brief float uniform 값 map. 키 = 셰이더 uniform 이름.
        std::unordered_map<std::string, float> Floats;

        /// @brief int / bool uniform 값 map. 키 = 셰이더 uniform 이름.
        /// @details GL_BOOL uniform 도 이 map 에 저장 (SetInt + GL_BOOL fall-through 컨벤션).
        std::unordered_map<std::string, int> Ints;

        /// @brief vec2 uniform 값 map. 키 = 셰이더 uniform 이름.
        std::unordered_map<std::string, glm::vec2> Vec2s;

        /// @brief vec3 uniform 값 map. 키 = 셰이더 uniform 이름.
        std::unordered_map<std::string, glm::vec3> Vec3s;

        /// @brief vec4 uniform 값 map. 키 = 셰이더 uniform 이름.
        std::unordered_map<std::string, glm::vec4> Vec4s;

        /// @brief mat4 uniform 값 map. 키 = 셰이더 uniform 이름.
        std::unordered_map<std::string, glm::mat4> Mat4s;

        /// @brief 텍스처 바인딩 map. 키 = 셰이더 sampler uniform 이름 (예: `"uAlbedo"`).
        std::unordered_map<std::string, TextureBinding> Textures;
    };
} // namespace SJH

#endif // __SJH_MATERIAL_PROPERTY_BLOCK_H__
