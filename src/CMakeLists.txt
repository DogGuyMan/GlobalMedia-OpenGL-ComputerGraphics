#  Source Modules
# 각 모듈은 자체 CMakeLists.txt에서 include 경로를 관리.
# target_link_libraries(SJH::<모듈>) 시 include 경로가 INTERFACE로 전파됨.
# 모듈 간 명시적 의존성: 각 모듈 CMakeLists.txt에 target_link_libraries 추가.

add_subdirectory(buffer)
add_subdirectory(object)
add_subdirectory(common)
add_subdirectory(context)
add_subdirectory(diagnostics)
add_subdirectory(layout)
add_subdirectory(program)
add_subdirectory(material)
add_subdirectory(shader)
add_subdirectory(resource_registry)
add_subdirectory(input)
