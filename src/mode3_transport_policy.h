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

// Automatic Full VR takes ownership before the legacy Cinema detector can
// cross its debounce window. Stop admitting AFW gameplay producers on that
// native edge as well, otherwise the unconsumed producer FIFO can fill before
// Cinema becomes visible to the older gate.
constexpr bool afw_gameplay_capture_allowed(
    bool route_active,
    int menu_state,
    bool cinema_active,
    bool loading_active,
    bool automatic_full_vr_active) noexcept {
    return route_active && menu_state == 0 && !cinema_active &&
        !loading_active && !automatic_full_vr_active;
}

// A queued producer is only a short-lived bridge between the temporal
// callback and presentation. If no exact consumer can use it for this many
// Presents, retaining it is less safe than dropping back to the real frame and
// allowing the bounded ring to repopulate.
inline constexpr uint64_t kAfwStaleProducerMaxAgePresents = 8;

constexpr bool afw_queued_producer_expired(
    uint64_t present,
    uint64_t ready_present) noexcept {
    return ready_present != UINT64_MAX && present > ready_present &&
        present - ready_present >= kAfwStaleProducerMaxAgePresents;
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

// Retained HUD capture must be able to discover REDengine's native t1 source
// before the scene-only pair exists. AER and Cinema already admit the complete
// HUD pipeline family during that fail-open interval; strict Stereo needs the
// same bootstrap or it can never produce the pair that enables scene-only.
constexpr bool native_hud_source_bootstrap_active(
    bool aer_post_hud_active,
    bool cinema_active,
    bool strict_stereo_active) noexcept {
    return aer_post_hud_active || cinema_active || strict_stereo_active;
}

// Eye-specific smoke variants need the immutable runtime FOV. The zero-centre
// world-up variant does not: it must always exist as the stable fail-open so a
// startup ordering difference can never restore REDengine's HMD-facing smoke.
constexpr bool real_smoke_variant_bootstrap_allowed(
    uint32_t variant_index,
    bool runtime_views_ready) noexcept {
    return variant_index == 2 ||
        (variant_index < 2 && runtime_views_ready);
}

// Pixels retained in a completed Cinema pair must keep the XrView frozen with
// that same pair. The sequential AER route deliberately does not set the
// general packed-pair validity bit, so its independent completion authority
// must be sufficient to select the packed view as well as the packed texture.
constexpr bool immutable_pair_view_ready(
    bool sequential_cinema_pair_available,
    bool packed_pair_available,
    bool packed_view_valid) noexcept {
    return (sequential_cinema_pair_available || packed_pair_available) &&
        packed_view_valid;
}

}  // namespace w3vr::mode3_transport
