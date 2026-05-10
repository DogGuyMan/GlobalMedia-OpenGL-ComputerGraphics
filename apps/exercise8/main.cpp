// sb6.h 헤더 파일을 포함시킨다.
#include "GL/gl3w.h"
#include <sb7.h>
#include <shader.h>
#include <type_traits>
#include <vector>
#include <vmath.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define MAC_WINE_TEST

namespace Engine::Model
{

	inline vmath::vec3 ComputeFaceNormal(const vmath::vec3 &p0,
	                                     const vmath::vec3 &p1,
	                                     const vmath::vec3 &p2)
	{
		vmath::vec3 e1 = p1 - p0;
		vmath::vec3 e2 = p2 - p0;
		return vmath::normalize(vmath::cross(e1, e2));
	}

	static const std::vector<vmath::vec3> CUBE_BASE_POSITIONS = {
	    {-0.25f, 0.0f, -0.25f}, // 0
	    {0.25f, 0.0f, -0.25f},  // 1
	    {0.25f, 0.0f, 0.25f},   // 2
	    {-0.25f, 0.0f, 0.25f},  // 3
	    {-0.25f, 0.5f, -0.25f}, // 4
	    {0.25f, 0.5f, -0.25f},  // 5
	    {0.25f, 0.5f, 0.25f},   // 6
	    {-0.25f, 0.5f, 0.25f}   // 7
	};

	static const std::vector<std::vector<GLuint>> CUBE_FACE_INDICES = {
	    {1, 0, 4, 1, 4, 5}, // -Z
	    {2, 1, 5, 2, 5, 6}, // +X
	    {3, 2, 6, 3, 6, 7}, // +Z
	    {0, 3, 7, 0, 7, 4}, // -X
	    {0, 1, 2, 0, 2, 3}, // -Y
	    {7, 6, 5, 7, 5, 4}  // +Y
	};

	static const std::vector<vmath::vec2> BASE_QUAD_MESH_UVS = {
	    {0.0f, 0.0f},
	    {1.0f, 0.0f},
	    {1.0f, 1.0f},
	    {0.0f, 1.0f}};

	static const std::vector<GLuint> QUAD_MESH_UVS_FAN = {0, 1, 2, 0, 2, 3};

	inline void PushVertex(std::vector<GLfloat> &vertices,
	                       const GLuint posSz, const GLfloat pos[],
	                       const GLuint colorSz = 0, const GLfloat color[] = nullptr,
	                       const GLuint uvSz = 0, const GLfloat uv[] = nullptr,
	                       const GLuint normalSz = 0, const GLfloat normal[] = nullptr)
	{
		for (int i = 0; i < posSz; i++)
			vertices.push_back(pos[i]);
		for (int i = 0; i < colorSz; i++)
			vertices.push_back(color[i]);
		for (int i = 0; i < uvSz; i++)
			vertices.push_back(uv[i]);
		for (int i = 0; i < normalSz; i++)
			vertices.push_back(normal[i]);
	}

	inline void BuildCube(std::vector<GLfloat> &buffer_data,
	                      const std::vector<std::vector<GLuint>> &cube_face_indices,
	                      const std::vector<vmath::vec2> &base_quad_mesh_uvs = BASE_QUAD_MESH_UVS)
	{

		for (int f = 0; f < 6; f++)
		{
			const auto &face_idxs = cube_face_indices[f];
			const vmath::vec3 &fp0 = CUBE_BASE_POSITIONS[face_idxs[0]];
			const vmath::vec3 &fp1 = CUBE_BASE_POSITIONS[face_idxs[1]];
			const vmath::vec3 &fp2 = CUBE_BASE_POSITIONS[face_idxs[2]];
			const vmath::vec3 white = vmath::vec3(1.0f, 1.0f, 1.0f);
			const vmath::vec3 faceNormal = ComputeFaceNormal(fp0, fp1, fp2);

			for (int i = 0; i < 6; i++)
			{
				const vmath::vec3 &pos = CUBE_BASE_POSITIONS[face_idxs[i]];
				const vmath::vec2 &uv = base_quad_mesh_uvs[QUAD_MESH_UVS_FAN[i]];

				PushVertex(buffer_data,
				           3, &pos[0],
				           3, white,
				           2, &uv[0],
				           3, &faceNormal[0]);
			}
		}
	}

	static const std::vector<vmath::vec3> QUAD_BASE_POSITIONS = {
	    {1.0f, 0.0f, -1.0f},
	    {-1.0f, 0.0f, -1.0f},
	    {-1.0f, 0.0f, 1.0f},
	    {1.0f, 0.0f, 1.0f}};

	static const std::vector<GLuint> QUAD_FACE_INDICES = {
	    0, 1, 2,
	    0, 2, 3};

	static const std::vector<vmath::vec3> QUAD_RGBY_COLORS = {
	    {1.0f, 0.0f, 0.0f},
	    {0.0f, 1.0f, 0.0f},
	    {0.0f, 0.0f, 1.0f},
	    {1.0f, 1.0f, 0.0f}};

	static const std::vector<vmath::vec2> QUAD_FLOOR_UVS = {
	    {3.0f, 3.0f},
	    {0.0f, 3.0f},
	    {0.0f, 0.0f},
	    {3.0f, 0.0f}};

	inline void BuildQuad(std::vector<GLfloat> &buffer_data,
	                      const std::vector<vmath::vec3> &corner_colors = QUAD_RGBY_COLORS,
	                      const std::vector<vmath::vec2> &corner_uvs = QUAD_FLOOR_UVS)
	{
		const vmath::vec3 normal = ComputeFaceNormal(QUAD_BASE_POSITIONS[0],
		                                             QUAD_BASE_POSITIONS[1],
		                                             QUAD_BASE_POSITIONS[2]);

		for (int i = 0; i < 4; i++)
		{
			const vmath::vec3 &pos = QUAD_BASE_POSITIONS[i];
			const vmath::vec3 &color = corner_colors[i];
			const vmath::vec2 &uv = corner_uvs[i];

			PushVertex(buffer_data,
			           3, &pos[0],
			           3, &color[0],
			           2, &uv[0],
			           3, &normal[0]);
		}
	}

	static const std::vector<vmath::vec3> PYRAMID_BASE_POSITIONS = {
	    {1.0f, 0.0f, -1.0f},
	    {-1.0f, 0.0f, -1.0f},
	    {-1.0f, 0.0f, 1.0f},
	    {1.0f, 0.0f, 1.0f},
	    {0.0f, 1.0f, 0.0f},
	    {0.0f, -1.0f, 0.0f}};

	static const std::vector<GLuint> PYRAMID_FACE_INDICES = {

	    4, 0, 1,
	    4, 1, 2,
	    4, 2, 3,
	    4, 3, 0,

	    5, 1, 0,
	    5, 2, 1,
	    5, 3, 2,
	    5, 0, 3};

	inline void BuildPyramid(std::vector<GLfloat> &buffer_data)
	{
		const vmath::vec3 white(1.0f, 1.0f, 1.0f);
		const vmath::vec2 zeroUV(0.0f, 0.0f);

		for (const vmath::vec3 &pos : PYRAMID_BASE_POSITIONS)
		{
			const vmath::vec3 normal = vmath::normalize(pos);
			PushVertex(buffer_data,
			           3, &pos[0],
			           3, &white[0],
			           2, &zeroUV[0],
			           3, &normal[0]);
		}
	}
} // namespace Engine::Model

// sb6::application을 상속받는다.
class my_application : public sb7::application
{
  public:
	// 쉐이더 프로그램 컴파일한다.
	GLuint compile_shader(const char *vs_file, const char *fs_file)
	{
		// 버텍스 쉐이더를 생성하고 컴파일한다.
		GLuint vertex_shader = sb7::shader::load(vs_file, GL_VERTEX_SHADER);

		// 프래그먼트 쉐이더를 생성하고 컴파일한다.
		GLuint fragment_shader = sb7::shader::load(fs_file, GL_FRAGMENT_SHADER);

		// 프로그램을 생성하고 쉐이더를 Attach시키고 링크한다.
		GLuint program = glCreateProgram();
		glAttachShader(program, vertex_shader);
		glAttachShader(program, fragment_shader);
		glLinkProgram(program);

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
		shader_program = compile_shader("./shaders/basic_lighting_vs.glsl", "./shaders/basic_lighting_fs.glsl");

		// VAO, VBO, EBO, texture 생성
		glGenVertexArrays(3, VAOs);
		glGenBuffers(3, VBOs);
		glGenBuffers(2, EBOs);
		glGenTextures(3, textures);

		stbi_set_flip_vertically_on_load(true);

		// 첫 번째 객체 정의 : 바닥 --------------------------------------------------
		glBindVertexArray(VAOs[0]);

		// 바닥 vertex data — Engine::Model::BuildQuad 로 런타임 생성. 11-float layout (basic_lighting 호환).
		std::vector<GLfloat> floor_vertices_data;
		Engine::Model::BuildQuad(floor_vertices_data);
		glBindBuffer(GL_ARRAY_BUFFER, VBOs[0]);
		glBufferData(GL_ARRAY_BUFFER,
		             floor_vertices_data.size() * sizeof(GLfloat),
		             floor_vertices_data.data(), GL_STATIC_DRAW);

		// 4 attributes (location 0/1/2/3 = pos/color/uv/normal), stride 11 float.
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(8 * sizeof(float)));
		glEnableVertexAttribArray(3);

		// EBO — namespace 의 QUAD_FACE_INDICES 직접 업로드.
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[0]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		             Engine::Model::QUAD_FACE_INDICES.size() * sizeof(GLuint),
		             Engine::Model::QUAD_FACE_INDICES.data(), GL_STATIC_DRAW);

		// VBO 및 버텍스 속성을 다 했으니 VBO와 VAO를 unbind한다.
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		// 텍스처 객체 만들고 바인딩
		glBindTexture(GL_TEXTURE_2D, textures[0]);

		// 텍스처 이미지 로드하기
		load_texture(textures[0], "./textures/wall.jpg", GL_RGB, GL_RGB);

		// 두 번째 객체 정의 : 박스 --------------------------------------------------
		glBindVertexArray(VAOs[1]);
		// 박스 점들의 위치와 컬러, 텍스처 좌표를 정의한다.

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

		std::vector<GLfloat> box_vertices_data;
		Engine::Model::BuildCube(box_vertices_data, Engine::Model::CUBE_FACE_INDICES);
		glBindBuffer(GL_ARRAY_BUFFER, VBOs[1]);
		glBufferData(GL_ARRAY_BUFFER,
		             box_vertices_data.size() * sizeof(GLfloat),
		             box_vertices_data.data(), GL_STATIC_DRAW);

		// VBO를 나누어서 각 버텍스 속성으로 연결
		// 위치 속성 (location = 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);
		// 컬러 속성 (location = 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// 텍스처 좌표 속성 (location = 2)
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);
		// 노멀 속성 (location = 3)
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(8 * sizeof(float)));
		glEnableVertexAttribArray(3);

		// Diffuse Map 이미지 로드 및 적용
		load_texture(textures[1], "./textures/container2.png", GL_RGB, GL_RGBA);
		// Specular Map 이미지 로드 및 적용
		load_texture(textures[2], "./textures/container2_specular.png", GL_RGB, GL_RGBA);

		// VBO 및 버텍스 속성을 다 했으니 VBO와 VAO를 unbind한다.
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		//  세 번째 객체 정의 : 피라미드 --------------------------------------------------
		glBindVertexArray(VAOs[2]);

		// 피라미드 vertex data — Engine::Model::BuildPyramid 로 런타임 생성. 11-float layout (basic_lighting 호환).
		std::vector<GLfloat> pyramid_vertices_data;
		Engine::Model::BuildPyramid(pyramid_vertices_data);
		glBindBuffer(GL_ARRAY_BUFFER, VBOs[2]);
		glBufferData(GL_ARRAY_BUFFER,
		             pyramid_vertices_data.size() * sizeof(GLfloat),
		             pyramid_vertices_data.data(), GL_STATIC_DRAW);

		// 4 attributes (location 0/1/2/3 = pos/color/uv/normal), stride 11 float.
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void *)(8 * sizeof(float)));
		glEnableVertexAttribArray(3);

		// EBO — namespace 의 PYRAMID_FACE_INDICES 직접 업로드.
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBOs[1]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		             Engine::Model::PYRAMID_FACE_INDICES.size() * sizeof(GLuint),
		             Engine::Model::PYRAMID_FACE_INDICES.data(), GL_STATIC_DRAW);

		// VBO 및 버텍스 속성을 다 했으니 VBO와 VAO를 unbind한다.
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	// 애플리케이션 끝날 때 호출된다.
	virtual void shutdown()
	{
		glDeleteTextures(3, textures);
		glDeleteBuffers(2, EBOs);
		glDeleteBuffers(3, VBOs);
		glDeleteVertexArrays(3, VAOs);
		glDeleteProgram(shader_program);
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

		GLint uniform_transform1 = glGetUniformLocation(shader_program, "transform");
		GLint uniform_transform2 = glGetUniformLocation(shader_program, "transform");

		// 카메라 매트릭스 계산
		float distance = 2.f;
		vmath::vec3 eye((float)cos(currentTime * 0.1f) * distance, 1.0, (float)sin(currentTime * 0.1f) * distance);
		vmath::vec3 center(0.0, 0.0, 0.0);
		vmath::vec3 up(0.0, 1.0, 0.0);
		vmath::mat4 lookAt = vmath::lookat(eye, center, up);
		float fov = 50.f;
		vmath::mat4 projM = vmath::perspective(fov, (float)info.windowWidth / info.windowHeight, 0.1f, 1000.0f);

		// 라이팅 설정 ---------------------------------------
		m_light.position = vmath::vec3((float)sin(currentTime * 0.5f), 0.25f, (float)cos(currentTime * 0.5f) * 0.7f); // (0.0f, 0.5f, 0.0f);
		m_light.ambient = vmath::vec3(0.3f, 0.3f, 0.3f);
		m_light.diffuse = vmath::vec3(1.0f, 1.0f, 1.0f);
		m_light.specular = vmath::vec3(1.0f, 1.0f, 1.0f);
		vmath::vec3 viewPos = eye;
		vmath::vec3 lightColor(1.0f, 1.0f, 1.0f);
		vmath::vec3 boxColor(1.0f, 1.0f, 1.0f);

		// 박스 그리기 ---------------------------------------
		vmath::mat4 transM = vmath::translate(vmath::vec3((float)sin(currentTime * 0.5f), 0.0f, (float)cos(currentTime * 0.5f) * 0.7f));
		float angle = currentTime * 100;
		vmath::mat4 rotateM = vmath::rotate(angle, 0.0f, 1.0f, 0.0f);

		glUseProgram(shader_program);

		glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, projM);
		glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, lookAt);
		glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, rotateM);

		glUniform3fv(glGetUniformLocation(shader_program, "viewPos"), 1, viewPos);
		glUniform3fv(glGetUniformLocation(shader_program, "objectColor"), 1, boxColor);

		glUniform3fv(glGetUniformLocation(shader_program, "light.position"), 1, m_light.position);
		glUniform3fv(glGetUniformLocation(shader_program, "light.ambient"), 1, m_light.ambient);
		glUniform3fv(glGetUniformLocation(shader_program, "light.diffuse"), 1, m_light.diffuse);
		glUniform3fv(glGetUniformLocation(shader_program, "light.specular"), 1, m_light.specular);

		glUniform1i(glGetUniformLocation(shader_program, "material.diffuse"), 1);
		glUniform1i(glGetUniformLocation(shader_program, "material.specular"), 2);

		// 텍스처 있는 메쉬 — useTexture = 1.0 → fs 가 라이팅 계산 경로 진입.
		glUniform1f(glGetUniformLocation(shader_program, "useTexture"), 1.0f);

		m_material.diffuseTexture = textures[1];
		m_material.specularTexture = textures[2];

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_material.diffuseTexture);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, m_material.specularTexture);

		glUniform3fv(glGetUniformLocation(shader_program, "light.position"), 1, m_light.position);
		glUniform3fv(glGetUniformLocation(shader_program, "light.ambient"), 1, m_light.ambient);
		glUniform3fv(glGetUniformLocation(shader_program, "light.diffuse"), 1, m_light.diffuse);
		glUniform3fv(glGetUniformLocation(shader_program, "light.specular"), 1, m_light.specular);

		glUniform1i(glGetUniformLocation(shader_program, "material.diffuse"), 1);
		glUniform1i(glGetUniformLocation(shader_program, "material.specular"), 2);

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
			glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, model);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}

		// 피라미드 (광원) 그리기 ---------------------------------------
		// 텍스처 없는 unlit 메쉬 — useTexture = 0.0 → fs 가 objectColor (=lightColor) 만 출력.
		float scaleFactor = 0.05f;
		vmath::mat4 transform = vmath::translate(m_light.position) *
		                        vmath::rotate(angle * 0.5f, 0.0f, 1.0f, 0.0f) *
		                        vmath::scale(scaleFactor, scaleFactor, scaleFactor);

		glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, transform);
		glUniform3fv(glGetUniformLocation(shader_program, "objectColor"), 1, lightColor);
		glUniform1f(glGetUniformLocation(shader_program, "useTexture"), 0.0f);

		glUniform3fv(glGetUniformLocation(shader_program, "color"), 1, lightColor);

		glBindVertexArray(VAOs[2]);
		glDrawElements(GL_TRIANGLES,
		               Engine::Model::PYRAMID_FACE_INDICES.size(),
		               GL_UNSIGNED_INT, 0);
	}

	void onResize(int w, int h)
	{
		sb7::application::onResize(w, h);
	}

  private:
	GLuint shader_program;
	GLuint VAOs[3], VBOs[3], EBOs[2];
	GLuint textures[3];

	struct Light
	{
		vmath::vec3 position;
		vmath::vec3 ambient;
		vmath::vec3 diffuse;
		vmath::vec3 specular;
	};
	Light m_light;

	struct Material
	{
		GLuint diffuseTexture;
		GLuint specularTexture;
	};
	Material m_material;

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