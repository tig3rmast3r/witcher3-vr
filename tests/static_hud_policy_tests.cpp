#include "config.h"

#include <cassert>
#include <initializer_list>

int main() {
    using w3vr::StaticHudModuleVisible;

    for (const bool navigation : {false, true}) {
        assert(StaticHudModuleVisible(
            false, false, false, false, navigation));
        assert(StaticHudModuleVisible(
            true, true, false, false, navigation));
        assert(StaticHudModuleVisible(
            true, false, true, false, navigation));
    }

    assert(!StaticHudModuleVisible(true, false, false, false, false));
    assert(!StaticHudModuleVisible(true, false, false, true, false));
    assert(!StaticHudModuleVisible(true, false, false, false, true));
    assert(StaticHudModuleVisible(true, false, false, true, true));
    return 0;
}
