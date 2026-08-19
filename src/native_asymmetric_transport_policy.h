#pragma once

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

}  // namespace w3vr::native_asymmetric_transport_policy
