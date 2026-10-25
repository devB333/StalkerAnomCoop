// The durable critical-outcome journal (S07-T03), docs/persistence-contract.md §6.
//
// One append-only file per checkpoint generation: a header, then records.
// A record is durable when append() returned true: the bytes were written
// through and FlushFileBuffers returned. The caller acknowledges nothing
// before that.
//
//   header: 'CJRN' u32, schema u32, campaign 16 B, generation u32,
//           base revision u64, CRC-32 u32
//   record: length u32 (payload), CRC-32 u32 (of sequence..payload),
//           sequence u64, kind u8, epoch u64, tick u64, revision u64, payload
//
// Reading: a torn last record (short or bad CRC at the end of the file) is
// dropped and reported; anything else wrong (a bad record followed by more
// bytes, a sequence gap, a header mismatch) is corruption and the file is
// refused as a whole.
#pragma once

#include "coop/proto/wire.h"
#include "coop/store/file.h"

#include <cstdint>
#include <string>
#include <vector>

namespace coop::store
{
constexpr std::uint32_t kJournalMagic = 0x4E524A43; // "CJRN"
constexpr std::uint32_t kJournalSchema = 1;
constexpr std::uint32_t kMaxJournalPayload = 64 * 1024;

// Record kinds (docs/persistence-contract.md §6). Application payloads are
// encoded by their adapters; the journal only carries them.
enum JournalKind : std::uint8_t
{
    kRecordEpoch = 1,    // payload: none (the epoch field is the new epoch)
    kRecordLedger = 2,   // S07-T03 synthetic effect
    kRecordDeath = 3,    // S07-T06/T07
    kRecordRespawn = 4,  // S07-T07
    kRecordEntity = 5,   // a durable entity created after the checkpoint
    kRecordTombstone = 6,
    kRecordPickup = 7,   // S08-T01: an item taken into a participant actor's inventory
    kRecordTransfer = 8, // S08-T02: an item dropped, or moved between a participant actor and a container
    kRecordEntityReserve = 9, // S08-T02: EntityIds below `upto` (u64 payload) may be issued; recovery starts above
};

struct JournalHeader
{
    proto::Uuid128 campaign;
    std::uint32_t generation = 0;
    std::uint64_t base_revision = 0;
};

struct JournalRecord
{
    std::uint64_t sequence = 0;
    std::uint8_t kind = 0;
    std::uint64_t epoch = 0;
    std::uint64_t tick = 0;
    std::uint64_t revision = 0;
    std::vector<std::uint8_t> payload;
};

class JournalWriter
{
public:
    // Creates the file with its header (refuses an existing file) or opens an
    // existing one whose header matches, continuing after its last readable
    // record (a torn tail is left in place and the next record is written
    // after the last good one, so the file is rewritten from that point: the
    // torn bytes are overwritten).
    bool create(const std::string& path, const JournalHeader& header, FileError* error = nullptr);
    bool open_existing(const std::string& path, const JournalHeader& expected, FileError* error = nullptr);
    // Durable append. False on any failure; the writer is then failed().
    bool append(std::uint8_t kind, std::uint64_t epoch, std::uint64_t tick, std::uint64_t revision,
        const std::vector<std::uint8_t>& payload, FileError* error = nullptr);
    bool failed() const { return file_.failed() || failed_; }
    bool is_open() const { return file_.is_open(); }
    std::uint64_t next_sequence() const { return sequence_ + 1; }
    std::uint64_t appends() const { return appends_; }
    double last_append_us() const { return file_.last_append_us(); }
    double max_append_us() const { return max_us_; }
    double total_append_us() const { return total_us_; }
    const std::string& path() const { return file_.path(); }
    void close() { file_.close(); }
    // Closes and forgets the file: ready for create() or open_existing() again.
    void reset();

private:
    AppendFile file_;
    std::uint64_t sequence_ = 0;
    std::uint64_t appends_ = 0;
    double max_us_ = 0, total_us_ = 0;
    bool failed_ = false;
};

enum class JournalStatus : std::uint8_t
{
    Ok,        // every record read
    TornTail,  // every record but a torn last one read
    Missing,   // no such file
    BadHeader, // not a journal, another schema, campaign or generation
    Corrupt,   // a bad record with bytes after it, or a sequence error
};
const char* to_string(JournalStatus s);

struct JournalContents
{
    JournalHeader header;
    std::vector<JournalRecord> records;
    std::uint64_t good_bytes = 0; // offset after the last good record
    std::string detail;
};

JournalStatus read_journal(const std::string& path, JournalContents& out);
// Reads only the header.
JournalStatus read_journal_header(const std::string& path, JournalHeader& out);

std::vector<std::uint8_t> encode_journal_header(const JournalHeader& h);
std::vector<std::uint8_t> encode_journal_record(const JournalRecord& r);
} // namespace coop::store
