import { Grumpkin } from '@aztec/circuits.js/barretenberg';
import { CircuitsWasm } from '@aztec/circuits.js';
import { randomBytes } from '@aztec/foundation/crypto';
import { decryptBuffer, deriveAESSecret, encryptBuffer } from './encrypt_buffer.js';
import { Point } from '@aztec/foundation/fields';

describe('encrypt buffer', () => {
  let grumpkin: Grumpkin;

  beforeAll(async () => {
    grumpkin = new Grumpkin(await CircuitsWasm.get());
  });

  it('derive shared secret', () => {
    const ownerPrivKey = randomBytes(32);
    const ownerPubKey = Point.fromBuffer(grumpkin.mul(Grumpkin.generator, ownerPrivKey));
    const ephPrivKey = randomBytes(32);
    const ephPubKey = Point.fromBuffer(grumpkin.mul(Grumpkin.generator, ephPrivKey));

    const secretBySender = deriveAESSecret(ownerPubKey, ephPrivKey, grumpkin);
    const secretByReceiver = deriveAESSecret(ephPubKey, ownerPrivKey, grumpkin);
    expect(secretBySender.toString('hex')).toEqual(secretByReceiver.toString('hex'));
  });

  it('convert to and from encrypted buffer', () => {
    const data = randomBytes(253);
    const ownerPrivKey = randomBytes(32);
    const ownerPubKey = Point.fromBuffer(grumpkin.mul(Grumpkin.generator, ownerPrivKey));
    const ephPrivKey = randomBytes(32);
    const encrypted = encryptBuffer(data, ownerPubKey, ephPrivKey, grumpkin);
    const decrypted = decryptBuffer(encrypted, ownerPrivKey, grumpkin);
    expect(decrypted).not.toBeUndefined();
    expect(decrypted).toEqual(data);
  });

  it('decrypting gibberish returns undefined', () => {
    const data = randomBytes(253);
    const ownerPrivKey = randomBytes(32);
    const ephPrivKey = randomBytes(32);
    const ownerPubKey = Point.fromBuffer(grumpkin.mul(Grumpkin.generator, ownerPrivKey));
    const encrypted = encryptBuffer(data, ownerPubKey, ephPrivKey, grumpkin);

    // Introduce gibberish.
    const gibberish = Buffer.concat([randomBytes(8), encrypted.subarray(8)]);

    const decrypted = decryptBuffer(gibberish, ownerPrivKey, grumpkin);
    expect(decrypted).toBeUndefined();
  });
});
