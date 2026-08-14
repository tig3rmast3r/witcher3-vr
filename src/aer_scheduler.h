#pragma once

#include <cstdint>

namespace w3vr::aer {

struct FrameIdentity {
    uint32_t eye{};
    uint64_t pair_id{};
};

// REDengine builds one real frame between consecutive Present calls. The
// render ordinal therefore owns the eye identity; both adjacent ordinals share
// one nonzero stereo pair id.
FrameIdentity identity_for_render_ordinal(uint64_t render_ordinal);

// Present count is incremented before the just-rendered backbuffer is captured.
// The first Present (count 1) consequently publishes render ordinal 0.
FrameIdentity identity_for_present_count(uint64_t present_count);

}  // namespace w3vr::aer
