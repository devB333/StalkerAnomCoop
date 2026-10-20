// Deterministic mutation fuzzing of decode(). Each input is copied so that it
// ends exactly at a no-access guard page: a read past the end faults and the
// run crashes (no sanitizer exists in the pinned toolchain, so this stands in
// for one). Properties checked on every input: decode returns; anything it
// accepts re-encodes to the same bytes (one canonical form, so no field was
// skipped or misread); with a stateful guard after it, nothing crashes.
#include "fixtures.h"

#include <windows.h>

#include <map>

namespace coop::test
{
namespace
{
class GuardedBuffer
{
public:
    explicit GuardedBuffer(std::size_t maxBytes)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        page_ = si.dwPageSize;
        dataPages_ = (maxBytes + page_ - 1) / page_;
        base_ = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, (dataPages_ + 1) * page_, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        DWORD old = 0;
        ok_ = base_ && VirtualProtect(base_ + dataPages_ * page_, page_, PAGE_NOACCESS, &old);
    }
    ~GuardedBuffer()
    {
        if (base_)
            VirtualFree(base_, 0, MEM_RELEASE);
    }
    bool ok() const { return ok_; }
    // The bytes, ending right before the guard page.
    const std::uint8_t* place(const std::vector<std::uint8_t>& v)
    {
        std::uint8_t* end = base_ + dataPages_ * page_;
        std::uint8_t* at = end - v.size();
        if (!v.empty())
            std::memcpy(at, v.data(), v.size());
        return at;
    }

private:
    std::size_t page_ = 4096, dataPages_ = 0;
    std::uint8_t* base_ = nullptr;
    bool ok_ = false;
};

struct Rng
{
    std::uint64_t s;
    std::uint64_t next()
    {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }
    std::size_t below(std::size_t n) { return n ? std::size_t(next() % n) : 0; }
};

std::vector<std::uint8_t> Mutate(std::vector<std::uint8_t> b, Rng& rng)
{
    const int edits = 1 + int(rng.below(4));
    for (int e = 0; e < edits; ++e)
    {
        switch (rng.below(9))
        {
        case 0: // flip a bit
            if (!b.empty())
                b[rng.below(b.size())] ^= std::uint8_t(1u << rng.below(8));
            break;
        case 1: // random byte
            if (!b.empty())
                b[rng.below(b.size())] = std::uint8_t(rng.next());
            break;
        case 2: // interesting byte
        {
            static const std::uint8_t v[] = {0, 1, 0x7F, 0x80, 0xFF, 0xFE, 64, 65, 9};
            if (!b.empty())
                b[rng.below(b.size())] = v[rng.below(sizeof(v))];
            break;
        }
        case 3: // truncate
            b.resize(rng.below(b.size() + 1));
            break;
        case 4: // extend
            for (std::size_t k = 1 + rng.below(16); k > 0; --k)
                b.push_back(std::uint8_t(rng.next()));
            break;
        case 5: // fix the header length to the new size (gets past the first check)
            FixLength(b);
            break;
        case 6: // a body byte that is probably a length or count
            if (b.size() > kHeaderBytes)
                b[kHeaderBytes + rng.below(b.size() - kHeaderBytes)] = std::uint8_t(rng.below(80));
            break;
        case 7: // a float slot: NaN/inf/huge
        {
            if (b.size() >= kHeaderBytes + 4)
            {
                static const std::uint32_t f[] = {0x7FC00000, 0x7F800000, 0xFF800000, 0x7F7FFFFF, 0x80000000};
                const std::size_t at = kHeaderBytes + rng.below(b.size() - kHeaderBytes - 3);
                const std::uint32_t v = f[rng.below(5)];
                for (int i = 0; i < 4; ++i)
                    b[at + std::size_t(i)] = std::uint8_t(v >> (8 * i));
            }
            break;
        }
        case 8: // erase a span
            if (b.size() > 1)
            {
                const std::size_t at = rng.below(b.size());
                const std::size_t n = 1 + rng.below(std::min<std::size_t>(8, b.size() - at));
                b.erase(b.begin() + std::ptrdiff_t(at), b.begin() + std::ptrdiff_t(at + n));
            }
            break;
        }
    }
    if (rng.below(2))
        FixLength(b);
    return b;
}
} // namespace

TEST("fuzz/decode-mutations")
{
    GuardedBuffer buf(kMaxMessageBytes[kBulk] + 64);
    REQUIRE(buf.ok());
    const auto fixtures = AllFixtures();
    std::vector<std::pair<MessageType, std::vector<std::uint8_t>>> seeds;
    for (const Fixture& f : fixtures)
        seeds.push_back({type_of(f.message), Encode(f)});
    Rng rng{0x9E3779B97F4A7C15ull};
    std::map<std::string, int> errors;
    int accepted = 0, nonCanonical = 0, guardViolations = 0, iterations = 0;
    const int kIterations = 2000000;
    for (int i = 0; i < kIterations; ++i)
    {
        const auto& seed = seeds[rng.below(seeds.size())];
        // Large seeds (the 60 KB chunk) are expensive; use them less often.
        if (seed.second.size() > 4096 && rng.below(16) != 0)
            continue;
        const std::vector<std::uint8_t> m = Mutate(seed.second, rng);
        // Also try it on a wrong channel or as the wrong receiver sometimes.
        net::ChannelId ch = channel_of(seed.first);
        Role receiver = ReceiverOf(seed.first);
        if (rng.below(10) == 0)
            ch = net::ChannelId(rng.below(kChannelCount + 1));
        if (rng.below(10) == 0)
            receiver = receiver == Role::Server ? Role::Client : Role::Server;
        const std::uint8_t* p = buf.place(m);
        Decoded d;
        const DecodeResult r = decode(ch, receiver, p, m.size(), d);
        ++iterations;
        if (r.ok())
        {
            ++accepted;
            std::vector<std::uint8_t> again;
            if (!encode(d.body, d.header.epoch, d.header.sequence, again) || again != m)
                ++nonCanonical;
            // The stateful guard must also take whatever decode accepted.
            StreamGuard g(receiver);
            g.begin_session(5);
            const GuardResult gr = g.check(ch, d.header);
            guardViolations += gr.verdict == Verdict::Violation ? 1 : 0;
        }
        else
            ++errors[r.ignorable ? "ignorable" : to_string(r.error)];
    }
    // Pure noise of every length up to a few hundred bytes.
    int noise = 0;
    for (int i = 0; i < 500000; ++i)
    {
        std::vector<std::uint8_t> m(rng.below(400));
        for (auto& x : m)
            x = std::uint8_t(rng.next());
        if (rng.below(2))
            FixLength(m);
        Decoded d;
        const DecodeResult r = decode(net::ChannelId(rng.below(kChannelCount)), rng.below(2) ? Role::Server : Role::Client,
            buf.place(m), m.size(), d);
        noise += r.ok() ? 1 : 0;
    }
    metric("iterations", iterations);
    metric("accepted_mutants", accepted);
    metric("non_canonical_accepted", nonCanonical);
    metric("noise_accepted", noise);
    for (auto& kv : errors)
        metric("rejected_" + kv.first, kv.second);
    note(fmt("guard verdict violation on %d accepted mutants (expected: sequence or epoch changes)", guardViolations));
    CHECK(nonCanonical == 0);
    CHECK(accepted > 0); // some mutations only touch free values; they must stay canonical
    CHECK(errors.size() >= 8); // many different rejection paths were reached
}
} // namespace coop::test
