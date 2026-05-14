#ifndef __ENGINE_UNIFORMS_H__
#define __ENGINE_UNIFORMS_H__

#include "GL/gl3w.h"
#include "engine/constants.h"
#include "engine/lighting.h"
#include "engine/material.h"
#include "diagnostics/uniform_diagnostics.h"
#include "vmath.h"
#include <string>

// ────────────────────────────────────────────────────────────────────
// 자유 함수 모음 — 광원/머티리얼 데이터를 GL program 의 uniform 으로 푸시.
// 헤더-only inline 으로 ODR 안전. 광원/머티리얼 자료형은 lighting.h / material.h 가 제공.
// 각 함수는 glUseProgram(progAddr) 이 끝난 뒤 호출해야 한다.
// ────────────────────────────────────────────────────────────────────

namespace Engine::Uniforms
{
	// glGetUniformLocation + 누락 감지 진단 (loc < 0 일 때 NotifyMissing).
	inline GLint UniformLoc(GLuint program, const char *name)
	{
		const GLint loc = glGetUniformLocation(program, name);
		if (loc < 0)
			SJH::Diagnostics::UniformDiagnostics::NotifyMissing(program, name);
		return loc;
	}

	// ─── chapter7 단일 Phong Light (texture_fs.glsl 셰이더 계약) ───
	// glUseProgram 직후 호출. inLightingEnabled = 1 로 라이팅 ON.
	inline void Apply(GLuint progAddr, const Engine::Lighting::Light &light, const vmath::vec3 &viewPos)
	{
		namespace U = Engine::Constants::UNIFORM;
		glUniform3fv(UniformLoc(progAddr, U::UNIFORM_LIGHT_POS), 1, light.Position);
		glUniform3fv(UniformLoc(progAddr, U::UNIFORM_LIGHT_COLOR), 1, light.Color);
		glUniform3fv(UniformLoc(progAddr, U::UNIFORM_VIEW_POS), 1, viewPos);
		glUniform1f(UniformLoc(progAddr, U::UNIFORM_AMBIENT_STRENGTH), light.AmbientStrength);
		glUniform1f(UniformLoc(progAddr, U::UNIFORM_SPECULAR_STRENGTH), light.SpecularStrength);
		glUniform1f(UniformLoc(progAddr, U::UNIFORM_SHININESS), light.Shininess);
		glUniform1f(UniformLoc(progAddr, U::UNIFORM_LIGHTING_ENABLED), 1.0f);
	}

	// 라이팅을 끄는 경로 — inLightingEnabled = 0 만 푸시. 다른 라이팅 uniform 은 셰이더에서 무시.
	inline void ApplyDisabled(GLuint progAddr)
	{
		namespace U = Engine::Constants::UNIFORM;
		glUniform1f(UniformLoc(progAddr, U::UNIFORM_LIGHTING_ENABLED), 0.0f);
	}

	// ─── chapter9 multi-light (basic_lighting_fs.glsl 셰이더 계약) ───
	// shader struct 멤버 단위로 uniform 을 푸시. prefix = "dirLight" / "pointLights[0]" / "spotLight" 등.
	inline void UniformsSetDirLight(GLuint program, const std::string &prefix, const Engine::Lighting::DirLight &light)
	{
		const GLint locDirection = UniformLoc(program, (prefix + ".direction").c_str());
		const GLint locAmbient = UniformLoc(program, (prefix + ".ambient").c_str());
		const GLint locDiffuse = UniformLoc(program, (prefix + ".diffuse").c_str());
		const GLint locSpecular = UniformLoc(program, (prefix + ".specular").c_str());

		glUniform3fv(locDirection, 1, light.direction);
		glUniform3fv(locAmbient, 1, light.ambient);
		glUniform3fv(locDiffuse, 1, light.diffuse);
		glUniform3fv(locSpecular, 1, light.specular);
	}

	inline void UniformsSetPointLight(GLuint program, const std::string &prefix, const Engine::Lighting::PointLight &light)
	{
		const GLint locPosition = UniformLoc(program, (prefix + ".position").c_str());
		const GLint locAmbient = UniformLoc(program, (prefix + ".ambient").c_str());
		const GLint locDiffuse = UniformLoc(program, (prefix + ".diffuse").c_str());
		const GLint locSpecular = UniformLoc(program, (prefix + ".specular").c_str());
		const GLint locC1 = UniformLoc(program, (prefix + ".c1").c_str());
		const GLint locC2 = UniformLoc(program, (prefix + ".c2").c_str());

		glUniform3fv(locPosition, 1, light.position);
		glUniform3fv(locAmbient, 1, light.ambient);
		glUniform3fv(locDiffuse, 1, light.diffuse);
		glUniform3fv(locSpecular, 1, light.specular);
		glUniform1f(locC1, light.c1);
		glUniform1f(locC2, light.c2);
	}

	inline void UniformsSetSpotLight(GLuint program, const std::string &prefix, const Engine::Lighting::SpotLight &light)
	{
		const GLint locPosition = UniformLoc(program, (prefix + ".position").c_str());
		const GLint locDirection = UniformLoc(program, (prefix + ".direction").c_str());
		const GLint locCutOff = UniformLoc(program, (prefix + ".cutOff").c_str());
		const GLint locOuterCutOff = UniformLoc(program, (prefix + ".outerCutOff").c_str());
		const GLint locC1 = UniformLoc(program, (prefix + ".c1").c_str());
		const GLint locC2 = UniformLoc(program, (prefix + ".c2").c_str());
		const GLint locAmbient = UniformLoc(program, (prefix + ".ambient").c_str());
		const GLint locDiffuse = UniformLoc(program, (prefix + ".diffuse").c_str());
		const GLint locSpecular = UniformLoc(program, (prefix + ".specular").c_str());

		glUniform3fv(locPosition, 1, light.position);
		glUniform3fv(locDirection, 1, light.direction);
		glUniform1f(locCutOff, light.cutOff);
		glUniform1f(locOuterCutOff, light.outerCutOff);
		glUniform1f(locC1, light.c1);
		glUniform1f(locC2, light.c2);
		glUniform3fv(locAmbient, 1, light.ambient);
		glUniform3fv(locDiffuse, 1, light.diffuse);
		glUniform3fv(locSpecular, 1, light.specular);
	}

	// PhongMaterial 의 sampler2D 유니폼에는 텍스처 "이미지 유닛 번호" 를 넣는다 (텍스처 객체 핸들이 아님).
	inline void UniformsSetMaterial(GLuint program, const std::string &prefix, const Engine::Material::PhongMaterial &material)
	{
		const GLint locDiffuse = UniformLoc(program, (prefix + ".diffuse").c_str());
		const GLint locSpecular = UniformLoc(program, (prefix + ".specular").c_str());
		const GLint locShininess = UniformLoc(program, (prefix + ".shininess").c_str());

		glUniform1i(locDiffuse, material.diffuseUnit);
		glUniform1i(locSpecular, material.specularUnit);
		glUniform1f(locShininess, material.shininess);
	}
} // namespace Engine::Uniforms

#endif // __ENGINE_UNIFORMS_H__
