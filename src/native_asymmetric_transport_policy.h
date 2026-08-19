#pragma once

#include <cstdint>

namespace w3vr::native_asymmetric_transport_policy {

struct PreflightInput {
    bool transport_capable{};
    bool cinema_mode{};
    bool automatic_full_vr_camera_active{};
    bool cinema_full_vr_enabled{};
    bool force_mono_cinema{};
};

constexpr bool preflight_ready(const PreflightInput& input) noexcept {
    const bool automatic_full_vr =
        input.cinema_mode &&
        input.automatic_full_vr_camera_active &&
        input.cinema_full_vr_enabled &&
        !input.force_mono_cinema;
    return input.transport_capable &&
        (!input.cinema_mode || automatic_full_vr);
}

struct CinemaPairInput {
    bool native_asymmetric_required{};
    bool slot_matches_pair_and_generation{};
    uint8_t factory_mask{};
    uint8_t temporal_mask{};
    uint8_t dlss_input_mask{};
    bool dlss_input_required{};
};

// Normal/manual Cinema remains symmetric and does not require the native
// asymmetric ledger. Automatic Full VR may publish off-axis pixels only after
// both eyes have completed every producer proof required by the backend.
constexpr bool cinema_pair_admissible(
    const CinemaPairInput& input) noexcept {
    if (!input.native_asymmetric_required) {
        return true;
    }
    return input.slot_matches_pair_and_generation &&
        input.factory_mask == 0x3u &&
        input.temporal_mask == 0x3u &&
        (!input.dlss_input_required || input.dlss_input_mask == 0x3u);
}

constexpr bool presentation_uses_native_asymmetric(
    bool packed_pair_native_asymmetric,
    bool sequential_cinema_pair_available,
    bool sequential_cinema_pair_native_asymmetric) noexcept {
    return packed_pair_native_asymmetric ||
        (sequential_cinema_pair_available &&
            sequential_cinema_pair_native_asymmetric);
}

}  // namespace w3vr::native_asymmetric_transport_policy
