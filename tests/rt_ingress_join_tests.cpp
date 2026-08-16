#include "rt_ingress_join.h"

#include <cstdlib>
#include <iostream>
#include <iterator>

namespace {

constexpr uint32_t kFullMask = 0x3Fu;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "Test failure: " << message << '\n';
        std::exit(1);
    }
}

w3vr::rt_join::Packet complete_packet(
    uint64_t pair,
    uint64_t predecessor) {
    w3vr::rt_join::Packet packet{};
    w3vr::rt_join::reset_packet(packet, {7, 1, pair, 3});
    w3vr::rt_join::observe(packet, kFullMask);
    w3vr::rt_join::mark_ready(packet, kFullMask);
    require(w3vr::rt_join::commit_predecessor(
        packet, predecessor, w3vr::rt_join::kAllPredecessors),
        "valid predecessor was rejected");
    return packet;
}

}  // namespace

int main() {
    const w3vr::rt_join::Identity current_identity{7, 1, 105, 3};

    w3vr::rt_join::CommandIdentityLatch command_latch{};
    constexpr uintptr_t kCommandList = 0x1234u;
    const w3vr::rt_join::Identity first_pass{7, 0, 105, 3};
    const w3vr::rt_join::Identity moving_ordinal{7, 1, 105, 3};
    require(command_latch.latch(kCommandList, first_pass).eye == 0,
        "first command-list identity was not latched");
    const auto same_recording =
        command_latch.latch(kCommandList, moving_ordinal);
    require(same_recording.eye == first_pass.eye &&
        same_recording.pair_id == first_pass.pair_id,
        "moving ordinal changed identity inside one recording");
    w3vr::rt_join::Identity looked_up{};
    require(command_latch.lookup(kCommandList, 7, 3, looked_up) &&
        looked_up.eye == first_pass.eye,
        "exact command-list identity lookup failed");
    command_latch.erase(kCommandList);
    require(!command_latch.lookup(kCommandList, 7, 3, looked_up),
        "Reset did not revoke the old command-list identity");
    require(command_latch.latch(kCommandList, moving_ordinal).eye == 1,
        "new recording did not accept its first identity");
    const w3vr::rt_join::Identity next_epoch{7, 0, 106, 4};
    require(command_latch.latch(kCommandList, next_epoch).route_epoch == 4,
        "new route epoch retained the preceding identity");
    command_latch.clear();
    require(command_latch.size() == 0,
        "command-list identity latch did not clear");

    w3vr::rt_join::TaggedIdentityCandidate camera_tags[] = {
        {{7, 0, 104, 3}, 0.0f, 0},
        {{7, 0, 105, 3}, 0.0f, 0},
        {{7, 1, 106, 3}, 0.025f, 0},
    };
    constexpr uint32_t kAoClaim = 1u << 0;
    constexpr uint32_t kReflectionClaim = 1u << 1;
    auto ao_first = w3vr::rt_join::select_tagged_identity(
        camera_tags, std::size(camera_tags), kAoClaim, 0.05f);
    require(ao_first.matched && ao_first.identity.pair_id == 104,
        "equal camera matrices did not claim the oldest AO tag");
    camera_tags[ao_first.index].claimed_sources |= kAoClaim;
    auto reflection_first = w3vr::rt_join::select_tagged_identity(
        camera_tags, std::size(camera_tags), kReflectionClaim, 0.05f);
    require(reflection_first.matched &&
        reflection_first.identity.pair_id == 104,
        "independent reflection command list did not join the AO tag");
    camera_tags[reflection_first.index].claimed_sources |= kReflectionClaim;
    auto ao_second = w3vr::rt_join::select_tagged_identity(
        camera_tags, std::size(camera_tags), kAoClaim, 0.05f);
    require(ao_second.matched && ao_second.identity.pair_id == 105,
        "second AO dispatch reused an already claimed camera tag");
    camera_tags[ao_second.index].claimed_sources |= kAoClaim;
    auto reflection_second = w3vr::rt_join::select_tagged_identity(
        camera_tags, std::size(camera_tags), kReflectionClaim, 0.05f);
    require(reflection_second.matched &&
        reflection_second.identity.pair_id == 105,
        "second reflection dispatch crossed into a newer camera tag");
    auto over_threshold = w3vr::rt_join::select_tagged_identity(
        camera_tags + 2, 1, kAoClaim, 0.01f);
    require(!over_threshold.matched,
        "camera tag above the strict matrix threshold was accepted");

    constexpr uint32_t kShadowClaim = 1u << 2;
    w3vr::rt_join::OpenTransactionCandidate open_transactions[] = {
        {{7, 1, 110, 3}, 0.0f, kReflectionClaim, true},
        {{7, 0, 111, 3}, 0.0f, kAoClaim, true},
    };
    auto ao_after_reflection = w3vr::rt_join::select_open_transaction(
        open_transactions, std::size(open_transactions),
        kAoClaim, true, 0.05f);
    require(ao_after_reflection.matched &&
        ao_after_reflection.identity.pair_id == 110 &&
        ao_after_reflection.identity.eye == 1,
        "AO could not join a transaction opened by reflection");
    auto reflection_after_ao = w3vr::rt_join::select_open_transaction(
        open_transactions, std::size(open_transactions),
        kReflectionClaim, true, 0.05f);
    require(reflection_after_ao.matched &&
        reflection_after_ao.identity.pair_id == 111 &&
        reflection_after_ao.identity.eye == 0,
        "reflection could not join a transaction opened by AO");
    auto shadow_without_signature = w3vr::rt_join::select_open_transaction(
        open_transactions, std::size(open_transactions),
        kShadowClaim, false, 0.05f);
    require(shadow_without_signature.matched &&
        shadow_without_signature.identity.pair_id == 110,
        "signature-free shadow did not consume the oldest open transaction");

    w3vr::rt_join::OpenTransactionCandidate shadow_first[] = {
        {{7, 0, 112, 3}, 0.0f, kShadowClaim, false},
    };
    auto ao_after_shadow = w3vr::rt_join::select_open_transaction(
        shadow_first, std::size(shadow_first),
        kAoClaim, true, 0.05f);
    require(ao_after_shadow.matched &&
        ao_after_shadow.identity.pair_id == 112,
        "AO could not attach a signature to a shadow-first transaction");

    w3vr::rt_join::OpenTransactionCandidate signed_mismatch[] = {
        {{7, 0, 113, 3}, 0.40f, kAoClaim, true},
    };
    auto rejected_cross_frame = w3vr::rt_join::select_open_transaction(
        signed_mismatch, std::size(signed_mismatch),
        kReflectionClaim, true, 0.05f);
    require(!rejected_cross_frame.matched,
        "a mismatching signed transaction was joined by arrival order");

    auto missing = w3vr::rt_join::evaluate(
        nullptr, nullptr, current_identity, kFullMask,
        w3vr::rt_join::kAllPredecessors);
    require(!missing.present && !missing.ready,
        "missing packet failed open");

    auto partial = complete_packet(105, 101);
    partial.ready_mask &= ~0x20u;
    auto previous = complete_packet(101, 97);
    auto partial_result = w3vr::rt_join::evaluate(
        &partial, &previous, current_identity, kFullMask,
        w3vr::rt_join::kAllPredecessors);
    require(partial_result.present && !partial_result.current_complete &&
        !partial_result.ready,
        "partial current packet was published");

    auto observed_partial = complete_packet(105, 101);
    observed_partial.observed_mask &= ~0x20u;
    observed_partial.ready_mask &= ~0x20u;
    auto observed_partial_result = w3vr::rt_join::evaluate(
        &observed_partial, &previous, current_identity, kFullMask,
        w3vr::rt_join::kAllPredecessors);
    require(!observed_partial_result.current_complete &&
        !observed_partial_result.ready,
        "packet with matching partial observed/ready masks was published");

    auto current = complete_packet(105, 101);
    auto exact_gap_result = w3vr::rt_join::evaluate(
        &current, &previous, current_identity, kFullMask,
        w3vr::rt_join::kAllPredecessors);
    require(exact_gap_result.ready &&
        exact_gap_result.predecessor_pair_id == 101,
        "exact non-contiguous predecessor did not recover the lineage");

    auto numeric_previous = complete_packet(104, 100);
    auto wrong_previous_result = w3vr::rt_join::evaluate(
        &current, &numeric_previous, current_identity, kFullMask,
        w3vr::rt_join::kAllPredecessors);
    require(!wrong_previous_result.predecessor_complete &&
        !wrong_previous_result.ready,
        "numeric pair-1 incorrectly replaced the exact predecessor");

    auto conflict = complete_packet(105, 101);
    require(!w3vr::rt_join::commit_predecessor(
        conflict, 102, w3vr::rt_join::kReflectionPredecessor),
        "conflicting family predecessor was accepted");
    auto conflict_result = w3vr::rt_join::evaluate(
        &conflict, &previous, current_identity, kFullMask,
        w3vr::rt_join::kAllPredecessors);
    require(!conflict_result.current_complete && !conflict_result.ready,
        "conflicting family lineage was published");

    w3vr::rt_join::AfwTransportProof transport{
        true, true, true, true, true, true, true};
    require(w3vr::rt_join::afw_transport_ready(transport),
        "complete AFW transport ticket was rejected");
    transport.submission_ordered = false;
    require(!w3vr::rt_join::afw_transport_ready(transport),
        "unordered AFW transport ticket was published");

    require(w3vr::rt_join::afw_sequence_accepts(
            false, UINT32_MAX, 0, 0, 10, true),
        "first exact final-color transaction was rejected");
    require(w3vr::rt_join::afw_sequence_accepts(
            true, 0, 10, 1, 10, false),
        "alternating monotonic transaction was rejected");
    require(w3vr::rt_join::afw_sequence_accepts(
            true, 0, 10, 0, 11, true),
        "strictly newer exact final-color transaction was rejected");
    require(!w3vr::rt_join::afw_sequence_accepts(
            true, 0, 10, 0, 10, true),
        "same-eye exact duplicate was accepted");
    require(!w3vr::rt_join::afw_sequence_accepts(
            true, 0, 10, 1, 9, true),
        "backward exact final-color transaction was accepted");

    require(w3vr::rt_join::afw_desired_eye(
            0, false, true, true) == 1,
        "immutable ticket incorrectly inherited packed-eye bootstrap");
    require(w3vr::rt_join::afw_desired_eye(
            0, false, true, false) == 0,
        "legacy packed bootstrap no longer selects its missing eye");

    std::cout << "All RT ingress-join tests passed.\n";
    return 0;
}
