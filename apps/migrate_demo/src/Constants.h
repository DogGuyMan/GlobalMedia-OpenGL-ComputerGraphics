#pragma once

/**
 * @file Constants.h
 * @brief migrate_demo 전역 리터럴 상수 — main.cpp 에 흩어진 문자열을 카테고리별로 추출.
 *
 * @details
 *   ### 분류
 *   - Programs    : Scene 셰이더 프로그램 이름 + .vs/.fs 경로 (ResourceRegistry::CreateProgram 키)
 *   - Meshes      : main 이 직접 FindMesh 하는 키 (Scene.Warmup.h 가 등록)
 *   - Actors      : Actor 이름 (디버그/조회용)
 *   - PostFXKey   : PostFX 5-패스가 동적으로 만드는 키 접두사 / 공통 uniform 이름
 *   - Uniforms    : 단발 셰이더 uniform 이름
 *   - LogMsg      : spdlog 로그 메시지 (fmt 포맷 문자열 포함)
 *   - UI          : ImGui 라벨 / 헤더
 *
 *   ### 컨벤션
 *   - C++17 `inline constexpr const char*` 로 ODR-safe (헤더 다중 include 가능).
 *   - Scene.Warmup.h 내부에서만 쓰이는 키 (mat_plane, tex_marble 등) 는 *그 파일 책임* — 여기 미포함.
 *   - main.cpp 의 PostFX 익명 네임스페이스 (kPostFXDefs, kPostFXVertFile, kFxLayer) 는
 *     코드와 강결합된 *데이터* 라 main.cpp 에 그대로 둔다.
 */

namespace MigrateDemo::Constants
{
	// === Scene Programs (ResourceRegistry::CreateProgram 키 + 경로) ===
	namespace Programs
	{
		inline constexpr const char *PhongName = "phong_tex";
		inline constexpr const char *PhongVS   = "resources/shaders/phong_tex.vs";
		inline constexpr const char *PhongFS   = "resources/shaders/phong_tex.fs";

		inline constexpr const char *SimpleName = "simple";
		inline constexpr const char *SimpleVS   = "resources/shaders/simple.vs";
		inline constexpr const char *SimpleFS   = "resources/shaders/simple.fs";

		inline constexpr const char *WindowName = "window";
		inline constexpr const char *WindowVS   = "resources/shaders/window.vs";
		inline constexpr const char *WindowFS   = "resources/shaders/window.fs";
	} // namespace Programs

	// === Mesh 등록 키 (main.cpp 가 직접 FindMesh 하는 것만) ===
	namespace Meshes
	{
		inline constexpr const char *ScreenQuad = "mesh_screen_quad";
	} // namespace Meshes

	// === Actor 이름 ===
	namespace Actors
	{
		inline constexpr const char *SceneCamera = "SceneCamera";
	} // namespace Actors

	// === PostFX 5-패스 동적 키 접두사 (BuildPostFXChain 에서 def.Name 과 concat) ===
	namespace PostFXKey
	{
		inline constexpr const char *ProgramPrefix  = "postfx_";        // + def.Name
		inline constexpr const char *MaterialPrefix = "mat_postfx_";    // + def.Name
		inline constexpr const char *QuadPrefix     = "PostFXQuad_";    // + def.Name
		inline constexpr const char *CameraPrefix   = "PostFXCamera_";  // + def.Name

		// PostFX 셰이더 uniform 이름 (체인 모든 패스 공통).
		inline constexpr const char *USceneSampler = "uScene"; // 입력 sampler2D
		inline constexpr const char *GammaUniform  = "gamma";  // gamma.fs 만 사용
	} // namespace PostFXKey

	// === Log 메시지 (spdlog — {} placeholder 포함 문자열은 fmt 포맷) ===
	namespace LogMsg
	{
		inline constexpr const char *SceneShaderLoadFail = "migrate_demo scene shader 로드 실패";
		inline constexpr const char *SceneFBFail         = "SceneFB 생성 실패";
		inline constexpr const char *PostFXQuadMissing   = "PostFX: mesh_screen_quad 미존재 — WarmupAssets 가 등록 필요.";
		inline constexpr const char *PostFXShaderFailFmt = "PostFX shader 로드 실패: {}";
		inline constexpr const char *PostFXFBFailFmt     = "PostFX intermediate FB 생성 실패 (pass: {})";
		inline constexpr const char *StartupDone         = "migrate_demo startup 완료. WASD/QE 이동, 우클릭 드래그 회전, F=FlashLight.";
	} // namespace LogMsg

	// === ImGui 라벨 / 헤더 ===
	namespace UI
	{
		inline constexpr const char *WindowTitle = "migrate_demo controls";

		// Lights 섹션
		inline constexpr const char *HeaderLights    = "Lights";
		inline constexpr const char *DirLight        = "DirLight";
		inline constexpr const char *PointLight0     = "PointLight 0";
		inline constexpr const char *PointLight1     = "PointLight 1";
		inline constexpr const char *SpotLight       = "SpotLight";
		inline constexpr const char *FlashLightMode  = "FlashLight mode (F)";
		inline constexpr const char *SpotCutoff      = "Spot cutoff (deg)";
		inline constexpr const char *SpotOuterCutoff = "Spot outerCutoff (deg)";
		inline constexpr const char *SpotDistance    = "Spot distance";
		inline constexpr const char *SpotDiffuse     = "Spot diffuse";

		// PostFX 섹션
		inline constexpr const char *HeaderPostFX = "PostFX (multi-pass, 알파벳 순)";
		inline constexpr const char *GammaSlider  = "gamma (gamma.fs uniform)";

		// Camera 섹션
		inline constexpr const char *HeaderCamera = "Camera";
		inline constexpr const char *Position     = "Position";
		inline constexpr const char *EulerRot     = "EulerRot";
		inline constexpr const char *ResetCamera  = "Reset camera";
		inline constexpr const char *ClearColor   = "Clear color (FB)";
	} // namespace UI
} // namespace MigrateDemo::Constants
