# 배선 first-green. 루트 CMakeLists 의 ENABLE_TESTING 블록이 find_package(Catch2 3)+
# enable_testing() 후 EXISTS 가드로 이 디렉토리를 add_subdirectory 한다.
add_executable(smoke smoke.cpp)
target_link_libraries(smoke PRIVATE Catch2::Catch2WithMain SJH::common)
target_compile_features(smoke PRIVATE cxx_std_17)

include(Catch) # vcpkg Catch2Config 가 Catch.cmake 를 CMAKE_MODULE_PATH 에 등록
catch_discover_tests(smoke)
