/**
 * @file material_uniforms.cpp
 * @brief Material properties bag setter family 구현 - bag 에 store 만 (GL 호출 없음).
 *
 * @details
 *  ### 책임
 *  - `SJH::Uniforms::Set*` 7종 함수의 구현 - 각각 `mat.Properties.<TypedMap>[name] = v` 한 줄.
 *  - `SetTexture` - `TextureBinding{tex, unit}` 을 `Textures` map 에 저장.
 *
 *  ### 비-책임
 *  - [X] GL `glUniform*` 호출 없음 - draw 시점 `PropertyBlockSetter::Set` 전담.
 *  - [X] Program uniform 존재 검증 없음 - `Material::SetProgram` 의 EagerBuild 가 사전 prune.
 *
 * @note 구현이 단순(1줄 대입)하므로 인라인화 가능하지만,
 *       헤더-구현 분리로 `Material` / `MaterialPropertyBlock` 의 include 를 .cpp 에 격리한다.
 */
#include "material/material_uniforms.h"
#include "material/material.h"

namespace SJH::Uniforms
{
    /// @brief float 값을 @c mat.Properties.Floats 에 store.
    void SetFloat(Material& mat, const char* name, float v)
    {
        mat.Properties.Floats[name] = v;
    }

    /// @brief int 값을 @c mat.Properties.Ints 에 store. GL_BOOL uniform 도 이 경로 사용.
    void SetInt(Material& mat, const char* name, int v)
    {
        mat.Properties.Ints[name] = v;
    }

    /// @brief vec2 값을 @c mat.Properties.Vec2s 에 store.
    void SetVec2(Material& mat, const char* name, const vmath::vec2& v)
    {
        mat.Properties.Vec2s[name] = v;
    }

    /// @brief vec3 값을 @c mat.Properties.Vec3s 에 store.
    void SetVec3(Material& mat, const char* name, const vmath::vec3& v)
    {
        mat.Properties.Vec3s[name] = v;
    }

    /// @brief vec4 값을 @c mat.Properties.Vec4s 에 store.
    void SetVec4(Material& mat, const char* name, const vmath::vec4& v)
    {
        mat.Properties.Vec4s[name] = v;
    }

    /// @brief mat4 값을 @c mat.Properties.Mat4s 에 store.
    void SetMat4(Material& mat, const char* name, const vmath::mat4& v)
    {
        mat.Properties.Mat4s[name] = v;
    }

    /// @brief Texture 비소유 포인터 + sampler unit 을 @c mat.Properties.Textures 에 store.
    void SetTexture(Material& mat, const char* name, const Texture* tex, GLint unit)
    {
        mat.Properties.Textures[name] = MaterialPropertyBlock::TextureBinding{ tex, unit };
    }

}
