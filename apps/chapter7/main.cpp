#include "GL/gl3w.h"
#include "GL/glcorearb.h"
#include "vmath.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sb7.h>
#include <shader.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;
using namespace vmath;

namespace Engine
{

	static const char *UNIFORM_MODEL_MAT = "inModelMat";
	static const char *UNIFORM_VIEW_MAT = "inViewMat";
	static const char *UNIFORM_PROJ_MAT = "inProjMat";
	static const char *UNIFORM_CURRENT_TIME = "inCurrentTime";
	static const char *UNIFORM_BASE_COLOR = "inBaseColor";

	static constexpr int TEXTURE_SLOT_COUNT = 4;

	static const char *SAMPLER_TEX[TEXTURE_SLOT_COUNT] = {
	    "tex1", "tex2", "tex3", "tex4"};
	static const char *UNIFORM_TEX_USED[TEXTURE_SLOT_COUNT] = {
	    "uTex1Used", "uTex2Used", "uTex3Used", "uTex4Used"};
	static const char *UNIFORM_UV_OFFSET[TEXTURE_SLOT_COUNT] = {
	    "inUVOffset1", "inUVOffset2", "inUVOffset3", "inUVOffset4"};
	static const char *UNIFORM_UV_RATIO[TEXTURE_SLOT_COUNT] = {
	    "inUVRatio1", "inUVRatio2", "inUVRatio3", "inUVRatio4"};

	// Phong 라이팅 uniform 이름
	static const char *UNIFORM_LIGHT_POS = "inLightPos";
	static const char *UNIFORM_LIGHT_COLOR = "inLightColor";
	static const char *UNIFORM_VIEW_POS = "inViewPos";
	static const char *UNIFORM_AMBIENT_STRENGTH = "inAmbientStrength";
	static const char *UNIFORM_SPECULAR_STRENGTH = "inSpecularStrength";
	static const char *UNIFORM_SHININESS = "inShininess";
	static const char *UNIFORM_LIGHTING_ENABLED = "inLightingEnabled";

	inline double ToRadian(double degree)
	{
		return degree * M_PI / 180.0;
	}
} // namespace Engine

namespace Engine::Transform
{
	class Transform
	{
	  public:
		std::string Name;

		vmath::vec3 Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
		vmath::vec3 EulerRot = vmath::vec3(0.0f, 0.0f, 0.0f);
		vmath::vec3 Scale = vmath::vec3(1.0f, 1.0f, 1.0f);

		Transform *Parent = nullptr;
		std::unordered_map<std::string, Transform *> Children;

		vmath::mat4 GetModelMatrix() const
		{
			vmath::mat4 local =
			    vmath::translate<float>(Translate) *
			    vmath::rotate<float>(EulerRot[2], 0.0f, 0.0f, 1.0f) *
			    vmath::rotate<float>(EulerRot[1], 0.0f, 1.0f, 0.0f) *
			    vmath::rotate<float>(EulerRot[0], 1.0f, 0.0f, 0.0f) *
			    vmath::scale<float>(Scale);
			if (Parent == nullptr)
				return local;
			return Parent->GetModelMatrix() * local;
		}
	};
} // namespace Engine::Transform

namespace Engine::Camera
{
	class Camera
	{
	  public:
		Transform::Transform Transform;

		vmath::vec3 Target = vmath::vec3(0.0f, 0.0f, 0.0f);
		vmath::vec3 WorldUp = vmath::vec3(0.0f, 1.0f, 0.0f);

		float Fov = 60.0f;
		float Aspect = 1.0f;
		float NearPlane = 0.1f;
		float FarPlane = 1000.0f;

		vmath::mat4 GetViewMatrix() const
		{
			return vmath::lookat(Transform.Translate, Target, WorldUp);
		}

		vmath::mat4 GetProjMatrix() const
		{
			return vmath::perspective(Fov, Aspect, NearPlane, FarPlane);
		}
	};
} // namespace Engine::Camera

namespace Engine::Material
{
	struct TextureSlot
	{
		GLuint TexAddr;
		vmath::vec2 UVOffset;
		vmath::vec2 UVRatio;

		TextureSlot()
		    : TexAddr(0),
		      UVOffset(vmath::vec2(0.0f, 0.0f)),
		      UVRatio(vmath::vec2(1.0f, 1.0f))
		{
		}
	};

	struct TextureParams
	{
		GLint WrapS = GL_REPEAT;
		GLint WrapT = GL_REPEAT;
		GLint MinFilter = GL_LINEAR_MIPMAP_LINEAR;
		GLint MagFilter = GL_LINEAR;
	};

	class Material
	{
	  public:
		vmath::vec4 BaseColor = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		std::vector<TextureSlot> Slots;

		Material()
		{
		}

		~Material()
		{
			for (auto &slot : Slots)
				if (slot.TexAddr != 0)
					glDeleteTextures(1, &slot.TexAddr);
		}

		// Model 에 결합된 값 타입. 복사 금지 — 같은 GPU 텍스처 핸들이 여러 Material 에
		// 공유되면 소멸 시 double-free 가 발생하기 때문.
		Material(const Material &) = delete;
		Material &operator=(const Material &) = delete;

		/*
		nrChannel
		    1 : 흑백
		    2 : 흑백 + 투명도
		    3 : RGB
		    4 : RGBA
		format
		    format 은 실제 텍스처와 동일한 형식으로 반드시 지정해야 한다!
		internal_format
		    OpenGL 에게 텍스처를 어떤 형식으로 저장할 것인지 결정.

		예를들어
		    JPG : 투명도 지원 X  -> format = GL_RGB
		    PNG : 투명도 지원     -> format = GL_RGBA
		*/
		void LoadTexture(const char *image_path,
		                 GLuint format = GL_RGB,
		                 GLint internal_format = GL_RGB,
		                 const TextureParams &params = TextureParams{})
		{
			TextureSlot slot;

			glGenTextures(1, &slot.TexAddr);
			glBindTexture(GL_TEXTURE_2D, slot.TexAddr);

			int width, height, nrChannels;
			unsigned char *data = stbi_load(image_path, &width, &height, &nrChannels, 0);
			if (data == 0)
			{
				std::cerr << "텍스쳐 로드 실패 : " << image_path << std::endl;
			}
			if (data)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0,
				             format, GL_UNSIGNED_BYTE, data);
				glGenerateMipmap(GL_TEXTURE_2D);
			}
			stbi_image_free(data);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, params.WrapS);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, params.WrapT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, params.MinFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, params.MagFilter);

			Slots.push_back(slot);
		}

		int GetSlotCount() const
		{
			return (int)Slots.size();
		}
	};
} // namespace Engine::Material

namespace Engine::Lighting
{
	/* Phong 라이팅 파라미터 묶음.
	   Position    : 월드 공간 광원 위치 (point light)
	   Color       : 광원 색 / 강도 (RGB, 보통 0~1, HDR 시 그 이상)
	   AmbientStrength  : 주변광 비율 (0~1, 보통 0.1)
	   SpecularStrength : 반사광 비율 (0~1, 보통 0.5)
	   Shininess        : 반사 광택 지수 (작을수록 흐리고 넓게, 클수록 날카롭게) */
	class Light
	{
	  public:
		vmath::vec3 Position = vmath::vec3(2.0f, 2.0f, 2.0f);
		vmath::vec3 Color = vmath::vec3(1.0f, 1.0f, 1.0f);
		float AmbientStrength = 0.1f;
		float SpecularStrength = 0.5f;
		float Shininess = 32.0f;

		// Light 파라미터 + 카메라 위치를 쉐이더 uniform 으로 푸시.
		// glUseProgram 이 끝난 다음에 호출해야 함.
		void Apply(GLuint progAddr, const vmath::vec3 &viewPos) const
		{
			glUniform3fv(glGetUniformLocation(progAddr, UNIFORM_LIGHT_POS),
			             1, Position);
			glUniform3fv(glGetUniformLocation(progAddr, UNIFORM_LIGHT_COLOR),
			             1, Color);
			glUniform3fv(glGetUniformLocation(progAddr, UNIFORM_VIEW_POS),
			             1, viewPos);
			glUniform1f(glGetUniformLocation(progAddr, UNIFORM_AMBIENT_STRENGTH),
			            AmbientStrength);
			glUniform1f(glGetUniformLocation(progAddr, UNIFORM_SPECULAR_STRENGTH),
			            SpecularStrength);
			glUniform1f(glGetUniformLocation(progAddr, UNIFORM_SHININESS),
			            Shininess);
			glUniform1f(glGetUniformLocation(progAddr, UNIFORM_LIGHTING_ENABLED),
			            1.0f);
		}

		// 라이팅을 끄는 경로 — 다른 모든 uniform 은 쉐이더에서 무시됨.
		static void ApplyDisabled(GLuint progAddr)
		{
			glUniform1f(glGetUniformLocation(progAddr, UNIFORM_LIGHTING_ENABLED),
			            0.0f);
		}
	};
} // namespace Engine::Lighting

namespace Engine::Model
{
	/* 정점 레이아웃
	   GPU 에 올리는 정점 하나 = Position(vec4) + Color(vec4) + UV(vec2) + Normal(vec3) interleaved.
	   VBO 안에서 [px py pz pw  r g b a  s t  nx ny nz] 형태 — 13 float / vertex.
	   stride = VERTEX_LEN * sizeof(float).
	   glVertexAttribPointer 로 location 0/1/2/3 에 각각 position/color/uv/normal 을 연결. */
	static const int VERTEX_POSITION_SIZE = 4; // (x, y, z, w) : w=1 이면 점, w = 0 이면 방향 벡터
	static const int VERTEX_COLOR_SIZE = 4;    // (r, g, b, a) : vertex color. FS 에서 texture 와 곱해짐
	static const int VERTEX_NORMAL_SIZE = 3;   // (nx, ny, nz) : 정점 노멀, 라이팅 계산용
	static const int VERTEX_UV_SIZE = 2;       // (s, t) : 텍스처 좌표 (보통 0 ~ 1)
	static constexpr int VERTEX_LEN =
	    VERTEX_POSITION_SIZE + VERTEX_COLOR_SIZE + VERTEX_NORMAL_SIZE + VERTEX_UV_SIZE; // = 13

	/* CCW 정렬된 3정점에서 face normal 계산 (외적 + 정규화)
	   p0, p1, p2 가 CCW 순서일 때 e1 × e2 가 바깥쪽 법선.
	   Lambert / Phong 라이팅의 N 벡터로 그대로 사용. */
	inline vmath::vec3 ComputeFaceNormal(const vmath::vec4 &p0,
	                                     const vmath::vec4 &p1,
	                                     const vmath::vec4 &p2)
	{
		vmath::vec3 e1 = vmath::vec3(p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]);
		vmath::vec3 e2 = vmath::vec3(p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]);
		return vmath::normalize(vmath::cross(e1, e2));
	}

	/* 공용 색/UV 상수 */
	static const vmath::vec4 BG_COLOR = vmath::vec4(0.0f, 0.0f, 0.0f, 1.0f); // glClearBufferfv 로 화면 지울 때의 배경색
	static const std::vector<vmath::vec4> ALL_WHITE_4(4, vmath::vec4(1.0f)); // 3정점 메쉬(Triangle 등) 용 흰색 팔레트
	static const std::vector<vmath::vec4> ALL_WHITE_6(6, vmath::vec4(1.0f)); // 6정점(=Quad=2tri) 메쉬 용 흰색 팔레트

	/* 삼각형 3정점용 UV : 밑변 (0,0)(1,0), 꼭대기 (0.5, 1) */
	static const std::vector<vmath::vec2> BASE_TRIANGLE_MESH_UVS{
	    {0.0, 0.0}, {1.0, 0.0}, {0.5, 1.0}};

	/* 위를 v 축으로 뒤집은 버전 : 거울/뒤집힌 면(Octahedron 아래절반)에 사용 */
	static const std::vector<vmath::vec2> BASE_TRIANGLE_INV_MESH_UVS{
	    {0.0, 1.0}, {1.0, 1.0}, {0.5, 0.0}};

	/* 쿼드 4코너 UV : 텍스처 전체를 사각면에 한 번 매핑. 6정점으로 풀 때 QUAD_MESH_UVS_FAN 사용 */
	static const std::vector<vmath::vec2> BASE_QUAD_MESH_UVS{
	    {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

	/* Face 인덱스 템플릿
	   OpenGL 기본 front face 규약 = CCW(반시계). glCullFace 와 glFrontFace 설정에 따라
	   앞/뒷면 판정이 달라짐. 같은 정점 집합에서 winding 만 뒤집으면 법선이 반대가 됨
	   (예: skybox 처럼 "cube 안쪽" 만 보이도록 할 때 _BACK 사용). */
	const std::vector<GLuint> TRIANGLE_FACE_INDICES = {0, 1, 2};           // CCW : 바깥 면
	const std::vector<GLuint> TRIANGLE_FACE_INDICES_BACK = {0, 2, 1};      // CW  : 안쪽 면
	const std::vector<GLuint> QUAD_FACE_INDICES = {0, 1, 2, 3, 4, 5};      // 2 tri (CCW)
	const std::vector<GLuint> QUAD_FACE_INDICES_BACK = {0, 2, 1, 3, 5, 4}; // 2 tri (CW)

	/* 쿼드 4코너 UV 를 6정점(2 tri) 슬롯에 펼치는 매핑 : (0,1,2)+(0,2,3) 팬 패턴 */
	const std::vector<GLuint> QUAD_MESH_UVS_FAN = {0, 1, 2, 0, 2, 3};

	/* 정삼각형 3정점 : XY 평면 (z=0). 밑변 길이 1, 높이 sqrt(3)/2 = 0.866 */
	static const std::vector<vmath::vec4> TRIANGLE_BASE_POSITION = {
	    {0.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 0.0, 1.0},
	    {0.5, 0.866, 0.0, 1.0},
	};

	/* Tetrahedron : 정사면체 (4 정점 / 4개 옆면)
	   0~2: 밑면 정삼각형 (y=0, xz 평면), 3: 꼭대기 (무게중심 위 y = 0.816) */
	static const std::vector<vmath::vec4> TETRA_BASE_POSITION = {
	    {0.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 0.0, 1.0},
	    {0.5, 0.0, 0.866, 1.0},
	    {0.5, 0.816, 0.2886, 1.0},
	};

	/* 4개 옆면 : 각 {i0, i1, i2} 가 CCW 순서라 법선이 바깥쪽 */
	static const std::vector<std::vector<GLuint>> TETRA_FACE_INDICES = {
	    {1, 0, 3}, {2, 1, 3}, {0, 2, 3}, {1, 0, 2}};

	/* Cone : 사각뿔 (실제로는 4 옆면 + 바닥 쿼드 조합으로 그림)
	   0~3: 바닥 정사각형 4코너 (y=0), 4(y=1, 중심 위) */
	static const std::vector<vmath::vec4> CONE_SIDE_BASE_POSITION = {
	    {0.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 1.0, 1.0},
	    {0.0, 0.0, 1.0, 1.0},
	    {0.5, 1.0, 0.5, 1.0}};

	/* 바닥 쿼드 전용 : SIDE 의 0~3 을 재사용 (Quad 빌더에 넘기기 위해 분리) */
	static const std::vector<vmath::vec4> CONE_BOTTOM_BASE_POSITION = {
	    CONE_SIDE_BASE_POSITION[0],
	    CONE_SIDE_BASE_POSITION[1],
	    CONE_SIDE_BASE_POSITION[2],
	    CONE_SIDE_BASE_POSITION[3]};

	/* 4 옆면 삼각형 : 각 면의 세 번째 인덱스 = 4. CCW 로 바깥 법선 */
	static const std::vector<std::vector<GLuint>> CONE_SIDE_FACE_INDICES = {
	    {1, 0, 4}, {2, 1, 4}, {3, 2, 4}, {0, 3, 4}};

	/* XY 평면 정사각형 4코너 (z=0) : 평면 메쉬 (스프라이트 등) 용 */
	static const std::vector<vmath::vec4> QUAD_BASE_POSITION = {
	    {0.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 0.0, 1.0},
	    {1.0, 1.0, 0.0, 1.0},
	    {0.0, 1.0, 0.0, 1.0},
	};

	/* QUAD_BASE_POSITION 4 코너를 2 tri 팬 패턴(0,1,2,0,2,3)으로 미리 펼친 6 정점 배열.
	   BuildQuad 의 positions 인자에 바로 넘기고 position_idxs 는 identity(QUAD_FACE_INDICES)
	   모두 XY 평면 (z=0), CCW +Z 뷰.
	   주의할 점: 혼자서는 "하나의 평면 쿼드" 일 뿐 : cube 6 면은 FACED_CUBE_QUAD_BASE_POSITION[f] 사용. */
	static const std::vector<vmath::vec4> FACED_QUAD_BASE_POSITION = {
	    {QUAD_BASE_POSITION[0],
	     QUAD_BASE_POSITION[1],
	     QUAD_BASE_POSITION[2],
	     QUAD_BASE_POSITION[0],
	     QUAD_BASE_POSITION[2],
	     QUAD_BASE_POSITION[3]}};

	/* Cube : 정육면체 8 코너 BuildCube 의 offset=-0.5 로 원점 중심화. */
	static const std::vector<vmath::vec4> CUBE_BASE_POSITIONS = {
	    {0.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 1.0, 1.0},
	    {0.0, 0.0, 1.0, 1.0},
	    {0.0, 1.0, 0.0, 1.0},
	    {1.0, 1.0, 0.0, 1.0},
	    {1.0, 1.0, 1.0, 1.0},
	    {0.0, 1.0, 1.0, 1.0}};

	/* Cube 6 면별로 CUBE_FACE_INDICES 를 미리 펼쳐둔 "6정점 완성형" 쿼드 배열. */
	static const std::vector<vmath::vec4> FACED_CUBE_QUAD_BASE_POSITION[6] = {
	    {CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[4], CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[4], CUBE_BASE_POSITIONS[5]},
	    {CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[6]},
	    {CUBE_BASE_POSITIONS[3], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[6], CUBE_BASE_POSITIONS[3], CUBE_BASE_POSITIONS[6], CUBE_BASE_POSITIONS[7]},
	    {CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[3], CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[4]},
	    {CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[3]},
	    {CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[6], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[4]}};

	/* Cube 6 면 인덱스 : 각 면은 6정점 CCW(바깥 법선) 순서.
	   face index 0~5 의 법선 방향: -Z, +X, +Z, -X, -Y, +Y
	   반대 winding(내부에서 보이는 면, 예: skybox) 은 QUAD_FACE_INDICES_BACK 조합. */
	static const std::vector<std::vector<GLuint>> CUBE_FACE_INDICES = {
	    {1, 0, 4, 1, 4, 5}, // -Z
	    {2, 1, 5, 2, 5, 6}, // +X
	    {3, 2, 6, 3, 6, 7}, // +Z
	    {0, 3, 7, 0, 7, 4}, // -X
	    {0, 1, 2, 0, 2, 3}, // -Y
	    {7, 6, 5, 7, 5, 4}  // +Y
	};

	/* 정점 빌더 */

	/* 정점 1개 추가 : VBO 에 올릴 interleaved 요소 넣기
	   vertices: 출력 버퍼 (13 float 씩 늘어남)
	   pos:      모델 로컬 좌표 (vec4, w 유지)
	   color:    vertex RGBA
	   uv:       텍스처 좌표 (s, t)
	   normal:   라이팅용 법선 (정규화된 모델 로컬 좌표)
	   offset:   pos.xyz 에 더할 평행이동 (예: -0.5 -> 원점 중심화) */
	void PushVertex(std::vector<GLfloat> &vertices,
	                const vmath::vec4 pos,
	                const vmath::vec4 color,
	                const vmath::vec3 normal,
	                const vmath::vec2 uv,
	                const vmath::vec3 &offset)
	{
		vertices.push_back(pos[0] + offset[0]);
		vertices.push_back(pos[1] + offset[1]);
		vertices.push_back(pos[2] + offset[2]);
		vertices.push_back(pos[3]);
		vertices.push_back(color[0]);
		vertices.push_back(color[1]);
		vertices.push_back(color[2]);
		vertices.push_back(color[3]);
		vertices.push_back(normal[0]);
		vertices.push_back(normal[1]);
		vertices.push_back(normal[2]);
		vertices.push_back(uv[0]);
		vertices.push_back(uv[1]);
	}

	/* 삼각형 1 개(3 정점) 를 buffer_data 에 추가.
	   buffer_data:   출력 VBO 용 float 배열
	   positions:     정점 좌표 테이블 (예: TETRA_BASE_POSITION)
	   colors:        vertex RGBA 3 개
	   uvs:           vertex UV 3 개
	   position_idxs: positions[] 에서 이 면이 참조할 3 개 정점의 인덱스
	   offset:        평행이동 (원점 중심화 등)
	   face_idxs:     winding. 기본 TRIANGLE_FACE_INDICES(CCW), _BACK 이면 CW 로 뒤집음 */
	void BuildTriangle(
	    std::vector<GLfloat> &buffer_data,
	    const std::vector<vmath::vec4> &positions,
	    const std::vector<vmath::vec4> &colors,
	    const std::vector<vmath::vec2> &uvs,
	    const std::vector<GLuint> &position_idxs,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    const std::vector<GLuint> &face_idxs = TRIANGLE_FACE_INDICES)
	{
		// face_idxs winding 순서대로 3정점 -> CCW edge 외적으로 face normal 계산.
		// flat-shading : 같은 면의 3정점 모두 동일한 normal 공유.
		const vmath::vec4 &fp0 = positions[position_idxs[face_idxs[0]]];
		const vmath::vec4 &fp1 = positions[position_idxs[face_idxs[1]]];
		const vmath::vec4 &fp2 = positions[position_idxs[face_idxs[2]]];
		const vmath::vec3 faceNormal = ComputeFaceNormal(fp0, fp1, fp2);

		for (int i = 0; i < 3; i++)
		{
			GLuint k = face_idxs[i];
			PushVertex(buffer_data,
			           positions[position_idxs[k]],
			           colors[k],
			           faceNormal,
			           uvs[k],
			           offset);
		}
	}

	/* 쿼드 1 개(6 정점) 를 buffer_data 에 추가.
	   positions:     정점 좌표 테이블 (예: CUBE_BASE_POSITIONS)
	   colors:        vertex RGBA 6 개
	   uvs:           4 코너 UV (내부에서 QUAD_MESH_UVS_FAN 로 6 슬롯에 펼침)
	   position_idxs: positions[] 에서 이 쿼드가 참조할 6 개 인덱스 (예: CUBE_FACE_INDICES[f])
	   offset:        평행이동
	   face_idxs:     winding. QUAD_FACE_INDICES(CCW) or _BACK(CW) */
	void BuildQuad(
	    std::vector<GLfloat> &buffer_data,
	    const std::vector<vmath::vec4> &positions,
	    const std::vector<vmath::vec4> &colors,
	    const std::vector<vmath::vec2> &uvs,
	    const std::vector<GLuint> &position_idxs,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    const std::vector<GLuint> &face_idxs = QUAD_FACE_INDICES)
	{
		// 평면 quad 가정 — 첫 3정점만으로 면 법선 계산해서 6정점에 동일 적용 (flat shading).
		const vmath::vec4 &fp0 = positions[position_idxs[face_idxs[0]]];
		const vmath::vec4 &fp1 = positions[position_idxs[face_idxs[1]]];
		const vmath::vec4 &fp2 = positions[position_idxs[face_idxs[2]]];
		const vmath::vec3 faceNormal = ComputeFaceNormal(fp0, fp1, fp2);

		for (int i = 0; i < 6; i++)
		{
			GLuint k = face_idxs[i];
			PushVertex(buffer_data,
			           positions[position_idxs[k]],
			           colors[k],
			           faceNormal,
			           uvs[QUAD_MESH_UVS_FAN[k]],
			           offset);
		}
	}

	/* 정육면체 6 면 모두 buffer_data 에 추가.
	   offset:    기본 -0.5 -> 로컬 0 ~ 1  큐브를 원점 중심 0,0,0 로 정렬
	   back_face: true 면 모든 면 winding 반전 (skybox 처럼 안쪽에서 보이게) */
	void BuildCube(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false)
	{
		const auto &face_idxs = back_face ? QUAD_FACE_INDICES_BACK : QUAD_FACE_INDICES;
		for (int f = 0; f < 6; f++)
			BuildQuad(buffer_data, CUBE_BASE_POSITIONS, ALL_WHITE_6,
			          BASE_QUAD_MESH_UVS, CUBE_FACE_INDICES[f], offset, face_idxs);
	}

	/* 사각뿔 : 4 옆면(삼각형) + 바닥 쿼드.
	   offset:    평행이동
	   back_face: winding 반전 (안쪽에서 보이게) */
	void BuildCone(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false)
	{
		const auto &tri_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
		const auto &quad_idxs = back_face ? QUAD_FACE_INDICES_BACK : QUAD_FACE_INDICES;
		for (int f = 0; f < 4; f++)
			BuildTriangle(buffer_data, CONE_SIDE_BASE_POSITION, ALL_WHITE_4,
			              BASE_TRIANGLE_MESH_UVS, CONE_SIDE_FACE_INDICES[f], offset, tri_idxs);
		BuildQuad(buffer_data, CONE_BOTTOM_BASE_POSITION, ALL_WHITE_6,
		          BASE_QUAD_MESH_UVS, QUAD_MESH_UVS_FAN, offset, quad_idxs);
	}

	/* 정사면체 */
	void BuildTetrahedron(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false)
	{
		const auto &tri_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
		for (int f = 0; f < 4; f++)
			BuildTriangle(buffer_data, TETRA_BASE_POSITION, ALL_WHITE_4,
			              BASE_TRIANGLE_MESH_UVS, TETRA_FACE_INDICES[f], offset, tri_idxs);
	}

	/* 정팔면체 : 위·아래 사각뿔을 붙인 모양 (CONE_SIDE 를 y 축 거울상으로 복제).
	   offset:    평행이동
	   back_face: winding 반전 */
	void BuildOctahedron(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false)
	{
		// 기본은 윗절반=FRONT, 아랫절반=BACK (xz 거울상이라 서로 반대 방향).
		// back_face=true 면 둘 다 반대로 뒤집음.
		const auto &top_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
		const auto &bottom_idxs = back_face ? TRIANGLE_FACE_INDICES : TRIANGLE_FACE_INDICES_BACK;

		for (int f = 0; f < 4; f++)
			BuildTriangle(buffer_data, CONE_SIDE_BASE_POSITION, ALL_WHITE_4,
			              BASE_TRIANGLE_MESH_UVS, CONE_SIDE_FACE_INDICES[f], offset, top_idxs);

		std::vector<vmath::vec4> coneDownSideBasePosition;
		vmath::mat4 xzMirrorMat = vmath::mat4::identity();
		xzMirrorMat[1][1] = -1;
		for (const auto &pos : CONE_SIDE_BASE_POSITION)
			coneDownSideBasePosition.push_back(pos * xzMirrorMat);

		for (int f = 0; f < 4; f++)
			BuildTriangle(buffer_data, coneDownSideBasePosition, ALL_WHITE_4,
			              BASE_TRIANGLE_INV_MESH_UVS, CONE_SIDE_FACE_INDICES[f],
			              offset, bottom_idxs);
	}

	/* 파라메트릭 서피스 */

	/* 원판/고리(ring) 파라메트릭 서피스 : XZ 평면, y = 0.
	   us, ue:  시작/끝 각도 [rad]. (0, 2 * M_PI) 면 완전한 원
	   uRes:    각도 분할 수 : 정점은 uRes+1 개 (끝이 시작과 겹침)
	   vs, ve:  반지름 비율 0 ~ 1. vs=0 -> 꽉 찬 디스크, vs>0 -> 고리(ring)
	   vRes:    반지름 분할 수 (쿼드 스트립 row 개수 = vRes)
	   radius:  최대 반지름 (ve 지점의 실제 크기)
	   offset:  평행이동
	   back_face: winding 반전 */
	void BuildDisk(std::vector<GLfloat> &buffer_data,
	               double us, double ue, int uRes, // 각도 (0 ~ 2*M_PI)
	               double vs, double ve, int vRes, // 반지름 비율 (0 ~ 1)
	               float radius = 1.0f,
	               const vmath::vec3 &offset = vmath::vec3(0.0f, 0.0f, 0.0f),
	               bool back_face = false)
	{
		int numCols = uRes + 1;
		int numRows = vRes + 1;

		std::vector<vmath::vec4> positions;
		std::vector<vmath::vec4> colors;
		std::vector<vmath::vec2> uvs;

		double deltaRad = (ve - vs) / vRes;
		double deltaAngle = (ue - us) / uRes;

		for (int row = 0; row < numRows; row++)
		{
			for (int col = 0; col < numCols; col++)
			{
				double currentRad = (vs + row * deltaRad) * radius;
				double currentAngle = (us + col * deltaAngle);

				positions.push_back(vmath::vec4(
				    currentRad * cos(currentAngle),
				    0.0,
				    -currentRad * sin(currentAngle),
				    1.0f));
				uvs.push_back(vmath::vec2((float)col / uRes, (float)row / vRes));
				colors.push_back(vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));
			}
		}

		// XZ 평면 디스크의 법선 = +Y (CCW front) / -Y (back_face)
		const vmath::vec3 diskNormal = back_face ? vmath::vec3(0.0f, -1.0f, 0.0f)
		                                         : vmath::vec3(0.0f, 1.0f, 0.0f);

		for (int row = 0; row < vRes; row++)
		{
			for (int col = 0; col < uRes; col++)
			{
				int p0 = row * numCols + col;
				int p1 = row * numCols + (col + 1);
				int p2 = (row + 1) * numCols + (col + 1);
				int p3 = (row + 1) * numCols + col;
				int indices_front[] = {p0, p1, p2, p0, p2, p3}; // CCW
				int indices_back[] = {p0, p2, p1, p0, p3, p2};  // CW
				const int *indices = back_face ? indices_back : indices_front;
				for (int i = 0; i < 6; i++)
					PushVertex(buffer_data, positions[indices[i]], colors[indices[i]], diskNormal,uvs[indices[i]], offset);
			}
		}
	}

	/* 원기둥 옆면 — XZ 평면에 base, +Y 방향으로 높이 height.
	   us, ue, uRes: 원주 각도 범위 [rad] 와 분할 수 (uRes+1 정점)
	   vs, ve, vRes: 높이 비율 (0 ~ 1) 와 분할 수. 실제 y = vs..ve 가 * height
	   radius:  기둥 반지름
	   height:  기둥 높이
	   offset:  평행이동
	   back_face: winding 반전 (기본 = 바깥에서 보이게) */
	void BuildCylinder(std::vector<GLfloat> &buffer_data,
	                   double us, double ue, int uRes, // 각도 (0 ~ 2*M_PI)
	                   double vs, double ve, int vRes, // 높이 비율 (0 ~ 1)
	                   float radius = 1.0f,
	                   float height = 1.0f,
	                   const vmath::vec3 &offset = vmath::vec3(0.0f, 0.0f, 0.0f),
	                   bool back_face = false)
	{
		int numCols = uRes + 1;
		int numRows = vRes + 1;

		std::vector<vmath::vec4> positions;
		std::vector<vmath::vec4> colors;
		std::vector<vmath::vec2> uvs;
		std::vector<vmath::vec3> normals;

		double deltaV = (ve - vs) / vRes;
		double deltaAngle = (ue - us) / uRes;

		for (int row = 0; row < numRows; row++)
		{
			for (int col = 0; col < numCols; col++)
			{
				double currentV = (vs + row * deltaV) * height;
				double currentAngle = (us + col * deltaAngle);

				positions.push_back(vmath::vec4(
				    radius * cos(currentAngle),
				    currentV,
				    -radius * sin(currentAngle),
				    1.0f));
				uvs.push_back(vmath::vec2((float)col / uRes, (float)row / vRes));
				colors.push_back(vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));
				// 옆면 법선은 축에서 바깥으로 — y 성분 0, xz 만 라디알.
				// back_face 면 안쪽으로 뒤집힌 법선 사용.
				vmath::vec3 outN(cos(currentAngle), 0.0f, -sin(currentAngle));
				normals.push_back(back_face ? -outN : outN);
			}
		}

		for (int row = 0; row < vRes; row++)
		{
			for (int col = 0; col < uRes; col++)
			{
				int p0 = row * numCols + col;
				int p1 = row * numCols + (col + 1);
				int p2 = (row + 1) * numCols + (col + 1);
				int p3 = (row + 1) * numCols + col;
				int indices_front[] = {p0, p1, p2, p0, p2, p3};
				int indices_back[] = {p0, p2, p1, p0, p3, p2};
				const int *indices = back_face ? indices_back : indices_front;
				for (int i = 0; i < 6; i++)
					PushVertex(buffer_data, positions[indices[i]], colors[indices[i]], normals[indices[i]], uvs[indices[i]], offset);
			}
		}
	}

	/* 반구 (북반구, +Y 방향) — 중심 원점의 구 절반.
	   us, ue, uRes: 경도 [rad] 범위와 분할 (uRes+1 정점)
	   vs, ve, vRes: 위도 비율 0 ~ 1 — 내부에서 ( * M_PI/2), 0 = 적도(y=0), 1 = 북극(y=radius)
	   radius:  구 반지름
	   offset:  평행이동
	   back_face: winding 반전 */
	void BuildHemiSphere(std::vector<GLfloat> &buffer_data,
	                     double us, double ue, int uRes, // 경도 (0 ~ 2*M_PI)
	                     double vs, double ve, int vRes, // 위도 비율 (0 ~ 1 -> PI/2)
	                     float radius = 1.0f,
	                     const vmath::vec3 &offset = vmath::vec3(0.0f, 0.0f, 0.0f),
	                     bool back_face = false)
	{
		int numCols = uRes + 1;
		int numRows = vRes + 1;

		std::vector<vmath::vec4> positions;
		std::vector<vmath::vec4> colors;
		std::vector<vmath::vec2> uvs;
		std::vector<vmath::vec3> normals;

		double deltaV = (ve - vs) / vRes;
		double deltaAngle = (ue - us) / uRes;

		for (int row = 0; row < numRows; row++)
		{
			for (int col = 0; col < numCols; col++)
			{
				double currentV = vs + row * deltaV;
				double latitude = currentV * (M_PI / 2.0);
				double currentAngle = (us + col * deltaAngle);

				double r = radius * cos(latitude);
				double y = radius * sin(latitude);

				float px = (float)(r * cos(currentAngle));
				float py = (float)y;
				float pz = (float)(-r * sin(currentAngle));

				positions.push_back(vmath::vec4(px, py, pz, 1.0f));
				uvs.push_back(vmath::vec2((float)col / uRes, (float)row / vRes));
				colors.push_back(vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));
				// 중심이 원점인 구면 — 법선은 위치 벡터를 정규화한 값.
				vmath::vec3 outN = vmath::normalize(vmath::vec3(px, py, pz));
				normals.push_back(back_face ? -outN : outN);
			}
		}

		for (int row = 0; row < vRes; row++)
		{
			for (int col = 0; col < uRes; col++)
			{
				int p0 = row * numCols + col;
				int p1 = row * numCols + (col + 1);
				int p2 = (row + 1) * numCols + (col + 1);
				int p3 = (row + 1) * numCols + col;
				int indices_front[] = {p0, p1, p2, p0, p2, p3};
				int indices_back[] = {p0, p2, p1, p0, p3, p2};
				const int *indices = back_face ? indices_back : indices_front;
				for (int i = 0; i < 6; i++)
					PushVertex(buffer_data, positions[indices[i]], colors[indices[i]], normals[indices[i]], uvs[indices[i]], offset);
			}
		}
	}

	/* ModelBase */

	class ModelBase
	{
	  protected:
		GLuint mVAOAddr;
		GLuint mVBOAddr;
		GLuint mEBOAddr;

		std::vector<GLfloat> mBufferData;
		std::vector<GLuint> mElementData;
		GLuint mIndexCount;

		bool isBuilted = false;

		Transform::Transform mTransform;
		Material::Material mMaterial;

	  public:
		Transform::Transform &GetTransform()
		{
			return mTransform;
		}
		Material::Material &GetMaterial()
		{
			return mMaterial;
		}

		GLuint GetVAOAddr() const
		{
			return mVAOAddr;
		}
		GLuint GetIndexCount() const
		{
			return mIndexCount;
		}

		ModelBase()
		{
		}

		virtual ~ModelBase()
		{
			Deconstruct();
		}

		void Deconstruct()
		{
			if (!isBuilted)
				return;
			glDeleteBuffers(1, &mEBOAddr);
			glDeleteBuffers(1, &mVBOAddr);
			glDeleteVertexArrays(1, &mVAOAddr);
			isBuilted = false;
		}

		void Build(const std::vector<GLfloat> &buffer_data)
		{
			if (isBuilted)
				return;
			if (buffer_data.empty())
			{
				std::cerr << "buffer_data 가 비어있음" << std::endl;
			}
			if (buffer_data.size() % VERTEX_LEN != 0)
			{
				std::cerr << "buffer_data 크기가 VERTEX_LEN("
				          << VERTEX_LEN << ") 배수가 아님 : " << buffer_data.size() << std::endl;
			}
			mBufferData = std::vector<GLfloat>(buffer_data);

			mIndexCount = mBufferData.size() / VERTEX_LEN;
			for (GLuint i = 0; i < mIndexCount; i++)
				mElementData.push_back(i);

			glGenVertexArrays(1, &mVAOAddr);
			glBindVertexArray(mVAOAddr);

			glGenBuffers(1, &mVBOAddr);
			glBindBuffer(GL_ARRAY_BUFFER, mVBOAddr);
			glBufferData(GL_ARRAY_BUFFER,
			             mBufferData.size() * sizeof(GLfloat),
			             mBufferData.data(), GL_STATIC_DRAW);

			glGenBuffers(1, &mEBOAddr);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBOAddr);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			             mElementData.size() * sizeof(GLuint),
			             mElementData.data(), GL_STATIC_DRAW);

			GLuint stride = VERTEX_LEN * sizeof(GLfloat);
			void *poffset = (void *)0;
			void *coffset = (void *)(VERTEX_POSITION_SIZE * sizeof(GLfloat));
			void *noffset = (void *)((VERTEX_POSITION_SIZE + VERTEX_COLOR_SIZE) * sizeof(GLfloat));
			void *uvoffset = (void *)((VERTEX_POSITION_SIZE + VERTEX_COLOR_SIZE + VERTEX_NORMAL_SIZE) * sizeof(GLfloat));

			glVertexAttribPointer(0, VERTEX_POSITION_SIZE, GL_FLOAT, false, stride, poffset);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, VERTEX_COLOR_SIZE, GL_FLOAT, false, stride, coffset);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(2, VERTEX_NORMAL_SIZE, GL_FLOAT, false, stride, noffset);
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(3, VERTEX_UV_SIZE, GL_FLOAT, false, stride, uvoffset);
			glEnableVertexAttribArray(3);
			isBuilted = true;
		}

		void Draw(GLuint progAddr) const
		{
			glUniformMatrix4fv(glGetUniformLocation(progAddr, UNIFORM_MODEL_MAT),
			                   1, false, mTransform.GetModelMatrix());
			glUniform4fv(glGetUniformLocation(progAddr, UNIFORM_BASE_COLOR),
			             1, mMaterial.BaseColor);
			glBindVertexArray(mVAOAddr);

			// 슬롯 i -> 텍스처 유닛 i. 쉐이더의 tex(i+1) 샘플러가 해당 유닛을 읽도록
			// glUniform1i 로 연결. 슬롯 수를 넘는 자리는 플래그 0 으로 꺼둠.
			const GLuint texture_enums[] = {
			    GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2, GL_TEXTURE3};
			for (int i = 0; i < TEXTURE_SLOT_COUNT; i++)
			{
				if (i < mMaterial.GetSlotCount())
				{
					const auto &slot = mMaterial.Slots[i];
					glActiveTexture(texture_enums[i]);
					glBindTexture(GL_TEXTURE_2D, slot.TexAddr);
					glUniform1i(glGetUniformLocation(progAddr, SAMPLER_TEX[i]), i);
					glUniform1f(glGetUniformLocation(progAddr, UNIFORM_TEX_USED[i]), 1.0f);
					glUniform2fv(glGetUniformLocation(progAddr, UNIFORM_UV_OFFSET[i]),
					             1, slot.UVOffset);
					glUniform2fv(glGetUniformLocation(progAddr, UNIFORM_UV_RATIO[i]),
					             1, slot.UVRatio);
				}
				else
				{
					glActiveTexture(texture_enums[i]);
					glBindTexture(GL_TEXTURE_2D, 0);
					glUniform1i(glGetUniformLocation(progAddr, SAMPLER_TEX[i]), i);
					glUniform1f(glGetUniformLocation(progAddr, UNIFORM_TEX_USED[i]), 0.0f);
				}
			}

			glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0);
		}
	};
} // namespace Engine::Model

namespace Engine::Program
{
	using namespace Model;

	class ShaderProgram
	{
	  public:
		GLuint ProgAddr;
		std::unordered_map<std::string, std::unique_ptr<ModelBase>> Models;

		// 라이팅이 필요한 쉐이더만 셋팅하면 됨. nullptr 이면 쉐이더에서
		// inLightingEnabled=0 으로 라이팅 끄도록 ApplyDisabled() 가 동작.
		Lighting::Light *AttachedLight = nullptr;

		ShaderProgram(const char *vs_path, const char *fs_path)
		{
			ProgAddr = glCreateProgram();
			GLuint vsAddr = sb7::shader::load(vs_path, GL_VERTEX_SHADER, true);
			if (vsAddr == 0)
			{
				std::cerr << "버텍스 쉐이더 로드 실패 : " << vs_path << std::endl;
				exit(1);
			}
			GLuint fsAddr = sb7::shader::load(fs_path, GL_FRAGMENT_SHADER, true);
			if (fsAddr == 0)
			{
				std::cerr << "프래그먼트 쉐이더 로드 실패 : " << fs_path << std::endl;
				exit(1);
			}
			glAttachShader(ProgAddr, vsAddr);
			glAttachShader(ProgAddr, fsAddr);
			glLinkProgram(ProgAddr);
			glDeleteShader(vsAddr);
			glDeleteShader(fsAddr);

		}

		virtual ~ShaderProgram()
		{
			glDeleteProgram(ProgAddr);
		}

		// 모델을 프로그램에 등록하고 Transform 계층을 parent 에 연결한다.
		// - name 을 모델의 Transform.Name 으로도 셋팅
		// - parent 가 nullptr 이 아니면 parent->Children[name]
		void AddModel(const std::string &name,
		              std::unique_ptr<ModelBase> model,
		              Transform::Transform *parent = nullptr)
		{
			if (Models.count(name) > 0)
			{
				std::cerr << "AddModel 중복 키 무시됨 : " << name << std::endl;
				exit(1);
			}

			Transform::Transform &transform = model->GetTransform();
			transform.Name = name;
			transform.Parent = parent;
			if (parent != nullptr)
				parent->Children[name] = &transform;
			Models.insert(std::make_pair(name, std::move(model)));
		}

		ModelBase *GetModel(const std::string &name)
		{
			auto it = Models.find(name);
			return (it == Models.end()) ? nullptr : it->second.get();
		}

		bool HasModel(const std::string &name) const
		{
			return Models.count(name) > 0;
		}

		// 제거 대상 모델이 다른 모델의 Parent 라면 자식의 Parent 는 dangling 이 된다.
		// 계층이 여러 단계면 자식부터 RemoveModel 하거나 cascade 로 확장할 것.
		void RemoveModel(const std::string &name)
		{
			auto it = Models.find(name);
			if (it == Models.end())
				return;

			Transform::Transform &transform = it->second->GetTransform();
			if (transform.Parent != nullptr)
				transform.Parent->Children.erase(name);

			Models.erase(it);
		}

		virtual void Render(double currentTime,
		                    const vmath::mat4 &view,
		                    const vmath::mat4 &proj,
		                    const vmath::vec3 &viewPos) = 0;
	};

	class DefaultShaderProgram : public ShaderProgram
	{
	  public:
		using ShaderProgram::ShaderProgram;

		void Render(double currentTime,
		            const vmath::mat4 &view,
		            const vmath::mat4 &proj,
		            const vmath::vec3 &viewPos) override
		{
			if (Models.empty())
				return;

			glUseProgram(ProgAddr);
			glUniformMatrix4fv(glGetUniformLocation(ProgAddr, UNIFORM_VIEW_MAT),
			                   1, false, view);
			glUniformMatrix4fv(glGetUniformLocation(ProgAddr, UNIFORM_PROJ_MAT),
			                   1, false, proj);

			for (auto &entry : Models)
				entry.second->Draw(ProgAddr);
		}
	};

	class TextureShaderProgram : public ShaderProgram
	{
	  public:
		using ShaderProgram::ShaderProgram;

		void Render(double currentTime,
		            const vmath::mat4 &view,
		            const vmath::mat4 &proj,
		            const vmath::vec3 &viewPos) override
		{
			if (Models.empty())
			{
				std::cout << "텍스쳐 프로그램의 모델이 텅 비어있음" << std::endl;
				return;
			}

			glUseProgram(ProgAddr);
			glUniformMatrix4fv(glGetUniformLocation(ProgAddr, UNIFORM_VIEW_MAT),
			                   1, false, view);
			glUniformMatrix4fv(glGetUniformLocation(ProgAddr, UNIFORM_PROJ_MAT),
			                   1, false, proj);

			// 라이팅 uniform 푸시 — Light 가 attach 되어 있으면 활성, 아니면 OFF.
			if (AttachedLight != nullptr)
				AttachedLight->Apply(ProgAddr, viewPos);
			else
				Lighting::Light::ApplyDisabled(ProgAddr);

			// 샘플러 바인딩과 슬롯별 UV/플래그는 각 모델의 Draw() 내부에서 수행.
			for (auto &entry : Models)
				entry.second->Draw(ProgAddr);
		}
	};
} // namespace Engine::Program

namespace Engine::Application
{
	class ApplicationBase : public sb7::application
	{
	  protected:
		std::unordered_map<std::string, std::unique_ptr<Transform::Transform>> hierarchies;
		Camera::Camera main_camera;

		// 디버그용
		std::unique_ptr<Program::ShaderProgram> mDummyProgram;
		GLuint mDummyVAO = 0;

		void ClearBuffer()
		{
			glClearBufferfv(GL_COLOR, 0, Model::BG_COLOR);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);
			// glEnable(GL_CULL_FACE);

			// !!
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			// !!
		}

		// 루트 Transform 생성 + hierarchies 에 등록.
		// 모델 그룹핑 시 parent 로 사용할 포인터를 반환.
		Transform::Transform *CreateRootTransform(const char *root_name)
		{
			auto root = std::make_unique<Transform::Transform>();
			root->Name = root_name;
			hierarchies.insert(std::make_pair(root_name, std::move(root)));
			return hierarchies[root_name].get();
		}

		virtual void UpdateMembers(double currentTime) = 0;

	  public:
		virtual void init() override
		{
			sb7::application::init();
		}

		virtual void startup() override
		{
			stbi_set_flip_vertically_on_load(true);
			mDummyProgram = make_unique<Program::DefaultShaderProgram>(
			    "./shaders/dummy_vs.glsl",
			    "./shaders/dummy_fs.glsl");
			glGenVertexArrays(1, &mDummyVAO);
			glBindVertexArray(mDummyVAO);
		}

		virtual void render(double currentTime) override
		{
			ClearBuffer();
			main_camera.Aspect = ((float)info.windowWidth) / info.windowHeight;
		}

		virtual void shutdown() override
		{
			if (mDummyVAO != 0)
				glDeleteVertexArrays(1, &mDummyVAO);
		}
	};
} // namespace Engine::Application


namespace chapter7
{
	using namespace Engine;
	using namespace Engine::Model;
	using namespace Engine::Material;

	static const char* wallTexture = "./textures/wall.jpg";

	class MyApplication : public Engine::Application::ApplicationBase
	{
	  protected:
		unique_ptr<Program::TextureShaderProgram> program;
		const char *name_of_WorldRoot_transform = "WorldRoot";
		
		virtual void UpdateMembers(double currentTime) override
		{

			// 	bmate->BaseColor = vec4(
			// 		bmate->BaseColor[0],
			// 		bmate->BaseColor[1],
			// 		bmate->BaseColor[2],
			// 		cos(currentTime) * 0.5 + 0.5
			// 	);
			// }
		}

	  public:
		virtual void startup() override
		{
			ApplicationBase::startup();
		}

		virtual void render(double currentTime) override
		{
			ApplicationBase::render(currentTime);
			UpdateMembers(currentTime);

			// glUseProgram(mDummyProgram->ProgAddr);
			// glBindVertexArray(mDummyVAO);
			// glDrawArrays(GL_TRIANGLES, 0, 12);
			program->Render(currentTime, main_camera.GetViewMatrix(), main_camera.GetProjMatrix());
		}

		virtual void shutdown() override
		{
			ApplicationBase::shutdown();
		}
	};
}; // namespace chapter7

DECLARE_MAIN(chapter7::MyApplication);
