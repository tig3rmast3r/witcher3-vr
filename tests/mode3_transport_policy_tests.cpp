#include "mode3_transport_policy.h"

#include <cassert>
#include <initializer_list>

int main() {
    using w3vr::mode3_transport::TemporalAdapter;
    using w3vr::mode3_transport::dlss_submission_route_active;
    using w3vr::mode3_transport::exact_afw_backend;
    using w3vr::mode3_transport::final_backbuffer_route_active;
    using w3vr::mode3_transport::final_color_submission_backend;
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
    return 0;
}
