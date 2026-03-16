#include <sb7.h>
#include <vector>
#include <vmath.h>

namespace SJH::Chapter3::DataTransfer
{
	// glVertexAttrib4fv는 4.1(410 core)에서 당연히 사용 가능
	// glVertexAttrib4fv vs glProgramUniform4fv
	// glVertexAttrib4fv는 vertex attribute의 기본값(generic vertex attribute)을 설정하는 함수
	// 	VAO에 바인딩된 attribute 배열이 비활성화(disabled) 상태일 때, 해당 location의 기본값으로 사용.
	// 	glVertexAttrib4fv(0, attrib); -> location 0의 vertex attribute 기본값을 설정
	// 	이 방식을 쓰려면 셰이더에서 offset이 in(vertex attribute)이어야 한다.

	const GLchar *VERTEX_SHADER_SOURCE_STR = R"(
		#version 410 core
		// 혼용 불가 layout (location = 0) in uniform vec4 offset;
		layout (location = 0) in vec4 offset; // uniform 아님
		layout (location = 1) in vec4 color; // uniform 아님

		// 구조체 처럼 제작할 수 있다.
		out VS_OUT {
			vec4 color;
		} vs_out;

		void main(void) {
			const vec4 vertices[3] = vec4[3](
				vec4(0.25, -0.25, 0.5, 1.0),
				vec4(-0.25, 0.25, 0.5, 1.0),
				vec4(0.25, 0.25, 0.5, 1.0)
			);

			gl_Position = vertices[gl_VertexID] + offset;
			vs_out.color = color;
		}
	)";

	const GLchar *FRAGMENT_SHADER_SOURCE_STR = R"(
		#version 410 core
		in VS_OUT {
			vec4 color;
		} fs_in;

		out vec4 color;
		void main(void) {
			color = fs_in.color;
		}
	)";

	int print_shader_log(GLuint shader)
	{
		int len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		if (len > 0)
		{
			char log_buf[512];
			glGetShaderInfoLog(shader, len, nullptr, log_buf);
			std::printf("Err: Shader program link failed: %s\n", log_buf);
			std::fflush(stdout);
			return -1;
		}
		return 0;
	}

	int print_program_log(GLuint prog)
	{
		int len = 0;
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
		if (len > 0)
		{
			char log_buf[512];
			glGetProgramInfoLog(prog, len, nullptr, log_buf);
			std::printf("Err: Shader program link failed: %s\n", log_buf);
			std::fflush(stdout);
			return -1;
		}
		return 0;
	}

	class my_application : public sb7::application
	{
	private:
		GLuint program_addr;
		GLuint vertex_array_object_addr;

		GLuint create_program(std::vector<GLuint> &&shader_addrs)
		{
			GLuint temp_program_addr = glCreateProgram();
			for (const auto &shader_addr : shader_addrs)
				glAttachShader(temp_program_addr, shader_addr);
			glLinkProgram(temp_program_addr);
			for (auto &shader_addr : shader_addrs)
				glDeleteShader(shader_addr);
			return temp_program_addr;
		}

		GLuint create_vertex_shader()
		{
			GLuint temp_vs_addr = glCreateShader(GL_VERTEX_SHADER);
			glShaderSource(temp_vs_addr, 1, &VERTEX_SHADER_SOURCE_STR, NULL);
			glCompileShader(temp_vs_addr);
			if (print_shader_log(temp_vs_addr) == -1)
			{
				glDeleteShader(temp_vs_addr);
				abort();
			}
			return temp_vs_addr;
		}

		GLuint create_fragment_shader()
		{
			GLuint temp_fs_addr = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(temp_fs_addr, 1, &FRAGMENT_SHADER_SOURCE_STR, NULL);
			glCompileShader(temp_fs_addr);
			if (print_shader_log(temp_fs_addr) == -1)
			{
				glDeleteShader(temp_fs_addr);
				abort();
			}
			return temp_fs_addr;
		}

	public:
		virtual void startup() override
		{
			program_addr = create_program(std::vector<GLuint>{create_vertex_shader(), create_fragment_shader()});

			// ! glCreateVertexArrays(1, &vertex_array_object_addr); 이거 아님!
			glGenVertexArrays(1, &vertex_array_object_addr); // ✅
			glBindVertexArray(vertex_array_object_addr);	 // ⭐️
		}

		virtual void render(double currentTime) override
		{
			const GLfloat gary[] = {0.5f, 0.5f, 0.5f, 1.0f};
			glClearBufferfv(GL_COLOR, 0, gary);
			glUseProgram(program_addr);
			GLfloat attrib[] = {
			    (float)sin(currentTime) * 0.5f,
			    (float)cos(currentTime) * 0.5f,
			    0.0f, 0.0f};

			GLfloat vattrib[] = {
			    1.0, 0.0, 0.0, 1.0};

			// glProgramUniform4fv(program_addr, loc_offset, <idx>, attrib); attrib 사이즈가 4라고 해서  <idx>에 4를 쓰면 안된다!
			// glProgramUniform4fv(program_addr, loc_offset, 1, attrib);

			glVertexAttrib4fv(0, attrib);  // ⭐️
			glVertexAttrib4fv(1, vattrib); // ⭐️
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		virtual void shutdown() override
		{
			glDeleteProgram(program_addr);
			glDeleteVertexArrays(1, &vertex_array_object_addr); // ⭐️
		}
	};
}

namespace SJH::Chapter3::Tesselation
{

	const GLchar *VERTEX_SHADER_SOURCE_STR = R"(
		#version 410 core
		layout (location = 0) in vec4 offset;
		layout (location = 1) in vec4 color;
		out VS_OUT { vec4 color; } vs_out;
		void main(void)
		{
			const vec4 vertices[3] = vec4[3](
				vec4(0.25, -0.25, 0.5, 1.0),
				vec4(-0.25, 0.25, 0.5, 1.0),
				vec4(0.25, 0.25, 0.5, 1.0)
			);
			gl_Position = vertices[gl_VertexID] + offset;
			vs_out.color = color;
		}
	)";

	const GLchar *FRAGMENT_SHADER_SOURCE_STR = R"(
		#version 410 core
		in VS_OUT {vec4 color;} fs_in;
		out vec4 color;
		void main(void) 
		{
			color = fs_in.color;
		}
	)";

	int PRINT_SHADER_LOG(GLuint shaderAddr)
	{
		GLint len = 0;
		glGetShaderiv(shaderAddr, GL_INFO_LOG_LENGTH, &len);
		if (len > 0)
		{
			char LOG_BUF[512] = {
			    0,
			};
			glGetShaderInfoLog(shaderAddr, len, nullptr, LOG_BUF);
			fprintf(stderr, "쉐이더 에러 발생 : %s", LOG_BUF);
			return -1;
		}
		return 0;
	}

	int PRINT_PROGRAM_LOG(GLuint programAddr)
	{
		GLint len = 0;
		glGetProgramiv(programAddr, GL_INFO_LOG_LENGTH, &len);
		if (len > 0)
		{
			char LOG_BUF[512] = {
			    0,
			};
			glGetProgramInfoLog(programAddr, len, nullptr, LOG_BUF);
			fprintf(stderr, "프로그램 에러 발생 %s", LOG_BUF);
			return -1;
		}
		return 0;
	}

	class my_application : public sb7::application
	{
	private:
		GLuint programAddr;
		// GLuint *vertexArrayObjectPtr;
		GLuint vertexArrayObjectAddr;

		GLuint createProgram(std::vector<GLuint> &&shader_addrs)
		{
			GLuint tempprogram = glCreateProgram();
			for (auto shader_addr : shader_addrs)
			{
				glAttachShader(tempprogram, shader_addr);
			}
			glLinkProgram(tempprogram);
			for (auto shader_addr : shader_addrs)
			{
				glDeleteShader(shader_addr);
			}
			return tempprogram;
		}

		GLuint createVertexShader()
		{
			GLuint tempvs = glCreateShader(GL_VERTEX_SHADER);
			glShaderSource(tempvs, 1, &VERTEX_SHADER_SOURCE_STR, NULL);
			glCompileShader(tempvs);
			if (PRINT_SHADER_LOG(tempvs) != 0)
			{
				glDeleteShader(tempvs);
				abort();
			}
			return tempvs;
		}

		GLuint createTessControllShader()
		{
			return -1;
		}

		GLuint createTessEvaluationShader()
		{
			return -1;
		}

		GLuint createFragmentShader()
		{
			GLuint tempfs = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(tempfs, 1, &FRAGMENT_SHADER_SOURCE_STR, NULL);
			glCompileShader(tempfs);
			if (PRINT_SHADER_LOG(tempfs) != 0)
			{
				glDeleteShader(tempfs);
				abort();
			}
			return tempfs;
		}

	public:
		virtual void startup() override
		{
			programAddr = createProgram(std::vector<GLuint>{createVertexShader(), createFragmentShader()});
			// glGenVertexArrays(1, vertexArrayObjectPtr);
			// glBindVertexArray(*vertexArrayObjectPtr);
			glGenVertexArrays(1, &vertexArrayObjectAddr);
			glBindVertexArray(vertexArrayObjectAddr);
		}

		virtual void render(double currentTime) override
		{
			GLfloat backgroundColor[] = {0.5f, 0.5f, 0.5f, 1.0f};
			glClearBufferfv(GL_COLOR, 0, backgroundColor);

			glUseProgram(programAddr);
			GLfloat vertexPositions[] = {
			    (float)sin(currentTime) * 0.8f,
			    (float)cos(currentTime) * 0.8f,
			    0.0f, 0.0f};
			GLfloat vertexColor[] = {1.0f, 1.0f, 0.0f, 1.0f};
			glVertexAttrib4fv(0, vertexPositions);
			glVertexAttrib4fv(1, vertexColor);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		virtual void shutdown() override
		{
			// glDeleteVertexArrays(1, vertexArrayObjectPtr);
			glDeleteVertexArrays(1, &vertexArrayObjectAddr);
			glDeleteProgram(programAddr);
		}
	};
}

DECLARE_MAIN(SJH::Chapter3::Tesselation::my_application);