#pragma once

#include <algorithm>
#include <cmath>

namespace w3vr::first_person {

struct CombatLockAngles {
    float pitch{};
    float yaw{};
};

inline float normalize_degrees(float angle) {
    angle = std::fmod(angle + 180.0f, 360.0f);
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    return angle - 180.0f;
}

inline float shortest_degrees(float target, float current) {
    return normalize_degrees(target - current);
}

inline bool combat_lock_angles(
    const float* source,
    const float* target,
    CombatLockAngles& angles) {
    if (source == nullptr || target == nullptr) {
        return false;
    }
    const float x = target[0] - source[0];
    const float y = target[1] - source[1];
    const float z = target[2] - source[2];
    const float horizontal = std::sqrt(x * x + y * y);
    if (!std::isfinite(horizontal) || horizontal < 0.001f ||
        !std::isfinite(z)) {
        return false;
    }
    constexpr float kRadiansToDegrees =
        57.295779513082320876798154814105f;
    angles.yaw = normalize_degrees(
        std::atan2(-x, y) * kRadiansToDegrees);
    angles.pitch = std::clamp(
        std::atan2(z, horizontal) * kRadiansToDegrees,
        -89.0f,
        89.0f);
    return std::isfinite(angles.pitch) && std::isfinite(angles.yaw);
}

inline float approach_degrees(
    float current,
    float target,
    float delta_seconds,
    float speed) {
    if (!std::isfinite(current) || !std::isfinite(target) ||
        !std::isfinite(delta_seconds) || !std::isfinite(speed)) {
        return current;
    }
    const float blend = std::clamp(delta_seconds * speed, 0.0f, 1.0f);
    return normalize_degrees(
        current + shortest_degrees(target, current) * blend);
}

}  // namespace w3vr::first_person
