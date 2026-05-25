#  Tests
# 각 test_*.cpp 는 독립 실행 파일 (Catch2WithMain 링크).
# 자세한 설계 의도는 doc/testing-curriculum.md 참조.
#
# Catch2 는 root CMakeLists.txt 의 ENABLE_TESTING 분기에서 add_subdirectory(extern/Catch2)
# 로 노출되므로 find_package 불필요. Catch.cmake 는 CMAKE_MODULE_PATH 에 사전 등록됨.

include(CTest)
include(Catch)

#  Phase A2 — common 모듈 단위 테스트
add_executable(test_common test_common.cpp)
target_link_libraries(test_common PRIVATE
    Catch2::Catch2WithMain
    project_deps              # include/ 경로 전파 (vmath.h 가 여기 있음)
    SJH::common
)
target_compile_features(test_common PRIVATE cxx_std_17)
catch_discover_tests(test_common)

#  Phase A3 — input utils 단위 테스트 [비활성화]
# 대상 모듈 src/input/glfw_input_utils.h 가 아직 미존재 — 테스트가 가리키는
# constexpr ActionToString/ModCtrl/... 가 구현되면 다시 활성화.
# add_executable(test_glfw_utils test_glfw_utils.cpp)
# target_link_libraries(test_glfw_utils PRIVATE Catch2::Catch2WithMain project_deps)
# target_compile_features(test_glfw_utils PRIVATE cxx_std_17)
# catch_discover_tests(test_glfw_utils)

#  Phase B1 — Headless GL 컨텍스트 fixture (재사용 STATIC lib)
# 향후 GL 상태 쿼리 테스트들이 공유하는 인프라.
# PUBLIC link 로 project_deps(gl3w + glfw3) 전파 -> 소비 테스트가 #include "GL/gl3w.h" 가능.
add_library(gl_test_fixture STATIC support/gl_test_fixture.cpp)
target_link_libraries(gl_test_fixture PUBLIC
    project_deps
)
target_include_directories(gl_test_fixture PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}   # "support/gl_test_fixture.h" 접근
)
target_compile_features(gl_test_fixture PRIVATE cxx_std_17)

#  Task 3 — spdlog 출력 캡처 RAII (재사용 STATIC lib)
# GL context 불필요. spdlog 의존만.
# 사용처: test_uniform_diagnostics.cpp (Task 4), test_gl_state_log.cpp (Task 5).
# 자세한 설계는 doc/testplan/2026-05-07-gl-state-and-test-quality-design.md §2.4 참조.
add_library(spdlog_capture STATIC support/spdlog_capture.cpp)
target_link_libraries(spdlog_capture PUBLIC
    spdlog::spdlog
)
target_include_directories(spdlog_capture PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}   # "support/spdlog_capture.h" 접근
)
target_compile_features(spdlog_capture PRIVATE cxx_std_17)

#  Task 6 — GL state snapshot wrapper + Diff (재사용 STATIC lib)
# SJH::diagnostics PUBLIC link -> 소비 테스트가 GLStateFields/SymbolicName 자동 가시.
# 사용처: test_gl_state_snapshot.cpp (Task 6) + 향후 다른 테스트가 GLStateSnapshot/Diff 활용.
add_library(gl_state_snapshot STATIC support/gl_state_snapshot.cpp)
target_link_libraries(gl_state_snapshot PUBLIC
    SJH::diagnostics
    project_deps        # gl3w 헤더 + GLuint/GLenum 타입
)
# spdlog 번들 fmt 사용 — 별도 fmt 라이브러리 없음 (include/spdlog/fmt/fmt.h)
target_link_libraries(gl_state_snapshot PRIVATE
    spdlog::spdlog
)
target_include_directories(gl_state_snapshot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}   # "support/gl_state_snapshot.h" 접근
)
target_compile_features(gl_state_snapshot PRIVATE cxx_std_17)

#  Phase B1 — fixture 자가검증
add_executable(test_gl_fixture test_gl_fixture.cpp)
target_link_libraries(test_gl_fixture PRIVATE
    Catch2::Catch2WithMain
    gl_test_fixture
)
target_compile_features(test_gl_fixture PRIVATE cxx_std_17)
catch_discover_tests(test_gl_fixture)

#  diagnostics 모듈 — GLDebug::CheckGL* 7개 함수 동작 검증
add_executable(test_gl_debug test_gl_debug.cpp)
target_link_libraries(test_gl_debug PRIVATE
    Catch2::Catch2WithMain
    gl_test_fixture
    SJH::diagnostics
)
target_compile_features(test_gl_debug PRIVATE cxx_std_17)
catch_discover_tests(test_gl_debug)

#  diagnostics 모듈 — UniformDiagnostics warn-once 트래커 (Task 4: 행동 단언)
# GL context 불필요 (spdlog 만 호출). SpdlogCapture로 로그 출력 행동 단언.
add_executable(test_uniform_diagnostics test_uniform_diagnostics.cpp)
target_link_libraries(test_uniform_diagnostics PRIVATE
    Catch2::Catch2WithMain
    SJH::diagnostics
    spdlog_capture
)
target_compile_features(test_uniform_diagnostics PRIVATE cxx_std_17)
catch_discover_tests(test_uniform_diagnostics)

#  program 모듈 — Uniforms 자유 함수 family 의 location 캐시 + setter 동작
# 인라인 GLSL -> Shader::CreateFromSource -> Program::Create -> Uniforms::Set*/Get 검증.
add_executable(test_program_uniforms test_program_uniforms.cpp)
target_link_libraries(test_program_uniforms PRIVATE
    Catch2::Catch2WithMain
    gl_test_fixture
    SJH::render      # DeviceContext::UseProgram (SP2 — glUseProgram owner)
    SJH::program
    SJH::shader      # Shader::CreateFromSource — 인라인 GLSL 컴파일
)
target_compile_features(test_program_uniforms PRIVATE cxx_std_17)
catch_discover_tests(test_program_uniforms)

#  buffer 모듈 — Buffer (VBO/EBO 통합 RAII) 검증
add_executable(test_buffer test_buffer.cpp)
target_link_libraries(test_buffer PRIVATE
    Catch2::Catch2WithMain
    gl_test_fixture
    SJH::buffer
)
target_compile_features(test_buffer PRIVATE cxx_std_17)
catch_discover_tests(test_buffer)

#  layout 모듈 — VertexLayout (VAO + attribute setter) 검증
add_executable(test_vertex_layout test_vertex_layout.cpp)
target_link_libraries(test_vertex_layout PRIVATE
    Catch2::Catch2WithMain
    gl_test_fixture
    SJH::layout
    SJH::buffer    # TrySetAttrib 테스트가 VBO 도 바인딩
)
target_compile_features(test_vertex_layout PRIVATE cxx_std_17)
catch_discover_tests(test_vertex_layout)

#  resource_registry 모듈 — Texture / ResourceRegistry RAII + 캐시 + sampler 계약 회귀
#  실행 디렉토리에 ./resources/texture/container.jpg 가 있어야 케이스 RUN — POST_BUILD 로 symlink.
add_executable(test_texture test_texture.cpp)
target_link_libraries(test_texture PRIVATE
    Catch2::Catch2WithMain
    gl_test_fixture
    SJH::resource_registry
)
target_compile_features(test_texture PRIVATE cxx_std_17)
catch_discover_tests(test_texture)

#  diagnostics 모듈 — GLStateFields::SymbolicName 정책 검증
# GL context 불필요 (순수 함수).
add_executable(test_gl_state_fields test_gl_state_fields.cpp)
target_link_libraries(test_gl_state_fields PRIVATE
    Catch2::Catch2WithMain
    SJH::diagnostics
)
target_compile_features(test_gl_state_fields PRIVATE cxx_std_17)
catch_discover_tests(test_gl_state_fields)

#  diagnostics 모듈 — CaptureGLState (GL context 필요)
add_executable(test_gl_state_capture test_gl_state_capture.cpp)
target_link_libraries(test_gl_state_capture PRIVATE
    Catch2::Catch2WithMain
    gl_test_fixture
    SJH::diagnostics
)
target_compile_features(test_gl_state_capture PRIVATE cxx_std_17)
catch_discover_tests(test_gl_state_capture)

#  diagnostics 모듈 — GLStateLog::Dump + EnableAutoOnError + FieldsToString (Task 5)
# spdlog_capture로 출력 단언, GL context는 일부 케이스만 필요.
add_executable(test_gl_state_log test_gl_state_log.cpp)
target_link_libraries(test_gl_state_log PRIVATE
    Catch2::Catch2WithMain
    gl_test_fixture
    spdlog_capture
    SJH::diagnostics
)
target_compile_features(test_gl_state_log PRIVATE cxx_std_17)
catch_discover_tests(test_gl_state_log)

#  diagnostics 모듈 — GLValidate 6 카테고리 (Cat A-F) + RunFullSweep
# A는 GL ctx 불필요, B-F는 inline VS/FS + GLContextFixture 사용.
add_executable(test_gl_validate test_gl_validate.cpp)
target_link_libraries(test_gl_validate PRIVATE
    Catch2::Catch2WithMain
    gl_test_fixture
    spdlog_capture
    SJH::diagnostics
)
target_compile_features(test_gl_validate PRIVATE cxx_std_17)
catch_discover_tests(test_gl_validate)

#  material 모듈 — Material 값 클래스 회귀 (default + setter + Instance via ResourceRegistry).
# GL context 불필요 — Material 은 Texture*/Program* 를 저장/반환만 함.
# SJH::resource_registry — `Material::Clone` 이 private + friend ResourceRegistry 이므로
#                          인스턴스 테스트는 `CreateMaterialInstanceFrom` 경로로만 진행 (SSoT 강제).
add_executable(test_material test_material.cpp)
target_link_libraries(test_material PRIVATE
    Catch2::Catch2WithMain
    SJH::material           # "material/material.h" 접근
    SJH::resource_registry  # "resource_registry/resource_registry.h" — Clone 공개 진입점
)
target_compile_features(test_material PRIVATE cxx_std_17)
catch_discover_tests(test_material)

#  object 모듈 — Light POD 클래스 회귀
# GL context 불필요. SJH::object 가 vmath 헤더 노출.
add_executable(test_light test_light.cpp)
target_link_libraries(test_light PRIVATE
    Catch2::Catch2WithMain
    SJH::object     # "object/light.h" 접근 + vmath
)
target_compile_features(test_light PRIVATE cxx_std_17)
catch_discover_tests(test_light)

#  object 모듈 — Transform 값 객체 (GetLocalMatrix TRS 합성)
# GL context 불필요. SJH::object 가 vmath 헤더 노출.
add_executable(test_transform test_transform.cpp)
target_link_libraries(test_transform PRIVATE
    Catch2::Catch2WithMain
    SJH::object               # "object/transform.h" 접근 + vmath
)
target_compile_features(test_transform PRIVATE cxx_std_17)
catch_discover_tests(test_transform)

#  Phase SP3 — Actor 라이프사이클 (GL 불요)
add_executable(test_actor_lifecycle test_actor_lifecycle.cpp)
target_link_libraries(test_actor_lifecycle PRIVATE
    Catch2::Catch2WithMain
    SJH::scene                # "scene/actor.h" + Transform 전파
    SJH::object               # vmath::vec3/mat4 (Transform 헤더 의존)
)
target_compile_features(test_actor_lifecycle PRIVATE cxx_std_17)
catch_discover_tests(test_actor_lifecycle)

#  Phase M1 — UniformAtlas math (GL 불요)
add_executable(test_uniform_atlas test_uniform_atlas.cpp)
target_link_libraries(test_uniform_atlas PRIVATE
    Catch2::Catch2WithMain
    SJH::sprite               # ComputeUVRect 자유함수
)
target_compile_features(test_uniform_atlas PRIVATE cxx_std_17)
catch_discover_tests(test_uniform_atlas)

#  object 모듈 — SceneNode 노드/계층 (GL 불요) [비활성화]
# 대상 헤더 src/object/scene_node.h 가 아직 미존재 — 구현 후 활성화.
# add_executable(test_scene_node test_scene_node.cpp)
# target_link_libraries(test_scene_node PRIVATE Catch2::Catch2WithMain SJH::object)
# target_compile_features(test_scene_node PRIVATE cxx_std_17)
# catch_discover_tests(test_scene_node)

#  object 모듈 — SceneGraph<TId> enum 인덱싱 파사드 (GL 불요) [비활성화]
# 대상 헤더 src/object/scene_graph.h 가 아직 미존재 — 구현 후 활성화.
# add_executable(test_scene_graph test_scene_graph.cpp)
# target_link_libraries(test_scene_graph PRIVATE Catch2::Catch2WithMain SJH::object)
# target_compile_features(test_scene_graph PRIVATE cxx_std_17)
# catch_discover_tests(test_scene_graph)

#  input 모듈 — MouseInput 드래그 (GL 불요)
add_executable(test_mouse_input test_mouse_input.cpp)
target_link_libraries(test_mouse_input PRIVATE
    Catch2::Catch2WithMain
    SJH::input
)
target_compile_features(test_mouse_input PRIVATE cxx_std_17)
catch_discover_tests(test_mouse_input)

#  input 모듈 — KeyboardInput<TAction> 디스패치 (GL 불요)
add_executable(test_keyboard_input test_keyboard_input.cpp)
target_link_libraries(test_keyboard_input PRIVATE
    Catch2::Catch2WithMain
    SJH::input
)
target_compile_features(test_keyboard_input PRIVATE cxx_std_17)
catch_discover_tests(test_keyboard_input)

#  Shader source contract — simple.fs 와 C++ Material/Light 클래스의 약속 검증
# GL context 불필요. CMake 가 SJH_RESOURCES_DIR 절대 경로 주입 (빌드 디렉토리 위치 무관).
add_executable(test_shader_uniform_contract test_shader_uniform_contract.cpp)
target_link_libraries(test_shader_uniform_contract PRIVATE
    Catch2::Catch2WithMain
)
target_compile_definitions(test_shader_uniform_contract PRIVATE
    SJH_RESOURCES_DIR="${CMAKE_SOURCE_DIR}/resources"
)
target_compile_features(test_shader_uniform_contract PRIVATE cxx_std_17)
catch_discover_tests(test_shader_uniform_contract)

#  test/support — GLStateSnapshot wrapper + Diff (Task 6)
# 대부분 GL context 불필요 — struct 직접 구성으로 path 강제. 일부 Capture sanity는 GL ctx.
add_executable(test_gl_state_snapshot test_gl_state_snapshot.cpp)
target_link_libraries(test_gl_state_snapshot PRIVATE
    Catch2::Catch2WithMain
    gl_state_snapshot
    gl_test_fixture
)
target_compile_features(test_gl_state_snapshot PRIVATE cxx_std_17)
catch_discover_tests(test_gl_state_snapshot)

# resources symlink (app/CMakeLists.txt 의 동일 패턴) — test_texture 가 컨테이너 이미지 접근.
if(EXISTS ${CMAKE_SOURCE_DIR}/resources)
    if(WIN32)
        add_custom_command(TARGET test_texture POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                    "${CMAKE_SOURCE_DIR}/resources"
                    "$<TARGET_FILE_DIR:test_texture>/resources"
            BYPRODUCTS "$<TARGET_FILE_DIR:test_texture>/resources"
            VERBATIM)
    else()
        add_custom_command(TARGET test_texture POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f
                    "$<TARGET_FILE_DIR:test_texture>/resources"
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "${CMAKE_SOURCE_DIR}/resources"
                    "$<TARGET_FILE_DIR:test_texture>/resources"
            VERBATIM)
    endif()
endif()

#  Umbrella 타겟 — `cmake --build <dir> --target tests` 로 모든 test_* 일괄 빌드.
# 새 테스트 추가 시 DEPENDS 목록만 갱신하면 됨.
add_custom_target(tests DEPENDS
    test_common
    # test_glfw_utils       # 비활성화 — src/input/glfw_input_utils.h 미존재
    test_gl_fixture
    test_gl_debug
    test_uniform_diagnostics
    test_gl_state_fields    # Task 1
    test_gl_state_capture   # Task 2
    test_gl_state_log       # Task 5
    test_gl_state_snapshot  # Task 6
    test_material           # POD value class regression
    test_light              # POD value class regression
    test_transform          # Transform 값 객체 (GetLocalMatrix)
    # test_scene_node       # 비활성화 — src/object/scene_node.h 미존재
    # test_scene_graph      # 비활성화 — src/object/scene_graph.h 미존재
    test_mouse_input        # MouseInput 드래그
    test_keyboard_input     # KeyboardInput<TAction> 디스패치
    test_shader_uniform_contract  # lighting.fs ↔ Material/Light caster contract
    test_gl_validate        # GLValidate 6 카테고리 (Cat A-F)
    test_program_uniforms
    test_buffer
    test_vertex_layout
    test_texture
    test_uniform_atlas      # M1 — UniformAtlas math (GL 불요)
)

#  Task 7 — Test smell linter (R1-R4 정적 검사, warn-only)
# 본 ctest entry는 항상 PASS — warning은 stdout으로만, 빌드/머지 차단 X.
# 자세한 규칙은 doc/testplan/2026-05-07-gl-state-and-test-quality-design.md §6.1 참조.
# 스크립트가 존재할 때만 등록 — scripts/check_test_smells.py 미존재 시 ctest entry 생략.
if(EXISTS ${CMAKE_SOURCE_DIR}/scripts/check_test_smells.py)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME test_smells
            COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_SOURCE_DIR}/scripts/check_test_smells.py
                    ${CMAKE_CURRENT_SOURCE_DIR}
        )
        set_tests_properties(test_smells PROPERTIES LABELS "lint")
    endif()
endif()
