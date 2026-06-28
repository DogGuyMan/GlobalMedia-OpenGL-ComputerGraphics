# 모듈별 characterization 단위 테스트. 각 exe 는 공개 헤더(계약)에만 바인딩(contract-anchoring).
# 루트 CMakeLists 의 ENABLE_TESTING 블록이 find_package(Catch2 3)+enable_testing() 후
# EXISTS 가드로 이 디렉토리를 add_subdirectory 한다.
include(Catch) # vcpkg Catch2Config -> catch_discover_tests 제공

# --- 헬퍼: test_<name>.cpp + 모듈 link + discover 를 한 줄로 ---
function(sjh_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain ${ARGN})
    target_compile_features(${name} PRIVATE cxx_std_17)
    catch_discover_tests(${name})
endfunction()

sjh_add_test(test_timer SJH::timer)
sjh_add_test(test_transform SJH::object)
sjh_add_test(test_light SJH::object)
sjh_add_test(test_sprite_uvrect SJH::sprite)
sjh_add_test(test_fsm SJH::fsm)
sjh_add_test(test_scene SJH::scene)
sjh_add_test(test_playable SJH::playable)
sjh_add_test(test_input SJH::input)
sjh_add_test(test_multiple_timer SJH::timer)
sjh_add_test(test_material SJH::material)
sjh_add_test(test_text SJH::text)

# 우산 타겟 - `cmake --build --preset ninja --target tests` 로 전체 일괄 빌드.
add_custom_target(tests DEPENDS
    test_timer test_transform test_light test_sprite_uvrect test_fsm
    test_scene test_playable test_input test_multiple_timer
    test_material test_text)

# test_smoke 는 소스 정착 시에만 (현재 디렉토리 미존재 — 부재해도 빌드 진행)
if(EXISTS ${CMAKE_SOURCE_DIR}/smoke/CMakeLists.txt)
    add_subdirectory(smoke)
endif()