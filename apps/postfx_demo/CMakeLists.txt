get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_executable(${CHAPTER_NAME} main.cpp)

# Core 엔진 묶음 — SJH::engine INTERFACE 가 13 모듈 자동 link (Cocos libcocos2d 정통).
# Framebuffer / Scene / Render / ResourceRegistry 모두 자동 포함.
target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    SJH::engine
)

# 리소스 복사
add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources)
