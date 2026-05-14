#include "engine/shader_program.h"
#include "engine/constants.h"
#include "engine/material.h"
#include "engine/uniforms.h"

#include "diagnostics/engine_diagnostics.h"
#include "diagnostics/gl_log.h"
#include "diagnostics/uniform_diagnostics.h"

#include <sb7.h>
#include <shader.h>

#include <cstdlib>
#include <iostream>

namespace diag = SJH::Diagnostics;

namespace Engine::Program
{
	using namespace Engine::Constants::UNIFORM;

	ShaderProgram::ShaderProgram(const char *vs_path, const char *fs_path)
	{
		ProgAddr = glCreateProgram();
		GLuint vsAddr = sb7::shader::load(vs_path, GL_VERTEX_SHADER, true);
		if (vsAddr == 0)
		{
			std::cerr << "버텍스 쉐이더 로드 실패 : " << vs_path << std::endl;
			exit(1);
		}
		// sb7::shader::load 는 GL_COMPILE_STATUS 가 false 여도 0 이 아닌 핸들을 돌려줄 수 있음.
		// 컴파일 로그를 명시 확인해서 link 전에 원인을 못 박는다.
		diag::GLObjectLog::CheckShaderCompile(vsAddr, vs_path);

		GLuint fsAddr = sb7::shader::load(fs_path, GL_FRAGMENT_SHADER, true);
		if (fsAddr == 0)
		{
			std::cerr << "프래그먼트 쉐이더 로드 실패 : " << fs_path << std::endl;
			exit(1);
		}
		diag::GLObjectLog::CheckShaderCompile(fsAddr, fs_path);

		glAttachShader(ProgAddr, vsAddr);
		glAttachShader(ProgAddr, fsAddr);
		glLinkProgram(ProgAddr);
		diag::GLObjectLog::CheckProgramLink(ProgAddr, fs_path);
		glDeleteShader(vsAddr);
		glDeleteShader(fsAddr);
	}

	ShaderProgram::~ShaderProgram()
	{
		diag::GLObjectLog::InvalidateProgramCache(ProgAddr);
		diag::UniformDiagnostics::Invalidate(ProgAddr);
		glDeleteProgram(ProgAddr);
	}

	void ShaderProgram::Render(const ModelMap &models,
	                           const vmath::mat4 &view,
	                           const vmath::mat4 &proj,
	                           const vmath::vec3 &viewPos)
	{
		Apply(view, proj, viewPos);
		for (auto &entry : models)
		{
			// material->program 이 this 와 다르면 skip — multi-program 씬 안전성.
			auto *mat = entry.second->GetMaterial();
			if (mat != nullptr && mat->program != nullptr && mat->program != this)
				continue;
			entry.second->Draw();
		}
	}

	void DefaultShaderProgram::Apply(const vmath::mat4 &view,
	                                 const vmath::mat4 &proj,
	                                 const vmath::vec3 &viewPos)
	{
		(void)viewPos;
		glUseProgram(ProgAddr);
		glUniformMatrix4fv(glGetUniformLocation(ProgAddr, UNIFORM_VIEW_MAT),
		                   1, false, view);
		glUniformMatrix4fv(glGetUniformLocation(ProgAddr, UNIFORM_PROJ_MAT),
		                   1, false, proj);
	}

	void TextureShaderProgram::Apply(const vmath::mat4 &view,
	                                 const vmath::mat4 &proj,
	                                 const vmath::vec3 &viewPos)
	{
		glUseProgram(ProgAddr);
		diag::EngineDiagnostics::CheckExpectedLightingUniforms(ProgAddr, "texture_program");
		glUniformMatrix4fv(glGetUniformLocation(ProgAddr, UNIFORM_VIEW_MAT),
		                   1, false, view);
		glUniformMatrix4fv(glGetUniformLocation(ProgAddr, UNIFORM_PROJ_MAT),
		                   1, false, proj);

		if (AttachedLight != nullptr)
			Engine::Uniforms::Apply(ProgAddr, *AttachedLight, viewPos);
		else
			Engine::Uniforms::ApplyDisabled(ProgAddr);
	}
} // namespace Engine::Program
