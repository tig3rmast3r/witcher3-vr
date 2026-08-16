#include "rt_flight_recorder.h"

#include <algorithm>
#include <thread>

namespace w3vr::rt_flight {

void Recorder::record(Event event) noexcept {
    if (paused_.load(std::memory_order_seq_cst)) {
        skipped_while_paused_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    active_writers_.fetch_add(1, std::memory_order_seq_cst);
    if (paused_.load(std::memory_order_seq_cst)) {
        active_writers_.fetch_sub(1, std::memory_order_seq_cst);
        skipped_while_paused_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const uint64_t sequence = next_sequence_.fetch_add(
        1, std::memory_order_relaxed);
    event.sequence = sequence + 1;
    events_[sequence % kCapacity] = event;
    active_writers_.fetch_sub(1, std::memory_order_seq_cst);
}

Snapshot Recorder::capture_since(int64_t minimum_qpc) {
    paused_.store(true, std::memory_order_seq_cst);
    while (active_writers_.load(std::memory_order_seq_cst) != 0) {
        std::this_thread::yield();
    }

    Snapshot snapshot{};
    snapshot.total_claimed = next_sequence_.load(std::memory_order_relaxed);
    const uint64_t first_sequence = snapshot.total_claimed > kCapacity
        ? snapshot.total_claimed - kCapacity
        : 0;
    snapshot.overwritten = first_sequence;
    snapshot.events.reserve(static_cast<size_t>(
        snapshot.total_claimed - first_sequence));
    for (uint64_t sequence = first_sequence;
         sequence < snapshot.total_claimed; ++sequence) {
        const auto& event = events_[sequence % kCapacity];
        if (event.sequence != sequence + 1 || event.qpc < minimum_qpc) {
            continue;
        }
        snapshot.events.push_back(event);
    }

    paused_.store(false, std::memory_order_seq_cst);
    snapshot.skipped_while_paused = skipped_while_paused_.exchange(
        0, std::memory_order_relaxed);
    return snapshot;
}

const char* event_code_name(EventCode code) noexcept {
    switch (code) {
    case EventCode::Present: return "present";
    case EventCode::IngressObserved: return "ingress_observed";
    case EventCode::IngressReady: return "ingress_ready";
    case EventCode::IngressSnapshot: return "ingress_snapshot";
    case EventCode::IdentityReserved: return "identity_reserved";
    case EventCode::IdentityResolved: return "identity_resolved";
    case EventCode::IdentityRejected: return "identity_rejected";
    case EventCode::AoReaderBegin: return "ao_reader_begin";
    case EventCode::AoReaderRejected: return "ao_reader_rejected";
    case EventCode::AoReaderComplete: return "ao_reader_complete";
    case EventCode::AoWriterBegin: return "ao_writer_begin";
    case EventCode::AoWriterRejected: return "ao_writer_rejected";
    case EventCode::AoWriterComplete: return "ao_writer_complete";
    case EventCode::ShadowReaderBegin: return "shadow_reader_begin";
    case EventCode::ShadowReaderRejected: return "shadow_reader_rejected";
    case EventCode::ShadowReaderComplete: return "shadow_reader_complete";
    case EventCode::ShadowWriterBegin: return "shadow_writer_begin";
    case EventCode::ShadowWriterFallback: return "shadow_writer_fallback";
    case EventCode::ShadowWriterComplete: return "shadow_writer_complete";
    case EventCode::SpecularReaderBegin: return "specular_reader_begin";
    case EventCode::SpecularReaderRejected: return "specular_reader_rejected";
    case EventCode::SpecularReaderComplete: return "specular_reader_complete";
    case EventCode::SpecularCameraBegin: return "specular_camera_begin";
    case EventCode::SpecularCameraRejected: return "specular_camera_rejected";
    case EventCode::SpecularCameraComplete: return "specular_camera_complete";
    case EventCode::AfwFinalize: return "afw_finalize";
    case EventCode::AfwSubmission: return "afw_submission";
    case EventCode::AfwEvaluateBegin: return "afw_evaluate_begin";
    case EventCode::AfwEvaluateRejected: return "afw_evaluate_rejected";
    case EventCode::AfwEvaluated: return "afw_evaluated";
    case EventCode::AfwPresentRoute: return "afw_present_route";
    case EventCode::RouteReset: return "route_reset";
    case EventCode::Count: break;
    }
    return "unknown";
}

const char* reject_reason_name(RejectReason reason) noexcept {
    switch (reason) {
    case RejectReason::None: return "none";
    case RejectReason::Identity: return "identity";
    case RejectReason::Descriptor: return "descriptor";
    case RejectReason::History: return "history";
    case RejectReason::Camera: return "camera";
    case RejectReason::Transaction: return "transaction";
    case RejectReason::ConstantBuffer: return "constant_buffer";
    case RejectReason::OverrideSlot: return "override_slot";
    case RejectReason::Resource: return "resource";
    case RejectReason::Fifo: return "fifo";
    case RejectReason::Bundle: return "bundle";
    case RejectReason::CrossQueue: return "cross_queue";
    case RejectReason::ColorCopy: return "color_copy";
    case RejectReason::Evaluate: return "evaluate";
    case RejectReason::Output: return "output";
    case RejectReason::PublishGate: return "publish_gate";
    }
    return "unknown";
}

}  // namespace w3vr::rt_flight
