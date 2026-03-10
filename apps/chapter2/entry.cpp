#include "entry.h"
#include <GL/gl3w.h>
#include <GL/glcorearb.h>
#include <vmath.h>
#include <common/common.h>

// sb7::application 상속받음
// class에서 상속 시 접근 지정자를 생략하면 기본값은 private 상속입니다.
// 이거 까먹은지 너무 오래되었는데 무조건 public 상속을 하자.
namespace SJH::Chapter2
{
	void my_application_1::render(double currentTime)
	{
		colorVectors[0] = (GLfloat)sin(currentTime) * 0.5f + 0.5f;
		colorVectors[1] = (GLfloat)cos(currentTime) * 0.5f + 0.5f;
		colorVectors[2] = (GLfloat)sin(currentTime) * 0.5f + 0.5f;
		colorVectors[3] = (GLfloat)1.0f;
		glClearBufferfv(GL_COLOR, 0, colorVectors);
	}
};

namespace SJH::Chapter2
{
	GLuint my_application_2::compile_shaders()
	{
		// 셰이더 파일 로드
		auto vs_source = SJH::Common::LoadTextFile("shaders/vertex_shader.glsl");
		auto fs_source = SJH::Common::LoadTextFile("shaders/fragment_shader.glsl");

		// 버텍스 쉐이더
		GLuint vertex_shader;
		const GLchar *vs_str = vs_source->c_str();
		vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex_shader, 1, &vs_str, NULL);
		glCompileShader(vertex_shader);

		// 프래그먼트 쉐이더
		GLuint fragment_shader;
		const GLchar *fs_str = fs_source->c_str();
		fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment_shader, 1, &fs_str, NULL);
		glCompileShader(fragment_shader);

		GLuint program;
		program = glCreateProgram();

		glAttachShader(program, vertex_shader);	  // Attatch 하고 난 다음에는 GPU VRAM에 복사가 끝나게 되서
		glAttachShader(program, fragment_shader); // Attatch 하고 난 다음에는 GPU VRAM에 복사가 끝나게 되서

		glLinkProgram(program);

		// 쉐이더는 삭제해도 됨
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
		return program;
	}

	void my_application_2::startup()
	{
		rendering_program = compile_shaders();
		glGenVertexArrays(1, &vertex_array_object);
		glBindVertexArray(vertex_array_object);
	}

	void my_application_2::render(double currentTime)
	{
		colorVectors[0] = (GLfloat)sin(currentTime) * 0.5f + 0.5f;
		colorVectors[1] = (GLfloat)cos(currentTime) * 0.5f + 0.5f;
		colorVectors[2] = (GLfloat)sin(currentTime) * 0.5f + 0.5f;
		colorVectors[3] = (GLfloat)1.0f;
		glClearBufferfv(GL_COLOR, 0, colorVectors);

		glUseProgram(rendering_program);

		glPointSize(40.0f);
		glDrawArrays(GL_POINTS, 0, 1);
	}

	void my_application_2::shutdown()
	{
		glDeleteVertexArrays(1, &vertex_array_object);
		glDeleteProgram(rendering_program);
	}
};

namespace SJH::Chapter2
{
	GLuint my_application_3::compile_shaders()
	{
		// 쉐이더 포인터
		GLuint vertex_shader;
		const GLchar *vertex_shader_source[] = {
		    "#version 410 core\n",
		    "\n",
		    "void main(void)\n",
		    "{\n",
		    "	const vec4 vertices[3] = vec4[3](vec4(0.25, -0.25, 0.5, 1.0),\n",
		    "	vec4(-0.25, 0.25, 0.5, 1.0),\n",
		    "	vec4(0.25, 0.25, 0.5, 1.0));\n",
		    "	gl_Position = vertices[gl_VertexID];\n",
		    "}\n",
		};
		vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex_shader, 9, vertex_shader_source, NULL);
		glCompileShader(vertex_shader);

		// 쉐이더 포인터
		GLuint fragment_shader;
		const GLchar *fragment_shader_source[] = {
		    "#version 410 core \n",
		    "out vec4 color; \n",
		    " \n",
		    "void main(void)  \n",
		    "{ \n",
		    "    color = vec4(0.0, 0.8, 1.0, 1.0); \n",
		    "} \n",
		};
		fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment_shader, 7, fragment_shader_source, NULL);
		glCompileShader(fragment_shader);

		GLuint program;
		program = glCreateProgram();

		glAttachShader(program, vertex_shader);
		glAttachShader(program, fragment_shader);

		glLinkProgram(program);

		// 쉐이더는 삭제해도 됨
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
		return program;
	}

	void my_application_3::render(double currentTime)
	{
		colorVectors[0] = (GLfloat)sin(currentTime) * 0.5f + 0.5f;
		colorVectors[1] = (GLfloat)cos(currentTime) * 0.5f + 0.5f;
		colorVectors[2] = (GLfloat)sin(currentTime) * 0.5f + 0.5f;
		colorVectors[3] = (GLfloat)1.0f;
		glClearBufferfv(GL_COLOR, 0, colorVectors);

		glUseProgram(rendering_program);

		glPointSize(40.0f);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
};