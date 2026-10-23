// The co-op store (S07-T02/T03): journal round trips, torn tails,
// corruption, fail-closed appends, checkpoint publication, recovery with
// fallback, publication failpoints, and a forced-termination matrix run in
// a child process (this executable, --filter store/child) that
// TerminateProcess()es itself at an armed failpoint while the parent then
// recovers what it left on disk.
#include "check.h"
#include "coop/store/checkpoint.h"
#include "coop/store/journal.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace coop::test
{
namespace
{
using namespace coop::store;

const proto::Uuid128 kCampaign{0x1111222233334444ULL, 0x5555666677778888ULL};
constexpr std::uint32_t kAlifeVersion = 100;

// Under the current directory (the build directory inside the workspace;
// the runner starts the tests there), never the user's temp folder.
std::string temp_dir(const char* tag)
{
    char base[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, base);
    std::string dir = std::string(base) + "\\coop-store-" + tag + "-" + std::to_string(GetCurrentProcessId()) + "-" +
        std::to_string(GetTickCount64());
    for (const std::string& name : list_files(dir, ""))
        delete_file(join_path(dir, name));
    REQUIRE(ensure_directory(dir));
    return dir;
}

void clean_dir(const std::string& dir)
{
    for (const std::string& name : list_files(dir, ""))
        delete_file(join_path(dir, name));
    RemoveDirectoryA(dir.c_str());
}

std::vector<std::uint8_t> payload_of(std::uint64_t n)
{
    std::vector<std::uint8_t> p(16 + n % 7);
    for (std::size_t i = 0; i < p.size(); ++i)
        p[i] = std::uint8_t(n * 31 + i);
    return p;
}

// A fake native save pair under the temporary names the engine would use.
void write_fake_save(const CheckpointStore& store, std::uint32_t generation, std::uint32_t tag)
{
    std::vector<std::uint8_t> scop(64);
    const std::uint32_t marker = 0xffffffffu, version = kAlifeVersion + 1, source = 1000 + tag;
    std::memcpy(scop.data(), &marker, 4);
    std::memcpy(scop.data() + 4, &version, 4);
    std::memcpy(scop.data() + 8, &source, 4);
    for (std::size_t i = 12; i < scop.size(); ++i)
        scop[i] = std::uint8_t(i + tag);
    REQUIRE(write_file_durable(store.path_of(StoreNames::temp_file(generation, "scop")), scop.data(), scop.size()));
    std::vector<std::uint8_t> scoc(32, std::uint8_t(tag));
    REQUIRE(write_file_durable(store.path_of(StoreNames::temp_file(generation, "scoc")), scoc.data(), scoc.size()));
}

Supplemental fake_supplemental(std::uint32_t generation, std::uint64_t revision)
{
    Supplemental s;
    s.campaign = kCampaign;
    s.party = {7, 7};
    s.generation = generation;
    s.revision = revision;
    s.epoch = 3;
    s.tick = 500 * generation;
    s.game_time = 1234567;
    s.level_id = 1;
    s.level = "l01_escape";
    s.next_entity = 30000 + generation;
    SupplementalParticipant p;
    p.slot = 0;
    p.slot_generation = 1;
    p.player = {1, 2};
    p.actor_id = 0;
    p.actor_entity = 5;
    p.alive = true;
    p.position[0] = 1.5f;
    p.position[1] = 2.5f;
    p.position[2] = -3.5f;
    p.name = "Player 1";
    s.participants.push_back(p);
    p.slot = 1;
    p.player = {3, 4};
    p.actor_id = 27000;
    p.actor_entity = 26999;
    p.alive = false;
    p.name = "Player 2";
    s.participants.push_back(p);
    for (std::uint16_t i = 0; i < 2000; ++i)
        s.entities.push_back({i, std::uint64_t(i) + 1});
    SupplementalOperation o;
    o.player = {1, 2};
    o.operation = 0x10;
    o.outcome = 1;
    o.reason = 0;
    o.revision = revision;
    s.operations.push_back(o);
    return s;
}

Manifest fake_manifest(std::uint32_t generation, std::uint64_t revision)
{
    Manifest m;
    m.generation = generation;
    m.revision = revision;
    m.epoch = 3;
    m.tick = 500 * generation;
    m.game_time = 1234567;
    m.level_id = 1;
    m.level = "l01_escape";
    m.build_id = std::string(64, 'a');
    m.content_digest = std::string(64, 'b');
    m.protocol_major = 1;
    return m;
}

bool publish_fake(CheckpointStore& store, std::uint32_t generation, std::uint64_t revision, JournalWriter& journal,
    std::string& detail)
{
    write_fake_save(store, generation, generation);
    return store.publish(generation, fake_manifest(generation, revision), encode_supplemental(fake_supplemental(generation, revision)),
        kAlifeVersion, journal, detail);
}

std::vector<std::uint64_t> revisions_of(const std::vector<JournalRecord>& records)
{
    std::vector<std::uint64_t> out;
    for (const JournalRecord& r : records)
        out.push_back(r.revision);
    return out;
}

std::string join(const std::vector<std::uint64_t>& v)
{
    std::string s;
    for (std::uint64_t x : v)
        s += (s.empty() ? "" : ",") + std::to_string(x);
    return s;
}
} // namespace

TEST("store/journal-round-trip")
{
    const std::string dir = temp_dir("journal");
    JournalWriter w;
    JournalHeader h;
    h.campaign = kCampaign;
    h.generation = 4;
    h.base_revision = 40;
    const std::string path = join_path(dir, "j.log");
    REQUIRE(w.create(path, h));
    for (std::uint64_t i = 1; i <= 200; ++i)
        REQUIRE(w.append(kRecordLedger, 2, 1000 + i, 40 + i, payload_of(i)));
    metric("journal_append_avg_us", w.total_append_us() / double(w.appends()));
    metric("journal_append_max_us", w.max_append_us());
    w.close();
    JournalContents c;
    CHECK(read_journal(path, c) == JournalStatus::Ok);
    REQUIRE(c.records.size() == 200);
    CHECK(c.header.campaign == kCampaign && c.header.generation == 4 && c.header.base_revision == 40);
    for (std::size_t i = 0; i < c.records.size(); ++i)
    {
        const JournalRecord& r = c.records[i];
        CHECK(r.sequence == i + 1 && r.kind == kRecordLedger && r.epoch == 2 && r.tick == 1001 + i &&
            r.revision == 41 + i && r.payload == payload_of(i + 1));
    }
    // Reopen and continue: the sequence goes on from the last record.
    JournalWriter again;
    REQUIRE(again.open_existing(path, h));
    CHECK(again.next_sequence() == 201);
    REQUIRE(again.append(kRecordDeath, 3, 5000, 300, {}));
    again.close();
    CHECK(read_journal(path, c) == JournalStatus::Ok && c.records.size() == 201 && c.records.back().epoch == 3);
    // Another generation's header is refused.
    JournalHeader other = h;
    other.generation = 5;
    JournalWriter wrong;
    CHECK(!wrong.open_existing(path, other));
    clean_dir(dir);
}

TEST("store/journal-torn-tail-repaired")
{
    const std::string dir = temp_dir("torn");
    const std::string path = join_path(dir, "j.log");
    JournalWriter w;
    JournalHeader h;
    h.campaign = kCampaign;
    h.generation = 1;
    REQUIRE(w.create(path, h));
    for (std::uint64_t i = 1; i <= 5; ++i)
        REQUIRE(w.append(kRecordLedger, 1, i, i, payload_of(i)));
    w.close();
    std::vector<std::uint8_t> bytes;
    REQUIRE(read_file(path, bytes));
    // Cut 3 bytes off the last record: a torn append.
    REQUIRE(truncate_file(path, bytes.size() - 3));
    JournalContents c;
    CHECK(read_journal(path, c) == JournalStatus::TornTail);
    CHECK(c.records.size() == 4);
    // Only the length written: also torn.
    REQUIRE(truncate_file(path, c.good_bytes + 4));
    CHECK(read_journal(path, c) == JournalStatus::TornTail && c.records.size() == 4);
    // Reopening repairs the tail and continues at sequence 5.
    JournalWriter again;
    REQUIRE(again.open_existing(path, h));
    CHECK(again.next_sequence() == 5);
    REQUIRE(again.append(kRecordLedger, 1, 9, 9, payload_of(9)));
    again.close();
    CHECK(read_journal(path, c) == JournalStatus::Ok && c.records.size() == 5 && c.records.back().revision == 9);
    clean_dir(dir);
}

TEST("store/journal-corruption-refused")
{
    const std::string dir = temp_dir("corrupt");
    const std::string path = join_path(dir, "j.log");
    JournalWriter w;
    JournalHeader h;
    h.campaign = kCampaign;
    h.generation = 1;
    REQUIRE(w.create(path, h));
    for (std::uint64_t i = 1; i <= 4; ++i)
        REQUIRE(w.append(kRecordLedger, 1, i, i, payload_of(i)));
    w.close();
    std::vector<std::uint8_t> good;
    REQUIRE(read_file(path, good));
    JournalContents c;
    REQUIRE(read_journal(path, c) == JournalStatus::Ok && c.records.size() == 4);
    // A flipped byte inside record 2 (after it, more records follow): corrupt.
    const std::size_t header_bytes = encode_journal_header(h).size();
    const std::size_t rec1 = encode_journal_record(c.records[0]).size();
    std::vector<std::uint8_t> bad = good;
    bad[header_bytes + rec1 + 20] ^= 0x55;
    REQUIRE(write_file_durable(path, bad.data(), bad.size()));
    CHECK(read_journal(path, c) == JournalStatus::Corrupt);
    JournalWriter refuse;
    CHECK(!refuse.open_existing(path, h));
    // A sequence gap: record 3 rewritten with sequence 5.
    bad = good;
    JournalRecord r = c.records[2];
    r.sequence = 5;
    const std::vector<std::uint8_t> rec3 = encode_journal_record(r);
    const std::size_t at = header_bytes + rec1 + encode_journal_record(c.records[1]).size();
    std::memcpy(bad.data() + at, rec3.data(), rec3.size());
    REQUIRE(write_file_durable(path, bad.data(), bad.size()));
    CHECK(read_journal(path, c) == JournalStatus::Corrupt);
    // A bad header.
    bad = good;
    bad[0] ^= 1;
    REQUIRE(write_file_durable(path, bad.data(), bad.size()));
    CHECK(read_journal(path, c) == JournalStatus::BadHeader);
    // Another campaign in the header.
    JournalHeader other = h;
    other.campaign = {9, 9};
    const std::vector<std::uint8_t> oh = encode_journal_header(other);
    bad = good;
    std::memcpy(bad.data(), oh.data(), oh.size());
    REQUIRE(write_file_durable(path, bad.data(), bad.size()));
    CHECK(read_journal(path, c) == JournalStatus::Ok && c.header.campaign == other.campaign);
    JournalWriter mismatch;
    CHECK(!mismatch.open_existing(path, h));
    CHECK(read_journal(join_path(dir, "none.log"), c) == JournalStatus::Missing);
    clean_dir(dir);
}

TEST("store/journal-append-failure-fails-closed")
{
    const std::string dir = temp_dir("failclosed");
    const std::string path = join_path(dir, "j.log");
    JournalWriter w;
    JournalHeader h;
    h.campaign = kCampaign;
    h.generation = 1;
    REQUIRE(w.create(path, h));
    REQUIRE(w.append(kRecordLedger, 1, 1, 1, payload_of(1)));
    // The second append's write fails ("disk full"): refused, the writer is
    // failed, and every later append is refused without touching the file.
    arm_failpoint("append:before", 1, false);
    FileError error;
    CHECK(!w.append(kRecordLedger, 1, 2, 2, payload_of(2), &error));
    CHECK(w.failed());
    arm_failpoint("", 0, false);
    CHECK(!w.append(kRecordLedger, 1, 3, 3, payload_of(3), &error));
    CHECK(error.what == "journal failed earlier");
    w.close();
    JournalContents c;
    CHECK(read_journal(path, c) == JournalStatus::Ok && c.records.size() == 1);
    // A failure after the data went out but before the flush: the record may
    // be on disk; the writer still refuses from then on and the owner fails
    // closed. Either way the file is readable.
    JournalWriter w2;
    REQUIRE(w2.open_existing(path, h));
    arm_failpoint("append:after-write", 1, false);
    CHECK(!w2.append(kRecordLedger, 1, 2, 2, payload_of(2), &error));
    arm_failpoint("", 0, false);
    CHECK(w2.failed());
    w2.close();
    const JournalStatus s = read_journal(path, c);
    CHECK(s == JournalStatus::Ok || s == JournalStatus::TornTail);
    CHECK(c.records.size() >= 1 && c.records.size() <= 2);
    clean_dir(dir);
}

TEST("store/supplemental-and-manifest-round-trip")
{
    const Supplemental s = fake_supplemental(3, 77);
    const std::vector<std::uint8_t> bytes = encode_supplemental(s);
    Supplemental d;
    std::string error;
    REQUIRE(decode_supplemental(bytes, d, &error));
    CHECK(d.campaign == s.campaign && d.party == s.party && d.generation == 3 && d.revision == 77 && d.epoch == 3);
    CHECK(d.level == "l01_escape" && d.level_id == 1 && d.next_entity == 30003 && d.game_time == 1234567);
    REQUIRE(d.participants.size() == 2 && d.entities.size() == 2000 && d.operations.size() == 1);
    CHECK(d.participants[1].player == (proto::Uuid128{3, 4}) && d.participants[1].actor_id == 27000 &&
        !d.participants[1].alive && d.participants[1].name == "Player 2" && d.participants[0].position[2] == -3.5f);
    CHECK(d.entities[1999].first == 1999 && d.entities[1999].second == 2000);
    CHECK(d.operations[0].operation == 0x10 && d.operations[0].revision == 77);
    std::vector<std::uint8_t> bad = bytes;
    bad[40] ^= 1;
    CHECK(!decode_supplemental(bad, d, &error) && error == "crc");
    bad = bytes;
    bad.resize(bad.size() - 1);
    CHECK(!decode_supplemental(bad, d, &error));

    Manifest m = fake_manifest(3, 77);
    m.campaign = kCampaign;
    m.journal = "coop-journal-3.log";
    m.files.push_back({"coop-gen-3.scop", 64, std::string(64, '0')});
    m.files.push_back({"coop-gen-3.sup", 10, std::string(64, '1')});
    const std::string text = encode_manifest(m);
    Manifest dm;
    REQUIRE(decode_manifest(text, dm, &error));
    CHECK(dm.campaign == kCampaign && dm.generation == 3 && dm.revision == 77 && dm.level == "l01_escape" &&
        dm.build_id == m.build_id && dm.content_digest == m.content_digest && dm.protocol_major == 1 &&
        dm.journal == "coop-journal-3.log" && dm.files.size() == 2 && dm.files[1].size == 10);
    CHECK(!decode_manifest("hello\n", dm, &error));
    CHECK(!decode_manifest(text.substr(0, 40), dm, &error));
    CHECK(decode_manifest(text + "future_key 42\n", dm, &error)); // additions are ignored
}

TEST("store/publish-recover-retention")
{
    const std::string dir = temp_dir("publish");
    CheckpointStore store(dir, kCampaign);
    Recovered r;
    CHECK(store.recover(r) == RecoverStatus::Fresh);
    // Generation 0: the journal before any publication.
    JournalWriter journal;
    std::string detail;
    REQUIRE(store.open_journal(0, 0, journal, detail));
    REQUIRE(journal.append(kRecordEpoch, 1, 0, 0, {}));
    REQUIRE(journal.append(kRecordLedger, 1, 10, 1, payload_of(1)));
    REQUIRE(journal.append(kRecordLedger, 1, 11, 2, payload_of(2)));
    // Publish 1 at revision 2: the journal rotates to coop-journal-1.
    REQUIRE_MSG(publish_fake(store, 1, 2, journal, detail), "%s", detail.c_str());
    CHECK(journal.path() == store.path_of("coop-journal-1.log") && journal.next_sequence() == 1);
    CHECK(store.current_generation() == 1);
    metric("publish_total_ms", store.last_timing.total_ms);
    REQUIRE(journal.append(kRecordLedger, 1, 20, 3, payload_of(3)));
    REQUIRE(publish_fake(store, 2, 3, journal, detail));
    REQUIRE(journal.append(kRecordLedger, 1, 30, 4, payload_of(4)));
    REQUIRE(journal.append(kRecordDeath, 1, 31, 5, payload_of(5)));
    REQUIRE(publish_fake(store, 3, 5, journal, detail));
    REQUIRE(journal.append(kRecordLedger, 1, 40, 6, payload_of(6)));
    journal.close();
    // Retention: 1 and its journal gone, 2 and 3 kept, journal 0 gone.
    const std::vector<std::uint32_t> present = store.generations_present();
    REQUIRE(present.size() == 2);
    CHECK(present[0] == 2 && present[1] == 3);
    CHECK(!file_exists(store.path_of("coop-journal-0.log")) && !file_exists(store.path_of("coop-journal-1.log")));
    CHECK(file_exists(store.path_of("coop-journal-2.log")) && file_exists(store.path_of("coop-journal-3.log")));
    CHECK(list_files(dir, "coop-tmp-").empty());
    // Recovery: generation 3, replay only revision 6.
    REQUIRE(store.recover(r) == RecoverStatus::Recovered);
    CHECK(r.generation == 3 && !r.fallback && r.manifest.revision == 5 && r.supplemental.generation == 3);
    CHECK(join(revisions_of(r.replay)) == "6");
    CHECK(r.highest_revision == 6 && r.highest_epoch == 3);
    CHECK(r.supplemental.entities.size() == 2000 && r.manifest.files.size() == 3);
    // Fallback: generation 3's save corrupted on disk -> 2, replay 4, 5 from
    // journal 2 and 6 from journal 3.
    std::vector<std::uint8_t> scop;
    REQUIRE(read_file(store.path_of("coop-gen-3.scop"), scop));
    scop[20] ^= 0xff;
    REQUIRE(write_file_durable(store.path_of("coop-gen-3.scop"), scop.data(), scop.size()));
    REQUIRE(store.recover(r) == RecoverStatus::Recovered);
    CHECK(r.generation == 2 && r.fallback);
    CHECK_MSG(join(revisions_of(r.replay)) == "4,5,6", "%s", join(revisions_of(r.replay)).c_str());
    // Both unreadable: refused, nothing deleted.
    REQUIRE(delete_file(store.path_of("coop-gen-2.manifest")));
    CHECK(store.recover(r) == RecoverStatus::NoValidGeneration);
    CHECK(file_exists(store.path_of("coop-gen-3.scop")) && file_exists(store.path_of("coop-gen-2.scop")));
    // No pointer with generations present: refused.
    REQUIRE(delete_file(store.path_of(StoreNames::pointer_file())));
    CHECK(store.recover(r) == RecoverStatus::NoPointer);
    // Another campaign's store: incompatible.
    const std::string text = std::string("3\n");
    REQUIRE(write_file_durable(store.path_of(StoreNames::pointer_file()), text.data(), text.size()));
    scop[20] ^= 0xff;
    REQUIRE(write_file_durable(store.path_of("coop-gen-3.scop"), scop.data(), scop.size()));
    REQUIRE(store.recover(r) == RecoverStatus::Recovered && r.generation == 3);
    CheckpointStore other(dir, proto::Uuid128{1, 1});
    CHECK(other.recover(r) == RecoverStatus::Incompatible);
    // A corrupt journal of the chosen generation refuses the recovery.
    std::vector<std::uint8_t> j3;
    REQUIRE(read_file(store.path_of("coop-journal-3.log"), j3));
    j3[j3.size() / 2] ^= 0x0f;
    j3.push_back(1);
    REQUIRE(write_file_durable(store.path_of("coop-journal-3.log"), j3.data(), j3.size()));
    CHECK(store.recover(r) == RecoverStatus::NoValidGeneration);
    clean_dir(dir);
}

TEST("store/publish-failures-keep-the-previous-generation")
{
    const std::string dir = temp_dir("pubfail");
    CheckpointStore store(dir, kCampaign);
    JournalWriter journal;
    std::string detail;
    REQUIRE(store.open_journal(0, 0, journal, detail));
    REQUIRE(journal.append(kRecordLedger, 1, 1, 1, payload_of(1)));
    REQUIRE(publish_fake(store, 1, 1, journal, detail));
    REQUIRE(journal.append(kRecordLedger, 1, 2, 2, payload_of(2)));
    const char* points[] = {"write:before", "rename:before", "checkpoint:after-temp", "checkpoint:after-rename",
        "checkpoint:after-manifest", "checkpoint:after-rotation"};
    for (const char* point : points)
    {
        // The fake native save is written before the point is armed: the
        // engine writes it outside the store, and the store's own writes
        // (the supplemental, the manifest, the pointer) are what fail here.
        write_fake_save(store, 2, 2);
        arm_failpoint(point, 1, false);
        const bool ok = store.publish(2, fake_manifest(2, 2), encode_supplemental(fake_supplemental(2, 2)),
            kAlifeVersion, journal, detail);
        arm_failpoint("", 0, false);
        CHECK_MSG(!ok, "%s should have failed", point);
        CHECK_MSG(store.current_generation() == 1, "%s moved the pointer", point);
        // The writer is still generation 1's and still works.
        CHECK_MSG(journal.path() == store.path_of("coop-journal-1.log"), "%s rotated the writer", point);
        CHECK_MSG(journal.append(kRecordLedger, 1, 3, 3, payload_of(3)), "%s broke the writer", point);
        Recovered r;
        CHECK_MSG(store.recover(r) == RecoverStatus::Recovered && r.generation == 1, "%s broke recovery", point);
    }
    // The same generation publishes cleanly afterwards (temporaries and
    // orphans overwritten).
    REQUIRE_MSG(publish_fake(store, 2, 9, journal, detail), "%s", detail.c_str());
    CHECK(store.current_generation() == 2);
    Recovered r;
    REQUIRE(store.recover(r) == RecoverStatus::Recovered);
    CHECK(r.generation == 2 && r.replay.empty());
    // A scop that is not a saved game is refused at verification.
    const std::string junk = "junk";
    REQUIRE(write_file_durable(store.path_of(StoreNames::temp_file(3, "scop")), junk.data(), junk.size()));
    REQUIRE(write_file_durable(store.path_of(StoreNames::temp_file(3, "scoc")), junk.data(), junk.size()));
    CHECK(!store.publish(3, fake_manifest(3, 9), encode_supplemental(fake_supplemental(3, 9)), kAlifeVersion, journal, detail));
    CHECK(detail.find("scop") != std::string::npos);
    CHECK(store.current_generation() == 2);
    journal.close();
    clean_dir(dir);
}

// --- forced termination -----------------------------------------------------

// The child: runs the script and terminates itself at the armed point.
// Script: journal 0 (epoch, revisions 1-3), publish 1 at 3, revisions 4-5,
// then the step under test: publish 2 at 5 (checkpoint points) or append
// revision 6 (append points).
TEST("store/child")
{
    char dir[MAX_PATH] = {}, point[128] = {};
    if (!GetEnvironmentVariableA("COOP_STORE_CHILD_DIR", dir, sizeof(dir)) ||
        !GetEnvironmentVariableA("COOP_STORE_CHILD_POINT", point, sizeof(point)))
        return; // not a child
    CheckpointStore store(dir, kCampaign);
    JournalWriter journal;
    std::string detail;
    REQUIRE(store.open_journal(0, 0, journal, detail));
    REQUIRE(journal.append(kRecordEpoch, 1, 0, 0, {}));
    for (std::uint64_t i = 1; i <= 3; ++i)
        REQUIRE(journal.append(kRecordLedger, 1, i, i, payload_of(i)));
    REQUIRE(publish_fake(store, 1, 3, journal, detail));
    for (std::uint64_t i = 4; i <= 5; ++i)
        REQUIRE(journal.append(kRecordLedger, 1, i, i, payload_of(i)));
    arm_failpoint(point, 1, true);
    if (!std::strncmp(point, "append:", 7))
        journal.append(kRecordLedger, 1, 6, 6, payload_of(6));
    else
        publish_fake(store, 2, 5, journal, detail);
    // Only reached when the point was not hit.
    arm_failpoint("", 0, false);
    std::fprintf(stderr, "child: point %s not hit\n", point);
    TerminateProcess(GetCurrentProcess(), 0xBAD);
}

TEST("store/termination-matrix")
{
    char exe[MAX_PATH];
    REQUIRE(GetModuleFileNameA(nullptr, exe, MAX_PATH));
    struct Case
    {
        const char* point;
        std::uint32_t pointer;          // expected current generation after
        const char* replay_min;         // replay must contain at least these
        const char* replay_max;         // and at most these
    };
    const Case cases[] = {
        {"append:before", 1, "4,5", "4,5"},
        {"append:after-write", 1, "4,5", "4,5,6"},
        {"append:after-flush", 1, "4,5,6", "4,5,6"},
        {"checkpoint:after-temp", 1, "4,5", "4,5"},
        {"checkpoint:after-rename", 1, "4,5", "4,5"},
        {"checkpoint:after-manifest", 1, "4,5", "4,5"},
        {"checkpoint:after-rotation", 1, "4,5", "4,5"},
        {"checkpoint:after-pointer", 2, "", ""},
    };
    for (const Case& c : cases)
    {
        const std::string dir = temp_dir("term");
        SetEnvironmentVariableA("COOP_STORE_CHILD_DIR", dir.c_str());
        SetEnvironmentVariableA("COOP_STORE_CHILD_POINT", c.point);
        std::string command = std::string("\"") + exe + "\" --filter store/child";
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        REQUIRE(CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
            &si, &pi));
        CHECK(WaitForSingleObject(pi.hProcess, 60000) == WAIT_OBJECT_0);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        SetEnvironmentVariableA("COOP_STORE_CHILD_DIR", nullptr);
        SetEnvironmentVariableA("COOP_STORE_CHILD_POINT", nullptr);
        CHECK_MSG(code == 0xDEAD, "%s: child exit 0x%lx (0xDEAD = terminated at the point)", c.point, code);
        CheckpointStore store(dir, kCampaign);
        Recovered r;
        const RecoverStatus status = store.recover(r);
        CHECK_MSG(status == RecoverStatus::Recovered, "%s: %s", c.point, to_string(status));
        CHECK_MSG(r.generation == c.pointer, "%s: generation %u", c.point, r.generation);
        const std::string replay = join(revisions_of(r.replay));
        const bool within = replay == c.replay_min || replay == c.replay_max;
        CHECK_MSG(within, "%s: replay %s (expected %s or %s)", c.point, replay.c_str(), c.replay_min, c.replay_max);
        note(std::string(c.point) + ": generation " + std::to_string(r.generation) + " replay [" + replay + "]" +
            (r.torn_tails ? " torn tail dropped" : ""));
        // The recovered journal reopens and takes a record (a torn tail is
        // cut), and the next publication cleans the leftovers.
        JournalWriter journal;
        std::string detail;
        CHECK_MSG(store.open_journal(r.generation, r.manifest.revision, journal, detail), "%s: %s", c.point, detail.c_str());
        CHECK(journal.append(kRecordLedger, 2, 100, r.highest_revision + 1, payload_of(100)));
        CHECK_MSG(publish_fake(store, r.generation + 1, r.highest_revision + 1, journal, detail), "%s: %s", c.point,
            detail.c_str());
        CHECK(list_files(dir, "coop-tmp-").empty());
        journal.close();
        clean_dir(dir);
    }
}
} // namespace coop::test
