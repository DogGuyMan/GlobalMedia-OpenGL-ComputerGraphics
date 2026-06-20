/**
 * @file Constants.h
 * @brief 앱 루트(main) 전용 문자열 상수 - scene actor 이름 / UI 오버레이 텍스처 / 클라 머티리얼 uniform.
 *
 * @details
 *  main.cpp 가 오케스트레이션으로 쓰는(다른 모듈이 참조하지 않는) 문자열을 단일 출처로 모은다.
 *  도메인 소속 문자열(Stage/Audio/Playable actor·패스명)은 각 모듈 Constants.h 에 둔다.
 *  엔진 @c src/common/constants.h 의 멀티섹션 grab-bag 컨벤션과 동일.
 * @note 로그 포맷 문자열은 제외(사용처 local 이라 상수화 이득 없음 - 기존 컨벤션).
 */
#ifndef _TOPDOWNSHOOTER_CONSTANTS__
#define _TOPDOWNSHOOTER_CONSTANTS__

namespace TopdownShooter
{
	// -- Scene actor 이름 (main 생성 - 포인터로 전달, 문자열 조회 없음) --
	constexpr const char *ACTOR_SCREEN_CAMERA = "ScreenCamera"; ///< CreateScreenCameraActor 이름.
	constexpr const char *ACTOR_FX_ROOT       = "FxRoot";       ///< 단발 시퀀스 부모 (SetSpawnContext 포인터 전달).

	// -- UI 오버레이 텍스처 (ResourceRegistry 키 + Image::Load 경로) --
	constexpr const char *STR_UI_TITLE     = "ui_title";    ///< Title 오버레이 텍스처 키.
	constexpr const char *STR_UI_PAUSE     = "ui_pause";    ///< Pause 오버레이 텍스처 키.
	constexpr const char *STR_UI_GAMEOVER  = "ui_gameover"; ///< GameOver 오버레이 텍스처 키.
	constexpr const char *PATH_UI_TITLE    = "resources/texture/Title.png";
	constexpr const char *PATH_UI_PAUSE    = "resources/texture/Pause.png";
	constexpr const char *PATH_UI_GAMEOVER = "resources/texture/GameOver.png";

	// -- 클라 머티리얼 uniform 이름 (Material::Properties 키) --
	constexpr const char *UNI_SKYBOX_TIME  = "u_time";             ///< 스카이박스 시간 (Floats).
	constexpr const char *UNI_FOG_INV_PROJ = "uInverseProjection"; ///< fog projection 역행렬 (Mat4s).
	constexpr const char *UNI_FOG_DEPTH    = "uDepth";             ///< fog depth 텍스처 (Textures).
}

#endif // _TOPDOWNSHOOTER_CONSTANTS__
