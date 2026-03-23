#include <sb7.h>
#include <GL/glcorearb.h> //
#include <GL/gl3w.h>
#include <vmath.h>
#include <vector>
#include <iostream>
#include <memory>

namespace exercise2
{
	struct shader_source
	{
		GLenum shader_type;
		const GLchar *shader_str;
	};

	class program_base
	{
	private:
		GLuint programAddr;
		GLuint vertexArrayAddr;

		GLuint create_shader(const shader_source &source) const
		{
			GLuint addr = glCreateShader(source.shader_type);
			glShaderSource(addr, 1, &source.shader_str, nullptr);
			glCompileShader(addr);

			int success = 0;
			glGetShaderiv(addr, GL_COMPILE_STATUS, &success);
			if (!success)
			{
				char logBuff[1024] = {
				    0,
				};
				glGetShaderInfoLog(addr, 1024, nullptr, logBuff);
				std::cerr << "fail to compile " << source.shader_str << "shader" << std::endl;
				std::cerr << "reason " << logBuff << std::endl;
				glDeleteShader(addr);
			}

			return addr;
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
			int success = 0;
			glGetProgramiv(addr, GL_LINK_STATUS, &success);
			if (!success)
			{
				char logBuff[1024] = {
				    0,
				};
				glGetProgramInfoLog(programAddr, 1024, nullptr, logBuff);
				std::cerr << "fail to link program" << std::endl;
				std::cerr << "reason " << logBuff << std::endl;
				glDeleteProgram(programAddr);
			}
			for (const auto &shader : created_shaders)
				glDeleteShader(shader);
			return addr;
		}

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

		void Draw() const {
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	};
	class pinwheel_program : public program_base
	{
		static constexpr const GLchar *vs = R"(
			#version 410 core
			void main(void) {
				const vec4 vertices[3] = vec4[3](
				    vec4(0.0, 0.2887, 0.0, 1.0),
				    vec4(-0.25, -0.1443, 0.0, 1.0),
				    vec4(0.25, -0.1443, 0.0, 1.0)
				);
				gl_Position = vertices[gl_VertexID];
			}
		)";
		static constexpr const GLchar *fs = R"(
			#version 410 core
			out vec4 color;
			void main(void) {
				color = vec4(0.0, 0.3, 0.8, 1.0);
			}
		)";

	public:
		pinwheel_program() : program_base({{GL_VERTEX_SHADER, vs},
						   {GL_FRAGMENT_SHADER, fs}}) {}
	};

	class my_application : public sb7::application
	{
	private:
		std::vector<std::unique_ptr<program_base>> programs;
		GLfloat background_color[4] = {0.5f, 0.5f, 0.5f, 1.0f};

	public:
		virtual void startup() override
		{
			// programs = std::vector<program_base>(2);
			programs.push_back(std::make_unique<pinwheel_program>());
		}

		virtual void render(double currentTime) override
		{
			glClearBufferfv(GL_COLOR_BUFFER_BIT, 0, background_color);
			for (const auto &program : programs){
				program->UseProgram();
				program->Draw();
			}
		}
		virtual void shutdown() override
		{
		}
	};
}

DECLARE_MAIN(exercise2::my_application);