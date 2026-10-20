// BCrypt/DPAPI implementation of coop/session/crypto.h.
#include "coop/session/crypto.h"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

#include <cstring>
#include <fstream>
#include <iterator>

namespace coop::session
{
namespace
{
constexpr std::uint8_t kFileMagic[8] = {'C', 'O', 'O', 'P', 'K', 'E', 'Y', '1'};
constexpr std::size_t kPrivateBlobBytes = sizeof(BCRYPT_ECCKEY_BLOB) + 3 * 32;
constexpr std::size_t kPublicBlobBytes = sizeof(BCRYPT_ECCKEY_BLOB) + 2 * 32;

bool Ok(NTSTATUS s) { return s >= 0; }

struct Alg
{
    BCRYPT_ALG_HANDLE h = nullptr;
    explicit Alg(LPCWSTR id) { BCryptOpenAlgorithmProvider(&h, id, nullptr, 0); }
    ~Alg()
    {
        if (h)
            BCryptCloseAlgorithmProvider(h, 0);
    }
};

struct Key
{
    BCRYPT_KEY_HANDLE h = nullptr;
    ~Key()
    {
        if (h)
            BCryptDestroyKey(h);
    }
};

bool Fail(std::string* error, const char* what)
{
    if (error)
        *error = what;
    return false;
}
} // namespace

PrivateKey::~PrivateKey() { wipe(); }
PrivateKey::PrivateKey(PrivateKey&& o) noexcept : blob_(std::move(o.blob_)) { o.blob_.clear(); }
PrivateKey& PrivateKey::operator=(PrivateKey&& o) noexcept
{
    if (this != &o)
    {
        wipe();
        blob_ = std::move(o.blob_);
        o.blob_.clear();
    }
    return *this;
}
void PrivateKey::wipe()
{
    if (!blob_.empty())
        SecureZeroMemory(blob_.data(), blob_.size());
    blob_.clear();
}

bool random_bytes(void* out, std::size_t n)
{
    return Ok(BCryptGenRandom(nullptr, static_cast<PUCHAR>(out), ULONG(n), BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

proto::Digest sha256(const void* data, std::size_t n)
{
    proto::Digest d{};
    BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, static_cast<PUCHAR>(const_cast<void*>(data)), ULONG(n), d.data(),
        ULONG(d.size()));
    return d;
}

bool generate_key(PrivateKey& key, PublicKey& pub)
{
    Alg alg(BCRYPT_ECDSA_P256_ALGORITHM);
    Key k;
    if (!alg.h || !Ok(BCryptGenerateKeyPair(alg.h, &k.h, 256, 0)) || !Ok(BCryptFinalizeKeyPair(k.h, 0)))
        return false;
    std::vector<std::uint8_t> blob(kPrivateBlobBytes);
    ULONG size = 0;
    if (!Ok(BCryptExportKey(k.h, nullptr, BCRYPT_ECCPRIVATE_BLOB, blob.data(), ULONG(blob.size()), &size, 0)) ||
        size != kPrivateBlobBytes)
    {
        SecureZeroMemory(blob.data(), blob.size());
        return false;
    }
    key.wipe();
    key.blob_ = std::move(blob);
    return public_of(key, pub);
}

bool public_of(const PrivateKey& key, PublicKey& pub)
{
    if (key.blob_.size() != kPrivateBlobBytes)
        return false;
    std::memcpy(pub.data(), key.blob_.data() + sizeof(BCRYPT_ECCKEY_BLOB), 64); // X, Y follow the header
    return true;
}

bool sign(const PrivateKey& key, const proto::Digest& hash, Signature& sig)
{
    if (key.blob_.size() != kPrivateBlobBytes)
        return false;
    Alg alg(BCRYPT_ECDSA_P256_ALGORITHM);
    Key k;
    if (!alg.h || !Ok(BCryptImportKeyPair(alg.h, nullptr, BCRYPT_ECCPRIVATE_BLOB, &k.h,
                       const_cast<PUCHAR>(key.blob_.data()), ULONG(key.blob_.size()), 0)))
        return false;
    ULONG size = 0;
    return Ok(BCryptSignHash(k.h, nullptr, const_cast<PUCHAR>(hash.data()), ULONG(hash.size()), sig.data(),
               ULONG(sig.size()), &size, 0)) &&
           size == sig.size();
}

bool verify(const PublicKey& pub, const proto::Digest& hash, const Signature& sig)
{
    std::uint8_t blob[kPublicBlobBytes];
    auto* header = reinterpret_cast<BCRYPT_ECCKEY_BLOB*>(blob);
    header->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
    header->cbKey = 32;
    std::memcpy(blob + sizeof(BCRYPT_ECCKEY_BLOB), pub.data(), 64);
    Alg alg(BCRYPT_ECDSA_P256_ALGORITHM);
    Key k;
    // Import rejects a point that is not on the curve.
    if (!alg.h || !Ok(BCryptImportKeyPair(alg.h, nullptr, BCRYPT_ECCPUBLIC_BLOB, &k.h, blob, ULONG(sizeof(blob)), 0)))
        return false;
    return Ok(BCryptVerifySignature(k.h, nullptr, const_cast<PUCHAR>(hash.data()), ULONG(hash.size()),
        const_cast<PUCHAR>(sig.data()), ULONG(sig.size()), 0));
}

bool save_key_file(const std::string& path, const proto::Uuid128& player, const PrivateKey& key, std::string* error)
{
    if (key.blob_.size() != kPrivateBlobBytes || player.zero())
        return Fail(error, "no key or player id");
    std::vector<std::uint8_t> plain(kFileMagic, kFileMagic + 8);
    proto::Writer w(plain);
    w.uuid(player);
    w.raw(key.blob_.data(), key.blob_.size());
    DATA_BLOB in{DWORD(plain.size()), plain.data()};
    DATA_BLOB out{};
    const BOOL ok = CryptProtectData(&in, L"coop player key", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out);
    SecureZeroMemory(plain.data(), plain.size());
    if (!ok)
        return Fail(error, "CryptProtectData failed");
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return f.good() ? true : Fail(error, "cannot write key file");
}

bool load_key_file(const std::string& path, proto::Uuid128& player, PrivateKey& key, PublicKey& pub, std::string* error)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return Fail(error, "cannot open key file");
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.empty() || data.size() > 4096)
        return Fail(error, "key file has the wrong size");
    DATA_BLOB in{DWORD(data.size()), data.data()};
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return Fail(error, "key file does not open for this Windows user, or is damaged");
    bool ok = out.cbData == 8 + 16 + kPrivateBlobBytes && std::memcmp(out.pbData, kFileMagic, 8) == 0;
    if (ok)
    {
        proto::Reader r(out.pbData + 8, 16);
        player = r.uuid(true);
        ok = r.ok();
    }
    if (ok)
    {
        key.wipe();
        key.blob_.assign(out.pbData + 24, out.pbData + out.cbData);
        const auto* header = reinterpret_cast<const BCRYPT_ECCKEY_BLOB*>(key.blob_.data());
        ok = header->dwMagic == BCRYPT_ECDSA_PRIVATE_P256_MAGIC && header->cbKey == 32 && public_of(key, pub);
    }
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    if (!ok)
    {
        key.wipe();
        return Fail(error, "key file contents are not a player key");
    }
    return true;
}

std::string to_hex(const std::uint8_t* p, std::size_t n)
{
    static const char* d = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (std::size_t i = 0; i < n; ++i)
    {
        s += d[p[i] >> 4];
        s += d[p[i] & 15];
    }
    return s;
}

bool from_hex(const std::string& s, std::uint8_t* out, std::size_t n)
{
    if (s.size() != n * 2)
        return false;
    auto v = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < n; ++i)
    {
        const int hi = v(s[2 * i]), lo = v(s[2 * i + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out[i] = std::uint8_t(hi * 16 + lo);
    }
    return true;
}

std::string uuid_hex(const proto::Uuid128& u)
{
    std::uint8_t b[16];
    for (int i = 0; i < 8; ++i)
    {
        b[i] = std::uint8_t(u.hi >> (56 - 8 * i));
        b[8 + i] = std::uint8_t(u.lo >> (56 - 8 * i));
    }
    return to_hex(b, 16);
}

bool uuid_from_hex(const std::string& s, proto::Uuid128& u)
{
    std::uint8_t b[16];
    if (!from_hex(s, b, 16))
        return false;
    u = {};
    for (int i = 0; i < 8; ++i)
    {
        u.hi = (u.hi << 8) | b[i];
        u.lo = (u.lo << 8) | b[8 + i];
    }
    return !u.zero();
}

proto::Uuid128 random_uuid()
{
    proto::Uuid128 u;
    do
    {
        random_bytes(&u.hi, 8);
        random_bytes(&u.lo, 8);
    } while (u.zero());
    return u;
}
} // namespace coop::session
