// Checkpoint generations (S07-T02): see coop/store/checkpoint.h.
#include "coop/store/checkpoint.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>

namespace coop::store
{
namespace
{
template <class T>
void put(std::vector<std::uint8_t>& out, T v)
{
    const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}

void put_string(std::vector<std::uint8_t>& out, const std::string& s)
{
    const std::size_t n = std::min<std::size_t>(s.size(), 255);
    put<std::uint8_t>(out, std::uint8_t(n));
    out.insert(out.end(), s.begin(), s.begin() + std::ptrdiff_t(n));
}

struct Taker
{
    const std::vector<std::uint8_t>& b;
    std::size_t at = 0;
    bool ok = true;
    template <class T>
    T take()
    {
        T v{};
        if (!ok || at + sizeof(T) > b.size())
        {
            ok = false;
            return v;
        }
        std::memcpy(&v, b.data() + at, sizeof(T));
        at += sizeof(T);
        return v;
    }
    std::string take_string()
    {
        const std::uint8_t n = take<std::uint8_t>();
        if (!ok || at + n > b.size())
        {
            ok = false;
            return {};
        }
        std::string s(b.begin() + std::ptrdiff_t(at), b.begin() + std::ptrdiff_t(at + n));
        at += n;
        return s;
    }
};

double ms_since(std::chrono::steady_clock::time_point t0)
{
    using namespace std::chrono;
    return double(duration_cast<microseconds>(steady_clock::now() - t0).count()) / 1000.0;
}

std::string hex_of(const proto::Uuid128& u)
{
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%016llx%016llx", (unsigned long long)u.hi, (unsigned long long)u.lo);
    return buf;
}

bool uuid_of(const std::string& s, proto::Uuid128& u)
{
    if (s.size() != 32)
        return false;
    unsigned long long hi = 0, lo = 0;
    if (std::sscanf(s.substr(0, 16).c_str(), "%llx", &hi) != 1 || std::sscanf(s.substr(16).c_str(), "%llx", &lo) != 1)
        return false;
    u.hi = hi;
    u.lo = lo;
    return true;
}
} // namespace

std::vector<std::uint8_t> encode_supplemental(const Supplemental& s)
{
    std::vector<std::uint8_t> out;
    out.reserve(64 + s.entities.size() * 10 + s.participants.size() * 64 + s.operations.size() * 40);
    put<std::uint32_t>(out, kSupplementalMagic);
    put<std::uint32_t>(out, kSupplementalSchema);
    put<std::uint64_t>(out, s.campaign.hi);
    put<std::uint64_t>(out, s.campaign.lo);
    put<std::uint64_t>(out, s.party.hi);
    put<std::uint64_t>(out, s.party.lo);
    put<std::uint32_t>(out, s.generation);
    put<std::uint64_t>(out, s.revision);
    put<std::uint64_t>(out, s.epoch);
    put<std::uint64_t>(out, s.tick);
    put<std::uint64_t>(out, s.game_time);
    put<std::uint32_t>(out, s.level_id);
    put_string(out, s.level);
    put<std::uint64_t>(out, s.next_entity);
    put<std::uint8_t>(out, std::uint8_t(std::min<std::size_t>(s.participants.size(), 255)));
    for (std::size_t i = 0; i < s.participants.size() && i < 255; ++i)
    {
        const SupplementalParticipant& p = s.participants[i];
        put<std::uint8_t>(out, p.slot);
        put<std::uint32_t>(out, p.slot_generation);
        put<std::uint64_t>(out, p.player.hi);
        put<std::uint64_t>(out, p.player.lo);
        put<std::uint16_t>(out, p.actor_id);
        put<std::uint64_t>(out, p.actor_entity);
        put<std::uint8_t>(out, p.alive ? 1 : 0);
        put<float>(out, p.position[0]);
        put<float>(out, p.position[1]);
        put<float>(out, p.position[2]);
        put<float>(out, p.entry[0]);
        put<float>(out, p.entry[1]);
        put<float>(out, p.entry[2]);
        put<std::uint32_t>(out, p.entry_vertex);
        put_string(out, p.name);
    }
    put<std::uint32_t>(out, std::uint32_t(s.entities.size()));
    for (const auto& e : s.entities)
    {
        put<std::uint16_t>(out, e.first);
        put<std::uint64_t>(out, e.second);
    }
    put<std::uint32_t>(out, std::uint32_t(s.operations.size()));
    for (const SupplementalOperation& o : s.operations)
    {
        put<std::uint64_t>(out, o.player.hi);
        put<std::uint64_t>(out, o.player.lo);
        put<std::uint64_t>(out, o.operation);
        put<std::uint8_t>(out, o.outcome);
        put<std::uint16_t>(out, o.reason);
        put<std::uint64_t>(out, o.revision);
    }
    put<std::uint32_t>(out, crc32_bytes(out.data(), out.size()));
    return out;
}

bool decode_supplemental(const std::vector<std::uint8_t>& bytes, Supplemental& out, std::string* error)
{
    auto fail = [&](const char* why) {
        if (error)
            *error = why;
        return false;
    };
    if (bytes.size() < 8)
        return fail("short");
    std::uint32_t crc = 0;
    std::memcpy(&crc, bytes.data() + bytes.size() - 4, 4);
    if (crc != crc32_bytes(bytes.data(), bytes.size() - 4))
        return fail("crc");
    Taker t{bytes};
    if (t.take<std::uint32_t>() != kSupplementalMagic)
        return fail("not a supplemental record");
    if (t.take<std::uint32_t>() != kSupplementalSchema)
        return fail("supplemental schema");
    out.campaign.hi = t.take<std::uint64_t>();
    out.campaign.lo = t.take<std::uint64_t>();
    out.party.hi = t.take<std::uint64_t>();
    out.party.lo = t.take<std::uint64_t>();
    out.generation = t.take<std::uint32_t>();
    out.revision = t.take<std::uint64_t>();
    out.epoch = t.take<std::uint64_t>();
    out.tick = t.take<std::uint64_t>();
    out.game_time = t.take<std::uint64_t>();
    out.level_id = t.take<std::uint32_t>();
    out.level = t.take_string();
    out.next_entity = t.take<std::uint64_t>();
    const std::uint8_t participants = t.take<std::uint8_t>();
    out.participants.clear();
    for (std::uint8_t i = 0; i < participants && t.ok; ++i)
    {
        SupplementalParticipant p;
        p.slot = t.take<std::uint8_t>();
        p.slot_generation = t.take<std::uint32_t>();
        p.player.hi = t.take<std::uint64_t>();
        p.player.lo = t.take<std::uint64_t>();
        p.actor_id = t.take<std::uint16_t>();
        p.actor_entity = t.take<std::uint64_t>();
        p.alive = t.take<std::uint8_t>() != 0;
        p.position[0] = t.take<float>();
        p.position[1] = t.take<float>();
        p.position[2] = t.take<float>();
        p.entry[0] = t.take<float>();
        p.entry[1] = t.take<float>();
        p.entry[2] = t.take<float>();
        p.entry_vertex = t.take<std::uint32_t>();
        p.name = t.take_string();
        out.participants.push_back(p);
    }
    const std::uint32_t entities = t.take<std::uint32_t>();
    if (!t.ok || entities > 70000)
        return fail("entities");
    out.entities.clear();
    out.entities.reserve(entities);
    for (std::uint32_t i = 0; i < entities && t.ok; ++i)
    {
        const std::uint16_t id = t.take<std::uint16_t>();
        const std::uint64_t entity = t.take<std::uint64_t>();
        out.entities.push_back({id, entity});
    }
    const std::uint32_t operations = t.take<std::uint32_t>();
    if (!t.ok || operations > 1000000)
        return fail("operations");
    out.operations.clear();
    out.operations.reserve(operations);
    for (std::uint32_t i = 0; i < operations && t.ok; ++i)
    {
        SupplementalOperation o;
        o.player.hi = t.take<std::uint64_t>();
        o.player.lo = t.take<std::uint64_t>();
        o.operation = t.take<std::uint64_t>();
        o.outcome = t.take<std::uint8_t>();
        o.reason = t.take<std::uint16_t>();
        o.revision = t.take<std::uint64_t>();
        out.operations.push_back(o);
    }
    if (!t.ok || t.at != bytes.size() - 4)
        return fail("layout");
    return true;
}

std::string encode_manifest(const Manifest& m)
{
    std::ostringstream o;
    o << "coop-checkpoint-manifest 1\n";
    o << "schema " << m.schema << "\n";
    o << "supplemental_schema " << m.supplemental_schema << "\n";
    o << "journal_schema " << m.journal_schema << "\n";
    o << "campaign " << hex_of(m.campaign) << "\n";
    o << "generation " << m.generation << "\n";
    o << "revision " << m.revision << "\n";
    o << "epoch " << m.epoch << "\n";
    o << "tick " << m.tick << "\n";
    o << "game_time " << m.game_time << "\n";
    o << "level_id " << m.level_id << "\n";
    o << "level " << m.level << "\n";
    o << "build_id " << m.build_id << "\n";
    o << "content_digest " << m.content_digest << "\n";
    o << "protocol_major " << m.protocol_major << "\n";
    o << "published_utc_ms " << m.published_utc_ms << "\n";
    o << "journal " << m.journal << "\n";
    for (const ManifestFile& f : m.files)
        o << "file " << f.name << " " << f.size << " " << f.sha256 << "\n";
    return o.str();
}

bool decode_manifest(const std::string& text, Manifest& out, std::string* error)
{
    auto fail = [&](const std::string& why) {
        if (error)
            *error = why;
        return false;
    };
    std::istringstream in(text);
    std::string line;
    bool first = true;
    out.files.clear();
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        std::istringstream l(line);
        std::string key;
        l >> key;
        if (first)
        {
            std::uint32_t v = 0;
            l >> v;
            if (key != "coop-checkpoint-manifest" || v != 1)
                return fail("not a manifest");
            first = false;
            continue;
        }
        if (key == "schema")
            l >> out.schema;
        else if (key == "supplemental_schema")
            l >> out.supplemental_schema;
        else if (key == "journal_schema")
            l >> out.journal_schema;
        else if (key == "campaign")
        {
            std::string hex;
            l >> hex;
            if (!uuid_of(hex, out.campaign))
                return fail("campaign");
        }
        else if (key == "generation")
            l >> out.generation;
        else if (key == "revision")
            l >> out.revision;
        else if (key == "epoch")
            l >> out.epoch;
        else if (key == "tick")
            l >> out.tick;
        else if (key == "game_time")
            l >> out.game_time;
        else if (key == "level_id")
            l >> out.level_id;
        else if (key == "level")
            l >> out.level;
        else if (key == "build_id")
            l >> out.build_id;
        else if (key == "content_digest")
            l >> out.content_digest;
        else if (key == "protocol_major")
            l >> out.protocol_major;
        else if (key == "published_utc_ms")
            l >> out.published_utc_ms;
        else if (key == "journal")
            l >> out.journal;
        else if (key == "file")
        {
            ManifestFile f;
            l >> f.name >> f.size >> f.sha256;
            if (f.name.empty() || f.sha256.size() != 64)
                return fail("file line");
            out.files.push_back(f);
        }
        // unknown keys are ignored (a later minor addition)
        if (l.fail() && key != "level")
            return fail("value of " + key);
    }
    if (first)
        return fail("empty");
    if (out.generation == 0 || out.files.empty())
        return fail("incomplete");
    return true;
}

std::string StoreNames::generation_file(std::uint32_t generation, const char* extension)
{
    return "coop-gen-" + std::to_string(generation) + "." + extension;
}
std::string StoreNames::journal_file(std::uint32_t generation)
{
    return "coop-journal-" + std::to_string(generation) + ".log";
}
std::string StoreNames::temp_file(std::uint32_t generation, const char* extension)
{
    return "coop-tmp-gen-" + std::to_string(generation) + "." + extension;
}
std::string StoreNames::temp_save_name(std::uint32_t generation) { return "coop-tmp-gen-" + std::to_string(generation); }
std::string StoreNames::save_name(std::uint32_t generation) { return "coop-gen-" + std::to_string(generation); }
const char* StoreNames::pointer_file() { return "coop-checkpoint.current"; }

const char* to_string(RecoverStatus s)
{
    switch (s)
    {
    case RecoverStatus::Fresh: return "fresh";
    case RecoverStatus::Recovered: return "recovered";
    case RecoverStatus::NoPointer: return "no-pointer";
    case RecoverStatus::NoValidGeneration: return "no-valid-generation";
    case RecoverStatus::Incompatible: return "incompatible";
    }
    return "?";
}

CheckpointStore::CheckpointStore(std::string directory, const proto::Uuid128& campaign)
    : dir_(std::move(directory)), campaign_(campaign)
{
}

bool CheckpointStore::read_pointer(std::uint32_t& out) const
{
    std::vector<std::uint8_t> bytes;
    if (!read_file(path_of(StoreNames::pointer_file()), bytes) || bytes.empty() || bytes.size() > 16)
        return false;
    std::string text(bytes.begin(), bytes.end());
    char* end = nullptr;
    const unsigned long v = std::strtoul(text.c_str(), &end, 10);
    if (end == text.c_str() || v == 0 || v > 0xffffffffUL)
        return false;
    out = std::uint32_t(v);
    return true;
}

std::uint32_t CheckpointStore::current_generation() const
{
    std::uint32_t g = 0;
    return read_pointer(g) ? g : 0;
}

std::vector<std::uint32_t> CheckpointStore::generations_present() const
{
    std::vector<std::uint32_t> out;
    for (const std::string& name : list_files(dir_, "coop-gen-"))
    {
        const std::size_t dot = name.rfind(".manifest");
        if (dot == std::string::npos || dot + 9 != name.size())
            continue;
        const unsigned long g = std::strtoul(name.c_str() + 9, nullptr, 10);
        if (g)
            out.push_back(std::uint32_t(g));
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool CheckpointStore::verify_scop_header(const std::string& path, std::uint32_t alife_version, std::string& detail) const
{
    std::vector<std::uint8_t> bytes;
    if (!read_file(path, bytes) || bytes.size() < 12)
    {
        detail = "scop unreadable or short";
        return false;
    }
    std::uint32_t marker = 0, version = 0, source = 0;
    std::memcpy(&marker, bytes.data(), 4);
    std::memcpy(&version, bytes.data() + 4, 4);
    std::memcpy(&source, bytes.data() + 8, 4);
    if (marker != 0xffffffffu || version < alife_version || source == 0)
    {
        detail = "scop header";
        return false;
    }
    return true;
}

bool CheckpointStore::verify_generation(std::uint32_t generation, Manifest& manifest, Supplemental& supplemental,
    std::string& detail) const
{
    std::vector<std::uint8_t> bytes;
    if (!read_file(path_of(StoreNames::generation_file(generation, "manifest")), bytes))
    {
        detail = "no manifest";
        return false;
    }
    std::string why;
    if (!decode_manifest(std::string(bytes.begin(), bytes.end()), manifest, &why))
    {
        detail = "manifest: " + why;
        return false;
    }
    if (manifest.generation != generation)
    {
        detail = "manifest names generation " + std::to_string(manifest.generation);
        return false;
    }
    if (manifest.campaign != campaign_)
    {
        detail = "another campaign";
        return false;
    }
    if (manifest.schema != kManifestSchema || manifest.supplemental_schema != kSupplementalSchema ||
        manifest.journal_schema != kJournalSchema)
    {
        detail = "schema";
        return false;
    }
    bool have_sup = false;
    for (const ManifestFile& f : manifest.files)
    {
        std::vector<std::uint8_t> data;
        if (!read_file(path_of(f.name), data))
        {
            detail = f.name + " unreadable";
            return false;
        }
        if (data.size() != f.size)
        {
            detail = f.name + " size " + std::to_string(data.size()) + " expected " + std::to_string(f.size);
            return false;
        }
        if (sha256_hex(data.data(), data.size()) != f.sha256)
        {
            detail = f.name + " hash";
            return false;
        }
        if (f.name == StoreNames::generation_file(generation, "sup"))
        {
            if (!decode_supplemental(data, supplemental, &why))
            {
                detail = "supplemental: " + why;
                return false;
            }
            if (supplemental.generation != generation || supplemental.campaign != campaign_)
            {
                detail = "supplemental names another generation or campaign";
                return false;
            }
            have_sup = true;
        }
    }
    if (!have_sup)
    {
        detail = "no supplemental file in the manifest";
        return false;
    }
    JournalHeader header;
    const JournalStatus js = read_journal_header(path_of(manifest.journal), header);
    if (js != JournalStatus::Ok)
    {
        detail = "journal header " + std::string(to_string(js));
        return false;
    }
    if (header.campaign != campaign_ || header.generation != generation || header.base_revision != manifest.revision)
    {
        detail = "journal header does not match the manifest";
        return false;
    }
    return true;
}

RecoverStatus CheckpointStore::recover(Recovered& out)
{
    out = Recovered{};
    for (const std::string& name : list_files(dir_, "coop-tmp-"))
    {
        delete_file(path_of(name));
        out.detail.push_back("removed leftover " + name);
    }
    const std::vector<std::uint32_t> present = generations_present();
    std::uint32_t pointed = 0;
    const bool have_pointer = read_pointer(pointed);
    if (!have_pointer)
    {
        if (present.empty())
            return RecoverStatus::Fresh;
        out.detail.push_back("generations present without a pointer");
        return RecoverStatus::NoPointer;
    }
    std::uint32_t chosen = 0;
    bool fallback = false;
    std::string detail;
    for (std::uint32_t candidate : {pointed, pointed > 1 ? pointed - 1 : 0u})
    {
        if (!candidate)
            continue;
        Manifest manifest;
        Supplemental supplemental;
        if (verify_generation(candidate, manifest, supplemental, detail))
        {
            chosen = candidate;
            out.manifest = manifest;
            out.supplemental = supplemental;
            break;
        }
        out.detail.push_back("generation " + std::to_string(candidate) + " refused: " + detail);
        if (detail == "another campaign" || detail == "schema")
            return RecoverStatus::Incompatible;
        fallback = true;
    }
    if (!chosen)
        return RecoverStatus::NoValidGeneration;
    out.generation = chosen;
    out.fallback = fallback;
    out.highest_epoch = out.manifest.epoch;
    out.highest_revision = out.manifest.revision;
    // The chosen generation's journal, every later journal present (a
    // fallback to G-1 must also replay what journal G holds) and the one
    // before it: with an asynchronous publication (S07-T08) the writer stays
    // on journal G-1 until G is published, so records with revisions above
    // R_G can sit there; the revision filter below keeps only those.
    std::vector<std::uint32_t> journals;
    for (const std::string& name : list_files(dir_, "coop-journal-"))
    {
        const unsigned long g = std::strtoul(name.c_str() + 13, nullptr, 10);
        if (g + 1 >= chosen)
            journals.push_back(std::uint32_t(g));
    }
    std::sort(journals.begin(), journals.end());
    std::vector<JournalRecord> all;
    for (std::uint32_t g : journals)
    {
        JournalContents contents;
        const JournalStatus js = read_journal(path_of(StoreNames::journal_file(g)), contents);
        if (js == JournalStatus::TornTail)
            ++out.torn_tails;
        else if (js != JournalStatus::Ok)
        {
            // A corrupt journal at or after the chosen generation could hold
            // acknowledged records: refuse rather than recover without them.
            out.detail.push_back("journal " + std::to_string(g) + " " + to_string(js) + ": " + contents.detail);
            return RecoverStatus::NoValidGeneration;
        }
        if (contents.header.campaign != campaign_ || contents.header.generation != g)
        {
            out.detail.push_back("journal " + std::to_string(g) + " names another generation or campaign");
            continue;
        }
        out.detail.push_back("journal " + std::to_string(g) + ": " + std::to_string(contents.records.size()) +
            " records" + (js == JournalStatus::TornTail ? " (torn tail dropped)" : ""));
        for (JournalRecord& r : contents.records)
        {
            if (r.epoch > out.highest_epoch)
                out.highest_epoch = r.epoch;
            if (r.revision > out.highest_revision)
                out.highest_revision = r.revision;
            all.push_back(std::move(r));
        }
    }
    std::stable_sort(all.begin(), all.end(),
        [](const JournalRecord& a, const JournalRecord& b) { return a.revision < b.revision; });
    for (JournalRecord& r : all)
        if (r.revision > out.manifest.revision && r.kind != kRecordEpoch)
            out.replay.push_back(std::move(r));
    return RecoverStatus::Recovered;
}

bool CheckpointStore::open_journal(std::uint32_t generation, std::uint64_t base_revision, JournalWriter& writer,
    std::string& detail) const
{
    JournalHeader header;
    header.campaign = campaign_;
    header.generation = generation;
    header.base_revision = base_revision;
    const std::string path = path_of(StoreNames::journal_file(generation));
    FileError error;
    if (file_exists(path))
    {
        if (!writer.open_existing(path, header, &error))
        {
            detail = "open journal: " + error.what;
            return false;
        }
        return true;
    }
    if (!writer.create(path, header, &error))
    {
        detail = "create journal: " + error.what;
        return false;
    }
    return true;
}

bool CheckpointStore::publish(std::uint32_t generation, Manifest manifest, const std::vector<std::uint8_t>& supplemental,
    std::uint32_t alife_version, JournalWriter* journal, std::string& detail)
{
    using clock = std::chrono::steady_clock;
    const auto t_start = clock::now();
    last_timing = Timing{};
    FileError error;
    manifest.campaign = campaign_;
    manifest.generation = generation;
    manifest.schema = kManifestSchema;
    manifest.supplemental_schema = kSupplementalSchema;
    manifest.journal_schema = kJournalSchema;
    manifest.journal = StoreNames::journal_file(generation);
    manifest.published_utc_ms = utc_now_ms();
    manifest.files.clear();

    // 1. The supplemental record to its temporary file (the native save is
    //    already under its temporary name).
    const std::string tmp_sup = path_of(StoreNames::temp_file(generation, "sup"));
    if (!write_file_durable(tmp_sup, supplemental.data(), supplemental.size(), &error))
    {
        detail = "supplemental write: " + error.what;
        return false;
    }
    // 2. Verify: read back, sizes, hashes, the .scop header.
    auto t = clock::now();
    const char* extensions[] = {"scop", "scoc", "sup"};
    for (const char* ext : extensions)
    {
        const std::string tmp = path_of(StoreNames::temp_file(generation, ext));
        std::vector<std::uint8_t> data;
        if (!read_file(tmp, data, &error) || data.empty())
        {
            detail = std::string("temporary ") + ext + " unreadable: " + error.what;
            return false;
        }
        ManifestFile f;
        f.name = StoreNames::generation_file(generation, ext);
        f.size = data.size();
        f.sha256 = sha256_hex(data.data(), data.size());
        manifest.files.push_back(f);
    }
    if (!verify_scop_header(path_of(StoreNames::temp_file(generation, "scop")), alife_version, detail))
        return false;
    last_timing.verify_ms = ms_since(t);
    if (!failpoint("checkpoint:after-temp"))
    {
        detail = "failpoint after temp";
        return false;
    }
    // 3. Flush the native save's files (the engine wrote them without a flush).
    t = clock::now();
    for (const char* ext : {"scop", "scoc"})
        if (!flush_file(path_of(StoreNames::temp_file(generation, ext)), &error))
        {
            detail = std::string("flush ") + ext + ": " + error.what;
            return false;
        }
    last_timing.flush_ms = ms_since(t);
    // 4. Rename into place.
    t = clock::now();
    for (const char* ext : extensions)
        if (!rename_file(path_of(StoreNames::temp_file(generation, ext)),
                path_of(StoreNames::generation_file(generation, ext)), true, &error))
        {
            detail = std::string("rename ") + ext + ": " + error.what;
            return false;
        }
    last_timing.rename_ms = ms_since(t);
    if (!failpoint("checkpoint:after-rename"))
    {
        detail = "failpoint after rename";
        return false;
    }
    // 5. Manifest.
    t = clock::now();
    const std::string text = encode_manifest(manifest);
    const std::string tmp_manifest = path_of(StoreNames::temp_file(generation, "manifest"));
    if (!write_file_durable(tmp_manifest, text.data(), text.size(), &error) ||
        !rename_file(tmp_manifest, path_of(StoreNames::generation_file(generation, "manifest")), true, &error))
    {
        detail = "manifest: " + error.what;
        return false;
    }
    last_timing.manifest_ms = ms_since(t);
    if (!failpoint("checkpoint:after-manifest"))
    {
        detail = "failpoint after manifest";
        return false;
    }
    // 6. Journal rotation: the new file with G's header; the old writer closes.
    t = clock::now();
    JournalWriter next;
    JournalHeader header;
    header.campaign = campaign_;
    header.generation = generation;
    header.base_revision = manifest.revision;
    const std::string journal_path = path_of(StoreNames::journal_file(generation));
    if (file_exists(journal_path))
        delete_file(journal_path); // an orphan of a publication that never reached the pointer
    if (!next.create(journal_path, header, &error))
    {
        detail = "journal rotation: " + error.what;
        return false;
    }
    next.close(); // reopened as the caller's writer after the pointer (one handle at a time)
    last_timing.rotate_ms = ms_since(t);
    if (!failpoint("checkpoint:after-rotation"))
    {
        detail = "failpoint after rotation";
        return false;
    }
    // 7. The pointer: the commit point.
    t = clock::now();
    const std::string pointer_text = std::to_string(generation) + "\n";
    const std::string tmp_pointer = path_of("coop-tmp-pointer");
    if (!write_file_durable(tmp_pointer, pointer_text.data(), pointer_text.size(), &error) ||
        !rename_file(tmp_pointer, path_of(StoreNames::pointer_file()), true, &error))
    {
        detail = "pointer: " + error.what;
        return false;
    }
    last_timing.pointer_ms = ms_since(t);
    if (journal)
    {
        journal->reset();
        if (!open_journal(generation, manifest.revision, *journal, detail))
            return false; // published, but the writer could not reopen: the caller fails closed
    }
    if (!failpoint("checkpoint:after-pointer"))
    {
        detail = "failpoint after pointer";
        return false;
    }
    // 8. Retention: everything older than G-1.
    t = clock::now();
    for (std::uint32_t g : generations_present())
        if (g + 1 < generation)
        {
            for (const char* ext : {"scop", "scoc", "sup", "manifest"})
                delete_file(path_of(StoreNames::generation_file(g, ext)));
            delete_file(path_of(StoreNames::journal_file(g)));
        }
    for (const std::string& name : list_files(dir_, "coop-journal-"))
    {
        const unsigned long g = std::strtoul(name.c_str() + 13, nullptr, 10);
        if (g + 1 < generation)
            delete_file(path_of(name));
    }
    last_timing.retention_ms = ms_since(t);
    last_timing.total_ms = ms_since(t_start);
    detail.clear();
    return true;
}
} // namespace coop::store
