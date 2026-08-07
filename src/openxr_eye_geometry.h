#pragma once

#include <array>
#include <cstdint>

#include <openxr/openxr.h>

namespace w3vr::openxr_eye_geometry {

struct EyeGeometry {
    XrQuaternionf cyclopean_orientation{0.0f, 0.0f, 0.0f, 1.0f};
    XrVector3f cyclopean_position{};
    std::array<XrQuaternionf, 2> eye_orientations{{
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}}};
    std::array<XrQuaternionf, 2> relative_orientations{{
        {0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}}};
    std::array<XrVector3f, 2> relative_positions{};
    float baseline_m{};
    float cant_degrees{};
};

struct HmdEulerDegrees {
    float pitch{};
    float yaw{};
    float roll{};
};

struct RedEngineViewEulerDegrees {
    float roll{};
    float pitch{};
    float yaw{};
};

XrQuaternionf multiply(
    const XrQuaternionf& left, const XrQuaternionf& right);
XrQuaternionf conjugate(const XrQuaternionf& quaternion);
bool normalize(
    const XrQuaternionf& quaternion, XrQuaternionf& normalized);
XrVector3f rotate(
    const XrQuaternionf& rotation, const XrVector3f& vector);
XrQuaternionf from_hmd_euler_degrees(
    float pitch, float yaw, float roll);
HmdEulerDegrees to_hmd_euler_degrees(
    const XrQuaternionf& quaternion);
float nearest_equivalent_degrees(float angle, float reference);
XrQuaternionf openxr_to_redengine_local_orientation(
    const XrQuaternionf& orientation);
XrQuaternionf from_redengine_view_euler_degrees(
    float roll, float pitch, float yaw);
RedEngineViewEulerDegrees to_redengine_view_euler_degrees(
    const XrQuaternionf& orientation);
bool compute(const std::array<XrView, 2>& views, EyeGeometry& geometry);

// Converts a legacy left-eye source-pixel shift, captured on a parallel-view
// reference headset, into the inverse physical distance of a cyclopean HUD
// plane. A negative left-eye shift produces a positive distance in front of
// the viewer. Including hud_size preserves the legacy shader's exact shift
// semantics while making the resulting plane portable across eye geometry.
float inverse_hud_distance_from_parallel_reference(
    float left_eye_shift_px,
    float hud_size,
    float reference_baseline_m,
    float reference_source_width_px,
    float reference_horizontal_tangent_span);

// Builds four perspective-correct clip-space corners for a cyclopean HUD
// plane. source_fov describes the symmetric projection that generated the HUD
// texture; target_fov is the exact raw FOV submitted for this eye. The output
// order is top-left, top-right, bottom-left, bottom-right.
bool build_cyclopean_hud_plane_clip_positions(
    const EyeGeometry& geometry,
    uint32_t eye,
    const XrFovf& source_fov,
    const XrFovf& target_fov,
    float hud_size,
    float inverse_distance_m,
    std::array<float, 16>& clip_positions);

} // namespace w3vr::openxr_eye_geometry
