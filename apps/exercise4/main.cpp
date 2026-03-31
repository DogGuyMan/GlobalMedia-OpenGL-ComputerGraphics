#include <sb7.h>
#include <shader.h>
#include <GL/glcorearb.h> //
#include <GL/gl3w.h>
#include <vmath.h>
#include <vector>
#include <iostream>
#include <memory>

namespace exercise3
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
			std::cout << "loaded" << std::endl;
			return sb7::shader::load(source.shader_path, source.shader_type, true);
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
		std::vector<std::unique_ptr<program_base>> programs;
		GLfloat background_color[4] = {0.0f,0.0f,0.0f, 1.0f};

	public:
		virtual void startup() override
		{
			// programs = std::vector<program_base>(2);
			programs.push_back(std::make_unique<stick_program>());
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

DECLARE_MAIN(exercise3::my_application);

/*
        // vec3 dirVec = (camPos - targetPos) * normalize(camPos - targetPos);
        // vec3 rightVec = normalize(worldUpVec * dirVec);
        // vec3 camUpVec = cross(dirVec, rightVec);

        // mat4 lookAt = mat4(
        //                 1.0, 0.0, 0.0, 0.0,
        //                 0.0, 1.0, 0.0, 0.0,
        //                 0.0, 0.0, 1.0, 0.0,
        //                 -camPos.x, -camPos.y, -camPos.z, 1
        //         ) * mat4(
        //                         rightVec.x, camUpVec.x, dirVec.x, 0.0,
        //                         rightVec.y, camUpVec.y, dirVec.y, 0.0,
        //                         rightVec.z, camUpVec.z, dirVec.z, 0.0,
        //                         0.0, 0.0, 0.0, 1
        //                 );
*/