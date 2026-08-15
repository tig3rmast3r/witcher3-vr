cmake_minimum_required(VERSION 3.24)
project(witcher3vr VERSION 0.1143.0 LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(W3VR_DEPENDENCIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external" CACHE PATH
    "Directory populated by scripts/setup-dependencies.ps1")

set(W3VR_MINHOOK_DIR "${W3VR_DEPENDENCIES_DIR}/minhook")
set(W3VR_OPENXR_DIR "${W3VR_DEPENDENCIES_DIR}/openxr-sdk")
set(W3VR_DIRECTX_HEADERS_DIR "${W3VR_DEPENDENCIES_DIR}/directx-headers")
set(W3VR_STREAMLINE_DIR "${W3VR_DEPENDENCIES_DIR}/streamline")

foreach(required_path IN ITEMS
    "${W3VR_MINHOOK_DIR}/include/MinHook.h"
    "${W3VR_OPENXR_DIR}/include/openxr/openxr.h"
    "${W3VR_DIRECTX_HEADERS_DIR}/include/directx/d3d12.h"
    "${W3VR_STREAMLINE_DIR}/external/ngx-sdk/include/nvsdk_ngx.h")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR
            "Missing dependency file: ${required_path}\n"
            "Run scripts/setup-dependencies.ps1 before configuring.")
    endif()
endforeach()

add_library(minhook STATIC
    "${W3VR_MINHOOK_DIR}/src/buffer.c"
    "${W3VR_MINHOOK_DIR}/src/hook.c"
    "${W3VR_MINHOOK_DIR}/src/trampoline.c"
    "${W3VR_MINHOOK_DIR}/src/hde/hde64.c"
)
target_include_directories(minhook PUBLIC
    "${W3VR_MINHOOK_DIR}/include"
    "${W3VR_MINHOOK_DIR}/src/hde"
)

add_library(witcher3vr_dxgi SHARED
    src/aer_scheduler.cpp
    src/dxgi_proxy.cpp
    src/dxgi_proxy.def
    src/openxr_eye_geometry.cpp
    src/puredark_afw_bridge.cpp
    src/puredark_afw_camera.cpp
)
target_compile_definitions(witcher3vr_dxgi PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_include_directories(witcher3vr_dxgi PRIVATE
    "${W3VR_OPENXR_DIR}/include"
    "${W3VR_DIRECTX_HEADERS_DIR}/include"
    "${W3VR_STREAMLINE_DIR}/external/ngx-sdk/include"
)
target_link_libraries(witcher3vr_dxgi PRIVATE
    minhook
    d3d12
    dxguid
    # WaitOnAddress/WakeByAddressAll for the engine pair producer barrier.
    Synchronization
)
set_target_properties(witcher3vr_dxgi PROPERTIES OUTPUT_NAME dxgi)
target_link_options(witcher3vr_dxgi PRIVATE /MAP)

if(BUILD_TESTING)
    add_executable(w3vr_aer_scheduler_tests
        tests/aer_scheduler_tests.cpp
        src/aer_scheduler.cpp
    )
    target_include_directories(w3vr_aer_scheduler_tests PRIVATE src)
    add_test(NAME renderer_aer_scheduler COMMAND w3vr_aer_scheduler_tests)

    add_executable(w3vr_puredark_afw_bridge_tests
        tests/puredark_afw_bridge_tests.cpp
        src/puredark_afw_bridge.cpp
    )
    target_include_directories(w3vr_puredark_afw_bridge_tests PRIVATE src)
    target_compile_definitions(w3vr_puredark_afw_bridge_tests PRIVATE
        UNICODE
        _UNICODE
    )
    target_link_libraries(w3vr_puredark_afw_bridge_tests PRIVATE
        d3d12
        dxgi
    )
    add_test(NAME renderer_puredark_afw_bridge
        COMMAND w3vr_puredark_afw_bridge_tests)

    add_executable(w3vr_puredark_afw_camera_tests
        tests/puredark_afw_camera_tests.cpp
        src/puredark_afw_camera.cpp
    )
    target_include_directories(w3vr_puredark_afw_camera_tests PRIVATE src)
    target_compile_definitions(w3vr_puredark_afw_camera_tests PRIVATE
        UNICODE
        _UNICODE
    )
    add_test(NAME renderer_puredark_afw_camera
        COMMAND w3vr_puredark_afw_camera_tests)

    add_executable(w3vr_openxr_eye_geometry_tests
        tests/openxr_eye_geometry_tests.cpp
        src/openxr_eye_geometry.cpp
    )
    target_include_directories(w3vr_openxr_eye_geometry_tests PRIVATE
        src
        "${W3VR_OPENXR_DIR}/include"
    )
    add_test(NAME renderer_eye_geometry COMMAND w3vr_openxr_eye_geometry_tests)

    add_executable(w3vr_first_person_strafe_policy_tests
        tests/first_person_strafe_policy_tests.cpp
    )
    add_test(
        NAME first_person_strafe_policy
        COMMAND w3vr_first_person_strafe_policy_tests
    )

    add_executable(w3vr_first_person_combat_lock_tests
        tests/first_person_combat_lock_tests.cpp
    )
    target_include_directories(w3vr_first_person_combat_lock_tests PRIVATE src)
    add_test(
        NAME first_person_combat_lock
        COMMAND w3vr_first_person_combat_lock_tests
    )

    add_executable(w3vr_first_person_anchor_smoothing_tests
        tests/first_person_anchor_smoothing_tests.cpp
    )
    target_include_directories(
        w3vr_first_person_anchor_smoothing_tests PRIVATE src)
    add_test(
        NAME first_person_anchor_smoothing
        COMMAND w3vr_first_person_anchor_smoothing_tests
    )
endif()

option(W3VR_BUILD_LAUNCHER "Build the Witcher 3 VR launcher" ON)
if(W3VR_BUILD_LAUNCHER)
    add_subdirectory(launcher)
endif()
