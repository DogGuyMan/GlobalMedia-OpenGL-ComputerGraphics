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
