# ______ 챕터별 실행 파일 등록 ______
# 한 번에 하나만 활성화한다. 새 게임/챕터 타겟을 추가하면 이 아래에 add_subdirectory 를 적고 주석 해제.

# _deptest_ — 게임/엔진 의존성(game_deps) 링크 검증용 임시 타겟.
# 의존성 점검이 필요할 때만 주석 해제: cmake --build --preset ninja --target _deptest_
# add_subdirectory(_deptest_)
# add_subdirectory(_MyApp_)

# imguitest — ImGui v1.53 통합 smoke test (SP3)
add_subdirectory(imguitest)
