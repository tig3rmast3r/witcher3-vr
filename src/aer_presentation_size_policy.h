#pragma once

namespace w3vr::aer_presentation_size_policy {

struct FinalOpenXrRemapInput {
    bool mode3_aer{};
    bool native_asymmetric{};
    bool gameplay{};
    bool hmd_freelook{};
    bool projection_pipeline_ready{};
    float presentation_scale{1.0f};
};

// AER renders a symmetric envelope and publishes the two final eyes through
// AFW. Below scale 1, remap only that completed gameplay image into the same
// per-eye scaled OpenXR FOV used by strict Stereo. Cinema and every producer
// route remain outside this final-presentation policy.
constexpr bool final_openxr_remap_active(
    const FinalOpenXrRemapInput& input) noexcept {
    return input.mode3_aer && input.native_asymmetric && input.gameplay &&
        input.hmd_freelook && input.projection_pipeline_ready &&
        input.presentation_scale > 0.0f &&
        input.presentation_scale < 0.9999f;
}

}  // namespace w3vr::aer_presentation_size_policy
