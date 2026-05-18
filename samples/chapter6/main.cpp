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
		GLuint programAddrs[2] = {
		    0,
		};
		GLuint vaoAddrs[2] = {
		    0,
		};
		GLuint vboAddrs[2] = {
		    0,
		};

		const char *vs_path[2] = {
		    "./shaders/pinwheel_vs.glsl",
		    "./shaders/stick_vs.glsl"};

		const char *fs_path[2] = {
		    "./shaders/pinwheel_fs.glsl",
		    "./shaders/stick_fs.glsl"};

		const vmath::vec4 pinwheel_flag_vertexs[3] = {
		    vmath::vec4(0.0, 0.0, 0.5, 1.0),
		    vmath::vec4(0.0, 0.5, 0.5, 1.0),
		    vmath::vec4(-0.5, 0.5, 0.5, 1.0)};

		const vmath::vec4 pinwheel_flag_colors[3] = {
		    vmath::vec4(1.0, 0.0, 0.0, 1.0),
		    vmath::vec4(0.0, 1.0, 0.0, 1.0),
		    vmath::vec4(0.0, 0.0, 1.0, 1.0)};

		const vmath::vec4 stick_vertexs[6] = {
		    vmath::vec4(0.01, 0.0, 0.5, 1.0),
		    vmath::vec4(-0.01, 0.0, 0.5, 1.0),
		    vmath::vec4(-0.01, -0.5, 0.5, 1.0),
		    vmath::vec4(0.01, 0.0, 0.5, 1.0),
		    vmath::vec4(-0.01, -0.5, 0.5, 1.0),
		    vmath::vec4(0.01, -0.5, 0.5, 1.0)};

		const vmath::vec4 stick_color = vmath::vec4(1.0, 0.0, 1.0, 1.0);

		std::vector<vmath::vec4> pinwheel_buffer_data;
		std::vector<vmath::vec4> stick_buffer_data;

		// lookAt 행렬 계산
		vmath::mat4 calc_lookat(vmath::vec3 camPos, vmath::vec3 targetPos, vmath::vec3 worldUp)
		{
			vmath::vec3 dirVec = vmath::normalize(camPos - targetPos);
			vmath::vec3 rightVec = vmath::normalize(vmath::cross(worldUp, dirVec));
			vmath::vec3 camUpVec = vmath::cross(dirVec, rightVec);

			vmath::mat4 lookRotate = vmath::mat4(
			    vmath::vec4(rightVec[0], camUpVec[0], dirVec[0], 0.0f),
			    vmath::vec4(rightVec[1], camUpVec[1], dirVec[1], 0.0f),
			    vmath::vec4(rightVec[2], camUpVec[2], dirVec[2], 0.0f),
			    vmath::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			vmath::mat4 lookMove = vmath::mat4(
			    vmath::vec4(1.0f, 0.0f, 0.0f, 0.0f),
			    vmath::vec4(0.0f, 1.0f, 0.0f, 0.0f),
			    vmath::vec4(0.0f, 0.0f, 1.0f, 0.0f),
			    vmath::vec4(-camPos[0], -camPos[1], -camPos[2], 1.0f));
			
			return lookRotate * lookMove;
		}

		// perspective projection 행렬 계산
		vmath::mat4 calc_perspective(float left, float right, float bottom, float top, float nearVal, float farVal)
		{
			return vmath::mat4(
			    vmath::vec4((2 * nearVal) / (right - left), 0.0f, 0.0f, 0.0f),
			    vmath::vec4(0.0f, (2 * nearVal) / (top - bottom), 0.0f, 0.0f),
			    vmath::vec4((right + left) / (right - left), (top + bottom) / (top - bottom), (nearVal + farVal) / (nearVal - farVal), -1.0f),
			    vmath::vec4(0.0f, 0.0f, (2 * nearVal * farVal) / (nearVal - farVal), 0.0f));
		}

		GLuint create_shader(GLenum shader_type, const char *shader_path)
		{
			GLuint shader_addr = sb7::shader::load(shader_path, shader_type, true);
			if (shader_addr == 0)
				std::cerr << "쉐이더 로드 실패 : " << shader_path << std::endl;
			return shader_addr;
		}

		GLuint create_program(int p_idx)
		{
			GLuint program_addr = glCreateProgram();
			GLuint vshader = create_shader(GL_VERTEX_SHADER, vs_path[p_idx]);
			GLuint fshader = create_shader(GL_FRAGMENT_SHADER, fs_path[p_idx]);

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
			for (int i = 0; i < 2; i++)
				programAddrs[i] = create_program(i);

			glGenVertexArrays(2, vaoAddrs);

			auto rotateMat = vmath::rotate(90.0f, 0.0f, 0.0f, 1.0f);
			vmath::mat4 currentMat = vmath::mat4::identity();
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					pinwheel_buffer_data.push_back(pinwheel_flag_vertexs[j] * currentMat);
					pinwheel_buffer_data.push_back(pinwheel_flag_colors[j]);
				}
				currentMat = currentMat * rotateMat;
			}

			// pinwheel VAO + VBO 설정
			{
				glBindVertexArray(vaoAddrs[0]);
				glGenBuffers(1, &vboAddrs[0]);
				glBindBuffer(GL_ARRAY_BUFFER, vboAddrs[0]);
				glBufferData(GL_ARRAY_BUFFER,
					     pinwheel_buffer_data.size() * sizeof(vmath::vec4),
					     pinwheel_buffer_data.data(),
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

			for (int i = 0; i < 6; i++)
			{
				stick_buffer_data.push_back(stick_vertexs[i]);
				stick_buffer_data.push_back(stick_color);
			}
			// stic VAO + VBO 설정
			{

				glBindVertexArray(vaoAddrs[1]);
				glGenBuffers(1, &vboAddrs[1]);
				glBindBuffer(GL_ARRAY_BUFFER, vboAddrs[1]);
				glBufferData(GL_ARRAY_BUFFER,
					     stick_buffer_data.size() * sizeof(vmath::vec4),
					     stick_buffer_data.data(), GL_STATIC_DRAW);
				GLuint stride = 2 * sizeof(vmath::vec4);
				// attribute 0: 위치 (offset 0)
				glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void *)0);
				glEnableVertexAttribArray(0);

				// attribute 1: 위치 (offset 1)
				glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void *)(sizeof(vmath::vec4)));
				glEnableVertexAttribArray(1);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				glBindVertexArray(0);
			}
		}

		virtual void render(double currentTime) override
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// 회전 행렬 (시간에 따라 회전)
			vmath::mat4 rotMat = vmath::rotate(vmath::degrees((float)currentTime), 0.0f, 0.0f, 1.0f);

			// 이동 행렬
			vmath::mat4 translateMat = vmath::mat4(
			    vmath::vec4(1.0f, 0.0f, 0.0f, 0.0f),
			    vmath::vec4(0.0f, 1.0f, 0.0f, 0.0f),
			    vmath::vec4(0.0f, 0.0f, 1.0f, 0.0f),
			    vmath::vec4(cosf(currentTime) * 0.5f, 0.0f, 0.0f, 1.0f));

			// lookAt 행렬
			vmath::vec3 camPos = vmath::vec3(0.2f, 0.3f, 0.8f);
			vmath::vec3 targetPos = vmath::vec3(0.0f, 0.0f, 0.5f);
			vmath::vec3 worldUp = vmath::vec3(0.0f, 1.0f, 0.0f);
			vmath::mat4 lookatMat = calc_lookat(camPos, targetPos, worldUp);

			// projection 행렬
			vmath::mat4 projMat = calc_perspective(
			    -0.1f, 0.1f,
			    -0.07f, 0.07f,
			    0.1f, 10.0f);

			// stick 그리기 (회전 없이 이동 + lookAt + projection)
			{
				glUseProgram(programAddrs[1]);

				GLuint translateMatLocation = glGetUniformLocation(programAddrs[1], "translateMat");
				GLuint lookatMatLocation = glGetUniformLocation(programAddrs[1], "lookatMat");
				GLuint projMatLocation = glGetUniformLocation(programAddrs[1], "projMat");
				glUniformMatrix4fv(translateMatLocation, 1, GL_FALSE, translateMat);
				glUniformMatrix4fv(lookatMatLocation, 1, GL_FALSE, lookatMat);
				glUniformMatrix4fv(projMatLocation, 1, GL_FALSE, projMat);
				glBindVertexArray(vaoAddrs[1]);
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
			
			// pinwheel 그리기
			{
				glUseProgram(programAddrs[0]);

				GLuint rotMatLocation = glGetUniformLocation(programAddrs[0], "rotMat");
				GLuint translateMatLocation = glGetUniformLocation(programAddrs[0], "translateMat");
				GLuint lookatMatLocation = glGetUniformLocation(programAddrs[0], "lookatMat");
				GLuint projMatLocation = glGetUniformLocation(programAddrs[0], "projMat");

				glUniformMatrix4fv(rotMatLocation, 1, GL_FALSE, rotMat);
				glUniformMatrix4fv(translateMatLocation, 1, GL_FALSE, translateMat);
				glUniformMatrix4fv(lookatMatLocation, 1, GL_FALSE, lookatMat);
				glUniformMatrix4fv(projMatLocation, 1, GL_FALSE, projMat);
				glBindVertexArray(vaoAddrs[0]);
				glDrawArrays(GL_TRIANGLES, 0, 12);
			}
			
		}

		virtual void shutdown() override
		{
			for (int i = 0; i < 2; i++) {
				glDeleteProgram(programAddrs[i]);
				glDeleteVertexArrays(1, &vaoAddrs[i]);
			}
		}
	};
}

DECLARE_MAIN(chapter6::my_application);