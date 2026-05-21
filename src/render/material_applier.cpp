/**
 * @file material_applier.cpp
 * @brief Material properties bag -> UniformCache 교집합 GL 송신 + 텍스처 바인딩 일괄.
 *
 * @details
 *  ### Apply 알고리즘 (SP6 T8 — Cache outer 정통)
 *  Unity / Unreal 의 *셰이더 reflection 기반* 송신 정통:
 *  - **Outer**: UniformCache 의 active uniforms 순회 (셰이더 schema 가 진실의 원천).
 *  - **Inner**: entry.Type 으로 어떤 Material typed map 에서 value 를 가져올지 dispatch.
 *  - Material 이 set 했지만 셰이더가 *안 받는* properties 는 자연 skip (cache 에 entry 없음).
 *
 *  ### 이전 (6 outer loop) 대비 효과
 *  - "셰이더에 정의된 uniform 만 송신 시도" — 무관한 properties 의 lookup 비용 자체가 0.
 *  - Apply 의 의도 *(셰이더 schema 매칭)* 가 코드 구조에서 즉시 보임.
 *  - 자료구조 (Floats/Ints/Vec3s/...) 는 typed 분리 유지 — Unity MaterialPropertyBlock 정통.
 */
#include "render/material_applier.h"
#include "render/render_context.h"
#include "material/material.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "program/uniform_cache.h"
#include "resource_registry/texture.h"
#include "GL/gl3w.h"   // GLuint / GL_FLOAT / GL_FLOAT_VEC3 / GL_SAMPLER_2D 등

namespace SJH::MaterialApplier
{
    namespace
    {
        /// @brief Cache 없는 fallback — SetProgram 이 비정상 경로로 호출됐을 때만 도달.
        /// @details 정상 흐름은 `cache != nullptr`. 안전 차원에서 옛 6-loop 보존.
        void ApplyWithoutCache(RenderContext& rc, const Material& mat, const Program* prog)
        {
            for (const auto& [name, value] : mat.Floats)
                Uniforms::SetFloat(*prog, name.c_str(), value);
            for (const auto& [name, value] : mat.Ints)
                Uniforms::SetInt(*prog, name.c_str(), value);
            for (const auto& [name, value] : mat.Vec3s)
                Uniforms::SetVec3(*prog, name.c_str(), value);
            for (const auto& [name, value] : mat.Vec4s)
                Uniforms::SetVec4(*prog, name.c_str(), value);
            for (const auto& [name, value] : mat.Mat4s)
                Uniforms::SetMat4(*prog, name.c_str(), value);
            for (const auto& [name, binding] : mat.Textures)
            {
                if (!binding.Tex) continue;
                Uniforms::SetInt(*prog, name.c_str(), binding.Unit);
                rc.BindTexture(static_cast<GLuint>(binding.Unit), binding.Tex->GetTextureID());
            }
        }
    }

    void Apply(RenderContext& rc, const Material& mat)
    {
        const Program* prog = mat.GetProgram();
        if (!prog) return;   // Observer cascade 후 안전 — Program 해제 시 mat 가 자동 무효.

        const UniformCache* cache = mat.GetCache();
        if (!cache)
        {
            // 비정상 경로 — 안전 fallback.
            ApplyWithoutCache(rc, mat, prog);
            return;
        }

        // === Cache outer + Type dispatch + Material lookup inner ===
        // 셰이더 schema 가 진실의 원천. 각 active uniform 에 대해:
        //   1. Type 으로 Material 의 어느 typed map 에서 가져올지 분기.
        //   2. find 로 lookup — 없으면 silent skip (Material 미설정).
        for (const auto& [name, entry] : cache->Entries())
        {
            switch (entry.Type)
            {
            case GL_FLOAT:
            {
                auto it = mat.Floats.find(name);
                if (it != mat.Floats.end())
                    Uniforms::SetFloat(*prog, name.c_str(), it->second);
                break;
            }
            case GL_INT:
            {
                auto it = mat.Ints.find(name);
                if (it != mat.Ints.end())
                    Uniforms::SetInt(*prog, name.c_str(), it->second);
                break;
            }
            case GL_FLOAT_VEC3:
            {
                auto it = mat.Vec3s.find(name);
                if (it != mat.Vec3s.end())
                    Uniforms::SetVec3(*prog, name.c_str(), it->second);
                break;
            }
            case GL_FLOAT_VEC4:
            {
                auto it = mat.Vec4s.find(name);
                if (it != mat.Vec4s.end())
                    Uniforms::SetVec4(*prog, name.c_str(), it->second);
                break;
            }
            case GL_FLOAT_MAT4:
            {
                auto it = mat.Mat4s.find(name);
                if (it != mat.Mat4s.end())
                    Uniforms::SetMat4(*prog, name.c_str(), it->second);
                break;
            }
            case GL_SAMPLER_2D:
            case GL_SAMPLER_CUBE:
            {
                auto it = mat.Textures.find(name);
                if (it == mat.Textures.end()) break;
                const auto& binding = it->second;
                if (!binding.Tex) break;
                Uniforms::SetInt(*prog, name.c_str(), binding.Unit);
                rc.BindTexture(static_cast<GLuint>(binding.Unit), binding.Tex->GetTextureID());
                break;
            }
            // 그 외 타입 — Material API 가 아직 지원하지 않음 (mat3 / sampler3D 등).
            // 추후 Properties bag 에 새 typed map 추가 시 case 보강.
            default:
                break;
            }
        }
    }
}
