#include "config.h"

#include <cassert>

int main() {
    using w3vr::CameraFollowEnabled;
    using w3vr::CameraFollowPolicy;

    assert(CameraFollowEnabled(
        CameraFollowPolicy::AlwaysOn, false, false));
    assert(!CameraFollowEnabled(
        CameraFollowPolicy::AlwaysOn, false, true));
    assert(CameraFollowEnabled(
        CameraFollowPolicy::AlwaysOn, true, false));
    assert(CameraFollowEnabled(
        CameraFollowPolicy::AlwaysOn, true, true));

    assert(!CameraFollowEnabled(
        CameraFollowPolicy::HorseBoatOnly, false, false));
    assert(!CameraFollowEnabled(
        CameraFollowPolicy::HorseBoatOnly, false, true));
    assert(CameraFollowEnabled(
        CameraFollowPolicy::HorseBoatOnly, true, false));
    assert(CameraFollowEnabled(
        CameraFollowPolicy::HorseBoatOnly, true, true));

    assert(!CameraFollowEnabled(
        CameraFollowPolicy::AlwaysOff, false, false));
    assert(!CameraFollowEnabled(
        CameraFollowPolicy::AlwaysOff, false, true));
    assert(!CameraFollowEnabled(
        CameraFollowPolicy::AlwaysOff, true, false));
    assert(!CameraFollowEnabled(
        CameraFollowPolicy::AlwaysOff, true, true));
    return 0;
}
