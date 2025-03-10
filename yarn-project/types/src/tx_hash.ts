import { deserializeBigInt, serializeBigInt } from '@aztec/foundation/serialize';
import { assertMemberLength } from '@aztec/circuits.js';

/**
 * A class representing hash of Aztec transaction.
 */
export class TxHash {
  /**
   * The size of the hash in bytes.
   */
  public static SIZE = 32;

  constructor(
    /**
     * The buffer containing the hash.
     */
    public buffer: Buffer,
  ) {
    assertMemberLength(this, 'buffer', TxHash.SIZE);
  }

  /**
   * Checks if this hash and another hash are equal.
   * @param hash - A hash to compare with.
   * @returns True if the hashes are equal, false otherwise.
   */
  public equals(hash: TxHash): boolean {
    return this.buffer.equals(hash.buffer);
  }

  /**
   * Convert this hash to a hex string.
   * @returns The hex string.
   */
  public toString() {
    return this.buffer.toString('hex');
  }
  /**
   * Convert this hash to a big int.
   * @returns The big int.
   */
  public toBigInt() {
    return deserializeBigInt(this.buffer, 0, TxHash.SIZE).elem;
  }
  /**
   * Creates a tx hash from a bigint.
   * @param hash - The tx hash as a big int.
   * @returns The TxHash.
   */
  public static fromBigInt(hash: bigint) {
    return new TxHash(serializeBigInt(hash, TxHash.SIZE));
  }
  /**
   * Converts this hash from a buffer of 28 bytes.
   * Verifies the input is 28 bytes.
   * @param buffer - The 28 byte buffer to construct from.
   * @returns A TxHash created from the input buffer with 4 bytes 0 padding at the front.
   */
  public static fromBuffer28(buffer: Buffer) {
    if (buffer.length != 28) {
      throw new Error(`Expected TxHash input buffer to be 28 bytes`);
    }
    const padded = Buffer.concat([Buffer.alloc(this.SIZE - 28), buffer]);
    return new TxHash(padded);
  }
}
