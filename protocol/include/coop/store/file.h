// Durable file primitives for the co-op store (S07-T02/T03), on the Windows
// file API only. Everything the journal and the checkpoint store need:
//
//   - write a whole file durably: write, FlushFileBuffers, close;
//   - append to a file opened with FILE_FLAG_WRITE_THROUGH and flush it;
//   - rename with MOVEFILE_WRITE_THROUGH (and REPLACE_EXISTING for the
//     pointer file), which is the publication's commit point;
//   - read a whole file, list a directory, delete.
//
// What these give and do not give is written down in
// docs/persistence-contract.md §5: FlushFileBuffers waits for the device to
// report the data and metadata written; MoveFileExW on one NTFS volume
// replaces a name atomically with respect to other openers. Tested against
// process termination; power loss is not tested and not claimed.
//
// Fault injection (tests and the engine's failpoints): every primitive asks
// the failpoint hook before and after its system call. A hook may fail the
// call ("disk full", "write error") or terminate the process at the point.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace coop::store
{
// Failpoints. Names are "<operation>:<phase>" for the primitives
// ("append:after-write", "rename:before") and whatever the engine adds for
// its own boundaries ("commit:after-flush"). The hook decides: Pass, Fail
// (the operation reports an error) or Terminate (TerminateProcess now).
enum class FailAction : std::uint8_t
{
    Pass,
    Fail,
    Terminate,
};
using FailHook = std::function<FailAction(const char* point)>;
void set_fail_hook(FailHook hook); // null clears it
// Asks the hook; terminates the process itself on Terminate. Returns false
// on Fail. Cheap when no hook is set.
bool failpoint(const char* point);
// A ready-made hook: the named point terminates the process (or fails, with
// terminate=false) on its count-th hit. Pass an empty name to clear.
void arm_failpoint(const std::string& point, std::uint32_t count, bool terminate);
std::uint64_t failpoint_hits(); // hits of the armed point so far

struct FileError
{
    std::string what; // empty: no error
    std::uint32_t code = 0; // GetLastError
    explicit operator bool() const { return !what.empty(); }
};

// Whole-file operations.
bool read_file(const std::string& path, std::vector<std::uint8_t>& out, FileError* error = nullptr);
// Writes to the path (truncating), flushes and closes. Not atomic by itself:
// write to a temporary name and rename_file() it into place.
bool write_file_durable(const std::string& path, const void* data, std::size_t size, FileError* error = nullptr);
bool file_exists(const std::string& path);
bool file_size(const std::string& path, std::uint64_t& out);
bool delete_file(const std::string& path, FileError* error = nullptr);
// MoveFileExW with MOVEFILE_WRITE_THROUGH; replace=true adds
// MOVEFILE_REPLACE_EXISTING (the pointer file).
bool rename_file(const std::string& from, const std::string& to, bool replace, FileError* error = nullptr);
// Flushes an existing file's data and metadata (FlushFileBuffers on a
// handle opened for write access).
bool flush_file(const std::string& path, FileError* error = nullptr);
// Cuts a file to the given size (SetEndOfFile) and flushes it.
bool truncate_file(const std::string& path, std::uint64_t size, FileError* error = nullptr);
// Names (not paths) of the files in a directory matching a prefix.
std::vector<std::string> list_files(const std::string& dir, const std::string& prefix);
bool ensure_directory(const std::string& dir);

// An append-only file opened with FILE_FLAG_WRITE_THROUGH. Each append()
// writes the bytes and flushes (FlushFileBuffers); a failure of either marks
// the file failed and every later append refuses (fail closed; the owner
// decides what that means).
class AppendFile
{
public:
    AppendFile() = default;
    ~AppendFile();
    AppendFile(const AppendFile&) = delete;
    AppendFile& operator=(const AppendFile&) = delete;
    AppendFile(AppendFile&& o) noexcept;
    AppendFile& operator=(AppendFile&& o) noexcept;
    // create=true refuses an existing file; false opens an existing one at its end.
    bool open(const std::string& path, bool create, FileError* error = nullptr);
    bool append(const void* data, std::size_t size, FileError* error = nullptr);
    bool is_open() const { return handle_ != nullptr; }
    bool failed() const { return failed_; }
    std::uint64_t size() const { return size_; } // bytes written or found at open
    const std::string& path() const { return path_; }
    void close();
    // Microseconds the last append spent in WriteFile + FlushFileBuffers.
    double last_append_us() const { return last_us_; }

private:
    void* handle_ = nullptr; // HANDLE
    std::string path_;
    std::uint64_t size_ = 0;
    bool failed_ = false;
    double last_us_ = 0;
};

std::string join_path(const std::string& dir, const std::string& name);
std::uint32_t crc32_bytes(const void* data, std::size_t size);
// SHA-256 as 64 lowercase hex digits (BCrypt).
std::string sha256_hex(const void* data, std::size_t size);
std::uint64_t utc_now_ms();
} // namespace coop::store
