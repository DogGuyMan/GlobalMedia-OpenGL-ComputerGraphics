/**
 * @file constants.h
 * @brief 프로젝트 전역 리터럴 문자열 상수 - 리소스 ID / 파일 경로 / uniform 이름 / ImGui 라벨 / 기하 데이터.
 *
 * @details
 *  ### 책임
 *  - 매직 스트링을 한 곳에 모아 오타로 인한 silent 실패를 방지한다.
 *    특히 uniform 이름(@c UNI_*)은 오타 시 위치 -1 로 조용히 무시되므로 중앙화가 중요하다.
 *  - @c SJH::Const::GEOMETRY - 정점 레이아웃 크기 + 원형 메시 위치/UV/인덱스 테이블
 *    (삼각형/사각형/정사면체/콘/큐브). @c SJH::Geometry 팩토리들이 참조한다.
 *  - 셰이더 파일 경로 (@c PATH_*), 텍스처 파일 경로 (@c PATH_TEX_*),
 *    리소스 ID 키 (@c STR_*), uniform 이름 (@c UNI_* / @c SHADER_PROPERTIE_*),
 *    ImGui 위젯 라벨 (@c LBL_*), depth test enum 쌍 (@c DEPTH_FUNC_LABELS / @c DEPTH_FUNC).
 *  - 배열 uniform 조합 컨벤션: @c UNI_POINT_LIGHTS_PREFIX + 인덱스 + @c STR_INDEX_CLOSE.
 *    예: @c "pointLights[" + @c "0" + @c "]".
 *  - 최대 광원 개수: @c MAX_POINT_LIGHTS = @c MAX_SPOT_LIGHTS = 16.
 *
 *  ### 비-책임
 *  - [X] 로그 포맷 문자열 / 진단 컨텍스트 문자열 - 사용처 지역(local)이라 상수화 이득이 없다.
 *  - [X] 런타임 상태 보관 - 모든 값은 @c inline constexpr (컴파일 타임 상수).
 *
 * @note @c GEOMETRY 내 @c const (비-constexpr) 배열은 TU 당 내부 링크(@c const 는 암묵 @c static).
 *       다중 TU 가 include 해도 ODR 위반 없음.
 */
#ifndef __SJH_CONSTANTS_H__
#define __SJH_CONSTANTS_H__

#include <vmath.h>
#include <vector>
#include "GL/gl3w.h"
namespace SJH::Const
{
	/**
	 * @brief 정점 레이아웃 크기 상수 및 원형 메시 데이터 테이블.
	 * @details 인터리브 VBO 레이아웃: pos(@c vec4) + color(@c vec4) + normal(@c vec3) + uv(@c vec2)
	 *          = 13 float / vertex. @c SJH::Geometry 팩토리들이 이 테이블로 메시를 구성한다.
	 */
	namespace GEOMETRY
	{
		/// @brief 위치 어트리뷰트 크기 (float 수). @c vec4 = 4.
		inline constexpr int VERTEX_POSITION_SIZE = 4;
		/// @brief 색상 어트리뷰트 크기 (float 수). @c vec4 = 4.
		inline constexpr int VERTEX_COLOR_SIZE = 4;
		/// @brief 법선 어트리뷰트 크기 (float 수). @c vec3 = 3.
		inline constexpr int VERTEX_NORMAL_SIZE = 3;
		/// @brief UV 어트리뷰트 크기 (float 수). @c vec2 = 2.
		inline constexpr int VERTEX_UV_SIZE = 2;
		/// @brief 정점 1개 전체 float 수 = 4 + 4 + 3 + 2 = 13.
		inline constexpr int VERTEX_LEN =
		    VERTEX_POSITION_SIZE + VERTEX_COLOR_SIZE + VERTEX_NORMAL_SIZE + VERTEX_UV_SIZE;

		/// @brief 배경 클리어 색 (불투명 검정).
		const vmath::vec4 COLOR_BG = vmath::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		/// @brief 정점 4개짜리 메시의 흰색 색상 벡터.
		const std::vector<vmath::vec4> COLOR_ALL_WHITE_4(4, vmath::vec4(1.0f));
		/// @brief 정점 6개짜리 메시의 흰색 색상 벡터.
		const std::vector<vmath::vec4> COLOR_ALL_WHITE_6(6, vmath::vec4(1.0f));

		/// @brief 삼각형 원형 UV - 아래-왼쪽, 아래-오른쪽, 위-중앙.
		const std::vector<vmath::vec2> TRIANGLE_BASE_MESH_UVS{
		    {0.0, 0.0}, {1.0, 0.0}, {0.5, 1.0}};

		/// @brief 삼각형 원형 UV (Y 반전) - 위-왼쪽, 위-오른쪽, 아래-중앙.
		const std::vector<vmath::vec2> TRIANGLE_BASE_INV_MESH_UVS{
		    {0.0, 1.0}, {1.0, 1.0}, {0.5, 0.0}};

		/// @brief 사각형 원형 UV - CCW 순서(좌하->우하->우상->좌상).
		const std::vector<vmath::vec2> QUAD_BASE_MESH_UVS{
		    {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};

		/// @brief 삼각형 정면 인덱스 (CCW).
		const std::vector<GLuint> TRIANGLE_FACE_INDICES = {0, 1, 2};
		/// @brief 삼각형 후면 인덱스 (CW = 후면 컬링 회피용).
		const std::vector<GLuint> TRIANGLE_FACE_INDICES_BACK = {0, 2, 1};
		/// @brief 사각형(2-삼각형 분할) 정면 인덱스 - 6개.
		const std::vector<GLuint> QUAD_FACE_INDICES = {0, 1, 2, 3, 4, 5};
		/// @brief 사각형(2-삼각형 분할) 후면 인덱스 - 6개.
		const std::vector<GLuint> QUAD_FACE_INDICES_BACK = {0, 2, 1, 3, 5, 4};
		/// @brief 사각형 팬 방식 인덱스 (v0-v1-v2, v0-v2-v3).
		const std::vector<GLuint> QUAD_MESH_UVS_FAN = {0, 1, 2, 0, 2, 3};

		/// @brief 정삼각형 원형 위치 (XY 평면, 단위 크기).
		const std::vector<vmath::vec4> TRIANGLE_BASE_POSITION = {
		    {0.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 0.0, 1.0},
		    {0.5, 0.866f, 0.0, 1.0},
		};

		/// @brief 정사면체 원형 위치 4개 (v[3] = apex).
		const std::vector<vmath::vec4> TETRA_BASE_POSITION = {
		    {0.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 0.0, 1.0},
		    {0.5, 0.0, 0.866f, 1.0},
		    {0.5, 0.816f, 0.2886f, 1.0},
		};

		/// @brief 정사면체 면 인덱스 4개.
		/// @details 면 0~2: 옆면(apex v[3] 포함), cross 가 apex 반대 방향=외향=CCW.
		///          면 3: 바닥(y=0), CCW 외향=-Y 방향.
		const std::vector<std::vector<GLuint>> TETRA_FACE_INDICES = {
		    {1, 0, 3}, {2, 1, 3}, {0, 2, 3}, {0, 1, 2}};

		/// @brief 콘 옆면 원형 위치 5개 (v[4] = apex).
		const std::vector<vmath::vec4> CONE_SIDE_BASE_POSITION = {
		    {0.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 1.0, 1.0},
		    {0.0, 0.0, 1.0, 1.0},
		    {0.5, 1.0, 0.5, 1.0}};

		/// @brief 콘 바닥면 원형 위치 4개 (@c CONE_SIDE_BASE_POSITION[0..3]).
		const std::vector<vmath::vec4> CONE_BOTTOM_BASE_POSITION = {
		    CONE_SIDE_BASE_POSITION[0],
		    CONE_SIDE_BASE_POSITION[1],
		    CONE_SIDE_BASE_POSITION[2],
		    CONE_SIDE_BASE_POSITION[3]};

		/// @brief 콘 옆면 인덱스 4면 (각 삼각형이 바닥 모서리 2개 + apex).
		const std::vector<std::vector<GLuint>> CONE_SIDE_FACE_INDICES = {
		    {1, 0, 4}, {2, 1, 4}, {3, 2, 4}, {0, 3, 4}};

		/// @brief 단위 사각형 원형 위치 4개 (XY 평면, CCW).
		const std::vector<vmath::vec4> QUAD_BASE_POSITION = {
		    {0.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 0.0, 1.0},
		    {1.0, 1.0, 0.0, 1.0},
		    {0.0, 1.0, 0.0, 1.0},
		};

		/// @brief 사각형을 삼각형 2개로 분할한 6정점 배열 (@c GL_TRIANGLES 직접 사용용).
		const std::vector<vmath::vec4> QUAD_BASE_FACED_POSITION = {
		    QUAD_BASE_POSITION[0],
		    QUAD_BASE_POSITION[1],
		    QUAD_BASE_POSITION[2],
		    QUAD_BASE_POSITION[0],
		    QUAD_BASE_POSITION[2],
		    QUAD_BASE_POSITION[3]};

		/// @brief 단위 큐브 고유 정점 8개.
		const std::vector<vmath::vec4> CUBE_BASE_POSITIONS = {
		    {0.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 0.0, 1.0},
		    {1.0, 0.0, 1.0, 1.0},
		    {0.0, 0.0, 1.0, 1.0},
		    {0.0, 1.0, 0.0, 1.0},
		    {1.0, 1.0, 0.0, 1.0},
		    {1.0, 1.0, 1.0, 1.0},
		    {0.0, 1.0, 1.0, 1.0}};

		/// @brief 큐브 6면 분할 정점 배열 (@c std::vector<vmath::vec4>[6] C-array).
		/// @details chapter7 원본 형태 그대로 유지. 각 면은 삼각형 2개(6정점).
		const std::vector<vmath::vec4> CUBE_QUAD_BASE_FACED_POSITION[6] = {
		    {CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[4], CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[4], CUBE_BASE_POSITIONS[5]},
		    {CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[6]},
		    {CUBE_BASE_POSITIONS[3], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[6], CUBE_BASE_POSITIONS[3], CUBE_BASE_POSITIONS[6], CUBE_BASE_POSITIONS[7]},
		    {CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[3], CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[4]},
		    {CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[1], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[0], CUBE_BASE_POSITIONS[2], CUBE_BASE_POSITIONS[3]},
		    {CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[6], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[7], CUBE_BASE_POSITIONS[5], CUBE_BASE_POSITIONS[4]}};

		/// @brief 큐브 면 인덱스 6면 (각 면 = 삼각형 2개, 6 인덱스). 면 순서: -Z / +X / +Z / -X / -Y / +Y.
		const std::vector<std::vector<GLuint>> CUBE_FACE_INDICES = {
		    {1, 0, 4, 1, 4, 5}, // -Z
		    {2, 1, 5, 2, 5, 6}, // +X
		    {3, 2, 6, 3, 6, 7}, // +Z
		    {0, 3, 7, 0, 7, 4}, // -X
		    {0, 1, 2, 0, 2, 3}, // -Y
		    {7, 6, 5, 7, 5, 4}  // +Y
		};
	} // namespace GEOMETRY

	// --- 리소스 ID - ResourceRegistry 의 Image/Texture/Material 조회 키 ---
	/// @brief @c ResourceRegistry::CreateTexture / @c FindTexture 의 @c key 인자값.
	/// @{
	inline constexpr auto STR_IMAGE_DARK_GRAY = "image_dark_gray";
	inline constexpr auto STR_IMAGE_GRAY = "image_gray";
	inline constexpr auto STR_IMAGE_MARBLE = "image_marble";
	inline constexpr auto STR_IMAGE_BOX1_DIFFUSE = "image_box1_diffuse";
	inline constexpr auto STR_IMAGE_BOX2_DIFFUSE = "image_box2_diffuse";
	inline constexpr auto STR_IMAGE_BOX2_SPECULAR = "image_box2_specular";
	inline constexpr auto STR_IMAGE_PLANE = "image_plane";

	inline constexpr auto STR_TEXTURE_DARK_GRAY = "texture_dark_gray";
	inline constexpr auto STR_TEXTURE_GRAY = "texture_gray";
	inline constexpr auto STR_TEXTURE_MARBLE = "texture_marble";
	inline constexpr auto STR_TEXTURE_BOX1_DIFFUSE = "texture_box1_diffuse";
	inline constexpr auto STR_TEXTURE_BOX2_DIFFUSE = "texture_box2_diffuse";
	inline constexpr auto STR_TEXTURE_BOX2_SPECULAR = "texture_box2_specular";
	inline constexpr auto STR_TEXTURE_WINDOW = "texture_window";

	inline constexpr auto STR_MATERIAL_PLANE = "material_plane";
	inline constexpr auto STR_MATERIAL_BOX1 = "material_box1";
	inline constexpr auto STR_MATERIAL_BOX2 = "material_box2";
	/// @}

	// --- 셰이더 GLSL 파일 경로 (Program::CreateWithVSFS 인자) ---
	/// @brief @c Program 생성 시 @c CreateProgram(key, vsPath, fsPath) 의 경로 인자값.
	/// @details 실행 디렉토리 기준 상대 경로 - @ref SJH::CrossPlatformDir 호출 후 유효.
	/// @{
	inline constexpr auto PATH_SHADER_LIGHTING_VS = "./resources/shader/lighting.vs";
	inline constexpr auto PATH_SHADER_LIGHTING_FS = "./resources/shader/lighting.fs";
	inline constexpr auto PATH_SHADER_SIMPLE_VS = "./resources/shader/simple.vs";
	inline constexpr auto PATH_SHADER_SIMPLE_FS = "./resources/shader/simple.fs";
	inline constexpr auto PATH_SHADER_TEXTURE_VS = "./resources/shader/texture.vs";
	inline constexpr auto PATH_SHADER_TEXTURE_FS = "./resources/shader/texture.fs";
	inline constexpr auto PATH_SHADER_POSTPROCESS_INVERT_FS = "./resources/shader/postprocess/invert.fs";
	inline constexpr auto PATH_SHADER_POSTPROCESS_GAMMA_FS = "./resources/shader/postprocess/gamma.fs";
	inline constexpr auto PATH_SHADER_POSTPROCESS_SHARPEN_FS = "./resources/shader/postprocess/sharpening.fs";
	inline constexpr auto PATH_SHADER_POSTPROCESS_BLUR_FS = "./resources/shader/postprocess/blurring.fs";
	inline constexpr auto PATH_SHADER_POSTPROCESS_SOBLE_FS = "./resources/shader/postprocess/sobel.fs";
	/// @}

	// --- 텍스처 이미지 파일 경로 (Image::Load 인자) ---
	/// @brief @c Image::Load(path) 의 @p path 인자값 - 텍스처 파일 상대 경로.
	/// @{
	inline constexpr auto PATH_TEX_MARBLE = "./resources/texture/marble.jpg";
	inline constexpr auto PATH_TEX_CONTAINER = "./resources/texture/container.jpg";
	inline constexpr auto PATH_TEX_CONTAINER2 = "./resources/texture/container2.png";
	inline constexpr auto PATH_TEX_CONTAINER2_SPECULAR = "./resources/texture/container2_specular.png";
	inline constexpr auto PATH_TEX_WINDOW = "./resources/texture/blending_transparent_window.png";
	/// @}

	// --- 셰이더 uniform 이름 ---
	/// @brief transform builtin uniform - @c MeshPassProcessor::Process 가 자동 송신 (Unity Camera builtin 정통).
	/// @{
	inline constexpr auto UNI_MODEL = "uModel";  ///< per-draw transient (world matrix).
	inline constexpr auto UNI_VIEW  = "uView";   ///< per-camera-pass transient (view matrix).
	inline constexpr auto UNI_PROJ  = "uProj";   ///< per-camera-pass transient (projection matrix).
	/// @}

	/// @brief 레거시 통합 transform uniform - 신규 셰이더는 @c UNI_MODEL/@c UNI_VIEW/@c UNI_PROJ 분리 권장.
	/// @{
	inline constexpr auto UNI_BASE_COLOR = "baseColor";
	inline constexpr auto UNI_TRANSFORM_MAT = "transformMat";
	inline constexpr auto UNI_MODEL_TRANSFORM_MAT = "modelTransformMat";
	/// @}

	/// @brief lighting builtin uniform - @c SceneRenderer::SendLightUniforms 가 자동 송신(씬 전역).
	/// @{
	inline constexpr auto UNI_VIEW_POS = "viewPos";
	inline constexpr auto UNI_DIR_LIGHT = "dirLight";
	inline constexpr auto UNI_DIR_LIGHT_ENABLED = "dirLightEnabled";
	/// @}

	/// @brief material / texture builtin uniform - @c PropertyBlockSetter 가 송신.
	/// @{
	inline constexpr auto UNI_TEX = "tex";
	inline constexpr auto UNI_POSTPROCESS_FRAMETEXTURE = "frameTexture";
	inline constexpr auto UNI_POSTPROCESS_GAMMA = "gamma";
	inline constexpr auto UNI_MATERIAL_DIFFUSE = "material.diffuse";
	inline constexpr auto UNI_MATERIAL_SPECULAR = "material.specular";
	inline constexpr auto UNI_MATERIAL_SHININESS = "material.shininess";
	/// @}

	/// @brief 배열 uniform 조합용 prefix/suffix.
	/// @details 조합 예: @c UNI_POINT_LIGHTS_PREFIX + @c "0" + @c STR_INDEX_CLOSE = @c "pointLights[0]".
	/// @{
	inline constexpr auto UNI_POINT_LIGHTS_PREFIX         = "pointLights[";
	inline constexpr auto UNI_POINT_LIGHTS_ENABLED_PREFIX = "pointLightsEnabled[";
	inline constexpr auto UNI_SPOT_LIGHTS_PREFIX          = "spotLights[";
	inline constexpr auto UNI_SPOT_LIGHTS_ENABLED_PREFIX  = "spotLightsEnabled[";
	inline constexpr auto STR_INDEX_CLOSE                 = "]";
	/// @}

	/// @brief 최대 동시 점 광원 수 - 셰이더 @c phong_lighting.slang 의 @c MAX_POINT_LIGHTS 와 1:1.
	/// @details 2026-05-26 변경: @c NUM_POINT_LIGHTS (= 2, 고정 개수로 오독) -> @c MAX_POINT_LIGHTS (= 16, 런타임 enabled 0~N 가변).
	inline constexpr int MAX_POINT_LIGHTS = 16;
	/// @brief 최대 동시 스포트 라이트 수 - 셰이더 @c #define 과 1:1. @c MAX_POINT_LIGHTS 와 대칭.
	inline constexpr int MAX_SPOT_LIGHTS  = 16;

	// --- uniform struct 멤버 suffix (program_uniforms 가 prefix 와 결합) ---
	/// @brief uniform struct 멤버 suffix - @c prefix(배열 요소) + @c SHADER_PROPERTIE_* 로 전체 uniform 이름 구성.
	/// @details 예: @c "pointLights[0]" + @c SHADER_PROPERTIE_POSITION = @c "pointLights[0].position".
	/// @{
	inline constexpr auto SHADER_PROPERTIE_DIRECTION = ".direction";
	inline constexpr auto SHADER_PROPERTIE_POSITION = ".position";
	inline constexpr auto SHADER_PROPERTIE_AMBIENT = ".ambient";
	inline constexpr auto SHADER_PROPERTIE_DIFFUSE = ".diffuse";
	inline constexpr auto SHADER_PROPERTIE_SPECULAR = ".specular";
	inline constexpr auto SHADER_PROPERTIE_ATTENUATION = ".attenuation";
	inline constexpr auto SHADER_PROPERTIE_CUTOFF = ".cutoff";
	inline constexpr auto SHADER_PROPERTIE_OUTER_CUTOFF = ".outerCutoff";
	/// @}

	// --- ImGui 위젯 라벨 (라벨이 위젯 ID 도 겸함 - 변경 시 상태 분리 주의) ---
	/// @brief ImGui 위젯 라벨 - ImGui 에서 라벨이 위젯 ID 도 겸하므로 변경 시 UI 상태 분리 주의.
	/// @{
	inline constexpr auto LBL_UI_WINDOW = "ui window";
	inline constexpr auto LBL_DIRLIGHT = "dirLight";
	inline constexpr auto LBL_DIR_ENABLED = "dir.enabled";
	inline constexpr auto LBL_DIR_DIRECTION = "dir.direction";
	inline constexpr auto LBL_DIR_AMBIENT = "dir.ambient";
	inline constexpr auto LBL_DIR_DIFFUSE = "dir.diffuse";
	inline constexpr auto LBL_DIR_SPECULAR = "dir.specular";
	inline constexpr auto LBL_POINTLIGHT_PREFIX = "pointLight[";
	inline constexpr auto LBL_P_ENABLED = "p.enabled";
	inline constexpr auto LBL_P_POSITION = "p.position";
	inline constexpr auto LBL_P_DISTANCE = "p.distance";
	inline constexpr auto LBL_P_AMBIENT = "p.ambient";
	inline constexpr auto LBL_P_DIFFUSE = "p.diffuse";
	inline constexpr auto LBL_P_SPECULAR = "p.specular";
	inline constexpr auto LBL_SPOTLIGHT = "spotLight";
	inline constexpr auto LBL_S_ENABLED = "s.enabled";
	inline constexpr auto LBL_S_POSITION = "s.position";
	inline constexpr auto LBL_S_DIRECTION = "s.direction";
	inline constexpr auto LBL_S_CUTOFF = "s.cutoff(deg)";
	inline constexpr auto LBL_S_OUTER_CUTOFF = "s.outerCutoff(deg)";
	inline constexpr auto LBL_S_DISTANCE = "s.distance";
	inline constexpr auto LBL_S_AMBIENT = "s.ambient";
	inline constexpr auto LBL_S_DIFFUSE = "s.diffuse";
	inline constexpr auto LBL_S_SPECULAR = "s.specular";
	inline constexpr auto LBL_FLASH_LIGHT = "flash light";
	inline constexpr auto LBL_ANIMATION = "animation";
	inline constexpr auto LBL_CLEAR_COLOR = "clear color";
	inline constexpr auto LBL_POSTPROCESS_GAMMA = "gamma";
	inline constexpr auto LBL_CAMERA_POS = "camera pos";
	inline constexpr auto LBL_CAMERA_YAW = "camera yaw";
	inline constexpr auto LBL_CAMERA_PITCH = "camera pitch";
	inline constexpr auto LBL_RESET_CAMERA = "reset camera";
	inline constexpr auto LBL_DEPTH_FUNC = "depth func";
	/// @}

	// --- depth test 비교 연산자 - ImGui Combo 라벨 <-> GL enum, 인덱스 1:1 동기 필수 ---
	/**
	 * @brief ImGui Combo 표시용 depth test 라벨 - @c DEPTH_FUNC 배열과 인덱스 1:1 동기 필수.
	 * @details @c glClearDepth(1.0f) 기준 - 가장 가까운 값=0, 가장 먼 값=1.
	 *
	 * | 인덱스 | GL enum       | 의미                            |
	 * |--------|---------------|---------------------------------|
	 * | 0      | @c GL_ALWAYS  | 항상 통과 (depth test 무력화)   |
	 * | 1      | @c GL_NEVER   | 항상 실패 (아무것도 안 그려짐)  |
	 * | 2      | @c GL_LESS    | 더 가까우면 통과 (기본값)       |
	 * | 3      | @c GL_LEQUAL  | 같거나 가까우면 통과            |
	 * | 4      | @c GL_GREATER | 더 멀면 통과                    |
	 * | 5      | @c GL_GEQUAL  | 같거나 멀면 통과                |
	 * | 6      | @c GL_EQUAL   | 깊이 같을 때만                  |
	 * | 7      | @c GL_NOTEQUAL| 깊이 다를 때만                  |
	 */
	inline constexpr const char *DEPTH_FUNC_LABELS[8] = {
	    "GL_ALWAYS", "GL_NEVER",
	    "GL_LESS", "GL_LEQUAL",
	    "GL_GREATER", "GL_GEQUAL",
	    "GL_EQUAL", "GL_NOTEQUAL"};

	/// @brief @c glDepthFunc 인자용 GL enum 배열 - @c DEPTH_FUNC_LABELS 와 인덱스 1:1 대응.
	inline constexpr GLenum DEPTH_FUNC[8] = {
	    GL_ALWAYS, GL_NEVER,
	    GL_LESS, GL_LEQUAL,
	    GL_GREATER, GL_GEQUAL,
	    GL_EQUAL, GL_NOTEQUAL};
} // namespace SJH::Const

#endif // __SJH_CONSTANTS_H__
