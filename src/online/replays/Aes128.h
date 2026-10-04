#pragma once
// ONLINE (PC addition): portable AES-128 encryption (FIPS-197) for the replay upload. OpenSSL's AES
// is avoided because the bundled Android arm64 libcrypto can't be linked into the shared library
// (its assembly references OPENSSL_armcap_P with non-PIC relocations).

#include <cstddef>
#include <cstdint>

namespace online {
namespace replays {

class Aes128 {
public:
    explicit Aes128(const uint8_t key[16]);
    void encryptBlock(const uint8_t in[16], uint8_t out[16]) const;
    // CBC over `length` bytes (a multiple of 16); `iv` is updated like OpenSSL's AES_cbc_encrypt.
    void encryptCbc(const uint8_t* in, uint8_t* out, size_t length, uint8_t iv[16]) const;

private:
    uint8_t _roundKeys[176];
};

// 16 bytes from the OS's secure random source (for IVs).
bool secureRandom(uint8_t* out, size_t length);

}  // namespace replays
}  // namespace online
