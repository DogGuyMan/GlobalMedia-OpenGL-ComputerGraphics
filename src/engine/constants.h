#ifndef __ENGINE_CONSTANTS_H__
#define __ENGINE_CONSTANTS_H__
#include "GL/gl3w.h"
#include "vmath.h"
#include <cmath>
#include <vector>

namespace Engine::Constants
{
	namespace GEOMETRY
	{
		// 정점 레이아웃: pos(vec4) + color(vec4) + normal(vec3) + uv(vec2) = 13 float / vertex
		inline constexpr int VERTEX_POSITION_SIZE = 4;
		inline constexpr int VERTEX_COLOR_SIZE = 4;
		inline constexpr int VERTEX_NORMAL_SIZE = 3;
		inline constexpr int VERTEX_UV_SIZE = 2;
		inline constexpr int VERTEX_LEN =
		    VERTEX_POSITION_SIZE + VERTEX_COLOR_SIZE + VERTEX_NORMAL_SIZE + VERTEX_UV_SIZE;

		// === Constants ===
		const vmath::vec4 COLOR_BG = vmath::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		const std::vector<vmath::vec4> COLOR_ALL_WHITE_4(4, vmath::vec4(1.0f));
		const std::vector<vmath::vec4> COLOR_ALL_WHITE_6(6, vmath::vec4(1.0f));

		const std::vector<vmath::vec2> TRIANGLE_BASE_MESH_UVS{
		    {0.0, 0.0}, {1.0, 0.0}, {0.5, 1.0}};

		const std::vector<vmath::vec2> TRIANGLE_BASE_INV_MESH_UVS{
		    {0.0, 1.0}, {1.0, 1.0}, {0.5, 0.0}};

		const std::vector<vmath::vec2> QUAD_BASE_MESH_UVS{
		    {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

		const std::vector<GLuint> TRIANGLE_FACE_INDICES = {0, 1, 2};
		const std::vector<GLuint> TRIANGLE_FACE_INDICES_BACK = {0, 2, 1};
		const std::vector<GLuint> QUAD_FACE_INDICES = {0, 1, 2, 3, 4, 5};
		const std::vector<GLuint> QUAD_FACE_INDICES_BACK = {0, 2, 1, 3, 5, 4};
		const std::vector<GLuint> QUAD_MESH_UVS_FAN = {0, 1, 2, 0, 2, 3};

		const std::vector<vmath::vec4> TRIANGLE_BASE_POSITION = {
		    {0.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 0.0, 1.0},
		    {0.5, 0.866, 0.0, 1.0},
		};

		const std::vector<vmath::vec4> TETRA_BASE_POSITION = {
		    {0.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 0.0, 1.0},
		    {0.5, 0.0, 0.866, 1.0},
		    {0.5, 0.816, 0.2886, 1.0},
		};

		const std::vector<std::vector<GLuint>> TETRA_FACE_INDICES = {
		    {1, 0, 3}, {2, 1, 3}, {0, 2, 3}, {1, 0, 2}};

		const std::vector<vmath::vec4> CONE_SIDE_BASE_POSITION = {
		    {0.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 1.0, 1.0},
		    {0.0, 0.0, 1.0, 1.0},
		    {0.5, 1.0, 0.5, 1.0}};

		const std::vector<vmath::vec4> CONE_BOTTOM_BASE_POSITION = {
		    CONE_SIDE_BASE_POSITION[0],
		    CONE_SIDE_BASE_POSITION[1],
		    CONE_SIDE_BASE_POSITION[2],
		    CONE_SIDE_BASE_POSITION[3]};

		const std::vector<std::vector<GLuint>> CONE_SIDE_FACE_INDICES = {
		    {1, 0, 4}, {2, 1, 4}, {3, 2, 4}, {0, 3, 4}};

		const std::vector<vmath::vec4> QUAD_BASE_POSITION = {
		    {0.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 0.0, 1.0},
		    {1.0, 1.0, 0.0, 1.0},
		    {0.0, 1.0, 0.0, 1.0},
		};

		const std::vector<vmath::vec4> QUAD_BASE_FACED_POSITION = {
		    QUAD_BASE_POSITION[0],
		    QUAD_BASE_POSITION[1],
		    QUAD_BASE_POSITION[2],
		    QUAD_BASE_POSITION[0],
		    QUAD_BASE_POSITION[2],
		    QUAD_BASE_POSITION[3]};

		const std::vector<vmath::vec4> CUBE_BASE_POSITIONS = {
		    {0.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 1.0, 1.0},
		    {0.0, 0.0, 1.0, 1.0},
		    {0.0, 1.0, 0.0, 1.0},
		    {1.0, 1.0, 0.0, 1.0},
		    {1.0, 1.0, 1.0, 1.0},
		    {0.0, 1.0, 1.0, 1.0}};

		// chapter7 원본 형태 그대로 — `std::vector<vmath::vec4>[6]` C-array.
		const std::vector<vmath::vec4> CUBE_QUAD_BASE_FACED_POSITION[6] = {
		    {CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[4], CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[4], CUBE_BASE_POSITIONS[5]},
		    {CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[6]},
		    {CUBE_BASE_POSITIONS[3], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[6], CUBE_BASE_POSITIONS[3], CUBE_BASE_POSITIONS[6], CUBE_BASE_POSITIONS[7]},
		    {CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[3], CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[4]},
		    {CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[3]},
		    {CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[6], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[4]}};

		const std::vector<std::vector<GLuint>> CUBE_FACE_INDICES = {
		    {1, 0, 4, 1, 4, 5}, // -Z
		    {2, 1, 5, 2, 5, 6}, // +X
		    {3, 2, 6, 3, 6, 7}, // +Z
		    {0, 3, 7, 0, 7, 4}, // -X
		    {0, 1, 2, 0, 2, 3}, // -Y
		    {7, 6, 5, 7, 5, 4}  // +Y
		};
	} // namespace GEOMETRY
	namespace UNIFORM
	{
		inline constexpr const char *UNIFORM_MODEL_MAT = "inModelMat";
		inline constexpr const char *UNIFORM_VIEW_MAT = "inViewMat";
		inline constexpr const char *UNIFORM_PROJ_MAT = "inProjMat";
		inline constexpr const char *UNIFORM_CURRENT_TIME = "inCurrentTime";
		inline constexpr const char *UNIFORM_BASE_COLOR = "inBaseColor";

		inline constexpr int TEXTURE_SLOT_COUNT = 4;

		inline constexpr const char *UNIFORM_SAMPLER_TEXS[TEXTURE_SLOT_COUNT] = {
		    "tex1", "tex2", "tex3", "tex4"};
		inline constexpr const char *UNIFORM_TEX_USED[TEXTURE_SLOT_COUNT] = {
		    "uTex1Used", "uTex2Used", "uTex3Used", "uTex4Used"};
		inline constexpr const char *UNIFORM_UV_OFFSET[TEXTURE_SLOT_COUNT] = {
		    "inUVOffset1", "inUVOffset2", "inUVOffset3", "inUVOffset4"};
		inline constexpr const char *UNIFORM_UV_RATIO[TEXTURE_SLOT_COUNT] = {
		    "inUVRatio1", "inUVRatio2", "inUVRatio3", "inUVRatio4"};

		inline constexpr const char *UNIFORM_LIGHT_POS = "inLightPos";
		inline constexpr const char *UNIFORM_LIGHT_COLOR = "inLightColor";
		inline constexpr const char *UNIFORM_VIEW_POS = "inViewPos";
		inline constexpr const char *UNIFORM_AMBIENT_STRENGTH = "inAmbientStrength";
		inline constexpr const char *UNIFORM_SPECULAR_STRENGTH = "inSpecularStrength";
		inline constexpr const char *UNIFORM_SHININESS = "inShininess";
		inline constexpr const char *UNIFORM_LIGHTING_ENABLED = "inLightingEnabled";
	} // namespace UNIFORM
} // namespace Engine::Constants

#endif //__ENGINE_UNIFORMS_H__