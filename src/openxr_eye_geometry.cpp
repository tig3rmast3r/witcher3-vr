#include "openxr_eye_geometry.h"

#include <algorithm>
#include <cmath>

namespace w3vr::openxr_eye_geometry {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float dot(const XrQuaternionf& left, const XrQuaternionf& right) {
    return left.x * right.x + left.y * right.y +
        left.z * right.z + left.w * right.w;
}

bool finite(const XrVector3f& vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) &&
        std::isfinite(vector.z);
}

XrQuaternionf negated(const XrQuaternionf& quaternion) {
    return {-quaternion.x, -quaternion.y, -quaternion.z, -quaternion.w};
}

} // namespace

XrQuaternionf multiply(
    const XrQuaternionf& left, const XrQuaternionf& right) {
    return {
        left.w * right.x + left.x * right.w +
            left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z +
            left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y -
            left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x -
            left.y * right.y - left.z * right.z};
}

XrQuaternionf conjugate(const XrQuaternionf& quaternion) {
    return {-quaternion.x, -quaternion.y, -quaternion.z, quaternion.w};
}

bool normalize(
    const XrQuaternionf& quaternion, XrQuaternionf& normalized) {
    const float norm_squared = dot(quaternion, quaternion);
    if (!std::isfinite(norm_squared) || norm_squared <= 1.0e-12f) {
        return false;
    }
    const float inverse_norm = 1.0f / std::sqrt(norm_squared);
    normalized = {
        quaternion.x * inverse_norm,
        quaternion.y * inverse_norm,
        quaternion.z * inverse_norm,
        quaternion.w * inverse_norm};
    return std::isfinite(normalized.x) && std::isfinite(normalized.y) &&
        std::isfinite(normalized.z) && std::isfinite(normalized.w);
}

XrVector3f rotate(
    const XrQuaternionf& rotation, const XrVector3f& vector) {
    const XrQuaternionf pure{vector.x, vector.y, vector.z, 0.0f};
    const auto rotated = multiply(
        multiply(rotation, pure), conjugate(rotation));
    return {rotated.x, rotated.y, rotated.z};
}

XrQuaternionf from_hmd_euler_degrees(
    float pitch, float yaw, float roll) {
    constexpr float kDegreesToHalfRadians = kPi / 360.0f;
    const float half_pitch = pitch * kDegreesToHalfRadians;
    const float half_yaw = yaw * kDegreesToHalfRadians;
    const float half_roll = roll * kDegreesToHalfRadians;
    const XrQuaternionf pitch_rotation{
        std::sin(half_pitch), 0.0f, 0.0f, std::cos(half_pitch)};
    const XrQuaternionf yaw_rotation{
        0.0f, std::sin(half_yaw), 0.0f, std::cos(half_yaw)};
    const XrQuaternionf roll_rotation{
        0.0f, 0.0f, std::sin(half_roll), std::cos(half_roll)};
    return multiply(multiply(yaw_rotation, pitch_rotation), roll_rotation);
}

HmdEulerDegrees to_hmd_euler_degrees(
    const XrQuaternionf& quaternion) {
    const float pitch_sine = std::clamp(
        2.0f * (quaternion.w * quaternion.x -
            quaternion.z * quaternion.y),
        -1.0f, 1.0f);
    const float yaw_sine = 2.0f *
        (quaternion.w * quaternion.y + quaternion.x * quaternion.z);
    const float yaw_cosine = 1.0f - 2.0f *
        (quaternion.x * quaternion.x + quaternion.y * quaternion.y);
    const float roll_sine = 2.0f *
        (quaternion.w * quaternion.z + quaternion.x * quaternion.y);
    const float roll_cosine = 1.0f - 2.0f *
        (quaternion.x * quaternion.x + quaternion.z * quaternion.z);
    constexpr float kRadiansToDegrees = 180.0f / kPi;
    return {
        std::asin(pitch_sine) * kRadiansToDegrees,
        std::atan2(yaw_sine, yaw_cosine) * kRadiansToDegrees,
        std::atan2(roll_sine, roll_cosine) * kRadiansToDegrees};
}

float nearest_equivalent_degrees(float angle, float reference) {
    if (!std::isfinite(angle) || !std::isfinite(reference)) {
        return angle;
    }
    return angle + 360.0f * std::round((reference - angle) / 360.0f);
}

XrQuaternionf openxr_to_redengine_local_orientation(
    const XrQuaternionf& orientation) {
    // OpenXR local +X/+Y/+Z maps to REDengine camera-local +X/+Z/-Y:
    // right/up/back becomes right/up/opposite-forward. This is the proper
    // +90-degree X-axis basis change, including REDengine's opposite roll
    // sign (the shipped additive route uses hmd_roll_scale=-1 for the same
    // reason).
    return {
        orientation.x,
        -orientation.z,
        orientation.y,
        orientation.w};
}

XrQuaternionf from_redengine_view_euler_degrees(
    float roll, float pitch, float yaw) {
    constexpr float kDegreesToHalfRadians = kPi / 360.0f;
    const float half_roll = roll * kDegreesToHalfRadians;
    const float half_pitch = pitch * kDegreesToHalfRadians;
    const float half_yaw = yaw * kDegreesToHalfRadians;
    const XrQuaternionf roll_rotation{
        0.0f, std::sin(half_roll), 0.0f, std::cos(half_roll)};
    const XrQuaternionf pitch_rotation{
        std::sin(half_pitch), 0.0f, 0.0f, std::cos(half_pitch)};
    const XrQuaternionf yaw_rotation{
        0.0f, 0.0f, std::sin(half_yaw), std::cos(half_yaw)};
    // FUN_1402c9700 builds the row-vector descriptor matrix as
    // Ry(view[4]) * Rx(view[5]) * Rz(view[6]). Its equivalent column-vector
    // orientation is the transpose: Rz(view[6]) * Rx(view[5]) * Ry(view[4]).
    return multiply(
        multiply(yaw_rotation, pitch_rotation), roll_rotation);
}

RedEngineViewEulerDegrees to_redengine_view_euler_degrees(
    const XrQuaternionf& orientation) {
    XrQuaternionf normalized{};
    if (!normalize(orientation, normalized)) {
        return {NAN, NAN, NAN};
    }
    // Inverse of Rz(yaw) * Rx(pitch) * Ry(roll), the column-vector
    // representation of REDengine's row-vector rebuild above.
    const float pitch_sine = std::clamp(
        2.0f * (normalized.w * normalized.x +
            normalized.y * normalized.z),
        -1.0f, 1.0f);
    const float roll_sine = 2.0f *
        (normalized.w * normalized.y -
            normalized.x * normalized.z);
    const float roll_cosine = 1.0f - 2.0f *
        (normalized.x * normalized.x +
            normalized.y * normalized.y);
    const float yaw_sine = 2.0f *
        (normalized.w * normalized.z -
            normalized.x * normalized.y);
    const float yaw_cosine = 1.0f - 2.0f *
        (normalized.x * normalized.x +
            normalized.z * normalized.z);
    constexpr float kRadiansToDegrees = 180.0f / kPi;
    return {
        std::atan2(roll_sine, roll_cosine) * kRadiansToDegrees,
        std::asin(pitch_sine) * kRadiansToDegrees,
        std::atan2(yaw_sine, yaw_cosine) * kRadiansToDegrees};
}

bool compute(const std::array<XrView, 2>& views, EyeGeometry& geometry) {
    EyeGeometry candidate{};
    if (!finite(views[0].pose.position) ||
        !finite(views[1].pose.position) ||
        !normalize(
            views[0].pose.orientation, candidate.eye_orientations[0]) ||
        !normalize(
            views[1].pose.orientation, candidate.eye_orientations[1])) {
        return false;
    }

    float eye_dot = dot(
        candidate.eye_orientations[0], candidate.eye_orientations[1]);
    if (eye_dot < 0.0f) {
        candidate.eye_orientations[1] =
            negated(candidate.eye_orientations[1]);
        eye_dot = -eye_dot;
    }
    eye_dot = std::clamp(eye_dot, 0.0f, 1.0f);

    const XrQuaternionf midpoint_sum{
        candidate.eye_orientations[0].x +
            candidate.eye_orientations[1].x,
        candidate.eye_orientations[0].y +
            candidate.eye_orientations[1].y,
        candidate.eye_orientations[0].z +
            candidate.eye_orientations[1].z,
        candidate.eye_orientations[0].w +
            candidate.eye_orientations[1].w};
    if (!normalize(midpoint_sum, candidate.cyclopean_orientation)) {
        return false;
    }

    candidate.cyclopean_position = {
        (views[0].pose.position.x + views[1].pose.position.x) * 0.5f,
        (views[0].pose.position.y + views[1].pose.position.y) * 0.5f,
        (views[0].pose.position.z + views[1].pose.position.z) * 0.5f};
    const auto inverse_cyclopean =
        conjugate(candidate.cyclopean_orientation);
    for (size_t eye = 0; eye < candidate.eye_orientations.size(); ++eye) {
        if (!normalize(
                multiply(inverse_cyclopean,
                    candidate.eye_orientations[eye]),
                candidate.relative_orientations[eye])) {
            return false;
        }
        const XrVector3f relative_world{
            views[eye].pose.position.x - candidate.cyclopean_position.x,
            views[eye].pose.position.y - candidate.cyclopean_position.y,
            views[eye].pose.position.z - candidate.cyclopean_position.z};
        candidate.relative_positions[eye] =
            rotate(inverse_cyclopean, relative_world);
        if (!finite(candidate.relative_positions[eye])) {
            return false;
        }
    }

    const float dx = views[1].pose.position.x - views[0].pose.position.x;
    const float dy = views[1].pose.position.y - views[0].pose.position.y;
    const float dz = views[1].pose.position.z - views[0].pose.position.z;
    candidate.baseline_m = std::sqrt(dx * dx + dy * dy + dz * dz);
    candidate.cant_degrees =
        2.0f * std::acos(eye_dot) * (180.0f / kPi);
    if (!std::isfinite(candidate.baseline_m) ||
        !std::isfinite(candidate.cant_degrees)) {
        return false;
    }

    geometry = candidate;
    return true;
}

bool derive_asymmetric_projection_descriptor(
    const XrFovf& fov,
    uint32_t render_width,
    uint32_t render_height,
    AsymmetricProjectionDescriptor& descriptor) {
    if (render_width == 0 || render_height == 0 ||
        !std::isfinite(fov.angleLeft) ||
        !std::isfinite(fov.angleRight) ||
        !std::isfinite(fov.angleUp) ||
        !std::isfinite(fov.angleDown)) {
        return false;
    }

    const float left = std::tan(fov.angleLeft);
    const float right = std::tan(fov.angleRight);
    const float up = std::tan(fov.angleUp);
    const float down = std::tan(fov.angleDown);
    const float horizontal_span = right - left;
    const float vertical_span = up - down;
    if (!std::isfinite(left) || !std::isfinite(right) ||
        !std::isfinite(up) || !std::isfinite(down) ||
        !std::isfinite(horizontal_span) ||
        !std::isfinite(vertical_span) ||
        horizontal_span <= 0.01f || vertical_span <= 0.01f) {
        return false;
    }

    AsymmetricProjectionDescriptor candidate{};
    candidate.horizontal_tangent_span = horizontal_span;
    candidate.vertical_tangent_span = vertical_span;
    candidate.vertical_fov_degrees =
        2.0f * std::atan(vertical_span * 0.5f) * (180.0f / kPi);
    candidate.aspect = horizontal_span / vertical_span;
    candidate.center_ndc_x = -(right + left) / horizontal_span;
    candidate.center_ndc_y = -(up + down) / vertical_span;
    candidate.optical_center_offset_px_x = candidate.center_ndc_x *
        static_cast<float>(render_width) * 0.5f;
    candidate.optical_center_offset_px_y = candidate.center_ndc_y *
        static_cast<float>(render_height) * 0.5f;
    // FUN_1415FE550 post-multiplies the REDengine row-vector projection by a
    // translation of +2*offset/extent, so its X field has the same sign as the
    // OpenXR optical center in NDC. V1043 used the opposite sign here.
    candidate.redengine_center_offset_px_x =
        candidate.optical_center_offset_px_x;
    candidate.redengine_center_offset_px_y =
        candidate.optical_center_offset_px_y;
    if (!std::isfinite(candidate.vertical_fov_degrees) ||
        !std::isfinite(candidate.aspect) ||
        !std::isfinite(candidate.center_ndc_x) ||
        !std::isfinite(candidate.center_ndc_y) ||
        !std::isfinite(candidate.optical_center_offset_px_x) ||
        !std::isfinite(candidate.optical_center_offset_px_y) ||
        !std::isfinite(candidate.redengine_center_offset_px_x) ||
        !std::isfinite(candidate.redengine_center_offset_px_y) ||
        candidate.vertical_fov_degrees <= 0.0f ||
        candidate.aspect <= 0.0f) {
        return false;
    }

    descriptor = candidate;
    return true;
}

bool derive_asymmetric_hud_source_shift(
    const AsymmetricProjectionDescriptor& descriptor,
    float base_hud_size,
    float asymmetric_hud_size,
    int legacy_horizontal_shift_px,
    int& source_shift_x_px,
    int& source_shift_y_px) {
    if (!std::isfinite(descriptor.optical_center_offset_px_x) ||
        !std::isfinite(descriptor.optical_center_offset_px_y) ||
        !std::isfinite(base_hud_size) || base_hud_size <= 0.01f ||
        !std::isfinite(asymmetric_hud_size) ||
        asymmetric_hud_size <= 0.01f) {
        return false;
    }

    const float source_x =
        (static_cast<float>(legacy_horizontal_shift_px) * base_hud_size -
            descriptor.optical_center_offset_px_x) /
        asymmetric_hud_size;
    const float source_y = descriptor.optical_center_offset_px_y /
        asymmetric_hud_size;
    if (!std::isfinite(source_x) || !std::isfinite(source_y)) {
        return false;
    }
    source_shift_x_px = std::clamp(
        static_cast<int>(std::lround(source_x)), -16384, 16384);
    source_shift_y_px = std::clamp(
        static_cast<int>(std::lround(source_y)), -16384, 16384);
    return true;
}

bool scale_asymmetric_projection_fov(
    const XrFovf& fov,
    float scale,
    XrFovf& scaled_fov) {
    if (!std::isfinite(scale) || scale <= 0.0f ||
        !std::isfinite(fov.angleLeft) ||
        !std::isfinite(fov.angleRight) ||
        !std::isfinite(fov.angleUp) ||
        !std::isfinite(fov.angleDown)) {
        return false;
    }

    const float left = std::tan(fov.angleLeft);
    const float right = std::tan(fov.angleRight);
    const float up = std::tan(fov.angleUp);
    const float down = std::tan(fov.angleDown);
    if (!std::isfinite(left) || !std::isfinite(right) ||
        !std::isfinite(up) || !std::isfinite(down) ||
        right - left <= 0.01f || up - down <= 0.01f) {
        return false;
    }

    // Preserve the exact runtime values at 1.0 so the established Native
    // Stereo route remains byte-identical at its historical setting.
    if (scale == 1.0f) {
        scaled_fov = fov;
        return true;
    }

    const float scaled_left = left * scale;
    const float scaled_right = right * scale;
    const float scaled_up = up * scale;
    const float scaled_down = down * scale;
    if (!std::isfinite(scaled_left) || !std::isfinite(scaled_right) ||
        !std::isfinite(scaled_up) || !std::isfinite(scaled_down) ||
        scaled_right - scaled_left <= 0.01f ||
        scaled_up - scaled_down <= 0.01f) {
        return false;
    }

    scaled_fov = {
        std::atan(scaled_left),
        std::atan(scaled_right),
        std::atan(scaled_up),
        std::atan(scaled_down)};
    return std::isfinite(scaled_fov.angleLeft) &&
        std::isfinite(scaled_fov.angleRight) &&
        std::isfinite(scaled_fov.angleUp) &&
        std::isfinite(scaled_fov.angleDown);
}

float inverse_hud_distance_from_parallel_reference(
    float left_eye_shift_px,
    float hud_size,
    float reference_baseline_m,
    float reference_source_width_px,
    float reference_horizontal_tangent_span) {
    if (!std::isfinite(left_eye_shift_px) || !std::isfinite(hud_size) ||
        !std::isfinite(reference_baseline_m) ||
        !std::isfinite(reference_source_width_px) ||
        !std::isfinite(reference_horizontal_tangent_span) ||
        hud_size <= 0.0f || reference_baseline_m <= 0.0f ||
        reference_source_width_px <= 0.0f ||
        reference_horizontal_tangent_span <= 0.0f) {
        return NAN;
    }
    return -2.0f * left_eye_shift_px * hud_size *
        reference_horizontal_tangent_span /
        (reference_baseline_m * reference_source_width_px);
}

bool build_cyclopean_hud_plane_clip_positions(
    const EyeGeometry& geometry,
    uint32_t eye,
    const XrFovf& source_fov,
    const XrFovf& target_fov,
    float hud_size,
    float inverse_distance_m,
    std::array<float, 16>& clip_positions) {
    if (eye >= geometry.relative_orientations.size() ||
        !std::isfinite(hud_size) || hud_size <= 0.0f ||
        !std::isfinite(inverse_distance_m) ||
        !finite(geometry.relative_positions[eye])) {
        return false;
    }
    XrQuaternionf relative_orientation{};
    if (!normalize(
            geometry.relative_orientations[eye], relative_orientation)) {
        return false;
    }

    const float source_left = std::tan(source_fov.angleLeft);
    const float source_right = std::tan(source_fov.angleRight);
    const float source_up = std::tan(source_fov.angleUp);
    const float source_down = std::tan(source_fov.angleDown);
    const float target_left = std::tan(target_fov.angleLeft);
    const float target_right = std::tan(target_fov.angleRight);
    const float target_up = std::tan(target_fov.angleUp);
    const float target_down = std::tan(target_fov.angleDown);
    const float target_horizontal_span = target_right - target_left;
    const float target_vertical_span = target_up - target_down;
    if (!std::isfinite(source_left) || !std::isfinite(source_right) ||
        !std::isfinite(source_up) || !std::isfinite(source_down) ||
        !std::isfinite(target_left) || !std::isfinite(target_right) ||
        !std::isfinite(target_up) || !std::isfinite(target_down) ||
        source_right <= source_left || source_up <= source_down ||
        target_horizontal_span <= 0.01f ||
        target_vertical_span <= 0.01f) {
        return false;
    }

    const float source_center_x = (source_left + source_right) * 0.5f;
    const float source_center_y = (source_up + source_down) * 0.5f;
    const float source_corner_x[4]{
        source_left, source_right, source_left, source_right};
    const float source_corner_y[4]{
        source_up, source_up, source_down, source_down};
    const auto inverse_relative_orientation =
        conjugate(relative_orientation);
    const auto& eye_origin = geometry.relative_positions[eye];
    for (uint32_t corner = 0; corner < 4; ++corner) {
        const float plane_tangent_x = source_center_x + hud_size *
            (source_corner_x[corner] - source_center_x);
        const float plane_tangent_y = source_center_y + hud_size *
            (source_corner_y[corner] - source_center_y);
        // Divide the physical eye-to-plane vector by the plane distance. This
        // remains well-defined for inverse_distance_m == 0 (infinite plane),
        // which is the natural zero-convergence canted-view case.
        const XrVector3f cyclopean_eye_to_plane{
            plane_tangent_x - inverse_distance_m * eye_origin.x,
            plane_tangent_y - inverse_distance_m * eye_origin.y,
            -1.0f - inverse_distance_m * eye_origin.z};
        const auto eye_space = rotate(
            inverse_relative_orientation, cyclopean_eye_to_plane);
        const float forward = -eye_space.z;
        if (!finite(eye_space) || !std::isfinite(forward) ||
            forward <= 1.0e-4f) {
            return false;
        }
        const float tangent_x = eye_space.x / forward;
        const float tangent_y = eye_space.y / forward;
        const float ndc_x =
            2.0f * (tangent_x - target_left) /
                target_horizontal_span - 1.0f;
        const float ndc_y =
            2.0f * (tangent_y - target_down) /
                target_vertical_span - 1.0f;
        if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y)) {
            return false;
        }
        clip_positions[corner * 4 + 0] = ndc_x * forward;
        clip_positions[corner * 4 + 1] = ndc_y * forward;
        clip_positions[corner * 4 + 2] = 0.5f * forward;
        clip_positions[corner * 4 + 3] = forward;
    }
    return true;
}

} // namespace w3vr::openxr_eye_geometry
