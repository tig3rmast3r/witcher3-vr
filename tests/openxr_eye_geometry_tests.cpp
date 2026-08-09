#include "openxr_eye_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace eye_geometry = w3vr::openxr_eye_geometry;

namespace {

constexpr float kPi = 3.14159265358979323846f;
int failures{};

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

float quaternion_dot(
    const XrQuaternionf& left, const XrQuaternionf& right) {
    return left.x * right.x + left.y * right.y +
        left.z * right.z + left.w * right.w;
}

bool quaternion_near(
    const XrQuaternionf& left,
    const XrQuaternionf& right,
    float max_angle_radians = 1.0e-4f) {
    XrQuaternionf normalized_left{};
    XrQuaternionf normalized_right{};
    if (!eye_geometry::normalize(left, normalized_left) ||
        !eye_geometry::normalize(right, normalized_right)) {
        return false;
    }
    auto difference = eye_geometry::multiply(
        eye_geometry::conjugate(normalized_left), normalized_right);
    if (difference.w < 0.0f) {
        difference = {
            -difference.x, -difference.y, -difference.z, -difference.w};
    }
    const float vector_length = std::sqrt(
        difference.x * difference.x + difference.y * difference.y +
        difference.z * difference.z);
    return 2.0f * std::atan2(vector_length, difference.w) <=
        max_angle_radians;
}

bool vector_near(
    const XrVector3f& left,
    const XrVector3f& right,
    float tolerance = 2.0e-6f) {
    return std::fabs(left.x - right.x) <= tolerance &&
        std::fabs(left.y - right.y) <= tolerance &&
        std::fabs(left.z - right.z) <= tolerance;
}

XrQuaternionf yaw(float degrees) {
    return eye_geometry::from_hmd_euler_degrees(0.0f, degrees, 0.0f);
}

std::array<XrView, 2> make_views(
    const XrQuaternionf& left_orientation,
    const XrQuaternionf& right_orientation,
    const XrVector3f& center,
    const XrQuaternionf& head_orientation,
    const XrVector3f& left_local,
    const XrVector3f& right_local) {
    std::array<XrView, 2> views{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
    views[0].pose.orientation = left_orientation;
    views[1].pose.orientation = right_orientation;
    const auto left_world = eye_geometry::rotate(head_orientation, left_local);
    const auto right_world = eye_geometry::rotate(head_orientation, right_local);
    views[0].pose.position = {
        center.x + left_world.x,
        center.y + left_world.y,
        center.z + left_world.z};
    views[1].pose.position = {
        center.x + right_world.x,
        center.y + right_world.y,
        center.z + right_world.z};
    return views;
}

void require_reconstruction(
    const std::array<XrView, 2>& views,
    const eye_geometry::EyeGeometry& geometry,
    const char* label) {
    for (size_t eye = 0; eye < 2; ++eye) {
        const auto reconstructed_orientation = eye_geometry::multiply(
            geometry.cyclopean_orientation,
            geometry.relative_orientations[eye]);
        require(quaternion_near(
            reconstructed_orientation, views[eye].pose.orientation), label);
        const auto relative_world = eye_geometry::rotate(
            geometry.cyclopean_orientation,
            geometry.relative_positions[eye]);
        const XrVector3f reconstructed_position{
            geometry.cyclopean_position.x + relative_world.x,
            geometry.cyclopean_position.y + relative_world.y,
            geometry.cyclopean_position.z + relative_world.z};
        require(vector_near(
            reconstructed_position, views[eye].pose.position), label);
    }
}

void test_parallel() {
    const auto head = eye_geometry::from_hmd_euler_degrees(12.0f, -21.0f, 3.0f);
    const XrVector3f center{0.2f, 1.6f, -0.4f};
    const auto views = make_views(
        head, head, center, head,
        {-0.032f, 0.0f, 0.0f}, {0.032f, 0.0f, 0.0f});
    eye_geometry::EyeGeometry geometry{};
    require(eye_geometry::compute(views, geometry), "parallel compute");
    require(quaternion_near(
        geometry.cyclopean_orientation, head), "parallel midpoint");
    require(quaternion_near(
        geometry.relative_orientations[0], {0, 0, 0, 1}),
        "parallel left identity");
    require(quaternion_near(
        geometry.relative_orientations[1], {0, 0, 0, 1}),
        "parallel right identity");
    require(vector_near(
        geometry.relative_positions[0], {-0.032f, 0, 0}),
        "parallel left position");
    require(vector_near(
        geometry.relative_positions[1], {0.032f, 0, 0}),
        "parallel right position");
    require(std::fabs(geometry.cant_degrees) < 1.0e-4f,
        "parallel zero cant");
    require_reconstruction(views, geometry, "parallel reconstruction");
}

void test_symmetric_cant_and_antipodal() {
    const auto head = eye_geometry::from_hmd_euler_degrees(23.0f, -37.0f, 14.0f);
    const XrVector3f center{0.31f, 1.42f, -0.77f};
    auto left = eye_geometry::multiply(head, yaw(5.0f));
    auto right = eye_geometry::multiply(head, yaw(-5.0f));
    auto views = make_views(
        left, right, center, head,
        {-0.032f, 0.001f, -0.002f},
        {0.032f, -0.001f, 0.002f});
    eye_geometry::EyeGeometry geometry{};
    require(eye_geometry::compute(views, geometry), "symmetric cant compute");
    require(quaternion_near(
        geometry.cyclopean_orientation, head), "symmetric cant midpoint");
    require(std::fabs(geometry.cant_degrees - 10.0f) < 0.001f,
        "symmetric total cant");
    require(quaternion_near(
        geometry.relative_orientations[0], yaw(5.0f)),
        "symmetric left relative");
    require(quaternion_near(
        geometry.relative_orientations[1], yaw(-5.0f)),
        "symmetric right relative");
    require_reconstruction(views, geometry, "symmetric reconstruction");

    views[1].pose.orientation = {
        -views[1].pose.orientation.x,
        -views[1].pose.orientation.y,
        -views[1].pose.orientation.z,
        -views[1].pose.orientation.w};
    eye_geometry::EyeGeometry antipodal{};
    require(eye_geometry::compute(views, antipodal), "antipodal compute");
    require(quaternion_near(
        antipodal.cyclopean_orientation,
        geometry.cyclopean_orientation), "antipodal midpoint");
    require_reconstruction(views, antipodal, "antipodal reconstruction");
}

void test_asymmetric_cant() {
    const auto head = eye_geometry::from_hmd_euler_degrees(-8.0f, 31.0f, -4.0f);
    const auto left = eye_geometry::multiply(head, yaw(7.0f));
    const auto right = eye_geometry::multiply(head, yaw(-3.0f));
    const XrVector3f center{-0.4f, 1.3f, 0.2f};
    const auto views = make_views(
        left, right, center, head,
        {-0.033f, 0.0015f, -0.001f},
        {0.033f, -0.0015f, 0.001f});
    eye_geometry::EyeGeometry geometry{};
    require(eye_geometry::compute(views, geometry), "asymmetric compute");
    const auto expected_midpoint = eye_geometry::multiply(head, yaw(2.0f));
    require(quaternion_near(
        geometry.cyclopean_orientation, expected_midpoint),
        "asymmetric midpoint");
    require(quaternion_near(
        geometry.relative_orientations[0], yaw(5.0f)),
        "asymmetric left relative");
    require(quaternion_near(
        geometry.relative_orientations[1], yaw(-5.0f)),
        "asymmetric right relative");
    require_reconstruction(views, geometry, "asymmetric reconstruction");
}

void test_normalization_and_invalid_input() {
    std::array<XrView, 2> views{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
    views[0].pose.orientation = {0.0f, 0.0f, 0.0f, 2.0f};
    views[1].pose.orientation = {0.0f, 0.0f, 0.0f, -3.0f};
    views[0].pose.position = {-0.032f, 0.0f, 0.0f};
    views[1].pose.position = {0.032f, 0.0f, 0.0f};
    eye_geometry::EyeGeometry geometry{};
    require(eye_geometry::compute(views, geometry), "non-unit compute");
    require(quaternion_near(
        geometry.cyclopean_orientation, {0, 0, 0, 1}),
        "non-unit normalization");

    views[0].pose.orientation = {};
    require(!eye_geometry::compute(views, geometry), "zero quaternion rejected");
    views[0].pose.orientation = {0, 0, 0, 1};
    views[1].pose.position.x = std::numeric_limits<float>::quiet_NaN();
    require(!eye_geometry::compute(views, geometry), "NaN position rejected");
}

void test_euler_roundtrip_and_wrap() {
    const auto source = eye_geometry::from_hmd_euler_degrees(23.0f, -37.0f, 14.0f);
    const auto euler = eye_geometry::to_hmd_euler_degrees(source);
    const auto reconstructed = eye_geometry::from_hmd_euler_degrees(
        euler.pitch, euler.yaw, euler.roll);
    require(quaternion_near(source, reconstructed), "Euler roundtrip");
    require(std::fabs(
        eye_geometry::nearest_equivalent_degrees(-179.0f, 181.0f) - 181.0f) <
        1.0e-5f, "nearest angle positive wrap");
    require(std::fabs(
        eye_geometry::nearest_equivalent_degrees(179.0f, -181.0f) + 181.0f) <
        1.0e-5f, "nearest angle negative wrap");
}

void test_redengine_descriptor_composition() {
    // Captured V1017 REDengine descriptor/basis sample. This anchors the
    // decompiled Rz(view[6]) * Rx(view[5]) * Ry(view[4]) convention to an
    // actual rebuilt camera rather than testing only our own round-trip.
    const auto captured =
        eye_geometry::from_redengine_view_euler_degrees(
            0.0f, -9.5f, -40.3269f);
    require(vector_near(
        eye_geometry::rotate(captured, {1.0f, 0.0f, 0.0f}),
        {0.7624f, -0.6471f, 0.0f}, 2.0e-4f),
        "captured REDengine right basis");
    require(vector_near(
        eye_geometry::rotate(captured, {0.0f, 1.0f, 0.0f}),
        {0.6383f, 0.7519f, -0.1650f}, 2.0e-4f),
        "captured REDengine forward basis");
    require(vector_near(
        eye_geometry::rotate(captured, {0.0f, 0.0f, 1.0f}),
        {0.1068f, 0.1258f, 0.9863f}, 2.0e-4f),
        "captured REDengine up basis");

    const auto base =
        eye_geometry::from_redengine_view_euler_degrees(
            -11.0f, 27.0f, 43.0f);
    const auto base_euler =
        eye_geometry::to_redengine_view_euler_degrees(base);
    require(std::fabs(base_euler.roll + 11.0f) < 1.0e-4f,
        "REDengine roll roundtrip");
    require(std::fabs(base_euler.pitch - 27.0f) < 1.0e-4f,
        "REDengine pitch roundtrip");
    require(std::fabs(base_euler.yaw - 43.0f) < 1.0e-4f,
        "REDengine yaw roundtrip");

    const auto xr_pitch = eye_geometry::openxr_to_redengine_local_orientation(
        eye_geometry::from_hmd_euler_degrees(7.0f, 0.0f, 0.0f));
    const auto xr_yaw = eye_geometry::openxr_to_redengine_local_orientation(
        eye_geometry::from_hmd_euler_degrees(0.0f, 7.0f, 0.0f));
    const auto xr_roll = eye_geometry::openxr_to_redengine_local_orientation(
        eye_geometry::from_hmd_euler_degrees(0.0f, 0.0f, 7.0f));
    const auto pitch_euler =
        eye_geometry::to_redengine_view_euler_degrees(xr_pitch);
    const auto yaw_euler =
        eye_geometry::to_redengine_view_euler_degrees(xr_yaw);
    const auto roll_euler =
        eye_geometry::to_redengine_view_euler_degrees(xr_roll);
    require(std::fabs(pitch_euler.pitch - 7.0f) < 1.0e-4f,
        "OpenXR pitch maps to REDengine view[5]");
    require(std::fabs(yaw_euler.yaw - 7.0f) < 1.0e-4f,
        "OpenXR yaw maps to REDengine view[6]");
    require(std::fabs(roll_euler.roll + 7.0f) < 1.0e-4f,
        "OpenXR roll maps with REDengine opposite sign");

    const auto relative_openxr =
        eye_geometry::from_hmd_euler_degrees(3.0f, 5.0f, 2.0f);
    const auto relative_engine =
        eye_geometry::openxr_to_redengine_local_orientation(
            relative_openxr);
    const auto final_orientation =
        eye_geometry::multiply(base, relative_engine);
    const auto final_euler =
        eye_geometry::to_redengine_view_euler_degrees(final_orientation);
    const auto reconstructed =
        eye_geometry::from_redengine_view_euler_degrees(
            final_euler.roll, final_euler.pitch, final_euler.yaw);
    require(quaternion_near(final_orientation, reconstructed),
        "REDengine local eye-pose composition");

    const auto near_gimbal =
        eye_geometry::from_redengine_view_euler_degrees(
            12.0f, 89.0f, -28.0f);
    const auto near_gimbal_euler =
        eye_geometry::to_redengine_view_euler_degrees(near_gimbal);
    const auto near_gimbal_reconstructed =
        eye_geometry::from_redengine_view_euler_degrees(
            near_gimbal_euler.roll,
            near_gimbal_euler.pitch,
            near_gimbal_euler.yaw);
    require(quaternion_near(
        near_gimbal, near_gimbal_reconstructed, 2.0e-4f),
        "REDengine near-gimbal roundtrip");
}

float projected_tangent_x(
    const std::array<float, 16>& clip_positions,
    uint32_t corner,
    const XrFovf& target_fov) {
    const float ndc_x = clip_positions[corner * 4 + 0] /
        clip_positions[corner * 4 + 3];
    const float left = std::tan(target_fov.angleLeft);
    const float right = std::tan(target_fov.angleRight);
    return left + (ndc_x + 1.0f) * 0.5f * (right - left);
}

void test_quest_reference_hud_plane() {
    constexpr float baseline = 0.065330f;
    constexpr float width = 3072.0f;
    constexpr float horizontal_span = 2.75276502f;
    constexpr float source_half_horizontal = 0.942478f;
    constexpr float source_half_vertical = 0.959931f;
    const XrFovf source_fov{
        -source_half_horizontal, source_half_horizontal,
        source_half_vertical, -source_half_vertical};
    const auto views = make_views(
        {0, 0, 0, 1}, {0, 0, 0, 1},
        {0, 0, 0}, {0, 0, 0, 1},
        {-baseline * 0.5f, 0, 0},
        {baseline * 0.5f, 0, 0});
    eye_geometry::EyeGeometry geometry{};
    require(eye_geometry::compute(views, geometry),
        "Quest HUD reference geometry");

    const float inverse_distance =
        eye_geometry::inverse_hud_distance_from_parallel_reference(
            -36.0f, 1.0f, baseline, width, horizontal_span);
    require(std::fabs(inverse_distance - 0.98756973f) < 2.0e-6f,
        "Quest HUD reference inverse distance");
    require(std::fabs(1.0f / inverse_distance - 1.0125867f) < 2.0e-5f,
        "Quest HUD reference physical distance");

    for (uint32_t eye = 0; eye < 2; ++eye) {
        std::array<float, 16> clip_positions{};
        require(eye_geometry::build_cyclopean_hud_plane_clip_positions(
            geometry, eye, source_fov, source_fov, 1.0f,
            inverse_distance, clip_positions),
            "Quest HUD reference clip build");
        const float source_left = std::tan(source_fov.angleLeft);
        const float projected_left = projected_tangent_x(
            clip_positions, 0, source_fov);
        const float output_shift_px =
            (projected_left - source_left) * width / horizontal_span;
        const float expected_output_shift = eye == 0 ? 36.0f : -36.0f;
        require(std::fabs(output_shift_px - expected_output_shift) < 0.002f,
            "Quest HUD plane preserves legacy per-eye offset");
    }

    // Full VR text defaults directly to gameplay-HUD depth at size 1.0.
    // Changing size must vary shift inversely to retain that physical depth.
    const float full_vr_size = 1.0f;
    const float full_vr_inverse_distance =
        eye_geometry::inverse_hud_distance_from_parallel_reference(
            -36.0f, full_vr_size, baseline, width, horizontal_span);
    require(std::fabs(full_vr_inverse_distance - inverse_distance) < 2.0e-6f,
        "Quest Full VR HUD matches gameplay depth");
    std::array<float, 16> full_vr_clip{};
    require(eye_geometry::build_cyclopean_hud_plane_clip_positions(
        geometry, 0, source_fov, source_fov, full_vr_size,
        full_vr_inverse_distance, full_vr_clip),
        "Quest Full VR HUD clip build");
    const float scaled_source_left =
        -full_vr_size * std::tan(source_half_horizontal);
    const float full_vr_output_shift =
        (projected_tangent_x(full_vr_clip, 0, source_fov) -
            scaled_source_left) * width / horizontal_span;
    require(std::fabs(full_vr_output_shift - 36.0f) < 0.01f,
        "Quest Full VR HUD preserves gameplay-depth offset");
    const float large_full_vr_inverse_distance =
        eye_geometry::inverse_hud_distance_from_parallel_reference(
            -24.0f, 1.50f, baseline, width, horizontal_span);
    require(std::fabs(large_full_vr_inverse_distance - inverse_distance) <
            2.0e-6f,
        "Quest Full VR HUD size is independent from physical depth");
}

void test_canted_hud_plane_and_invalid_fallback() {
    constexpr float pimax_baseline = 0.068047f;
    const auto left = yaw(10.0f);
    const auto right = yaw(-10.0f);
    const auto views = make_views(
        left, right, {0, 0, 0}, {0, 0, 0, 1},
        {-pimax_baseline * 0.5f, 0, 0},
        {pimax_baseline * 0.5f, 0, 0});
    eye_geometry::EyeGeometry geometry{};
    require(eye_geometry::compute(views, geometry),
        "Pimax HUD geometry");
    constexpr float source_half_horizontal = 0.816143f;
    constexpr float source_half_vertical = 0.786983f;
    const XrFovf source_fov{
        -source_half_horizontal, source_half_horizontal,
        source_half_vertical, -source_half_vertical};
    const XrFovf raw_fovs[2]{
        {-0.900349f, 0.931882f, 0.903724f, -0.903724f},
        {-0.931882f, 0.900349f, 0.903724f, -0.903724f}};
    const float inverse_distance =
        eye_geometry::inverse_hud_distance_from_parallel_reference(
            -36.0f, 1.0f, 0.065330f, 3072.0f, 2.75276502f);
    for (uint32_t eye = 0; eye < 2; ++eye) {
        std::array<float, 16> clip_positions{};
        require(eye_geometry::build_cyclopean_hud_plane_clip_positions(
            geometry, eye, source_fov, raw_fovs[eye], 1.0f,
            inverse_distance, clip_positions),
            "Pimax canted HUD clip build");
        for (uint32_t corner = 0; corner < 4; ++corner) {
            require(std::isfinite(clip_positions[corner * 4 + 0]) &&
                std::isfinite(clip_positions[corner * 4 + 1]) &&
                clip_positions[corner * 4 + 3] > 0.0f,
                "Pimax canted HUD finite forward corner");
            const float source_edge = corner == 0 || corner == 2
                ? -std::tan(source_half_horizontal)
                : std::tan(source_half_horizontal);
            const auto& origin = geometry.relative_positions[eye];
            const XrVector3f corner_to_eye{
                source_edge - inverse_distance * origin.x,
                -inverse_distance * origin.y,
                -1.0f - inverse_distance * origin.z};
            const auto corner_eye_space = eye_geometry::rotate(
                eye_geometry::conjugate(
                    geometry.relative_orientations[eye]),
                corner_to_eye);
            const float expected_corner_tangent =
                corner_eye_space.x / -corner_eye_space.z;
            require(std::fabs(
                projected_tangent_x(
                    clip_positions, corner, raw_fovs[eye]) -
                expected_corner_tangent) < 2.0e-5f,
                "Pimax raw-FOV clip preserves physical corner tangent");
        }

        // Project the plane center directly. Expressed in the symmetric source
        // pixel scale, the current Pimax capture should need about 215 pixels
        // per eye, not the old universal 36-pixel shift.
        const auto& origin = geometry.relative_positions[eye];
        const XrVector3f center_to_eye{
            -inverse_distance * origin.x,
            -inverse_distance * origin.y,
            -1.0f - inverse_distance * origin.z};
        const auto eye_space = eye_geometry::rotate(
            eye_geometry::conjugate(
                geometry.relative_orientations[eye]),
            center_to_eye);
        const float center_tangent = eye_space.x / -eye_space.z;
        const float equivalent_output_px = center_tangent * 2160.0f /
            (2.0f * std::tan(source_half_horizontal));
        const float expected = eye == 0 ? 214.7f : -214.7f;
        require(std::fabs(equivalent_output_px - expected) < 0.8f,
            "Pimax canted HUD automatic center compensation");
    }

    std::array<float, 16> invalid_clip{};
    XrFovf invalid_fov = source_fov;
    invalid_fov.angleRight = invalid_fov.angleLeft;
    require(!eye_geometry::build_cyclopean_hud_plane_clip_positions(
        geometry, 0, source_fov, invalid_fov, 1.0f,
        inverse_distance, invalid_clip),
        "HUD plane rejects invalid target FOV");
    require(std::isnan(
        eye_geometry::inverse_hud_distance_from_parallel_reference(
            -36.0f, 1.0f, 0.0f, 3072.0f, 2.75276502f)),
        "HUD plane rejects invalid reference baseline");
}

void test_parallel_headset_adaptation() {
    constexpr float baseline = 0.068047f;
    constexpr float width = 2160.0f;
    constexpr float half_horizontal = 0.816143f;
    const XrFovf fov{
        -half_horizontal, half_horizontal, 0.786983f, -0.786983f};
    const auto views = make_views(
        {0, 0, 0, 1}, {0, 0, 0, 1},
        {0, 0, 0}, {0, 0, 0, 1},
        {-baseline * 0.5f, 0, 0},
        {baseline * 0.5f, 0, 0});
    eye_geometry::EyeGeometry geometry{};
    require(eye_geometry::compute(views, geometry),
        "alternate parallel headset geometry");
    const float inverse_distance =
        eye_geometry::inverse_hud_distance_from_parallel_reference(
            -36.0f, 1.0f, 0.065330f, 3072.0f, 2.75276502f);
    const float span = 2.0f * std::tan(half_horizontal);
    for (uint32_t eye = 0; eye < 2; ++eye) {
        std::array<float, 16> clip_positions{};
        require(eye_geometry::build_cyclopean_hud_plane_clip_positions(
            geometry, eye, fov, fov, 1.0f,
            inverse_distance, clip_positions),
            "alternate parallel headset HUD clip build");
        const float projected_left = projected_tangent_x(
            clip_positions, 0, fov);
        const float output_shift_px =
            (projected_left + std::tan(half_horizontal)) * width / span;
        const float expected = eye == 0 ? 34.123f : -34.123f;
        require(std::fabs(output_shift_px - expected) < 0.01f,
            "parallel headset adapts Quest depth to baseline/FOV/resolution");
    }
}

void test_asymmetric_projection_descriptor() {
    constexpr uint32_t width = 3072;
    constexpr uint32_t height = 3216;
    const XrFovf quest_left{
        -0.942478f, 0.698132f, 0.767945f, -0.959931f};
    const XrFovf quest_right{
        -0.698132f, 0.942478f, 0.767945f, -0.959931f};
    eye_geometry::AsymmetricProjectionDescriptor left{};
    eye_geometry::AsymmetricProjectionDescriptor right{};
    require(eye_geometry::derive_asymmetric_projection_descriptor(
        quest_left, width, height, left),
        "Quest left asymmetric descriptor");
    require(eye_geometry::derive_asymmetric_projection_descriptor(
        quest_right, width, height, right),
        "Quest right asymmetric descriptor");
    require(std::fabs(left.optical_center_offset_px_x - 372.50f) < 0.2f,
        "Quest left optical center X");
    require(std::fabs(right.optical_center_offset_px_x + 372.50f) < 0.2f,
        "Quest right optical center X");
    require(std::fabs(left.optical_center_offset_px_y - 310.65f) < 0.2f &&
        std::fabs(right.optical_center_offset_px_y - 310.65f) < 0.2f,
        "Quest optical center Y");
    require(std::fabs(left.redengine_center_offset_px_x - 372.50f) < 0.2f &&
        std::fabs(right.redengine_center_offset_px_x + 372.50f) < 0.2f,
        "Quest REDengine native center X convention");
    require(std::fabs(left.redengine_center_offset_px_y - 310.65f) < 0.2f &&
        std::fabs(right.redengine_center_offset_px_y - 310.65f) < 0.2f,
        "Quest REDengine native center Y convention");
    require(std::fabs(left.center_ndc_x + right.center_ndc_x) < 2.0e-6f,
        "Quest mirrored horizontal NDC centers");
    require(std::fabs(left.center_ndc_y - right.center_ndc_y) < 2.0e-6f,
        "Quest shared vertical NDC center");
    require(std::fabs(left.aspect - right.aspect) < 2.0e-6f,
        "Quest mirrored raw aspect");
    require(std::fabs(left.vertical_fov_degrees - 100.2439f) < 0.002f &&
        std::fabs(right.vertical_fov_degrees - 100.2439f) < 0.002f,
        "Quest lossless vertical FOV");
    require(std::fabs(left.aspect - 0.92549446f) < 2.0e-6f &&
        std::fabs(right.aspect - 0.92549446f) < 2.0e-6f,
        "Quest lossless tangent aspect");
    require(std::fabs(left.horizontal_tangent_span - 2.21548265f) <
            3.0e-6f &&
        std::fabs(right.horizontal_tangent_span - 2.21548265f) <
            3.0e-6f &&
        std::fabs(left.vertical_tangent_span - 2.39383676f) <
            3.0e-6f &&
        std::fabs(right.vertical_tangent_span - 2.39383676f) <
            3.0e-6f,
        "Quest lossless tangent spans");
    const auto require_fov_roundtrip = [=](
            const XrFovf& source,
            const eye_geometry::AsymmetricProjectionDescriptor& descriptor,
            const char* label) {
        constexpr float pi = 3.14159265358979323846f;
        const float reconstructed_vertical_span = 2.0f * std::tan(
            descriptor.vertical_fov_degrees * pi / 360.0f);
        const float reconstructed_horizontal_span =
            descriptor.aspect * reconstructed_vertical_span;
        const float sum_x =
            -descriptor.center_ndc_x * reconstructed_horizontal_span;
        const float reconstructed_left =
            (sum_x - reconstructed_horizontal_span) * 0.5f;
        const float reconstructed_right =
            (sum_x + reconstructed_horizontal_span) * 0.5f;
        const float sum_y =
            -descriptor.center_ndc_y * reconstructed_vertical_span;
        const float reconstructed_down =
            (sum_y - reconstructed_vertical_span) * 0.5f;
        const float reconstructed_up =
            (sum_y + reconstructed_vertical_span) * 0.5f;
        require(std::fabs(
            descriptor.horizontal_tangent_span -
                (std::tan(source.angleRight) -
                    std::tan(source.angleLeft))) < 2.0e-6f,
            label);
        require(std::fabs(
            descriptor.vertical_tangent_span -
                (std::tan(source.angleUp) -
                    std::tan(source.angleDown))) < 2.0e-6f,
            label);
        require(std::fabs(reconstructed_left -
                std::tan(source.angleLeft)) < 2.0e-6f &&
            std::fabs(reconstructed_right -
                std::tan(source.angleRight)) < 2.0e-6f &&
            std::fabs(reconstructed_up -
                std::tan(source.angleUp)) < 2.0e-6f &&
            std::fabs(reconstructed_down -
                std::tan(source.angleDown)) < 2.0e-6f,
            label);
    };
    require_fov_roundtrip(quest_left, left,
        "Quest left tangent roundtrip");
    require_fov_roundtrip(quest_right, right,
        "Quest right tangent roundtrip");

    const XrFovf symmetric{-0.8f, 0.8f, 0.9f, -0.9f};
    eye_geometry::AsymmetricProjectionDescriptor centered{};
    require(eye_geometry::derive_asymmetric_projection_descriptor(
        symmetric, 2160, 2104, centered),
        "symmetric descriptor");
    require(std::fabs(centered.optical_center_offset_px_x) < 1.0e-5f &&
        std::fabs(centered.optical_center_offset_px_y) < 1.0e-5f &&
        std::fabs(centered.redengine_center_offset_px_x) < 1.0e-5f &&
        std::fabs(centered.redengine_center_offset_px_y) < 1.0e-5f,
        "symmetric descriptor has zero optical offset");

    XrFovf invalid = symmetric;
    invalid.angleRight = invalid.angleLeft;
    require(!eye_geometry::derive_asymmetric_projection_descriptor(
        invalid, width, height, centered),
        "asymmetric descriptor rejects collapsed FOV");
    require(!eye_geometry::derive_asymmetric_projection_descriptor(
        symmetric, 0, height, centered),
        "asymmetric descriptor rejects zero extent");
}

} // namespace

int main() {
    test_parallel();
    test_symmetric_cant_and_antipodal();
    test_asymmetric_cant();
    test_normalization_and_invalid_input();
    test_euler_roundtrip_and_wrap();
    test_redengine_descriptor_composition();
    test_quest_reference_hud_plane();
    test_canted_hud_plane_and_invalid_fallback();
    test_parallel_headset_adaptation();
    test_asymmetric_projection_descriptor();
    if (failures != 0) {
        std::fprintf(stderr, "%d eye-geometry test(s) failed\n", failures);
        return 1;
    }
    std::puts("OpenXR eye-geometry tests passed");
    return 0;
}
