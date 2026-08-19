#pragma once

#include <cstdint>

namespace w3vr::taau_submission {

struct AuthorityDecision {
    uint64_t effective_pair{};
    bool preserve_previous{};
};

// A stale resolve does not contain the pixels named by its old producer tag:
// it copies the already-valid private eye history instead. It therefore must
// not change submitted-pair authority in either direction. Normal resolves
// retain the established last-submission semantics, including diagnostics for
// unexpected non-forward ordering.
constexpr AuthorityDecision decide_authority(
    uint64_t previous_pair,
    uint64_t incoming_pair,
    bool replayed_as_stale) {
    return replayed_as_stale
        ? AuthorityDecision{previous_pair, true}
        : AuthorityDecision{incoming_pair, false};
}

}  // namespace w3vr::taau_submission
