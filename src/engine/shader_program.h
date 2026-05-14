#ifndef __ENGINE_SHADER_PROGRAM_H__
#define __ENGINE_SHADER_PROGRAM_H__

#include "GL/gl3w.h"
#include "engine/geometry.h"
#include "engine/lighting.h"
#include "engine/model_base.h"
#include "vmath.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Engine::Program
{
	// chapter 가 Models 컬렉션을 보유하다가 Render 호출 시 전달하는 컨테이너 타입.
	using ModelMap = std::unordered_map<std::string, std::unique_ptr<Model::ModelBase>>;

	// 기저 클래스: GL Program 자체의 lifecycle (compile / link / destroy) 만 책임.
	// Apply() / Render() 는 셰이더 의존이라 subclass 가 구현.
	// PhongMaterial 이 Material 의 specific 인 것과 동일한 패턴.
	class ShaderProgram
	{
	  public:
		GLuint ProgAddr = 0;

		void ClearBuffer()
		{
			glClearBufferfv(GL_COLOR, 0, Constants::GEOMETRY::COLOR_BG);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);
			// glEnable(GL_CULL_FACE);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		ShaderProgram(const char *vs_path, const char *fs_path);
		virtual ~ShaderProgram();
		ShaderProgram(const ShaderProgram &) = delete;
		ShaderProgram &operator=(const ShaderProgram &) = delete;

		// glUseProgram + 셰이더별 per-frame uniform (view, proj, light, ...) 셋업.
		// Apply 호출 후에 Material::Apply / Model::Draw 가 동일 program 위에 uniform 을 얹는다.
		virtual void Apply(const vmath::mat4 &view,
		                   const vmath::mat4 &proj,
		                   const vmath::vec3 &viewPos) = 0;

		// 편의 메소드: Apply(...) + models 순회 + 각 model->Draw().
		// material->program == this 가 아닌 모델은 skip (multi-program 씬 안전).
		void Render(const ModelMap &models,
		            const vmath::mat4 &view,
		            const vmath::mat4 &proj,
		            const vmath::vec3 &viewPos);
	};

	// 단순 셰이더: view / proj 만 셋업, 텍스처/라이팅 없음.
	class DefaultShaderProgram : public ShaderProgram
	{
	  public:
		using ShaderProgram::ShaderProgram;

		void Apply(const vmath::mat4 &view,
		           const vmath::mat4 &proj,
		           const vmath::vec3 &viewPos) override;
	};

	// 텍스처 + 옵션 라이팅 셰이더. AttachedLight 가 nullptr 이면 ApplyDisabled() 로 라이팅 OFF.
	// AttachedLight 는 외부 소유 (씬이 가진 Light 객체를 참조만).
	class TextureShaderProgram : public ShaderProgram
	{
	  public:
		using ShaderProgram::ShaderProgram;

		// 셰이더-의존 멤버 (PhongMaterial 가 shininess 를 멤버로 갖는 것과 동일 패턴)
		Engine::Lighting::Light *AttachedLight = nullptr;

		void Apply(const vmath::mat4 &view,
		           const vmath::mat4 &proj,
		           const vmath::vec3 &viewPos) override;
	};
} // namespace Engine::Program

#endif // __ENGINE_SHADER_PROGRAM_H__
