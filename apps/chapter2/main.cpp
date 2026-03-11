#include "entry.h"

sb7::application::APPINFO info;
GLFWwindow *window;

namespace SJH::Chapter2
{
	sb7::application *app;
	void inline OnResize(GLFWwindow *window, int w, int h)
	{
		app->onResize(w, h);
	}

	void inline OnKey(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		app->onKey(key, action);
	}

	void inline OnMouseButton(GLFWwindow *window, int button, int action, int mods)
	{
		app->onMouseButton(button, action);
	}

	void inline OnMouseMove(GLFWwindow *window, double x, double y)
	{
		app->onMouseMove(static_cast<int>(x), static_cast<int>(y));
	}

	void inline OnMouseWheel(GLFWwindow *window, double xoffset, double yoffset)
	{
		app->onMouseWheel(static_cast<int>(yoffset));
	}
}

// 윈도우/컨텍스트 생성 옵션 지정
void setupWindowHints()
{
	/* 버전/프로파일: */
	// 요청할 OpenGL 최소 버전
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, info.majorVersion);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, info.minorVersion);
	// GLFW_OPENGL_PROFILE	CORE_PROFILE	레거시 함수 제거된 코어 프로파일 사용
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	// GLFW_OPENGL_FORWARD_COMPAT	GL_TRUE	deprecated 기능 제거 (macOS 3.2+ 필수)
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	/* 디버깅 */
#ifndef _DEBUG
	if (info.flags.debug)
#endif /* _DEBUG */
	{
		// GL_TRUE	디버그 컨텍스트 활성화 (GL 호출 유효성 검사)
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	}

	if (info.flags.robust)
	{
		// GLFW_CONTEXT_ROBUSTNESS	LOSE_CONTEXT_ON_RESET	GPU 리셋 시 컨텍스트를 잃도록 설정 (안정성)
		glfwWindowHint(GLFW_CONTEXT_ROBUSTNESS, GLFW_LOSE_CONTEXT_ON_RESET);
	}

	/* 렌더링 품질 */
	// GLFW_SAMPLES	0	MSAA 멀티샘플링 수 (0 = 비활성)
	glfwWindowHint(GLFW_SAMPLES, info.samples);
	// GLFW_STEREO	GL_FALSE	스테레오 3D 렌더링 (비활성)
	glfwWindowHint(GLFW_STEREO, info.flags.stereo ? GL_TRUE : GL_FALSE);
}

void setupIOCallbacks()
{
	glfwSetWindowSizeCallback(window, SJH::Chapter2::OnResize);
	glfwSetKeyCallback(window, SJH::Chapter2::OnKey);
	glfwSetMouseButtonCallback(window, SJH::Chapter2::OnMouseButton);
	glfwSetCursorPosCallback(window, SJH::Chapter2::OnMouseMove);
	glfwSetScrollCallback(window, SJH::Chapter2::OnMouseWheel);

	if (!info.flags.cursor)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	}
}

void init()
{
	strcpy(info.title, "OpenGL SuperBible Example");
	info.windowWidth = 800;
	info.windowHeight = 600;
#ifdef __APPLE__
	info.majorVersion = 3;
	info.minorVersion = 2;
#else
	info.majorVersion = 4;
	info.minorVersion = 3;
#endif
	info.samples = 0;
	info.flags.all = 0;
	info.flags.cursor = 1;
#ifdef _DEBUG
	info.flags.debug = 1;
#endif
}

int entry()
{
	SJH::Chapter2::app = new SJH::Chapter2::my_application_4();
	bool running = true;

	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return 1;
	}

	init();

	setupWindowHints();

	window = glfwCreateWindow(
	    info.windowWidth, info.windowHeight, info.title,
	    info.flags.fullscreen ? glfwGetPrimaryMonitor() : NULL, NULL);
	if (!window)
	{
		fprintf(stderr, "Failed to open window\n");
		return 1;
	}

	glfwMakeContextCurrent(window);

	setupIOCallbacks();

	gl3wInit();

	SJH::Chapter2::app->startup();
	do
	{
		SJH::Chapter2::app->render(glfwGetTime());

		glfwSwapBuffers(window);
		glfwPollEvents();

		running &= (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE);
		running &= (glfwWindowShouldClose(window) != GL_TRUE);
	} while (running);
	glfwDestroyWindow(window);
	glfwTerminate();

	SJH::Chapter2::app->shutdown();

	delete SJH::Chapter2::app;
	return 0;
}

#if defined _WIN32
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		     LPSTR lpCmdLine, int nCmdShow)
{
	return entry();
}
#else
int main(int argc, const char **argv)
{
	return entry();
}
#endif