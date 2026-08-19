#include "native_asymmetric_transport_policy.h"

#include <cstdlib>
#include <iostream>

namespace policy = w3vr::native_asymmetric_transport_policy;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main() {
    require(!policy::frame_fallback_uses_symmetric_projection(
            {false, true, true}),
        "an unconfigured asymmetric route needs no fallback override");
    require(!policy::frame_fallback_uses_symmetric_projection(
            {true, true, false}),
        "AER gameplay must keep its established asymmetric route");
    require(!policy::frame_fallback_uses_symmetric_projection(
            {true, false, true}),
        "strict Stereo Full VR must remain on its asymmetric route");
    require(policy::frame_fallback_uses_symmetric_projection(
            {true, true, true}),
        "only the AER Full VR frame fallback must be symmetric");

    require(!policy::stereo_frame_fallback_admissible(
            {false, true, false}),
        "an untagged frame cannot own the stereo fallback");
    require(policy::stereo_frame_fallback_admissible(
            {true, true, false}),
        "AER must repair a reused Full VR camera when the factory is stale");
    require(!policy::stereo_frame_fallback_admissible(
            {true, true, true}),
        "AER must not rewrite an ordinary recent factory camera");
    require(policy::stereo_frame_fallback_admissible(
            {true, false, true}),
        "strict Stereo keeps its established internal fallback proof");

    require(!policy::cinema_presentation_uses_native_asymmetric(
            {false, true}),
        "a sequential AER Cinema pair must retain symmetric presentation");
    require(policy::cinema_presentation_uses_native_asymmetric(
            {true, false}),
        "the strict packed native pair must retain off-axis presentation");

    require(!policy::preflight_ready({false, false, false, true, false}),
        "invalid transport must fail closed");
    require(policy::preflight_ready({true, false, false, true, false}),
        "gameplay keeps the established preflight route");
    require(policy::preflight_ready({true, true, true, true, false}),
        "strict Stereo Full VR may bootstrap asymmetric transport");
    require(!policy::preflight_ready({true, true, true, true, false, true}),
        "AER Full VR must retain V1242 symmetric Cinema preflight");
    require(!policy::preflight_ready({true, true, false, true, false}),
        "Cinema without active Full VR camera must remain excluded");
    require(!policy::preflight_ready({true, true, true, false, false}),
        "normal Cinema must remain excluded when Full VR is disabled");
    require(!policy::preflight_ready({true, true, true, true, true}),
        "manual forced Cinema must remain excluded");

    std::cout << "native asymmetric transport policy tests passed\n";
    return 0;
}
