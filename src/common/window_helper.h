/**
 * @file window_helper.h
 * @brief GLFW 윈도우로부터 프레임버퍼 크기와 aspect ratio 를 한 번에 조회하는 경량 헬퍼.
 *
 * @details
 *  ### 책임
 *  - @ref SJH::FramebufferInfo - 프레임버퍼 픽셀 크기(@c Width/@c Height) + @c Aspect 를
 *    단일 구조체로 묶어 반환.
 *  - @ref SJH::GetFramebufferInfo - @c glfwGetFramebufferSize 호출 + @c Aspect 계산을 1 호출로 캡슐화.
 *    Retina HiDPI 환경에서 논리 좌표와 물리 픽셀이 다를 때 투영 행렬/뷰포트 설정에 반드시 필요.
 *
 *  ### 비-책임
 *  - [X] GL 로더(@c gl3w) 포함 금지 - @c GLFWwindow* 전방선언만으로 GLFW 전체 헤더를 끌어들이지 않는다.
 *  - [X] 윈도우 생성/소멸 - @c sb7::application 이 담당.
 *
 * @note @c GLFWwindow 는 전방선언(@c struct GLFWwindow;)으로 격리.
 *       구현(@c window_helper.cpp)에서만 @c \<GLFW/glfw3.h\> 를 include 한다.
 */
#ifndef __SJH_COMMON_WINDOW_HELPER_H__
#define __SJH_COMMON_WINDOW_HELPER_H__

struct GLFWwindow; // forward - common 본체가 glfw 전체 의존 받지 않도록 격리 (D-5)

namespace SJH
{
	/**
	 * @brief 프레임버퍼 픽셀 크기와 aspect ratio 를 묶어 반환하는 POD 구조체.
	 * @details @ref GetFramebufferInfo 의 반환 타입.
	 *          Retina HiDPI 에서는 @c Width / @c Height 가 논리 윈도우 크기와 다를 수 있다
	 *          (@c glfwGetFramebufferSize 기준 - 물리 픽셀).
	 */
	struct FramebufferInfo
	{
		/// @brief 프레임버퍼 픽셀 너비.
		int   Width  = 0;
		/// @brief 프레임버퍼 픽셀 높이.
		int   Height = 0;
		/// @brief @c Width / @c Height. @c Height == 0 이면 @c 0.0f (0-div 방지).
		float Aspect = 0.0f;
	};

	/**
	 * @brief @c glfwGetFramebufferSize 로 물리 픽셀 크기를 얻고 aspect ratio 를 계산해 반환.
	 * @details Retina HiDPI 환경에서 투영 행렬(@c vmath::perspective) 과
	 *          @c glViewport 설정 모두 물리 픽셀 기준이어야 하므로 본 함수를 통해 얻어야 한다.
	 * @param window GLFW 윈도우 핸들. @c nullptr 전달 시 기본값(@c {0,0,0.0f}) 반환.
	 * @return @ref FramebufferInfo - Width, Height, Aspect.
	 */
	FramebufferInfo GetFramebufferInfo(GLFWwindow* window);
}

#endif // __SJH_COMMON_WINDOW_HELPER_H__
