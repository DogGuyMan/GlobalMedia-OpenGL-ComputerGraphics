// sb6.h 헤더 파일을 포함시킨다.
#include "GL/gl3w.h"
#include <cmath>
#include <cstdio>
#include <ostream>
#include <sb7.h>
#include <shader.h>
#include <vector>
#include <vmath.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <sstream>
#include <string>

#define MAC_WINE_TEST
#include "diagnostics/engine_diagnostics.h"   // diag::
#include "diagnostics/gl_log.h"              // diag::
#include "diagnostics/gl_state_log.h"        // diag::
#include "diagnostics/uniform_diagnostics.h" // diag::
namespace diag = SJH::Diagnostics;           // diag::

#define NUM_POINT_LIGHTS 2

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

	// ───────────────────────────────────────────────────────────────────────
	// EBO 기반 공유 정점 메시 — Build 함수는 정점 데이터만 만들고,
	// EBO 에는 XXX_FACE_INDICES 상수를 그대로 업로드한다.
	//
	// 정점 normal = normalize(pos - center) (radial).
	// 대칭 도형(정육면체/정사면체/이중 피라미드)에서 이 값은 인접 면 face normal
	// 평균과 등가하므로 Phong 보간 시 자연스러운 셰이딩이 나온다. 면 단위 평면
	// 셰이딩(flat)이 필요하면 정점을 면당 복제해야 함 — 이 빌더는 정점 공유를
	// 위해 그 방식은 포기. UV 도 정점당 1개라 박스 UV 매핑 불가 → (0,0) 고정.
	// ───────────────────────────────────────────────────────────────────────

	// 평면 Quad: 4정점, 단일 면 → face normal 1개를 4정점 모두 공유.
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

	// Cube: 8정점 (각 코너), 6면을 12삼각형 (= 36 인덱스) 으로 분할.
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

	// EBO 용 평면 인덱스. 외부에서 CCW (GL_CULL_FACE 활성 상태에서 외면만 그림).
	static const std::vector<GLuint> CUBE_FACE_INDICES = {
	    1, 0, 4, 1, 4, 5, // -Z
	    2, 1, 5, 2, 5, 6, // +X
	    3, 2, 6, 3, 6, 7, // +Z
	    0, 3, 7, 0, 7, 4, // -X
	    0, 1, 2, 0, 2, 3, // -Y
	    7, 6, 5, 7, 5, 4  // +Y
	};

	// Tetrahedron: 4정점 (베이스 3 + apex 1), 4면을 4삼각형 (= 12 인덱스).
	// 주의: vmath::radians 는 template<T> T radians(T) — 정수 리터럴 전달 시 T=int 로
	// 추론되어 M_PI/180 이 0 으로 잘림. 반드시 float 리터럴(0.0f, 120.0f, ...) 사용.
	static const std::vector<vmath::vec3> TETRAHEDRON_BASE_POSITIONS{
	    {cosf(vmath::radians(0.0f)), 0.0f, sinf(vmath::radians(0.0f))},
	    {cosf(vmath::radians(120.0f)), 0.0f, sinf(vmath::radians(120.0f))},
	    {cosf(vmath::radians(240.0f)), 0.0f, sinf(vmath::radians(240.0f))},
	    {0.0f, (float)sqrt(2.0), 0.0f}}; // 정 4면체: 모든 모서리 길이 = √3 이 되도록 apex 높이 = √2

	// EBO 용 평면 인덱스. 외부에서 CCW (3개 측면 + 1개 밑면).
	static const std::vector<GLuint> TETRAHEDRON_FACE_INDICES = {
	    0, 3, 1,
	    1, 3, 2,
	    2, 3, 0,
	    0, 1, 2, // 밑면
	};

	// Pyramid (이중 피라미드): 6정점 (베이스 4 + apex 위/아래 2), 8삼각형 (= 24 인덱스).
	static const std::vector<vmath::vec3> PYRAMID_BASE_POSITIONS = {
	    {1.0f, 0.0f, -1.0f},
	    {-1.0f, 0.0f, -1.0f},
	    {-1.0f, 0.0f, 1.0f},
	    {1.0f, 0.0f, 1.0f},
	    {0.0f, 1.0f, 0.0f},   // 위쪽 apex
	    {0.0f, -1.0f, 0.0f}}; // 아래쪽 apex

	static const std::vector<GLuint> PYRAMID_FACE_INDICES = {
	    4, 0, 1,
	    4, 1, 2,
	    4, 2, 3,
	    4, 3, 0,
	    5, 1, 0,
	    5, 2, 1,
	    5, 3, 2,
	    5, 0, 3};

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

	inline void BuildCube(std::vector<GLfloat> &vertex_data)
	{
		// bounding box 중심: y∈[0,0.5], xz∈[-0.25,0.25] → (0, 0.25, 0)
		const vmath::vec3 center(0.0f, 0.25f, 0.0f);
		const vmath::vec3 white(1.0f, 1.0f, 1.0f);
		const vmath::vec2 zeroUV(0.0f, 0.0f);

		for (const vmath::vec3 &pos : CUBE_BASE_POSITIONS)
		{
			const vmath::vec3 normal = vmath::normalize(pos - center);
			PushVertex(vertex_data,
			           3, &pos[0],
			           3, &white[0],
			           2, &zeroUV[0],
			           3, &normal[0]);
		}
	}

	inline void BuildTetrahedron(std::vector<GLfloat> &vertex_data)
	{
		// centroid: base 3개 평균 (0,0,0) + apex (0,√2,0) → (0, √2/4, 0)
		const vmath::vec3 center(0.0f, (float)(sqrt(2.0) / 4.0), 0.0f);
		const vmath::vec3 white(1.0f, 1.0f, 1.0f);
		const vmath::vec2 zeroUV(0.0f, 0.0f);

		for (const vmath::vec3 &pos : TETRAHEDRON_BASE_POSITIONS)
		{
			const vmath::vec3 normal = vmath::normalize(pos - center);
			PushVertex(vertex_data,
			           3, &pos[0],
			           3, &white[0],
			           2, &zeroUV[0],
			           3, &normal[0]);
		}
	}

	inline void BuildQuad(std::vector<GLfloat> &vertex_data,
	                      const std::vector<vmath::vec3> &corner_colors = QUAD_RGBY_COLORS,
	                      const std::vector<vmath::vec2> &corner_uvs = QUAD_FLOOR_UVS)
	{
		// 평면 단일 면 → 4정점이 같은 face normal 공유 (radial 필요 없음)
		const vmath::vec3 normal = ComputeFaceNormal(QUAD_BASE_POSITIONS[0],
		                                             QUAD_BASE_POSITIONS[1],
		                                             QUAD_BASE_POSITIONS[2]);
		for (int i = 0; i < 4; i++)
		{
			PushVertex(vertex_data,
			           3, &QUAD_BASE_POSITIONS[i][0],
			           3, &corner_colors[i][0],
			           2, &corner_uvs[i][0],
			           3, &normal[0]);
		}
	}

	inline void BuildPyramid(std::vector<GLfloat> &vertex_data)
	{
		// PYRAMID 정점들이 origin 기준 대칭 → radial normal = normalize(pos)
		const vmath::vec3 white(1.0f, 1.0f, 1.0f);
		const vmath::vec2 zeroUV(0.0f, 0.0f);

		for (const vmath::vec3 &pos : PYRAMID_BASE_POSITIONS)
		{
			const vmath::vec3 normal = vmath::normalize(pos);
			PushVertex(vertex_data,
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
		GLuint diffuseTexture;  // diffuse map 텍스처 객체 핸들
		GLuint specularTexture; // specular map 텍스처 객체 핸들
		GLint diffuseUnit;      // diffuseTexture 를 바인딩할 텍스처 이미지 유닛 번호 (sampler2D 에 넣는 값)
		GLint specularUnit;     // specularTexture 를 바인딩할 텍스처 이미지 유닛 번호
		float shininess;
	};

	static GLint UniformLoc(GLuint program, const char *name, const GLint loc) // diag::
	{                                                                          // diag::
		if (loc < 0)                                                           // diag::
			diag::UniformDiagnostics::NotifyMissing(program, name);            // diag::
		return loc;                                                            // diag::
	} // diag::

	static void UniformsSetDirLight(GLuint program, const std::string &prefix, const DirLight &light)
	{
		const GLint locDirection = glGetUniformLocation(program, (prefix + ".direction").c_str());
		UniformLoc(program, (prefix + ".direction").c_str(), locDirection); // diag::
		const GLint locAmbient = glGetUniformLocation(program, (prefix + ".ambient").c_str());
		UniformLoc(program, (prefix + ".ambient").c_str(), locAmbient); // diag::
		const GLint locDiffuse = glGetUniformLocation(program, (prefix + ".diffuse").c_str());
		UniformLoc(program, (prefix + ".diffuse").c_str(), locDiffuse); // diag::
		const GLint locSpecular = glGetUniformLocation(program, (prefix + ".specular").c_str());
		UniformLoc(program, (prefix + ".specular").c_str(), locSpecular); // diag::

		glUniform3fv(locDirection, 1, light.direction);
		glUniform3fv(locAmbient, 1, light.ambient);
		glUniform3fv(locDiffuse, 1, light.diffuse);
		glUniform3fv(locSpecular, 1, light.specular);
	}

	static void UniformsSetPointLight(GLuint program, const std::string &prefix, const PointLight &light)
	{
		const GLint locPosition = glGetUniformLocation(program, (prefix + ".position").c_str());
		UniformLoc(program, (prefix + ".position").c_str(), locPosition); // diag::
		const GLint locAmbient = glGetUniformLocation(program, (prefix + ".ambient").c_str());
		UniformLoc(program, (prefix + ".ambient").c_str(), locAmbient); // diag::
		const GLint locDiffuse = glGetUniformLocation(program, (prefix + ".diffuse").c_str());
		UniformLoc(program, (prefix + ".diffuse").c_str(), locDiffuse); // diag::
		const GLint locSpecular = glGetUniformLocation(program, (prefix + ".specular").c_str());
		UniformLoc(program, (prefix + ".specular").c_str(), locSpecular); // diag::
		const GLint locC1 = glGetUniformLocation(program, (prefix + ".c1").c_str());
		UniformLoc(program, (prefix + ".c1").c_str(), locC1); // diag::
		const GLint locC2 = glGetUniformLocation(program, (prefix + ".c2").c_str());
		UniformLoc(program, (prefix + ".c2").c_str(), locC2); // diag::

		glUniform3fv(locPosition, 1, light.position);
		glUniform3fv(locAmbient, 1, light.ambient);
		glUniform3fv(locDiffuse, 1, light.diffuse);
		glUniform3fv(locSpecular, 1, light.specular);
		glUniform1f(locC1, light.c1);
		glUniform1f(locC2, light.c2);
	}

	static void UniformsSetSpotLight(GLuint program, const std::string &prefix, const SpotLight &light)
	{
		const GLint locPosition = glGetUniformLocation(program, (prefix + ".position").c_str());
		UniformLoc(program, (prefix + ".position").c_str(), locPosition); // diag::
		const GLint locDirection = glGetUniformLocation(program, (prefix + ".direction").c_str());
		UniformLoc(program, (prefix + ".direction").c_str(), locDirection); // diag::
		const GLint locCutOff = glGetUniformLocation(program, (prefix + ".cutOff").c_str());
		UniformLoc(program, (prefix + ".cutOff").c_str(), locCutOff); // diag::
		const GLint locOuterCutOff = glGetUniformLocation(program, (prefix + ".outerCutOff").c_str());
		UniformLoc(program, (prefix + ".outerCutOff").c_str(), locOuterCutOff); // diag::
		const GLint locC1 = glGetUniformLocation(program, (prefix + ".c1").c_str());
		UniformLoc(program, (prefix + ".c1").c_str(), locC1); // diag::
		const GLint locC2 = glGetUniformLocation(program, (prefix + ".c2").c_str());
		UniformLoc(program, (prefix + ".c2").c_str(), locC2); // diag::
		const GLint locAmbient = glGetUniformLocation(program, (prefix + ".ambient").c_str());
		UniformLoc(program, (prefix + ".ambient").c_str(), locAmbient); // diag::
		const GLint locDiffuse = glGetUniformLocation(program, (prefix + ".diffuse").c_str());
		UniformLoc(program, (prefix + ".diffuse").c_str(), locDiffuse); // diag::
		const GLint locSpecular = glGetUniformLocation(program, (prefix + ".specular").c_str());
		UniformLoc(program, (prefix + ".specular").c_str(), locSpecular); // diag::

		glUniform3fv(locPosition, 1, light.position);
		glUniform3fv(locDirection, 1, light.direction);
		glUniform1f(locCutOff, light.cutOff);
		glUniform1f(locOuterCutOff, light.outerCutOff);
		glUniform1f(locC1, light.c1);
		glUniform1f(locC2, light.c2);
		glUniform3fv(locAmbient, 1, light.ambient);
		glUniform3fv(locDiffuse, 1, light.diffuse);
		glUniform3fv(locSpecular, 1, light.specular);
	}

	// sampler2D 유니폼에는 텍스처 "이미지 유닛 번호"를 넣는다 (텍스처 객체 핸들이 아님).
	static void UniformsSetMaterial(GLuint program, const std::string &prefix, const Material &material)
	{
		const GLint locDiffuse = glGetUniformLocation(program, (prefix + ".diffuse").c_str());
		UniformLoc(program, (prefix + ".diffuse").c_str(), locDiffuse); // diag::
		const GLint locSpecular = glGetUniformLocation(program, (prefix + ".specular").c_str());
		UniformLoc(program, (prefix + ".specular").c_str(), locSpecular); // diag::
		const GLint locShininess = glGetUniformLocation(program, (prefix + ".shininess").c_str());
		UniformLoc(program, (prefix + ".shininess").c_str(), locShininess); // diag::

		glUniform1i(locDiffuse, material.diffuseUnit);
		glUniform1i(locSpecular, material.specularUnit);
		glUniform1f(locShininess, material.shininess);
	}

	// UniformsSetMaterial 이 셰이더에 알려준 것과 동일한 유닛에 텍스처를 바인딩한다.
	static void BindMaterialTextures(const Material &material)
	{
		glActiveTexture(GL_TEXTURE0 + material.diffuseUnit);
		glBindTexture(GL_TEXTURE_2D, material.diffuseTexture);
		glActiveTexture(GL_TEXTURE0 + material.specularUnit);
		glBindTexture(GL_TEXTURE_2D, material.specularTexture);
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
		// 라이팅 셰이더 단일 프로그램만 사용한다.
		shader_program = compile_shader("./shaders/basic_lighting_vs.glsl", "./shaders/basic_lighting_fs.glsl");

		// VAO, VBO, EBO, texture 생성
		glGenVertexArrays(3, VAO);
		glGenBuffers(3, VBO);
		glGenBuffers(3, EBO);
		glGenTextures(3, textures);

		stbi_set_flip_vertically_on_load(true);
		// 바닥 텍스처
		load_texture(textures[0], "./textures/wall.jpg", GL_RGB, GL_RGB);
		// 머티리얼 Diffuse Map / Specular Map
		load_texture(textures[1], "./textures/container2.png", GL_RGB, GL_RGBA);
		load_texture(textures[2], "./textures/container2_specular.png", GL_RGB, GL_RGBA);

		// Engine::Model 로 바닥 Quad 정점 데이터 생성 (pos3 + color3 + uv2 + normal3 = 11 float/vertex)
		{
			std::vector<GLfloat> quad_vertices;
			Engine::Model::BuildQuad(quad_vertices);
			const std::vector<GLuint> &quad_indices = Engine::Model::QUAD_FACE_INDICES;
			const GLsizei stride = 11 * static_cast<GLsizei>(sizeof(float));

			glBindVertexArray(VAO[0]);
			diag::GLDebug::CheckGLBindVertexArray(VAO[0]); // diag::

			// VBO 에 Quad 정점 복사
			glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
			diag::GLDebug::CheckGLBindBuffer(VBO[0]); // diag::
			glBufferData(GL_ARRAY_BUFFER, quad_vertices.size() * sizeof(GLfloat), quad_vertices.data(), GL_STATIC_DRAW);
			diag::GLDebug::CheckGLBufferData(static_cast<GLint>(quad_vertices.size() * sizeof(GLfloat))); // diag::

			// 위치 속성 (location = 0)
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(0);
			diag::GLDebug::CheckGLEnableVertexAttribArray(0); // diag::
			// 컬러 속성 (location = 1) — 라이팅 셰이더는 안 읽지만 인터리브 레이아웃 유지
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(1);
			diag::GLDebug::CheckGLEnableVertexAttribArray(1); // diag::
			// 텍스처 좌표 속성 (location = 2)
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(2);
			diag::GLDebug::CheckGLEnableVertexAttribArray(2); // diag::
			// 노말 속성 (location = 3)
			glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void *)(9 * sizeof(float)));
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(3);
			diag::GLDebug::CheckGLEnableVertexAttribArray(3); // diag::

			// EBO 에 Quad 인덱스 복사
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[0]);
			diag::GLDebug::CheckGLBindBuffer(EBO[0]); // diag::
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, quad_indices.size() * sizeof(GLuint), quad_indices.data(), GL_STATIC_DRAW);
			diag::GLDebug::CheckGLBufferData(static_cast<GLint>(quad_indices.size() * sizeof(GLuint))); // diag::
			                                                                                            // VBO[0] 및 버텍스 속성을 다 했으니 VBO[0]와 VAO[0]를 unbind한다.
			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
		{
			// BuildCube 는 인덱스 메시가 아니라 면당 6정점을 펼쳐 36정점 배열을 만든다 → glDrawArrays 로 그린다 (EBO 불필요).
			std::vector<GLfloat> cube_vertices;
			Engine::Model::BuildCube(cube_vertices, Engine::Model::CUBE_FACE_INDICES);
			cube_vertex_count = static_cast<GLsizei>(cube_vertices.size() / 11);
			const GLsizei stride = 11 * static_cast<GLsizei>(sizeof(float));

			glBindVertexArray(VAO[1]);
			diag::GLDebug::CheckGLBindVertexArray(VAO[1]); // diag::

			// VBO 에 Cube 정점 복사
			glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
			diag::GLDebug::CheckGLBindBuffer(VBO[1]); // diag::
			glBufferData(GL_ARRAY_BUFFER, cube_vertices.size() * sizeof(GLfloat), cube_vertices.data(), GL_STATIC_DRAW);
			diag::GLDebug::CheckGLBufferData(static_cast<GLint>(cube_vertices.size() * sizeof(GLfloat))); // diag::

			// 위치 속성 (location = 0)
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(0);
			diag::GLDebug::CheckGLEnableVertexAttribArray(0); // diag::
			// 컬러 속성 (location = 1) — 라이팅 셰이더는 안 읽지만 인터리브 레이아웃 유지
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(1);
			diag::GLDebug::CheckGLEnableVertexAttribArray(1); // diag::
			// 텍스처 좌표 속성 (location = 2)
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(2);
			diag::GLDebug::CheckGLEnableVertexAttribArray(2); // diag::
			// 노말 속성 (location = 3)
			glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void *)(9 * sizeof(float)));
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(3);
			diag::GLDebug::CheckGLEnableVertexAttribArray(3); // diag::

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		{
			std::vector<GLfloat> tetrahedron_vertices;
			Engine::Model::BuildTetrahedron(
			    tetrahedron_vertices,
			    Engine::Model::TETRAHEDRON_FACE_INDICES);
			// diag:: VBO 업로드 직전 — stride(11), pos(off=0,sz=3), normal(off=8,sz=3) 검증
			// stride 불일치 / NaN normal / 퇴화 면(normal 길이 0) / 빈 배열을 감지한다.
			diag::EngineDiagnostics::CheckInterleavedVertexBuffer(
			    tetrahedron_vertices, 11, 0, 3, 8, 3, "tetrahedron"); // diag::
			tetrahedron_vertex_count = static_cast<GLsizei>(tetrahedron_vertices.size() / 11);
			const GLsizei stride = 11 * static_cast<GLsizei>(sizeof(float));

			glBindVertexArray(VAO[2]);
			diag::GLDebug::CheckGLBindVertexArray(VAO[2]); // diag::

			// VBO 에 Tetrahedron 정점 복사
			glBindBuffer(GL_ARRAY_BUFFER, VBO[2]);
			diag::GLDebug::CheckGLBindBuffer(VBO[2]); // diag::
			glBufferData(GL_ARRAY_BUFFER, tetrahedron_vertices.size() * sizeof(GLfloat), tetrahedron_vertices.data(), GL_STATIC_DRAW);
			diag::GLDebug::CheckGLBufferData(static_cast<GLint>(tetrahedron_vertices.size() * sizeof(GLfloat))); // diag::

			// 위치 속성 (location = 0)
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(0);
			diag::GLDebug::CheckGLEnableVertexAttribArray(0); // diag::
			// 컬러 속성 (location = 1) — 라이팅 셰이더는 안 읽지만 인터리브 레이아웃 유지
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(1);
			diag::GLDebug::CheckGLEnableVertexAttribArray(1); // diag::
			// 텍스처 좌표 속성 (location = 2)
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(2);
			diag::GLDebug::CheckGLEnableVertexAttribArray(2); // diag::
			// 노말 속성 (location = 3)
			glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void *)(9 * sizeof(float)));
			diag::GLDebug::CheckGLVertexAttribPointer({stride}); // diag::
			glEnableVertexAttribArray(3);
			diag::GLDebug::CheckGLEnableVertexAttribArray(3); // diag::

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		m_material[0].diffuseTexture = textures[0];
		m_material[0].specularTexture = textures[0];
		m_material[0].diffuseUnit = 0;
		m_material[0].specularUnit = 0;
		m_material[0].shininess = 32.0f;

		m_material[1].diffuseTexture = textures[1];
		m_material[1].specularTexture = textures[1];
		m_material[1].diffuseUnit = 1;
		m_material[1].specularUnit = 1;
		m_material[1].shininess = 32.0f;

		m_dirLight.direction = vmath::vec3(0.0, -1.0f, 0.0);
		m_dirLight.ambient = vmath::vec3(1.0f, 1.0f, 1.0f);
		m_dirLight.diffuse = vmath::vec3(1.0f, 1.0f, 1.0f);
		m_dirLight.specular = vmath::vec3(1.0f, 1.0f, 1.0f);

		m_pointLights[0].position = vmath::vec3(1.2f, 1.0f, 1.0f);
		m_pointLights[0].c1 = 0.09f;
		m_pointLights[0].c2 = 0.032f;
		m_pointLights[0].ambient = vmath::vec3(0.05f, 0.05f, 0.05f);
		m_pointLights[0].diffuse = vmath::vec3(0.8f, 0.4f, 0.2f);
		m_pointLights[0].specular = vmath::vec3(1.0f, 1.0f, 1.0f);

		m_pointLights[1].position = vmath::vec3(-1.2f, 1.0f, -1.0f);
		m_pointLights[1].c1 = 0.09f;
		m_pointLights[1].c2 = 0.032f;
		m_pointLights[1].ambient = vmath::vec3(0.05f, 0.05f, 0.05f);
		m_pointLights[1].diffuse = vmath::vec3(0.2f, 0.4f, 0.8f);
		m_pointLights[1].specular = vmath::vec3(1.0f, 1.0f, 1.0f);

		m_spotLight.position = vmath::vec3(0.0f, 1.5f, 0.0f);
		m_spotLight.direction = vmath::vec3(0.0f, -1.0f, 0.0f);
		m_spotLight.cutOff = std::cos(vmath::radians(12.5f));
		m_spotLight.outerCutOff = std::cos(vmath::radians(17.5f));
		m_spotLight.c1 = 0.09f;
		m_spotLight.c2 = 0.032f;
		m_spotLight.ambient = vmath::vec3(0.0f, 0.0f, 0.0f);
		m_spotLight.diffuse = vmath::vec3(1.0f, 1.0f, 1.0f);
		m_spotLight.specular = vmath::vec3(1.0f, 1.0f, 1.0f);
	}

	// 애플리케이션 끝날 때 호출된다.
	virtual void shutdown()
	{
		glDeleteTextures(3, textures);
		glDeleteBuffers(3, EBO);
		glDeleteBuffers(3, VBO);
		glDeleteVertexArrays(3, VAO);
		glDeleteProgram(shader_program);

		// 같은 GLuint 핸들이 재발급될 때 stale 캐시 결과를 막기 위해 진단 캐시 무효화.
		// (CheckExpectedUniforms/CheckExpectedAttributes 와 UniformDiagnostics 가 program 핸들을 키로 캐시함)
		diag::GLObjectLog::InvalidateProgramCache(shader_program); // diag::
		diag::UniformDiagnostics::Invalidate(shader_program);      // diag::
	}

	// 렌더링 virtual 함수를 작성해서 오버라이딩한다.
	virtual void render(double currentTime)
	{
		const GLfloat black[] = {0.0f, 0.0f, 0.0f, 1.0f};
		glClearBufferfv(GL_COLOR, 0, black);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		// 카메라 매트릭스 계산 (eye 가 viewPos)
		float camdistance = 3.0f;
		vmath::vec3 eye((float)cos(currentTime * 0.1f) * camdistance, 1.5f, (float)sin(currentTime * 0.1f) * camdistance);
		vmath::vec3 center(0.0f, 0.0f, 0.0f);
		vmath::vec3 up(0.0f, 1.0f, 0.0f);
		vmath::mat4 lookAt = vmath::lookat(eye, center, up);
		float fov = 50.0f;
		vmath::mat4 projM = vmath::perspective(fov, (float)info.windowWidth / info.windowHeight, 0.1f, 1000.0f);

		glUseProgram(shader_program);
		{
			vmath::mat4 model = vmath::scale(1.5f);
			glUniformMatrix4fv(UniformLoc(shader_program, "model", glGetUniformLocation(shader_program, "model")), 1, GL_FALSE, model);
			glUniformMatrix4fv(UniformLoc(shader_program, "view", glGetUniformLocation(shader_program, "view")), 1, GL_FALSE, lookAt);
			glUniformMatrix4fv(UniformLoc(shader_program, "projection", glGetUniformLocation(shader_program, "projection")), 1, GL_FALSE, projM);

			const vmath::vec3 objectColor(1.0f, 1.0f, 1.0f);
			glUniform3fv(UniformLoc(shader_program, "viewPos", glGetUniformLocation(shader_program, "viewPos")), 1, eye);
			glUniform3fv(UniformLoc(shader_program, "objectColor", glGetUniformLocation(shader_program, "objectColor")), 1, objectColor);

			UniformsSetMaterial(shader_program, "material", m_material[0]);
			UniformsSetDirLight(shader_program, "dirLight", m_dirLight);
			UniformsSetPointLight(shader_program, "pointLights[0]", m_pointLights[0]);
			UniformsSetPointLight(shader_program, "pointLights[1]", m_pointLights[1]);
			UniformsSetSpotLight(shader_program, "spotLight", m_spotLight);

			BindMaterialTextures(m_material[0]);

			glBindVertexArray(VAO[0]);
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(Engine::Model::QUAD_FACE_INDICES.size()), GL_UNSIGNED_INT, 0);
		}
		glUseProgram(shader_program);
		{
			float orbitRadius = 0.5f;
			vmath::mat4 model = vmath::translate(cosf(vmath::radians(30)) * orbitRadius, 0.0f, sinf(vmath::radians(30)) * orbitRadius) * vmath::rotate(30.0f, vmath::vec3(0.0, 1.0, 0.0));
			glUniformMatrix4fv(UniformLoc(shader_program, "model", glGetUniformLocation(shader_program, "model")), 1, GL_FALSE, model);
			glUniformMatrix4fv(UniformLoc(shader_program, "view", glGetUniformLocation(shader_program, "view")), 1, GL_FALSE, lookAt);
			glUniformMatrix4fv(UniformLoc(shader_program, "projection", glGetUniformLocation(shader_program, "projection")), 1, GL_FALSE, projM);
			const vmath::vec3 objectColor(1.0f, 1.0f, 1.0f);
			glUniform3fv(UniformLoc(shader_program, "viewPos", glGetUniformLocation(shader_program, "viewPos")), 1, eye);
			glUniform3fv(UniformLoc(shader_program, "objectColor", glGetUniformLocation(shader_program, "objectColor")), 1, objectColor);

			UniformsSetMaterial(shader_program, "material", m_material[1]);
			UniformsSetDirLight(shader_program, "dirLight", m_dirLight);
			UniformsSetPointLight(shader_program, "pointLights[0]", m_pointLights[0]);
			UniformsSetPointLight(shader_program, "pointLights[1]", m_pointLights[1]);
			UniformsSetSpotLight(shader_program, "spotLight", m_spotLight);

			BindMaterialTextures(m_material[1]);

			glBindVertexArray(VAO[1]);
			glDrawArrays(GL_TRIANGLES, 0, cube_vertex_count); // BuildCube 는 비인덱스 메시
		}
		glUseProgram(shader_program);
		{
			float orbitRadius = 1.0f;
			vmath::mat4 model = vmath::translate(
			    cosf(vmath::radians(120)) * orbitRadius,
			    1.0f,
			    sinf(vmath::radians(120)) * orbitRadius);
			model *= vmath::rotate(30.0f, vmath::vec3(0.0, 1.0, 0.0));
			model *= vmath::scale(0.3f,0.3f,0.3f);

			glUniformMatrix4fv(UniformLoc(shader_program, "model", glGetUniformLocation(shader_program, "model")), 1, GL_FALSE, model);
			glUniformMatrix4fv(UniformLoc(shader_program, "view", glGetUniformLocation(shader_program, "view")), 1, GL_FALSE, lookAt);
			glUniformMatrix4fv(UniformLoc(shader_program, "projection", glGetUniformLocation(shader_program, "projection")), 1, GL_FALSE, projM);
			const vmath::vec3 objectColor(1.0f, 1.0f, 1.0f);
			glUniform3fv(UniformLoc(shader_program, "viewPos", glGetUniformLocation(shader_program, "viewPos")), 1, eye);
			glUniform3fv(UniformLoc(shader_program, "objectColor", glGetUniformLocation(shader_program, "objectColor")), 1, objectColor);

			UniformsSetMaterial(shader_program, "material", m_material[1]);
			UniformsSetDirLight(shader_program, "dirLight", m_dirLight);
			UniformsSetPointLight(shader_program, "pointLights[0]", m_pointLights[0]);
			UniformsSetPointLight(shader_program, "pointLights[1]", m_pointLights[1]);
			UniformsSetSpotLight(shader_program, "spotLight", m_spotLight);

			BindMaterialTextures(m_material[1]);
			glBindVertexArray(VAO[2]);
			// diag:: 첫 프레임에 1회만 — 매 프레임 호출 시 glGet* GPU stall 유발
			{
				static bool s_dumped = false; // diag::
				if (!s_dumped)
				{
					diag::GLObjectLog::CheckProgramValidate(shader_program, "tetra_draw"); // diag::
					diag::GLStateLog::Dump("before_tetra_draw");                           // diag::
					s_dumped = true;
				}
			}
			glDrawArrays(GL_TRIANGLES, 0, tetrahedron_vertex_count); // BuildTetrahedron 은 비인덱스 메시 (12정점)
			// diag:: draw 후 1회 — GL error 확인 (GL_INVALID_OPERATION 등)
			{
				static bool s_checked = false; // diag::
				if (!s_checked)
				{
					const GLenum err = glGetError();
					std::cerr << "[diag] tetra glDrawArrays count=" << tetrahedron_vertex_count
					          << " GLerr=0x" << std::hex << err << std::dec
					          << (err == GL_NO_ERROR ? " (NO_ERROR)" : " (ERROR)")
					          << std::endl;
					s_checked = true;
				}
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
	GLuint shader_program;                // basic_lighting 단일 프로그램
	GLuint VAO[3], VBO[3], EBO[3];        // [0]=바닥 Quad(인덱스), [1]=Cube(비인덱스)
	GLuint textures[3];                   // [0]=wall.jpg, [1]=container2(diffuse), [2]=container2_specular
	GLsizei cube_vertex_count = 0;        // BuildCube 가 만든 펼친 정점 수 (36)
	GLsizei tetrahedron_vertex_count = 0; // BuildTetrahedron 이 만든 펼친 정점 수 (4면 × 3정점 = 12)

	Material m_material[3];
	DirLight m_dirLight;
	PointLight m_pointLights[2]; // 배열로 선언
	SpotLight m_spotLight;
};

// DECLARE_MAIN의 하나뿐인 인스턴스
DECLARE_MAIN(my_application)