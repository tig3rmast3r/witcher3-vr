#include <cassert>
#include <cmath>

namespace {

constexpr float kRunConeDegrees = 60.0f;
constexpr float kWalkSpeed = 0.6f;

bool outside_run_cone(float local_heading_degrees) {
    return std::isfinite(local_heading_degrees) &&
        std::fabs(local_heading_degrees) > kRunConeDegrees;
}

float apply_speed_policy(
    float native_speed,
    float local_heading_degrees,
    bool owner_active,
    bool input_active) {
    if (!owner_active || !input_active ||
        !outside_run_cone(local_heading_degrees)) {
        return native_speed;
    }
    return std::fmin(native_speed, kWalkSpeed);
}

bool sprint_allowed(
    bool native_allowed,
    float local_heading_degrees,
    bool owner_active,
    bool input_active) {
    return native_allowed &&
        (!owner_active || !input_active ||
         !outside_run_cone(local_heading_degrees));
}

} // namespace

int main() {
    assert(!outside_run_cone(0.0f));
    assert(!outside_run_cone(59.9f));
    assert(!outside_run_cone(-59.9f));
    assert(!outside_run_cone(60.0f));
    assert(!outside_run_cone(-60.0f));
    assert(outside_run_cone(60.1f));
    assert(outside_run_cone(-60.1f));
    assert(outside_run_cone(90.0f));
    assert(outside_run_cone(-90.0f));
    assert(outside_run_cone(180.0f));

    assert(apply_speed_policy(1.5f, 180.0f, true, true) == kWalkSpeed);
    assert(apply_speed_policy(0.3f, 180.0f, true, true) == 0.3f);
    assert(apply_speed_policy(1.5f, 60.0f, true, true) == 1.5f);
    assert(apply_speed_policy(1.5f, 180.0f, false, true) == 1.5f);
    assert(apply_speed_policy(1.5f, 180.0f, true, false) == 1.5f);

    assert(!sprint_allowed(true, 180.0f, true, true));
    assert(!sprint_allowed(true, -90.0f, true, true));
    assert(sprint_allowed(true, 60.0f, true, true));
    assert(sprint_allowed(true, 180.0f, false, true));
    assert(sprint_allowed(true, 180.0f, true, false));
    assert(!sprint_allowed(false, 0.0f, true, true));
    return 0;
}
