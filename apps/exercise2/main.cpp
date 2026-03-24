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

		virtual void Draw(float currentTime) const {
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	};
	
	class pinwheel_program : public program_base
	{
		static constexpr const GLchar *vs = R"(
			#version 410 core
			layout (location = 0) in float currentTime; 
			// [1] 
			layout(location = 0) in vec4 vertexColor;

			out VS_OUT {
				vec4 color;
			} vs_out;

			void main(void) {
				const vec4 base[3] = vec4[3](
					vec4(0.0, 0.0, 0.0, 1.0),
					vec4(0.0, 0.5, 0.0, 1.0),
					vec4(-0.5, 0.5, 0.0, 1.0)
				);

				// [2]
				// const vec4 bladeColors[4] = vec4[4](
				// 	vec4(1.0, 0.0, 0.0, 1.0),  // blade 0: 빨강
				// 	vec4(0.0, 1.0, 0.0, 1.0),  // blade 1: 초록
				// 	vec4(0.0, 0.0, 1.0, 1.0),  // blade 2: 파랑
				// 	vec4(1.0, 1.0, 0.0, 1.0)   // blade 3: 노랑
				// );

				// [3]
				float curTimeCos = cos(currentTime) * 0.5 + 0.5f;
				float curTimeSin = sin(currentTime) * 0.5 + 0.5f;
				vec4 vertexTints[3] = vec4[3](
					vec4(curTimeCos, curTimeSin, curTimeCos, 1.0),
					vec4(curTimeSin, curTimeCos, curTimeSin, 1.0),
					vec4(curTimeSin, curTimeSin, curTimeCos, 1.0)
				);

				int bladeID = gl_VertexID / 3;
				int bladeVertID = gl_VertexID % 3;
				
				float angle = float(bladeID) * radians(90.0) + currentTime;
				float curRotateCos = cos(angle);
				float curRotateSin = sin(angle);
				mat2 rot = mat2(
					curRotateCos, curRotateSin,
					-curRotateSin, curRotateCos
				);
				vec2 rotated = rot * base[bladeVertID].xy;
				gl_Position = vec4(rotated, 0.0, 1.0);
				// [1] 
				vs_out.color = vertexColor;
				// [2] vs_out.color = bladeColors[bladeID];
				// [3] vs_out.color = vec4(vertexTints[bladeVertID].rgb, 1.0);
			}
		)";
		static constexpr const GLchar *fs = R"(
			#version 410 core
			out vec4 color;

			in VS_OUT {
				vec4 color;
			} fs_in;
			void main(void) {
				color = fs_in.color;
			}
		)";

	public:
		pinwheel_program() : program_base({{GL_VERTEX_SHADER, vs},
						   {GL_FRAGMENT_SHADER, fs}}) {}
		
		virtual void Draw(float currentTime) const override { 
			GLfloat bladeColors[] = {
				1.0, 0.0, 0.0, 1.0,  // blade 0: 빨강
				0.0, 1.0, 0.0, 1.0,  // blade 1: 초록
				0.0, 0.0, 1.0, 1.0,  // blade 2: 파랑
				1.0, 1.0, 0.0, 1.0   // blade 3: 노랑
			};
			glVertexAttrib4fv(1, bladeColors);
			// glVertexAttrib1f(0, (float)currentTime);
			glDrawArrays(GL_TRIANGLES, 0, 12);
		}
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
			glClearBufferfv(GL_COLOR, 0, background_color);
			for (const auto &program : programs){
				program->UseProgram();
				program->Draw(currentTime);
			}
		}
		
		virtual void shutdown() override
		{
			for(auto& program : programs)
				program.reset();
		}
	};
}

DECLARE_MAIN(exercise2::my_application);