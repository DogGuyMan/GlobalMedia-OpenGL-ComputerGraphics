# cmake/SlangPostProcess410.cmake
# slangc 의 GLSL 출력을 macOS OpenGL 4.1 (GLSL 410) 호환으로 치환한다.
# 사용: cmake -DSLANG_IN=raw.glsl -DSLANG_OUT=final.fs -P cmake/SlangPostProcess410.cmake
#
# 치환 3종 (정본 spec 3.1):
#   1) #version 450            -> #version 410 core
#   2) layout(binding = N)     단독 라인 제거 (UBO/텍스처 explicit binding 은 GLSL 420+)
#   3) layout(row_major) buffer; 라인 제거 (SSBO 기본 레이아웃, GLSL 430+ — 410 거부)
# binding 은 셰이더가 아니라 C++ 가 glUniformBlockBinding 으로 묶는다 (Phase 2).

if(NOT DEFINED SLANG_IN OR NOT DEFINED SLANG_OUT)
    message(FATAL_ERROR "SlangPostProcess410: SLANG_IN / SLANG_OUT 인자 필요")
endif()

file(READ "${SLANG_IN}" _content)

# 1) #version 450 -> 410 core
string(REPLACE "#version 450" "#version 410 core" _content "${_content}")

# 2) layout(binding = N) 단독 라인 제거 (뒤따르는 개행까지)
string(REGEX REPLACE "layout\\(binding = [0-9]+\\)\n" "" _content "${_content}")

# 3) layout(row_major) buffer; / layout(column_major) buffer; 라인 제거
string(REGEX REPLACE "layout\\((row|column)_major\\) buffer;\n" "" _content "${_content}")

file(WRITE "${SLANG_OUT}" "${_content}")
message(STATUS "[slang:410] ${SLANG_OUT}")
