#include "coop/proto/guard.h"

namespace coop::proto
{
namespace
{
bool Reliable(net::ChannelId channel) { return channel != kSnapshot && channel != kInput; }

bool Handshake(std::uint16_t type) { return handshake_type(type); }
} // namespace

void StreamGuard::begin_session(std::uint64_t epoch)
{
    if (epoch_ == 0 && epoch != 0)
        epoch_ = epoch;
}

bool StreamGuard::advance_epoch(std::uint64_t epoch)
{
    if (epoch_ == 0 || epoch <= epoch_)
        return false;
    epoch_ = epoch;
    return true;
}

GuardResult StreamGuard::check(net::ChannelId channel, const Header& h)
{
    auto violation = [this](const char* why) {
        ++violations;
        return GuardResult{Verdict::Violation, why};
    };
    if (channel >= kChannelCount)
        return violation("unknown channel");
    // Sequence first: reliable streams must stay contiguous even for a
    // message that is then dropped for its epoch.
    std::uint64_t& last = last_[channel];
    if (Reliable(channel))
    {
        if (h.sequence != last + 1)
            return violation(h.sequence <= last ? "repeated reliable sequence" : "gap in reliable sequence");
        last = h.sequence;
    }
    else
    {
        if (h.sequence <= last)
        {
            ++dropped_obsolete;
            return {Verdict::Drop, "obsolete transient sequence"};
        }
        last = h.sequence;
    }
    if (Handshake(h.type))
    {
        if (epoch_ != 0)
            return violation("handshake message inside a session");
        // decode() checked direction; the session layer checks their order.
        return {};
    }
    if (epoch_ == 0)
        return violation("message before the session started");
    if (h.epoch < epoch_)
    {
        ++dropped_stale_epoch;
        return {Verdict::Drop, "older authority epoch"};
    }
    if (h.epoch > epoch_)
        return violation("authority epoch from the future");
    (void)receiver_;
    return {};
}

EntityTable::Ref EntityTable::classify(const EntityRef& ref) const
{
    if (ref.id == 0 || ref.generation == 0)
        return Ref::Invalid;
    auto it = entries_.find(ref.id);
    if (it == entries_.end())
        return Ref::Unknown;
    const Entry& e = it->second;
    if (ref.generation < e.generation)
        return Ref::Stale;
    if (ref.generation > e.generation)
        return Ref::Unknown; // a newer incarnation whose spawn has not arrived
    return e.alive ? Ref::Current : Ref::Stale;
}

EntityTable::Apply EntityTable::spawn(const EntityRef& ref)
{
    if (ref.id == 0 || ref.generation == 0)
        return Apply::Invalid;
    auto it = entries_.find(ref.id);
    if (it != entries_.end())
    {
        Entry& e = it->second;
        if (e.alive && ref.generation == e.generation)
            return Apply::Duplicate;
        if (ref.generation <= e.generation)
            return Apply::Recycled;
        if (e.alive)
        {
            // A newer incarnation replaces the live one (its despawn was not
            // sent separately); still one live entry.
            e.generation = ref.generation;
            return Apply::Ok;
        }
        tombOrder_.erase(e.tomb);
        e.generation = ref.generation;
        e.alive = true;
        ++live_;
        return Apply::Ok;
    }
    if (live_ >= capacity_)
        return Apply::Full;
    // Make room by forgetting the oldest tombstones.
    while (entries_.size() >= capacity_ && !tombOrder_.empty())
    {
        entries_.erase(tombOrder_.front());
        tombOrder_.pop_front();
    }
    Entry e;
    e.generation = ref.generation;
    e.alive = true;
    entries_.emplace(ref.id, e);
    ++live_;
    return Apply::Ok;
}

EntityTable::Apply EntityTable::despawn(const EntityRef& ref)
{
    if (ref.id == 0 || ref.generation == 0)
        return Apply::Invalid;
    auto it = entries_.find(ref.id);
    if (it == entries_.end() || !it->second.alive || it->second.generation != ref.generation)
        return Apply::NotLive;
    Entry& e = it->second;
    e.alive = false;
    e.tomb = tombOrder_.insert(tombOrder_.end(), ref.id);
    --live_;
    return Apply::Ok;
}

GuardResult ResyncTracker::begin(const ResyncBegin& b)
{
    if (active_)
        return {Verdict::Violation, "resync already in progress"};
    active_ = true;
    begin_ = b;
    next_ = 0;
    bytes_ = 0;
    crc_ = 0;
    return {};
}

GuardResult ResyncTracker::chunk(const ResyncChunk& c)
{
    if (!active_)
        return {Verdict::Violation, "chunk without a resync"};
    if (c.resync_id != begin_.resync_id)
        return {Verdict::Violation, "chunk of another resync"};
    if (c.index != next_ || c.index >= begin_.chunk_count)
        return {Verdict::Violation, "chunk out of order or past the count"};
    const bool lastChunk = c.index + 1 == begin_.chunk_count;
    const std::uint64_t expect =
        lastChunk ? begin_.total_bytes - std::uint64_t(begin_.chunk_bytes) * c.index : begin_.chunk_bytes;
    if (c.data.size() != expect)
        return {Verdict::Violation, "chunk size disagrees with the announced sizes"};
    crc_ = crc32(c.data.data(), c.data.size(), crc_);
    bytes_ += c.data.size();
    ++next_;
    return {};
}

GuardResult ResyncTracker::end(const ResyncEnd& e)
{
    if (!active_)
        return {Verdict::Violation, "end without a resync"};
    active_ = false;
    if (e.resync_id != begin_.resync_id || e.chunk_count != begin_.chunk_count || next_ != begin_.chunk_count ||
        bytes_ != begin_.total_bytes)
        return {Verdict::Violation, "resync incomplete"};
    if (e.total_crc32 != crc_)
        return {Verdict::Violation, "resync checksum mismatch"};
    return {};
}

std::vector<std::pair<std::uint64_t, Input>> FreshInputs(const InputBatch& batch, std::uint64_t batch_sequence,
    std::uint64_t last_processed)
{
    std::vector<std::pair<std::uint64_t, Input>> out;
    for (std::size_t i = batch.inputs.size(); i-- > 0;)
    {
        const std::uint64_t seq = batch_sequence - i;
        if (seq > last_processed)
            out.push_back({seq, batch.inputs[i]});
    }
    return out;
}

} // namespace coop::proto
