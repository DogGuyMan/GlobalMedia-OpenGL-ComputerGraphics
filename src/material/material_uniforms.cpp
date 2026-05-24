/**
 * @file material_uniforms.cpp
 * @brief Material properties bag setter family — bag 에 store 만 (GL 호출 없음).
 */
#include "material/material_uniforms.h"
#include "material/material.h"

namespace SJH::Uniforms
{
    void SetFloat(Material& mat, const char* name, float v)
    {
        mat.Properties.Floats[name] = v;
    }

    void SetInt(Material& mat, const char* name, int v)
    {
        mat.Properties.Ints[name] = v;
    }

    void SetVec2(Material& mat, const char* name, const vmath::vec2& v)
    {
        mat.Properties.Vec2s[name] = v;
    }

    void SetVec3(Material& mat, const char* name, const vmath::vec3& v)
    {
        mat.Properties.Vec3s[name] = v;
    }

    void SetVec4(Material& mat, const char* name, const vmath::vec4& v)
    {
        mat.Properties.Vec4s[name] = v;
    }

    void SetMat4(Material& mat, const char* name, const vmath::mat4& v)
    {
        mat.Properties.Mat4s[name] = v;
    }

    void SetTexture(Material& mat, const char* name, const Texture* tex, GLint unit)
    {
        mat.Properties.Textures[name] = MaterialPropertyBlock::TextureBinding{ tex, unit };
    }

}
