#pragma once

#include "puredark_afw_bridge.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace w3vr::puredark_afw {

struct ParallelEyePose {
    float orientation[4]{0.0f, 0.0f, 0.0f, 1.0f};
    float position[3]{};
};

// Exact off-axis geometry of one rendered eye. Tangent spans control the
// projection scale while center_ndc identifies the optical center already
// carried by REDengine's cameraViewToClip matrix.
struct EyeProjectionGeometry {
    float horizontal_tangent_span{};
    float vertical_tangent_span{};
    float center_ndc_x{};
    float center_ndc_y{};
};

// Builds the source-eye and missing-eye matrices consumed by PureDark from
// Streamline 1.5 sl::Constants. Streamline stores row-major matrices while
// PureDark uses GLM-compatible matrix memory; the byte layout is the transpose
// required to move from row-vector to column-vector convention.
bool build_camera_data_from_streamline(
    const float* constants,
    size_t float_count,
    uint32_t source_eye,
    float eye_baseline_m,
    CameraData& camera_data,
    std::wstring& error);

// Mode-3 native pixels are rendered with an off-axis optical center while the
// Streamline projection supplied to DLSS remains centered. Add the absolute
// source-eye center before deriving the peer-eye projection.
bool apply_source_eye_projection_center(
    const EyeProjectionGeometry& source_geometry,
    CameraData& camera_data,
    std::wstring& error);

// PureDark beta.5 expects destViewToClip to describe the missing eye. Preserve
// every source projection term (including reverse-Z and jitter), then replace
// only the scale and optical-center terms with the frozen peer-eye geometry.
bool retarget_destination_eye_projection(
    const EyeProjectionGeometry& source_geometry,
    const EyeProjectionGeometry& destination_geometry,
    CameraData& camera_data,
    std::wstring& error);

// Reanchors a same-time parallel-eye pair to the exact source-eye pose which
// completed REDengine rendering. The captured inter-eye translation remains
// intact while both eyes inherit the exact rendered orientation.
bool rebase_parallel_eye_pose(
    const ParallelEyePose& captured_source,
    const ParallelEyePose& captured_destination,
    const ParallelEyePose& exact_source,
    ParallelEyePose& exact_destination,
    std::wstring& error);

}  // namespace w3vr::puredark_afw
