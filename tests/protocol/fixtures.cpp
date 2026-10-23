#include "fixtures.h"

namespace coop::test
{
Digest MakeDigest(std::uint8_t seed)
{
    Digest d{};
    for (std::size_t i = 0; i < d.size(); ++i)
        d[i] = std::uint8_t(seed * 17 + i);
    return d;
}

Uuid128 MakeUuid(std::uint64_t seed)
{
    return {0x1000000000000000ull + seed, 0x2000000000000000ull + seed * 3};
}

std::vector<Fixture> AllFixtures()
{
    std::vector<Fixture> f;

    Hello h;
    h.build_id = MakeDigest(1);
    h.content_digest = MakeDigest(2);
    h.player_id = MakeUuid(1);
    h.display_name = "Player One";
    h.client_nonce = MakeDigest(5);
    f.push_back({"hello", h, 0, 1});
    Hello hr = h;
    hr.display_name = std::string(kMaxDisplayName, 'x');
    hr.resume = true;
    hr.resume_request.session_id = MakeUuid(9);
    hr.resume_request.token = std::vector<std::uint8_t>(kMaxResumeToken, 0x00); // binary, zeros allowed
    hr.resume_request.last_event = 77;
    hr.resume_request.last_snapshot = 88;
    f.push_back({"hello-resume", hr, 0, 1});

    Welcome w;
    w.campaign_id = MakeUuid(2);
    w.session_id = MakeUuid(3);
    w.player_id = MakeUuid(1);
    w.party_id = MakeUuid(4);
    w.slot = 1;
    w.slot_generation = 3;
    w.authority_epoch = 5;
    w.server_tick = 123456;
    w.resume_token = std::vector<std::uint8_t>(32, 0x3C);
    f.push_back({"welcome", w, 0, 1});

    AuthChallenge ch;
    ch.server_nonce = MakeDigest(6);
    ch.campaign_id = MakeUuid(2);
    f.push_back({"auth-challenge", ch, 0, 2});
    AuthProof pr;
    for (std::size_t i = 0; i < pr.signature.size(); ++i)
        pr.signature[i] = std::uint8_t(0xF0 ^ i);
    f.push_back({"auth-proof", pr, 0, 2});

    Reject rj;
    rj.reason = std::uint16_t(RejectReason::ContentMismatch);
    rj.detail = "content digest differs";
    f.push_back({"reject", rj, 0, 1});
    Reject rj2;
    rj2.reason = 4242; // a reason a newer server might send
    f.push_back({"reject-unknown-reason", rj2, 0, 1});

    f.push_back({"heartbeat", Heartbeat{11, 10, 999}, 5, 2});

    ResyncBegin rb;
    rb.resync_id = 1;
    rb.checkpoint_id = 9;
    rb.authority_epoch = 6;
    rb.content_hash = MakeDigest(3);
    rb.event_watermark = 400;
    rb.total_bytes = 150000;
    rb.chunk_bytes = kMaxResyncChunk;
    rb.chunk_count = 3;
    f.push_back({"resync-begin", rb, 5, 3});
    f.push_back({"resync-end", ResyncEnd{1, 3, 0xDEADBEEF}, 5, 4});

    ActionIntent ai;
    ai.operation_id = 1001;
    ai.kind = 7;
    ai.target = {555, 2};
    ai.expected_revision = 12;
    ai.params = std::vector<std::uint8_t>(kMaxActionParams, 1);
    f.push_back({"action-intent", ai, 5, 1});
    ActionIntent ai2;
    ai2.operation_id = 1002;
    ai2.kind = kMaxActionKind;
    f.push_back({"action-intent-no-target", ai2, 5, 2});

    ActionResult ar;
    ar.operation_id = 1001;
    ar.outcome = ActionOutcome::Rejected;
    ar.reason = 3;
    ar.server_tick = 1000;
    f.push_back({"action-result", ar, 5, 1});

    Spawn sp;
    sp.entity = {555, 2};
    sp.archetype = "stalker_novice_1.ltx-a";
    sp.level = 3;
    sp.transform = {-kMaxCoord, 12.5f, kMaxCoord, -3.14159f};
    sp.parent = {42, 1};
    sp.commit_id = 8;
    sp.state = std::vector<std::uint8_t>(kMaxSpawnState, 0x77);
    f.push_back({"spawn", sp, 5, 1});
    f.push_back({"despawn", Despawn{{555, 2}, DespawnReason::OutOfRange, 9}, 5, 2});
    // v1.6 (S08-T01): an item taken into an inventory, and one put on the ground.
    f.push_back({"reparent", Reparent{{556, 3}, {42, 1}, {1.f, 2.f, 3.f, 0.25f}, 10}, 5, 3});
    f.push_back({"reparent-ground", Reparent{{556, 3}, {}, {-kMaxCoord, 0.f, kMaxCoord, -3.14159f}, 11}, 5, 4});
    CampaignEvent ce;
    ce.event_id = 31;
    ce.kind = 2;
    ce.revision = 4;
    ce.payload = std::vector<std::uint8_t>(kMaxEventPayload, 9);
    f.push_back({"campaign-event", ce, 5, 3});

    ResyncChunk rc;
    rc.resync_id = 1;
    rc.index = 0;
    rc.data = std::vector<std::uint8_t>(kMaxResyncChunk, 0x42);
    f.push_back({"resync-chunk", rc, 5, 1});

    Snapshot sn;
    sn.server_tick = 2000;
    sn.snapshot_id = 4;
    sn.baseline_id = 3;
    sn.last_input = 50;
    sn.part = 1;
    sn.part_count = 2;
    for (std::uint32_t i = 0; i < 20; ++i)
    {
        SnapshotEntity e;
        e.entity = {1000 + i, 1 + i};
        e.components = std::uint16_t(i % 2 ? kStateComponents : kTransform);
        e.transform = {float(i), 2.f, 3.f, 0.5f};
        e.vx = -kMaxSpeed;
        e.vy = 1.f;
        e.vz = kMaxSpeed;
        e.health = 10000;
        e.state_bits = 0xFFFF;
        e.qx = -0.5f;
        e.qy = 0.5f;
        e.qz = -0.5f;
        e.qw = 0.5f;
        e.aim_yaw = -3.f;
        e.aim_pitch = 1.2f;
        e.weapon_slot = 2;
        e.weapon_state = 5;
        e.ammo = 8;
        e.shots = 65535;
        sn.entities.push_back(e);
    }
    SnapshotEntity removed;
    removed.entity = {999, 7};
    removed.components = kRemoved;
    sn.entities.push_back(removed);
    f.push_back({"snapshot", sn, 5, 10});
    Snapshot sn0;
    sn0.snapshot_id = 1;
    f.push_back({"snapshot-empty-full", sn0, 5, 1});
    f.push_back({"snapshot-ack", SnapshotAck{10, 3, 400}, 5, 7});
    f.push_back({"snapshot-ack-missing", SnapshotAck{0, 3, 0}, 5, 8});

    InputBatch ib;
    for (int i = 0; i < int(kMaxInputsPerBatch); ++i)
    {
        Input in;
        in.tick = 500 - std::uint64_t(i);
        in.move_x = -1.f;
        in.move_y = 1.f;
        in.yaw = 3.1f;
        in.pitch = -1.5f;
        in.buttons = kKnownButtons;
        in.stance = Stance::Sprint;
        ib.inputs.push_back(in);
    }
    f.push_back({"input-batch", ib, 5, 20});
    return f;
}

Role ReceiverOf(MessageType type) { return sender_of(type) == Role::Server ? Role::Client : Role::Server; }

std::vector<std::uint8_t> Encode(const Fixture& f)
{
    std::vector<std::uint8_t> out;
    std::string error;
    if (!encode(f.message, f.epoch, f.sequence, out, &error))
        fail(__FILE__, __LINE__, std::string("fixture ") + f.name + " does not encode: " + error);
    return out;
}

std::vector<std::uint8_t> Frame(std::uint16_t type, std::uint64_t epoch, std::uint64_t sequence,
    const std::vector<std::uint8_t>& body, std::uint16_t flags)
{
    std::vector<std::uint8_t> out;
    Writer w(out);
    w.u16(type);
    w.u16(flags);
    w.u32(std::uint32_t(body.size()));
    w.u64(epoch);
    w.u64(sequence);
    w.raw(body.data(), body.size());
    return out;
}

std::vector<std::uint8_t> Body(const std::function<void(Writer&)>& write)
{
    std::vector<std::uint8_t> out;
    Writer w(out);
    write(w);
    return out;
}

DecodeResult DecodeAs(MessageType type, const std::vector<std::uint8_t>& bytes, Decoded* out)
{
    Decoded scratch;
    return decode(channel_of(type), ReceiverOf(type), bytes.data(), bytes.size(), out ? *out : scratch);
}

void FixLength(std::vector<std::uint8_t>& frame)
{
    if (frame.size() < kHeaderBytes)
        return;
    const std::uint32_t n = std::uint32_t(frame.size() - kHeaderBytes);
    for (int i = 0; i < 4; ++i)
        frame[4 + std::size_t(i)] = std::uint8_t(n >> (8 * i));
}
} // namespace coop::test
