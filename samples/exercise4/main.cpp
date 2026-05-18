#include <sb7.h>
#include <shader.h>
#include <GL/glcorearb.h> //
#include <GL/gl3w.h>
#include <vmath.h>
#include <vector>
#include <iostream>
#include <memory>

namespace exercise4
{
	struct shader_source
	{
		GLenum shader_type;
		const GLchar *shader_path;
	};

	class program_base
	{
	private:
		GLuint programAddr;
		GLuint vertexArrayAddr;

		GLuint create_shader(const shader_source &source) const
		{
			GLuint shaderAddr = sb7::shader::load(source.shader_path, source.shader_type);
			if (shaderAddr == 0) {
				std::cerr << "셰이더 로드 실패: " << source.shader_path << std::endl;
			}
			return shaderAddr;
		}

		GLuint create_program(const std::vector<shader_source> &sources)
		{
			GLuint addr = glCreateProgram();
			std::vector<GLchar> created_shaders;
			for (const auto &source : sources){
				created_shaders.push_back(create_shader(source));
				glAttachShader(addr, created_shaders.back());
			}
			glLinkProgram(addr);
			for (const auto &shader : created_shaders)
				glDeleteShader(shader);
			return addr;
		}
		// vertex_shade = sb7::shader::load("PATH", 쉐이더 파일);

	public:
		program_base(const std::vector<shader_source> &sources)
		    : programAddr(create_program(sources))
		{
			glGenVertexArrays(1, &vertexArrayAddr);
			glBindVertexArray(vertexArrayAddr);
		}

		virtual ~program_base()
		{
			glDeleteProgram(programAddr);
			glDeleteVertexArrays(1, &vertexArrayAddr);
		}

		const GLuint GetProgram() const
		{
			return programAddr;
		}

		void UseProgram() const
		{
			glUseProgram(programAddr);
		}

		virtual void Draw(float currentTime) const {
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	};
	
	class pinwheel_program : public program_base
	{
		static constexpr const GLchar *vs_path = "./shaders/pinwheel_vs.glsl";
		static constexpr const GLchar *fs_path = "./shaders/pinwheel_fs.glsl";


	public:
		pinwheel_program() : program_base({
			{GL_VERTEX_SHADER, vs_path},
			{GL_FRAGMENT_SHADER, fs_path}
		}) {}
		
		virtual void Draw(float currentTime) const override { 
			glVertexAttrib1f(0, (float)currentTime);
			glDrawArrays(GL_TRIANGLES, 0, 12);
		}
	};

	class stick_program : public program_base
	{
		static constexpr const GLchar *vs_path = "./shaders/stick_vs.glsl";
		static constexpr const GLchar *fs_path = "./shaders/stick_fs.glsl";

	public:
		stick_program() : program_base({
			{GL_VERTEX_SHADER, vs_path},
			{GL_FRAGMENT_SHADER, fs_path}
		}) {}
		
		virtual void Draw(float currentTime) const override { 
			glVertexAttrib1f(0, (float)currentTime);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
	};

	class my_application : public sb7::application
	{
	private:
		std::vector<program_base> programs;
		GLfloat background_color[4] = {0.0f,0.0f,0.0f, 1.0f};

	public:
		virtual void startup() override
		{
			programs.push_back(stick_program());
			programs.push_back(pinwheel_program());
		}

		virtual void render(double currentTime) override
		{
			glClearBufferfv(GL_COLOR, 0, background_color);
			for (const auto &program : programs){
				program.UseProgram();
				program.Draw(currentTime);
			}
		}
		
		virtual void shutdown() override
		{
		}
	};
}

DECLARE_MAIN(exercise4::my_application);
