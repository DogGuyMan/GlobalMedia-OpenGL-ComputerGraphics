#include <sb7.h>
#include <vmath.h>
#include <math.h>

class my_application : public sb7::application {
public:
  virtual void render(double currentTime) {
    GLfloat values[] = {
        0.0f, 1.0f, 1.0f, 1.0f
    };
    glClearBufferfv(GL_COLOR, 0, values);
  }
};

sb7::application* app = 0;        
GLFWwindow* window;

void test_glfw_onresize(GLFWwindow * window, int w, int h) {
    app->onResize(w, h);
}

void test_glfw_onkey(GLFWwindow* window, int key, int scancode, int action,
    int mods) {
    app->onKey(key, action);
}

void test_glfw_onmousebutton(GLFWwindow* window, int button, int action,
    int mods) {
    app->onMouseButton(button, action);
}

void test_glfw_onmousemove(GLFWwindow* window, double x, double y) {
    app->onMouseMove(static_cast<int>(x), static_cast<int>(y));
}

void test_glfw_onmousewheel(GLFWwindow* window, double xoffset,
    double yoffset) {
    app->onMouseWheel(static_cast<int>(yoffset));
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, \
    LPSTR lpCmdLine, int nCmdShow) {
    app = new my_application;
            bool running = true;
            if (!glfwInit()) {
                fprintf(stderr, "Failed to initialize GLFW\n");
                return 11;
            }

            app->init();

            /*
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, info.majorVersion);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, info.minorVersion);
            
            {
                glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
            }
            if (info.flags.robust) {
                glfwWindowHint(GLFW_CONTEXT_ROBUSTNESS, GLFW_LOSE_CONTEXT_ON_RESET);
            }
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
            glfwWindowHint(GLFW_SAMPLES, info.samples);
            glfwWindowHint(GLFW_STEREO, info.flags.stereo ? GL_TRUE : GL_FALSE);
            {
                window = glfwCreateWindow(
                    800, 600, "Hello OpenGL",NULL, NULL);
                if (!window) {
                    fprintf(stderr, "Failed to open window\n");
                    return 21;
                }
            }
            */
            window = glfwCreateWindow(800, 600, "Hello OpenGL", NULL, NULL);

            if (!window) {
                fprintf(stderr, "Failed to open window\n");
                return 21;
            }
            glfwMakeContextCurrent(window);

            glfwSetWindowSizeCallback(window, test_glfw_onresize);
            glfwSetKeyCallback(window, test_glfw_onkey);
            glfwSetMouseButtonCallback(window, test_glfw_onmousebutton);
            glfwSetCursorPosCallback(window, test_glfw_onmousemove);
            glfwSetScrollCallback(window, test_glfw_onmousewheel);
            /*
            if (!info.flags.cursor) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            }
            */

            // info.flags.stereo = (glfwGetWindowParam(GLFW_STEREO) ? 1 : 0);

            gl3wInit();

            app->startup();

            do {
                app->render(glfwGetTime());

                glfwSwapBuffers(window);
                glfwPollEvents();

                running &= (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE);
                running &= (glfwWindowShouldClose(window) != GL_TRUE);
            } while (running);

            app->shutdown();

            glfwDestroyWindow(window);
            glfwTerminate();

        delete app;
        return 0;
}