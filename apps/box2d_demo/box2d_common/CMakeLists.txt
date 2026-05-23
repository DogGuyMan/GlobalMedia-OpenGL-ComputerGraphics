add_library(box2d_common STATIC
    debug_draw.cpp
)

target_include_directories(box2d_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(box2d_common PUBLIC project_deps game_deps)
