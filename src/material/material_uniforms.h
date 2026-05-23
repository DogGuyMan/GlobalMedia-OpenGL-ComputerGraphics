/**
 * @file material_uniforms.h
 * @brief Material properties bag 의 typed store family — Unity `material.SetXxx` 정통 (EngineAPI §4.3 / §4.5).
 *
 * @details
 *  ### 핵심 — *store-only* semantics
 *  본 family 는 GL 호출을 **하지 않는다**. Material 의 typed map (Floats/Ints/Vec3s/...) 에 *값만 기록*.
 *  실제 `glUniform*` 송신은 draw 시점에 `PropertyBlockSetter::Set` 가
 *  `UniformCache` 교집합 (셰이더가 실제로 받는 uniform 만) 으로 일괄 수행.
 *
 *  ### 2-layer uniform 송신 (§4.5) — 본 family 의 자리
 *  | family | 인자 | 동작 | 용도 |
 *  |---|---|---|---|
 *  | **`Set*(Material&, ...)`** *(본 헤더)* | `Material&` | **store-only** | 사용자 컨텐츠 (color/texture/shininess) |
 *  | `Set*(const Program&, ...)` *(program_uniforms.h)* | `const Program&` | 즉시 GL 호출 | 광원 / uModel / uView / Apply 의 내부 |
 *
 *  ### OCP 함의
 *  셰이더 schema 가 바뀌어도 본 family 호출은 *그대로*. 셰이더에 *추가* 된 uniform 은
 *  setup 코드에서 `SetFloat / SetTexture` 한 줄 추가, *제거* 된 uniform 은 bag 에 남아도 silent skip.
 *  Material 클래스는 셰이더 schema 를 모른다 — 진실의 원천은 `Program::UniformCache`.
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
        ///          (b) `DeviceContext::BindTexture(unit, texID)` 로 텍스처 바인딩.
        void SetTexture(Material& mat, const char* name, const Texture* tex, GLint unit);
    }
}

#endif // __SJH_MATERIAL_UNIFORMS_H__
