add_library(sjhopengl_input STATIC mouse_input.cpp)
add_library(SJH::input ALIAS sjhopengl_input)

target_include_directories(sjhopengl_input
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_input
    PUBLIC  project_deps   # mouse_input.h / keyboard_input.h: <GLFW/glfw3.h>, 키·버튼 상수, GLFWwindow*
)

target_compile_features(sjhopengl_input PUBLIC cxx_std_17)
