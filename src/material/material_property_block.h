/**
 * @file material_property_block.h
 * @brief Material 의 *셰이더 무관 properties bag* — Unity `MaterialPropertyBlock` 정통.
 *
 * @details
 *  ### 정통 매핑
 *  Unity `UnityEngine.MaterialPropertyBlock`:
 *  - 데이터 컨테이너 (값 보유) — *applier 아님*
 *  - `block.SetFloat(name, value)` / `SetTexture(name, tex)` 로 typed store
 *  - `Renderer.SetPropertyBlock(block)` 으로 *renderer 별 override* 전달
 *
 *  본 클래스는 Unity 정통의 *typed map 6 묶음 (Float/Int/Vec3/Vec4/Mat4/Texture)* 만 분리.
 *  *적용* (uniform 송신 + 텍스처 바인딩) 은 별도 `PropertyBlockSetter::Set` 책임 (Applier 패턴).
 *
 *  ### 분리 동기 (Material 의 책임 분할)
 *  - **`Material`** = Program 참조 + Pass.Kind + `MaterialPropertyBlock` 1 개 보유 (큰 분류 + 메타)
 *  - **`MaterialPropertyBlock`** = *셰이더 무관 typed properties* (이 클래스)
 *  - **`PropertyBlockSetter`** = *block + Program -> GL 송신* (별도 Applier)
 *
 *  세 책임 분리로 ddd Separation of Concerns + 진실의 원천 단일화 만족.
 */
#ifndef __SJH_MATERIAL_PROPERTY_BLOCK_H__
#define __SJH_MATERIAL_PROPERTY_BLOCK_H__

#include "GL/gl3w.h"
#include <string>
#include <unordered_map>
#include <vmath.h>

namespace SJH
{
    class Texture; // 비소유 관찰자.

    /// @brief Unity MaterialPropertyBlock 정통 — typed properties bag.
    /// @details 6 typed map + TextureBinding nested struct.
    ///          type erasure 대신 *typed 분리* — Unity URP/HDRP / Cocos Material 정통.
    struct MaterialPropertyBlock
    {
        /// @brief 텍스처 바인딩 — sampler unit 과 비소유 텍스처 관찰자.
        struct TextureBinding
        {
            const Texture *Tex = nullptr;
            GLint Unit = 0;
        };

        // 타입별 분리 map — type erasure 비용 회피.
        std::unordered_map<std::string, float> Floats;
        std::unordered_map<std::string, int> Ints;
        std::unordered_map<std::string, vmath::vec3> Vec3s;
        std::unordered_map<std::string, vmath::vec4> Vec4s;
        std::unordered_map<std::string, vmath::mat4> Mat4s;
        std::unordered_map<std::string, TextureBinding> Textures;
    };
} // namespace SJH

#endif // __SJH_MATERIAL_PROPERTY_BLOCK_H__
