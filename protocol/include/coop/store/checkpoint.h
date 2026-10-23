// Checkpoint generations (S07-T02), docs/persistence-contract.md §5 and §7.
//
// The store owns the files in one directory:
//   coop-gen-<G>.scop / .scoc      the native save (written by the engine
//                                   under a temporary name, published here)
//   coop-gen-<G>.sup               the supplemental record
//   coop-gen-<G>.manifest          text manifest: sizes, SHA-256, revision...
//   coop-journal-<G>.log           the journal opened at G's publication
//   coop-checkpoint.current        "<G>\n": the one file that names the
//                                   current generation
//   coop-tmp-*                      publication work files
//
// publish() runs steps 2-8 of the contract: verify, flush, rename, manifest,
// journal rotation, pointer, retention. recover() runs steps 1-3: pointer,
// verification with fallback to G-1, journal reading. The engine does the
// native save (step 1) and the loading (steps 5-7).
#pragma once

#include "coop/proto/wire.h"
#include "coop/store/journal.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace coop::store
{
constexpr std::uint32_t kSupplementalMagic = 0x50555343; // "CSUP"
constexpr std::uint32_t kSupplementalSchema = 2; // 2 (S07-T08/T07): the participant's level entry point
constexpr std::uint32_t kManifestSchema = 1;

struct SupplementalParticipant
{
    std::uint8_t slot = 0;
    std::uint32_t slot_generation = 0;
    proto::Uuid128 player;
    std::uint16_t actor_id = 0xffff; // engine u16, 0xffff none
    std::uint64_t actor_entity = 0;
    bool alive = false;
    float position[3] = {0, 0, 0};
    float entry[3] = {0, 0, 0};              // schema 2: where the participant entered this level (the respawn point)
    std::uint32_t entry_vertex = 0xffffffffu; // level vertex of the entry point (0xffffffff: unknown)
    std::string name;
};

struct SupplementalOperation
{
    proto::Uuid128 player;
    std::uint64_t operation = 0;
    std::uint8_t outcome = 0; // proto::ActionOutcome
    std::uint16_t reason = 0;
    std::uint64_t revision = 0;
};

// Everything the native save does not hold (contract §5.1).
struct Supplemental
{
    proto::Uuid128 campaign, party;
    std::uint32_t generation = 0;
    std::uint64_t revision = 0, epoch = 0, tick = 0, game_time = 0;
    std::uint32_t level_id = 0;
    std::string level;
    std::uint64_t next_entity = 0;
    std::vector<SupplementalParticipant> participants;
    std::vector<std::pair<std::uint16_t, std::uint64_t>> entities; // u16 -> EntityId
    std::vector<SupplementalOperation> operations;
};
std::vector<std::uint8_t> encode_supplemental(const Supplemental& s);
bool decode_supplemental(const std::vector<std::uint8_t>& bytes, Supplemental& out, std::string* error = nullptr);

struct ManifestFile
{
    std::string name;
    std::uint64_t size = 0;
    std::string sha256;
};

struct Manifest
{
    std::uint32_t schema = kManifestSchema;
    std::uint32_t supplemental_schema = kSupplementalSchema;
    std::uint32_t journal_schema = kJournalSchema;
    proto::Uuid128 campaign;
    std::uint32_t generation = 0;
    std::uint64_t revision = 0, epoch = 0, tick = 0, game_time = 0;
    std::uint32_t level_id = 0;
    std::string level;
    std::string build_id;       // hex
    std::string content_digest; // hex
    std::uint16_t protocol_major = 0;
    std::uint64_t published_utc_ms = 0;
    std::vector<ManifestFile> files; // scop, scoc, sup
    std::string journal;             // coop-journal-<G>.log
};
std::string encode_manifest(const Manifest& m);
bool decode_manifest(const std::string& text, Manifest& out, std::string* error = nullptr);

struct StoreNames
{
    static std::string generation_file(std::uint32_t generation, const char* extension); // coop-gen-<G>.<ext>
    static std::string journal_file(std::uint32_t generation);                           // coop-journal-<G>.log
    static std::string temp_file(std::uint32_t generation, const char* extension);       // coop-tmp-gen-<G>.<ext>
    static std::string temp_save_name(std::uint32_t generation);                         // coop-tmp-gen-<G>
    static std::string save_name(std::uint32_t generation);                              // coop-gen-<G>
    static const char* pointer_file();                                                   // coop-checkpoint.current
};

enum class RecoverStatus : std::uint8_t
{
    Fresh,          // no pointer and no generation: first start
    Recovered,      // a generation verified (out.generation; out.fallback says G-1 was used)
    NoPointer,      // generations exist but no pointer: refused
    NoValidGeneration, // neither G nor G-1 verified: refused
    Incompatible,   // the pointed generation is of another campaign or schema: refused
};
const char* to_string(RecoverStatus s);

struct Recovered
{
    std::uint32_t generation = 0;
    bool fallback = false; // G-1 was used because G did not verify
    Manifest manifest;
    Supplemental supplemental;
    std::vector<JournalRecord> replay;    // records with revision > manifest.revision, in revision order
    std::uint64_t highest_epoch = 0;      // over the manifest and every journal read
    std::uint64_t highest_revision = 0;   // max(manifest.revision, records)
    std::uint32_t torn_tails = 0;
    std::vector<std::string> detail;      // what was checked and why something was skipped
};

class CheckpointStore
{
public:
    CheckpointStore(std::string directory, const proto::Uuid128& campaign);
    const std::string& directory() const { return dir_; }

    // Step 1-3 of recovery. Also removes coop-tmp-* leftovers.
    RecoverStatus recover(Recovered& out);
    // The current generation per the pointer (0 when none).
    std::uint32_t current_generation() const;
    bool read_pointer(std::uint32_t& out) const;

    // Publication of generation G from the temporary files the engine wrote:
    // coop-tmp-gen-<G>.scop and .scoc (the native save under
    // temp_save_name(G)) and the supplemental bytes given here. Fills the
    // manifest's files, journal and published time; verifies the .scop
    // header (the engine's "valid saved game" rule: u32(-1) then a version
    // >= alife_version); flushes, renames, writes the manifest, rotates the
    // journal (with a writer, it is closed and reopened on the new file with
    // the header of G; with null the new journal is only created and the
    // caller switches its writer later with open_journal: S07-T08's
    // asynchronous publication, where the simulation thread keeps appending
    // to the previous journal meanwhile), replaces the pointer, applies
    // retention. On failure before the pointer step nothing published
    // changes and the writer stays on its old file; detail says which step
    // failed. Safe to run on another thread than the writer's as long as
    // the writer is not passed in.
    bool publish(std::uint32_t generation, Manifest manifest, const std::vector<std::uint8_t>& supplemental,
        std::uint32_t alife_version, JournalWriter* journal, std::string& detail);

    // Verifies one generation fully (manifest, sizes, hashes, .sup decode,
    // journal header). detail says what failed.
    bool verify_generation(std::uint32_t generation, Manifest& manifest, Supplemental& supplemental,
        std::string& detail) const;
    // Opens (or creates for generation 0) the journal of a generation for
    // appending: the header must match.
    bool open_journal(std::uint32_t generation, std::uint64_t base_revision, JournalWriter& writer,
        std::string& detail) const;
    std::vector<std::uint32_t> generations_present() const; // from manifests, ascending
    std::string path_of(const std::string& name) const { return join_path(dir_, name); }

    struct Timing
    {
        double verify_ms = 0, flush_ms = 0, rename_ms = 0, manifest_ms = 0, rotate_ms = 0, pointer_ms = 0,
               retention_ms = 0, total_ms = 0;
    } last_timing;

private:
    bool verify_scop_header(const std::string& path, std::uint32_t alife_version, std::string& detail) const;
    std::string dir_;
    proto::Uuid128 campaign_;
};
} // namespace coop::store
