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
    require(!policy::preflight_ready({false, false, false, true, false}),
        "invalid transport must fail closed");
    require(policy::preflight_ready({true, false, false, true, false}),
        "gameplay keeps the established preflight route");
    require(policy::preflight_ready({true, true, true, true, false}),
        "automatic Full VR may bootstrap asymmetric transport");
    require(!policy::preflight_ready({true, true, false, true, false}),
        "Cinema without active Full VR camera must remain excluded");
    require(!policy::preflight_ready({true, true, true, false, false}),
        "normal Cinema must remain excluded when Full VR is disabled");
    require(!policy::preflight_ready({true, true, true, true, true}),
        "manual forced Cinema must remain excluded");

    require(policy::cinema_pair_admissible(
            {false, false, 0x0u, 0x0u, 0x0u, true}),
        "normal Cinema or the symmetric preflight pair needs no ASYM proof");
    require(!policy::cinema_pair_admissible(
            {true, true, 0x2u, 0x3u, 0x3u, true}),
        "a half-built Full VR factory pair must be held");
    require(!policy::cinema_pair_admissible(
            {true, true, 0x3u, 0x2u, 0x3u, true}),
        "a half-built Full VR temporal pair must be held");
    require(!policy::cinema_pair_admissible(
            {true, true, 0x3u, 0x3u, 0x1u, true}),
        "DLSS Full VR must require both exact input eyes");
    require(policy::cinema_pair_admissible(
            {true, true, 0x3u, 0x3u, 0x3u, true}),
        "a complete DLSS Full VR pair must be admitted");
    require(policy::cinema_pair_admissible(
            {true, true, 0x3u, 0x3u, 0x0u, false}),
        "non-DLSS Full VR must not require a DLSS input proof");
    require(!policy::presentation_uses_native_asymmetric(false, true, false),
        "a symmetric sequential pair must retain symmetric presentation");
    require(policy::presentation_uses_native_asymmetric(false, true, true),
        "an immutable sequential ASYM pair must select ASYM presentation");
    require(!policy::presentation_uses_native_asymmetric(false, false, true),
        "stale sequential metadata must not select ASYM presentation");
    require(policy::presentation_uses_native_asymmetric(true, false, false),
        "the established packed ASYM route must remain unchanged");

    std::cout << "native asymmetric transport policy tests passed\n";
    return 0;
}
