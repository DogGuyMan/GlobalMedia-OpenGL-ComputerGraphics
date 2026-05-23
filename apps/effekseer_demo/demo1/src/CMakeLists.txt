target_sources(${CHAPTER_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Controller.Camera.cpp
)

target_include_directories(${CHAPTER_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
