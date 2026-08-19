#include "aer_presentation_size_policy.h"

#include <cstdlib>
#include <iostream>

namespace policy = w3vr::aer_presentation_size_policy;

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    const policy::FinalOpenXrRemapInput active{
        true, true, true, true, true, 0.75f};
    require(policy::final_openxr_remap_active(active),
        "AER asymmetric gameplay below scale 1 must use the final OpenXR remap");

    auto input = active;
    input.mode3_aer = false;
    require(!policy::final_openxr_remap_active(input),
        "strict Stereo must keep its existing native presentation");
    input = active;
    input.native_asymmetric = false;
    require(!policy::final_openxr_remap_active(input),
        "AER symmetric projection must remain unchanged");
    input = active;
    input.gameplay = false;
    require(!policy::final_openxr_remap_active(input),
        "Cinema and loading presentation must remain unchanged");
    input = active;
    input.projection_pipeline_ready = false;
    require(!policy::final_openxr_remap_active(input),
        "the remap must fail closed without the final projection pipeline");
    input = active;
    input.presentation_scale = 1.0f;
    require(!policy::final_openxr_remap_active(input),
        "scale 1 must retain the established AER submission exactly");

    std::cout << "AER presentation size policy tests passed\n";
    return 0;
}
