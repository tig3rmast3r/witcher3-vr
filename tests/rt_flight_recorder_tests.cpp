#include "rt_flight_recorder.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "Test failure: " << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    auto recorder = std::make_unique<w3vr::rt_flight::Recorder>();
    for (uint64_t index = 0; index < 6; ++index) {
        w3vr::rt_flight::Event event{};
        event.qpc = 100 + static_cast<int64_t>(index) * 10;
        event.present = index;
        event.pair_id = 1000 + index;
        event.code = w3vr::rt_flight::EventCode::IngressReady;
        recorder->record(event);
    }
    auto recent = recorder->capture_since(130);
    require(recent.events.size() == 3, "10-second cutoff equivalent failed");
    require(recent.events.front().present == 3 &&
        recent.events.back().pair_id == 1005,
        "snapshot order or payload changed");

    recorder = std::make_unique<w3vr::rt_flight::Recorder>();
    for (uint64_t index = 0;
         index < w3vr::rt_flight::Recorder::kCapacity + 5; ++index) {
        w3vr::rt_flight::Event event{};
        event.qpc = 1;
        event.present = index;
        recorder->record(event);
    }
    auto wrapped = recorder->capture_since(0);
    require(wrapped.events.size() ==
            w3vr::rt_flight::Recorder::kCapacity,
        "bounded recorder did not retain exactly its capacity");
    require(wrapped.overwritten == 5 &&
        wrapped.events.front().present == 5 &&
        wrapped.events.back().present ==
            w3vr::rt_flight::Recorder::kCapacity + 4,
        "ring wrap did not retain the newest ordered events");
    require(std::string(w3vr::rt_flight::event_code_name(
                w3vr::rt_flight::EventCode::AfwSubmission)) ==
            "afw_submission" &&
        std::string(w3vr::rt_flight::reject_reason_name(
                w3vr::rt_flight::RejectReason::CrossQueue)) ==
            "cross_queue",
        "dump labels changed");

    std::cout << "All RT flight-recorder tests passed.\n";
    return 0;
}
