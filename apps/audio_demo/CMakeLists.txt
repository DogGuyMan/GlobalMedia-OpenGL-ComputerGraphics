# audio_demo — FMOD Studio + ImGui 파라미터 데모
# demo1: SDK 샘플 Music.bank 자동 발견 파라미터 위젯
# demo2: 사용자 제작 bank — BGM/SFX 버스 + Damaged/Slash one-shot + Health/BGM_STATE 파라미터
# 정책: ImGui v1.53 은 Core Module 이 아닌 Client 코드 — 각 데모가 ImGui 소스를 인라인 컴파일.
add_subdirectory(demo1)
add_subdirectory(demo2)
