add_library(sjhopengl_common STATIC
    common.cpp
)
add_library(SJH::common ALIAS sjhopengl_common)

target_include_directories(sjhopengl_common
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_common PRIVATE spdlog)

target_compile_features(sjhopengl_common PRIVATE cxx_std_17)
