/**
 * @file draw_ops.h
 * @brief draw 시점 공유 헬퍼 - Material UBO 멤버 업로드 + sampler 바인딩. 잎(MeshRenderer)과 ScreenQuad 공용.
 * @details 구 mesh_pass_processor.cpp 익명 namespace 헬퍼를 공유 위치로 추출(중복 제거).
 */
#ifndef __SJH_DRAW_OPS_H__
#define __SJH_DRAW_OPS_H__

#include "GL/gl3w.h"
#include "program/program.h"
#include "material/material_property_block.h"
#include "render/device_context.h"
#include "texture/texture.h"
#include <glm/glm.hpp>

namespace SJH
{
	/// @brief MaterialBlock UBO 멤버를 Properties typed map 에서 author 이름 매칭 업로드(비-UBO 멤버 자동 skip).
	inline void UploadMaterialUboMembers(const Program &p, const MaterialPropertyBlock &m)
	{
		for (auto &kv : m.Floats) p.UpdateUniformMember(kv.first, &kv.second, sizeof(float));
		for (auto &kv : m.Ints)   p.UpdateUniformMember(kv.first, &kv.second, sizeof(int));
		for (auto &kv : m.Vec2s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::vec2));
		for (auto &kv : m.Vec3s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::vec3));
		for (auto &kv : m.Vec4s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::vec4));
		for (auto &kv : m.Mat4s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::mat4));
	}

	/// @brief material 의 sampler(texture)를 program 에 바인딩(셰이더 미선언 sampler 는 location<0 skip).
	inline void BindSamplers(DeviceContext &rc, const MaterialPropertyBlock &b, const Program &p)
	{
		for (auto &kv : b.Textures)
		{
			if (!kv.second.Tex) continue;
			const GLint loc = p.GetLocation(kv.first.c_str());
			if (loc < 0) continue;
			glUniform1i(loc, kv.second.Unit);
			rc.BindTexture(static_cast<GLuint>(kv.second.Unit), kv.second.Tex->GetTextureID());
		}
	}
} // namespace SJH

#endif // __SJH_DRAW_OPS_H__
