#include "engine/material.h"

#include "diagnostics/engine_diagnostics.h"
#include "engine/constants.h"
#include "engine/shader_program.h" // Material::Apply 에서 program->ProgAddr 사용
#include "stb_image.h"
#include <iostream>

namespace diag = SJH::Diagnostics;

namespace Engine::Material
{
	Material::Material() = default;

	Material::~Material()
	{
		for (auto &slot : Slots)
			if (slot.TexAddr != 0)
				glDeleteTextures(1, &slot.TexAddr);
	}

	void Material::LoadTexture(const char *image_path,
	                           GLuint format,
	                           GLint internal_format,
	                           const TextureParams &params)
	{
		TextureSlot slot;

		glGenTextures(1, &slot.TexAddr);
		glBindTexture(GL_TEXTURE_2D, slot.TexAddr);

		int width, height, nrChannels;
		unsigned char *data = stbi_load(image_path, &width, &height, &nrChannels, 0);
		if (data == 0)
		{
			std::cerr << "텍스쳐 로드 실패 : " << image_path << std::endl;
		}
		// stbi 채널 수 ↔ GL format 정합성 (JPG 를 GL_RGBA 로 로드 등 흔한 버그)
		diag::EngineDiagnostics::CheckTextureFormat(image_path, nrChannels, width, height,
		                                            format, internal_format, image_path);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0,
			             format, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		stbi_image_free(data);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, params.WrapS);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, params.WrapT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, params.MinFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, params.MagFilter);

		Slots.push_back(slot);
	}

	void Material::Apply() const
	{
		if (program == nullptr)
			return;

		const GLuint progAddr = program->ProgAddr;
		glUniform4fv(glGetUniformLocation(progAddr, Engine::Constants::UNIFORM::UNIFORM_BASE_COLOR),
		             1, BaseColor);

		const GLuint texture_enums[] = {
		    GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2, GL_TEXTURE3};
		for (int i = 0; i < Engine::Constants::UNIFORM::TEXTURE_SLOT_COUNT; i++)
		{
			if (i < GetSlotCount())
			{
				const auto &slot = Slots[i];
				glActiveTexture(texture_enums[i]);
				glBindTexture(GL_TEXTURE_2D, slot.TexAddr);
				glUniform1i(glGetUniformLocation(progAddr, Engine::Constants::UNIFORM::UNIFORM_SAMPLER_TEXS[i]), i);
				glUniform1f(glGetUniformLocation(progAddr, Engine::Constants::UNIFORM::UNIFORM_TEX_USED[i]), 1.0f);
				glUniform2fv(glGetUniformLocation(progAddr, Engine::Constants::UNIFORM::UNIFORM_UV_OFFSET[i]),
				             1, slot.UVOffset);
				glUniform2fv(glGetUniformLocation(progAddr, Engine::Constants::UNIFORM::UNIFORM_UV_RATIO[i]),
				             1, slot.UVRatio);
			}
			else
			{
				glActiveTexture(texture_enums[i]);
				glBindTexture(GL_TEXTURE_2D, 0);
				glUniform1i(glGetUniformLocation(progAddr, Engine::Constants::UNIFORM::UNIFORM_SAMPLER_TEXS[i]), i);
				glUniform1f(glGetUniformLocation(progAddr, Engine::Constants::UNIFORM::UNIFORM_TEX_USED[i]), 0.0f);
			}
		}
	}
} // namespace Engine::Material
