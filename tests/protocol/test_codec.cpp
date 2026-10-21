// Codec fixtures: round trip, truncation, oversize, unknown version/type,
// header rules, value bounds.
#include "fixtures.h"

#include <cmath>
#include <limits>
#include <map>

namespace coop::test
{
namespace
{
const char* Err(const DecodeResult& r) { return to_string(r.error); }

// Overwrite bytes of a valid frame at an offset from the start of the body.
std::vector<std::uint8_t> Patch(std::vector<std::uint8_t> frame, std::size_t bodyOffset,
    const std::vector<std::uint8_t>& bytes)
{
    for (std::size_t i = 0; i < bytes.size(); ++i)
        frame[kHeaderBytes + bodyOffset + i] = bytes[i];
    return frame;
}

std::vector<std::uint8_t> F32(float v)
{
    std::uint32_t b;
    std::memcpy(&b, &v, 4);
    return {std::uint8_t(b), std::uint8_t(b >> 8), std::uint8_t(b >> 16), std::uint8_t(b >> 24)};
}

Fixture Find(const char* name)
{
    for (auto& f : AllFixtures())
        if (std::string(f.name) == name)
            return f;
    fail(__FILE__, __LINE__, std::string("no fixture ") + name);
    throw Abort{};
}
} // namespace

TEST("codec/layout")
{
    // The protocol's channel layout is a valid transport config, and every
    // message limit is the channel's.
    const net::TransportConfig c = transport_config();
    CHECK(net::validate(c).empty());
    REQUIRE(c.channels.size() == kChannelCount);
    for (std::size_t i = 0; i < kChannelCount; ++i)
        CHECK(c.channels[i].max_message_bytes == kMaxMessageBytes[i]);
    CHECK(c.channels[kSnapshot].delivery == net::Delivery::Transient);
    CHECK(c.channels[kInput].delivery == net::Delivery::Transient);
    CHECK(c.channels[kBulk].delivery == net::Delivery::Bulk);
    CHECK(c.channels[kControl].delivery == net::Delivery::Critical);
    // Every type has one channel and the variant order matches the types.
    for (const Fixture& f : AllFixtures())
    {
        const MessageType t = type_of(f.message);
        CHECK(known_type(std::uint16_t(t)));
        CHECK(channel_of(t) < kChannelCount);
    }
}

TEST("codec/round-trip")
{
    int n = 0;
    std::map<std::string, std::size_t> sizes;
    for (const Fixture& f : AllFixtures())
    {
        const auto bytes = Encode(f);
        REQUIRE_MSG(!bytes.empty(), "%s", f.name);
        const MessageType t = type_of(f.message);
        CHECK_MSG(bytes.size() <= kMaxMessageBytes[channel_of(t)], "%s is %zu bytes", f.name, bytes.size());
        Decoded d;
        const DecodeResult r = DecodeAs(t, bytes, &d);
        CHECK_MSG(r.ok(), "%s: %s at %s", f.name, Err(r), r.field);
        CHECK(d.header.type == std::uint16_t(t));
        CHECK(d.header.epoch == f.epoch && d.header.sequence == f.sequence);
        CHECK(d.body.index() == f.message.index());
        // Canonical: decoding then encoding gives the same bytes.
        std::vector<std::uint8_t> again;
        CHECK(encode(d.body, d.header.epoch, d.header.sequence, again));
        CHECK_MSG(again == bytes, "%s does not re-encode identically", f.name);
        sizes[f.name] = bytes.size();
        ++n;
    }
    CHECK(n == 22);
    for (auto& kv : sizes)
        metric("bytes_" + kv.first, double(kv.second));
    // Spot checks of decoded fields.
    Decoded d;
    REQUIRE(DecodeAs(MessageType::Spawn, Encode(Find("spawn")), &d).ok());
    const Spawn& s = std::get<Spawn>(d.body);
    CHECK(s.entity == (EntityRef{555, 2}) && s.archetype == "stalker_novice_1.ltx-a" && s.level == 3);
    CHECK(s.transform.x == -kMaxCoord && s.transform.yaw == -3.14159f && s.state.size() == kMaxSpawnState);
    REQUIRE(DecodeAs(MessageType::Hello, Encode(Find("hello-resume")), &d).ok());
    const Hello& h = std::get<Hello>(d.body);
    CHECK(h.resume && h.resume_request.token.size() == kMaxResumeToken && h.resume_request.last_snapshot == 88);
    REQUIRE(DecodeAs(MessageType::Snapshot, Encode(Find("snapshot")), &d).ok());
    const Snapshot& sn = std::get<Snapshot>(d.body);
    CHECK(sn.entities.size() == 23 && sn.entities[1].health == 10000 && sn.entities[1].vx == -kMaxSpeed);
    CHECK(sn.entities[0].components == kTransform && sn.entities[0].health == 0);
    CHECK(sn.snapshot_id == 4 && sn.baseline_id == 3 && sn.entities[1].qx == -0.5f && sn.entities[1].qw == 0.5f);
    CHECK(sn.entities[22].components == kRemoved && sn.entities[22].entity == (EntityRef{999, 7}));
    CHECK(sn.entities[1].aim_yaw == -3.f && sn.entities[1].aim_pitch == 1.2f); // v1.3
}

TEST("codec/truncation")
{
    // Every proper prefix of every fixture is refused, whether the header's
    // length is left as sent (BadLength) or rewritten to match (the body
    // itself runs out).
    int prefixes = 0;
    std::map<std::string, int> errors;
    for (const Fixture& f : AllFixtures())
    {
        const auto bytes = Encode(f);
        const MessageType t = type_of(f.message);
        // Big fixtures: every prefix of the first 600 bytes and of the last 600.
        for (std::size_t len = 0; len < bytes.size(); ++len)
        {
            if (len > 600 && len + 600 < bytes.size())
                continue;
            std::vector<std::uint8_t> cut(bytes.begin(), bytes.begin() + std::ptrdiff_t(len));
            DecodeResult r = DecodeAs(t, cut);
            CHECK_MSG(!r.ok(), "%s prefix %zu accepted", f.name, len);
            FixLength(cut);
            r = DecodeAs(t, cut);
            CHECK_MSG(!r.ok(), "%s prefix %zu (length fixed) accepted", f.name, len);
            ++errors[to_string(r.error)];
            ++prefixes;
        }
    }
    metric("prefixes", prefixes);
    for (auto& kv : errors)
        metric(std::string("error_") + kv.first, kv.second);
}

TEST("codec/trailing-and-length")
{
    for (const Fixture& f : AllFixtures())
    {
        auto bytes = Encode(f);
        const MessageType t = type_of(f.message);
        if (bytes.size() + 1 > kMaxMessageBytes[channel_of(t)])
            continue;
        bytes.push_back(0);
        CHECK_MSG(DecodeAs(t, bytes).error == DecodeError::BadLength, "%s +1 byte, old length", f.name);
        FixLength(bytes);
        CHECK_MSG(DecodeAs(t, bytes).error == DecodeError::TrailingBytes, "%s +1 byte", f.name);
    }
    // The whole message over its channel limit is refused before parsing.
    std::vector<std::uint8_t> big(kMaxMessageBytes[kInput] + 1, 0);
    Decoded d;
    CHECK(decode(kInput, Role::Server, big.data(), big.size(), d).error == DecodeError::TooLong);
}

TEST("codec/field-limits")
{
    // Lengths and counts one past their limit, written raw (encode() would
    // refuse them), are refused by decode before anything is allocated.
    const Hello h = std::get<Hello>(Find("hello").message);
    auto hello = [&](const std::string& name) {
        return Frame(std::uint16_t(MessageType::Hello), 0, 1, Body([&](Writer& w) {
            w.u32(kMagic);
            w.u16(1);
            w.u16(0);
            w.digest(h.build_id);
            w.digest(h.content_digest);
            w.uuid(h.player_id);
            w.str8(name);
            w.digest(h.client_nonce);
            w.u8(0);
        }));
    };
    CHECK(DecodeAs(MessageType::Hello, hello("ok")).ok());
    CHECK(DecodeAs(MessageType::Hello, hello(std::string(kMaxDisplayName + 1, 'a'))).error == DecodeError::TooLong);
    CHECK(DecodeAs(MessageType::Hello, hello("")).error == DecodeError::BadValue);
    CHECK(DecodeAs(MessageType::Hello, hello("tab\there")).error == DecodeError::BadString);
    // A length prefix claiming more than the message holds.
    auto lying = hello("ok");
    lying[kHeaderBytes + 88] = 200; // display-name length
    CHECK(DecodeAs(MessageType::Hello, lying).error == DecodeError::TooLong);
    lying[kHeaderBytes + 88] = 30;
    CHECK(DecodeAs(MessageType::Hello, lying).error == DecodeError::Truncated);

    auto intent = [&](std::size_t params, std::uint16_t kind) {
        return Frame(std::uint16_t(MessageType::ActionIntent), 5, 1, Body([&](Writer& w) {
            w.u64(1);
            w.u16(kind);
            w.u64(0);
            w.u32(0);
            w.u64(0);
            w.bytes16(std::vector<std::uint8_t>(params, 0));
        }));
    };
    CHECK(DecodeAs(MessageType::ActionIntent, intent(kMaxActionParams, 1)).ok());
    CHECK(DecodeAs(MessageType::ActionIntent, intent(kMaxActionParams + 1, 1)).error == DecodeError::TooLong);
    CHECK(DecodeAs(MessageType::ActionIntent, intent(0, 0)).error == DecodeError::BadValue);
    CHECK(DecodeAs(MessageType::ActionIntent, intent(0, kMaxActionKind + 1)).error == DecodeError::BadValue);

    // Counts: 9 inputs, 65 snapshot entities, and a count the bytes cannot hold.
    auto batch = [&](std::uint8_t count, std::size_t actual) {
        return Frame(std::uint16_t(MessageType::InputBatch), 5, 50, Body([&](Writer& w) {
            w.u8(count);
            for (std::size_t i = 0; i < actual; ++i)
            {
                w.u64(100 - i);
                for (int k = 0; k < 4; ++k)
                    w.f32(0.f);
                w.u32(0);
                w.u8(0);
            }
        }));
    };
    CHECK(DecodeAs(MessageType::InputBatch, batch(8, 8)).ok());
    CHECK(DecodeAs(MessageType::InputBatch, batch(9, 9)).error == DecodeError::TooLong);
    CHECK(DecodeAs(MessageType::InputBatch, batch(0, 0)).error == DecodeError::BadValue);
    CHECK(DecodeAs(MessageType::InputBatch, batch(8, 2)).error == DecodeError::Truncated);
    auto snap = [&](std::uint8_t count) {
        return Frame(std::uint16_t(MessageType::Snapshot), 5, 2, Body([&](Writer& w) {
            w.u64(1);
            w.u64(1);
            w.u64(0);
            w.u64(0);
            w.u8(0);
            w.u8(1);
            w.u8(count);
        }));
    };
    CHECK(DecodeAs(MessageType::Snapshot, snap(65)).error == DecodeError::TooLong);
    CHECK(DecodeAs(MessageType::Snapshot, snap(64)).error == DecodeError::Truncated); // no bytes behind the count

    // Spawn: archetype characters and length, state size.
    Spawn sp = std::get<Spawn>(Find("spawn").message);
    sp.state.clear();
    std::vector<std::uint8_t> out;
    sp.archetype = "Bad Name";
    CHECK(!encode(sp, 5, 1, out));
    sp.archetype = std::string(kMaxArchetype + 1, 'a');
    CHECK(!encode(sp, 5, 1, out));
    sp.archetype = "ok";
    sp.state.assign(kMaxSpawnState + 1, 0);
    std::string why;
    CHECK(!encode(sp, 5, 1, out, &why) && out.empty());
    note("encode refusal: " + why);
    // Resync: total over 256 MB, a chunk count that disagrees, a chunk CRC.
    ResyncBegin rb = std::get<ResyncBegin>(Find("resync-begin").message);
    rb.total_bytes = kMaxResyncBytes + 1;
    rb.chunk_count = std::uint32_t((rb.total_bytes + rb.chunk_bytes - 1) / rb.chunk_bytes);
    CHECK(!encode(rb, 5, 3, out));
    rb = std::get<ResyncBegin>(Find("resync-begin").message);
    rb.chunk_count = 4;
    CHECK(!encode(rb, 5, 3, out));
    auto chunk = Encode(Find("resync-chunk"));
    chunk[kHeaderBytes + 20] ^= 1; // first data byte
    CHECK(DecodeAs(MessageType::ResyncChunk, chunk).error == DecodeError::BadValue);
}

TEST("codec/versions")
{
    LocalBuild local;
    local.build_id = MakeDigest(1);
    local.content_digest = MakeDigest(2);
    Hello h = std::get<Hello>(Find("hello").message);
    std::uint16_t minor = 99;
    CHECK(check_hello(h, local, &minor) == 0 && minor == kProtocolMinor);
    h.minor = 7; // a newer minor client: the lower minor is used
    local.minor = 2;
    CHECK(check_hello(h, local, &minor) == 0 && minor == 2);
    // A different major is still decodable, so the server can say why.
    h.major = 2;
    std::vector<std::uint8_t> bytes;
    REQUIRE(encode(h, 0, 1, bytes));
    Decoded d;
    REQUIRE(DecodeAs(MessageType::Hello, bytes, &d).ok());
    CHECK(check_hello(std::get<Hello>(d.body), local, nullptr) == std::uint16_t(RejectReason::VersionMismatch));
    h.major = kProtocolMajor;
    h.build_id = MakeDigest(9);
    CHECK(check_hello(h, local, nullptr) == std::uint16_t(RejectReason::BuildMismatch));
    h.build_id = local.build_id;
    h.content_digest = MakeDigest(9);
    CHECK(check_hello(h, local, nullptr) == std::uint16_t(RejectReason::ContentMismatch));
    // Wrong magic.
    auto wrong = Encode(Find("hello"));
    wrong[kHeaderBytes] ^= 0xFF;
    CHECK(DecodeAs(MessageType::Hello, wrong).error == DecodeError::BadValue);
    // A client refuses a Welcome of another major.
    Welcome w = std::get<Welcome>(Find("welcome").message);
    auto wb = Encode(Find("welcome"));
    wb[kHeaderBytes] = 2;
    CHECK(DecodeAs(MessageType::Welcome, wb).error == DecodeError::BadValue);
    (void)w;
    // Reject keeps reason codes it does not know.
    REQUIRE(DecodeAs(MessageType::Reject, Encode(Find("reject-unknown-reason")), &d).ok());
    CHECK(std::get<Reject>(d.body).reason == 4242);
}

TEST("codec/unknown-type-and-header")
{
    const auto valid = Encode(Find("heartbeat"));
    Decoded d;
    auto as = [&](std::vector<std::uint8_t> b, net::ChannelId ch, Role receiver) {
        return decode(ch, receiver, b.data(), b.size(), d);
    };
    // Unknown type: an error; with the optional bit: skipped.
    auto b = valid;
    b[0] = 0x99;
    CHECK(as(b, kControl, Role::Server).error == DecodeError::UnknownType);
    b[1] = 0x80;
    const DecodeResult opt = as(b, kControl, Role::Server);
    CHECK(opt.ignorable && opt.error == DecodeError::None && !opt.ok());
    // Reserved flags.
    b = valid;
    b[2] = 1;
    CHECK(as(b, kControl, Role::Server).error == DecodeError::ReservedNonZero);
    // Its own channel only.
    CHECK(as(valid, kActions, Role::Server).error == DecodeError::WrongChannel);
    CHECK(as(valid, 9, Role::Server).error == DecodeError::WrongChannel);
    // Direction: a server never accepts server-only messages, nor a client
    // client-only ones; heartbeats go both ways.
    CHECK(as(valid, kControl, Role::Client).ok());
    CHECK(as(Encode(Find("welcome")), kControl, Role::Server).error == DecodeError::WrongDirection);
    CHECK(as(Encode(Find("spawn")), kLifecycle, Role::Server).error == DecodeError::WrongDirection);
    CHECK(as(Encode(Find("hello")), kControl, Role::Client).error == DecodeError::WrongDirection);
    CHECK(as(Encode(Find("input-batch")), kInput, Role::Client).error == DecodeError::WrongDirection);
    // Epoch: 0 exactly for handshake messages.
    CHECK(DecodeAs(MessageType::Heartbeat, Frame(4, 0, 1, Body([](Writer& w) {
        w.u64(1);
        w.u64(0);
        w.u64(0);
    }))).error == DecodeError::BadValue);
    auto hello = Encode(Find("hello"));
    hello[8] = 5; // epoch
    CHECK(DecodeAs(MessageType::Hello, hello).error == DecodeError::BadValue);
    // Sequence 0 is never valid.
    b = valid;
    for (int i = 16; i < 24; ++i)
        b[std::size_t(i)] = 0;
    CHECK(as(b, kControl, Role::Server).error == DecodeError::BadValue);
    // A header alone, and nothing at all.
    CHECK(DecodeAs(MessageType::Heartbeat, std::vector<std::uint8_t>(10, 0)).error == DecodeError::Truncated);
    CHECK(DecodeAs(MessageType::Heartbeat, {}).error == DecodeError::Truncated);
}

TEST("codec/values")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    // Spawn transform x at body offset 12 (ref) + 1 + archetype length + 2 (level).
    const Spawn sp = std::get<Spawn>(Find("spawn").message);
    const std::size_t xAt = 12 + 1 + sp.archetype.size() + 2;
    const auto spawn = Encode(Find("spawn"));
    CHECK(DecodeAs(MessageType::Spawn, Patch(spawn, xAt, F32(nan))).error == DecodeError::NotFinite);
    CHECK(DecodeAs(MessageType::Spawn, Patch(spawn, xAt, F32(-inf))).error == DecodeError::NotFinite);
    CHECK(DecodeAs(MessageType::Spawn, Patch(spawn, xAt, F32(kMaxCoord * 1.01f))).error == DecodeError::BadValue);
    CHECK(DecodeAs(MessageType::Spawn, Patch(spawn, xAt + 12, F32(4.5f))).error == DecodeError::BadValue); // yaw
    // Input axes and buttons (first input starts at body offset 1).
    const auto batch = Encode(Find("input-batch"));
    CHECK(DecodeAs(MessageType::InputBatch, Patch(batch, 1 + 8, F32(1.5f))).error == DecodeError::BadValue);
    CHECK(DecodeAs(MessageType::InputBatch, Patch(batch, 1 + 8, F32(nan))).error == DecodeError::NotFinite);
    CHECK(DecodeAs(MessageType::InputBatch, Patch(batch, 1 + 24, {0, 0, 1, 0})).error == DecodeError::ReservedNonZero);
    CHECK(DecodeAs(MessageType::InputBatch, Patch(batch, 1 + 28, {4})).error == DecodeError::BadValue); // stance
    CHECK(DecodeAs(MessageType::InputBatch, Patch(batch, 1 + 29, {244, 1, 0, 0, 0, 0, 0, 0})).error ==
          DecodeError::BadValue); // second input's tick 500 >= first's
    // Snapshot: unknown component bit, health over 10000, the same entity twice.
    Snapshot sn = std::get<Snapshot>(Find("snapshot").message);
    sn.entities[0].components = 0x80;
    std::vector<std::uint8_t> out;
    CHECK(!encode(sn, 5, 10, out));
    sn = std::get<Snapshot>(Find("snapshot").message);
    sn.entities[1].health = 10001;
    CHECK(!encode(sn, 5, 10, out));
    sn = std::get<Snapshot>(Find("snapshot").message);
    sn.entities[1].entity = sn.entities[0].entity;
    CHECK(!encode(sn, 5, 10, out));
    sn = std::get<Snapshot>(Find("snapshot").message);
    sn.snapshot_id = 3;
    CHECK(!encode(sn, 5, 10, out)); // baseline 3 is not before snapshot 3
    sn.snapshot_id = 0;
    sn.baseline_id = 0;
    CHECK(!encode(sn, 5, 10, out)); // v1.2: a snapshot has an id
    sn = std::get<Snapshot>(Find("snapshot").message);
    CHECK(encode(sn, 5, 2, out)); // v1.2: the baseline is a snapshot id, not a sequence
    sn.part = 2;
    CHECK(!encode(sn, 5, 10, out)); // part 2 of 2
    // v1.2: a removal carries nothing else; a rotation is a unit quaternion.
    sn = std::get<Snapshot>(Find("snapshot").message);
    sn.entities[22].components = kRemoved | kHealth;
    CHECK(!encode(sn, 5, 10, out));
    sn = std::get<Snapshot>(Find("snapshot").message);
    sn.entities[1].qw = 0.f;
    CHECK(!encode(sn, 5, 10, out));
    sn.entities[1].qw = 1.5f;
    CHECK(!encode(sn, 5, 10, out));
    CHECK(!encode(SnapshotAck{0, 0, 9}, 5, 1, out)); // acknowledges nothing, asks for nothing
    CHECK(encode(SnapshotAck{0, 4, 0}, 5, 1, out));
    // Identities: zero player, zero entity, half-set reference, entity its own parent.
    Hello h = std::get<Hello>(Find("hello").message);
    h.player_id = {};
    CHECK(!encode(h, 0, 1, out));
    Spawn s2 = sp;
    s2.entity = {};
    CHECK(!encode(s2, 5, 1, out));
    s2 = sp;
    s2.parent = {42, 0};
    CHECK(!encode(s2, 5, 1, out));
    s2 = sp;
    s2.parent = {sp.entity.id, 9};
    CHECK(!encode(s2, 5, 1, out));
    Welcome w = std::get<Welcome>(Find("welcome").message);
    w.slot_generation = 0;
    CHECK(!encode(w, 0, 1, out));
    w = std::get<Welcome>(Find("welcome").message);
    w.snapshot_rate_hz = 31;
    CHECK(!encode(w, 0, 1, out));
    // Action results: accepted with a reason, rejected without one.
    ActionResult ar = std::get<ActionResult>(Find("action-result").message);
    ar.reason = 0;
    CHECK(!encode(ar, 5, 1, out));
    ar.outcome = ActionOutcome::Accepted;
    CHECK(encode(ar, 5, 1, out));
    ar.reason = 1;
    CHECK(!encode(ar, 5, 1, out));
    // A resume flag other than 0/1.
    auto hello = Encode(Find("hello"));
    hello[hello.size() - 1] = 2;
    CHECK(DecodeAs(MessageType::Hello, hello).error == DecodeError::BadValue);
}
} // namespace coop::test
