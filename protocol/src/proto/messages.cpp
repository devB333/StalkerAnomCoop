// Co-op protocol v1 codec. See messages.h and docs/network-protocol.md.
#include "coop/proto/messages.h"

#include <set>
#include <type_traits>

namespace coop::proto
{
const char* to_string(DecodeError e)
{
    switch (e)
    {
    case DecodeError::None: return "none";
    case DecodeError::Truncated: return "truncated";
    case DecodeError::TrailingBytes: return "trailing_bytes";
    case DecodeError::TooLong: return "too_long";
    case DecodeError::BadValue: return "bad_value";
    case DecodeError::NotFinite: return "not_finite";
    case DecodeError::BadString: return "bad_string";
    case DecodeError::ZeroIdentity: return "zero_identity";
    case DecodeError::ReservedNonZero: return "reserved_nonzero";
    case DecodeError::UnknownType: return "unknown_type";
    case DecodeError::WrongChannel: return "wrong_channel";
    case DecodeError::WrongDirection: return "wrong_direction";
    case DecodeError::BadLength: return "bad_length";
    }
    return "?";
}

std::uint32_t crc32(const void* data, std::size_t size, std::uint32_t seed)
{
    static const auto table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i)
        {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t c = ~seed;
    const auto* p = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i)
        c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}

net::TransportConfig transport_config()
{
    using net::Delivery;
    net::TransportConfig c;
    c.channels = {
        {"control", Delivery::Critical, 0, 1, std::uint32_t(kMaxMessageBytes[kControl]), 64 * 1024, 0},
        {"actions", Delivery::Critical, 0, 1, std::uint32_t(kMaxMessageBytes[kActions]), 64 * 1024, 0},
        {"lifecycle", Delivery::Critical, 0, 1, std::uint32_t(kMaxMessageBytes[kLifecycle]), 1024 * 1024, 0},
        {"bulk", Delivery::Bulk, 2, 1, std::uint32_t(kMaxMessageBytes[kBulk]), 192 * 1024, 0},
        {"snapshot", Delivery::Transient, 1, 1, std::uint32_t(kMaxMessageBytes[kSnapshot]), 16 * 1024, 50},
        {"input", Delivery::Transient, 0, 1, std::uint32_t(kMaxMessageBytes[kInput]), 8 * 1024, 50},
    };
    // v1.1: a burst of objects switching online is ~100 spawns of a few KiB.
    c.send_buffer_bytes = 2 * 1024 * 1024;
    c.recv_buffer_bytes = 4 * 1024 * 1024;
    return c;
}

namespace
{
struct TypeInfo
{
    MessageType type;
    const char* name;
    net::ChannelId channel;
    int sender; // 0 client, 1 server, 2 both
};

constexpr TypeInfo kTypes[] = {
    {MessageType::Hello, "hello", kControl, 0},
    {MessageType::Welcome, "welcome", kControl, 1},
    {MessageType::AuthChallenge, "auth_challenge", kControl, 1},
    {MessageType::AuthProof, "auth_proof", kControl, 0},
    {MessageType::Reject, "reject", kControl, 1},
    {MessageType::Heartbeat, "heartbeat", kControl, 2},
    {MessageType::ResyncBegin, "resync_begin", kControl, 1},
    {MessageType::ResyncEnd, "resync_end", kControl, 1},
    {MessageType::ActionIntent, "action_intent", kActions, 0},
    {MessageType::ActionResult, "action_result", kActions, 1},
    {MessageType::Spawn, "spawn", kLifecycle, 1},
    {MessageType::Despawn, "despawn", kLifecycle, 1},
    {MessageType::CampaignEvent, "campaign_event", kLifecycle, 1},
    {MessageType::ResyncChunk, "resync_chunk", kBulk, 1},
    {MessageType::Snapshot, "snapshot", kSnapshot, 1},
    {MessageType::SnapshotAck, "snapshot_ack", kSnapshot, 0},
    {MessageType::InputBatch, "input_batch", kInput, 0},
};

const TypeInfo* info(std::uint16_t type)
{
    for (const TypeInfo& t : kTypes)
        if (std::uint16_t(t.type) == type)
            return &t;
    return nullptr;
}

bool NameChar(char c) { return c >= 0x20 && c < 0x7F; } // printable ASCII
bool ArchetypeChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
}

// ---- field groups shared by several messages
void WriteRef(Writer& w, const EntityRef& r)
{
    w.u64(r.id);
    w.u32(r.generation);
}
// required: both parts non-zero. Otherwise none (both zero) is also allowed.
EntityRef ReadRef(Reader& r, bool required)
{
    EntityRef e;
    e.id = r.u64();
    e.generation = r.u32();
    if (!r.ok())
        return e;
    if (e.none())
    {
        if (required)
            r.fail(DecodeError::ZeroIdentity);
    }
    else if (e.id == 0 || e.generation == 0)
        r.fail(DecodeError::ZeroIdentity); // half set
    return e;
}
void WriteTransform(Writer& w, const Transform& t)
{
    w.f32(t.x);
    w.f32(t.y);
    w.f32(t.z);
    w.f32(t.yaw);
}
Transform ReadTransform(Reader& r)
{
    Transform t;
    t.x = r.f32(-kMaxCoord, kMaxCoord);
    t.y = r.f32(-kMaxCoord, kMaxCoord);
    t.z = r.f32(-kMaxCoord, kMaxCoord);
    t.yaw = r.f32(-kMaxAngle, kMaxAngle);
    return t;
}
std::uint64_t NonZero64(Reader& r)
{
    const std::uint64_t v = r.u64();
    if (r.ok() && v == 0)
        r.fail(DecodeError::ZeroIdentity);
    return v;
}

// ---- per message: write / read
void Write(Writer& w, const Hello& m)
{
    w.u32(kMagic);
    w.u16(m.major);
    w.u16(m.minor);
    w.digest(m.build_id);
    w.digest(m.content_digest);
    w.uuid(m.player_id);
    w.str8(m.display_name);
    w.digest(m.client_nonce);
    w.u8(m.resume ? 1 : 0);
    if (m.resume)
    {
        w.uuid(m.resume_request.session_id);
        w.str8(std::string(m.resume_request.token.begin(), m.resume_request.token.end()));
        w.u64(m.resume_request.last_event);
        w.u64(m.resume_request.last_snapshot);
    }
}
void Read(Reader& r, Hello& m)
{
    r.field("hello.magic");
    if (r.u32() != kMagic && r.ok())
        r.fail(DecodeError::BadValue);
    r.field("hello.version");
    m.major = r.u16();
    m.minor = r.u16();
    // A different major is still decoded, so the server can answer it with
    // a VersionMismatch reject; the layout of Hello is frozen across majors.
    r.field("hello.build_id");
    m.build_id = r.digest();
    m.content_digest = r.digest();
    r.field("hello.player_id");
    m.player_id = r.uuid(true);
    r.field("hello.display_name");
    m.display_name = r.str8(1, kMaxDisplayName, NameChar);
    r.field("hello.client_nonce");
    m.client_nonce = r.digest();
    r.field("hello.resume");
    const std::uint8_t resume = r.u8();
    if (r.ok() && resume > 1)
        r.fail(DecodeError::BadValue);
    m.resume = resume == 1;
    if (m.resume)
    {
        m.resume_request.session_id = r.uuid(true);
        r.field("hello.resume.token");
        const std::size_t len = r.u8();
        if (r.ok() && (len < kMinResumeToken || len > kMaxResumeToken))
            r.fail(len > kMaxResumeToken ? DecodeError::TooLong : DecodeError::BadValue);
        if (r.ok() && len > r.remaining())
            r.fail(DecodeError::Truncated);
        if (r.ok())
        {
            // Re-read as raw bytes (the token is binary, str8 checks characters).
            m.resume_request.token.resize(len);
            for (std::size_t i = 0; i < len; ++i)
                m.resume_request.token[i] = r.u8();
        }
        m.resume_request.last_event = r.u64();
        m.resume_request.last_snapshot = r.u64();
    }
}

void Write(Writer& w, const AuthChallenge& m)
{
    w.digest(m.server_nonce);
    w.uuid(m.campaign_id);
}
void Read(Reader& r, AuthChallenge& m)
{
    r.field("auth_challenge");
    m.server_nonce = r.digest();
    m.campaign_id = r.uuid(true);
}

void Write(Writer& w, const AuthProof& m) { w.raw(m.signature.data(), m.signature.size()); }
void Read(Reader& r, AuthProof& m)
{
    r.field("auth_proof.signature");
    for (auto& b : m.signature)
        b = r.u8();
}

void Write(Writer& w, const Welcome& m)
{
    w.u16(m.major);
    w.u16(m.minor);
    w.uuid(m.campaign_id);
    w.uuid(m.session_id);
    w.uuid(m.player_id);
    w.uuid(m.party_id);
    w.u8(m.slot);
    w.u32(m.slot_generation);
    w.u64(m.authority_epoch);
    w.u64(m.server_tick);
    w.u16(m.tick_rate_hz);
    w.u16(m.snapshot_rate_hz);
    w.u8(std::uint8_t(m.resume_token.size()));
    w.raw(m.resume_token.data(), m.resume_token.size());
}
void Read(Reader& r, Welcome& m)
{
    r.field("welcome.version");
    m.major = r.u16();
    m.minor = r.u16();
    if (r.ok() && m.major != kProtocolMajor)
        r.fail(DecodeError::BadValue);
    r.field("welcome.ids");
    m.campaign_id = r.uuid(true);
    m.session_id = r.uuid(true);
    m.player_id = r.uuid(true);
    m.party_id = r.uuid(true);
    r.field("welcome.slot");
    m.slot = r.u8();
    if (r.ok() && m.slot >= kMaxSlots)
        r.fail(DecodeError::BadValue);
    m.slot_generation = r.u32();
    if (r.ok() && m.slot_generation == 0)
        r.fail(DecodeError::ZeroIdentity);
    r.field("welcome.epoch");
    m.authority_epoch = NonZero64(r);
    m.server_tick = r.u64();
    r.field("welcome.rates");
    m.tick_rate_hz = r.u16();
    m.snapshot_rate_hz = r.u16();
    if (r.ok() && (m.tick_rate_hz < 1 || m.tick_rate_hz > 240 || m.snapshot_rate_hz < 1 ||
                      m.snapshot_rate_hz > m.tick_rate_hz))
        r.fail(DecodeError::BadValue);
    r.field("welcome.resume_token");
    const std::size_t len = r.u8();
    if (r.ok() && (len < kMinResumeToken || len > kMaxResumeToken))
        r.fail(len > kMaxResumeToken ? DecodeError::TooLong : DecodeError::BadValue);
    if (r.ok() && len > r.remaining())
        r.fail(DecodeError::Truncated);
    if (r.ok())
    {
        m.resume_token.resize(len);
        for (std::size_t i = 0; i < len; ++i)
            m.resume_token[i] = r.u8();
    }
}

void Write(Writer& w, const Reject& m)
{
    w.u16(m.reason);
    w.u16(m.server_major);
    w.u16(m.server_minor);
    w.str8(m.detail);
}
void Read(Reader& r, Reject& m)
{
    r.field("reject.reason");
    m.reason = r.u16();
    if (r.ok() && m.reason == 0)
        r.fail(DecodeError::BadValue);
    m.server_major = r.u16();
    m.server_minor = r.u16();
    r.field("reject.detail");
    m.detail = r.str8(0, kMaxRejectDetail, NameChar);
}

void Write(Writer& w, const Heartbeat& m)
{
    w.u64(m.nonce);
    w.u64(m.echo);
    w.u64(m.sent_tick);
}
void Read(Reader& r, Heartbeat& m)
{
    r.field("heartbeat");
    m.nonce = NonZero64(r);
    m.echo = r.u64();
    m.sent_tick = r.u64();
}

void Write(Writer& w, const ResyncBegin& m)
{
    w.u64(m.resync_id);
    w.u64(m.checkpoint_id);
    w.u64(m.authority_epoch);
    w.digest(m.content_hash);
    w.u64(m.event_watermark);
    w.u64(m.total_bytes);
    w.u32(m.chunk_bytes);
    w.u32(m.chunk_count);
}
void Read(Reader& r, ResyncBegin& m)
{
    r.field("resync_begin.ids");
    m.resync_id = NonZero64(r);
    m.checkpoint_id = NonZero64(r);
    m.authority_epoch = NonZero64(r);
    m.content_hash = r.digest();
    m.event_watermark = r.u64();
    r.field("resync_begin.sizes");
    m.total_bytes = r.u64();
    m.chunk_bytes = r.u32();
    m.chunk_count = r.u32();
    if (!r.ok())
        return;
    if (m.total_bytes == 0 || m.total_bytes > kMaxResyncBytes || m.chunk_bytes == 0 || m.chunk_bytes > kMaxResyncChunk)
        r.fail(DecodeError::TooLong);
    else if (std::uint64_t(m.chunk_count) != (m.total_bytes + m.chunk_bytes - 1) / m.chunk_bytes)
        r.fail(DecodeError::BadValue);
    else if (m.chunk_count > kMaxResyncChunks)
        r.fail(DecodeError::TooLong);
}

void Write(Writer& w, const ResyncEnd& m)
{
    w.u64(m.resync_id);
    w.u32(m.chunk_count);
    w.u32(m.total_crc32);
}
void Read(Reader& r, ResyncEnd& m)
{
    r.field("resync_end");
    m.resync_id = NonZero64(r);
    m.chunk_count = r.u32();
    if (r.ok() && (m.chunk_count == 0 || m.chunk_count > kMaxResyncChunks))
        r.fail(DecodeError::BadValue);
    m.total_crc32 = r.u32();
}

void Write(Writer& w, const ActionIntent& m)
{
    w.u64(m.operation_id);
    w.u16(m.kind);
    WriteRef(w, m.target);
    w.u64(m.expected_revision);
    w.bytes16(m.params);
}
void Read(Reader& r, ActionIntent& m)
{
    r.field("action_intent.operation_id");
    m.operation_id = NonZero64(r);
    r.field("action_intent.kind");
    m.kind = r.u16();
    if (r.ok() && (m.kind == 0 || m.kind > kMaxActionKind))
        r.fail(DecodeError::BadValue);
    r.field("action_intent.target");
    m.target = ReadRef(r, false);
    m.expected_revision = r.u64();
    r.field("action_intent.params");
    m.params = r.bytes16(0, kMaxActionParams);
}

void Write(Writer& w, const ActionResult& m)
{
    w.u64(m.operation_id);
    w.u8(std::uint8_t(m.outcome));
    w.u16(m.reason);
    w.u64(m.commit_id);
    w.u64(m.server_tick);
    w.u64(m.revision);
}
void Read(Reader& r, ActionResult& m)
{
    r.field("action_result.operation_id");
    m.operation_id = NonZero64(r);
    r.field("action_result.outcome");
    const std::uint8_t o = r.u8();
    if (r.ok() && (o < 1 || o > 3))
        r.fail(DecodeError::BadValue);
    m.outcome = ActionOutcome(o);
    m.reason = r.u16();
    // An accepted action has no reason; a rejected one must say why.
    if (r.ok() && ((m.outcome == ActionOutcome::Accepted) != (m.reason == 0)) && m.outcome != ActionOutcome::Duplicate)
        r.fail(DecodeError::BadValue);
    m.commit_id = r.u64();
    m.server_tick = r.u64();
    m.revision = r.u64();
}

void Write(Writer& w, const Spawn& m)
{
    WriteRef(w, m.entity);
    w.str8(m.archetype);
    w.u16(m.level);
    WriteTransform(w, m.transform);
    WriteRef(w, m.parent);
    w.u64(m.commit_id);
    w.bytes16(m.state);
}
void Read(Reader& r, Spawn& m)
{
    r.field("spawn.entity");
    m.entity = ReadRef(r, true);
    r.field("spawn.archetype");
    m.archetype = r.str8(1, kMaxArchetype, ArchetypeChar);
    m.level = r.u16();
    r.field("spawn.transform");
    m.transform = ReadTransform(r);
    r.field("spawn.parent");
    m.parent = ReadRef(r, false);
    if (r.ok() && !m.parent.none() && m.parent.id == m.entity.id)
        r.fail(DecodeError::BadValue);
    m.commit_id = r.u64();
    r.field("spawn.state");
    m.state = r.bytes16(0, kMaxSpawnState);
}

void Write(Writer& w, const Despawn& m)
{
    WriteRef(w, m.entity);
    w.u8(std::uint8_t(m.reason));
    w.u64(m.commit_id);
}
void Read(Reader& r, Despawn& m)
{
    r.field("despawn.entity");
    m.entity = ReadRef(r, true);
    r.field("despawn.reason");
    const std::uint8_t reason = r.u8();
    if (r.ok() && (reason < 1 || reason > 3))
        r.fail(DecodeError::BadValue);
    m.reason = DespawnReason(reason);
    m.commit_id = r.u64();
}

void Write(Writer& w, const CampaignEvent& m)
{
    w.u64(m.event_id);
    w.u16(m.kind);
    w.u64(m.revision);
    w.bytes16(m.payload);
}
void Read(Reader& r, CampaignEvent& m)
{
    r.field("campaign_event");
    m.event_id = NonZero64(r);
    m.kind = r.u16();
    if (r.ok() && m.kind == 0)
        r.fail(DecodeError::BadValue);
    m.revision = r.u64();
    r.field("campaign_event.payload");
    m.payload = r.bytes16(0, kMaxEventPayload);
}

void Write(Writer& w, const ResyncChunk& m)
{
    w.u64(m.resync_id);
    w.u32(m.index);
    w.u32(crc32(m.data.data(), m.data.size()));
    w.bytes32(m.data);
}
void Read(Reader& r, ResyncChunk& m)
{
    r.field("resync_chunk");
    m.resync_id = NonZero64(r);
    m.index = r.u32();
    const std::uint32_t crc = r.u32();
    r.field("resync_chunk.data");
    m.data = r.bytes32(1, kMaxResyncChunk);
    if (r.ok() && crc32(m.data.data(), m.data.size()) != crc)
        r.fail(DecodeError::BadValue);
}

void Write(Writer& w, const Snapshot& m)
{
    w.u64(m.server_tick);
    w.u64(m.baseline_id);
    w.u64(m.last_input);
    w.u8(m.part);
    w.u8(m.part_count);
    w.u8(std::uint8_t(m.entities.size()));
    for (const SnapshotEntity& e : m.entities)
    {
        WriteRef(w, e.entity);
        w.u16(e.components);
        if (e.components & kTransform)
            WriteTransform(w, e.transform);
        if (e.components & kVelocity)
        {
            w.f32(e.vx);
            w.f32(e.vy);
            w.f32(e.vz);
        }
        if (e.components & kHealth)
            w.u16(e.health);
        if (e.components & kStateBits)
            w.u16(e.state_bits);
    }
}
void Read(Reader& r, Snapshot& m, std::uint64_t sequence)
{
    r.field("snapshot.header");
    m.server_tick = r.u64();
    m.baseline_id = r.u64();
    if (r.ok() && m.baseline_id >= sequence)
        r.fail(DecodeError::BadValue); // a baseline is an earlier snapshot
    m.last_input = r.u64();
    m.part = r.u8();
    m.part_count = r.u8();
    if (r.ok() && (m.part_count < 1 || m.part_count > kMaxSnapshotParts || m.part >= m.part_count))
        r.fail(DecodeError::BadValue);
    r.field("snapshot.count");
    const std::size_t count = r.u8();
    if (r.ok() && count > kMaxSnapshotEntities)
        r.fail(DecodeError::TooLong);
    // Each entry is at least 14 bytes: refuse a count the bytes cannot hold
    // before reserving anything.
    if (r.ok() && count * 14 > r.remaining())
        r.fail(DecodeError::Truncated);
    if (!r.ok())
        return;
    m.entities.reserve(count);
    std::set<std::uint64_t> seen;
    for (std::size_t i = 0; i < count && r.ok(); ++i)
    {
        SnapshotEntity e;
        r.field("snapshot.entity");
        e.entity = ReadRef(r, true);
        if (r.ok() && !seen.insert(e.entity.id).second)
            r.fail(DecodeError::BadValue); // the same entity twice
        r.field("snapshot.components");
        e.components = r.u16();
        if (r.ok() && (e.components & ~std::uint16_t(kKnownComponents)))
            r.fail(DecodeError::ReservedNonZero);
        if (e.components & kTransform)
            e.transform = ReadTransform(r);
        if (e.components & kVelocity)
        {
            r.field("snapshot.velocity");
            e.vx = r.f32(-kMaxSpeed, kMaxSpeed);
            e.vy = r.f32(-kMaxSpeed, kMaxSpeed);
            e.vz = r.f32(-kMaxSpeed, kMaxSpeed);
        }
        if (e.components & kHealth)
        {
            r.field("snapshot.health");
            e.health = r.u16();
            if (r.ok() && e.health > 10000)
                r.fail(DecodeError::BadValue);
        }
        if (e.components & kStateBits)
            e.state_bits = r.u16();
        m.entities.push_back(e);
    }
}

void Write(Writer& w, const SnapshotAck& m)
{
    w.u64(m.snapshot_id);
    w.u64(m.baseline_id);
    w.u64(m.event_cursor);
}
void Read(Reader& r, SnapshotAck& m)
{
    r.field("snapshot_ack");
    m.snapshot_id = NonZero64(r);
    m.baseline_id = r.u64();
    if (r.ok() && m.baseline_id > m.snapshot_id)
        r.fail(DecodeError::BadValue);
    m.event_cursor = r.u64();
}

void Write(Writer& w, const InputBatch& m)
{
    w.u8(std::uint8_t(m.inputs.size()));
    for (const Input& in : m.inputs)
    {
        w.u64(in.tick);
        w.f32(in.move_x);
        w.f32(in.move_y);
        w.f32(in.yaw);
        w.f32(in.pitch);
        w.u32(in.buttons);
        w.u8(std::uint8_t(in.stance));
    }
}
void Read(Reader& r, InputBatch& m, std::uint64_t sequence)
{
    r.field("input_batch.count");
    const std::size_t count = r.u8();
    if (r.ok() && (count < 1 || count > kMaxInputsPerBatch))
        r.fail(count < 1 ? DecodeError::BadValue : DecodeError::TooLong);
    if (r.ok() && count > sequence)
        r.fail(DecodeError::BadValue); // sequences would go below 1
    if (r.ok() && count * 29 > r.remaining()) // each input is 29 bytes
        r.fail(DecodeError::Truncated);
    if (!r.ok())
        return;
    m.inputs.reserve(count);
    for (std::size_t i = 0; i < count && r.ok(); ++i)
    {
        Input in;
        r.field("input.tick");
        in.tick = r.u64();
        if (r.ok() && i > 0 && in.tick >= m.inputs.back().tick)
            r.fail(DecodeError::BadValue); // newest first, strictly decreasing
        r.field("input.axes");
        in.move_x = r.f32(-1.f, 1.f);
        in.move_y = r.f32(-1.f, 1.f);
        in.yaw = r.f32(-kMaxAngle, kMaxAngle);
        in.pitch = r.f32(-kMaxAngle / 2, kMaxAngle / 2);
        r.field("input.buttons");
        in.buttons = r.u32();
        if (r.ok() && (in.buttons & ~kKnownButtons))
            r.fail(DecodeError::ReservedNonZero);
        r.field("input.stance");
        const std::uint8_t s = r.u8();
        if (r.ok() && s > 3)
            r.fail(DecodeError::BadValue);
        in.stance = Stance(s);
        m.inputs.push_back(in);
    }
}

template <class T>
bool ReadBody(Reader& r, std::uint16_t type, std::uint64_t sequence, Message& out)
{
    T m;
    if constexpr (std::is_same_v<T, Snapshot> || std::is_same_v<T, InputBatch>)
        Read(r, m, sequence);
    else
    {
        (void)sequence;
        Read(r, m);
    }
    (void)type;
    out = std::move(m);
    return r.ok();
}
} // namespace

bool known_type(std::uint16_t type) { return info(type) != nullptr; }

bool handshake_type(std::uint16_t type)
{
    return type == std::uint16_t(MessageType::Hello) || type == std::uint16_t(MessageType::Welcome) ||
           type == std::uint16_t(MessageType::Reject) || type == std::uint16_t(MessageType::AuthChallenge) ||
           type == std::uint16_t(MessageType::AuthProof);
}

net::ChannelId channel_of(MessageType type)
{
    const TypeInfo* t = info(std::uint16_t(type));
    return t ? t->channel : net::ChannelId(0xFF);
}

Role sender_of(MessageType type, bool* both)
{
    const TypeInfo* t = info(std::uint16_t(type));
    if (both)
        *both = t && t->sender == 2;
    return t && t->sender == 1 ? Role::Server : Role::Client;
}

const char* to_string(MessageType type)
{
    const TypeInfo* t = info(std::uint16_t(type));
    return t ? t->name : "unknown";
}

MessageType type_of(const Message& m)
{
    static constexpr MessageType order[] = {MessageType::Hello, MessageType::Welcome, MessageType::Reject,
        MessageType::Heartbeat, MessageType::ResyncBegin, MessageType::ResyncEnd, MessageType::ActionIntent,
        MessageType::ActionResult, MessageType::Spawn, MessageType::Despawn, MessageType::CampaignEvent,
        MessageType::ResyncChunk, MessageType::Snapshot, MessageType::SnapshotAck, MessageType::InputBatch,
        MessageType::AuthChallenge, MessageType::AuthProof};
    static_assert(std::variant_size_v<Message> == sizeof(order) / sizeof(order[0]));
    return order[m.index()];
}

DecodeResult decode(net::ChannelId channel, Role receiver, const void* data, std::size_t size, Decoded& out)
{
    DecodeResult res;
    Reader r(data, size);
    auto done = [&](DecodeError forced = DecodeError::None) {
        if (forced != DecodeError::None)
            r.fail(forced);
        res.error = r.error();
        res.offset = r.error_offset();
        res.field = r.error_field();
        return res;
    };
    if (channel >= kChannelCount)
        return done(DecodeError::WrongChannel);
    if (size > kMaxMessageBytes[channel])
        return done(DecodeError::TooLong);
    r.field("header");
    Header& h = out.header;
    h.type = r.u16();
    h.flags = r.u16();
    h.payload_bytes = r.u32();
    h.epoch = r.u64();
    h.sequence = r.u64();
    if (!r.ok())
        return done();
    if (h.payload_bytes != r.remaining())
        return done(DecodeError::BadLength);
    if (h.sequence == 0)
        return done(DecodeError::BadValue);
    const TypeInfo* t = info(h.type);
    if (!t)
    {
        if (h.type & kOptionalTypeBit)
        {
            res.ignorable = true; // a newer minor's optional message
            return res;
        }
        return done(DecodeError::UnknownType);
    }
    if (h.flags != 0)
        return done(DecodeError::ReservedNonZero);
    if (t->channel != channel)
        return done(DecodeError::WrongChannel);
    const bool fromServer = receiver == Role::Client;
    if (t->sender != 2 && (t->sender == 1) != fromServer)
        return done(DecodeError::WrongDirection);
    // Before the session exists (Hello, Welcome, Reject) the epoch is 0;
    // every other message carries the session's epoch.
    if (handshake_type(h.type) != (h.epoch == 0))
        return done(DecodeError::BadValue);

    switch (t->type)
    {
    case MessageType::Hello: ReadBody<Hello>(r, h.type, h.sequence, out.body); break;
    case MessageType::Welcome: ReadBody<Welcome>(r, h.type, h.sequence, out.body); break;
    case MessageType::Reject: ReadBody<Reject>(r, h.type, h.sequence, out.body); break;
    case MessageType::Heartbeat: ReadBody<Heartbeat>(r, h.type, h.sequence, out.body); break;
    case MessageType::ResyncBegin: ReadBody<ResyncBegin>(r, h.type, h.sequence, out.body); break;
    case MessageType::ResyncEnd: ReadBody<ResyncEnd>(r, h.type, h.sequence, out.body); break;
    case MessageType::ActionIntent: ReadBody<ActionIntent>(r, h.type, h.sequence, out.body); break;
    case MessageType::ActionResult: ReadBody<ActionResult>(r, h.type, h.sequence, out.body); break;
    case MessageType::Spawn: ReadBody<Spawn>(r, h.type, h.sequence, out.body); break;
    case MessageType::Despawn: ReadBody<Despawn>(r, h.type, h.sequence, out.body); break;
    case MessageType::CampaignEvent: ReadBody<CampaignEvent>(r, h.type, h.sequence, out.body); break;
    case MessageType::ResyncChunk: ReadBody<ResyncChunk>(r, h.type, h.sequence, out.body); break;
    case MessageType::Snapshot: ReadBody<Snapshot>(r, h.type, h.sequence, out.body); break;
    case MessageType::SnapshotAck: ReadBody<SnapshotAck>(r, h.type, h.sequence, out.body); break;
    case MessageType::InputBatch: ReadBody<InputBatch>(r, h.type, h.sequence, out.body); break;
    case MessageType::AuthChallenge: ReadBody<AuthChallenge>(r, h.type, h.sequence, out.body); break;
    case MessageType::AuthProof: ReadBody<AuthProof>(r, h.type, h.sequence, out.body); break;
    }
    r.expect_end();
    return done();
}

bool encode(const Message& m, std::uint64_t epoch, std::uint64_t sequence, std::vector<std::uint8_t>& out,
    std::string* error)
{
    out.clear();
    const MessageType type = type_of(m);
    // Lengths that do not fit their prefix would be truncated by the writer;
    // refuse them before writing.
    bool fits = true;
    std::visit(
        [&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Hello>)
                fits = v.display_name.size() <= 255 && v.resume_request.token.size() <= 255;
            else if constexpr (std::is_same_v<T, Welcome>)
                fits = v.resume_token.size() <= 255;
            else if constexpr (std::is_same_v<T, Reject>)
                fits = v.detail.size() <= 255;
            else if constexpr (std::is_same_v<T, ActionIntent>)
                fits = v.params.size() <= 0xFFFF;
            else if constexpr (std::is_same_v<T, Spawn>)
                fits = v.archetype.size() <= 255 && v.state.size() <= 0xFFFF;
            else if constexpr (std::is_same_v<T, CampaignEvent>)
                fits = v.payload.size() <= 0xFFFF;
            else if constexpr (std::is_same_v<T, Snapshot>)
                fits = v.entities.size() <= 255;
            else if constexpr (std::is_same_v<T, InputBatch>)
                fits = v.inputs.size() <= 255;
        },
        m);
    if (!fits)
    {
        if (error)
            *error = std::string(to_string(type)) + ": a field is longer than its length prefix";
        return false;
    }
    Writer w(out);
    w.u16(std::uint16_t(type));
    w.u16(0);
    w.u32(0); // payload length, patched below
    w.u64(epoch);
    w.u64(sequence);
    std::visit([&](const auto& v) { Write(w, v); }, m);
    w.patch_u32(4, std::uint32_t(out.size() - kHeaderBytes)); // after type and flags
    // Never send what the receiver would refuse.
    const Role receiver = sender_of(type) == Role::Server ? Role::Client : Role::Server;
    Decoded check;
    const DecodeResult res = decode(channel_of(type), receiver, out.data(), out.size(), check);
    if (!res.ok())
    {
        if (error)
            *error = std::string(to_string(type)) + ": " + to_string(res.error) + " at " + res.field;
        out.clear();
        return false;
    }
    return true;
}

std::uint16_t check_hello(const Hello& hello, const LocalBuild& local, std::uint16_t* negotiated_minor)
{
    if (hello.major != local.major)
        return std::uint16_t(RejectReason::VersionMismatch);
    if (hello.build_id != local.build_id)
        return std::uint16_t(RejectReason::BuildMismatch);
    if (hello.content_digest != local.content_digest)
        return std::uint16_t(RejectReason::ContentMismatch);
    if (negotiated_minor)
        *negotiated_minor = hello.minor < local.minor ? hello.minor : local.minor;
    return 0;
}

} // namespace coop::proto
