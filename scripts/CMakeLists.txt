# scripts/CMakeLists.txt
# Python 보조 스크립트 디렉토리의 *경로 등록* 책임 (책임 분리 2026-06-21).
#
# 책임:
#   - 본 디렉토리의 Python 스크립트 경로를 *변수* 로 부모 scope 에 export.
#   - CMake 모듈(cmake/*.cmake) 이 그 변수로 스크립트를 호출 (경로 직접 지정 회피).
# 비-책임:
#   - [X] 빌드 타겟 생성 - 본 디렉토리는 *런타임 보조 스크립트* 라 빌드 대상 0.
#   - [X] 스크립트 실행 - CMake 모듈 (예: cmake/Slang.cmake) 이 add_custom_command 안에서 호출.
#   - [X] 스크립트 자체 의존성 관리 - Python 표준 라이브러리만 사용 가정 (외부 패키지 0).
#
# 신규 Python 스크립트 추가 시: 본 파일에 SJH_<NAME>_SCRIPT 변수 추가 +
#                              사용 측 CMake 모듈에서 해당 변수를 *읽어* 사용.

# Slang 셰이더 컴파일 파이프라인 (cmake/Slang.cmake 가 sjh_compile_slang/sjh_reflect_slang 안에서 호출).
#   slangc 명령줄 + GLSL 410 post-process + reflection + ninja DEPFILE 생성을 한 호출로.
set(SJH_SLANG_COMPILE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/slang_compile.py" PARENT_SCOPE)
