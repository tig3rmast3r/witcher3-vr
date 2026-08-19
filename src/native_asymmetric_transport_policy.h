#pragma once

namespace w3vr::native_asymmetric_transport_policy {

struct FullVrFrameFallbackInput {
    bool native_asymmetric_route{};
    bool aer_presentation{};
    bool automatic_full_vr{};
};

// V1242 proves that ordinary AER Cinema pairs must keep the established
// symmetric presentation. Only a reused-camera Full-VR frame lacks the normal
// factory correction; keep that exact fallback symmetric as well instead of
// manufacturing raw off-axis pixels that the AER Cinema presenter does not
// own.
constexpr bool frame_fallback_uses_symmetric_projection(
    const FullVrFrameFallbackInput& input) noexcept {
    return input.native_asymmetric_route &&
        input.aer_presentation && input.automatic_full_vr;
}

struct StereoFrameFallbackAdmissionInput {
    bool tagged_stereo_frame{};
    bool symmetric_aer_full_vr_fallback{};
    bool full_vr_factory_camera_recent{};
};

// The final-frame fallback owns only REDengine's reused-camera interval. A
// recent Full-VR perspective factory has already applied the complete camera
// for an ordinary cutscene, so rewriting that frame creates a second camera
// scale and separates terrain/foliage layers.
constexpr bool stereo_frame_fallback_admissible(
    const StereoFrameFallbackAdmissionInput& input) noexcept {
    return input.tagged_stereo_frame &&
        !(input.symmetric_aer_full_vr_fallback &&
            input.full_vr_factory_camera_recent);
}

struct CinemaPresentationInput {
    bool packed_pair_native_asymmetric{};
    bool sequential_aer_pair_available{};
};

// The strict packed cache owns native off-axis presentation. The independent
// sequential AER Cinema cache is intentionally symmetric, even when a complete
// pair is available; promoting it was the V1248 regression.
constexpr bool cinema_presentation_uses_native_asymmetric(
    const CinemaPresentationInput& input) noexcept {
    return input.packed_pair_native_asymmetric;
}

struct PreflightInput {
    bool transport_capable{};
    bool cinema_mode{};
    bool automatic_full_vr_camera_active{};
    bool cinema_full_vr_enabled{};
    bool force_mono_cinema{};
    bool aer_presentation{};
};

constexpr bool preflight_ready(const PreflightInput& input) noexcept {
    const bool automatic_full_vr =
        !input.aer_presentation &&
        input.cinema_mode &&
        input.automatic_full_vr_camera_active &&
        input.cinema_full_vr_enabled &&
        !input.force_mono_cinema;
    return input.transport_capable &&
        (!input.cinema_mode || automatic_full_vr);
}

}  // namespace w3vr::native_asymmetric_transport_policy
