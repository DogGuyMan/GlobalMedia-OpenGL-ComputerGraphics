get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_executable(${CHAPTER_NAME} main.cpp)

target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    SJH::program
    SJH::object
    SJH::material
    SJH::render
    SJH::scene
)

# 리소스 (셰이더) 복사
add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources)
