#pragma once

#include <cstdint>
#include <limits>

namespace w3vr::strict_dlss_smoke {

constexpr bool valid_exact_identity(
    bool task_provenance_valid,
    uint32_t eye,
    uint64_t pair_id,
    uint32_t tag_generation,
    uint32_t current_generation) {
    return task_provenance_valid && eye <= 1 && pair_id != 0 &&
        pair_id != std::numeric_limits<uint64_t>::max() &&
        tag_generation == current_generation;
}

} // namespace w3vr::strict_dlss_smoke
