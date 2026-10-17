#include "mode3_transport_policy.h"
#include "mode3_hud_ownership.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <initializer_list>

int main() {
    using w3vr::mode3_transport::AfwPixelProjection;
    using w3vr::mode3_transport::FinalTransport;
    using w3vr::mode3_transport::HudProjectionRoute;
    using w3vr::mode3_transport::TemporalAdapter;
    using w3vr::mode3_transport::afw_gameplay_capture_allowed;
    using w3vr::mode3_transport::afw_queued_producer_expired;
    using w3vr::mode3_transport::dlss_submission_route_active;
    using w3vr::mode3_transport::decide_final_submit;
    using w3vr::mode3_transport::decide_afw_pixel_projection;
    using w3vr::mode3_transport::decide_projection_pair;
    using w3vr::mode3_transport::decide_runtime_projection_transition;
    using w3vr::mode3_transport::exact_afw_backend;
    using w3vr::mode3_transport::final_backbuffer_route_active;
    using w3vr::mode3_transport::final_color_submission_backend;
    using w3vr::mode3_transport::immutable_pair_view_ready;
    using w3vr::mode3_transport::hud_scene_only_source_pair_id;
    using w3vr::mode3_transport::late_hud_composite_source_ready;
    using w3vr::mode3_transport::retained_hud_snapshot_matches_scene;
    using w3vr::mode3_transport::strict_stereo_scene_only_output_pair_id;
    using w3vr::mode3_transport::aer_full_vr_scene_only_admission_ready;
    using w3vr::mode3_transport::retained_hud_scene_lifecycle_active;
    using w3vr::mode3_transport::native_hud_source_bootstrap_active;
    using w3vr::mode3_transport::real_smoke_variant_bootstrap_allowed;
    using w3vr::mode3_transport::select_swapchain_dimension;
    using w3vr::mode3_transport::submission_queue_eligible;
    using w3vr::mode3_transport::mode3_symmetric_subimage_active;
    using w3vr::mode3_transport::submitted_hud_join_route_active;
    using w3vr::mode3_transport::submitted_hud_join_window_matches;
    using w3vr::mode3_transport::native_asymmetric_effect_center_application_active;
    using w3vr::mode3_transport::native_asymmetric_effect_preparation_configured;
    using w3vr::mode3_transport::strict_stereo_retained_hud_pair_fresh;
    using w3vr::mode3_transport::symmetric_producer_fov_scale;

    // Effect variants and their functional metadata are prepared before F2,
    // including a SYM startup, for both Stereo and AER. Draw substitution is
    // native-ASym-only. Cinema panels and centered Full-VR fallbacks remain
    // suppressed; an exact native-ASym Full-VR source admits the same fix.
    const bool effect_preparation =
        native_asymmetric_effect_preparation_configured(
            true, true, true, true);
    assert(effect_preparation);
    assert(!native_asymmetric_effect_center_application_active(
        effect_preparation, false, false, false, false));
    assert(native_asymmetric_effect_center_application_active(
        effect_preparation, true, false, false, false));
    assert(!native_asymmetric_effect_center_application_active(
        effect_preparation, true, true, false, false));
    assert(!native_asymmetric_effect_center_application_active(
        effect_preparation, true, false, true, false));
    assert(native_asymmetric_effect_center_application_active(
        effect_preparation, true, false, true, true));
    assert(!native_asymmetric_effect_preparation_configured(
        false, true, true, true));
    assert(!native_asymmetric_effect_preparation_configured(
        true, false, true, true));
    assert(!native_asymmetric_effect_preparation_configured(
        true, true, false, true));
    assert(!native_asymmetric_effect_preparation_configured(
        true, true, true, false));

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

    const auto final_submit = decide_final_submit({
        true, true, 3072, 3264, 3072, 3264, true, true});
    assert(final_submit.transport == FinalTransport::DirectCopy);
    assert(final_submit.width == 3072 && final_submit.height == 3264);
    const auto identity_submit = decide_final_submit({
        true, true, 3072, 3264, 3072, 3264, true, true});
    assert(identity_submit.transport == FinalTransport::DirectCopy);
    assert(decide_final_submit({
        true, true, 2458, 2611, 3072, 3264, true, true})
        .transport == FinalTransport::Unavailable);

    // Mode 3 transports the complete selected-resolution texture into a
    // matching OpenXR destination. A genuine Mode-3 SYM source may select a
    // subImage from that texture, but the GPU handoff never resizes or pads it.
    assert(select_swapchain_dimension(
        true, 2496, 3072, 2496, 16384) == 2496);
    assert(select_swapchain_dimension(
        true, 2592, 3264, 2592, 16384) == 2592);
    assert(select_swapchain_dimension(
        true, 3072, 3072, 3072, 16384) == 3072);
    assert(select_swapchain_dimension(
        false, 2496, 3072, 3900, 16384) == 3900);
    assert(select_swapchain_dimension(
        true, 20000, 3072, 20000, 16384) == 16384);

    // Symmetric producer geometry always covers both displaced runtime eyes.
    // Presentation Size is a final OpenXR concern and has no producer input.
    // The calibrated Quest 3 cover is about 0.804821.
    const float symmetric_envelope =
        symmetric_producer_fov_scale(0.804821f);
    assert(symmetric_envelope > 1.24250f);
    assert(symmetric_envelope < 1.24253f);
    assert(symmetric_producer_fov_scale(1.0f) == 1.0f);
    assert(symmetric_producer_fov_scale(0.0f) == 2.0f);
    assert(symmetric_producer_fov_scale(2.0f) == 1.0f);

    // Every Mode-3 route maps a genuinely symmetric source to the per-eye
    // tangent interval. Runtime F2 changes the producer itself; it is not a
    // second optical gate. A native ASYM source always stays full-frame.
    assert(mode3_symmetric_subimage_active(true, false));
    assert(!mode3_symmetric_subimage_active(false, false));
    assert(!mode3_symmetric_subimage_active(true, true));

    // F2 requests are consumed only at an eligible safe boundary. Multiple
    // Present/Present1 observations collapse by parity, so two edges cannot
    // cause a needless generation reset.
    const auto sym_to_asym = decide_runtime_projection_transition(
        false, 1, true);
    assert(sym_to_asym.apply && sym_to_asym.native_asymmetric);
    const auto asym_to_sym = decide_runtime_projection_transition(
        true, 1, true);
    assert(asym_to_sym.apply && !asym_to_sym.native_asymmetric);
    const auto even_requests = decide_runtime_projection_transition(
        false, 2, true);
    assert(!even_requests.apply && !even_requests.native_asymmetric);
    const auto odd_requests = decide_runtime_projection_transition(
        true, 3, true);
    assert(odd_requests.apply && !odd_requests.native_asymmetric);
    const auto ineligible = decide_runtime_projection_transition(
        false, 1, false);
    assert(!ineligible.apply && !ineligible.native_asymmetric);

    // AER may publish through the final-eye cache, the packed cache or the
    // AFW sequencer. Whichever owner is selected, both eyes must prove the
    // same current generation and the same projection encoding.
    const auto symmetric_pair = decide_projection_pair(
        true, true, 7, 7, 7, false, false);
    assert(symmetric_pair.ready && !symmetric_pair.native_asymmetric);
    const auto asymmetric_pair = decide_projection_pair(
        true, true, 7, 7, 7, true, true);
    assert(asymmetric_pair.ready && asymmetric_pair.native_asymmetric);
    assert(!decide_projection_pair(
        false, true, 7, 7, 7, false, false).ready);
    assert(!decide_projection_pair(
        true, true, 6, 7, 7, false, false).ready);
    assert(!decide_projection_pair(
        true, true, 7, 7, 7, false, true).ready);

    // Centered AFW matrices do not classify the completed pixels. The exact
    // producer says Shared or Native; Native additionally proves the real eye
    // survived its factory rebuild. The synthesized peer needs no factory bit.
    assert(decide_afw_pixel_projection(
        AfwPixelProjection::NativeAsymmetric,
        true, 7, 7, true, 7, 0x1u, 0, true, false) ==
        AfwPixelProjection::NativeAsymmetric);
    assert(decide_afw_pixel_projection(
        AfwPixelProjection::NativeAsymmetric,
        true, 7, 7, true, 7, 0x2u, 0, true, false) ==
        AfwPixelProjection::Invalid);
    assert(decide_afw_pixel_projection(
        AfwPixelProjection::NativeAsymmetric,
        true, 7, 7, true, 7, 0x2u, 1, true, false) ==
        AfwPixelProjection::NativeAsymmetric);
    assert(decide_afw_pixel_projection(
        AfwPixelProjection::NativeAsymmetric,
        true, 7, 7, true, 7, 0x0u, 0, true, false) ==
        AfwPixelProjection::Invalid);
    assert(decide_afw_pixel_projection(
        AfwPixelProjection::NativeAsymmetric,
        true, 7, 7, true, 6, 0x1u, 0, true, false) ==
        AfwPixelProjection::Invalid);
    assert(decide_afw_pixel_projection(
        AfwPixelProjection::NativeAsymmetric,
        true, 7, 7, true, 7, 0x1u, 0, false, false) ==
        AfwPixelProjection::Invalid);
    assert(decide_afw_pixel_projection(
        AfwPixelProjection::SharedSymmetric,
        true, 7, 7, false, 0, 0, 0, false, true) ==
        AfwPixelProjection::SharedSymmetric);
    assert(decide_afw_pixel_projection(
        AfwPixelProjection::SharedSymmetric,
        true, 7, 7, false, 0, 0, 0, false, false) ==
        AfwPixelProjection::Invalid);
    assert(decide_afw_pixel_projection(
        AfwPixelProjection::Invalid,
        true, 7, 7, true, 7, 0x3u, 0, true, true) ==
        AfwPixelProjection::Invalid);

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

    // The ownership query follows the final image source. In AER+AFW the
    // completed output may trail DLSS5's current renderer trace; using 1280
    // below would suppress a valid retained HUD from final pair 640.
    assert(hud_scene_only_source_pair_id(false, true, 640, 1280) == 640);
    assert(hud_scene_only_source_pair_id(true, false, 700, 1400) == 700);
    assert(hud_scene_only_source_pair_id(false, false, 0, 1400) == 1400);

    // Reproduce PID39228: scene N advances before its HUD draw proof has
    // completed, while the exact same-generation predecessor is publishable.
    // All strict projections share this submission cadence, including Full VR.
    for (const auto route : {HudProjectionRoute::Gameplay,
                            HudProjectionRoute::Cinema,
                            HudProjectionRoute::FullVr}) {
        w3vr::mode3_transport::HudSceneOwnership ownership{};
        ownership.reset(9);
        for (uint64_t scene = 42; scene != 50; ++scene) {
            const uint64_t predecessor = scene - 1;
            ownership.record(9, predecessor, 0, true);
            ownership.record(9, predecessor, 1, true);
            assert(!ownership.ready_for_composite(9, scene));
            const uint64_t proof = strict_stereo_late_hud_proof_pair_id(
                true, route, 9, 9, scene, predecessor);
            assert(ownership.ready_for_composite(9, proof));
            // A native HUD draw revokes admission; another projection or an
            // old generation must never turn missing/negative proof positive.
            ownership.record(9, predecessor, 1, false);
            assert(!ownership.ready_for_composite(9, proof));
            assert(!ownership.ready_for_composite(9,
                strict_stereo_late_hud_proof_pair_id(
                    false, route, 9, 9, scene, predecessor)));
            assert(!ownership.ready_for_composite(9,
                strict_stereo_late_hud_proof_pair_id(
                    true, route, 9, 8, scene, predecessor)));
        }
    }

    // The delayed identity never leaks into AER, another generation, or a
    // malformed relation.
    assert(strict_stereo_late_hud_proof_pair_id(
        true, HudProjectionRoute::Gameplay, 9, 9, 42, 41) == 41);
    assert(strict_stereo_late_hud_proof_pair_id(
        false, HudProjectionRoute::Gameplay, 9, 9, 42, 41) == 42);
    assert(strict_stereo_late_hud_proof_pair_id(
        true, HudProjectionRoute::Cinema, 9, 9, 42, 41) == 41);
    assert(strict_stereo_late_hud_proof_pair_id(
        false, HudProjectionRoute::Cinema, 9, 9, 42, 41) == 42);
    assert(strict_stereo_late_hud_proof_pair_id(
        true, HudProjectionRoute::Cinema, 9, 8, 42, 41) == 42);
    assert(strict_stereo_late_hud_proof_pair_id(
        true, HudProjectionRoute::FullVr, 9, 9, 42, 41) == 41);
    assert(strict_stereo_late_hud_proof_pair_id(
        false, HudProjectionRoute::FullVr, 9, 9, 42, 41) == 42);
    assert(strict_stereo_late_hud_proof_pair_id(
        true, HudProjectionRoute::FullVr, 9, 8, 42, 41) == 42);
    assert(strict_stereo_late_hud_proof_pair_id(
        true, HudProjectionRoute::FullVr, 9, 9, 42, 42) == 42);
    assert(strict_stereo_late_hud_proof_pair_id(
        true, HudProjectionRoute::Gameplay, 9, 8, 42, 41) == 42);
    assert(strict_stereo_late_hud_proof_pair_id(
        true, HudProjectionRoute::Gameplay, 9, 9, 42, 42) == 42);
    assert(strict_stereo_late_hud_proof_pair_id(
        true, HudProjectionRoute::Gameplay, 9, 9, 42, 43) == 42);
    assert(strict_stereo_late_hud_proof_pair_id(
        true, HudProjectionRoute::Gameplay, 9, 9, 0, 41) == 0);

    // Every route still fails open on its native HUD until that selected final
    // pair proves both eyes were rendered scene-only; the fix changes the
    // queried identity, not the single-owner safety rule.
    assert(!late_hud_composite_source_ready(
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

    // The retained texture keeps the predecessor label, while scene-only
    // ownership follows the current scene submitted to OpenXR.
    assert(strict_stereo_scene_only_output_pair_id(42, 41) == 42);
    assert(strict_stereo_scene_only_output_pair_id(42, 42) == 42);
    assert(strict_stereo_scene_only_output_pair_id(0, 41) == 0);
    assert(strict_stereo_scene_only_output_pair_id(42, 0) == 0);
    assert(strict_stereo_scene_only_output_pair_id(41, 42) == 0);
    assert(retained_hud_snapshot_matches_scene(41, 42, false));
    assert(!retained_hud_snapshot_matches_scene(42, 42, false));
    assert(retained_hud_snapshot_matches_scene(42, 42, true));
    assert(!retained_hud_snapshot_matches_scene(43, 42, true));
    assert(!retained_hud_snapshot_matches_scene(0, 42, true));

    // AER automatic Full VR may never inherit AFW gameplay readiness. It
    // removes the baked HUD only when both its sequential scene and retained
    // HUD pairs are complete; unrelated routes are unaffected.
    assert(!aer_full_vr_scene_only_admission_ready(true, false, false));
    assert(!aer_full_vr_scene_only_admission_ready(true, true, false));
    assert(!aer_full_vr_scene_only_admission_ready(true, false, true));
    assert(aer_full_vr_scene_only_admission_ready(true, true, true));
    assert(aer_full_vr_scene_only_admission_ready(false, false, false));

    // Strict Stereo gameplay keeps the native baked HUD. Retained removal may
    // start on the early Full-VR camera flag or the later Cinema detector.
    assert(!retained_hud_scene_lifecycle_active(false, false));
    assert(retained_hud_scene_lifecycle_active(true, false));
    assert(retained_hud_scene_lifecycle_active(false, true));
    assert(retained_hud_scene_lifecycle_active(true, true));

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

    // A stale retained HUD must not outlive loading and darken newer Stereo
    // scene pairs. Bootstrap remains on the native baked HUD until a retained
    // pair reaches the exact predecessor target in the current generation.
    assert(!strict_stereo_retained_hud_pair_fresh(7, 7, 0, 7, 1));
    assert(!strict_stereo_retained_hud_pair_fresh(7, 7, 100, 7, 99));
    assert(!strict_stereo_retained_hud_pair_fresh(7, 6, 100, 7, 100));
    assert(!strict_stereo_retained_hud_pair_fresh(7, 7, 100, 6, 100));
    assert(strict_stereo_retained_hud_pair_fresh(7, 7, 100, 7, 100));
    assert(strict_stereo_retained_hud_pair_fresh(7, 7, 100, 7, 101));

    // AER uses the submitted-order safety net whenever its retained-HUD route
    // is configured, including sequential Full VR with No AA. Strict Stereo
    // keeps the same fallback for temporal backends, but not No AA.
    assert(submitted_hud_join_route_active(
        true, true, true, TemporalAdapter::Dlss));
    assert(submitted_hud_join_route_active(
        true, true, true, TemporalAdapter::Taau));
    assert(submitted_hud_join_route_active(
        true, true, true, TemporalAdapter::None));
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
