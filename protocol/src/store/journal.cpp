// The durable journal (S07-T03): see coop/store/journal.h.
#include "coop/store/journal.h"

#include <cstring>

namespace coop::store
{
namespace
{
constexpr std::size_t kHeaderBytes = 4 + 4 + 16 + 4 + 8 + 4;
constexpr std::size_t kRecordFixed = 4 + 4 + 8 + 1 + 8 + 8 + 8;

template <class T>
void put(std::vector<std::uint8_t>& out, T v)
{
    const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}

template <class T>
bool take(const std::uint8_t* data, std::size_t size, std::size_t& at, T& v)
{
    if (at + sizeof(T) > size)
        return false;
    std::memcpy(&v, data + at, sizeof(T));
    at += sizeof(T);
    return true;
}

bool decode_header(const std::uint8_t* data, std::size_t size, JournalHeader& h, std::string& why)
{
    if (size < kHeaderBytes)
    {
        why = "short header";
        return false;
    }
    std::size_t at = 0;
    std::uint32_t magic = 0, schema = 0, crc = 0;
    take(data, size, at, magic);
    take(data, size, at, schema);
    take(data, size, at, h.campaign.hi);
    take(data, size, at, h.campaign.lo);
    take(data, size, at, h.generation);
    take(data, size, at, h.base_revision);
    take(data, size, at, crc);
    if (magic != kJournalMagic)
    {
        why = "not a journal";
        return false;
    }
    if (schema != kJournalSchema)
    {
        why = "journal schema " + std::to_string(schema);
        return false;
    }
    if (crc != crc32_bytes(data, kHeaderBytes - 4))
    {
        why = "header crc";
        return false;
    }
    return true;
}
} // namespace

const char* to_string(JournalStatus s)
{
    switch (s)
    {
    case JournalStatus::Ok: return "ok";
    case JournalStatus::TornTail: return "torn-tail";
    case JournalStatus::Missing: return "missing";
    case JournalStatus::BadHeader: return "bad-header";
    case JournalStatus::Corrupt: return "corrupt";
    }
    return "?";
}

std::vector<std::uint8_t> encode_journal_header(const JournalHeader& h)
{
    std::vector<std::uint8_t> out;
    out.reserve(kHeaderBytes);
    put<std::uint32_t>(out, kJournalMagic);
    put<std::uint32_t>(out, kJournalSchema);
    put<std::uint64_t>(out, h.campaign.hi);
    put<std::uint64_t>(out, h.campaign.lo);
    put<std::uint32_t>(out, h.generation);
    put<std::uint64_t>(out, h.base_revision);
    put<std::uint32_t>(out, crc32_bytes(out.data(), out.size()));
    return out;
}

std::vector<std::uint8_t> encode_journal_record(const JournalRecord& r)
{
    std::vector<std::uint8_t> body;
    body.reserve(kRecordFixed - 8 + r.payload.size());
    put<std::uint64_t>(body, r.sequence);
    put<std::uint8_t>(body, r.kind);
    put<std::uint64_t>(body, r.epoch);
    put<std::uint64_t>(body, r.tick);
    put<std::uint64_t>(body, r.revision);
    body.insert(body.end(), r.payload.begin(), r.payload.end());
    std::vector<std::uint8_t> out;
    out.reserve(8 + body.size());
    put<std::uint32_t>(out, std::uint32_t(r.payload.size()));
    put<std::uint32_t>(out, crc32_bytes(body.data(), body.size()));
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

void JournalWriter::reset()
{
    file_.close();
    file_ = AppendFile();
    sequence_ = 0;
    appends_ = 0;
    max_us_ = total_us_ = 0;
    failed_ = false;
}

bool JournalWriter::create(const std::string& path, const JournalHeader& header, FileError* error)
{
    sequence_ = 0;
    failed_ = false;
    if (!file_.open(path, true, error))
        return false;
    const std::vector<std::uint8_t> h = encode_journal_header(header);
    if (!file_.append(h.data(), h.size(), error))
    {
        failed_ = true;
        return false;
    }
    return true;
}

bool JournalWriter::open_existing(const std::string& path, const JournalHeader& expected, FileError* error)
{
    JournalContents contents;
    const JournalStatus status = read_journal(path, contents);
    if (status != JournalStatus::Ok && status != JournalStatus::TornTail)
    {
        if (error)
            error->what = std::string("journal ") + to_string(status) + ": " + contents.detail;
        return false;
    }
    if (contents.header.campaign != expected.campaign || contents.header.generation != expected.generation ||
        contents.header.base_revision != expected.base_revision)
    {
        if (error)
            error->what = "journal header does not match the generation";
        return false;
    }
    if (status == JournalStatus::TornTail)
    {
        // Cut the torn tail at the last good record and append from there.
        if (!truncate_file(path, contents.good_bytes, error))
            return false;
    }
    sequence_ = contents.records.empty() ? 0 : contents.records.back().sequence;
    failed_ = false;
    return file_.open(path, false, error);
}

bool JournalWriter::append(std::uint8_t kind, std::uint64_t epoch, std::uint64_t tick, std::uint64_t revision,
    const std::vector<std::uint8_t>& payload, FileError* error)
{
    if (failed())
    {
        if (error)
            error->what = "journal failed earlier";
        return false;
    }
    if (payload.size() > kMaxJournalPayload)
    {
        if (error)
            error->what = "journal payload too large";
        return false;
    }
    JournalRecord r;
    r.sequence = sequence_ + 1;
    r.kind = kind;
    r.epoch = epoch;
    r.tick = tick;
    r.revision = revision;
    r.payload = payload;
    const std::vector<std::uint8_t> bytes = encode_journal_record(r);
    if (!file_.append(bytes.data(), bytes.size(), error))
    {
        failed_ = true;
        return false;
    }
    sequence_ = r.sequence;
    ++appends_;
    const double us = file_.last_append_us();
    total_us_ += us;
    if (us > max_us_)
        max_us_ = us;
    return true;
}

JournalStatus read_journal_header(const std::string& path, JournalHeader& out)
{
    std::vector<std::uint8_t> bytes;
    if (!file_exists(path))
        return JournalStatus::Missing;
    if (!read_file(path, bytes))
        return JournalStatus::Corrupt;
    std::string why;
    return decode_header(bytes.data(), bytes.size(), out, why) ? JournalStatus::Ok : JournalStatus::BadHeader;
}

JournalStatus read_journal(const std::string& path, JournalContents& out)
{
    out.records.clear();
    out.good_bytes = 0;
    out.detail.clear();
    if (!file_exists(path))
    {
        out.detail = "missing";
        return JournalStatus::Missing;
    }
    std::vector<std::uint8_t> bytes;
    FileError error;
    if (!read_file(path, bytes, &error))
    {
        out.detail = "read: " + error.what;
        return JournalStatus::Corrupt;
    }
    if (!decode_header(bytes.data(), bytes.size(), out.header, out.detail))
        return JournalStatus::BadHeader;
    std::size_t at = kHeaderBytes;
    out.good_bytes = at;
    std::uint64_t expected_sequence = 1;
    while (at < bytes.size())
    {
        const std::size_t start = at;
        std::uint32_t length = 0, crc = 0;
        bool torn = false;
        if (!take(bytes.data(), bytes.size(), at, length) || !take(bytes.data(), bytes.size(), at, crc))
            torn = true;
        else if (length > kMaxJournalPayload)
        {
            out.detail = "record " + std::to_string(expected_sequence) + " length " + std::to_string(length);
            return JournalStatus::Corrupt;
        }
        else
        {
            const std::size_t body = kRecordFixed - 8 + length;
            if (start + 8 + body > bytes.size())
                torn = true;
            else if (crc != crc32_bytes(bytes.data() + start + 8, body))
            {
                // A bad CRC at the very end is a torn append (the length was
                // written, the body not fully); with bytes after it, corruption.
                if (start + 8 + body == bytes.size())
                    torn = true;
                else
                {
                    out.detail = "record " + std::to_string(expected_sequence) + " crc";
                    return JournalStatus::Corrupt;
                }
            }
        }
        if (torn)
        {
            out.detail = "torn tail after " + std::to_string(out.records.size()) + " records";
            return JournalStatus::TornTail;
        }
        JournalRecord r;
        std::size_t p = start + 8;
        take(bytes.data(), bytes.size(), p, r.sequence);
        take(bytes.data(), bytes.size(), p, r.kind);
        take(bytes.data(), bytes.size(), p, r.epoch);
        take(bytes.data(), bytes.size(), p, r.tick);
        take(bytes.data(), bytes.size(), p, r.revision);
        r.payload.assign(bytes.begin() + std::ptrdiff_t(p), bytes.begin() + std::ptrdiff_t(p + length));
        if (r.sequence != expected_sequence)
        {
            out.detail = "sequence " + std::to_string(r.sequence) + " where " + std::to_string(expected_sequence) +
                " was expected";
            return JournalStatus::Corrupt;
        }
        ++expected_sequence;
        at = p + length;
        out.good_bytes = at;
        out.records.push_back(std::move(r));
    }
    return JournalStatus::Ok;
}
} // namespace coop::store
