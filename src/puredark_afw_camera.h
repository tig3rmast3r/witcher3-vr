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
