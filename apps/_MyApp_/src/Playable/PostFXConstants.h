/**
 * @file PostFXConstants.h
 * @brief PostFX 파이프라인 구성 상수 모음 -- 패스 순서/셰이더 경로/초기 uniform 값 정의.
 *
 * @details
 *  ### 책임
 *  - @c PASSTHOURH_PROGRAM_CONFIG : 화면 패스스루(ScreenQuad bypass) 프로그램 이름+셰이더 경로 정의.
 *  - @c POSTFX_PROGRAM_CONFIGS    : PostFX 체인 전체 구성(패스명/셰이더/InitFloats) 를 data-driven 으로 기술.
 *    체인 인덱스 = 실행 순서. 순서 변경은 이 배열만 수정하면 된다 (D-6 data-driven 결정).
 *  - @c FOG_COLOR / @c VIGNETTE_COLOR / @c FOG_MODE : float 외 초기값 상수 (startup 에서 Material 에 직접 set).
 *  ### 비-책임
 *  - [X] 셰이더 컴파일/프로그램 등록 -- @c RenderPipeline / PassComponent 담당.
 *  - [X] Material uniform 실시간 갱신 -- @c HpGrayscalePostFX / @c PostFXTweenPlayable 담당.
 *  ### 실행 순서 (2026-06-01 사용자 확정)
 *  gamma -> sharpening -> bloom -> fog -> grayscale_vignetting -> invert -> blurring -> sobel
 * @note 모든 PostFX 패스는 공통 @c postprocess.vs 를 공유하고 fs 만 달라진다.
 *       @c POSTFX_PROGRAM_CONFIGS 의 InitFloats 에 없는 vec3/int 초기값은
 *       @c FOG_COLOR / @c FOG_MODE / @c VIGNETTE_COLOR 로 별도 지정 후 startup 에서 수동 set.
 */
#ifndef _TOPDOWNSHOOTER_PLAYABLE_POSTFX_CONSTANTS__
#define _TOPDOWNSHOOTER_PLAYABLE_POSTFX_CONSTANTS__

#include "render_bootstrap/render_pipeline.h" // SJH::Render::PostFXStageConfig
#include <vector>
#include <vmath.h>

namespace TopdownShooter::Playable
{
	/**
	 * @brief 화면 패스스루(ScreenQuad bypass) 프로그램 이름+셰이더 파일 경로 묶음.
	 * @details @c RenderPipeline 셋업 시 passthrough 패스를 등록할 때 주입한다.
	 *          PassComponent 가 disabled 상태일 때 InputFB -> OutputFB 를 그대로 blit 하는 용도.
	 */
	struct ProgramConfig
	{
		const char *Name;      ///< 프로그램 식별 이름 (예: "screen_passthrough").
		const char *VertFile;  ///< 버텍스 셰이더 파일 경로 (실행 파일 디렉토리 기준 상대경로).
		const char *FragFile;  ///< 프래그먼트 셰이더 파일 경로 (실행 파일 디렉토리 기준 상대경로).
	};
	/// @brief 화면 패스스루 프로그램 설정 상수. @c PassComponent bypass blit 용.
	const ProgramConfig PASSTHOURH_PROGRAM_CONFIG = {
	    "screen_passthrough",
	    "./resources/shaders/passthrough.vs",
	    "./resources/shaders/passthrough.fs"};

	/// @brief PostFX 체인 전체 구성 (패스명 / 셰이더 경로 / float 초기값) 배열.
	/// @details 배열 인덱스 = 실행 순서. 모든 패스가 동일 @c postprocess.vs 공유, fs 만 다름.
	///          셰이더 실제 디렉토리: @c resources/shaders/postprocess/.
	///          D-6 data-driven -- 순서 변경/패스 추가 시 이 배열만 수정.
	///          vec3/int 타입 초기값(@c uFogColor / @c FOG_MODE / @c uVignetteColor) 은
	///          InitFloats 에 표현 불가능해 startup 에서 Material 에 직접 set.
	const std::vector<SJH::Render::PostFXStageConfig> POSTFX_PROGRAM_CONFIGS = {
	    {"gamma", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/gamma.fs", {{"gamma", 1.0f}}},
	    {"sharpening", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/sharpening.fs", {}},
	    {"bloom", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/bloom.fs",
	     {{"uBloomThreshold", 0.769f}, {"uBloomSpread", 2.342f}, {"uBloomIntensity", 0.927f}}},
	    {"fog", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/fog.fs",
	     {{"uFogDensity", 0.042f}, {"uFogStart", 0.0f}, {"uFogEnd", 50.0f}}},
	    {"grayscale_vignetting", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/grayscale_vignetting.fs",
	     {{"uGrayscaleAmount", 1.0f}, {"uVignetteAmount", 0.0f}}}, // uVignetteColor(vec3)는 startup 에서 set(VIGNETTE_COLOR).
	    {"invert", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/invert.fs", {}},
	    {"blurring", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/blurring.fs", {}},
	    {"sobel", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/sobel.fs", {}},
	};

	/// @brief fog 셰이더 @c uFogColor vec3 초기값 -- RGB (20, 36, 10) 어두운 녹색 안개.
	/// @note @c PostFXStageConfig.InitFloats 가 float 전용이라 startup 에서 Material 에 직접 set.
	const     vmath::vec3 FOG_COLOR      = vmath::vec3(20.0f / 255.0f, 36.0f / 255.0f, 10.0f / 255.0f); // {20,36,10}
	/// @brief fog 셰이더 @c uFogMode int 초기값 -- 0=Linear, 1=Exp, 2=Exp2. 기본 Exp2.
	constexpr int         FOG_MODE       = 2;                                                           // 0=Linear, 1=Exp, 2=Exp2
	/// @brief grayscale_vignetting 셰이더 @c uVignetteColor vec3 초기값 -- 빨강 (255, 0, 0) 비네팅.
	/// @note startup 에서 Material 에 직접 set (@c PostFXStageConfig.InitFloats 밖).
	const     vmath::vec3 VIGNETTE_COLOR = vmath::vec3(1.0f, 0.0f, 0.0f);                                // {255,0,0} 빨강 비네팅
} // namespace TopdownShooter::Playable

#endif //_TOPDOWNSHOOTER_PLAYABLE_POSTFX_CONSTANTS__
