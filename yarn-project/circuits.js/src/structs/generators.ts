/**
 * Enumerate the hash_indices which are used for pedersen hashing (copied from cpp).
 * @see circuits/cpp/src/aztec3/constants.hpp
 */
export enum GeneratorIndex {
  COMMITMENT = 1,
  COMMITMENT_PLACEHOLDER,
  OUTER_COMMITMENT,
  NULLIFIER_HASHED_PRIVATE_KEY,
  NULLIFIER,
  INITIALISATION_NULLIFIER,
  OUTER_NULLIFIER,
  PUBLIC_DATA_READ,
  PUBLIC_DATA_UPDATE_REQUEST,
  VK,
  FUNCTION_DATA,
  FUNCTION_LEAF,
  CONTRACT_DEPLOYMENT_DATA,
  CONSTRUCTOR,
  CONSTRUCTOR_ARGS,
  CONTRACT_ADDRESS,
  CONTRACT_LEAF,
  CALL_CONTEXT,
  CALL_STACK_ITEM,
  CALL_STACK_ITEM_2,
  L2_TO_L1_MSG,
  PRIVATE_CIRCUIT_PUBLIC_INPUTS,
  PUBLIC_CIRCUIT_PUBLIC_INPUTS,
  TX_CONTEXT,
  TX_REQUEST,
  PUBLIC_LEAF_INDEX,
  PUBLIC_DATA_LEAF,
  SIGNED_TX_REQUEST,
  L1_TO_L2_MESSAGE_SECRET,
  FUNCTION_ARGS,
}
