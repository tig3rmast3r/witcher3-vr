#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace w3vr::rt_flight {

// The recorder is intentionally independent from the text logger. Event
// producers only claim a bounded RAM slot and copy this compact POD payload;
// filesystem work happens exclusively when the user requests a snapshot.
enum class EventCode : uint16_t {
    Present = 0,
    IngressObserved,
    IngressReady,
    IngressSnapshot,
    IdentityReserved,
    IdentityResolved,
    IdentityRejected,
    AoReaderBegin,
    AoReaderRejected,
    AoReaderComplete,
    AoWriterBegin,
    AoWriterRejected,
    AoWriterComplete,
    ShadowReaderBegin,
    ShadowReaderRejected,
    ShadowReaderComplete,
    ShadowWriterBegin,
    ShadowWriterFallback,
    ShadowWriterComplete,
    SpecularReaderBegin,
    SpecularReaderRejected,
    SpecularReaderComplete,
    SpecularCameraBegin,
    SpecularCameraRejected,
    SpecularCameraComplete,
    AfwFinalize,
    AfwSubmission,
    AfwEvaluateBegin,
    AfwEvaluateRejected,
    AfwEvaluated,
    AfwPresentRoute,
    RouteReset,
    Count,
};

enum class RejectReason : uint16_t {
    None = 0,
    Identity,
    Descriptor,
    History,
    Camera,
    Transaction,
    ConstantBuffer,
    OverrideSlot,
    Resource,
    Fifo,
    Bundle,
    CrossQueue,
    ColorCopy,
    Evaluate,
    Output,
    PublishGate,
};

struct Event {
    uint64_t sequence{};
    int64_t qpc{};
    uint64_t present{};
    uint64_t pair_id{};
    uint64_t previous_pair_id{};
    uint64_t route_epoch{};
    uint32_t generation{};
    uint32_t eye{UINT32_MAX};
    EventCode code{EventCode::Present};
    RejectReason reason{RejectReason::None};
    uint32_t flags{};
    uint32_t detail0{};
    uint32_t detail1{};
    uint32_t detail2{};
    uint32_t detail3{};
};

struct Snapshot {
    std::vector<Event> events;
    uint64_t total_claimed{};
    uint64_t overwritten{};
    uint64_t skipped_while_paused{};
};

class Recorder {
public:
    static constexpr size_t kCapacity = 32768;

    void record(Event event) noexcept;
    Snapshot capture_since(int64_t minimum_qpc);

private:
    std::array<Event, kCapacity> events_{};
    std::atomic<uint64_t> next_sequence_{};
    std::atomic<uint32_t> active_writers_{};
    std::atomic<uint64_t> skipped_while_paused_{};
    std::atomic<bool> paused_{};
};

const char* event_code_name(EventCode code) noexcept;
const char* reject_reason_name(RejectReason reason) noexcept;

}  // namespace w3vr::rt_flight
