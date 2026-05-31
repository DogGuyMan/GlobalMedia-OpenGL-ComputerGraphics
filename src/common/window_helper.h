#ifndef __SJH_COMMON_WINDOW_HELPER_H__
#define __SJH_COMMON_WINDOW_HELPER_H__

struct GLFWwindow; // forward — common 본체가 glfw 전체 의존 받지 않도록 격리 (D-5)

namespace SJH
{
	/// @brief 프레임버퍼 크기 + aspect 1 호출 packed return.
	struct FramebufferInfo
	{
		int   Width  = 0;
		int   Height = 0;
		float Aspect = 0.0f;
	};

	/// @brief glfwGetFramebufferSize + aspect 계산.
	/// @details Retina HiDPI 의 physical framebuffer 기준. Aspect = Width/Height (Height==0 시 0).
	FramebufferInfo GetFramebufferInfo(GLFWwindow* window);
}

#endif // __SJH_COMMON_WINDOW_HELPER_H__
