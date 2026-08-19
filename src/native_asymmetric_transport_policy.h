#pragma once

namespace w3vr::native_asymmetric_transport_policy {

struct FullVrFrameFallbackInput {
    bool native_asymmetric_route{};
    bool automatic_full_vr{};
};

// V1242/V1252 prove that the final-frame repair owns only a reused-camera
// Full-VR interval. AER already needs centered fallback pixels, and V1254's
// strict Stereo trace proves that its failed native pair is also consumed by
// the symmetric presenter. Keep that exact fallback centered in either
// presentation route; ordinary factory-built asymmetric pairs never reach it.
constexpr bool frame_fallback_uses_symmetric_projection(
    const FullVrFrameFallbackInput& input) noexcept {
    return input.native_asymmetric_route && input.automatic_full_vr;
}

struct StereoFrameFallbackAdmissionInput {
    bool tagged_stereo_frame{};
    bool symmetric_asymmetric_full_vr_fallback{};
    bool full_vr_factory_camera_recent{};
};

// The final-frame fallback owns only REDengine's reused-camera interval. A
// recent Full-VR perspective factory has already applied the complete camera
// for an ordinary cutscene, so rewriting that frame creates a second camera
// scale and separates terrain/foliage layers.
constexpr bool stereo_frame_fallback_admissible(
    const StereoFrameFallbackAdmissionInput& input) noexcept {
    return input.tagged_stereo_frame &&
        !(input.symmetric_asymmetric_full_vr_fallback &&
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
