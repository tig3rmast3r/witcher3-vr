#include "aer_scheduler.h"

#include <cstdio>

namespace {

int failures{};

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

}  // namespace

int main() {
    using w3vr::aer::identity_for_present_count;
    using w3vr::aer::identity_for_render_ordinal;

    const auto render0 = identity_for_render_ordinal(0);
    const auto render1 = identity_for_render_ordinal(1);
    const auto render2 = identity_for_render_ordinal(2);
    const auto render3 = identity_for_render_ordinal(3);
    require(render0.eye == 0 && render0.pair_id == 1,
        "render 0 must be left eye of pair 1");
    require(render1.eye == 1 && render1.pair_id == 1,
        "render 1 must be right eye of pair 1");
    require(render2.eye == 0 && render2.pair_id == 2,
        "render 2 must be left eye of pair 2");
    require(render3.eye == 1 && render3.pair_id == 2,
        "render 3 must be right eye of pair 2");

    const auto present1 = identity_for_present_count(1);
    const auto present2 = identity_for_present_count(2);
    require(present1.eye == 0 && present1.pair_id == 1,
        "first Present must publish render 0");
    require(present2.eye == 1 && present2.pair_id == 1,
        "second Present must publish render 1");

    const auto saturated = identity_for_present_count(0);
    require(saturated.eye == 0 && saturated.pair_id == 1,
        "zero Present count must not underflow");

    if (failures != 0) {
        std::fprintf(stderr, "%d AER scheduler test(s) failed\n", failures);
        return 1;
    }
    std::puts("AER scheduler tests passed");
    return 0;
}
