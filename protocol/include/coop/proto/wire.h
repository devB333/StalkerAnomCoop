// Bounded little-endian reader and writer for the co-op wire format (S04-T03).
//
// The Reader never reads past its buffer: every read checks the remaining
// length first, the first failure is sticky (later reads return zero and do
// not move), and variable-length fields are checked against both their limit
// and the bytes actually left before anything is allocated.
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace coop::proto
{
enum class DecodeError : std::uint8_t
{
    None,
    Truncated,       // a field runs past the end
    TrailingBytes,   // bytes left after the last field
    TooLong,         // a length or count over its limit
    BadValue,        // enum, range or consistency check failed
    NotFinite,       // NaN or infinity
    BadString,       // characters outside the field's set
    ZeroIdentity,    // an identity that must be set is zero
    ReservedNonZero, // reserved bits or flags set
    UnknownType,     // message type this version does not know (and not optional)
    WrongChannel,    // message type on another channel than its own
    WrongDirection,  // e.g. a client sending a server-only message
    BadLength,       // header length disagrees with the message
};

const char* to_string(DecodeError e);

struct Uuid128
{
    std::uint64_t hi = 0;
    std::uint64_t lo = 0;
    bool zero() const { return hi == 0 && lo == 0; }
    bool operator==(const Uuid128& o) const { return hi == o.hi && lo == o.lo; }
    bool operator!=(const Uuid128& o) const { return !(*this == o); }
};

using Digest = std::array<std::uint8_t, 32>;

class Writer
{
public:
    explicit Writer(std::vector<std::uint8_t>& out) : out_(out) {}
    void u8(std::uint8_t v) { out_.push_back(v); }
    void u16(std::uint16_t v) { put(v, 2); }
    void u32(std::uint32_t v) { put(v, 4); }
    void u64(std::uint64_t v) { put(v, 8); }
    void f32(float v)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &v, 4);
        u32(bits);
    }
    void uuid(const Uuid128& v)
    {
        u64(v.hi);
        u64(v.lo);
    }
    void digest(const Digest& d) { out_.insert(out_.end(), d.begin(), d.end()); }
    void raw(const void* p, std::size_t n)
    {
        const auto* b = static_cast<const std::uint8_t*>(p);
        out_.insert(out_.end(), b, b + n);
    }
    // u8 length + bytes; u16 length + bytes; u32 length + bytes.
    void str8(const std::string& s)
    {
        u8(std::uint8_t(s.size()));
        raw(s.data(), s.size());
    }
    void bytes16(const std::vector<std::uint8_t>& b)
    {
        u16(std::uint16_t(b.size()));
        raw(b.data(), b.size());
    }
    void bytes32(const std::vector<std::uint8_t>& b)
    {
        u32(std::uint32_t(b.size()));
        raw(b.data(), b.size());
    }
    std::size_t size() const { return out_.size(); }
    // Overwrite a u32 written earlier (the header's payload length).
    void patch_u32(std::size_t at, std::uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            out_[at + std::size_t(i)] = std::uint8_t(v >> (8 * i));
    }

private:
    void put(std::uint64_t v, int n)
    {
        for (int i = 0; i < n; ++i)
            out_.push_back(std::uint8_t(v >> (8 * i)));
    }
    std::vector<std::uint8_t>& out_;
};

class Reader
{
public:
    Reader(const void* data, std::size_t size) : p_(static_cast<const std::uint8_t*>(data)), n_(size) {}

    bool ok() const { return err_ == DecodeError::None; }
    DecodeError error() const { return err_; }
    std::size_t error_offset() const { return errAt_; }
    const char* error_field() const { return errField_; }
    std::size_t offset() const { return at_; }
    std::size_t remaining() const { return n_ - at_; }

    // Names the field being read, for error reports.
    void field(const char* name) { field_ = name; }

    void fail(DecodeError e)
    {
        if (err_ == DecodeError::None)
        {
            err_ = e;
            errAt_ = at_;
            errField_ = field_;
        }
    }

    std::uint8_t u8() { return std::uint8_t(get(1)); }
    std::uint16_t u16() { return std::uint16_t(get(2)); }
    std::uint32_t u32() { return std::uint32_t(get(4)); }
    std::uint64_t u64() { return get(8); }

    // A finite float within [lo, hi].
    float f32(float lo, float hi)
    {
        const std::uint32_t bits = u32();
        float v;
        std::memcpy(&v, &bits, 4);
        if (!ok())
            return 0.f;
        if (!std::isfinite(v))
        {
            fail(DecodeError::NotFinite);
            return 0.f;
        }
        if (v < lo || v > hi)
        {
            fail(DecodeError::BadValue);
            return 0.f;
        }
        return v;
    }

    Uuid128 uuid(bool required)
    {
        Uuid128 u;
        u.hi = u64();
        u.lo = u64();
        if (ok() && required && u.zero())
            fail(DecodeError::ZeroIdentity);
        return u;
    }

    Digest digest()
    {
        Digest d{};
        if (take(32))
            std::memcpy(d.data(), p_ + at_ - 32, 32);
        return d;
    }

    // Length-prefixed string of [min, max] bytes; every byte must pass `allowed`.
    std::string str8(std::size_t min, std::size_t max, bool (*allowed)(char))
    {
        const std::size_t len = u8();
        std::string s;
        if (!checkLen(len, min, max))
            return s;
        s.assign(reinterpret_cast<const char*>(p_ + at_ - len), len);
        for (char c : s)
            if (!allowed(c))
            {
                fail(DecodeError::BadString);
                return {};
            }
        return s;
    }

    std::vector<std::uint8_t> bytes16(std::size_t min, std::size_t max) { return bytesN(u16(), min, max); }
    std::vector<std::uint8_t> bytes32(std::size_t min, std::size_t max) { return bytesN(u32(), min, max); }

    void expect_end()
    {
        if (ok() && at_ != n_)
            fail(DecodeError::TrailingBytes);
    }

private:
    bool take(std::size_t k)
    {
        if (!ok())
            return false;
        if (k > n_ - at_)
        {
            fail(DecodeError::Truncated);
            return false;
        }
        at_ += k;
        return true;
    }
    std::uint64_t get(int k)
    {
        if (!take(std::size_t(k)))
            return 0;
        std::uint64_t v = 0;
        for (int i = 0; i < k; ++i)
            v |= std::uint64_t(p_[at_ - std::size_t(k) + std::size_t(i)]) << (8 * i);
        return v;
    }
    // Checks a length before anything is allocated, then consumes it.
    bool checkLen(std::size_t len, std::size_t min, std::size_t max)
    {
        if (!ok())
            return false;
        if (len > max || len < min)
        {
            fail(len > max ? DecodeError::TooLong : DecodeError::BadValue);
            return false;
        }
        return take(len);
    }
    std::vector<std::uint8_t> bytesN(std::size_t len, std::size_t min, std::size_t max)
    {
        std::vector<std::uint8_t> b;
        if (checkLen(len, min, max))
            b.assign(p_ + at_ - len, p_ + at_);
        return b;
    }

    const std::uint8_t* p_;
    std::size_t n_;
    std::size_t at_ = 0;
    DecodeError err_ = DecodeError::None;
    std::size_t errAt_ = 0;
    const char* field_ = "";
    const char* errField_ = "";
};

std::uint32_t crc32(const void* data, std::size_t size, std::uint32_t seed = 0);

} // namespace coop::proto
