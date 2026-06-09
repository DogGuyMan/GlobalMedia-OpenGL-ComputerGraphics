/**
 * @file property_block_setter.cpp
 * @brief PropertyBlockSetter::Set 구현 - Cache outer + Type dispatch + Block lookup inner.
 *
 * @details
 *  ### 알고리즘 (Unity MaterialPropertyBlock reflection 정통)
 *  - **Outer**: @c prog.GetUniformCache().Entries() 순회 - 셰이더 schema 가 진실의 원천.
 *    Block 이 설정했지만 셰이더가 *안 받는* properties 는 cache entry 없음 -> 자연 skip.
 *  - **Type dispatch**: @c entry.Type(@c GL_FLOAT / @c GL_INT / @c GL_BOOL 등) 으로
 *    @c block 의 typed map (@c Floats / @c Ints / @c Vec3s / @c Textures 등) 선택.
 *  - **Inner lookup**: @c std::unordered_map::find - 없으면 silent skip (Block 미설정).
 *
 *  ### GL_BOOL 처리 - 삭제/이동 금지
 *  GLSL @c bool uniform 은 GL 내부적으로 @c GL_BOOL 타입으로 조회된다.
 *  그러나 값은 @c glUniform1i(0/1) 로 송신해야 하며, @c MaterialPropertyBlock 에는
 *  @c SetInt(name, 0/1) 로 @c Ints 맵에 저장된다.
 *  -> @c GL_BOOL case 를 @c GL_INT 와 fall-through 처리하여 @c Ints 맵에서 조회.
 *  이 fall-through 를 제거하면 @c uEnableHit / @c uEnableDissolve 등 sprite FX bool uniform 이
 *  영영 미업로드되어 FX 가 무반응 상태가 된다 (2026년 확인 버그, PropertyBlockSetter GL_BOOL 갭).
 *
 *  ### 이력 (PropertyBlockSetter 대비 정밀화)
 *  - 시그니처: `Apply(rc, Material)` -> `Set(rc, MaterialPropertyBlock, Program)` (SRP).
 *  - Cache outer 만 - @c ApplyWithoutCache fallback 제거 (Observer cascade 가 cache 보장).
 *  - 지원 타입: @c GL_FLOAT / @c GL_FLOAT_VEC2 / @c GL_BOOL(fall-through) / @c GL_INT /
 *    @c GL_FLOAT_VEC3 / @c GL_FLOAT_VEC4 / @c GL_FLOAT_MAT4 / @c GL_SAMPLER_2D / @c GL_SAMPLER_CUBE.
 *    미지원 타입(@c mat3 / @c sampler3D 등) 은 @c default: break - 추후 Block API 확장 시 case 보강.
 */
#include "render/property_block_setter.h"
#include "GL/gl3w.h"
#include "material/material_property_block.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "program/uniform_cache.h"
#include "render/device_context.h"
#include "resource_registry/texture.h"

namespace SJH::PropertyBlockSetter
{
	void Set(DeviceContext &rc, const MaterialPropertyBlock &block, const Program &prog)
	{
		const UniformCache &cache = prog.GetUniformCache();

		// === Cache outer + Type dispatch + Block lookup inner ===
		// 셰이더 schema 가 진실의 원천. 각 active uniform 에 대해:
		//   1. Type 으로 PropertyBlock 의 어느 typed map 에서 가져올지 분기.
		//   2. find 로 lookup - 없으면 silent skip (Block 미설정).
		for (const auto &[name, entry] : cache.Entries())
		{
			switch (entry.Type)
			{
			case GL_FLOAT: {
				auto it = block.Floats.find(name);
				if (it != block.Floats.end())
					Uniforms::SetFloat(prog, name.c_str(), it->second);
				break;
			}
			case GL_FLOAT_VEC2: {
				auto it = block.Vec2s.find(name);
				if (it != block.Vec2s.end())
					Uniforms::SetVec2(prog, name.c_str(), it->second);
				break;
			}
			case GL_BOOL: // GLSL bool 은 glUniform1i(0/1) 로 set - Ints 맵에서 가져온다 (Material 이 SetInt 로 저장).
			case GL_INT: {
				auto it = block.Ints.find(name);
				if (it != block.Ints.end())
					Uniforms::SetInt(prog, name.c_str(), it->second);
				break;
			}
			case GL_FLOAT_VEC3: {
				auto it = block.Vec3s.find(name);
				if (it != block.Vec3s.end())
					Uniforms::SetVec3(prog, name.c_str(), it->second);
				break;
			}
			case GL_FLOAT_VEC4: {
				auto it = block.Vec4s.find(name);
				if (it != block.Vec4s.end())
					Uniforms::SetVec4(prog, name.c_str(), it->second);
				break;
			}
			case GL_FLOAT_MAT4: {
				auto it = block.Mat4s.find(name);
				if (it != block.Mat4s.end())
					Uniforms::SetMat4(prog, name.c_str(), it->second);
				break;
			}
			case GL_SAMPLER_2D:
			case GL_SAMPLER_CUBE: {
				auto it = block.Textures.find(name);
				if (it == block.Textures.end())
					break;
				const auto &binding = it->second;
				if (!binding.Tex)
					break;
				Uniforms::SetInt(prog, name.c_str(), binding.Unit);
				rc.BindTexture(static_cast<GLuint>(binding.Unit), binding.Tex->GetTextureID());
				break;
			}
			// 그 외 타입 - Block API 가 아직 지원하지 않음 (mat3 / sampler3D 등).
			// 추후 PropertyBlock 에 새 typed map 추가 시 case 보강.
			default:
				break;
			}
		}
	}
} // namespace SJH::PropertyBlockSetter
