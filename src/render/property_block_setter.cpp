/**
 * @file property_block_setter.cpp
 * @brief MaterialPropertyBlock -> Program UniformCache 교집합 GL 송신 + 텍스처 바인딩.
 *
 * @details
 *  ### Set 알고리즘 (Cache outer 정통)
 *  Unity / Unreal 의 *셰이더 reflection 기반* 송신 정통:
 *  - **Outer**: UniformCache 의 active uniforms 순회 (셰이더 schema 가 진실의 원천).
 *  - **Inner**: entry.Type 으로 어떤 PropertyBlock typed map 에서 value 를 가져올지 dispatch.
 *  - Block 이 set 했지만 셰이더가 *안 받는* properties 는 자연 skip (cache 에 entry 없음).
 *
 *  ### PropertyBlockSetter 대비 정밀화
 *  - Material 통째 -> PropertyBlock 직접 (SRP — Pass.Kind / Program 참조는 Setter 무관)
 *  - Cache outer 만 — ApplyWithoutCache fallback 제거 (Observer cascade 가 cache 보장)
 */
#include "render/property_block_setter.h"
#include "render/device_context.h"
#include "material/material_property_block.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "program/uniform_cache.h"
#include "resource_registry/texture.h"
#include "GL/gl3w.h"

namespace SJH::PropertyBlockSetter
{
    void Set(DeviceContext& rc, const MaterialPropertyBlock& block, const Program& prog)
    {
        const UniformCache& cache = prog.GetUniformCache();

        // === Cache outer + Type dispatch + Block lookup inner ===
        // 셰이더 schema 가 진실의 원천. 각 active uniform 에 대해:
        //   1. Type 으로 PropertyBlock 의 어느 typed map 에서 가져올지 분기.
        //   2. find 로 lookup — 없으면 silent skip (Block 미설정).
        for (const auto& [name, entry] : cache.Entries())
        {
            switch (entry.Type)
            {
            case GL_FLOAT:
            {
                auto it = block.Floats.find(name);
                if (it != block.Floats.end())
                    Uniforms::SetFloat(prog, name.c_str(), it->second);
                break;
            }
            case GL_INT:
            {
                auto it = block.Ints.find(name);
                if (it != block.Ints.end())
                    Uniforms::SetInt(prog, name.c_str(), it->second);
                break;
            }
            case GL_FLOAT_VEC3:
            {
                auto it = block.Vec3s.find(name);
                if (it != block.Vec3s.end())
                    Uniforms::SetVec3(prog, name.c_str(), it->second);
                break;
            }
            case GL_FLOAT_VEC4:
            {
                auto it = block.Vec4s.find(name);
                if (it != block.Vec4s.end())
                    Uniforms::SetVec4(prog, name.c_str(), it->second);
                break;
            }
            case GL_FLOAT_MAT4:
            {
                auto it = block.Mat4s.find(name);
                if (it != block.Mat4s.end())
                    Uniforms::SetMat4(prog, name.c_str(), it->second);
                break;
            }
            case GL_SAMPLER_2D:
            case GL_SAMPLER_CUBE:
            {
                auto it = block.Textures.find(name);
                if (it == block.Textures.end()) break;
                const auto& binding = it->second;
                if (!binding.Tex) break;
                Uniforms::SetInt(prog, name.c_str(), binding.Unit);
                rc.BindTexture(static_cast<GLuint>(binding.Unit), binding.Tex->GetTextureID());
                break;
            }
            // 그 외 타입 — Block API 가 아직 지원하지 않음 (mat3 / sampler3D 등).
            // 추후 PropertyBlock 에 새 typed map 추가 시 case 보강.
            default:
                break;
            }
        }
    }
}
