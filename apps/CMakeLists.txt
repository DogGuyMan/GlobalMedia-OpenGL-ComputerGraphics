# ______ 챕터별 실행 파일 등록 ______
# 한 번에 하나만 활성화한다. 새 게임/챕터 타겟을 추가하면 이 아래에 add_subdirectory 를 적고 주석 해제.

# _deptest_ — 게임/엔진 의존성(game_deps) 링크 검증용 임시 타겟.
# 의존성 점검이 필요할 때만 주석 해제: cmake --build --preset ninja --target _deptest_
# add_subdirectory(_MyApp_)

# migrate_demo — SP5 (Light) + SP6 (Material) + SP7 (Camera/Input) 통합 검증
#   Compound Actor / TargetLock / CameraController Builder Pattern / Phong 색상 셰이더.
add_subdirectory(migrate_demo)

# audio_demo — FMOD Studio + ImGui 파라미터 데모 (신규)
# [임시 비활성] ImGui 의존 — src/imgui/ 모듈 복원 후 재활성.
# add_subdirectory(audio_demo)
