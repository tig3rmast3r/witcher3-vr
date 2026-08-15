#include "puredark_afw_camera.h"

#include <cmath>
#include <cstdio>

namespace {

bool nearly_equal(float a, float b, float epsilon = 0.00001f) {
    return std::fabs(a - b) <= epsilon;
}

bool product_is_identity(
    const w3vr::puredark_afw::Matrix4x4& a,
    const w3vr::puredark_afw::Matrix4x4& b) {
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            float value{};
            for (size_t index = 0; index < 4; ++index) {
                value += a.values[row * 4 + index] *
                    b.values[index * 4 + column];
            }
            if (!nearly_equal(value, row == column ? 1.0f : 0.0f)) {
                return false;
            }
        }
    }
    return true;
}

void fill_constants(float* constants, float eye_x) {
    for (size_t matrix = 0; matrix < 2; ++matrix) {
        float* values = constants + matrix * 16;
        values[0] = 1.0f;
        values[5] = 1.0f;
        values[10] = 1.0f;
        values[15] = 1.0f;
    }
    constants[86] = eye_x;
    constants[87] = 1.5f;
    constants[88] = -3.0f;
    constants[89] = 0.0f;
    constants[90] = 1.0f;
    constants[91] = 0.0f;
    constants[92] = 1.0f;
    constants[93] = 0.0f;
    constants[94] = 0.0f;
    constants[95] = 0.0f;
    constants[96] = 0.0f;
    constants[97] = -1.0f;
}

}  // namespace

int main() {
    using namespace w3vr::puredark_afw;
    constexpr float baseline = 0.06533f;
    float constants[102]{};
    CameraData camera{};
    std::wstring error;

    fill_constants(constants, -baseline * 0.5f);
    if (!build_camera_data_from_streamline(
            constants, std::size(constants), EyeLeft,
            baseline, camera, error) ||
        !nearly_equal(
            camera.destination_view_to_world.values[12],
            baseline * 0.5f) ||
        !product_is_identity(
            camera.source_view_to_world,
            camera.source_world_to_view) ||
        !product_is_identity(
            camera.destination_view_to_world,
            camera.destination_world_to_view)) {
        std::fwprintf(stderr, L"left-to-right camera test failed: %ls\n", error.c_str());
        return 1;
    }

    fill_constants(constants, baseline * 0.5f);
    if (!build_camera_data_from_streamline(
            constants, std::size(constants), EyeRight,
            baseline, camera, error) ||
        !nearly_equal(
            camera.destination_view_to_world.values[12],
            -baseline * 0.5f)) {
        std::fwprintf(stderr, L"right-to-left camera test failed: %ls\n", error.c_str());
        return 1;
    }

    // V12076: the missing eye must use its own off-axis projection. Preserve
    // reverse-Z/depth terms from Streamline while changing peer scale/center.
    fill_constants(constants, -baseline * 0.5f);
    constants[0] = 1.2f;
    constants[5] = 1.5f;
    constants[8] = -0.10f;
    constants[9] = 0.03f;
    constants[10] = 0.0f;
    constants[11] = 1.0f;
    constants[14] = 0.1f;
    constants[15] = 0.0f;
    const EyeProjectionGeometry source_projection{
        2.0f, 2.0f, -0.10f, 0.03f};
    const EyeProjectionGeometry destination_projection{
        2.5f, 1.8f, 0.12f, -0.04f};
    if (!build_camera_data_from_streamline(
            constants, std::size(constants), EyeLeft,
            baseline, camera, error) ||
        !retarget_destination_eye_projection(
            source_projection, destination_projection, camera, error) ||
        !nearly_equal(camera.destination_view_to_clip.values[0], 0.96f) ||
        !nearly_equal(
            camera.destination_view_to_clip.values[5], 1.6666667f) ||
        !nearly_equal(camera.destination_view_to_clip.values[8], 0.12f) ||
        !nearly_equal(camera.destination_view_to_clip.values[9], -0.04f) ||
        !nearly_equal(camera.destination_view_to_clip.values[10], 0.0f) ||
        !nearly_equal(camera.destination_view_to_clip.values[11], 1.0f) ||
        !nearly_equal(camera.destination_view_to_clip.values[14], 0.1f) ||
        !product_is_identity(
            camera.destination_view_to_clip,
            camera.destination_clip_to_view)) {
        std::fwprintf(
            stderr, L"destination-eye projection test failed: %ls\n",
            error.c_str());
        return 1;
    }

    // V12081: Mode-3 native DLSS exposes a centered Streamline projection
    // even though its pixels use the per-eye off-axis centers. Apply the
    // source center first, then derive the peer without doubling the offset.
    fill_constants(constants, -baseline * 0.5f);
    constants[8] = 0.0f;
    constants[9] = 0.0f;
    const EyeProjectionGeometry native_left{
        2.2f, 2.4f, 0.24f, 0.19f};
    const EyeProjectionGeometry native_right{
        2.2f, 2.4f, -0.24f, 0.19f};
    if (!build_camera_data_from_streamline(
            constants, std::size(constants), EyeLeft,
            baseline, camera, error) ||
        !apply_source_eye_projection_center(
            native_left, camera, error) ||
        !retarget_destination_eye_projection(
            native_left, native_right, camera, error) ||
        !nearly_equal(camera.source_view_to_clip.values[8], 0.24f) ||
        !nearly_equal(camera.source_view_to_clip.values[9], 0.19f) ||
        !nearly_equal(camera.destination_view_to_clip.values[8], -0.24f) ||
        !nearly_equal(camera.destination_view_to_clip.values[9], 0.19f) ||
        !product_is_identity(
            camera.source_view_to_clip,
            camera.source_clip_to_view) ||
        !product_is_identity(
            camera.destination_view_to_clip,
            camera.destination_clip_to_view)) {
        std::fwprintf(
            stderr, L"absolute asymmetric projection test failed: %ls\n",
            error.c_str());
        return 1;
    }

    constants[92] = NAN;
    if (build_camera_data_from_streamline(
            constants, std::size(constants), EyeRight,
            baseline, camera, error) || error.empty()) {
        std::fputs("non-finite camera rejection failed\n", stderr);
        return 1;
    }
    fill_constants(constants, 0.0f);
    if (build_camera_data_from_streamline(
            constants, std::size(constants), EyeLeft,
            0.0f, camera, error) || error.empty()) {
        std::fputs("invalid baseline rejection failed\n", stderr);
        return 1;
    }

    const ParallelEyePose captured_left{
        {0.0f, 0.0f, 0.0f, 1.0f}, {-0.032665f, 1.0f, 2.0f}};
    const ParallelEyePose captured_right{
        {0.0f, 0.0f, 0.0f, 1.0f}, {0.032665f, 1.0f, 2.0f}};
    const ParallelEyePose exact_left{
        {0.0f, 0.0f, 0.0f, 1.0f}, {4.0f, 5.0f, 6.0f}};
    ParallelEyePose exact_right{};
    if (!rebase_parallel_eye_pose(
            captured_left, captured_right, exact_left,
            exact_right, error) ||
        !nearly_equal(exact_right.position[0], 4.06533f) ||
        !nearly_equal(exact_right.position[1], 5.0f) ||
        !nearly_equal(exact_right.position[2], 6.0f) ||
        !nearly_equal(exact_right.orientation[0],
            exact_left.orientation[0]) ||
        !nearly_equal(exact_right.orientation[3],
            exact_left.orientation[3])) {
        std::fwprintf(
            stderr, L"completed-view pose rebase failed: %ls\n",
            error.c_str());
        return 1;
    }
    const ParallelEyePose rotated_exact_left{
        {0.0f, 0.70710678f, 0.0f, 0.70710678f},
        {4.0f, 5.0f, 6.0f}};
    if (!rebase_parallel_eye_pose(
            captured_left, captured_right, rotated_exact_left,
            exact_right, error) ||
        !nearly_equal(exact_right.position[0], 4.0f) ||
        !nearly_equal(exact_right.position[1], 5.0f) ||
        !nearly_equal(exact_right.position[2], 6.0f - 0.06533f)) {
        std::fwprintf(
            stderr, L"rotated completed-view baseline failed: %ls\n",
            error.c_str());
        return 1;
    }

    std::puts("PureDark AFW camera tests passed");
    return 0;
}
