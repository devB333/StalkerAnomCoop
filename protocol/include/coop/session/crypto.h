// Player credentials for S04-T04, on Windows' own cryptography (BCrypt:
// ECDSA P-256, SHA-256, the system RNG; DPAPI for the key file). BCrypt is
// already loaded by the GNS DLL (S04-T01), so nothing new ships.
//
// A private key never leaves PrivateKey except into its DPAPI-protected file;
// it is wiped on destruction and has no printable form.
#pragma once

#include "coop/proto/wire.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace coop::session
{
using PublicKey = std::array<std::uint8_t, 64>; // P-256 point X || Y
using Signature = std::array<std::uint8_t, 64>; // r || s

class PrivateKey
{
public:
    PrivateKey() = default;
    ~PrivateKey();
    PrivateKey(const PrivateKey&) = delete;
    PrivateKey& operator=(const PrivateKey&) = delete;
    PrivateKey(PrivateKey&& o) noexcept;
    PrivateKey& operator=(PrivateKey&& o) noexcept;
    bool empty() const { return blob_.empty(); }

private:
    friend bool generate_key(PrivateKey&, PublicKey&);
    friend bool sign(const PrivateKey&, const proto::Digest&, Signature&);
    friend bool save_key_file(const std::string&, const proto::Uuid128&, const PrivateKey&, std::string*);
    friend bool load_key_file(const std::string&, proto::Uuid128&, PrivateKey&, PublicKey&, std::string*);
    friend bool public_of(const PrivateKey&, PublicKey&);
    void wipe();
    std::vector<std::uint8_t> blob_; // BCRYPT_ECCPRIVATE_BLOB
};

bool random_bytes(void* out, std::size_t n);
proto::Digest sha256(const void* data, std::size_t n);

bool generate_key(PrivateKey& key, PublicKey& pub);
bool public_of(const PrivateKey& key, PublicKey& pub);
bool sign(const PrivateKey& key, const proto::Digest& hash, Signature& sig);
bool verify(const PublicKey& pub, const proto::Digest& hash, const Signature& sig);

// The player's key file: player id and private key, encrypted with DPAPI for
// the current Windows user (CryptProtectData), so it opens only for them.
bool save_key_file(const std::string& path, const proto::Uuid128& player, const PrivateKey& key, std::string* error);
bool load_key_file(const std::string& path, proto::Uuid128& player, PrivateKey& key, PublicKey& pub,
    std::string* error);

std::string to_hex(const std::uint8_t* p, std::size_t n);
bool from_hex(const std::string& s, std::uint8_t* out, std::size_t n);
std::string uuid_hex(const proto::Uuid128& u);
bool uuid_from_hex(const std::string& s, proto::Uuid128& u);
proto::Uuid128 random_uuid();
} // namespace coop::session
