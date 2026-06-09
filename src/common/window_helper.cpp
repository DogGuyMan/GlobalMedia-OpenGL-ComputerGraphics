/**
 * @file window_helper.cpp
 * @brief @ref SJH::GetFramebufferInfo 구현 - @c glfwGetFramebufferSize + aspect 계산.
 *
 * @details
 *  ### 책임
 *  - @c GLFW/glfw3.h 를 이 파일에서만 include - 헤더(@c window_helper.h)는 @c GLFWwindow
 *    전방선언만 노출해 소비자의 전이 의존을 차단한다.
 *  - @c nullptr 가드: @p window 가 @c nullptr 이면 기본값 @c FramebufferInfo{} 반환.
 */
#include "common/window_helper.h"

#include <GLFW/glfw3.h>

namespace SJH
{
	FramebufferInfo GetFramebufferInfo(GLFWwindow* window)
	{
		FramebufferInfo info;
		if (!window)
			return info;
		glfwGetFramebufferSize(window, &info.Width, &info.Height);
		info.Aspect = (info.Height > 0)
		    ? static_cast<float>(info.Width) / static_cast<float>(info.Height)
		    : 0.0f;
		return info;
	}
}
