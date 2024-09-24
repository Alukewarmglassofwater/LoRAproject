#include <AES.h>
#include <AuthenticatedCipher.h>
#include <BigNumberUtil.h>
#include <BLAKE2b.h>
#include <BLAKE2s.h>
#include <BlockCipher.h>
#include <ChaCha.h>
#include <ChaChaPoly.h>
#include <Cipher.h>
#include <Crypto.h>
#include <CTR.h>
#include <Curve25519.h>
#include <EAX.h>
#include <Ed25519.h>
#include <GCM.h>
#include <GF128.h>
#include <GHASH.h>
#include <Hash.h>
#include <HKDF.h>
#include <KeccakCore.h>
#include <NoiseSource.h>
#include <OMAC.h>
#include <P521.h>
#include <Poly1305.h>
#include <RNG.h>
#include <SHA224.h>
#include <SHA256.h>
#include <SHA3.h>
#include <SHA384.h>
#include <SHA512.h>
#include <SHAKE.h>
#include <XOF.h>
#include <XTS.h>

const byte key[32] = {
  0x4f, 0xf6, 0x3b, 0x2c, 0x1a, 0x45, 0x89, 0x6d,
  0x7e, 0x9c, 0x00, 0x6f, 0x37, 0x11, 0x8a, 0x5b,
  0x9d, 0x74, 0x6e, 0xb2, 0x3c, 0xf0, 0x1d, 0x97,
  0x88, 0x6e, 0x3f, 0x52, 0xc8, 0xa0, 0x1d
};

void setup() {
    Serial.begin(9600);

    // HKDF parameters
    byte outputKey[32]; // Output key material for ChaCha20-Poly1305
    const char* info = "ChaCha20-Poly1305"; // Info for HKDF
    byte salt[32] = {0}; // Salt, should ideally be random or derived from a key material

    // Step 1: Extract
    byte prk[32]; // Pseudorandom key
    HMAC<SHA256> hmac;
    hmac.begin(salt, sizeof(salt));
    hmac.update(key, sizeof(key));
    hmac.end(prk, sizeof(prk));

    // Step 2: Expand
    size_t len = 0;
    size_t index = 1;

    while (len < sizeof(outputKey)) {
        byte block[32 + 1]; // Max block size for SHA256 + 1 byte for index

        hmac.begin(prk, sizeof(prk));
        hmac.update(block, len);
        hmac.update((byte*)&index, 1);
        hmac.update((byte*)info, strlen(info));
        hmac.end(block, sizeof(block));

        size_t toCopy = min(sizeof(outputKey) - len, sizeof(block));
        memcpy(outputKey + len, block, toCopy);
        len += toCopy;
        index++;
    }

    // Convert output to hex string
    char outputHex[65]; // 64 characters + null terminator
    for (size_t i = 0; i < 32; i++) {
        sprintf(outputHex + (i * 2), "%02x", outputKey[i]);
    }
    outputHex[64] = '\0'; // Null terminate

    Serial.print("Generated ChaCha20-Poly1305 Key: ");
    Serial.println(outputHex);
}

void loop() {
    // Nothing to do here
}
