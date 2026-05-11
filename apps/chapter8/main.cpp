// sb6.h 헤더 파일을 포함시킨다.
#include "GL/gl3w.h"
#include <ostream>
#include <sb7.h>
#include <shader.h>
#include <vector>
#include <vmath.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <string>

#define MAC_WINE_TEST
#include "diagnostics/gl_log.h"       // diag::
#include "diagnostics/gl_state_log.h" // diag::
namespace diag = SJH::Diagnostics;    // diag::

#define NUM_POINT_LIGHTS 2

// sb6::application을 상속받는다.
class my_application : public sb7::application
{
  public:
	// ───────── 라이트 / 머티리얼 데이터 구조 ─────────
	struct Light
	{
		vmath::vec3 position;
		vmath::vec3 ambient;
		vmath::vec3 diffuse;
		vmath::vec3 specular;
	};

	struct DirLight
	{
		vmath::vec3 direction;
		vmath::vec3 ambient, diffuse, specular;
	};

	struct PointLight
	{
		vmath::vec3 position;
		float c1, c2;
		vmath::vec3 ambient, diffuse, specular;
	};

	struct SpotLight
	{
		vmath::vec3 position;
		vmath::vec3 direction;
		float cutOff, outerCutOff;
		float c1, c2;
		vmath::vec3 ambient, diffuse, specular;
	};

	struct Material
	{
		GLuint diffuseTexture;
		GLuint specularTexture;
		float shininess;
	};

	static void UniformsSetDirLight(GLuint program, const std::string &prefix, const DirLight &light)
	{
		glUniform3fv(glGetUniformLocation(program, (prefix + ".direction").c_str()), 1, light.direction);
		glUniform3fv(glGetUniformLocation(program, (prefix + ".ambient").c_str()), 1, light.ambient);
		glUniform3fv(glGetUniformLocation(program, (prefix + ".diffuse").c_str()), 1, light.diffuse);
		glUniform3fv(glGetUniformLocation(program, (prefix + ".specular").c_str()), 1, light.specular);
	}

	static void UniformsSetPointLight(GLuint program, const std::string &prefix, const PointLight &light)
	{
		glUniform3fv(glGetUniformLocation(program, (prefix + ".position").c_str()), 1, light.position);
		glUniform3fv(glGetUniformLocation(program, (prefix + ".ambient").c_str()), 1, light.ambient);
		glUniform3fv(glGetUniformLocation(program, (prefix + ".diffuse").c_str()), 1, light.diffuse);
		glUniform3fv(glGetUniformLocation(program, (prefix + ".specular").c_str()), 1, light.specular);
		glUniform1f(glGetUniformLocation(program, (prefix + ".c1").c_str()), light.c1);
		glUniform1f(glGetUniformLocation(program, (prefix + ".c2").c_str()), light.c2);
	}

	static void UniformsSetStopLight(GLuint program, const std::string &prefix, const SpotLight &light)
	{
		glUniform3fv(glGetUniformLocation(program, (prefix + ".position").c_str()), 1, light.position);
		glUniform3fv(glGetUniformLocation(program, (prefix + ".direction").c_str()), 1, light.direction);
		glUniform1f(glGetUniformLocation(program, (prefix + ".cutOff").c_str()), light.cutOff);
		glUniform1f(glGetUniformLocation(program, (prefix + ".outerCutOff").c_str()), light.outerCutOff);
		glUniform1f(glGetUniformLocation(program, (prefix + ".c1").c_str()), light.c1);
		glUniform1f(glGetUniformLocation(program, (prefix + ".c2").c_str()), light.c2);
		glUniform3fv(glGetUniformLocation(program, (prefix + ".ambient").c_str()), 1, light.ambient);
		glUniform3fv(glGetUniformLocation(program, (prefix + ".diffuse").c_str()), 1, light.diffuse);
		glUniform3fv(glGetUniformLocation(program, (prefix + ".specular").c_str()), 1, light.specular);
	}

	static void UniformsSeMaterial(GLuint program, const std::string &prefix, const Material &material, int diffuseUnit, int specularUnit)
	{
		glUniform1i(glGetUniformLocation(program, (prefix + ".diffuse").c_str()), diffuseUnit);
		glUniform1i(glGetUniformLocation(program, (prefix + ".specular").c_str()), specularUnit);
		glUniform1f(glGetUniformLocation(program, (prefix + ".shininess").c_str()), material.shininess);
	}

	// 쉐이더 프로그램 컴파일한다.
	GLuint compile_shader(const char *vs_file, const char *fs_file)
	{
		// 버텍스 쉐이더를 생성하고 컴파일한다.
		GLuint vertex_shader = sb7::shader::load(vs_file, GL_VERTEX_SHADER);

		diag::GLObjectLog::CheckShaderCompile(vertex_shader, vs_file); // diag::

		// 프래그먼트 쉐이더를 생성하고 컴파일한다.
		GLuint fragment_shader = sb7::shader::load(fs_file, GL_FRAGMENT_SHADER);
		diag::GLObjectLog::CheckShaderCompile(fragment_shader, fs_file); // diag::

		// 프로그램을 생성하고 쉐이더를 Attach시키고 링크한다.
		GLuint program = glCreateProgram();
		glAttachShader(program, vertex_shader);
		glAttachShader(program, fragment_shader);
		glLinkProgram(program);
		// 링크 실패 시 uniform/attribute location 이 전부 무효해지므로 필수 검사.
		diag::GLObjectLog::CheckProgramLink(program, fs_file); // diag::

		// 이제 프로그램이 쉐이더를 소유하므로 쉐이더를 삭제한다.
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);

		return program;
	}

	void load_texture(GLuint textureID, char const *filename, GLenum glColorFormat = GL_RGB, GLenum inputColorFormat = GL_RGB)
	{
		// 텍스처 객체 만들고 바인딩
		glBindTexture(GL_TEXTURE_2D, textureID);

		// 텍스처 이미지 로드하기
		int width, height, nrChannels;
		unsigned char *data = stbi_load(filename, &width, &height, &nrChannels, 0);

		if (data)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, glColorFormat, width, height, 0, inputColorFormat, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		stbi_image_free(data);

		// 텍스처 샘플링/필터링 설정
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	virtual void init()
	{
		sb7::application::init();
#ifdef MAC_WINE_TEST
		info.majorVersion = 4;
		info.minorVersion = 1;
#endif
	}

	// 애플리케이션 초기화 수행한다.
	virtual void startup()
	{
		// 쉐이더 프로그램 컴파일 및 연결
		shader_programs[0] = compile_shader("./resources/shaders/basic_texturing_vs.glsl", "./resources/shaders/basic_texturing_fs.glsl");
		shader_programs[1] = compile_shader("./resources/shaders/basic_lighting_vs.glsl", "./resources/shaders/basic_lighting_fs.glsl");
		shader_programs[2] = compile_shader("./resources/shaders/simple_color_vs.glsl", "./resources/shaders/simple_color_fs.glsl");

		// VAO, VBO, EBO, texture 생성
		glGenVertexArrays(3, VAOs);
		diag::GLDebug::CheckGLGenVertexArrays(); // diag::
		glGenBuffers(3, VBOs);
		diag::GLDebug::CheckGLGenBuffers(VBOs[0]); // diag::
		glGenBuffers(2, EBOs);
		diag::GLDebug::CheckGLGenBuffers(EBOs[0]); // diag::
		glGenTextures(3, textures);

		stbi_set_flip_vertically_on_load(true);

		glBindVertexArray(VAOs[0]);
		diag::GLDebug::CheckGLBindVertexArray(VAOs[0]); // diag::

		// VBO를 생성하여 vertices 값들을 복사
		glBindBuffer(GL_ARRAY_BUFFER, VBOs[0]);
		diag::GLDebug::CheckGLBindBuffer(VBOs[0]); // diag::
		glBufferData(GL_ARRAY_BUFFER, sizeof(floor_vertices), floor_vertices, GL_STATIC_DRAW);
		diag::GLDebug::CheckGLBufferData(static_cast<GLint>(sizeof(floor_vertices))); // diag::

		// VBO를 나누어서 각 버텍스 속성으로 연결
		// 위치 속성 (location = 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
		diag::GLDebug::CheckGLVertexAttribPointer({8 * static_cast<GLsizei>(sizeof(float))}); // diag::
		glEnableVertexAttribArray(0);
		diag::GLDebug::CheckGLEnableVertexAttribArray(0); // diag::
		// 컬러 속성 (location = 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
		diag::GLDebug::CheckGLVertexAttribPointer({8 * static_cast<GLsizei>(sizeof(float))}); // diag::
		glEnableVertexAttribArray(1);
		diag::GLDebug::CheckGLEnableVertexAttribArray(1); // diag::
		// 텍스처 좌표 속성 (location = 2)
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
		diag::GLDebug::CheckGLVertexAttribPointer({8 * static_cast<GLsizei>(sizeof(float))}); // diag::
		glEnableVertexAttribArray(2);
		diag::GLDebug::CheckGLEnableVertexAttribArray(2); // diag::

		// EBO를 생성하고 indices 값들을 복사
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[0]);
		diag::GLDebug::CheckGLBindBuffer(EBOs[0]); // diag::
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(floor_indices), floor_indices, GL_STATIC_DRAW);
		diag::GLDebug::CheckGLBufferData(static_cast<GLint>(sizeof(floor_indices))); // diag::

		// VBO 및 버텍스 속성을 다 했으니 VBO와 VAO를 unbind한다.
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		// 텍스처 객체 만들고 바인딩
		glBindTexture(GL_TEXTURE_2D, textures[0]);

		// 텍스처 이미지 로드하기
		load_texture(textures[0], "./resources/textures/wall.jpg", GL_RGB, GL_RGB);

		glBindVertexArray(VAOs[1]);
		diag::GLDebug::CheckGLBindVertexArray(VAOs[1]); // diag::

		// 박스 10개 포지션 정의
		boxPositions.push_back(vmath::vec3(0.0f, 0.0f, 0.0f));
		boxPositions.push_back(vmath::vec3(2.0f, 5.0f, -15.0f));
		boxPositions.push_back(vmath::vec3(-1.5f, -2.2f, -2.5f));
		boxPositions.push_back(vmath::vec3(-3.8f, -2.0f, -12.3f));
		boxPositions.push_back(vmath::vec3(2.4f, -0.4f, -3.5f));
		boxPositions.push_back(vmath::vec3(-1.7f, 3.0f, -7.5f));
		boxPositions.push_back(vmath::vec3(1.3f, -2.0f, -2.5f));
		boxPositions.push_back(vmath::vec3(1.5f, 2.0f, -2.5f));
		boxPositions.push_back(vmath::vec3(1.5f, 0.2f, -1.5f));
		boxPositions.push_back(vmath::vec3(-1.3f, 1.0f, -1.5f));

		// VBO를 생성하여 vertices 값들을 복사
		glBindBuffer(GL_ARRAY_BUFFER, VBOs[1]);
		diag::GLDebug::CheckGLBindBuffer(VBOs[1]); // diag::
		glBufferData(GL_ARRAY_BUFFER, sizeof(box_vertices), box_vertices, GL_STATIC_DRAW);
		diag::GLDebug::CheckGLBufferData(static_cast<GLint>(sizeof(box_vertices))); // diag::

		// VBO를 나누어서 각 버텍스 속성으로 연결
		// 위치 속성 (location = 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)0);
		diag::GLDebug::CheckGLVertexAttribPointer({11 * static_cast<GLsizei>(sizeof(float))}); // diag::
		glEnableVertexAttribArray(0);
		diag::GLDebug::CheckGLEnableVertexAttribArray(0); // diag::
		// 컬러 속성 (location = 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(3 * sizeof(float)));
		diag::GLDebug::CheckGLVertexAttribPointer({11 * static_cast<GLsizei>(sizeof(float))}); // diag::
		glEnableVertexAttribArray(1);
		diag::GLDebug::CheckGLEnableVertexAttribArray(1); // diag::
		// 텍스처 좌표 속성 (location = 2)
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(6 * sizeof(float)));
		diag::GLDebug::CheckGLVertexAttribPointer({11 * static_cast<GLsizei>(sizeof(float))}); // diag::
		glEnableVertexAttribArray(2);
		diag::GLDebug::CheckGLEnableVertexAttribArray(2); // diag::
		// 노멀 속성 (location = 3)
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(8 * sizeof(float)));
		diag::GLDebug::CheckGLVertexAttribPointer({11 * static_cast<GLsizei>(sizeof(float))}); // diag::
		glEnableVertexAttribArray(3);
		diag::GLDebug::CheckGLEnableVertexAttribArray(3); // diag::

		// Diffuse Map 이미지 로드 및 적용
		load_texture(textures[1], "./resources/textures/container2.png", GL_RGB, GL_RGBA);
		// Specular Map 이미지 로드 및 적용
		load_texture(textures[2], "./resources/textures/container2_specular.png", GL_RGB, GL_RGBA);

		// VBO 및 버텍스 속성을 다 했으니 VBO와 VAO를 unbind한다.
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		//  세 번째 객체 정의 : 피라미드
		glBindVertexArray(VAOs[2]);
		diag::GLDebug::CheckGLBindVertexArray(VAOs[2]); // diag::
		// 피라미드 점들의 위치와 컬러, 텍스처 좌표를 정의한다.

		// VBO를 생성하여 vertices 값들을 복사
		glBindBuffer(GL_ARRAY_BUFFER, VBOs[2]);
		diag::GLDebug::CheckGLBindBuffer(VBOs[2]); // diag::
		glBufferData(GL_ARRAY_BUFFER, sizeof(pyramid_vertices), pyramid_vertices, GL_STATIC_DRAW);
		diag::GLDebug::CheckGLBufferData(static_cast<GLint>(sizeof(pyramid_vertices))); // diag::

		// VBO를 나누어서 각 버텍스 속성으로 연결
		// 위치 속성 (location = 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
		diag::GLDebug::CheckGLVertexAttribPointer({3 * static_cast<GLsizei>(sizeof(float))}); // diag::
		glEnableVertexAttribArray(0);
		diag::GLDebug::CheckGLEnableVertexAttribArray(0); // diag::

		// EBO를 생성하고 indices 값들을 복사
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[1]);
		diag::GLDebug::CheckGLBindBuffer(EBOs[1]); // diag::
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(pyramid_indices), pyramid_indices, GL_STATIC_DRAW);
		diag::GLDebug::CheckGLBufferData(static_cast<GLint>(sizeof(pyramid_indices))); // diag::

		// VBO 및 버텍스 속성을 다 했으니 VBO와 VAO를 unbind한다.
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		diag::GLObjectLog::CheckExpectedAttributes(shader_programs[1], {"pos", "color", "texCoord", "normal"}, "basic_lighting"); // diag::
		diag::GLObjectLog::CheckExpectedUniforms(shader_programs[1],                                                              // diag::
		                                         {"model", "view", "projection", "viewPos", "objectColor",                        // diag::
		                                          "light.position", "light.ambient", "light.diffuse", "light.specular",           // diag::
		                                          "material.diffuse", "material.specular"},                                       // diag::
		                                         "basic_lighting");                                                               // diag::
		diag::GLObjectLog::CheckExpectedAttributes(shader_programs[2], {"pos"}, "simple_color");                                  // diag::
		diag::GLObjectLog::CheckExpectedUniforms(shader_programs[2], {"model", "view", "projection", "color"}, "simple_color");   // diag::

		diag::GLStateLog::Dump("after startup()"); // diag::
	}

	// 애플리케이션 끝날 때 호출된다.
	virtual void shutdown()
	{
		glDeleteTextures(3, textures);
		glDeleteBuffers(2, EBOs);
		glDeleteBuffers(3, VBOs);
		glDeleteVertexArrays(3, VAOs);
		glDeleteProgram(shader_programs[0]);
		glDeleteProgram(shader_programs[1]);
		glDeleteProgram(shader_programs[2]);

		// 같은 GLuint 핸들이 재발급될 때 stale 캐시 결과를 막기 위해 진단 캐시 무효화.
		// (CheckExpectedUniforms / CheckExpectedAttributes 가 program 핸들을 키로 1회 캐시함)
		diag::GLObjectLog::InvalidateProgramCache(shader_programs[0]); // diag::
		diag::GLObjectLog::InvalidateProgramCache(shader_programs[1]); // diag::
		diag::GLObjectLog::InvalidateProgramCache(shader_programs[2]); // diag::
	}

	// 렌더링 virtual 함수를 작성해서 오버라이딩한다.
	virtual void render(double currentTime)
	{
		// currentTime = 1.46;
		// const GLfloat color[] = { (float)sin(currentTime) * 0.5f + 0.5f, (float)cos(currentTime) * 0.5f + 0.5f, 0.0f, 1.0f };
		const GLfloat black[] = {0.0f, 0.0f, 0.0f, 1.0f};
		glClearBufferfv(GL_COLOR, 0, black);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		GLint uniform_transform1 = glGetUniformLocation(shader_programs[0], "transform");
		GLint uniform_transform2 = glGetUniformLocation(shader_programs[1], "transform");

		float angle = currentTime * 100;

		// 카메라 매트릭스 계산
		float distance = 2.f;
		vmath::vec3 eye((float)cos(currentTime * 0.1f) * distance, 1.0, (float)sin(currentTime * 0.1f) * distance);
		vmath::vec3 center(0.0, 0.0, 0.0);
		vmath::vec3 up(0.0, 1.0, 0.0);
		vmath::mat4 lookAt = vmath::lookat(eye, center, up);
		float fov = 50.f;
		vmath::mat4 projM = vmath::perspective(fov, (float)info.windowWidth / info.windowHeight, 0.1f, 1000.0f);

		// 바닥 그리기
		/*
		glUseProgram(shader_programs[0]);
		glUniformMatrix4fv(glGetUniformLocation(shader_programs[0], "transform"), 1, GL_FALSE, projM*lookAt*vmath::scale(1.5f));
		glUniform1i(glGetUniformLocation(shader_programs[0], "texture1"), 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textures[0]);
		glBindVertexArray(VAOs[0]);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		*/

		// 라이팅 설정
		m_dirLight.direction = vmath::vec3(-0.2f, -1.0f, -0.3f);
		m_dirLight.ambient = vmath::vec3(0.05f, 0.05f, 0.05f);
		m_dirLight.diffuse = vmath::vec3(0.4f, 0.4f, 0.4f);
		m_dirLight.specular = vmath::vec3(0.5f, 0.5f, 0.5f);

		for(int i = 0; i < NUM_POINT_LIGHTS; i++) {
			m_pointLights[i].position = vmath::vec3((float)sin(currentTime * 0.5f), 0.25f, (float)cos(currentTime * 0.5f) * 0.7f); // (0.0f, 0.5f, 0.0f);;
			m_pointLights[i].ambient = vmath::vec3(0.05f, 0.05f, 0.05f);
			m_pointLights[i].diffuse = vmath::vec3(0.8f, 0.8f, 0.8f);
			m_pointLights[i].specular = vmath::vec3(1.0f, 1.0f, 1.0f);
			m_pointLights[i].c1 = 0.09f;
			m_pointLights[i].c2 = 0.032f;
		}

		m_spotLight.position = eye;
		m_spotLight.direction = center - eye;
		m_spotLight.cutOff = (float)cos(vmath::radians(12.5));
		m_spotLight.outerCutOff = (float)cos(vmath::radians(15.5));
		m_spotLight.c1 = 0.09f;
		m_spotLight.c2 = 0.032f;
		m_spotLight.ambient = vmath::vec3(0.0f, 0.0f, 0.0f);
		m_spotLight.diffuse = vmath::vec3(1.0f, 1.0f, 1.0f);
		m_spotLight.specular = vmath::vec3(1.0f, 1.0f, 1.0f);

		vmath::vec3 viewPos = eye;
		vmath::vec3 lightColor(1.0f, 1.0f, 1.0f);
		vmath::vec3 boxColor(1.0f, 1.0f, 1.0f);

		// 박스 그리기
		{
			vmath::mat4 transM = vmath::translate(vmath::vec3((float)sin(currentTime * 0.5f), 0.0f, (float)cos(currentTime * 0.5f) * 0.7f));
			vmath::mat4 rotateM = vmath::rotate(angle, 0.0f, 1.0f, 0.0f);

			glUseProgram(shader_programs[1]);

			static bool s_dumpedRenderState = false;                 // diag::
			if (!s_dumpedRenderState)                                // diag::
			{                                                        // diag::
				diag::GLStateLog::Dump("render(): before box draw"); // diag::
				s_dumpedRenderState = true;                          // diag::
			} // diag::

			glUniformMatrix4fv(glGetUniformLocation(shader_programs[1], "projection"), 1, GL_FALSE, projM);
			glUniformMatrix4fv(glGetUniformLocation(shader_programs[1], "view"), 1, GL_FALSE, lookAt);
			glUniformMatrix4fv(glGetUniformLocation(shader_programs[1], "model"), 1, GL_FALSE, rotateM);
			glUniform3fv(glGetUniformLocation(shader_programs[1], "objectColor"), 1, boxColor);
			glUniform3fv(glGetUniformLocation(shader_programs[1], "viewPos"), 1, viewPos);

			// 라이트/머티리얼 uniform 은 구조체 단위 헬퍼로 일괄 설정한다.
			UniformsSetDirLight(shader_programs[1], "dirLight", m_dirLight);
			for (int i = 0; i < NUM_POINT_LIGHTS; i++)
				UniformsSetPointLight(shader_programs[1], "pointLights[" + std::to_string(i) + "]", m_pointLights[i]);
			UniformsSetStopLight(shader_programs[1], "spotLight", m_spotLight);

			m_material.shininess = 32.0f;
			UniformsSeMaterial(shader_programs[1], "material", m_material, /*diffuseUnit=*/1, /*specularUnit=*/2);

			m_material.diffuseTexture = textures[1];
			m_material.specularTexture = textures[2];

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, m_material.diffuseTexture);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, m_material.specularTexture);

			glBindVertexArray(VAOs[1]);

			for (int i = 0; i < boxPositions.size(); i++)
			{
				float angle = 20.f * i;
				vmath::mat4 model = vmath::translate(boxPositions[i]) *
				                    vmath::rotate(angle, 1.0f, 0.3f, 0.5f) *
				                    vmath::scale(1.0f);
				glUniformMatrix4fv(glGetUniformLocation(shader_programs[1], "model"), 1, GL_FALSE, model);
				glDrawArrays(GL_TRIANGLES, 0, 36);
			}
		}

		// 포인트 라이트 개수(NUM_POINT_LIGHTS)만큼 광원 위치에 피라미드 그리기
		{
			const float scaleFactor = 0.05f; // (float)cos(currentTime)*0.05f + 0.2f;

			glUseProgram(shader_programs[2]);

			// projection/view/color 는 광원 간에 동일하므로 루프 밖에서 한 번만 설정한다.
			glUniformMatrix4fv(glGetUniformLocation(shader_programs[2], "projection"), 1, GL_FALSE, projM);
			glUniformMatrix4fv(glGetUniformLocation(shader_programs[2], "view"), 1, GL_FALSE, lookAt);
			glUniform3fv(glGetUniformLocation(shader_programs[2], "color"), 1, lightColor);

			const GLint modelLoc = glGetUniformLocation(shader_programs[2], "model");

			glBindVertexArray(VAOs[2]);
			for (int i = 0; i < NUM_POINT_LIGHTS; i++)
			{
				vmath::mat4 model = vmath::translate(m_pointLights[i].position) *
				                    vmath::rotate(angle * 0.5f, 0.0f, 1.0f, 0.0f) *
				                    vmath::scale(scaleFactor, scaleFactor, scaleFactor);
				glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
				glDrawElements(GL_TRIANGLES, 24, GL_UNSIGNED_INT, 0);
			}
		}
	}

	void onResize(int w, int h)
	{
		sb7::application::onResize(w, h);
		std::cout << "width : " << w
		          << "height : " << h
		          << std::endl;
	}

  private:
	GLuint shader_programs[3];
	GLuint VAOs[3], VBOs[3], EBOs[2];
	GLuint textures[3];

	// 라이트/머티리얼 구조체 정의는 클래스 상단(public)으로 이동했다.
	// Light m_light; // (현재 미사용)
	Material m_material;

	DirLight m_dirLight;
	PointLight m_pointLights[2]; // 배열로 선언
	SpotLight m_spotLight;

	std::vector<vmath::vec3> boxPositions;

	static constexpr float box_s = 1.0f, box_t = 1.0f;
	static constexpr float floor_s = 3.0f, floor_t = 3.0f;

	static constexpr GLfloat box_vertices[] = {
	    // 뒷면
	    -0.25f, 0.5f, -0.25f, 1.0f, 0.0f, 0.0f, box_s, box_t, 0.0f, 0.0f, -1.0f,
	    0.25f, 0.0f, -0.25f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
	    -0.25f, 0.0f, -0.25f, 1.0f, 0.0f, 0.0f, box_s, 0.0f, 0.0f, 0.0f, -1.0f,

	    0.25f, 0.0f, -0.25f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
	    -0.25f, 0.5f, -0.25f, 1.0f, 0.0f, 0.0f, box_s, box_t, 0.0f, 0.0f, -1.0f,
	    0.25f, 0.5f, -0.25f, 1.0f, 0.0f, 0.0f, 0.0f, box_t, 0.0f, 0.0f, -1.0f,
	    // 우측면
	    0.25f, 0.0f, -0.25f, 0.0f, 1.0f, 0.0f, box_s, 0.0f, 1.0f, 0.0f, 0.0f,
	    0.25f, 0.5f, -0.25f, 0.0f, 1.0f, 0.0f, box_s, box_t, 1.0f, 0.0f, 0.0f,
	    0.25f, 0.0f, 0.25f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,

	    0.25f, 0.0f, 0.25f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
	    0.25f, 0.5f, -0.25f, 0.0f, 1.0f, 0.0f, box_s, box_t, 1.0f, 0.0f, 0.0f,
	    0.25f, 0.5f, 0.25f, 0.0f, 1.0f, 0.0f, 0.0f, box_t, 1.0f, 0.0f, 0.0f,
	    // 정면
	    0.25f, 0.0f, 0.25f, 0.0f, 0.0f, 1.0f, box_s, 0.0f, 0.0f, 0.0f, 1.0f,
	    0.25f, 0.5f, 0.25f, 0.0f, 0.0f, 1.0f, box_s, box_t, 0.0f, 0.0f, 1.0f,
	    -0.25f, 0.0f, 0.25f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,

	    -0.25f, 0.0f, 0.25f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
	    0.25f, 0.5f, 0.25f, 0.0f, 0.0f, 1.0f, box_s, box_t, 0.0f, 0.0f, 1.0f,
	    -0.25f, 0.5f, 0.25f, 0.0f, 0.0f, 1.0f, 0.0f, box_t, 0.0f, 0.0f, 1.0f,
	    // 좌측면
	    -0.25f, 0.0f, 0.25f, 1.0f, 0.0f, 1.0f, box_s, 0.0f, -1.0f, 0.0f, 0.0f,
	    -0.25f, 0.5f, 0.25f, 1.0f, 0.0f, 1.0f, box_s, box_t, -1.0f, 0.0f, 0.0f,
	    -0.25f, 0.0f, -0.25f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,

	    -0.25f, 0.0f, -0.25f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
	    -0.25f, 0.5f, 0.25f, 1.0f, 0.0f, 1.0f, box_s, box_t, -1.0f, 0.0f, 0.0f,
	    -0.25f, 0.5f, -0.25f, 1.0f, 0.0f, 1.0f, 0.0f, box_t, -1.0f, 0.0f, 0.0f,
	    // 바닥면
	    -0.25f, 0.0f, 0.25f, 1.0f, 1.0f, 0.0f, box_s, 0.0f, 0.0f, -1.0f, 0.0f,
	    0.25f, 0.0f, -0.25f, 1.0f, 1.0f, 0.0f, 0.0f, box_t, 0.0f, -1.0f, 0.0f,
	    0.25f, 0.0f, 0.25f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,

	    0.25f, 0.0f, -0.25f, 1.0f, 1.0f, 0.0f, 0.0f, box_t, 0.0f, -1.0f, 0.0f,
	    -0.25f, 0.0f, 0.25f, 1.0f, 1.0f, 0.0f, box_s, 0.0, 0.0f, -1.0f, 0.0f,
	    -0.25f, 0.0f, -0.25f, 1.0f, 1.0f, 0.0f, box_s, box_t, 0.0f, -1.0f, 0.0f,
	    // 윗면
	    -0.25f, 0.5f, -0.25f, 0.0f, 1.0f, 1.0f, 0.0f, box_t, 0.0f, 1.0f, 0.0f,
	    0.25f, 0.5f, 0.25f, 0.0f, 1.0f, 1.0f, box_s, 0.0f, 0.0f, 1.0f, 0.0f,
	    0.25f, 0.5f, -0.25f, 0.0f, 1.0f, 1.0f, box_s, box_t, 0.0f, 1.0f, 0.0f,

	    0.25f, 0.5f, 0.25f, 0.0f, 1.0f, 1.0f, box_s, 0.0f, 0.0f, 1.0f, 0.0f,
	    -0.25f, 0.5f, -0.25f, 0.0f, 1.0f, 1.0f, 0.0f, box_t, 0.0f, 1.0f, 0.0f,
	    -0.25f, 0.5f, 0.25f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};

	static constexpr GLfloat floor_vertices[] = {
	    1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, floor_s, floor_t, // 우측 상단
	    -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, floor_t,   // 좌측 상단
	    -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,       // 좌측 하단
	    1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, floor_s, 0.0f      // 우측 하단
	};

	// 삼각형으로 그릴 인덱스를 정의한다.
	static constexpr GLuint floor_indices[] = {
	    0, 1, 2, // 첫번째 삼각형
	    0, 2, 3  // 두번째 삼각형
	};

	static constexpr GLfloat pyramid_vertices[] = {
	    1.0f,
	    0.0f,
	    -1.0f, // 우측 상단
	    -1.0f,
	    0.0f,
	    -1.0f, // 좌측 상단
	    -1.0f,
	    0.0f,
	    1.0f, // 좌측 하단
	    1.0f,
	    0.0f,
	    1.0f, // 우측 하단
	    0.0f,
	    1.0f,
	    0.0f, // 상단 꼭지점
	    0.0f,
	    -1.0f,
	    0.0f, // 하단 꼭지점
	};

	// 삼각형으로 그릴 인덱스를 정의한다.
	static constexpr GLuint pyramid_indices[] = {
	    4,
	    0,
	    1,
	    4,
	    1,
	    2,
	    4,
	    2,
	    3,
	    4,
	    3,
	    0,

	    5,
	    1,
	    0,
	    5,
	    2,
	    1,
	    5,
	    3,
	    2,
	    5,
	    0,
	    3,
	};
};

// DECLARE_MAIN의 하나뿐인 인스턴스
DECLARE_MAIN(my_application)