add_library(sjhopengl_common STATIC
    common.cpp
    window_helper.cpp        # GLFW 의존 격리 — GetFramebufferInfo 자유 함수
)
add_library(SJH::common ALIAS sjhopengl_common)

target_include_directories(sjhopengl_common
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_common
    PRIVATE
        spdlog
        project_deps    # window_helper.cpp 의 <GLFW/glfw3.h>
)

target_compile_features(sjhopengl_common PRIVATE cxx_std_17)
