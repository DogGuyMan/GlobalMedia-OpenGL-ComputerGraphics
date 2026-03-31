#include <sb7.h>
#include <shader.h>

#include <vmath.h>
#include <vector>
#include <iostream>

namespace chapter6
{

	class my_application : public sb7::application
	{
	private:
		GLuint programAddr = 0;
		GLuint vaoAddr = 0;
		GLuint vboAddr = 0;

		const char *vs_path = "./shaders/cube_vs.glsl";
		const char *fs_path = "./shaders/cube_fs.glsl";

		const vmath::vec4 tries[2][4] = {
			{
				vmath::vec4(0.5f - 0.25f, 0.5f - 0.25f, 0.5f - 0.25f, 1.0f),
				vmath::vec4(0.0f - 0.25f, 0.5f - 0.25f, 0.5f - 0.25f, 1.0f),
				vmath::vec4(0.0f - 0.25f, 0.0f - 0.25f, 0.5f - 0.25f, 1.0f),
				vmath::vec4(0.5f - 0.25f, 0.0f - 0.25f, 0.5f - 0.25f, 1.0f)
			},
			{
				vmath::vec4(0.5f - 0.25f, 0.5f - 0.25f, 0.0f - 0.25f, 1.0f),
				vmath::vec4(0.0f - 0.25f, 0.5f - 0.25f, 0.0f - 0.25f, 1.0f),
				vmath::vec4(0.0f - 0.25f, 0.0f - 0.25f, 0.0f - 0.25f, 1.0f),
				vmath::vec4(0.5f - 0.25f, 0.0f - 0.25f, 0.0f - 0.25f, 1.0f)
			},
		};

		const vmath::vec4 cube_colors[6] = {
			vmath::vec4(1.0f, 0.0f, 0.0f, 1.0f),
			vmath::vec4(0.0f, 1.0f, 0.0f, 1.0f),
			vmath::vec4(0.0f, 0.0f, 1.0f, 1.0f),
			vmath::vec4(1.0f, 0.0f, 1.0f, 1.0f),
			vmath::vec4(1.0f, 1.0f, 0.0f, 1.0f),
			vmath::vec4(0.0f, 1.0f, 1.0f, 1.0f),
		};

		const vmath::vec4 cube_vertexs[6][12] = {
			{
				tries[0][0], cube_colors[0],
				tries[0][1], cube_colors[0],
				tries[0][2], cube_colors[0],
				tries[0][0], cube_colors[0],
				tries[0][2], cube_colors[0],
				tries[0][3], cube_colors[0]
			},
			{
				tries[1][0], cube_colors[1],
				tries[0][0], cube_colors[1],
				tries[0][3], cube_colors[1],
				tries[1][0], cube_colors[1],
				tries[0][3], cube_colors[1],
				tries[1][3], cube_colors[1]
			},
			{
				tries[1][1], cube_colors[2],
				tries[1][0], cube_colors[2],
				tries[1][3], cube_colors[2],
				tries[1][1], cube_colors[2],
				tries[1][3], cube_colors[2],
				tries[1][2], cube_colors[2]
			},
			{
				tries[0][1], cube_colors[3],
				tries[1][1], cube_colors[3],
				tries[1][2], cube_colors[3],
				tries[0][1], cube_colors[3],
				tries[1][2], cube_colors[3],
				tries[0][2], cube_colors[3]
			},
			{
				tries[1][0], cube_colors[4],
				tries[1][1], cube_colors[4],
				tries[0][1], cube_colors[4],
				tries[1][0], cube_colors[4],
				tries[0][1], cube_colors[4],
				tries[0][0], cube_colors[4]
			},
			{
				tries[0][3], cube_colors[5],
				tries[0][2], cube_colors[5],
				tries[1][2], cube_colors[5],
				tries[0][3], cube_colors[5],
				tries[1][2], cube_colors[5],
				tries[1][3], cube_colors[5]
			}
		};

		GLuint create_shader(GLenum shader_type, const char *shader_path)
		{
			GLuint shader_addr = sb7::shader::load(shader_path, shader_type, true);
			if (shader_addr == 0)
				std::cerr << "쉐이더 로드 실패 : " << shader_path << std::endl;
			return shader_addr;
		}

		GLuint create_program()
		{
			GLuint program_addr = glCreateProgram();
			GLuint vshader = create_shader(GL_VERTEX_SHADER, vs_path);
			GLuint fshader = create_shader(GL_FRAGMENT_SHADER, fs_path);

			glAttachShader(program_addr, vshader);
			glAttachShader(program_addr, fshader);

			glLinkProgram(program_addr);

			glDeleteShader(vshader);
			glDeleteShader(fshader);
			return program_addr;
		}

	public:
		virtual void startup() override
		{
			programAddr = create_program();

			glGenVertexArrays(1, &vaoAddr);
			glBindVertexArray(vaoAddr);

			glGenBuffers(1, &vboAddr);
			glBindBuffer(GL_ARRAY_BUFFER, vboAddr);
			glBufferData(GL_ARRAY_BUFFER,
				     sizeof(cube_vertexs),
				     cube_vertexs,
				     GL_STATIC_DRAW);

			// 인터리브 레이아웃
			GLuint stride = 2 * sizeof(vmath::vec4);

			// attribute 0: 위치 (offset 0)
			glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void *)0);
			glEnableVertexAttribArray(0);
			// attribute 1: 색상 (offset 16)
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void *)sizeof(vmath::vec4));
			glEnableVertexAttribArray(1);

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		}

		virtual void render(double currentTime) override
		{
			glClear(GL_COLOR_BUFFER_BIT);
			glEnable(GL_CULL_FACE);

			float angle = vmath::degrees((float)currentTime * 2);
			vmath::mat4 rotMat = vmath::rotate(angle, 0.0f, 1.0f, 0.0f);

			vmath::mat4 translateMat = vmath::translate<float>((float)cos(currentTime * 2), 0.0f, (float)sin(currentTime * 2));

			vmath::vec3 camPos = vmath::vec3(0.0f, 1.0f, 3.0f);
			vmath::vec3 targetPos = vmath::vec3(0.0f, 0.0f, 0.0f);
			vmath::vec3 worldUp = vmath::vec3(0.0f, 1.0f, 0.0f);
			vmath::mat4 lookatMat = vmath::lookat(camPos, targetPos, worldUp);
			vmath::mat4 projM = vmath::perspective(50.0f, (float)info.windowWidth / info.windowHeight, 0.1f, 1000.0f);

			glUseProgram(programAddr);
			GLuint translateMatLocation = glGetUniformLocation(programAddr, "translateMat");
			GLuint rotMatLocation = glGetUniformLocation(programAddr, "rotMat");
			GLuint lookatMatLocation = glGetUniformLocation(programAddr, "lookatMat");
			GLuint projMatLocation = glGetUniformLocation(programAddr, "projMat");
			glUniformMatrix4fv(translateMatLocation, 1, GL_FALSE, translateMat);
			glUniformMatrix4fv(rotMatLocation, 1, GL_FALSE, rotMat);
			glUniformMatrix4fv(lookatMatLocation, 1, GL_FALSE, lookatMat);
			glUniformMatrix4fv(projMatLocation, 1, GL_FALSE, projM);

			glBindVertexArray(vaoAddr);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}

		virtual void shutdown() override
		{

		}
	};
}

DECLARE_MAIN(chapter6::my_application);
