#include <GL/glcorearb.h>
#include <sb7.h>
#include <GL/gl3w.h>
#include <vmath.h>

namespace SJH::quiz1
{

	class my_application : public sb7::application
	{
	protected:
		GLfloat colorVectors[4] = {
		    0,
		};
		GLuint rendering_program;
		GLuint vertex_array_object;

	public:
		virtual GLuint compile_shaders(void);
		virtual void startup() override;
		virtual void shutdown() override;
		virtual void render(double currentTime) override;
	};
};

namespace SJH::quiz1
{
	GLuint my_application::compile_shaders()
	{
		// 쉐이더 포인터
		GLuint vertex_shader;
		const GLchar *vertex_shader_source[] = {
		    R"(
#version 410 core
void main(void)
{
const vec4 vertices[12] = vec4[12](
    vec4(0.25	-0.25, -0.25	+ 0.25, 0.5, 1.0),
    vec4(-0.25	-0.25, 0.25	+ 0.25, 0.5, 1.0),
    vec4(0.25	-0.25, 0.25	+ 0.25, 0.5, 1.0),
    vec4(0.25	-0.25, -0.25	+ 0.25, 0.5, 1.0),
    vec4(0.75	-0.25, -0.25	+ 0.25, 0.5, 1.0),
    vec4(0.75	-0.25, 0.25	+ 0.25, 0.5, 1.0),
    vec4(0.25	-0.25, -0.25	+ 0.25, 0.5, 1.0),
    vec4(0.75	-0.25, -0.75	+ 0.25, 0.5, 1.0),
    vec4(0.25	-0.25, -0.75	+ 0.25, 0.5, 1.0),
    vec4(0.25	-0.25, -0.25	+ 0.25, 0.5, 1.0),
    vec4(-0.25	-0.25, -0.75	+ 0.25, 0.5, 1.0),
    vec4(-0.25	-0.25, -0.25	+ 0.25, 0.5, 1.0)
);
gl_Position = vertices[gl_VertexID];
}
		    )"};
		vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex_shader, 1, vertex_shader_source, NULL);
		glCompileShader(vertex_shader);

		// 쉐이더 포인터
		GLuint fragment_shader;
		const GLchar *fragment_shader_source[] = {
		    R"(
#version 410 core
out vec4 color;

void main(void) 
{
    color = vec4(0.0, 0.8, 1.0, 1.0);
}
    		)"};

		fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment_shader, 1, fragment_shader_source, NULL);
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

	void my_application::startup()
	{
		rendering_program = compile_shaders();
		glGenVertexArrays(1, &vertex_array_object);
		glBindVertexArray(vertex_array_object);
	}

	void my_application::render(double currentTime)
	{
		colorVectors[0] = (GLfloat)sin(currentTime) * 0.5f + 0.5f;
		colorVectors[1] = (GLfloat)cos(currentTime) * 0.5f + 0.5f;
		colorVectors[2] = (GLfloat)sin(currentTime) * 0.5f + 0.5f;
		colorVectors[3] = (GLfloat)1.0f;
		glClearBufferfv(GL_COLOR, 0, colorVectors);

		glUseProgram(rendering_program);

		glPointSize(40.0f);
		glDrawArrays(GL_TRIANGLES, 0, 12);
	}

	void my_application::shutdown()
	{
		glDeleteVertexArrays(1, &vertex_array_object);
		glDeleteProgram(rendering_program);
	}
};

DECLARE_MAIN(SJH::quiz1::my_application);
