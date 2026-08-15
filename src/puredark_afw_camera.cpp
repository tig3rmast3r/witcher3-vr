#include "puredark_afw_camera.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace w3vr::puredark_afw {
namespace {

constexpr size_t kRequiredStreamlineFloatCount = 98;

void set_identity(Matrix4x4& matrix) {
    matrix = {};
    matrix.values[0] = 1.0f;
    matrix.values[5] = 1.0f;
    matrix.values[10] = 1.0f;
    matrix.values[15] = 1.0f;
}

bool finite_values(const float* values, size_t count) {
    if (values == nullptr) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        if (!std::isfinite(values[index])) {
            return false;
        }
    }
    return true;
}

float dot3(const float* a, const float* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

bool plausible_basis(const float* right, const float* up, const float* fwd) {
    const float right_length = std::sqrt(dot3(right, right));
    const float up_length = std::sqrt(dot3(up, up));
    const float fwd_length = std::sqrt(dot3(fwd, fwd));
    if (right_length < 0.5f || right_length > 1.5f ||
        up_length < 0.5f || up_length > 1.5f ||
        fwd_length < 0.5f || fwd_length > 1.5f) {
        return false;
    }
    const float right_up = std::fabs(dot3(right, up) /
        (right_length * up_length));
    const float right_fwd = std::fabs(dot3(right, fwd) /
        (right_length * fwd_length));
    const float up_fwd = std::fabs(dot3(up, fwd) /
        (up_length * fwd_length));
    return right_up < 0.25f && right_fwd < 0.25f && up_fwd < 0.25f;
}

bool invert_4x4(const Matrix4x4& source, Matrix4x4& inverse) {
    float augmented[4][8]{};
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            augmented[row][column] = source.values[row * 4 + column];
        }
        augmented[row][4 + row] = 1.0f;
    }

    for (size_t pivot_column = 0; pivot_column < 4; ++pivot_column) {
        size_t pivot_row = pivot_column;
        for (size_t row = pivot_column + 1; row < 4; ++row) {
            if (std::fabs(augmented[row][pivot_column]) >
                std::fabs(augmented[pivot_row][pivot_column])) {
                pivot_row = row;
            }
        }
        if (std::fabs(augmented[pivot_row][pivot_column]) < 0.000001f) {
            return false;
        }
        if (pivot_row != pivot_column) {
            for (size_t column = 0; column < 8; ++column) {
                std::swap(
                    augmented[pivot_row][column],
                    augmented[pivot_column][column]);
            }
        }

        const float pivot = augmented[pivot_column][pivot_column];
        for (float& value : augmented[pivot_column]) {
            value /= pivot;
        }
        for (size_t row = 0; row < 4; ++row) {
            if (row == pivot_column) {
                continue;
            }
            const float scale = augmented[row][pivot_column];
            for (size_t column = 0; column < 8; ++column) {
                augmented[row][column] -=
                    scale * augmented[pivot_column][column];
            }
        }
    }

    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            inverse.values[row * 4 + column] =
                augmented[row][4 + column];
        }
    }
    return true;
}

void build_view_to_world(
    const float* right,
    const float* up,
    const float* fwd,
    const float* position,
    Matrix4x4& matrix) {
    matrix.values[0] = right[0];
    matrix.values[1] = right[1];
    matrix.values[2] = right[2];
    matrix.values[3] = 0.0f;
    matrix.values[4] = up[0];
    matrix.values[5] = up[1];
    matrix.values[6] = up[2];
    matrix.values[7] = 0.0f;
    matrix.values[8] = fwd[0];
    matrix.values[9] = fwd[1];
    matrix.values[10] = fwd[2];
    matrix.values[11] = 0.0f;
    matrix.values[12] = position[0];
    matrix.values[13] = position[1];
    matrix.values[14] = position[2];
    matrix.values[15] = 1.0f;
}

}  // namespace

bool build_camera_data_from_streamline(
    const float* constants,
    size_t float_count,
    uint32_t source_eye,
    float eye_baseline_m,
    CameraData& camera_data,
    std::wstring& error) {
    camera_data = {};
    if (constants == nullptr || float_count < kRequiredStreamlineFloatCount) {
        error = L"Streamline constants snapshot is incomplete";
        return false;
    }
    if (source_eye > 1) {
        error = L"PureDark camera source eye is invalid";
        return false;
    }
    if (!std::isfinite(eye_baseline_m) || eye_baseline_m < 0.01f ||
        eye_baseline_m > 0.20f) {
        error = L"OpenXR eye baseline is outside the supported range";
        return false;
    }
    if (!finite_values(constants, 32) ||
        !finite_values(constants + 86, 12)) {
        error = L"Streamline camera matrices or vectors are not finite";
        return false;
    }

    const float* position = constants + 86;
    const float* up = constants + 89;
    const float* right = constants + 92;
    const float* fwd = constants + 95;
    if (!plausible_basis(right, up, fwd)) {
        error = L"Streamline camera basis is not orthonormal";
        return false;
    }

    std::memcpy(
        camera_data.source_view_to_clip.values,
        constants, sizeof(camera_data.source_view_to_clip.values));
    std::memcpy(
        camera_data.source_clip_to_view.values,
        constants + 16, sizeof(camera_data.source_clip_to_view.values));
    camera_data.destination_view_to_clip =
        camera_data.source_view_to_clip;
    camera_data.destination_clip_to_view =
        camera_data.source_clip_to_view;

    build_view_to_world(
        right, up, fwd, position,
        camera_data.source_view_to_world);
    if (!invert_4x4(
            camera_data.source_view_to_world,
            camera_data.source_world_to_view)) {
        error = L"Streamline source view transform is singular";
        return false;
    }

    const float direction = source_eye == EyeLeft ? 1.0f : -1.0f;
    const float destination_position[3]{
        position[0] + direction * right[0] * eye_baseline_m,
        position[1] + direction * right[1] * eye_baseline_m,
        position[2] + direction * right[2] * eye_baseline_m};
    build_view_to_world(
        right, up, fwd, destination_position,
        camera_data.destination_view_to_world);
    if (!invert_4x4(
            camera_data.destination_view_to_world,
            camera_data.destination_world_to_view)) {
        error = L"PureDark destination view transform is singular";
        return false;
    }

    set_identity(camera_data.camera_world_to_view);
    set_identity(camera_data.camera_view_to_world);
    set_identity(camera_data.camera_view_to_clip);
    set_identity(camera_data.camera_clip_to_view);
    error.clear();
    return true;
}

bool apply_source_eye_projection_center(
    const EyeProjectionGeometry& source_geometry,
    CameraData& camera_data,
    std::wstring& error) {
    if (!std::isfinite(source_geometry.horizontal_tangent_span) ||
        !std::isfinite(source_geometry.vertical_tangent_span) ||
        !std::isfinite(source_geometry.center_ndc_x) ||
        !std::isfinite(source_geometry.center_ndc_y) ||
        source_geometry.horizontal_tangent_span <= 0.01f ||
        source_geometry.vertical_tangent_span <= 0.01f ||
        std::fabs(source_geometry.center_ndc_x) > 1.0f ||
        std::fabs(source_geometry.center_ndc_y) > 1.0f ||
        !finite_values(camera_data.source_view_to_clip.values, 16)) {
        error = L"PureDark source-eye projection center is invalid";
        return false;
    }

    auto source = camera_data.source_view_to_clip;
    source.values[8] += source_geometry.center_ndc_x;
    source.values[9] += source_geometry.center_ndc_y;
    Matrix4x4 source_inverse{};
    if (!finite_values(source.values, 16) ||
        !invert_4x4(source, source_inverse)) {
        error = L"PureDark source-eye projection is singular";
        return false;
    }
    camera_data.source_view_to_clip = source;
    camera_data.source_clip_to_view = source_inverse;
    error.clear();
    return true;
}

bool retarget_destination_eye_projection(
    const EyeProjectionGeometry& source_geometry,
    const EyeProjectionGeometry& destination_geometry,
    CameraData& camera_data,
    std::wstring& error) {
    const auto valid_geometry = [](const EyeProjectionGeometry& geometry) {
        return std::isfinite(geometry.horizontal_tangent_span) &&
            std::isfinite(geometry.vertical_tangent_span) &&
            std::isfinite(geometry.center_ndc_x) &&
            std::isfinite(geometry.center_ndc_y) &&
            geometry.horizontal_tangent_span > 0.01f &&
            geometry.vertical_tangent_span > 0.01f &&
            std::fabs(geometry.center_ndc_x) <= 1.0f &&
            std::fabs(geometry.center_ndc_y) <= 1.0f;
    };
    if (!valid_geometry(source_geometry) ||
        !valid_geometry(destination_geometry) ||
        !finite_values(camera_data.source_view_to_clip.values, 16)) {
        error = L"PureDark source or destination eye projection is invalid";
        return false;
    }

    auto destination = camera_data.source_view_to_clip;
    destination.values[0] *=
        source_geometry.horizontal_tangent_span /
        destination_geometry.horizontal_tangent_span;
    destination.values[5] *=
        source_geometry.vertical_tangent_span /
        destination_geometry.vertical_tangent_span;
    destination.values[8] +=
        destination_geometry.center_ndc_x - source_geometry.center_ndc_x;
    destination.values[9] +=
        destination_geometry.center_ndc_y - source_geometry.center_ndc_y;
    if (!finite_values(destination.values, 16)) {
        error = L"PureDark destination projection contains non-finite values";
        return false;
    }

    Matrix4x4 destination_inverse{};
    if (!invert_4x4(destination, destination_inverse)) {
        error = L"PureDark destination projection is singular";
        return false;
    }
    camera_data.destination_view_to_clip = destination;
    camera_data.destination_clip_to_view = destination_inverse;
    error.clear();
    return true;
}

bool rebase_parallel_eye_pose(
    const ParallelEyePose& captured_source,
    const ParallelEyePose& captured_destination,
    const ParallelEyePose& exact_source,
    ParallelEyePose& exact_destination,
    std::wstring& error) {
    if (!finite_values(captured_source.orientation, 4) ||
        !finite_values(captured_source.position, 3) ||
        !finite_values(captured_destination.orientation, 4) ||
        !finite_values(captured_destination.position, 3) ||
        !finite_values(exact_source.orientation, 4) ||
        !finite_values(exact_source.position, 3)) {
        error = L"OpenXR AFW pose contains non-finite values";
        return false;
    }
    const float captured_quaternion_length = std::sqrt(
        captured_source.orientation[0] * captured_source.orientation[0] +
        captured_source.orientation[1] * captured_source.orientation[1] +
        captured_source.orientation[2] * captured_source.orientation[2] +
        captured_source.orientation[3] * captured_source.orientation[3]);
    const float exact_quaternion_length = std::sqrt(
        exact_source.orientation[0] * exact_source.orientation[0] +
        exact_source.orientation[1] * exact_source.orientation[1] +
        exact_source.orientation[2] * exact_source.orientation[2] +
        exact_source.orientation[3] * exact_source.orientation[3]);
    if (captured_quaternion_length < 0.9f ||
        captured_quaternion_length > 1.1f ||
        exact_quaternion_length < 0.9f ||
        exact_quaternion_length > 1.1f) {
        error = L"Captured or completed OpenXR source orientation is invalid";
        return false;
    }

    exact_destination = exact_source;
    const float captured_baseline[3]{
        captured_destination.position[0] - captured_source.position[0],
        captured_destination.position[1] - captured_source.position[1],
        captured_destination.position[2] - captured_source.position[2]};
    const float baseline = std::sqrt(
        captured_baseline[0] * captured_baseline[0] +
        captured_baseline[1] * captured_baseline[1] +
        captured_baseline[2] * captured_baseline[2]);
    if (baseline < 0.01f || baseline > 0.20f) {
        error = L"Captured OpenXR inter-eye translation is invalid";
        return false;
    }

    const auto normalize_quaternion = [](const float* q, float length) {
        return std::array<float, 4>{
            q[0] / length, q[1] / length,
            q[2] / length, q[3] / length};
    };
    const auto multiply_quaternion = [](
            const std::array<float, 4>& a,
            const std::array<float, 4>& b) {
        return std::array<float, 4>{
            a[3] * b[0] + a[0] * b[3] +
                a[1] * b[2] - a[2] * b[1],
            a[3] * b[1] - a[0] * b[2] +
                a[1] * b[3] + a[2] * b[0],
            a[3] * b[2] + a[0] * b[1] -
                a[1] * b[0] + a[2] * b[3],
            a[3] * b[3] - a[0] * b[0] -
                a[1] * b[1] - a[2] * b[2]};
    };
    const auto captured_orientation = normalize_quaternion(
        captured_source.orientation, captured_quaternion_length);
    const auto exact_orientation = normalize_quaternion(
        exact_source.orientation, exact_quaternion_length);
    const std::array<float, 4> captured_inverse{
        -captured_orientation[0], -captured_orientation[1],
        -captured_orientation[2], captured_orientation[3]};
    const auto delta_orientation = multiply_quaternion(
        exact_orientation, captured_inverse);
    const std::array<float, 4> baseline_quaternion{
        captured_baseline[0], captured_baseline[1],
        captured_baseline[2], 0.0f};
    const std::array<float, 4> delta_inverse{
        -delta_orientation[0], -delta_orientation[1],
        -delta_orientation[2], delta_orientation[3]};
    const auto rotated_baseline = multiply_quaternion(
        multiply_quaternion(delta_orientation, baseline_quaternion),
        delta_inverse);
    for (size_t axis = 0; axis < 3; ++axis) {
        exact_destination.position[axis] += rotated_baseline[axis];
    }
    error.clear();
    return true;
}

}  // namespace w3vr::puredark_afw
