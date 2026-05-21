get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

add_executable(${CHAPTER_NAME} main.cpp)

# 자원은 ResourceRegistry 위탁 → SJH::resource_registry 가 program/object/material/common
# 을 PUBLIC 으로 노출하므로 명시 link 중복. 직접 사용하는 상위 모듈만 link:
#   - SJH::scene  : Director / Actor / Camera / Components
#   - SJH::render : RenderSystem / RenderContext
#   - SJH::resource_registry : Get() + Create/Register 자원 위탁
# project_deps 는 sb7 / glfw / OpenGL — chapter main.cpp 의 #include <sb7.h> 등.
target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    SJH::scene
    SJH::render
    SJH::resource_registry
)

# 리소스 (셰이더) 복사
add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources)
