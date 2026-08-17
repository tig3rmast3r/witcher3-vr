#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace w3vr::rt_join {

struct Identity {
    uint32_t generation{};
    uint32_t eye{UINT32_MAX};
    uint64_t pair_id{};
    uint64_t route_epoch{};
};

struct TaggedIdentityCandidate {
    Identity identity{};
    float matrix_error{};
    uint32_t claimed_sources{};
};

struct TaggedIdentitySelection {
    Identity identity{};
    size_t index{SIZE_MAX};
    float matrix_error{};
    bool matched{};
};

struct OpenTransactionCandidate {
    Identity identity{};
    float signature_error{};
    uint32_t claimed_sources{};
    bool signature_valid{};
};

// Once a pass has recovered an exact camera-created RTX identity, cache it for
// the rest of that D3D12 command-list recording. This latch never invents an
// identity from timing; Reset starts the next recording.
class CommandIdentityLatch {
public:
    bool lookup(
        uintptr_t command_list_key,
        uint32_t generation,
        uint64_t route_epoch,
        Identity& identity) const;
    Identity latch(
        uintptr_t command_list_key,
        const Identity& candidate);
    void erase(uintptr_t command_list_key);
    void clear();
    size_t size() const;

private:
    mutable std::mutex mutex_{};
    std::unordered_map<uintptr_t, Identity> identities_{};
};

// A camera tag is created before RTX recording begins. AO and reflection may
// later arrive on different command lists, so each family claims the oldest
// still-unclaimed tag among equally matching camera matrices. Arrival timing
// and the moving natural-render ordinal are never candidate identities.
TaggedIdentitySelection select_tagged_identity(
    const TaggedIdentityCandidate* candidates,
    size_t candidate_count,
    uint32_t source_mask,
    float maximum_matrix_error) noexcept;

// RTX families are independent producers. Select an already-open transaction
// without assuming which family arrived first. A matching NRD signature wins;
// if no producer has supplied a signature yet, per-family FIFO is the fallback.
// A signed mismatch is never silently joined.
TaggedIdentitySelection select_open_transaction(
    const OpenTransactionCandidate* candidates,
    size_t candidate_count,
    uint32_t source_mask,
    bool incoming_signature_valid,
    float maximum_signature_error) noexcept;

// GPU queue ordering proves the pixels. The removed CPU diagnostic ingress
// ledger is not part of this transport gate and cannot discard an immutable
// DLSS/AFW ticket.
struct AfwTransportProof {
    bool slot_matches{};
    bool bundle_ready{};
    bool exact_identity{};
    bool submission_ordered{};
    bool gameplay_active{};
    bool epoch_matches{};
    bool generation_matches{};
};

bool afw_transport_ready(const AfwTransportProof& proof) noexcept;
bool afw_sequence_accepts(
    bool sequence_valid,
    uint32_t last_real_eye,
    uint64_t last_pair_id,
    uint32_t incoming_eye,
    uint64_t incoming_pair_id,
    bool exact_final_color) noexcept;
uint32_t afw_desired_eye(
    uint32_t last_real_eye,
    bool packed_eye_zero_valid,
    bool packed_eye_one_valid,
    bool immutable_color_ticket) noexcept;

}  // namespace w3vr::rt_join
