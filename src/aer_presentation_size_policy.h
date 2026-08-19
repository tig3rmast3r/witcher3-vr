#pragma once

namespace w3vr::aer_presentation_size_policy {

struct FixedResolutionRouteInput {
    bool mode3_aer{};
    bool native_asymmetric{};
    bool taau_backend{};
};

// AER+TAAU owns a completed, full-resolution per-eye source before final
// OpenXR presentation. Presentation Size must therefore never resize its
// swapchain; it only changes the angular FOV used for the final submission.
constexpr bool fixed_resolution_route_active(
    const FixedResolutionRouteInput& input) noexcept {
    return input.mode3_aer && input.native_asymmetric && input.taau_backend;
}

struct FinalOpenXrRemapInput {
    bool mode3_aer{};
    bool native_asymmetric{};
    bool taau_backend{};
    bool gameplay{};
    bool hmd_freelook{};
    bool projection_pipeline_ready{};
    float presentation_scale{1.0f};
};

// AER renders a symmetric envelope and publishes the two final eyes through
// AFW. Below scale 1, remap only that completed gameplay image into the same
// per-eye scaled OpenXR FOV used by strict Stereo. AER+TAAU also takes this
// route at scale 1 so it cannot fall through to the legacy cover crop. Cinema
// and every producer route remain outside this final-presentation policy.
constexpr bool final_openxr_remap_active(
    const FinalOpenXrRemapInput& input) noexcept {
    return input.mode3_aer && input.native_asymmetric && input.gameplay &&
        input.hmd_freelook && input.projection_pipeline_ready &&
        input.presentation_scale > 0.0f &&
        input.presentation_scale <= 1.0001f &&
        (input.taau_backend || input.presentation_scale < 0.9999f);
}

}  // namespace w3vr::aer_presentation_size_policy
