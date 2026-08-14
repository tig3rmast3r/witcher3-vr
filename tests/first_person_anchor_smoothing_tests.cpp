#include "first_person_anchor_smoothing.h"

#include <cmath>
#include <cstdio>

namespace {

bool near(float actual, float expected, float epsilon = 0.0001f) {
    if (std::fabs(actual - expected) <= epsilon) {
        return true;
    }
    std::fprintf(stderr, "actual %.6f expected %.6f\n", actual, expected);
    return false;
}

bool near3(const float* actual, const float* expected) {
    return near(actual[0], expected[0]) && near(actual[1], expected[1]) &&
        near(actual[2], expected[2]);
}

}  // namespace

int main() {
    using w3vr::first_person::AnchorSmoothingState;
    using w3vr::first_person::exponential_smoothing_blend;
    using w3vr::first_person::kAnchorSmoothingMaxSeconds;
    using w3vr::first_person::kAnchorSmoothingMinSeconds;
    using w3vr::first_person::smooth_anchor_lateral_vertical;

    const float forward[3]{0.0f, 1.0f, 0.0f};
    const float root0[3]{10.0f, 20.0f, 2.0f};
    const float anchor0[3]{10.0f, 23.0f, 4.0f};
    float output[3]{};
    AnchorSmoothingState state{};
    if (!smooth_anchor_lateral_vertical(
            anchor0, root0, forward, 0.25f, state, output) ||
        !near3(output, anchor0)) {
        return 1;
    }

    // A lateral/vertical step is filtered, while the new depth of 5 m is
    // applied immediately.
    const float anchor1[3]{12.0f, 25.0f, 6.0f};
    const float expected1[3]{10.5f, 25.0f, 4.5f};
    if (!smooth_anchor_lateral_vertical(
            anchor1, root0, forward, 0.25f, state, output) ||
        !near3(output, expected1)) {
        return 2;
    }

    // Root locomotion is never filtered: the same relative pose follows a
    // ten-metre root translation in one call.
    const float root1[3]{20.0f, 30.0f, 12.0f};
    const float anchor2[3]{22.0f, 35.0f, 16.0f};
    const float expected2[3]{20.875f, 35.0f, 14.875f};
    if (!smooth_anchor_lateral_vertical(
            anchor2, root1, forward, 0.25f, state, output) ||
        !near3(output, expected2)) {
        return 3;
    }

    if (!near(exponential_smoothing_blend(0.08f, 0.08f),
            1.0f - std::exp(-1.0f)) ||
        !near(exponential_smoothing_blend(0.01f, 0.0f), 1.0f)) {
        return 4;
    }
    if (!near(kAnchorSmoothingMinSeconds, 0.08f) ||
        !near(kAnchorSmoothingMaxSeconds, 0.20f) ||
        exponential_smoothing_blend(
            1.0f / 90.0f, kAnchorSmoothingMaxSeconds) >=
            exponential_smoothing_blend(
                1.0f / 90.0f, kAnchorSmoothingMinSeconds)) {
        return 5;
    }

    const float invalid_forward[3]{0.0f, 0.0f, 1.0f};
    if (smooth_anchor_lateral_vertical(
            anchor2, root1, invalid_forward, 0.5f, state, output)) {
        return 6;
    }
    return 0;
}
