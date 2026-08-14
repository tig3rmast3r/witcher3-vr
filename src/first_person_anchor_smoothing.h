#pragma once

#include <algorithm>
#include <cmath>

namespace w3vr::first_person {

inline constexpr float kAnchorSmoothingMinSeconds = 0.08f;
inline constexpr float kAnchorSmoothingMaxSeconds = 0.20f;

struct AnchorSmoothingState {
    bool valid{};
    float lateral{};
    float vertical{};
};

inline float exponential_smoothing_blend(
    float delta_seconds,
    float smoothing_seconds) {
    if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0f ||
        !std::isfinite(smoothing_seconds) || smoothing_seconds <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(
        1.0f - std::exp(-delta_seconds / smoothing_seconds),
        0.0f,
        1.0f);
}

// Smooth only camera-local X/Y: lateral displacement and world vertical
// displacement relative to the current player root. Camera-local Z (forward
// depth) and the complete root translation remain current-frame values.
inline bool smooth_anchor_lateral_vertical(
    const float* raw_anchor,
    const float* root,
    const float* camera_forward,
    float blend,
    AnchorSmoothingState& state,
    float* output) {
    if (raw_anchor == nullptr || root == nullptr || camera_forward == nullptr ||
        output == nullptr || !std::isfinite(blend)) {
        return false;
    }
    for (int index = 0; index < 3; ++index) {
        if (!std::isfinite(raw_anchor[index]) || !std::isfinite(root[index]) ||
            !std::isfinite(camera_forward[index])) {
            return false;
        }
    }

    const float forward_length = std::sqrt(
        camera_forward[0] * camera_forward[0] +
        camera_forward[1] * camera_forward[1]);
    if (!std::isfinite(forward_length) || forward_length <= 0.001f) {
        return false;
    }
    const float forward_x = camera_forward[0] / forward_length;
    const float forward_y = camera_forward[1] / forward_length;
    const float right_x = forward_y;
    const float right_y = -forward_x;
    const float relative_x = raw_anchor[0] - root[0];
    const float relative_y = raw_anchor[1] - root[1];
    const float raw_depth =
        relative_x * forward_x + relative_y * forward_y;
    const float raw_lateral =
        relative_x * right_x + relative_y * right_y;
    const float raw_vertical = raw_anchor[2] - root[2];
    if (!std::isfinite(raw_depth) || !std::isfinite(raw_lateral) ||
        !std::isfinite(raw_vertical)) {
        return false;
    }

    blend = std::clamp(blend, 0.0f, 1.0f);
    if (!state.valid || !std::isfinite(state.lateral) ||
        !std::isfinite(state.vertical)) {
        state.valid = true;
        state.lateral = raw_lateral;
        state.vertical = raw_vertical;
    } else {
        state.lateral += (raw_lateral - state.lateral) * blend;
        state.vertical += (raw_vertical - state.vertical) * blend;
    }

    output[0] = root[0] + forward_x * raw_depth + right_x * state.lateral;
    output[1] = root[1] + forward_y * raw_depth + right_y * state.lateral;
    output[2] = root[2] + state.vertical;
    return std::isfinite(output[0]) && std::isfinite(output[1]) &&
        std::isfinite(output[2]);
}

}  // namespace w3vr::first_person
