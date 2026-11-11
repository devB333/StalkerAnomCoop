// Stateful checks on decoded messages (S04-T03), after decode() and before
// anything reaches the simulation. One set per connection, owned by the
// thread that polls the transport.
//
//   StreamGuard   epoch fencing and per-channel sequence rules
//   EntityTable   entity generations: recycled or stale references refused
//   ResyncTracker one bulk resync: ordered chunks, sizes and CRC
//   FreshInputs   which inputs of a redundant batch are new
#pragma once

#include "coop/proto/messages.h"

#include <cstdint>
#include <list>
#include <unordered_map>

namespace coop::proto
{
enum class Verdict : std::uint8_t
{
    Accept,
    Drop,      // harmless and expected (an older epoch, an obsolete transient); count it
    Violation, // the peer broke the protocol; close it (net::close_code::kProtocolViolation)
};

struct GuardResult
{
    Verdict verdict = Verdict::Accept;
    const char* why = "";
};

// Sequence rules, per channel and direction, for one connection's lifetime:
// reliable channels count 1, 2, 3... with no gap or repeat (the transport
// guarantees order, so anything else is a broken or hostile peer); transient
// channels only move forward, and an older or repeated sequence is dropped.
//
// Epoch rules: Hello, Welcome and Reject carry epoch 0 and only travel before
// the session starts; after it, every message carries the session's
// authority epoch. A message from an older epoch (sent before a resync or
// recovery) is dropped; a newer epoch than the receiver was told is a
// violation. A resync announces the next epoch in ResyncBegin's body, under
// the current one, and both sides then call advance_epoch().
class StreamGuard
{
public:
    explicit StreamGuard(Role receiver) : receiver_(receiver) {}

    void begin_session(std::uint64_t epoch);
    bool advance_epoch(std::uint64_t epoch); // false unless newer
    std::uint64_t epoch() const { return epoch_; }
    bool in_session() const { return epoch_ != 0; }

    GuardResult check(net::ChannelId channel, const Header& header);

    std::uint64_t dropped_stale_epoch = 0;
    std::uint64_t dropped_obsolete = 0;
    std::uint64_t violations = 0;

private:
    Role receiver_;
    std::uint64_t epoch_ = 0;
    std::uint64_t last_[kChannelCount] = {};
};

// Next sequence to send on each channel.
class SequenceCounter
{
public:
    std::uint64_t next(net::ChannelId channel) { return ++last_[channel]; }
    // B64: the sequence the next send will carry, then done() with whether it
    // was sent. A reliable channel's sequence is taken only by a sent message
    // (its receiver refuses a gap: an unencodable message, retried, had left
    // one). A transient channel's is taken by every attempt, as before: gaps
    // are allowed there, and an input batch's sequence must reach its input
    // count (decode), which the first attempts' sequences climb to.
    std::uint64_t peek(net::ChannelId channel) const { return last_[channel] + 1; }
    void done(net::ChannelId channel, bool sent)
    {
        if (sent || channel == kSnapshot || channel == kInput)
            ++last_[channel];
    }

private:
    std::uint64_t last_[kChannelCount] = {};
};

// The receiver's view of entity incarnations. EntityIds are never reused in a
// campaign, but an entity can leave and come back (ALife switching keeps its
// EntityId and turns its generation, docs/participant-identity.md); a
// generation can only move forward, so a reference to an older one is stale
// and a spawn that does not move it forward is a recycled generation.
// Bounded: at capacity, the oldest tombstones are forgotten first; live
// entities are never evicted (a spawn beyond capacity is refused).
class EntityTable
{
public:
    explicit EntityTable(std::size_t capacity) : capacity_(capacity) {}

    enum class Ref : std::uint8_t
    {
        Current, // live at this generation
        Stale,   // older generation, or despawned
        Unknown, // not spawned yet here (e.g. a snapshot ahead of its spawn)
        Invalid, // zero
    };
    Ref classify(const EntityRef& ref) const;

    enum class Apply : std::uint8_t
    {
        Ok,
        Recycled,  // spawn at a generation not newer than one seen
        Duplicate, // spawn of the live generation again
        NotLive,   // despawn of something not live at that generation
        Full,      // live entities at capacity
        Invalid,
    };
    Apply spawn(const EntityRef& ref);
    Apply despawn(const EntityRef& ref);

    std::size_t live() const { return live_; }
    std::size_t tombstones() const { return entries_.size() - live_; }

private:
    struct Entry
    {
        std::uint32_t generation = 0;
        bool alive = false;
        std::list<std::uint64_t>::iterator tomb; // position in tombOrder_ when dead
    };
    std::size_t capacity_;
    std::size_t live_ = 0;
    std::unordered_map<std::uint64_t, Entry> entries_;
    std::list<std::uint64_t> tombOrder_; // oldest first
};

// Receives one resync: ResyncBegin, its chunks in order on the bulk channel,
// ResyncEnd. Streams the data out without buffering the whole checkpoint.
class ResyncTracker
{
public:
    GuardResult begin(const ResyncBegin& b);
    GuardResult chunk(const ResyncChunk& c); // the caller consumes c.data after Accept
    GuardResult end(const ResyncEnd& e);
    bool active() const { return active_; }
    std::uint64_t received_bytes() const { return bytes_; }

private:
    bool active_ = false;
    ResyncBegin begin_;
    std::uint32_t next_ = 0;
    std::uint64_t bytes_ = 0;
    std::uint32_t crc_ = 0;
};

// Inputs of a batch with sequence > last_processed, oldest first (at most
// kMaxInputsPerBatch). Inputs are numbered header.sequence - i.
std::vector<std::pair<std::uint64_t, Input>> FreshInputs(const InputBatch& batch, std::uint64_t batch_sequence,
    std::uint64_t last_processed);

} // namespace coop::proto
