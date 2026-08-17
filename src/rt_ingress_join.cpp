#include "rt_ingress_join.h"

#include <cmath>
#include <limits>

namespace w3vr::rt_join {

namespace {

bool valid_command_identity(
    uintptr_t command_list_key,
    const Identity& identity) noexcept {
    return command_list_key != 0 && identity.eye <= 1 &&
        identity.pair_id != 0 && identity.pair_id != UINT64_MAX;
}

}  // namespace

bool CommandIdentityLatch::lookup(
    uintptr_t command_list_key,
    uint32_t generation,
    uint64_t route_epoch,
    Identity& identity) const {
    if (command_list_key == 0) {
        return false;
    }
    std::scoped_lock lock{mutex_};
    const auto found = identities_.find(command_list_key);
    if (found == identities_.end() ||
        found->second.generation != generation ||
        found->second.route_epoch != route_epoch) {
        return false;
    }
    identity = found->second;
    return true;
}

Identity CommandIdentityLatch::latch(
    uintptr_t command_list_key,
    const Identity& candidate) {
    if (!valid_command_identity(command_list_key, candidate)) {
        return {};
    }
    std::scoped_lock lock{mutex_};
    auto found = identities_.find(command_list_key);
    if (found == identities_.end()) {
        identities_.emplace(command_list_key, candidate);
        return candidate;
    }
    if (found->second.generation != candidate.generation ||
        found->second.route_epoch != candidate.route_epoch) {
        found->second = candidate;
    }
    return found->second;
}

void CommandIdentityLatch::erase(uintptr_t command_list_key) {
    if (command_list_key == 0) {
        return;
    }
    std::scoped_lock lock{mutex_};
    identities_.erase(command_list_key);
}

void CommandIdentityLatch::clear() {
    std::scoped_lock lock{mutex_};
    identities_.clear();
}

size_t CommandIdentityLatch::size() const {
    std::scoped_lock lock{mutex_};
    return identities_.size();
}

TaggedIdentitySelection select_tagged_identity(
    const TaggedIdentityCandidate* candidates,
    size_t candidate_count,
    uint32_t source_mask,
    float maximum_matrix_error) noexcept {
    TaggedIdentitySelection selected{};
    selected.matrix_error = std::numeric_limits<float>::infinity();
    if (candidates == nullptr || candidate_count == 0 || source_mask == 0 ||
        !std::isfinite(maximum_matrix_error) || maximum_matrix_error < 0.0f) {
        return selected;
    }

    for (size_t index = 0; index < candidate_count; ++index) {
        const auto& candidate = candidates[index];
        if (candidate.identity.eye > 1 || candidate.identity.pair_id == 0 ||
            candidate.identity.pair_id == UINT64_MAX ||
            (candidate.claimed_sources & source_mask) != 0 ||
            !std::isfinite(candidate.matrix_error) ||
            candidate.matrix_error > maximum_matrix_error) {
            continue;
        }
        const bool lower_error = !selected.matched ||
            candidate.matrix_error < selected.matrix_error;
        const bool same_error_older_tag = selected.matched &&
            candidate.matrix_error == selected.matrix_error &&
            candidate.identity.pair_id < selected.identity.pair_id;
        if (!lower_error && !same_error_older_tag) {
            continue;
        }
        selected.identity = candidate.identity;
        selected.index = index;
        selected.matrix_error = candidate.matrix_error;
        selected.matched = true;
    }
    return selected;
}

TaggedIdentitySelection select_open_transaction(
    const OpenTransactionCandidate* candidates,
    size_t candidate_count,
    uint32_t source_mask,
    bool incoming_signature_valid,
    float maximum_signature_error) noexcept {
    TaggedIdentitySelection exact{};
    TaggedIdentitySelection unsigned_fifo{};
    exact.matrix_error = std::numeric_limits<float>::infinity();
    unsigned_fifo.matrix_error = std::numeric_limits<float>::infinity();
    if (candidates == nullptr || candidate_count == 0 || source_mask == 0 ||
        !std::isfinite(maximum_signature_error) ||
        maximum_signature_error < 0.0f) {
        return exact;
    }

    const auto older_than = [](const Identity& lhs, const Identity& rhs) {
        return lhs.pair_id < rhs.pair_id ||
            (lhs.pair_id == rhs.pair_id && lhs.eye < rhs.eye);
    };
    for (size_t index = 0; index < candidate_count; ++index) {
        const auto& candidate = candidates[index];
        if (candidate.identity.eye > 1 || candidate.identity.pair_id == 0 ||
            candidate.identity.pair_id == UINT64_MAX ||
            candidate.claimed_sources == 0 ||
            (candidate.claimed_sources & source_mask) != 0) {
            continue;
        }
        if (!incoming_signature_valid) {
            if (!unsigned_fifo.matched ||
                older_than(candidate.identity, unsigned_fifo.identity)) {
                unsigned_fifo.identity = candidate.identity;
                unsigned_fifo.index = index;
                unsigned_fifo.matrix_error = candidate.signature_error;
                unsigned_fifo.matched = true;
            }
            continue;
        }
        if (!candidate.signature_valid) {
            if (!unsigned_fifo.matched ||
                older_than(candidate.identity, unsigned_fifo.identity)) {
                unsigned_fifo.identity = candidate.identity;
                unsigned_fifo.index = index;
                unsigned_fifo.matrix_error = candidate.signature_error;
                unsigned_fifo.matched = true;
            }
            continue;
        }
        if (!std::isfinite(candidate.signature_error) ||
            candidate.signature_error > maximum_signature_error) {
            continue;
        }
        const bool lower_error = !exact.matched ||
            candidate.signature_error < exact.matrix_error;
        const bool same_error_older = exact.matched &&
            candidate.signature_error == exact.matrix_error &&
            older_than(candidate.identity, exact.identity);
        if (lower_error || same_error_older) {
            exact.identity = candidate.identity;
            exact.index = index;
            exact.matrix_error = candidate.signature_error;
            exact.matched = true;
        }
    }
    return exact.matched ? exact : unsigned_fifo;
}

bool afw_transport_ready(const AfwTransportProof& proof) noexcept {
    return proof.slot_matches && proof.bundle_ready &&
        proof.exact_identity && proof.submission_ordered &&
        proof.gameplay_active && proof.epoch_matches &&
        proof.generation_matches;
}

bool afw_sequence_accepts(
    bool sequence_valid,
    uint32_t last_real_eye,
    uint64_t last_pair_id,
    uint32_t incoming_eye,
    uint64_t incoming_pair_id,
    bool exact_final_color) noexcept {
    if (incoming_eye > 1 || incoming_pair_id == 0 ||
        incoming_pair_id == UINT64_MAX) {
        return false;
    }
    if (!sequence_valid) {
        return true;
    }

    const bool alternating = last_real_eye > 1 || incoming_eye != last_real_eye;
    const bool monotonic = incoming_pair_id >= last_pair_id;
    const bool exact_forward = exact_final_color &&
        incoming_pair_id > last_pair_id;
    return exact_forward || (alternating && monotonic);
}

uint32_t afw_desired_eye(
    uint32_t last_real_eye,
    bool packed_eye_zero_valid,
    bool packed_eye_one_valid,
    bool immutable_color_ticket) noexcept {
    uint32_t desired = last_real_eye <= 1
        ? 1u - last_real_eye : UINT32_MAX;
    if (immutable_color_ticket) {
        return desired;
    }
    if (!packed_eye_zero_valid) {
        return 0;
    }
    if (!packed_eye_one_valid) {
        return 1;
    }
    return desired;
}

}  // namespace w3vr::rt_join
