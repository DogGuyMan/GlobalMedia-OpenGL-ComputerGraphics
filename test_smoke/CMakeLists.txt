# Catch2 v3 통합 검증용 최소 smoke test.
# ENABLE_TESTING=ON 일 때만 빌드 — root CMakeLists.txt 의 add_subdirectory 분기.
# 이 타겟은 src/ 모듈에 의존하지 않는다 — Catch2 자체가 동작하는지만 본다.

add_executable(test_smoke test_smoke.cpp)
target_link_libraries(test_smoke PRIVATE Catch2::Catch2WithMain)
target_compile_features(test_smoke PRIVATE cxx_std_17)

include(Catch)
catch_discover_tests(test_smoke)
