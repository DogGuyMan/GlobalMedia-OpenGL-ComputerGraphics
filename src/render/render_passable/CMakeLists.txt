# render_passable 서브폴더 -- IPassable 추상 + 그 구현체 전부.
#   - render_passable.{h,cpp}       : IPassable 최상위 순수 추상 + vtable 홈 TU (파일 그대로 이동).
#   - render_passable.impls.{h,cpp} : SceneRenderer + ScreenQuadStage + CameraStage 구현 통합.
#
# *별도 라이브러리가 아니라* 부모 sjhopengl_render 타겟에 소스를 기여한다 (target_sources):
#   render_passable.impls 는 render-core(MeshPassProcessor/DeviceContext/LightUboUploader/
#   PassComponent/MeshRenderer) 에 의존한다. 분리 STATIC 라이브러리로 쪼개면 render-core 와의
#   엣지 방향을 일일이 관리해야 하고 SceneRenderer<->render-core 상호 의존 위험이 있어,
#   단일 라이브러리 유지(소스만 하위 폴더로 조직화)로 사이클을 원천 회피한다.
#
# 부모 CMakeLists.txt 가 add_library(sjhopengl_render) 정의 후 add_subdirectory(render_passable) 를
# 호출하므로 이 시점에 타겟이 이미 존재한다. include 경로/링크/컴파일 옵션은 부모 설정을 상속.
target_sources(sjhopengl_render
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/render_passable.cpp        # IPassable vtable 홈 TU (최상위 추상, 파일 그대로 이동)
        ${CMAKE_CURRENT_SOURCE_DIR}/render_passable.impls.cpp  # SceneRenderer + ScreenQuadStage + CameraStage 구현
)
