#pragma once

#include <cstdint>

namespace w3vr::mode3_transport {

enum class TemporalAdapter : uint8_t {
    None,
    Dlss,
    Taau,
};

enum class HudProjectionRoute : uint8_t {
    Gameplay,
    FullVr,
    Cinema,
};

// Projection is intentionally absent from this policy. Symmetric and
// asymmetric rendering may prepare different camera/FOV/shader inputs, but
// they must enter the same post-render transport once the temporal producer
// has an exact eye/pair identity.
constexpr bool dlss_submission_route_active(
    bool mode3_aer_active,
    TemporalAdapter backend) noexcept {
    return mode3_aer_active && backend == TemporalAdapter::Dlss;
}

constexpr bool final_backbuffer_route_active(
    bool mode3_aer_active,
    TemporalAdapter backend) noexcept {
    return mode3_aer_active &&
        (backend == TemporalAdapter::None ||
            backend == TemporalAdapter::Dlss);
}

constexpr TemporalAdapter exact_afw_backend(
    bool dlss_route_configured,
    bool taau_route_configured) noexcept {
    return dlss_route_configured
        ? TemporalAdapter::Dlss
        : (taau_route_configured
            ? TemporalAdapter::Taau
            : TemporalAdapter::None);
}

constexpr TemporalAdapter final_color_submission_backend(
    bool dlss_submission_active,
    bool taau_submission_active) noexcept {
    return dlss_submission_active
        ? TemporalAdapter::Dlss
        : (taau_submission_active
            ? TemporalAdapter::Taau
            : TemporalAdapter::None);
}

// Pending submissions are keyed by the exact command-list pointer. Scanning a
// hooked ExecuteCommandLists call is therefore safe on both the swapchain queue
// and any secondary queue; queue identity is not temporal identity.
constexpr bool submission_queue_eligible(
    bool route_active,
    bool /*queue_is_primary*/) noexcept {
    return route_active;
}

// Gameplay owns the validated continuously scene-only contract. Cinema and
// automatic Full VR can cross their activation boundary between the two AER
// renders, so their late HUD is safe only when the exact published pair proves
// that both final eyes were rendered without a baked native HUD.
constexpr bool late_hud_composite_source_ready(
    HudProjectionRoute route,
    bool scene_only_pair_ready) noexcept {
    return route == HudProjectionRoute::Gameplay ||
        scene_only_pair_ready;
}

}  // namespace w3vr::mode3_transport
