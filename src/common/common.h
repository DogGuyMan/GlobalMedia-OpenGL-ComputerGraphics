/**
 * @file common.h
 * @brief SJH 엔진 전체가 공유하는 최소 공통 유틸리티 - 매크로/별칭/수학/시간/CWD 보정.
 *
 * @details
 *  ### 책임
 *  - @c CLASS_PTR 매크로: 클래스 forward declaration + @c unique_ptr / @c shared_ptr /
 *    @c weak_ptr 별칭 3종을 한 번에 생성. (헤더 중복 포함 없이 포인터 타입 사용 가능)
 *  - 단위 변환 (@ref SJH::Deg2Rad, @ref SJH::Rad2Deg) - @c constexpr 컴파일 타임 평가.
 *  - 가변 델타 타임 (@ref SJH::DeltaTime) / 고정 스텝 (@ref SJH::FixedTime).
 *  - 텍스트 파일 로드 (@ref SJH::LoadTextFile) - 셰이더 소스 등.
 *  - macOS GLFW CWD 부수효과 보정 (@ref SJH::CrossPlatformDir).
 *  - @c SJH::Size 타입 별칭 (@c vmath::vec2 래핑).
 *
 *  ### 비-책임
 *  - [X] GL 로더(@c gl3w) 포함 금지 - GL 비의존 레이어.
 *  - [X] 파일시스템 전역 조작 (Cross-platform dir 보정을 제외한 임의 @c chdir).
 *  - [X] 로깅 인프라 초기화 - @c spdlog 사용은 @c LoadTextFile 내부에 한정.
 *
 * @note @c #pragma once 와 @c #ifndef 가드가 중복 존재 - 의도적 (일부 컴파일러 호환).
 *       실질 방어는 @c #ifndef 가드가 담당.
 */
#ifndef __SJH_COMMON_H__
#define __SJH_COMMON_H__

#include "vmath.h"
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <cmath>

/**
 * @def CLASS_PTR(klassName)
 * @brief 클래스 forward declaration + @c std 스마트 포인터 별칭을 일괄 생성하는 매크로.
 * @details 다음 3가지 @c using 별칭을 한 번에 정의:
 *  - @c <klassName>UPtr - @c std::unique_ptr<klassName> (단독 소유)
 *  - @c <klassName>Ptr  - @c std::shared_ptr<klassName> (공유 소유)
 *  - @c <klassName>WPtr - @c std::weak_ptr<klassName>   (약한 참조)
 *
 *  완전한 클래스 정의 없이 포인터/스마트 포인터 타입만 필요한 헤더에서 전방선언 겸 별칭 생성에 쓴다.
 *  모든 @c SJH:: 네임스페이스 클래스 헤더 상단에서 관용적으로 사용.
 * @par 예시
 * @code
 * CLASS_PTR(Shader)  // class Shader; + ShaderUPtr / ShaderPtr / ShaderWPtr 자동 생성
 * @endcode
 * @note 매크로 확장 결과가 *현재 네임스페이스*에 선언된다 - 항상 올바른 네임스페이스 스코프에서 호출.
 */
#define CLASS_PTR(klassName)                            \
	class klassName;                                    \
	using klassName##UPtr = std::unique_ptr<klassName>; \
	using klassName##Ptr = std::shared_ptr<klassName>;  \
	using klassName##WPtr = std::weak_ptr<klassName>;

namespace SJH
{
	/// @brief 2D 크기 표현을 위한 @c vmath::vec2 별칭. x=width, y=height.
	typedef vmath::vec2 Size;

	/**
	 * @brief 텍스트 파일을 한 번에 읽어 @c std::string 으로 반환.
	 * @param filename 읽을 파일의 경로 (실행 디렉토리 기준 상대 경로 또는 절대 경로).
	 * @return 성공 시 파일 내용 문자열, 실패 시 @c std::nullopt.
	 * @details 실패 사유(파일 없음/권한 등)는 @c spdlog::error 로 출력.
	 *          GLSL 셰이더 소스 등 텍스트 리소스 로딩에 사용.
	 */
	std::optional<std::string> LoadTextFile(const std::string &filename);

	/**
	 * @brief Degree -> Radian 변환. @c M_PI 기반.
	 * @details @c constexpr 이므로 컴파일 타임 평가 가능. 그래픽스 코드 일반적인 @c float 정밀도.
	 *          Effekseer 등 라디안 입력 API 호환용.
	 */
	constexpr inline float Deg2Rad(float deg)
	{
		return deg * static_cast<float>(M_PI) / 180.0f;
	}

	/**
	 * @brief Radian -> Degree 변환. @c M_PI 기반.
	 * @details @c constexpr - 컴파일 타임 평가 가능. vmath::perspective 등 degree 입력 API 호환용.
	 */
	constexpr inline float Rad2Deg(float rad)
	{
		return rad * 180.0f / static_cast<float>(M_PI);
	}

	/**
	 * @brief 직전 호출 시각과의 차이(초)를 반환.
	 * @param currentTime sb7 가 넘겨주는 절대 시각(초). @c glfwGetTime() 기반.
	 * @return 직전 호출 이후 경과한 시간(초). 첫 호출은 @c currentTime 그 자체.
	 * @details @c static 내부 상태로 직전 시각을 보관  호출 지점이 여러 곳이면 서로 간섭한다.
	 *          물리/파티클 등 결정적 스텝이 필요한 곳은 @ref FixedTime 사용.
	 */
	double inline DeltaTime(double currentTime) {
		static double lastTime = 0;
		double dt = currentTime - lastTime;
		lastTime = currentTime;
		return dt;
	}

	/**
	 * @brief 고정 (=1/60초) 반환. @c constexpr.
	 * @details Box2D @c b2World::Step / Effekseer 매니저 업데이트처럼 결정적 시뮬레이션이 필요한 곳에 사용.
	 *          가변 dt 가 필요하면 @ref DeltaTime 사용.
	 */
	constexpr inline float FixedTime() {
		return 1.0f / 60.0f;
	}

	/**
	 * @brief macOS GLFW 3.0.4 의 @c _GLFW_USE_CHDIR 부수효과 (앱 번들 Resources 경로로 CWD 변경) 를
	 *        실행 파일 디렉토리로 *되돌린다*.
	 * @details
	 *  - @c sb7::application::init() override 의 첫 줄에서 호출하는 것이 정통.
	 *  - @c resources/ 상대 경로 로딩의 *전제 조건* - chdir 안 하면 macOS 에서 GLFW 가 cwd 를
	 *    번들의 @c Contents/Resources 로 옮긴 상태라 @c resources/shaders/foo.vert 같은 경로
	 *    해석이 실패한다.
	 *  - macOS 외 (Linux/Windows) 는 *no-op* - 실행 디렉토리가 이미 cwd.
	 *  - 본 함수가 macOS 전용 헤더 (@c libgen.h / @c mach-o/dyld.h 등) 흡수 - 호출처는
	 *    @c "common/common.h" 만 include 하면 됨.
	 */
	void CrossPlatformDir();
} // namespace SJH

#endif //__SJH_COMMON_H__
