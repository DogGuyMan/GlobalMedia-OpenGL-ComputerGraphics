#include "entry.h"
#include <GL/gl3w.h>
#include <GL/glcorearb.h>
#include <vmath.h>
#include <common/common.h>
#include <iostream>

// class my_application_1
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

// class my_application_2
namespace SJH::Chapter2
{
	GLuint my_application_2::create_program(GLuint vertex_shader, GLuint fragment_shader)
	{
		GLuint program = glCreateProgram();
		glAttachShader(program, vertex_shader);
		glAttachShader(program, fragment_shader);
		glLinkProgram(program);

		// 링크 후 셰이더는 삭제해도 됨
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
		return program;
	}

	GLuint my_application_2::compile_vertex_shader()
	{
		auto vs_source = SJH::Common::LoadTextFile("shaders/vertex_shader_0.glsl");
		std::cout << vs_source->c_str() << std::endl;
		const GLchar *vs_str = vs_source->c_str();

		GLuint shader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(shader, 1, &vs_str, NULL);
		glCompileShader(shader);
		return shader;
	}

	GLuint my_application_2::compile_fragment_shader()
	{
		auto fs_source = SJH::Common::LoadTextFile("shaders/fragment_shader.glsl");
		std::cout << fs_source->c_str() << std::endl;
		const GLchar *fs_str = fs_source->c_str();

		GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(shader, 1, &fs_str, NULL);
		glCompileShader(shader);
		return shader;
	}

	void my_application_2::startup()
	{
		GLuint vs = compile_vertex_shader();
		GLuint fs = compile_fragment_shader();
		rendering_program = create_program(vs, fs);
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
	GLuint my_application_3::compile_vertex_shader()
	{
		auto vs_shader_source_string = SJH::Common::LoadTextFile("shaders/vertex_shader_1.glsl");
		auto vs_shader_object = glCreateShader(GL_VERTEX_SHADER);
		const GLchar *vs_str = vs_shader_source_string->c_str();
		glShaderSource(vs_shader_object, 1, &vs_str, NULL);
		glCompileShader(vs_shader_object);
		return vs_shader_object;
	}

	GLuint my_application_3::compile_fragment_shader()
	{
		auto fs_shader_source_string = SJH::Common::LoadTextFile("shaders/fragment_shader.glsl");
		auto fs_shader_object = glCreateShader(GL_FRAGMENT_SHADER);
		const GLchar *fs_str = fs_shader_source_string->c_str();
		glShaderSource(fs_shader_object, 1, &fs_str, NULL);
		glCompileShader(fs_shader_object);
		return fs_shader_object;
	}

	GLuint my_application_3::create_program(GLuint vertex_shader, GLuint fragment_shader)
	{
		GLuint program = glCreateProgram();
		glAttachShader(program, vertex_shader);
		glAttachShader(program, fragment_shader);
		glLinkProgram(program);

		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
		return program;
	}

	void my_application_3::startup()
	{
		rendering_program = create_program(compile_vertex_shader(), compile_fragment_shader());
		glGenVertexArrays(1, &vertex_array_object);
		glBindVertexArray(vertex_array_object);
	}

	void my_application_3::shutdown()
	{
		glDeleteProgram(rendering_program);
		glDeleteVertexArrays(1, &vertex_array_object);
	}

	void my_application_3::render(double currentTime)
	{
		colorVectors[0] = (GLfloat)sin(currentTime) * 0.5f + 0.5f;
		colorVectors[1] = (GLfloat)cos(currentTime) * 0.5f + 0.5f;
		colorVectors[2] = (GLfloat)sin(currentTime) * 0.5f + 0.5f;
		colorVectors[3] = (GLfloat)1.0f;
		glClearBufferfv(GL_COLOR, 0, colorVectors);

		glUseProgram(rendering_program);
		glDrawArrays(GL_TRIANGLES, 0, 12);
	}
};