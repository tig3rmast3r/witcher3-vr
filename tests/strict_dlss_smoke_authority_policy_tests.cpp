#include "strict_dlss_smoke_authority_policy.h"

#include <cassert>
#include <cstdint>
#include <limits>

int main() {
    using w3vr::strict_dlss_smoke::valid_exact_identity;

    assert(valid_exact_identity(true, 0, 41, 7, 7));
    assert(valid_exact_identity(true, 1, 41, 7, 7));
    assert(!valid_exact_identity(false, 0, 41, 7, 7));
    assert(!valid_exact_identity(true, 2, 41, 7, 7));
    assert(!valid_exact_identity(true, 0, 0, 7, 7));
    assert(!valid_exact_identity(
        true, 0, std::numeric_limits<uint64_t>::max(), 7, 7));
    assert(!valid_exact_identity(true, 0, 41, 6, 7));
    return 0;
}
