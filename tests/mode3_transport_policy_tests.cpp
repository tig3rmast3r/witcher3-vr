#include "mode3_transport_policy.h"

#include <cassert>
#include <initializer_list>

int main() {
    using w3vr::mode3_transport::HudProjectionRoute;
    using w3vr::mode3_transport::TemporalAdapter;
    using w3vr::mode3_transport::dlss_submission_route_active;
    using w3vr::mode3_transport::exact_afw_backend;
    using w3vr::mode3_transport::final_backbuffer_route_active;
    using w3vr::mode3_transport::final_color_submission_backend;
    using w3vr::mode3_transport::immutable_pair_view_ready;
    using w3vr::mode3_transport::late_hud_composite_source_ready;
    using w3vr::mode3_transport::native_hud_source_bootstrap_active;
    using w3vr::mode3_transport::submission_queue_eligible;

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
