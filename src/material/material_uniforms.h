/**
 * @file material_uniforms.h
 * @brief Material 의 properties bag 에 typed value store — Unity `material.SetFloat / SetVector / SetTexture` 정통.
 *
 * @details
 *  ### 디자인 동기 (SP6)
 *  - SP1~SP5: `SJH::Uniforms::Set*(const Program&, name, value)` — 즉시 GL 호출.
 *  - SP6: 책임 이전 — Material 이 properties bag 보유. setter 는 *bag 에 store* 만, GL 호출은 Apply 시점.
 *  - 자유 함수 family 유지 — SP1~SP5 의 `Uniforms` 패턴과 일관. Material 인자 첫 자리.
 *
 *  ### Unity 매핑
 *  | Unity                                | SJH (SP6)                                 |
 *  |--------------------------------------|-------------------------------------------|
 *  | `material.SetFloat("_X", v)`         | `Uniforms::SetFloat(mat, "_X", v)`        |
 *  | `material.SetVector("_X", v4)`       | `Uniforms::SetVec4 (mat, "_X", v4)`       |
 *  | `material.SetTexture("_MainTex", t)` | `Uniforms::SetTexture(mat, "_X", t, unit)` |
 *
 *  ### 책임 분리
 *  - 본 자유함수: properties bag 에 store (GL 호출 없음).
 *  - `MaterialApplier::Apply`: bag -> UniformCache 교집합 -> 일괄 GL 송신.
 *  - `SJH::Uniforms::Set*(const Program&, ...)`: light 송신 등 Material 우회 직접 호출용 (program_uniforms.h 유지).
 */
#ifndef __SJH_MATERIAL_UNIFORMS_H__
#define __SJH_MATERIAL_UNIFORMS_H__

#include "GL/gl3w.h"
#include <vmath.h>

namespace SJH
{
    class Material;
    class Texture;

    namespace Uniforms
    {
        // === Material properties bag setter family ============================
        // 모두 bag 에 store 만 — 셰이더 무관. Apply 시점에 UniformCache 교집합 송신.

        void SetFloat  (Material& mat, const char* name, float v);
        void SetInt    (Material& mat, const char* name, int v);
        void SetVec3   (Material& mat, const char* name, const vmath::vec3& v);
        void SetVec4   (Material& mat, const char* name, const vmath::vec4& v);
        void SetMat4   (Material& mat, const char* name, const vmath::mat4& v);

        /// @brief Texture binding — Texture* 비소유 관찰자 + sampler unit.
        /// @details Apply 시점에 (a) `Uniforms::SetInt(prog, name, unit)` 으로 sampler slot 송신 +
        ///          (b) `RenderContext::BindTexture(unit, texID)` 로 텍스처 바인딩.
        void SetTexture(Material& mat, const char* name, const Texture* tex, GLint unit);
    }
}

#endif // __SJH_MATERIAL_UNIFORMS_H__
