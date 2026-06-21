add_library(myapp_input_handler STATIC
    CameraController.cpp
    PlayerController.cpp
    ActorFolower.cpp
)
add_library(MyApp::InputHandler ALIAS myapp_input_handler)

target_include_directories(myapp_input_handler
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# CameraController.h 헤더 노출 의존 (PUBLIC):
#   input/keyboard_input_handler.h, input/mouse_input_handler.h   -> SJH::input
#   scene/actor.h, scene/camera.h                 -> SJH::scene
#   <glm/glm.hpp>                                     -> project_deps (include/ 노출)
#   <spdlog/spdlog.h>                             -> game_deps (spdlog INTERFACE)
# CameraController.cpp 전용 의존 (PRIVATE):
#   object/transform.h                            -> SJH::object
#   <GLFW/glfw3.h>                                -> project_deps (이미 PUBLIC 으로 흡수)
# PlayerController.cpp 전용 의존 (PRIVATE):
#   sprite/sprite_component.h (flipX 캐시 dynamic_cast typeinfo)   -> SJH::sprite
#   Entity/Components/WeaponComponents.h (Weapon::UseWeapon 심볼)  -> MyApp::Entity
#   (의존 방향 input_handler -> entity 단방향. Entity 가 src/ 에서 먼저 add_subdirectory 됨)
target_link_libraries(myapp_input_handler
    PUBLIC  SJH::input
            SJH::scene
            project_deps
            game_deps
    PRIVATE SJH::object
            SJH::sprite
            MyApp::Entity
            MyApp::Spawns   # PlayerController.cpp: VFX::Spawn (R키 laser VFX 테스트)
)

target_compile_features(myapp_input_handler PUBLIC cxx_std_17)
