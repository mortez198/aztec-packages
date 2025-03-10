#include "init.hpp"

#include "aztec3/circuits/abis/rollup/root/root_rollup_inputs.hpp"
#include "aztec3/circuits/abis/rollup/root/root_rollup_public_inputs.hpp"
#include "aztec3/circuits/rollup/components/components.hpp"
#include "aztec3/constants.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <vector>

namespace aztec3::circuits::rollup::native_root_rollup {

// TODO: can we aggregate proofs if we do not have a working circuit impl
// TODO: change the public inputs array - we wont be using this?

// Access Native types through NT namespace

/**
 * @brief Calculates the messages subtree from the leaves array
 * @param leaves
 * @return root
 */
NT::fr calculate_subtree(std::array<NT::fr, NUMBER_OF_L1_L2_MESSAGES_PER_ROLLUP> leaves)
{
    MerkleTree merkle_tree = MerkleTree(L1_TO_L2_MSG_SUBTREE_DEPTH);

    for (size_t i = 0; i < NUMBER_OF_L1_L2_MESSAGES_PER_ROLLUP; i++) {
        merkle_tree.update_element(i, leaves[i]);
    }
    return merkle_tree.root();
}

/**
 * @brief Computes the messages hash from the leaves array
 * @param leaves
 * @param return - hash split into two field elements
 */
std::array<NT::fr, 2> compute_messages_hash(std::array<NT::fr, NUMBER_OF_L1_L2_MESSAGES_PER_ROLLUP> leaves)
{
    // convert vector of field elements into uint_8
    std::array<uint8_t, 32 * NUMBER_OF_L1_L2_MESSAGES_PER_ROLLUP> messages_hash_input_bytes;
    for (size_t i = 0; i < NUMBER_OF_L1_L2_MESSAGES_PER_ROLLUP; i++) {
        auto bytes = leaves[i].to_buffer();
        for (size_t j = 0; j < 32; j++) {
            messages_hash_input_bytes[i * 32 + j] = bytes[j];
        }
    }

    std::vector<uint8_t> const messages_hash_input_bytes_vec(messages_hash_input_bytes.begin(),
                                                             messages_hash_input_bytes.end());
    auto h = sha256::sha256(messages_hash_input_bytes_vec);

    std::array<uint8_t, 32> buf_1;
    std::array<uint8_t, 32> buf_2;
    for (uint8_t i = 0; i < 16; i++) {
        buf_1[i] = 0;
        buf_1[16 + i] = h[i];
        buf_2[i] = 0;
        buf_2[16 + i] = h[i + 16];
    }
    auto high = fr::serialize_from_buffer(buf_1.data());
    auto low = fr::serialize_from_buffer(buf_2.data());

    return { high, low };
}

RootRollupPublicInputs root_rollup_circuit(DummyComposer& composer, RootRollupInputs const& rootRollupInputs)
{
    // TODO: Verify the previous rollup proofs
    // TODO: Check both previous rollup vks (in previous_rollup_data) against the permitted set of kernel vks.
    // we don't have a set of permitted kernel vks yet.

    auto left = rootRollupInputs.previous_rollup_data[0].base_or_merge_rollup_public_inputs;
    auto right = rootRollupInputs.previous_rollup_data[1].base_or_merge_rollup_public_inputs;

    auto aggregation_object = components::aggregate_proofs(left, right);
    components::assert_both_input_proofs_of_same_rollup_type(composer, left, right);
    components::assert_both_input_proofs_of_same_height_and_return(composer, left, right);
    components::assert_equal_constants(composer, left, right);
    components::assert_prev_rollups_follow_on_from_each_other(composer, left, right);

    // Update the historic private data tree
    auto end_tree_of_historic_private_data_tree_roots_snapshot = components::insert_subtree_to_snapshot_tree(
        composer,
        left.constants.start_tree_of_historic_private_data_tree_roots_snapshot,
        rootRollupInputs.new_historic_private_data_tree_root_sibling_path,
        fr::zero(),
        right.end_private_data_tree_snapshot.root,
        0,
        "historic private data tree roots insertion");

    // Update the historic private data tree
    auto end_tree_of_historic_contract_tree_roots_snapshot =
        components::insert_subtree_to_snapshot_tree(composer,
                                                    left.constants.start_tree_of_historic_contract_tree_roots_snapshot,
                                                    rootRollupInputs.new_historic_contract_tree_root_sibling_path,
                                                    fr::zero(),
                                                    right.end_contract_tree_snapshot.root,
                                                    0,
                                                    "historic contract tree roots insertion");

    // Check correct l1 to l2 tree given
    // Compute subtree inserting l1 to l2 messages
    auto l1_to_l2_subtree_root = calculate_subtree(rootRollupInputs.l1_to_l2_messages);

    // // Insert subtree into the l1 to l2 data tree
    const auto empty_l1_to_l2_subtree_root = components::calculate_empty_tree_root(L1_TO_L2_MSG_SUBTREE_DEPTH);
    auto new_l1_to_l2_messages_tree_snapshot =
        components::insert_subtree_to_snapshot_tree(composer,
                                                    rootRollupInputs.start_l1_to_l2_message_tree_snapshot,
                                                    rootRollupInputs.new_l1_to_l2_message_tree_root_sibling_path,
                                                    empty_l1_to_l2_subtree_root,
                                                    l1_to_l2_subtree_root,
                                                    L1_TO_L2_MSG_SUBTREE_DEPTH,
                                                    "l1 to l2 message tree insertion");

    // Update the historic l1 to l2 data tree
    auto end_l1_to_l2_data_roots_tree_snapshot = components::insert_subtree_to_snapshot_tree(
        composer,
        rootRollupInputs.start_historic_tree_l1_to_l2_message_tree_roots_snapshot,
        rootRollupInputs.new_historic_l1_to_l2_message_roots_tree_sibling_path,
        fr::zero(),
        new_l1_to_l2_messages_tree_snapshot.root,
        0,
        "historic l1 to l2 message tree roots insertion");

    RootRollupPublicInputs public_inputs = {
        .end_aggregation_object = aggregation_object,
        .start_private_data_tree_snapshot = left.start_private_data_tree_snapshot,
        .end_private_data_tree_snapshot = right.end_private_data_tree_snapshot,
        .start_nullifier_tree_snapshot = left.start_nullifier_tree_snapshot,
        .end_nullifier_tree_snapshot = right.end_nullifier_tree_snapshot,
        .start_contract_tree_snapshot = left.start_contract_tree_snapshot,
        .end_contract_tree_snapshot = right.end_contract_tree_snapshot,
        .start_public_data_tree_root = left.start_public_data_tree_root,
        .end_public_data_tree_root = right.end_public_data_tree_root,
        .start_tree_of_historic_private_data_tree_roots_snapshot =
            left.constants.start_tree_of_historic_private_data_tree_roots_snapshot,
        .end_tree_of_historic_private_data_tree_roots_snapshot = end_tree_of_historic_private_data_tree_roots_snapshot,
        .start_tree_of_historic_contract_tree_roots_snapshot =
            left.constants.start_tree_of_historic_contract_tree_roots_snapshot,
        .end_tree_of_historic_contract_tree_roots_snapshot = end_tree_of_historic_contract_tree_roots_snapshot,
        .start_l1_to_l2_messages_tree_snapshot = rootRollupInputs.start_l1_to_l2_message_tree_snapshot,
        .end_l1_to_l2_messages_tree_snapshot = new_l1_to_l2_messages_tree_snapshot,
        .start_tree_of_historic_l1_to_l2_messages_tree_roots_snapshot =
            rootRollupInputs.start_historic_tree_l1_to_l2_message_tree_roots_snapshot,
        .end_tree_of_historic_l1_to_l2_messages_tree_roots_snapshot = end_l1_to_l2_data_roots_tree_snapshot,
        .calldata_hash = components::compute_calldata_hash(rootRollupInputs.previous_rollup_data),
        .l1_to_l2_messages_hash = compute_messages_hash(rootRollupInputs.l1_to_l2_messages)
    };

    return public_inputs;
}

}  // namespace aztec3::circuits::rollup::native_root_rollup