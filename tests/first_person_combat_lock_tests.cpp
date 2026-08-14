#include "first_person_combat_lock.h"

#include <cmath>
#include <cstdio>

namespace {

bool near(float actual, float expected, float epsilon = 0.001f) {
    if (std::fabs(actual - expected) <= epsilon) {
        return true;
    }
    std::fprintf(stderr, "actual %.6f expected %.6f\n", actual, expected);
    return false;
}

}  // namespace

int main() {
    using w3vr::first_person::CombatLockAngles;
    using w3vr::first_person::approach_degrees;
    using w3vr::first_person::combat_lock_angles;

    const float source[3]{0.0f, 0.0f, 1.6f};
    CombatLockAngles angles{};

    const float forward[3]{0.0f, 10.0f, 1.6f};
    if (!combat_lock_angles(source, forward, angles) ||
        !near(angles.yaw, 0.0f) || !near(angles.pitch, 0.0f)) {
        return 1;
    }

    const float right[3]{10.0f, 0.0f, 1.6f};
    if (!combat_lock_angles(source, right, angles) ||
        !near(angles.yaw, -90.0f)) {
        return 2;
    }

    const float up[3]{0.0f, 10.0f, 11.6f};
    if (!combat_lock_angles(source, up, angles) ||
        !near(angles.pitch, 45.0f)) {
        return 3;
    }

    if (!near(approach_degrees(0.0f, 90.0f, 0.1f, 4.0f), 36.0f)) {
        return 4;
    }
    if (!near(approach_degrees(179.0f, -179.0f, 0.25f, 2.0f), -180.0f)) {
        return 5;
    }
    return 0;
}
