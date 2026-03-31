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
		const char *shader_path;
	};

	class program_base
	{
	protected:
		GLuint programAddr;
		GLuint vaoAddr;
		GLuint vboAddr;

		GLuint create_shader(const shader_source &source) const
		{
			GLuint addr = sb7::shader::load(source.shader_path, source.shader_type, true);
			if(addr == 0)
				std::cerr << "셰이더 로드 실패 : " << source.shader_path << std::endl;
			return addr;
		}

		GLuint create_program(const std::vector<shader_source> &sources)
		{
			GLuint addr = glCreateProgram();
			std::vector<GLchar> created_shaders;
			for (const auto &source : sources)
			{
				created_shaders.push_back(create_shader(source));
				glAttachShader(addr, created_shaders.back());
			}
			glLinkProgram(addr);
			for (const auto &shader : created_shaders)
				glDeleteShader(shader);
			return addr;
		}

	public:
		program_base(const std::vector<shader_source> &sources)
		    : programAddr(create_program(sources))
		{
			glGenVertexArrays(1, &vaoAddr);
			glBindVertexArray(vaoAddr);
		}

		virtual ~program_base()
		{
			glDeleteProgram(programAddr);
			glDeleteVertexArrays(1, &vaoAddr);
		}

		const GLuint GetProgram() const
		{
			return programAddr;
		}

		void UseProgram() const
		{
			glUseProgram(programAddr);
		}

		// uniform mat4 전달
		void SetUniformMat4(const char *name, vmath::mat4 *mat) const
		{
			GLint loc = glGetUniformLocation(programAddr, name);
			glUniformMatrix4fv(loc, 1, GL_FALSE, *mat);
		}

		virtual void Draw(float currentTime, vmath::mat4 *vpMatrix) const
		{
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	};

	class triangle_program : public program_base
	{
	private:
		static constexpr const char* vs_path = "./shaders/triangle_vs.glsl";
		static constexpr const char* fs_path = "./shaders/triangle_fs.glsl";
		GLfloat vertices[24] = {
			0.0f	, 0.866f	, 0.0f	, 1.0f	, 1.0f, 0.0f, 0.0f, 1.0f,
			-0.5f	, -0.433f	, 0.0f	, 1.0f	, 0.0f, 1.0f, 0.0f, 1.0f,
			0.5f	, -0.433f	, 0.0f	, 1.0f	, 0.0f, 0.0f, 1.0f, 1.0f
		};

	public:
		triangle_program() 
		: program_base({
			{GL_VERTEX_SHADER, vs_path},
			{GL_FRAGMENT_SHADER, fs_path}
		}) {
			glGenBuffers(1, &vboAddr);
			glBindBuffer(GL_ARRAY_BUFFER, vboAddr);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

			glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);

			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
			glEnableVertexAttribArray(1);

			// UNBind
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		}
		virtual void Draw(float currentTime, vmath::mat4 *vpMatrix) const override
		{
			glBindVertexArray(vaoAddr);
			if(vpMatrix != nullptr) SetUniformMat4("mvp", vpMatrix);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	};

	class my_application : public sb7::application
	{
	private:
		std::vector<std::unique_ptr<program_base>> programs;
		GLfloat background_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	public:
		virtual void startup() override
		{
			programs.push_back(std::make_unique<triangle_program>());
		}

		virtual void render(double currentTime) override
		{
			glClearBufferfv(GL_COLOR, 0, background_color);
			for (const auto &program : programs)
			{
				program->UseProgram();
				program->Draw(currentTime, nullptr);
			}
		}

		// 키보드 입력 처리 (sb7 콜백)
		virtual void onKey(int key, int action) override
		{
			const float moveSpeed = 0.05f;
			const float rotSpeed = 2.0f;
		}

		virtual void shutdown() override
		{
			for (auto &program : programs)
				program.reset();
		}
	};
}

DECLARE_MAIN(exercise3::my_application);