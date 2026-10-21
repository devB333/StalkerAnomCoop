// S06-T02: see coop/proto/snapshot.h.
#include "coop/proto/snapshot.h"

#include <algorithm>
#include <cmath>

namespace coop::proto
{
namespace
{
bool Differs(float a, float b, float tolerance) { return std::fabs(a - b) > tolerance; }

float AngleDistance(float a, float b)
{
    const float two_pi = 6.28318530718f;
    float d = std::fmod(std::fabs(a - b), two_pi);
    return d > two_pi * 0.5f ? two_pi - d : d;
}

SnapshotEntity Entry(const EntityState& s, std::uint16_t components)
{
    SnapshotEntity e;
    e.entity = s.entity;
    e.components = components;
    e.transform = s.transform;
    e.vx = s.vx;
    e.vy = s.vy;
    e.vz = s.vz;
    e.health = s.health;
    e.state_bits = s.state_bits;
    e.qx = s.qx;
    e.qy = s.qy;
    e.qz = s.qz;
    e.qw = s.qw;
    e.aim_yaw = s.aim_yaw;
    e.aim_pitch = s.aim_pitch;
    e.weapon_slot = s.weapon_slot;
    e.weapon_state = s.weapon_state;
    e.ammo = s.ammo;
    e.shots = s.shots;
    return e;
}

// The one merge rule, used by the sender for its frames and by the receiver
// for its own, so the two always agree: a removal erases; an entry for
// another generation (or a new entity) replaces; otherwise the entry's
// components overwrite the baseline's and the component set only grows.
void Merge(SnapshotFrame& frame, const SnapshotEntity& e)
{
    if (e.components & kRemoved)
    {
        frame.erase(e.entity.id);
        return;
    }
    EntityState& s = frame[e.entity.id];
    if (!(s.entity == e.entity))
        s = EntityState{};
    s.entity = e.entity;
    s.components = std::uint16_t(s.components | e.components);
    if (e.components & kTransform)
        s.transform = e.transform;
    if (e.components & kVelocity)
    {
        s.vx = e.vx;
        s.vy = e.vy;
        s.vz = e.vz;
    }
    if (e.components & kHealth)
        s.health = e.health;
    if (e.components & kStateBits)
        s.state_bits = e.state_bits;
    if (e.components & kRotation)
    {
        s.qx = e.qx;
        s.qy = e.qy;
        s.qz = e.qz;
        s.qw = e.qw;
    }
    if (e.components & kAim)
    {
        s.aim_yaw = e.aim_yaw;
        s.aim_pitch = e.aim_pitch;
    }
    if (e.components & kWeapon)
    {
        s.weapon_slot = e.weapon_slot;
        s.weapon_state = e.weapon_state;
        s.ammo = e.ammo;
        s.shots = e.shots;
    }
}
} // namespace

std::uint16_t changed_components(const EntityState* base, const EntityState& now, const SnapshotTolerance& t)
{
    const std::uint16_t have = std::uint16_t(now.components & kStateComponents);
    if (!base || !(base->entity == now.entity))
        return have;
    std::uint16_t c = std::uint16_t(have & ~base->components); // components the baseline lacks
    if ((have & kTransform) && (base->components & kTransform) &&
        (Differs(now.transform.x, base->transform.x, t.position) || Differs(now.transform.y, base->transform.y, t.position) ||
            Differs(now.transform.z, base->transform.z, t.position) ||
            AngleDistance(now.transform.yaw, base->transform.yaw) > t.yaw))
        c |= kTransform;
    if ((have & kVelocity) && (base->components & kVelocity) &&
        (Differs(now.vx, base->vx, t.velocity) || Differs(now.vy, base->vy, t.velocity) ||
            Differs(now.vz, base->vz, t.velocity)))
        c |= kVelocity;
    if ((have & kHealth) && (base->components & kHealth) && now.health != base->health)
        c |= kHealth;
    if ((have & kStateBits) && (base->components & kStateBits) && now.state_bits != base->state_bits)
        c |= kStateBits;
    if ((have & kRotation) && (base->components & kRotation) &&
        (Differs(now.qx, base->qx, t.rotation) || Differs(now.qy, base->qy, t.rotation) ||
            Differs(now.qz, base->qz, t.rotation) || Differs(now.qw, base->qw, t.rotation)))
        c |= kRotation;
    if ((have & kAim) && (base->components & kAim) &&
        (AngleDistance(now.aim_yaw, base->aim_yaw) > t.yaw || AngleDistance(now.aim_pitch, base->aim_pitch) > t.yaw))
        c |= kAim;
    if ((have & kWeapon) && (base->components & kWeapon) &&
        (now.weapon_slot != base->weapon_slot || now.weapon_state != base->weapon_state || now.ammo != base->ammo ||
            now.shots != base->shots))
        c |= kWeapon;
    return c;
}

std::size_t entry_bytes(std::uint16_t c)
{
    std::size_t n = 12 + 2; // reference, components
    if (c & kRemoved)
        return n;
    if (c & kTransform)
        n += 16;
    if (c & kVelocity)
        n += 12;
    if (c & kHealth)
        n += 2;
    if (c & kStateBits)
        n += 2;
    if (c & kRotation)
        n += 16;
    if (c & kAim)
        n += 8;
    if (c & kWeapon)
        n += 6;
    return n;
}

// ------------------------------------------------------------- sender
SnapshotSender::SnapshotSender(const SnapshotSenderConfig& config) : config_(config)
{
    if (config_.history < 1)
        config_.history = 1;
    if (config_.max_parts < 1)
        config_.max_parts = 1;
    if (config_.max_parts > kMaxSnapshotParts)
        config_.max_parts = kMaxSnapshotParts;
    if (config_.max_part_bytes > kMaxMessageBytes[kSnapshot])
        config_.max_part_bytes = kMaxMessageBytes[kSnapshot];
    budget_ = config_.max_parts;
    counters_.min_budget = budget_;
}

void SnapshotSender::sent(bool congested)
{
    if (congested)
    {
        ++counters_.congestions;
        budget_ = budget_ > 1 ? budget_ / 2 : 1;
        counters_.min_budget = std::min(counters_.min_budget, budget_);
        clean_ = 0;
        return;
    }
    if (++clean_ >= config_.grow_after && budget_ < config_.max_parts)
    {
        ++budget_;
        clean_ = 0;
    }
}

const SnapshotSender::Sent* SnapshotSender::find(std::uint64_t id) const
{
    for (const Sent& s : history_)
        if (s.id == id)
            return &s;
    return nullptr;
}

const SnapshotFrame* SnapshotSender::frame(std::uint64_t id) const
{
    const Sent* s = find(id);
    return s ? &s->frame : nullptr;
}

std::uint64_t SnapshotSender::next_baseline() const
{
    if (full_requested_ || !acked_)
        return 0;
    return find(acked_) ? acked_ : 0;
}

std::uint64_t SnapshotSender::build(std::uint64_t server_tick, std::uint64_t last_input,
    const std::vector<EntityState>& wanted, std::vector<Snapshot>& parts, std::size_t pinned)
{
    parts.clear();
    const Sent* base = nullptr;
    if (!full_requested_ && acked_)
    {
        base = find(acked_);
        if (!base)
        {
            ++counters_.baseline_lost;
            acked_ = 0;
        }
    }
    const std::uint64_t baseline = base ? base->id : 0;
    SnapshotFrame frame = base ? base->frame : SnapshotFrame{};

    // Entries, in sending order: the pinned wanted entities that changed;
    // removals of what the baseline holds and is no longer wanted; the other
    // changed entities by the caller's order, each moved ahead by the
    // snapshots it has already been deferred.
    std::set<std::uint64_t> wanted_ids;
    for (const EntityState& w : wanted)
        if (w.entity.id)
            wanted_ids.insert(w.entity.id);
    struct Candidate
    {
        double key;
        SnapshotEntity entry;
    };
    // Deferral moves an entity ahead without limit, but never ahead of the
    // pinned entities or the removals.
    const double kPinnedKey = -1e300, kRemovalKey = -1e299;
    std::vector<Candidate> candidates;
    if (base)
        for (const auto& b : base->frame)
            if (!wanted_ids.count(b.first))
                candidates.push_back({kRemovalKey, Entry(b.second, kRemoved)});
    std::set<std::uint64_t> seen;
    std::size_t index = 0;
    for (const EntityState& w : wanted)
    {
        const std::size_t at = index++;
        if (!w.entity.id || !seen.insert(w.entity.id).second)
            continue;
        const EntityState* b = nullptr;
        if (base)
        {
            const auto it = base->frame.find(w.entity.id);
            if (it != base->frame.end())
                b = &it->second;
        }
        const std::uint16_t c = changed_components(b, w, config_.tolerance);
        if (!c)
        {
            ++counters_.unchanged;
            deferrals_.erase(w.entity.id);
            continue;
        }
        double key = kPinnedKey;
        if (at >= pinned)
        {
            const auto d = deferrals_.find(w.entity.id);
            key = double(at) - double(d == deferrals_.end() ? 0 : d->second) * double(config_.deferral_boost);
        }
        candidates.push_back({key, Entry(w, c)});
    }
    std::stable_sort(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.key < b.key; });
    for (auto it = deferrals_.begin(); it != deferrals_.end();)
        it = seen.count(it->first) ? std::next(it) : deferrals_.erase(it);

    // Parts within the budget; what does not fit is deferred (the frame
    // keeps the baseline's state for it).
    const std::uint64_t id = ++last_id_;
    std::vector<std::vector<SnapshotEntity>> packed(1);
    std::vector<std::size_t> bytes(1, kSnapshotPartOverhead);
    for (const Candidate& candidate : candidates)
    {
        const SnapshotEntity& e = candidate.entry;
        const std::size_t size = entry_bytes(e.components);
        if (bytes.back() + size > config_.max_part_bytes || packed.back().size() >= kMaxSnapshotEntities)
        {
            if (packed.size() >= budget_)
            {
                ++counters_.deferred;
                if (!(e.components & kRemoved))
                    counters_.max_deferrals = std::max(counters_.max_deferrals, ++deferrals_[e.entity.id]);
                continue;
            }
            packed.emplace_back();
            bytes.push_back(kSnapshotPartOverhead);
        }
        if (!(e.components & kRemoved))
            deferrals_.erase(e.entity.id);
        packed.back().push_back(e);
        bytes.back() += size;
        Merge(frame, e);
        ++counters_.entries;
        if (e.components & kRemoved)
            ++counters_.removals;
    }
    std::size_t total = 0;
    for (std::size_t i = 0; i < packed.size(); ++i)
    {
        Snapshot s;
        s.server_tick = server_tick;
        s.snapshot_id = id;
        s.baseline_id = baseline;
        s.last_input = last_input;
        s.part = std::uint8_t(i);
        s.part_count = std::uint8_t(packed.size());
        s.entities = std::move(packed[i]);
        parts.push_back(std::move(s));
        total += bytes[i];
    }
    ++counters_.snapshots;
    if (baseline)
        ++counters_.delta;
    else
    {
        ++counters_.full;
        full_requested_ = false;
    }
    counters_.parts += parts.size();
    counters_.bytes += total;
    counters_.max_snapshot_bytes = std::max(counters_.max_snapshot_bytes, total);
    counters_.max_parts_used = std::max(counters_.max_parts_used, parts.size());

    Sent sent;
    sent.id = id;
    sent.tick = server_tick;
    sent.frame = std::move(frame);
    history_.push_back(std::move(sent));
    while (history_.size() > config_.history)
        history_.pop_front();
    return id;
}

void SnapshotSender::on_ack(const SnapshotAck& ack)
{
    ++counters_.acks;
    if (ack.missing_baseline)
    {
        ++counters_.full_requested;
        full_requested_ = true;
    }
    if (!ack.snapshot_id)
        return;
    if (ack.snapshot_id > last_id_)
    {
        ++counters_.acks_invalid;
        return;
    }
    if (ack.snapshot_id <= acked_ || !find(ack.snapshot_id))
    {
        ++counters_.acks_old;
        return;
    }
    acked_ = ack.snapshot_id;
}

bool SnapshotSender::acked_holds(const EntityState& state) const
{
    const Sent* s = acked_ ? find(acked_) : nullptr;
    if (!s)
        return false;
    const auto it = s->frame.find(state.entity.id);
    return it != s->frame.end() && changed_components(&it->second, state, config_.tolerance) == 0;
}

void SnapshotSender::reset()
{
    history_.clear();
    last_id_ = 0;
    acked_ = 0;
    full_requested_ = false;
    budget_ = config_.max_parts;
    clean_ = 0;
    deferrals_.clear();
}

// ------------------------------------------------------------- receiver
SnapshotReceiver::SnapshotReceiver(const SnapshotReceiverConfig& config) : config_(config)
{
    if (config_.history < 1)
        config_.history = 1;
    if (config_.assembling < 1)
        config_.assembling = 1;
}

const SnapshotFrame* SnapshotReceiver::frame(std::uint64_t id) const
{
    for (const Complete& c : complete_)
        if (c.id == id)
            return &c.frame;
    return nullptr;
}

const SnapshotFrame* SnapshotReceiver::newest_frame(std::uint64_t* server_tick) const
{
    if (complete_.empty())
        return nullptr;
    if (server_tick)
        *server_tick = complete_.back().tick;
    return &complete_.back().frame;
}

void SnapshotReceiver::hand_on(std::uint64_t tick, const EntityState& state, SnapshotUpdate& update)
{
    const auto it = handed_.find(state.entity.id);
    if ((it != handed_.end() && it->second >= tick) || tick + config_.max_age_ticks < newest_tick_)
    {
        ++counters_.older;
        return;
    }
    handed_[state.entity.id] = tick;
    update.states.push_back(state);
    ++counters_.states;
}

void SnapshotReceiver::hand_on_removal(std::uint64_t tick, const EntityRef& ref, SnapshotUpdate& update)
{
    const auto it = handed_.find(ref.id);
    if ((it != handed_.end() && it->second >= tick) || tick + config_.max_age_ticks < newest_tick_)
    {
        ++counters_.older;
        return;
    }
    handed_[ref.id] = tick;
    update.removed.push_back(ref);
    ++counters_.removals;
}

SnapshotVerdict SnapshotReceiver::offer(const Snapshot& part, SnapshotUpdate& update)
{
    update = SnapshotUpdate{};
    update.server_tick = part.server_tick;
    update.last_input = part.last_input;
    ++counters_.parts;
    if (frame(part.snapshot_id))
    {
        ++counters_.duplicate_parts;
        return SnapshotVerdict::Duplicate;
    }
    if (complete_.size() >= config_.history && part.snapshot_id < complete_.front().id)
        return SnapshotVerdict::TooOld;

    auto a = std::find_if(assembling_.begin(), assembling_.end(),
        [&](const Assembly& x) { return x.id == part.snapshot_id; });
    if (a == assembling_.end())
    {
        const SnapshotFrame* base = nullptr;
        if (part.baseline_id)
        {
            base = frame(part.baseline_id);
            if (!base)
            {
                ++counters_.missing_baseline;
                missing_ = std::max(missing_, part.baseline_id);
                return SnapshotVerdict::MissingBaseline;
            }
        }
        Assembly fresh;
        fresh.id = part.snapshot_id;
        fresh.tick = part.server_tick;
        fresh.baseline = part.baseline_id;
        fresh.last_input = part.last_input;
        fresh.count = part.part_count;
        fresh.have.assign(part.part_count, false);
        if (base)
            fresh.frame = *base;
        const auto at = std::lower_bound(assembling_.begin(), assembling_.end(), fresh.id,
            [](const Assembly& x, std::uint64_t id) { return x.id < id; });
        a = assembling_.insert(at, std::move(fresh));
        if (assembling_.size() > config_.assembling)
        {
            if (a == assembling_.begin())
            {
                assembling_.pop_front();
                return SnapshotVerdict::TooOld;
            }
            assembling_.pop_front();
            ++counters_.incomplete_dropped;
            a = std::find_if(assembling_.begin(), assembling_.end(),
                [&](const Assembly& x) { return x.id == part.snapshot_id; });
        }
    }
    else if (a->count != part.part_count || a->baseline != part.baseline_id || a->tick != part.server_tick ||
             a->last_input != part.last_input)
    {
        ++counters_.inconsistent;
        return SnapshotVerdict::Inconsistent;
    }
    if (a->have[part.part])
    {
        ++counters_.duplicate_parts;
        return SnapshotVerdict::Duplicate;
    }
    for (const SnapshotEntity& e : part.entities)
        if (a->touched.count(e.entity.id))
        {
            ++counters_.inconsistent; // one entity in two parts of a snapshot
            return SnapshotVerdict::Inconsistent;
        }

    a->have[part.part] = true;
    newest_tick_ = std::max(newest_tick_, part.server_tick);
    for (const SnapshotEntity& e : part.entities)
    {
        a->touched.insert(e.entity.id);
        Merge(a->frame, e);
        if (e.components & kRemoved)
            hand_on_removal(part.server_tick, e.entity, update);
        else
            hand_on(part.server_tick, a->frame[e.entity.id], update);
    }
    if (handed_.size() > 8192)
        for (auto it = handed_.begin(); it != handed_.end();)
            it = it->second + config_.max_age_ticks < newest_tick_ ? handed_.erase(it) : std::next(it);

    if (std::find(a->have.begin(), a->have.end(), false) != a->have.end())
        return SnapshotVerdict::Applied;
    Complete done;
    done.id = a->id;
    done.tick = a->tick;
    done.frame = std::move(a->frame);
    if (a->baseline)
        ++counters_.delta;
    else
        ++counters_.full;
    ++counters_.completed;
    assembling_.erase(a);
    // Anything assembling that is older than this snapshot can still
    // complete (a reordered part) and is kept.
    const auto at = std::lower_bound(complete_.begin(), complete_.end(), done.id,
        [](const Complete& x, std::uint64_t id) { return x.id < id; });
    complete_.insert(at, std::move(done));
    while (complete_.size() > config_.history)
        complete_.pop_front();
    newest_complete_ = std::max(newest_complete_, part.snapshot_id);
    return SnapshotVerdict::Completed;
}

bool SnapshotReceiver::take_ack(SnapshotAck& out)
{
    if (newest_complete_ == acked_sent_ && !missing_)
        return false;
    out = SnapshotAck{};
    out.snapshot_id = newest_complete_;
    out.missing_baseline = missing_;
    acked_sent_ = newest_complete_;
    missing_ = 0;
    return true;
}

void SnapshotReceiver::reset()
{
    assembling_.clear();
    complete_.clear();
    handed_.clear();
    newest_complete_ = acked_sent_ = missing_ = newest_tick_ = 0;
}

} // namespace coop::proto
