add_subdirectory(Algebraic)
add_subdirectory(Entity)
add_subdirectory(InputHandler)
add_subdirectory(Physics)

# 우산 INTERFACE — 클라이언트 스크립트 전체 묶음
add_library(myapp_client INTERFACE)
add_library(MyApp::Client ALIAS myapp_client)

target_link_libraries(myapp_client INTERFACE
    MyApp::Algebraic
    MyApp::Entity
    MyApp::InputHandler
    MyApp::Physics
)
