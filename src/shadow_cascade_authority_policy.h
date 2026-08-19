#pragma once

#include <cstdint>

namespace w3vr::shadow_cascade {

enum class AuthorityAction : uint8_t {
    Seed,
    Reuse,
    SameEye,
};

constexpr int caller_kind(uintptr_t return_rva) {
    switch (return_rva) {
    case 0x01E0AB0F:
        return 0;
    case 0x01E0ADDC:
        return 1;
    default:
        return -1;
    }
}

constexpr AuthorityAction decide_authority(
    bool slot_valid,
    uint64_t slot_pair,
    uint32_t slot_generation,
    uint32_t authority_eye,
    uint64_t incoming_pair,
    uint32_t incoming_generation,
    uint32_t incoming_eye) {
    if (!slot_valid || slot_pair != incoming_pair ||
        slot_generation != incoming_generation) {
        return AuthorityAction::Seed;
    }
    return authority_eye == incoming_eye
        ? AuthorityAction::SameEye
        : AuthorityAction::Reuse;
}

}  // namespace w3vr::shadow_cascade
