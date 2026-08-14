#include "aer_scheduler.h"

namespace w3vr::aer {

FrameIdentity identity_for_render_ordinal(uint64_t render_ordinal) {
    return {
        static_cast<uint32_t>(render_ordinal & 1ull),
        render_ordinal / 2ull + 1ull};
}

FrameIdentity identity_for_present_count(uint64_t present_count) {
    const uint64_t render_ordinal = present_count > 0
        ? present_count - 1ull
        : 0ull;
    return identity_for_render_ordinal(render_ordinal);
}

}  // namespace w3vr::aer
