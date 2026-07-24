cmake_minimum_required(VERSION 3.24)
project(Witcher3VRLauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(w3vr_launcher_config STATIC config.cpp config.h)
target_compile_definitions(w3vr_launcher_config PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(w3vr_launcher_config PRIVATE /W4 /permissive-)
target_link_libraries(w3vr_launcher_config PUBLIC shell32 ole32)

add_executable(Witcher3VRLauncher WIN32
    main.cpp
    launcher.rc
    resources.h
    vr_dx12user.settings
)
target_compile_definitions(Witcher3VRLauncher PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(Witcher3VRLauncher PRIVATE /W4 /permissive-)
target_link_libraries(Witcher3VRLauncher PRIVATE w3vr_launcher_config comctl32)

include(CTest)
if(BUILD_TESTING)
    add_executable(w3vr_launcher_tests tests.cpp)
    target_compile_definitions(w3vr_launcher_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_options(w3vr_launcher_tests PRIVATE /W4 /permissive-)
    target_link_libraries(w3vr_launcher_tests PRIVATE w3vr_launcher_config)
    add_test(NAME launcher_config COMMAND w3vr_launcher_tests)
endif()
