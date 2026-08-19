#include "shadow_cascade_authority_policy.h"

#include <iostream>

int main() {
    using w3vr::shadow_cascade::AuthorityAction;
    using w3vr::shadow_cascade::caller_kind;
    using w3vr::shadow_cascade::decide_authority;

    bool ok = true;
    ok &= caller_kind(0x01E0AB0F) == 0;
    ok &= caller_kind(0x01E0ADDC) == 1;
    ok &= caller_kind(0x01E0AB10) == -1;

    ok &= decide_authority(false, 0, 0, 0, 1604, 7, 0) ==
        AuthorityAction::Seed;
    ok &= decide_authority(true, 1603, 7, 0, 1604, 7, 1) ==
        AuthorityAction::Seed;
    ok &= decide_authority(true, 1604, 6, 0, 1604, 7, 1) ==
        AuthorityAction::Seed;
    ok &= decide_authority(true, 1604, 7, 0, 1604, 7, 0) ==
        AuthorityAction::SameEye;
    ok &= decide_authority(true, 1604, 7, 0, 1604, 7, 1) ==
        AuthorityAction::Reuse;

    if (!ok) {
        std::cerr << "shadow cascade authority policy tests failed\n";
        return 1;
    }
    return 0;
}
