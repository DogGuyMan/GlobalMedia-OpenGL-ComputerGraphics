# MyApp::Text — WorldTextSystem (minogram BMFont 보유). GameSystems 가 소유.
add_library(myapp_worldtext STATIC
    WorldTextSystem.cpp
)
add_library(MyApp::Text ALIAS myapp_worldtext)

target_include_directories(myapp_worldtext
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(myapp_worldtext
    PUBLIC
        SJH::text              # BitmapFont (헤더 노출)
        SJH::resource_registry # ResourceRegistry (Init 인자)
)
target_compile_features(myapp_worldtext PUBLIC cxx_std_17)
