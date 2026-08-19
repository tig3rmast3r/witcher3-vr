#include "mode3_transport_policy.h"

#include <cassert>
#include <initializer_list>

int main() {
    using w3vr::mode3_transport::DlssCompletionOwner;
    using w3vr::mode3_transport::HudProjectionRoute;
    using w3vr::mode3_transport::TemporalAdapter;
    using w3vr::mode3_transport::afw_gameplay_capture_allowed;
    using w3vr::mode3_transport::afw_queued_producer_expired;
    using w3vr::mode3_transport::dlss_submission_route_active;
    using w3vr::mode3_transport::dlss_completion_apply_public_jitter;
    using w3vr::mode3_transport::dlss_completion_capture_public_bundle;
    using w3vr::mode3_transport::dlss_completion_ngx_uses_public_bundle;
    using w3vr::mode3_transport::dlss_completion_resolved_owner;
    using w3vr::mode3_transport::exact_afw_backend;
    using w3vr::mode3_transport::final_backbuffer_route_active;
    using w3vr::mode3_transport::final_color_submission_backend;
    using w3vr::mode3_transport::immutable_pair_view_ready;
    using w3vr::mode3_transport::late_hud_composite_source_ready;
    using w3vr::mode3_transport::native_hud_source_bootstrap_active;
    using w3vr::mode3_transport::real_smoke_variant_bootstrap_allowed;
    using w3vr::mode3_transport::submission_queue_eligible;
    using w3vr::mode3_transport::streamline_dlss_evaluate_callback_active;
    using w3vr::mode3_transport::submitted_hud_join_route_active;
    using w3vr::mode3_transport::submitted_hud_join_window_matches;

    // The public owner is intentionally narrow. Only Mode-3 OpenXR DLSS can
    // enter it. Projection is not an input, so AER and strict Stereo receive
    // the same callback policy.
    assert(streamline_dlss_evaluate_callback_active(
        true, true, 3, TemporalAdapter::Dlss));
    assert(!streamline_dlss_evaluate_callback_active(
        false, true, 3, TemporalAdapter::Dlss));
    assert(!streamline_dlss_evaluate_callback_active(
        true, false, 3, TemporalAdapter::Dlss));
    assert(!streamline_dlss_evaluate_callback_active(
        true, true, 2, TemporalAdapter::Dlss));
    assert(!streamline_dlss_evaluate_callback_active(
        true, true, 3, TemporalAdapter::Taau));

    // The first exact evaluation probes both boundaries. Once an owner is
    // observed, only that route records a normal per-frame producer. Public
    // jitter is delayed until the public route has actually been proven.
    assert(dlss_completion_capture_public_bundle(
        DlssCompletionOwner::Probe));
    assert(!dlss_completion_apply_public_jitter(
        DlssCompletionOwner::Probe));
    assert(!dlss_completion_capture_public_bundle(
        DlssCompletionOwner::Ngx));
    assert(dlss_completion_capture_public_bundle(
        DlssCompletionOwner::Streamline));
    assert(dlss_completion_apply_public_jitter(
        DlssCompletionOwner::Streamline));
    assert(dlss_completion_ngx_uses_public_bundle(
        DlssCompletionOwner::Probe, true));
    assert(!dlss_completion_ngx_uses_public_bundle(
        DlssCompletionOwner::Ngx, true));
    assert(dlss_completion_ngx_uses_public_bundle(
        DlssCompletionOwner::Streamline, true));
    assert(!dlss_completion_ngx_uses_public_bundle(
        DlssCompletionOwner::Streamline, false));
    assert(dlss_completion_resolved_owner(true) ==
        DlssCompletionOwner::Ngx);
    assert(dlss_completion_resolved_owner(false) ==
        DlssCompletionOwner::Streamline);

    // Projection is not an input: both symmetric and asymmetric exercise this
    // same policy and must obtain exactly the same answers.
    for (const bool native_asymmetric : {false, true}) {
        (void)native_asymmetric;
        assert(dlss_submission_route_active(true, TemporalAdapter::Dlss));
        assert(final_backbuffer_route_active(true, TemporalAdapter::Dlss));
        assert(final_backbuffer_route_active(true, TemporalAdapter::None));
        assert(!final_backbuffer_route_active(true, TemporalAdapter::Taau));

        assert(exact_afw_backend(true, false) == TemporalAdapter::Dlss);
        assert(exact_afw_backend(false, true) == TemporalAdapter::Taau);
        assert(final_color_submission_backend(true, false) ==
            TemporalAdapter::Dlss);
        assert(final_color_submission_backend(false, true) ==
            TemporalAdapter::Taau);
    }

    assert(!dlss_submission_route_active(false, TemporalAdapter::Dlss));
    assert(!dlss_submission_route_active(true, TemporalAdapter::Taau));
    assert(!final_backbuffer_route_active(false, TemporalAdapter::Dlss));
    assert(exact_afw_backend(false, false) == TemporalAdapter::None);
    assert(final_color_submission_backend(false, false) ==
        TemporalAdapter::None);

    // An exact command-list publication must be observable on either queue.
    assert(submission_queue_eligible(true, true));
    assert(submission_queue_eligible(true, false));
    assert(!submission_queue_eligible(false, true));
    assert(!submission_queue_eligible(false, false));

    // Native automatic Full VR can start before the debounced Cinema flag.
    // That early ownership edge must stop gameplay capture by itself so no
    // producer can be stranded while the final cutscene route is active.
    assert(afw_gameplay_capture_allowed(true, 0, false, false, false));
    assert(!afw_gameplay_capture_allowed(false, 0, false, false, false));
    assert(!afw_gameplay_capture_allowed(true, 1, false, false, false));
    assert(!afw_gameplay_capture_allowed(true, 0, true, false, false));
    assert(!afw_gameplay_capture_allowed(true, 0, false, true, false));
    assert(!afw_gameplay_capture_allowed(true, 0, false, false, true));

    // An unselectable AFW packet gets a short grace period, then loses to the
    // real-frame fallback so the fixed-size producer ring cannot deadlock.
    assert(!afw_queued_producer_expired(100, UINT64_MAX));
    assert(!afw_queued_producer_expired(100, 100));
    assert(!afw_queued_producer_expired(107, 100));
    assert(afw_queued_producer_expired(108, 100));
    assert(afw_queued_producer_expired(1000, 100));

    // Gameplay continuously removes the baked HUD. A cutscene or Cinema
    // boundary must fail open on its native HUD until the exact final pair
    // proves both eyes were rendered scene-only; otherwise the late layer
    // would draw the same subtitle a second time.
    assert(late_hud_composite_source_ready(
        HudProjectionRoute::Gameplay, false));
    assert(late_hud_composite_source_ready(
        HudProjectionRoute::Gameplay, true));
    assert(!late_hud_composite_source_ready(
        HudProjectionRoute::FullVr, false));
    assert(late_hud_composite_source_ready(
        HudProjectionRoute::FullVr, true));
    assert(!late_hud_composite_source_ready(
        HudProjectionRoute::Cinema, false));
    assert(late_hud_composite_source_ready(
        HudProjectionRoute::Cinema, true));

    // Every retained-HUD route must be able to discover native t1 before the
    // first complete scene-only pair exists. Strict Stereo previously omitted
    // its bootstrap and could not transition away from the baked HUD.
    assert(native_hud_source_bootstrap_active(true, false, false));
    assert(native_hud_source_bootstrap_active(false, true, false));
    assert(native_hud_source_bootstrap_active(false, false, true));
    assert(!native_hud_source_bootstrap_active(false, false, false));

    // Cross-command-list retained HUD metadata is admitted only inside the
    // short recording/submission interval for the current renderer generation.
    // A one- or two-Present skew is possible when worker lists straddle the
    // counter edge; anything older remains on the baked fail-open path.
    assert(submitted_hud_join_window_matches(7, 7, 100, 100));
    assert(submitted_hud_join_window_matches(7, 7, 99, 100));
    assert(submitted_hud_join_window_matches(7, 7, 98, 100));
    assert(submitted_hud_join_window_matches(7, 7, 102, 100));
    assert(!submitted_hud_join_window_matches(7, 7, 97, 100));
    assert(!submitted_hud_join_window_matches(7, 8, 100, 100));

    // AER retains its AFW-scoped contract. Strict Stereo uses the same
    // submitted-order safety net for both temporal backends, but not No AA.
    assert(submitted_hud_join_route_active(
        true, true, true, TemporalAdapter::Dlss));
    assert(submitted_hud_join_route_active(
        true, true, true, TemporalAdapter::Taau));
    assert(!submitted_hud_join_route_active(
        true, true, false, TemporalAdapter::Dlss));
    assert(submitted_hud_join_route_active(
        true, false, false, TemporalAdapter::Dlss));
    assert(submitted_hud_join_route_active(
        true, false, false, TemporalAdapter::Taau));
    assert(!submitted_hud_join_route_active(
        true, false, false, TemporalAdapter::None));
    assert(!submitted_hud_join_route_active(
        false, false, false, TemporalAdapter::Dlss));

    // Runtime FOV is required only for the two optical-centre variants. The
    // independent world-up fallback must never depend on first-camera timing.
    assert(!real_smoke_variant_bootstrap_allowed(0, false));
    assert(!real_smoke_variant_bootstrap_allowed(1, false));
    assert(real_smoke_variant_bootstrap_allowed(2, false));
    assert(real_smoke_variant_bootstrap_allowed(0, true));
    assert(real_smoke_variant_bootstrap_allowed(1, true));
    assert(real_smoke_variant_bootstrap_allowed(2, true));
    assert(!real_smoke_variant_bootstrap_allowed(3, true));

    // AER Cinema keeps its completed pixels in the packed resources without
    // publishing the unrelated strict-Stereo packed-valid bit. Its completed
    // pair authority must still select the matching immutable XrView. During
    // the next half-pair, advancing staging views must never replace it.
    assert(immutable_pair_view_ready(true, false, true));
    assert(!immutable_pair_view_ready(true, false, false));
    assert(immutable_pair_view_ready(false, true, true));
    assert(!immutable_pair_view_ready(false, false, true));
    return 0;
}
