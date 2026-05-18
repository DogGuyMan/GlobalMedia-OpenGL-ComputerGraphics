#include <sb7.h>
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
			for (const auto &source : sources)
			{
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

		// uniform mat4 전달
		void SetUniformMat4(const char *name, const vmath::mat4 &mat) const
		{
			GLint loc = glGetUniformLocation(programAddr, name);
			glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
		}

		virtual void Draw(float currentTime, const vmath::mat4 &vpMatrix) const
		{
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	};

	class triangle_program : public program_base
	{
	private:
		static constexpr const GLchar *vs = R"(
			#version 410 core

			uniform mat4 mvp;

			out VS_OUT {
				vec4 color;
			} vs_out;

			void main(void) {
				const vec4 triangle[3] = vec4[3] (
					vec4(0.0, 0.866, 0.0, 1.0),
					vec4(-0.5, -0.433, 0.0, 1.0),
					vec4(0.5, -0.433, 0.0, 1.0)
				);

				mat3 tintMat = mat3(1.0);
				gl_Position = mvp * triangle[gl_VertexID];
				vs_out.color = vec4(tintMat[gl_VertexID], 1.0);
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
		triangle_program() : program_base({{GL_VERTEX_SHADER, vs},
						   {GL_FRAGMENT_SHADER, fs}}) {}
		virtual void Draw(float currentTime, const vmath::mat4 &vpMatrix) const override
		{
			SetUniformMat4("mvp", vpMatrix);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
	};

	// 변환 인터페이스
	class ITransformable
	{
	public:
		virtual ~ITransformable() = default;
		virtual void translate(const vmath::vec3& delta) = 0;
		virtual void rotate(float angleDeg, const vmath::vec3& axis) = 0;
		virtual void scale(const vmath::vec3& factor) = 0;
	};

	// 룩엣 카메라
	class camera : public ITransformable
	{
	private:
		vmath::vec3 eye;
		vmath::vec3 center;
		vmath::vec3 up;
		float fov;
		float aspect;
		float nearPlane;
		float farPlane;

	public:
		camera() :	eye(0.0f, 0.0f, 3.0f), 
				center(0.0f, 0.0f, 0.0f), 
				up(0.0f, 1.0f, 0.0f), 
				fov(50.0f), aspect(800.0f / 600.0f), 
				nearPlane(0.1f), 
				farPlane(100.0f)
		{
		}

		// View * Projection 행렬 반환
		vmath::mat4 getVPMatrix() const
		{
			vmath::mat4 view = vmath::lookat(eye, center, up);
			vmath::mat4 proj = vmath::perspective(fov, aspect, nearPlane, farPlane);
			return proj * view;
		}

		// 카메라 전방 벡터
		vmath::vec3 forward() const
		{
			return vmath::normalize(center - eye);
		}

		// 카메라 오른쪽 벡터
		vmath::vec3 right() const
		{
			return vmath::normalize(vmath::cross(forward(), up));
		}

		// ITransformable 구현: eye와 center를 함께 이동
		void translate(const vmath::vec3& delta) override
		{
			eye += delta;
			center += delta;
		}

		// ITransformable 구현: eye 기준으로 center를 회전
		void rotate(float angleDeg, const vmath::vec3& axis) override
		{
			vmath::vec4 dir(center[0]-eye[0], center[1]-eye[1], center[2]-eye[2], 0.0f);
			vmath::vec4 r = dir * vmath::rotate(angleDeg, axis);
			center = eye + vmath::vec3(r[0], r[1], r[2]);
		}

		// ITransformable 구현: FOV 조절 (줌)
		void scale(const vmath::vec3& factor) override
		{
			fov *= factor[0];
			if (fov < 10.0f) fov = 10.0f;
			if (fov > 120.0f) fov = 120.0f;
		}
	};

	class my_application : public sb7::application
	{
	private:
		std::vector<std::unique_ptr<program_base>> programs;
		GLfloat background_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		camera cam;

	public:
		virtual void startup() override
		{
			programs.push_back(std::make_unique<triangle_program>());
		}

		virtual void render(double currentTime) override
		{
			glClearBufferfv(GL_COLOR, 0, background_color);
			vmath::mat4 vpMatrix = cam.getVPMatrix();
			for (const auto &program : programs)
			{
				program->UseProgram();
				program->Draw(currentTime, vpMatrix);
			}
		}

		// 키보드 입력 처리 (sb7 콜백)
		virtual void onKey(int key, int action) override
		{
			const float moveSpeed = 0.05f;
			const float rotSpeed = 2.0f;

			if (action == GLFW_PRESS || action == GLFW_REPEAT)
			{
				// 이동 (translate)
				if (key == GLFW_KEY_W)
					cam.translate(cam.forward() * moveSpeed);
				if (key == GLFW_KEY_S)
					cam.translate(cam.forward() * -moveSpeed);
				if (key == GLFW_KEY_A)
					cam.translate(cam.right() * -moveSpeed);
				if (key == GLFW_KEY_D)
					cam.translate(cam.right() * moveSpeed);

				// 회전 (rotate) — 좌우: Y축, 상하: 카메라 right축
				if (key == GLFW_KEY_LEFT)
					cam.rotate(rotSpeed, vmath::vec3(0.0f, 1.0f, 0.0f));
				if (key == GLFW_KEY_RIGHT)
					cam.rotate(-rotSpeed, vmath::vec3(0.0f, 1.0f, 0.0f));
				if (key == GLFW_KEY_UP)
					cam.rotate(rotSpeed, cam.right());
				if (key == GLFW_KEY_DOWN)
					cam.rotate(-rotSpeed, cam.right());

			}
		}

		virtual void shutdown() override
		{
			for (auto &program : programs)
				program.reset();
		}
	};
}

DECLARE_MAIN(exercise3::my_application);