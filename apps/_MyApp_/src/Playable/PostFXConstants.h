#ifndef _TOPDOWNSHOOTER_PLAYABLE_POSTFX_CONSTANTS__
#define _TOPDOWNSHOOTER_PLAYABLE_POSTFX_CONSTANTS__

#include "render/render_pipeline.h" // SJH::Render::PostFXStageConfig
#include <vector>
#include <vmath.h>

namespace TopdownShooter::Playable
{
	// 화면 패스스루(ScreenQuad bypass) 프로그램 — RenderPipeline 셋업에 주입.
	struct ProgramConfig
	{
		const char *Name;
		const char *VertFile;
		const char *FragFile;
	};
	const ProgramConfig PASSTHOURH_PROGRAM_CONFIG = {
	    "screen_passthrough",
	    "./resources/shaders/passthrough.vs",
	    "./resources/shaders/passthrough.fs"};

	// PostFX 체인 — 체인 인덱스 = 실행 순서. 모든 단계가 동일 postprocess.vs 공유.
	// 실제 디렉토리 = resources/shaders/postprocess/. D-6 data-driven — 초기값을 InitFloats 로 명시.
	// 실행 순서 (2026-06-01 사용자 지정): gamma->sharpening->bloom->fog->grayscale_vignetting->invert->blur->sobel.
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

	// fog/vignette 비-float 초기값 (PostFXStageConfig.InitFloats 밖 — startup 에서 Material 에 직접 set).
	const     vmath::vec3 FOG_COLOR      = vmath::vec3(20.0f / 255.0f, 36.0f / 255.0f, 10.0f / 255.0f); // {20,36,10}
	constexpr int         FOG_MODE       = 2;                                                           // 0=Linear, 1=Exp, 2=Exp2
	const     vmath::vec3 VIGNETTE_COLOR = vmath::vec3(1.0f, 0.0f, 0.0f);                                // {255,0,0} 빨강 비네팅
} // namespace TopdownShooter::Playable

#endif //_TOPDOWNSHOOTER_PLAYABLE_POSTFX_CONSTANTS__
