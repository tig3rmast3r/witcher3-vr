cmake_minimum_required(VERSION 3.24)
project(witcher3vr VERSION 0.1581.0 LANGUAGES C CXX)

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
set(W3VR_RENDERDOC_DIR "${W3VR_DEPENDENCIES_DIR}/renderdoc")

foreach(required_path IN ITEMS
    "${W3VR_MINHOOK_DIR}/include/MinHook.h"
    "${W3VR_OPENXR_DIR}/include/openxr/openxr.h"
    "${W3VR_DIRECTX_HEADERS_DIR}/include/directx/d3d12.h"
    "${W3VR_STREAMLINE_DIR}/external/ngx-sdk/include/nvsdk_ngx.h"
    "${W3VR_RENDERDOC_DIR}/renderdoc_app.h")
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
    src/command_list_identity.cpp
    src/dxgi_proxy.cpp
    src/dxgi_proxy.def
    src/openxr_eye_geometry.cpp
    src/pipeline_flight_recorder.cpp
    src/route_flight_recorder.cpp
    src/puredark_afw_bridge.cpp
    src/puredark_afw_camera.cpp
    src/rt_ingress_join.cpp
)
target_compile_definitions(witcher3vr_dxgi PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_include_directories(witcher3vr_dxgi PRIVATE
    "${W3VR_OPENXR_DIR}/include"
    "${W3VR_DIRECTX_HEADERS_DIR}/include"
    "${W3VR_STREAMLINE_DIR}/external/ngx-sdk/include"
    "${W3VR_RENDERDOC_DIR}"
)
target_link_libraries(witcher3vr_dxgi PRIVATE
    minhook
    d3d12
    dxguid
    user32
    gdi32
    # WaitOnAddress/WakeByAddressAll for the engine pair producer barrier.
    Synchronization
)
set_target_properties(witcher3vr_dxgi PROPERTIES OUTPUT_NAME dxgi)
target_link_options(witcher3vr_dxgi PRIVATE /MAP)

if(BUILD_TESTING)
    add_executable(w3vr_swapchain_identity_tests tests/swapchain_identity_tests.cpp)
    target_include_directories(w3vr_swapchain_identity_tests PRIVATE src)
    target_compile_definitions(w3vr_swapchain_identity_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(w3vr_swapchain_identity_tests PRIVATE d3d12 dxgi dxguid user32)
    add_test(NAME renderer_present_swapchain_identity COMMAND w3vr_swapchain_identity_tests)
    add_executable(w3vr_hud_producer_publication_tests tests/hud_producer_publication_tests.cpp)
    target_include_directories(w3vr_hud_producer_publication_tests PRIVATE src)
    add_test(NAME hud_producer_publication_order COMMAND w3vr_hud_producer_publication_tests)
    add_executable(w3vr_minimal_xr_disk_log_tests tests/minimal_xr_disk_log_tests.cpp)
    target_include_directories(w3vr_minimal_xr_disk_log_tests PRIVATE src)
    target_compile_definitions(w3vr_minimal_xr_disk_log_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME minimal_xr_log_survives_termination COMMAND w3vr_minimal_xr_disk_log_tests)
    add_executable(w3vr_command_list_identity_tests
        tests/command_list_identity_tests.cpp
        src/command_list_identity.cpp)
    target_include_directories(w3vr_command_list_identity_tests PRIVATE src)
    target_compile_definitions(w3vr_command_list_identity_tests PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME renderer_command_list_identity
        COMMAND w3vr_command_list_identity_tests)

    # Optional standalone smoke probe: argv supplies an offline ReShade DLL.
    # Deliberately not a CTest dependency for source-only/product builds.
    add_executable(w3vr_command_list_identity_gpu
        tests/command_list_identity_gpu.cpp
        src/command_list_identity.cpp)
    target_include_directories(w3vr_command_list_identity_gpu PRIVATE src)
    target_compile_definitions(w3vr_command_list_identity_gpu PRIVATE
        WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(w3vr_command_list_identity_gpu PRIVATE d3d12 dxgi dxguid)

    add_executable(w3vr_world_detail_range_policy_tests
        tests/world_detail_range_policy_tests.cpp
    )
    target_include_directories(w3vr_world_detail_range_policy_tests
        PRIVATE src)
    add_test(NAME renderer_world_detail_range_policy
        COMMAND w3vr_world_detail_range_policy_tests)

    add_executable(w3vr_witcher_sense_extent_policy_tests
        tests/witcher_sense_extent_policy_tests.cpp
    )
    target_include_directories(w3vr_witcher_sense_extent_policy_tests
        PRIVATE src)
    add_test(NAME renderer_witcher_sense_extent_policy
        COMMAND w3vr_witcher_sense_extent_policy_tests)

    add_executable(w3vr_optiscaler_jitter_policy_tests
        tests/optiscaler_jitter_policy_tests.cpp
    )
    target_include_directories(w3vr_optiscaler_jitter_policy_tests PRIVATE src)
    add_test(NAME renderer_optiscaler_jitter_policy
        COMMAND w3vr_optiscaler_jitter_policy_tests)

    add_executable(w3vr_shadow_cascade_authority_policy_tests
        tests/shadow_cascade_authority_policy_tests.cpp
    )
    target_include_directories(w3vr_shadow_cascade_authority_policy_tests
        PRIVATE src)
    add_test(NAME renderer_shadow_cascade_authority_policy
        COMMAND w3vr_shadow_cascade_authority_policy_tests)

    add_executable(w3vr_taau_submission_policy_tests
        tests/taau_submission_policy_tests.cpp
    )
    target_include_directories(w3vr_taau_submission_policy_tests PRIVATE src)
    add_test(NAME renderer_taau_submission_policy
        COMMAND w3vr_taau_submission_policy_tests)

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

    add_executable(w3vr_rt_ingress_join_tests
        tests/rt_ingress_join_tests.cpp
        src/rt_ingress_join.cpp
    )
    target_include_directories(w3vr_rt_ingress_join_tests PRIVATE src)
    add_test(NAME renderer_rt_ingress_join
        COMMAND w3vr_rt_ingress_join_tests)

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

    add_executable(w3vr_first_person_horse_gallop_offset_tests
        tests/first_person_horse_gallop_offset_tests.cpp
    )
    target_include_directories(
        w3vr_first_person_horse_gallop_offset_tests PRIVATE src)
    add_test(
        NAME first_person_horse_gallop_offset
        COMMAND w3vr_first_person_horse_gallop_offset_tests
    )

    add_executable(w3vr_hmd_camera_orientation_tests
        tests/hmd_camera_orientation_tests.cpp
        src/openxr_eye_geometry.cpp
    )
    target_include_directories(w3vr_hmd_camera_orientation_tests PRIVATE
        src
        "${W3VR_OPENXR_DIR}/include"
    )
    add_test(
        NAME hmd_camera_orientation
        COMMAND w3vr_hmd_camera_orientation_tests
    )

    add_executable(w3vr_cinema_aspect_tests
        tests/cinema_aspect_tests.cpp
    )
    target_include_directories(w3vr_cinema_aspect_tests PRIVATE src)
    add_test(
        NAME cinema_aspect
        COMMAND w3vr_cinema_aspect_tests
    )

    add_executable(w3vr_camera_follow_policy_tests
        tests/camera_follow_policy_tests.cpp
    )
    target_include_directories(w3vr_camera_follow_policy_tests PRIVATE launcher)
    add_test(
        NAME camera_follow_policy
        COMMAND w3vr_camera_follow_policy_tests
    )

    add_executable(w3vr_static_hud_policy_tests
        tests/static_hud_policy_tests.cpp
    )
    target_include_directories(w3vr_static_hud_policy_tests PRIVATE launcher)
    add_test(
        NAME static_hud_policy
        COMMAND w3vr_static_hud_policy_tests
    )

    add_executable(w3vr_mode3_transport_policy_tests
        tests/mode3_transport_policy_tests.cpp
    )
    target_include_directories(w3vr_mode3_transport_policy_tests PRIVATE src)
    add_test(
        NAME renderer_mode3_transport_policy
        COMMAND w3vr_mode3_transport_policy_tests
    )

    add_executable(w3vr_mode3_hud_ownership_tests
        tests/mode3_hud_ownership_tests.cpp)
    target_include_directories(w3vr_mode3_hud_ownership_tests PRIVATE src)
    add_test(NAME renderer_mode3_hud_ownership
        COMMAND w3vr_mode3_hud_ownership_tests)

    add_executable(w3vr_mode3_dlss_constants_policy_tests
        tests/mode3_dlss_constants_policy_tests.cpp
    )
    target_include_directories(
        w3vr_mode3_dlss_constants_policy_tests PRIVATE src)
    add_test(
        NAME renderer_mode3_dlss_constants_policy
        COMMAND w3vr_mode3_dlss_constants_policy_tests
    )

    add_executable(w3vr_native_asymmetric_transport_policy_tests
        tests/native_asymmetric_transport_policy_tests.cpp
    )
    target_include_directories(
        w3vr_native_asymmetric_transport_policy_tests PRIVATE src)
    add_test(
        NAME renderer_native_asymmetric_transport_policy
        COMMAND w3vr_native_asymmetric_transport_policy_tests
    )

    add_executable(w3vr_pipeline_flight_thread_tests
        tests/pipeline_flight_thread_tests.cpp
        src/pipeline_flight_recorder.cpp
    )
    target_include_directories(w3vr_pipeline_flight_thread_tests PRIVATE
        src
        "${W3VR_DIRECTX_HEADERS_DIR}/include"
    )
    target_compile_definitions(w3vr_pipeline_flight_thread_tests PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(w3vr_pipeline_flight_thread_tests PRIVATE
        d3d12
        dxguid
    )
    add_test(
        NAME pipeline_flight_thread_placement
        COMMAND w3vr_pipeline_flight_thread_tests
    )

    add_executable(w3vr_route_flight_recorder_tests
        tests/route_flight_recorder_tests.cpp
        src/route_flight_recorder.cpp
    )
    target_include_directories(w3vr_route_flight_recorder_tests PRIVATE src)
    target_compile_definitions(w3vr_route_flight_recorder_tests PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    add_test(
        NAME route_flight_recorder
        COMMAND w3vr_route_flight_recorder_tests
    )

    add_executable(w3vr_cbv_descriptor_cache_policy_tests
        tests/cbv_descriptor_cache_policy_tests.cpp
    )
    target_include_directories(w3vr_cbv_descriptor_cache_policy_tests PRIVATE src)
    add_test(
        NAME renderer_cbv_descriptor_cache_policy
        COMMAND w3vr_cbv_descriptor_cache_policy_tests
    )

    add_executable(w3vr_foliage_native_snapshot_policy_tests
        tests/foliage_native_snapshot_policy_tests.cpp
    )
    target_include_directories(
        w3vr_foliage_native_snapshot_policy_tests PRIVATE src)
    add_test(
        NAME renderer_foliage_native_snapshot_policy
        COMMAND w3vr_foliage_native_snapshot_policy_tests
    )

    add_executable(w3vr_smoke_eye_authority_policy_tests
        tests/smoke_eye_authority_policy_tests.cpp
    )
    target_include_directories(
        w3vr_smoke_eye_authority_policy_tests PRIVATE src)
    add_test(
        NAME renderer_smoke_eye_authority_policy
        COMMAND w3vr_smoke_eye_authority_policy_tests
    )

    add_executable(w3vr_focus_fire_projection_policy_tests
        tests/focus_fire_projection_policy_tests.cpp
    )
    target_include_directories(
        w3vr_focus_fire_projection_policy_tests PRIVATE src)
    add_test(
        NAME renderer_focus_fire_projection_policy
        COMMAND w3vr_focus_fire_projection_policy_tests
    )

    add_test(
        NAME renderer_foliage_native_snapshot_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            "-DLIFETIME_EVIDENCE=${CMAKE_CURRENT_SOURCE_DIR}/support/foliage_native_table_lifetime_contract.json"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/foliage_native_current_previous_descriptor_contract.cmake"
    )

    add_test(
        NAME hud_editor_subtitle_preview_nudge_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DHUD_EDITOR_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/support/modWitcher3VRHUDEditor/content/scripts/local/witcher3vr_hud_editor/hud_editor.ws"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/hud_editor_subtitle_preview_nudge_contract.cmake"
    )

    add_test(
        NAME hud_editor_subtitle_live_refresh_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DHUD_EDITOR_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/support/modWitcher3VRHUDEditor/content/scripts/local/witcher3vr_hud_editor/hud_editor.ws"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/hud_editor_subtitle_live_refresh_contract.cmake"
    )

    add_test(
        NAME renderer_mode3_projection_transition_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/mode3_projection_transition_contract.cmake"
    )

    add_test(
        NAME renderer_mode3_hud_projection_lifecycle_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/mode3_hud_projection_lifecycle_contract.cmake"
    )

    add_test(
        NAME renderer_cinema_hud_freshness_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cinema_hud_freshness_contract.cmake"
    )

    add_test(
        NAME renderer_mode3_afw_pixel_projection_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/mode3_afw_pixel_projection_contract.cmake"
    )

    add_test(
        NAME renderer_afw_native_motion_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/afw_native_motion_contract.cmake"
    )

    add_test(
        NAME renderer_mode3_asymmetric_effect_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/mode3_asymmetric_effect_contract.cmake"
    )

    add_test(
        NAME renderer_taau_cb10_producer_identity_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/taau_cb10_producer_identity_contract.cmake"
    )

    add_test(
        NAME renderer_dlss_per_eye_constants_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/dlss_per_eye_constants_contract.cmake"
    )

    add_test(
        NAME renderer_dlss_owner_isolation_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            "-DOPTISCALER_BRIDGE_INI=${CMAKE_CURRENT_SOURCE_DIR}/optiscaler_bridge.ini"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/optiscaler_direct_ngx_contract.cmake"
    )

    add_test(
        NAME renderer_cross_queue_retained_hud_ownership_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross_queue_retained_hud_ownership_contract.cmake"
    )

    add_test(
        NAME renderer_per_queue_private_slot_retirement_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/per_queue_private_slot_retirement_contract.cmake"
    )

    add_test(
        NAME renderer_cross_queue_scene_capture_ownership_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross_queue_scene_capture_ownership_contract.cmake"
    )

    add_test(
        NAME renderer_streamline_rgba8_copy_family_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/streamline_rgba8_copy_family_contract.cmake"
    )

    add_test(
        NAME renderer_cross_queue_rt_history_ownership_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cross_queue_rt_history_ownership_contract.cmake"
    )

    add_test(
        NAME renderer_presentation_size_producer_fov_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/presentation_size_producer_fov_contract.cmake"
    )


    add_test(
        NAME renderer_mode3_native_reused_camera_fallback_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/mode3_native_reused_camera_fallback_contract.cmake"
    )

    add_test(
        NAME renderer_smoke_selector_flight_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            "-DROUTE_FLIGHT_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/route_flight_recorder.h"
            "-DROUTE_FLIGHT_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/route_flight_recorder.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke_selector_flight_contract.cmake"
    )

    add_test(
        NAME renderer_smoke_packed_basis_world_up_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke_packed_basis_world_up_contract.cmake"
    )

    add_test(
        NAME renderer_clean_mode1_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/clean_mode1_renderer_contract.cmake"
    )

    add_test(
        NAME renderer_noaa_safe_optimization_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/noaa_safe_optimization_contract.cmake"
    )

    add_test(
        NAME renderer_noaa_aggressive_optimization_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/noaa_aggressive_optimization_contract.cmake"
    )

    add_test(
        NAME renderer_witcher_sense_bounded_history_suppression_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/witcher_sense_bounded_history_suppression_contract.cmake"
    )

    add_test(
        NAME renderer_witcher_sense_bounded_descriptor_hot_path_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/witcher_sense_bounded_descriptor_hot_path_contract.cmake"
    )

    add_test(
        NAME renderer_witcher_sense_expanded_partition_cache_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/witcher_sense_expanded_partition_cache_contract.cmake"
    )

    add_test(
        NAME renderer_witcher_sense_descriptor_extent_authority_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/witcher_sense_descriptor_extent_authority_contract.cmake"
    )

    add_test(
        NAME renderer_stereo_full_vr_hud_revert_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/stereo_full_vr_hud_revert_contract.cmake"
    )

    add_test(
        NAME renderer_aer_full_vr_retained_hud_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/aer_full_vr_retained_hud_contract.cmake"
    )

    add_test(
        NAME renderer_aer_full_vr_scene_only_admission_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/aer_full_vr_scene_only_admission_contract.cmake"
    )

    add_test(
        NAME renderer_strict_stereo_gameplay_retained_hud_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/strict_stereo_gameplay_retained_hud_contract.cmake"
    )

    add_test(
        NAME renderer_strict_stereo_gameplay_eye_authority_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/strict_stereo_gameplay_eye_authority_contract.cmake"
    )

    add_test(
        NAME renderer_taau_recent_exact_submission_authority_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/taau_recent_exact_submission_authority_contract.cmake"
    )

    add_test(
        NAME renderer_f10_key_edge_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/f10_key_edge_contract.cmake"
    )

    add_test(
        NAME renderer_f9_key_edge_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/f9_key_edge_contract.cmake"
    )

    add_test(
        NAME renderer_all_keybind_edges_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/all_keybind_edges_contract.cmake"
    )

    add_test(
        NAME renderer_strict_stereo_hud_accepted_predecessor_join_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/strict_stereo_hud_accepted_predecessor_join_contract.cmake"
    )

    add_test(
        NAME renderer_strict_stereo_hud_four_present_window_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/strict_stereo_hud_four_present_window_contract.cmake"
    )

    add_test(
        NAME renderer_full_vr_transparent_fix_admission_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/full_vr_transparent_fix_admission_contract.cmake"
    )

    add_test(
        NAME renderer_full_vr_native_source_lifetime_authority_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/full_vr_native_source_lifetime_authority_contract.cmake"
    )

    add_test(
        NAME launcher_v1414_ui_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DLAUNCHER_MAIN=${CMAKE_CURRENT_SOURCE_DIR}/launcher/main.cpp"
            "-DLAUNCHER_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/launcher/config.cpp"
            "-DLAUNCHER_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/launcher/config.h"
            "-DLAUNCHER_DEFAULTS=${CMAKE_CURRENT_SOURCE_DIR}/launcher/witcher3vr.default.ini"
            "-DSTARTUP_CHECKS=${CMAKE_CURRENT_SOURCE_DIR}/launcher/startup_checks.cpp"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/launcher_v1414_ui_contract.cmake"
    )

    add_test(
        NAME launcher_v1498_ofxr_off_old_launch_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DLAUNCHER_MAIN=${CMAKE_CURRENT_SOURCE_DIR}/launcher/main.cpp"
            "-DLAUNCHER_CONFIG_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/launcher/config.h"
            "-DOFXR_ENV_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/launcher/ofxr_launch_environment.cpp"
            "-DOFXR_ENV_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/launcher/ofxr_launch_environment.h"
            "-DLAUNCHER_CMAKE=${CMAKE_CURRENT_SOURCE_DIR}/launcher/CMakeLists.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/launcher_v1498_ofxr_off_old_launch_contract.cmake"
    )

    add_test(
        NAME launcher_v1510_ofxr_root_layout_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DLAUNCHER_CONFIG_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/launcher/config.cpp"
            "-DLAUNCHER_MAIN_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/launcher/main.cpp"
            "-DOFXR_ENV_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/launcher/ofxr_launch_environment.cpp"
            "-DOFXR_ENV_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/launcher/ofxr_launch_environment.h"
            "-DLAUNCHER_TEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/launcher/tests.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/launcher_v1510_ofxr_root_layout_contract.cmake"
    )

    add_test(
        NAME launcher_v1512_ofxr_startup_whitelist_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DSTARTUP_CHECKS_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/launcher/startup_checks.cpp"
            "-DSTARTUP_CHECKS_TEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/launcher/startup_checks_tests.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/launcher_v1512_ofxr_startup_whitelist_contract.cmake"
    )

    add_test(
        NAME renderer_v1516_world_detail_range_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DRENDERER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/world_detail_range_contract.cmake"
    )

    add_test(
        NAME launcher_v1518_world_detail_range_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DLAUNCHER_MAIN_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/launcher/main.cpp"
            "-DLAUNCHER_CONFIG_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/launcher/config.cpp"
            "-DLAUNCHER_CONFIG_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/launcher/config.h"
            "-DLAUNCHER_DEFAULT_INI=${CMAKE_CURRENT_SOURCE_DIR}/launcher/witcher3vr.default.ini"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/launcher_v1518_world_detail_range_contract.cmake"
    )

    add_test(
        NAME canonical_v1528_integration_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/canonical_v1528_integration_contract.cmake"
    )

    add_test(
        NAME renderer_v23000_reshade_secondary_bootstrap_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/reshade_secondary_bootstrap_contract.cmake"
    )

    add_test(
        NAME renderer_v23033_reshade_immediate_command_list_isolation_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/reshade_immediate_command_list_isolation_contract.cmake"
    )

    add_executable(w3vr_reshade_overlay_policy_tests
        tests/reshade_overlay_policy_tests.cpp
    )
    target_include_directories(w3vr_reshade_overlay_policy_tests PRIVATE src)
    add_test(
        NAME renderer_v23036_reshade_overlay_policy
        COMMAND w3vr_reshade_overlay_policy_tests
    )

    add_test(
        NAME renderer_v23036_reshade_overlay_menu_isolation_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/reshade_overlay_menu_isolation_contract.cmake"
    )

    add_test(
        NAME renderer_v23011_reshade_opaque_submit_cleanup_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/reshade_opaque_submit_cleanup_contract.cmake"
    )

    add_test(
        NAME renderer_command_list_identity_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/command_list_identity_contract.cmake"
    )

    add_test(
        NAME renderer_v1538_hud_consumer_retirement_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/hud_consumer_retirement_contract.cmake"
    )

    add_test(
        NAME renderer_v1543_renderdoc_resource_release_isolation_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DDXGI_PROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dxgi_proxy.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/renderdoc_resource_release_isolation_contract.cmake"
    )

    add_test(
        NAME launcher_v1566_four_integration_modes_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/launcher_v1566_four_integration_modes_contract.cmake"
    )

endif()

option(W3VR_BUILD_LAUNCHER "Build the Witcher 3 VR launcher" ON)
if(W3VR_BUILD_LAUNCHER)
    add_subdirectory(launcher)
endif()
