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

enum class FinalTransport : uint8_t {
    Inactive,
    DirectCopy,
    IdentityShader,
    Unavailable,
};

// This describes how the producer encoded the pixels, not how an evaluator
// chose to represent its internal camera. PureDark is allowed to keep centered
// matrices while the completed texture still contains a native per-eye frame.
enum class AfwPixelProjection : uint8_t {
    Invalid,
    SharedSymmetric,
    NativeAsymmetric,
};

struct FinalSubmitInput {
    bool active{};
    bool source_pair_ready{};
    uint32_t source_width{};
    uint32_t source_height{};
    uint32_t swapchain_width{};
    uint32_t swapchain_height{};
    bool copy_compatible{};
    bool identity_shader_ready{};
};

struct FinalSubmitDecision {
    FinalTransport transport{FinalTransport::Inactive};
    uint32_t width{};
    uint32_t height{};
};

struct RuntimeProjectionTransitionDecision {
    bool apply{};
    bool native_asymmetric{};
};

struct ProjectionPairDecision {
    bool ready{};
    bool native_asymmetric{};
};

// Every Mode-3 route must prepare the structurally enrolled transparent-effect
// variants before F2 can request the native per-eye producer. Preparation is a
// static route property. Selection is frame-dynamic and requires both an
// actual native producer and a draw that has not already consumed the optical
// centre. The same contract is used by strict Stereo and AER.
constexpr bool native_asymmetric_effect_preparation_configured(
    bool trial_enabled,
    bool mode3,
    bool supported_backend,
    bool hmd_freelook) noexcept {
    return trial_enabled && mode3 && supported_backend && hmd_freelook;
}

constexpr bool native_asymmetric_effect_center_application_active(
    bool preparation_configured,
    bool actual_native_asymmetric,
    bool cinema_panel,
    bool full_vr_scene,
    bool full_vr_native_source) noexcept {
    return preparation_configured && actual_native_asymmetric &&
        !cinema_panel && (!full_vr_scene || full_vr_native_source);
}

// Validate the producer-owned projection at the exact AFW transaction. Native
// pixels require the source eye's post-rebuild factory proof and both immutable
// pair FOVs. AFW synthesizes the peer, so its factory bit is deliberately not
// required. A deliberate shared producer carries its own exact shared FOV.
constexpr AfwPixelProjection decide_afw_pixel_projection(
    AfwPixelProjection actual_producer,
    bool exact_identity_valid,
    uint32_t producer_generation,
    uint32_t current_generation,
    bool pair_slot_valid,
    uint32_t pair_generation,
    uint8_t factory_mask,
    uint32_t source_eye,
    bool frozen_pair_fovs_valid,
    bool shared_fov_valid) noexcept {
    if (!exact_identity_valid || source_eye > 1 ||
        producer_generation != current_generation) {
        return AfwPixelProjection::Invalid;
    }
    if (actual_producer == AfwPixelProjection::SharedSymmetric) {
        return shared_fov_valid
            ? AfwPixelProjection::SharedSymmetric
            : AfwPixelProjection::Invalid;
    }
    if (actual_producer != AfwPixelProjection::NativeAsymmetric) {
        return AfwPixelProjection::Invalid;
    }
    const uint8_t source_bit = static_cast<uint8_t>(1u << source_eye);
    return pair_slot_valid && pair_generation == producer_generation &&
            (factory_mask & source_bit) != 0 && frozen_pair_fovs_valid
        ? AfwPixelProjection::NativeAsymmetric
        : AfwPixelProjection::Invalid;
}

// Coalesce every physical F2 edge observed before the next safe Present
// boundary. An even number is a no-op; an odd number flips the producer once.
// Route eligibility is revalidated at that boundary, not merely when the key
// was sampled.
constexpr RuntimeProjectionTransitionDecision
decide_runtime_projection_transition(
    bool current_native_asymmetric,
    uint32_t request_count,
    bool route_eligible) noexcept {
    const bool apply = route_eligible && (request_count & 1u) != 0;
    return {
        apply,
        apply ? !current_native_asymmetric : current_native_asymmetric};
}

// A completed source can define one OpenXR projection only when both eyes
// belong to the current producer generation and encode the same geometry.
// Mixed SYM/ASYM or stale-generation pairs are unavailable; callers must hold
// or clear instead of interpreting either eye with the wrong rays.
constexpr ProjectionPairDecision decide_projection_pair(
    bool eye0_valid,
    bool eye1_valid,
    uint32_t eye0_generation,
    uint32_t eye1_generation,
    uint32_t current_generation,
    bool eye0_native_asymmetric,
    bool eye1_native_asymmetric) noexcept {
    const bool ready = eye0_valid && eye1_valid &&
        eye0_generation == current_generation &&
        eye1_generation == current_generation &&
        eye0_native_asymmetric == eye1_native_asymmetric;
    return {
        ready,
        ready && eye0_native_asymmetric};
}

// A Mode-3 producer already owns the selected full-frame resolution. Give
// OpenXR that exact destination extent so the GPU handoff remains an identity
// operation with no stretch, padding or presentation-scale resize. A genuine
// Mode-3 SYM source may still select a per-eye subImage from the completed
// texture; that optical choice never changes the texture transport extent.
// Legacy modes retain their existing runtime/presentation extent selection.
constexpr uint32_t select_swapchain_dimension(
    bool mode3_transport,
    uint32_t selected_source,
    uint32_t scaled_runtime,
    uint32_t presentation,
    uint32_t maximum) noexcept {
    const uint32_t requested = mode3_transport
        ? selected_source
        : (scaled_runtime > presentation ? scaled_runtime : presentation);
    return requested < maximum ? requested : maximum;
}

// A centered symmetric camera starts from the union of the two displaced
// runtime-eye frusta. Presentation Size is deliberately absent: it belongs to
// the final OpenXR presenter and must never alter producer geometry.
constexpr float symmetric_producer_fov_scale(
    float runtime_cover_fraction) noexcept {
    const float cover = runtime_cover_fraction < 0.5f
        ? 0.5f
        : (runtime_cover_fraction > 1.0f
            ? 1.0f
            : runtime_cover_fraction);
    return 1.0f / cover;
}

// Route, backend and projection encoding are deliberately absent. Once the
// completed eye pair is selected, every non-panel Mode-3 route uses this same
// full-texture handoff. Projection-specific OpenXR imageRect/FOV selection is
// a separate optical decision after transport.
constexpr FinalSubmitDecision decide_final_submit(
    const FinalSubmitInput& input) noexcept {
    if (!input.active) return {};
    FinalSubmitDecision result{
        FinalTransport::Unavailable,
        input.swapchain_width,
        input.swapchain_height};
    if (!input.source_pair_ready || input.source_width == 0 ||
        input.source_height == 0 ||
        input.source_width != input.swapchain_width ||
        input.source_height != input.swapchain_height) return result;
    result.transport = input.copy_compatible
        ? FinalTransport::DirectCopy
        : (input.identity_shader_ready
            ? FinalTransport::IdentityShader
            : FinalTransport::Unavailable);
    return result;
}

// A truly symmetric Mode-3 producer stores a centered envelope, so its final
// optical submit always selects the matching per-eye tangent interval. A
// native asymmetric producer already owns that interval in its full frame.
// This source-geometry decision is identical in strict Stereo and AER.
constexpr bool mode3_symmetric_subimage_active(
    bool mode3_transport,
    bool source_native_asymmetric) noexcept {
    return mode3_transport && !source_native_asymmetric;
}

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

// The late compositor must query ownership with the identity of the image it
// will actually publish. AER+AFW presents the completed AFW pair, not the
// renderer's newer trace pair; Cinema likewise presents its completed
// sequential pair. Strict Stereo without either final-source route continues
// to own the trace pair directly.
constexpr uint64_t hud_scene_only_source_pair_id(
    bool sequential_source_active,
    bool afw_final_source_ready,
    uint64_t final_scene_pair_id,
    uint64_t trace_pair_id) noexcept {
    return sequential_source_active || afw_final_source_ready
        ? final_scene_pair_id
        : trace_pair_id;
}

// Strict Stereo publishes the retained HUD for accepted predecessor H while
// the two submitted HUD command lists that prove scene-only ownership finish
// during the following stereo cadence. At the OpenXR boundary for scene N,
// the complete ownership record therefore belongs to exact predecessor H,
// not to N. This also applies to strict-stereo Cinema: its retained HUD
// readiness can suppress the baked text at H while the scene-N ownership
// record has not completed yet. The same delayed proof occurs in strict
// Full VR: its retained HUD target is H while the scene trace has advanced
// to N. AER/sequential routes and invalid relations retain the final image
// identity selected above.
constexpr uint64_t strict_stereo_late_hud_proof_pair_id(
    bool strict_stereo_join_active,
    HudProjectionRoute route,
    uint32_t current_generation,
    uint32_t target_generation,
    uint64_t final_scene_pair_id,
    uint64_t retained_hud_pair_id) noexcept {
    return strict_stereo_join_active &&
        (route == HudProjectionRoute::Gameplay ||
            route == HudProjectionRoute::Cinema ||
            route == HudProjectionRoute::FullVr) &&
        current_generation == target_generation &&
        final_scene_pair_id != 0 && final_scene_pair_id != UINT64_MAX &&
        retained_hud_pair_id != 0 && retained_hud_pair_id != UINT64_MAX &&
        retained_hud_pair_id < final_scene_pair_id
        ? retained_hud_pair_id
        : final_scene_pair_id;
}

// [FIX:HUD-FINAL-SCENE-OWNERSHIP V1536] A missing proof is allowed to block
// the late composite only after the proof lookup targets that exact published
// image. This keeps the native-HUD fail-open contract for genuinely baked
// output without mistaking DLSS5's ahead-of-AFW trace pair for the output.
constexpr bool late_hud_composite_source_ready(
    HudProjectionRoute /*route*/,
    bool scene_only_pair_ready) noexcept {
    return scene_only_pair_ready;
}

// A scene-only HUD draw prepares the following XR scene. It may use the
// already-completed HUD snapshot of the current accepted pair; the final
// compositor still requires that pair to precede the scene it submits.
constexpr bool retained_hud_snapshot_matches_scene(
    uint64_t target_pair, uint64_t requested_scene_pair,
    bool preparing_scene_draw) noexcept {
    return target_pair != 0 && target_pair != UINT64_MAX &&
        requested_scene_pair != 0 &&
        requested_scene_pair != UINT64_MAX &&
        (target_pair < requested_scene_pair ||
            (preparing_scene_draw && target_pair == requested_scene_pair));
}

// Strict Stereo labels the retained HUD texture with the accepted predecessor,
// but that texture label is not the identity of the backbuffer carrying the HUD
// draw. The ownership proof belongs to the current accepted scene. Keep the two
// identities separate so the scene-only ledger can authorize the exact image
// later submitted to OpenXR without breaking the native-HUD bootstrap.
constexpr uint64_t strict_stereo_scene_only_output_pair_id(
    uint64_t current_accepted_scene_pair,
    uint64_t accepted_predecessor_pair) noexcept {
    return current_accepted_scene_pair != 0 &&
        current_accepted_scene_pair != UINT64_MAX &&
        accepted_predecessor_pair != 0 &&
        accepted_predecessor_pair != UINT64_MAX &&
        accepted_predecessor_pair <= current_accepted_scene_pair
        ? current_accepted_scene_pair
        : 0;
}

// Automatic AER Full VR owns a sequential Cinema pair. Its baked HUD may be
// removed only after both that exact source family and the retained eye-local
// HUD have complete pairs. Other routes keep their established admission.
constexpr bool aer_full_vr_scene_only_admission_ready(
    bool aer_full_vr_route,
    bool sequential_cinema_pair_ready,
    bool retained_hud_pair_ready) noexcept {
    return !aer_full_vr_route ||
        (sequential_cinema_pair_ready && retained_hud_pair_ready);
}

// Strict Stereo gameplay already owns a native eye-local HUD draw. Its
// retained scene-only replacement is useful only after automatic/manual Cinema
// lifecycle begins; AER has an independent post-AFW route and does not consume
// this policy.
constexpr bool retained_hud_scene_lifecycle_active(
    bool cinema_active,
    bool automatic_full_vr_camera_active) noexcept {
    return cinema_active || automatic_full_vr_camera_active;
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

// REDengine can record the retained t1 copy, the final HUD draw and the game
// backbuffer PRESENT transition on separate command lists. Their queue
// submission order is authoritative, but worker-thread recording can straddle
// a Present counter edge. Accept only the same narrow producer interval and
// leave the native baked HUD visible for every wider or stale join.
inline constexpr uint64_t kSubmittedHudJoinMaxPresentDistance = 2;

constexpr bool submitted_hud_join_window_matches(
    uint32_t candidate_generation,
    uint32_t current_generation,
    uint64_t candidate_present,
    uint64_t boundary_present) noexcept {
    const uint64_t distance = candidate_present > boundary_present
        ? candidate_present - boundary_present
        : boundary_present - candidate_present;
    return candidate_generation == current_generation &&
        distance <= kSubmittedHudJoinMaxPresentDistance;
}

// Strict Stereo removes the native baked HUD before compositing its retained
// copy. A previously accepted HUD pair is safe only while it has reached the
// exact predecessor required by the currently published scene. Otherwise a
// loading fade captured by an old pair could be blended forever.
constexpr bool strict_stereo_retained_hud_pair_fresh(
    uint32_t current_generation,
    uint32_t target_generation,
    uint64_t target_pair,
    uint32_t accepted_generation,
    uint64_t accepted_pair) noexcept {
    return target_pair != 0 &&
        target_generation == current_generation &&
        accepted_generation == current_generation &&
        accepted_pair >= target_pair;
}

// The submitted-order fallback repairs D3D12 command-list topology, not an AFW
// or DLSS algorithm. AER enters it whenever a retained-HUD transport is
// configured, including its exact sequential Full-VR pair. Strict Stereo uses
// the same queue/generation/window contract for either temporal backend; its
// No-AA pointer-exact path remains unchanged.
constexpr bool submitted_hud_join_route_active(
    bool mode3_transport,
    bool aer_presentation,
    bool aer_retained_hud_transport,
    TemporalAdapter backend) noexcept {
    if (!mode3_transport) {
        return false;
    }
    if (aer_presentation) {
        return aer_retained_hud_transport;
    }
    return backend == TemporalAdapter::Dlss ||
        backend == TemporalAdapter::Taau;
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
