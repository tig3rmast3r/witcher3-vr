#pragma once

#include <cmath>
#include <cstdint>

namespace w3vr::fullscreen_cutscene_framing {

inline bool automatic_transition_recenter_allowed(
    bool fullscreen, bool full_vr, bool manual_cinema) {
    return !(fullscreen && full_vr && !manual_cinema);
}

inline float hud_counter_yaw_degrees(float current_yaw, float entry_yaw) {
    return -(current_yaw - entry_yaw);
}

// Reduce rendered eye separation as zoom amplifies disparity. Zero preserves
// the runtime baseline, half strength uses the FOV gain, full squares it.
// This must not scale cyclopean head translation, eye cant, or HUD geometry.
inline float stereo_zoom_gain(float fov_scale, float intensity) {
    if (!std::isfinite(fov_scale) || !std::isfinite(intensity)) return 1.0f;
    const float scale = fov_scale < 0.05f ? 0.05f :
        fov_scale > 1.0f ? 1.0f : fov_scale;
    const float strength = intensity < 0.0f ? 0.0f :
        intensity > 1.0f ? 1.0f : intensity;
    return std::pow(scale, 2.0f * strength);
}

// Gameplay may lock/recenter game-camera pitch, but a cinematic director
// deliberately aims each shot. Replacing that pitch with the gameplay
// reference moves subjects above/below their framed-shot positions.
inline float scene_pitch_degrees(
    float director_pitch, float gameplay_pitch,
    bool automatic_fullscreen_cutscene) {
    return automatic_fullscreen_cutscene
        ? director_pitch : gameplay_pitch;
}

// Aim upward to lower the shot in the headset. Calibrate at the optical
// center using the actual producer FOV, not the unzoomed headset FOV.
// Rotation is not a uniform image translation away from the optical center.
inline float subject_lowering_pitch_degrees(float render_fov_degrees,
    float screen_height_fraction) {
    constexpr float kRadiansPerDegree = 3.14159265358979323846f / 180.0f;
    if (!std::isfinite(render_fov_degrees) || render_fov_degrees <= 0.0f ||
        render_fov_degrees >= 179.0f ||
        !std::isfinite(screen_height_fraction)) {
        return 0.0f;
    }
    const float tangent_span = 2.0f * std::tan(
        render_fov_degrees * kRadiansPerDegree * 0.5f);
    return std::atan(screen_height_fraction * tangent_span) /
        kRadiansPerDegree;
}

// Translate the rendered shot to the apparent center of the Cinema3D panel
// without moving or rotating the camera. The panel's center stays at y/d
// regardless of its angular size or the cutscene zoom. REDengine's projection
// center field is in pixels and shifts NDC by 2*offset/render_height.
inline float vertical_center_offset_px(
    float panel_local_y, float panel_distance,
    float submitted_fov_up, float submitted_fov_down,
    uint32_t render_height,
    float additional_screen_height_fraction = 0.0f) {
    if (!std::isfinite(panel_local_y) ||
        !std::isfinite(panel_distance) || panel_distance <= 0.0f ||
        !std::isfinite(submitted_fov_up) ||
        !std::isfinite(submitted_fov_down) ||
        !std::isfinite(additional_screen_height_fraction) ||
        render_height == 0 || render_height > 16384) {
        return 0.0f;
    }
    const float span = std::tan(submitted_fov_up) -
        std::tan(submitted_fov_down);
    if (!std::isfinite(span) || span <= 0.01f) {
        return 0.0f;
    }
    return ((panel_local_y / panel_distance) / span +
        additional_screen_height_fraction) *
        static_cast<float>(render_height);
}

// HUD down offset is NDC: 2 NDC units span one visible screen height.
inline float hud_down_ndc_after_screen_raise(
    float down_ndc, float screen_height_fraction) {
    return down_ndc - 2.0f * screen_height_fraction;
}

inline bool translated_projection_tangents(float left, float right,
    float up, float down, float lower_fraction,
    float& result_up, float& result_down) {
    if (!std::isfinite(left) || !std::isfinite(right) ||
        !std::isfinite(up) || !std::isfinite(down) ||
        !std::isfinite(lower_fraction) || right <= left || up <= down) {
        return false;
    }
    const float offset = -lower_fraction * (up - down);
    result_up = up + offset;
    result_down = down + offset;
    return std::isfinite(result_up) && std::isfinite(result_down);
}

} // namespace w3vr::fullscreen_cutscene_framing
