#include "c_bind.h"

#include "call_stack_item.hpp"
#include "function_data.hpp"
#include "function_leaf_preimage.hpp"
#include "kernel_circuit_public_inputs.hpp"
#include "previous_kernel_data.hpp"
#include "private_circuit_public_inputs.hpp"
#include "tx_context.hpp"
#include "tx_request.hpp"
#include "private_kernel/private_kernel_inputs_inner.hpp"
#include "public_kernel/public_kernel_inputs.hpp"
#include "public_kernel/public_kernel_inputs_no_previous_kernel.hpp"
#include "rollup/base/base_or_merge_rollup_public_inputs.hpp"
#include "rollup/base/base_rollup_inputs.hpp"
#include "rollup/root/root_rollup_inputs.hpp"
#include "rollup/root/root_rollup_public_inputs.hpp"

#include "aztec3/circuits/abis/combined_accumulated_data.hpp"
#include "aztec3/circuits/abis/new_contract_data.hpp"
#include "aztec3/circuits/abis/private_kernel/private_kernel_inputs_init.hpp"
#include "aztec3/circuits/abis/signed_tx_request.hpp"
#include "aztec3/circuits/abis/types.hpp"
#include "aztec3/circuits/hash.hpp"
#include "aztec3/constants.hpp"
#include "aztec3/utils/types/native_types.hpp"

#include <barretenberg/barretenberg.hpp>

namespace {

using aztec3::circuits::compute_constructor_hash;
using aztec3::circuits::compute_contract_address;
using aztec3::circuits::abis::CallStackItem;
using aztec3::circuits::abis::FunctionData;
using aztec3::circuits::abis::FunctionLeafPreimage;
using aztec3::circuits::abis::NewContractData;
using aztec3::circuits::abis::SignedTxRequest;
using aztec3::circuits::abis::TxContext;
using aztec3::circuits::abis::TxRequest;
using NT = aztec3::utils::types::NativeTypes;
using aztec3::circuits::abis::PublicTypes;

// Cbind helper functions

/**
 * @brief Fill in zero-leaves to get a full tree's bottom layer.
 *
 * @details Given the a vector of nonzero leaves starting at the left,
 * append zeroleaves to that list until it represents a FULL set of leaves
 * for a tree of the given height.
 * **MODIFIES THE INPUT `leaves` REFERENCE!**
 *
 * @tparam TREE_HEIGHT height of the tree used to determine max leaves
 * @param leaves the nonzero leaves of the tree starting at the left
 * @param zero_leaf the leaf value to be used for any empty/unset leaves
 */
template <size_t TREE_HEIGHT> void rightfill_with_zeroleaves(std::vector<NT::fr>& leaves, NT::fr& zero_leaf)
{
    constexpr size_t max_leaves = 2 << (TREE_HEIGHT - 1);
    // input cant exceed max leaves
    // FIXME don't think asserts will show in wasm
    ASSERT(leaves.size() <= max_leaves);

    // fill in input vector with zero-leaves
    // to get a full bottom layer of the tree
    leaves.insert(leaves.end(), max_leaves - leaves.size(), zero_leaf);
}

}  // namespace

// Note: We don't have a simple way of calling the barretenberg c-bind.
// Mimick bbmalloc behaviour.
static void* bbmalloc(size_t size)
{
    auto* ptr = aligned_alloc(64, size);
    return ptr;
}

/** Copy this string to a bbmalloc'd buffer */
static const char* bbmalloc_copy_string(const char* data, size_t len)
{
    char* output_copy = static_cast<char*>(bbmalloc(len + 1));
    memcpy(output_copy, data, len + 1);
    return output_copy;
}

/**
 * For testing only. Take this object, write it to a buffer, then output it. */
template <typename T> static const char* as_string_output(uint8_t const* input_buf, uint32_t* size)
{
    T obj;
    read(input_buf, obj);
    std::ostringstream stream;
    stream << obj;
    std::string const str = stream.str();
    *size = static_cast<uint32_t>(str.size());
    return bbmalloc_copy_string(str.c_str(), *size);
}

/**
 * For testing only. Take this object, serialize it to a buffer, then output it. */
template <typename T> static const char* as_serialized_output(uint8_t const* input_buf, uint32_t* size)
{
    T obj;
    read(input_buf, obj);
    std::vector<uint8_t> stream;
    write(stream, obj);
    *size = static_cast<uint32_t>(stream.size());
    return bbmalloc_copy_string(reinterpret_cast<char*>(stream.data()), *size);
}

// WASM Cbinds
/**
 * @brief Hashes a TX request. This is a WASM-export that can be called from Typescript.
 *
 * @details given a `uint8_t*` buffer representing a full TX request,
 * read it into a `TxRequest` object, hash it to a `fr`,
 * and serialize it to a `uint8_t*` output buffer
 *
 * @param tx_request_buf buffer of bytes containing all data needed to construct a TX request via `read()`
 * @param output buffer that will contain the output which will be the hashed `TxRequest`
 */
WASM_EXPORT void abis__hash_tx_request(uint8_t const* tx_request_buf, uint8_t* output)
{
    TxRequest<NT> tx_request;
    read(tx_request_buf, tx_request);
    // TODO(dbanks12) consider using write() and read() instead of
    // serialize to/from everywhere here and in test
    NT::fr::serialize_to_buffer(tx_request.hash(), output);
}

/**
 * @brief Generates a function's "selector" from its "signature" using keccak256.
 * This is a WASM-export that can be called from Typescript.
 *
 * @details given a `char const*` c-string representing a "function signature",
 * hash using keccak and return its first 4 bytes (the "function selector")
 * by copying them into the `output` buffer arg. This is a workalike of
 * Ethereum/solidity's function selector computation....
 * Ethereum function selector is computed as follows:
 * `uint8_t* hash = keccak256(const char* func_sig);`
 * where func_sig does NOT include the trailing null character
 * And the resulting cstring for "transfer(address,uint256)" is:
 * `0xa9059cbb`
 * The 0th to 3rd bytes make up the function selector like:
 * where 0xa9 is hash[0], 05 is hash[1], 9c is hash[2], and bb is hash[3]
 *
 * @param func_sig_cstr c-string representing the function signature string like "transfer(uint256,address)"
 * @param output buffer that will contain the output which will be 4-byte function selector
 */
WASM_EXPORT void abis__compute_function_selector(char const* func_sig_cstr, uint8_t* output)
{
    // hash the function signature using keccak256
    auto keccak_hash = ethash_keccak256(reinterpret_cast<uint8_t const*>(func_sig_cstr), strlen(func_sig_cstr));
    // get a pointer to the start of the hash bytes
    auto const* hash_bytes = reinterpret_cast<uint8_t const*>(&keccak_hash.word64s[0]);
    // get the correct number of bytes from the hash and copy into output buffer
    std::copy_n(hash_bytes, aztec3::FUNCTION_SELECTOR_NUM_BYTES, output);
}

/**
 * @brief Hash/compress verification key data.
 * This is a WASM-export that can be called from Typescript.
 *
 * @details Pedersen compress VK to use later when computing function leaf
 * or constructor hash. Return the serialized results in the `output` buffer.
 *
 * @param vk_data_buf buffer of bytes representing serialized verification_key_data
 * @param output buffer that will contain the output. The serialized vk_hash.
 */
WASM_EXPORT void abis__hash_vk(uint8_t const* vk_data_buf, uint8_t* output)
{
    NT::VKData vk_data;
    read(vk_data_buf, vk_data);

    NT::fr::serialize_to_buffer(vk_data.compress_native(aztec3::GeneratorIndex::VK), output);
}

/**
 * @brief Generates a function tree leaf from its preimage.
 * This is a WASM-export that can be called from Typescript.
 *
 * @details given a `uint8_t const*` buffer representing a function leaf's prieimage,
 * construct a FunctionLeafPreimage instance, hash, and return the serialized results
 * in the `output` buffer.
 *
 * @param function_leaf_preimage_buf a buffer of bytes representing the function leaf's preimage
 * contents (`function_selector`, `is_private`, `vk_hash`, and `acir_hash`)
 * @param output buffer that will contain the output. The hashed and serialized function leaf.
 */
WASM_EXPORT void abis__compute_function_leaf(uint8_t const* function_leaf_preimage_buf, uint8_t* output)
{
    FunctionLeafPreimage<NT> leaf_preimage;
    read(function_leaf_preimage_buf, leaf_preimage);
    leaf_preimage.hash();
    NT::fr::serialize_to_buffer(leaf_preimage.hash(), output);
}

/**
 * @brief Compute a function tree root from its nonzero leaves.
 * This is a WASM-export that can be called from Typescript.
 *
 * @details given a serialized vector of nonzero function leaves,
 * compute the corresponding tree's root and return the
 * serialized results via `root_out` buffer.
 *
 * @param function_leaves_in input buffer representing a serialized vector of
 * nonzero function leaves where each leaf is an `fr` starting at the left of the tree
 * @param root_out buffer that will contain the serialized function tree root `fr`.
 */
WASM_EXPORT void abis__compute_function_tree_root(uint8_t const* function_leaves_in, uint8_t* root_out)
{
    std::vector<NT::fr> leaves;
    // fill in nonzero leaves to start
    read(function_leaves_in, leaves);
    // fill in zero leaves to complete tree
    NT::fr zero_leaf = FunctionLeafPreimage<NT>().hash();  // hash of empty/0 preimage
    rightfill_with_zeroleaves<aztec3::FUNCTION_TREE_HEIGHT>(leaves, zero_leaf);

    // compute the root of this complete tree, return
    NT::fr const root = plonk::stdlib::merkle_tree::compute_tree_root_native(leaves);

    // serialize and return root
    NT::fr::serialize_to_buffer(root, root_out);
}

/**
 * @brief Compute all of a function tree's nodes from its nonzero leaves.
 * This is a WASM-export that can be called from Typescript.
 *
 * @details given a serialized vector of nonzero function leaves,
 * compute ALL of the corresponding tree's nodes (including root) and return
 * the serialized results via `tree_nodes_out` buffer.
 *
 * @param function_leaves_in input buffer representing a serialized vector of
 * nonzero function leaves where each leaf is an `fr` starting at the left of the tree.
 * @param tree_nodes_out buffer that will contain the serialized function tree.
 * The 0th node is the bottom leftmost leaf. The last entry is the root.
 */
WASM_EXPORT void abis__compute_function_tree(uint8_t const* function_leaves_in, uint8_t* tree_nodes_out)
{
    std::vector<NT::fr> leaves;
    // fill in nonzero leaves to start
    read(function_leaves_in, leaves);
    // fill in zero leaves to complete tree
    NT::fr zero_leaf = FunctionLeafPreimage<NT>().hash();  // hash of empty/0 preimage
    rightfill_with_zeroleaves<aztec3::FUNCTION_TREE_HEIGHT>(leaves, zero_leaf);

    std::vector<NT::fr> const tree = plonk::stdlib::merkle_tree::compute_tree_native(leaves);

    // serialize and return tree
    write(tree_nodes_out, tree);
}

/**
 * @brief Hash some constructor info.
 * This is a WASM-export that can be called from Typescript.
 *
 * @details Hash constructor info to use later when deriving/generating contract address:
 * hash(function_signature_hash, args_hash, constructor_vk_hash)
 * Return the serialized results in the `output` buffer.
 *
 * @param function_data_buf function data struct but as a buffer of bytes
 * @param args_buf constructor args (array of fields) but as a buffer of bytes
 * @param constructor_vk_hash_buf constructor vk hashed to a field but as a buffer of bytes
 * @param output buffer that will contain the output. The serialized constructor_vk_hash.
 */
WASM_EXPORT void abis__hash_constructor(uint8_t const* function_data_buf,
                                        uint8_t const* args_hash_buf,
                                        uint8_t const* constructor_vk_hash_buf,
                                        uint8_t* output)
{
    FunctionData<NT> function_data;
    NT::fr args_hash;
    NT::fr constructor_vk_hash;

    read(function_data_buf, function_data);
    read(args_hash_buf, args_hash);
    read(constructor_vk_hash_buf, constructor_vk_hash);

    NT::fr const constructor_hash = compute_constructor_hash(function_data, args_hash, constructor_vk_hash);

    NT::fr::serialize_to_buffer(constructor_hash, output);
}

CBIND(abis__compute_contract_address, compute_contract_address<NT>);

/**
 * @brief Hash args for a function call.
 *
 * @param args_buf array of args (fields), with the length on the first position
 * @param output buffer that will contain the output
 */
WASM_EXPORT void abis__compute_var_args_hash(uint8_t const* args_buf, uint8_t* output)
{
    std::vector<NT::fr> args;
    read(args_buf, args);
    NT::fr const args_hash = aztec3::circuits::compute_var_args_hash<NT>(args);
    NT::fr::serialize_to_buffer(args_hash, output);
}

/**
 * @brief Generates a function tree leaf from its preimage.
 * This is a WASM-export that can be called from Typescript.
 *
 * @details given a `uint8_t const*` buffer representing a function leaf's prieimage,
 * construct a NewContractData instance, hash, and return the serialized results
 * in the `output` buffer.
 *
 * @param contract_leaf_preimage_buf a buffer of bytes representing the contract leaf's preimage
 * contents (`contract_address`, `portal_contract_address`, `function_tree_root`)
 * @param output buffer that will contain the output. The hashed and serialized contract leaf.
 */
WASM_EXPORT void abis__compute_contract_leaf(uint8_t const* contract_leaf_preimage_buf, uint8_t* output)
{
    NewContractData<NT> leaf_preimage;
    read(contract_leaf_preimage_buf, leaf_preimage);
    // as per the circuit implementation, if contract address == zero then return a zero leaf
    auto to_write = leaf_preimage.hash();
    NT::fr::serialize_to_buffer(to_write, output);
}

/**
 * @brief Generates a siloed commitment tree leaf from the contract and the commitment.
 */
CBIND(abis__silo_commitment, aztec3::circuits::silo_commitment<NT>);

/**
 * @brief Generates a signed tx request hash from it's pre-image
 * This is a WASM-export that can be called from Typescript.
 *
 * @details given a `uint8_t const*` buffer representing a signed tx request's pre-image,
 * construct a SignedTxRequest instance, hash, and return the serialized results
 * in the `output` buffer.
 *
 * @param signed_tx_request_buf a buffer of bytes representing the signed tx request
 * @param output buffer that will contain the output. The hashed and serialized signed tx request.
 */
WASM_EXPORT void abis__compute_transaction_hash(uint8_t const* signed_tx_request_buf, uint8_t* output)
{
    SignedTxRequest<NT> signed_tx_request_preimage;
    read(signed_tx_request_buf, signed_tx_request_preimage);
    auto to_write = signed_tx_request_preimage.hash();
    NT::fr::serialize_to_buffer(to_write, output);
}

WASM_EXPORT void abis__compute_call_stack_item_hash(uint8_t const* call_stack_item_buf, uint8_t* output)
{
    CallStackItem<NT, PublicTypes> call_stack_item;
    read(call_stack_item_buf, call_stack_item);
    NT::fr::serialize_to_buffer(get_call_stack_item_hash(call_stack_item), output);
}

/**
 * @brief Computes the hash of a message secret for use in l1 -> l2 messaging
 *
 * @param secret
 * @param output
 */
WASM_EXPORT void abis__compute_message_secret_hash(uint8_t const* secret, uint8_t* output)
{
    NT::fr message_secret;
    read(secret, message_secret);
    // TODO(sean): This is not using the generator correctly and is unsafe, update
    auto secret_hash = crypto::pedersen_commitment::compress_native(
        { aztec3::GeneratorIndex::L1_TO_L2_MESSAGE_SECRET, message_secret });
    NT::fr::serialize_to_buffer(secret_hash, output);
}

/* Typescript test helpers that call as_string_output() to stress serialization.
 * Each of these take an object buffer, and a string size pointer.
 * They return a string pointer (to be bbfree'd) and write to the string size pointer. */
WASM_EXPORT const char* abis__test_roundtrip_serialize_tx_context(uint8_t const* tx_context_buf, uint32_t* size)
{
    return as_string_output<TxContext<NT>>(tx_context_buf, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_tx_request(uint8_t const* tx_request_buf, uint32_t* size)
{
    return as_string_output<TxRequest<NT>>(tx_request_buf, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_call_context(uint8_t const* call_context_buf, uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::CallContext<NT>>(call_context_buf, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_private_circuit_public_inputs(
    uint8_t const* private_circuits_public_inputs_buf, uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::PrivateCircuitPublicInputs<NT>>(private_circuits_public_inputs_buf,
                                                                                    size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_function_data(uint8_t const* function_data_buf, uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::FunctionData<NT>>(function_data_buf, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_base_rollup_inputs(uint8_t const* rollup_inputs_buf,
                                                                          uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::BaseRollupInputs<NT>>(rollup_inputs_buf, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_previous_kernel_data(uint8_t const* kernel_data_buf,
                                                                            uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::PreviousKernelData<NT>>(kernel_data_buf, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_base_or_merge_rollup_public_inputs(
    uint8_t const* rollup_inputs_buf, uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::BaseOrMergeRollupPublicInputs<NT>>(rollup_inputs_buf, size);
}

WASM_EXPORT const char* abis__test_roundtrip_reserialize_base_or_merge_rollup_public_inputs(
    uint8_t const* rollup_inputs_buf, uint32_t* size)
{
    return as_serialized_output<aztec3::circuits::abis::BaseOrMergeRollupPublicInputs<NT>>(rollup_inputs_buf, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_root_rollup_inputs(uint8_t const* rollup_inputs_buf,
                                                                          uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::RootRollupInputs<NT>>(rollup_inputs_buf, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_root_rollup_public_inputs(uint8_t const* rollup_inputs_buf,
                                                                                 uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::RootRollupPublicInputs<NT>>(rollup_inputs_buf, size);
}

WASM_EXPORT const char* abis__test_roundtrip_reserialize_root_rollup_public_inputs(uint8_t const* rollup_inputs_buf,
                                                                                   uint32_t* size)
{
    return as_serialized_output<aztec3::circuits::abis::RootRollupPublicInputs<NT>>(rollup_inputs_buf, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_combined_accumulated_data(uint8_t const* input, uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::CombinedAccumulatedData<NT>>(input, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_signature(uint8_t const* input, uint32_t* size)
{
    return as_string_output<NT::ecdsa_signature>(input, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_signed_tx_request(uint8_t const* input, uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::SignedTxRequest<NT>>(input, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_private_kernel_inputs_inner(uint8_t const* input, uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::private_kernel::PrivateKernelInputsInner<NT>>(input, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_private_kernel_inputs_init(uint8_t const* input, uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::private_kernel::PrivateKernelInputsInit<NT>>(input, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_kernel_circuit_public_inputs(uint8_t const* input,
                                                                                    uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::KernelCircuitPublicInputs<NT>>(input, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_public_kernel_inputs(uint8_t const* input, uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::public_kernel::PublicKernelInputs<NT>>(input, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_public_kernel_inputs_no_previous_kernel(uint8_t const* input,
                                                                                               uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::public_kernel::PublicKernelInputsNoPreviousKernel<NT>>(input, size);
}

WASM_EXPORT const char* abis__test_roundtrip_serialize_function_leaf_preimage(uint8_t const* function_leaf_preimage_buf,
                                                                              uint32_t* size)
{
    return as_string_output<aztec3::circuits::abis::FunctionLeafPreimage<NT>>(function_leaf_preimage_buf, size);
}
