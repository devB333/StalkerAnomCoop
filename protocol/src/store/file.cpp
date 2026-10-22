// Durable file primitives (S07-T02/T03): see coop/store/file.h.
#include "coop/store/file.h"

#include "coop/proto/wire.h"

#include <windows.h>
#include <bcrypt.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <utility>

namespace coop::store
{
namespace
{
std::mutex g_hook_mutex;
FailHook g_hook;
std::string g_armed;
std::uint32_t g_armed_count = 0;
bool g_armed_terminate = true;
std::atomic<std::uint64_t> g_armed_hits{0};
std::atomic<std::uint64_t> g_armed_seen{0};

std::wstring wide(const std::string& s)
{
    if (s.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w)
{
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

void set_error(FileError* error, const char* what, DWORD code)
{
    if (!error)
        return;
    error->what = what;
    error->code = code;
}

double now_us()
{
    using namespace std::chrono;
    return double(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}
} // namespace

void set_fail_hook(FailHook hook)
{
    std::lock_guard<std::mutex> lock(g_hook_mutex);
    g_hook = std::move(hook);
}

void arm_failpoint(const std::string& point, std::uint32_t count, bool terminate)
{
    std::lock_guard<std::mutex> lock(g_hook_mutex);
    g_armed = point;
    g_armed_count = count;
    g_armed_terminate = terminate;
    g_armed_hits = 0;
    g_armed_seen = 0;
    if (point.empty())
    {
        g_hook = nullptr;
        return;
    }
    g_hook = [](const char* p) -> FailAction {
        if (g_armed != p)
            return FailAction::Pass;
        const std::uint64_t seen = ++g_armed_seen;
        if (seen != g_armed_count)
            return FailAction::Pass;
        ++g_armed_hits;
        return g_armed_terminate ? FailAction::Terminate : FailAction::Fail;
    };
}

std::uint64_t failpoint_hits() { return g_armed_hits.load(); }

bool failpoint(const char* point)
{
    FailHook hook;
    {
        std::lock_guard<std::mutex> lock(g_hook_mutex);
        if (!g_hook)
            return true;
        hook = g_hook;
    }
    switch (hook(point))
    {
    case FailAction::Pass: return true;
    case FailAction::Fail: return false;
    case FailAction::Terminate:
        // The whole point: no destructors, no flush, no log line.
        TerminateProcess(GetCurrentProcess(), 0xDEAD);
        return false;
    }
    return true;
}

std::string join_path(const std::string& dir, const std::string& name)
{
    if (dir.empty())
        return name;
    const char last = dir.back();
    return (last == '\\' || last == '/') ? dir + name : dir + "\\" + name;
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& out, FileError* error)
{
    HANDLE h = CreateFileW(wide(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        set_error(error, "open for read", GetLastError());
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart > 0x7fffffff)
    {
        set_error(error, "size", GetLastError());
        CloseHandle(h);
        return false;
    }
    out.resize(size_t(size.QuadPart));
    std::size_t at = 0;
    while (at < out.size())
    {
        DWORD got = 0;
        if (!ReadFile(h, out.data() + at, DWORD(out.size() - at), &got, nullptr) || got == 0)
        {
            set_error(error, "read", GetLastError());
            CloseHandle(h);
            return false;
        }
        at += got;
    }
    CloseHandle(h);
    return true;
}

bool write_file_durable(const std::string& path, const void* data, std::size_t size, FileError* error)
{
    if (!failpoint("write:before"))
    {
        set_error(error, "write refused (failpoint)", 0);
        return false;
    }
    HANDLE h = CreateFileW(wide(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        set_error(error, "create", GetLastError());
        return false;
    }
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    std::size_t at = 0;
    while (at < size)
    {
        DWORD put = 0;
        if (!WriteFile(h, p + at, DWORD(size - at), &put, nullptr) || put == 0)
        {
            set_error(error, "write", GetLastError());
            CloseHandle(h);
            return false;
        }
        at += put;
    }
    if (!failpoint("write:after-write"))
    {
        set_error(error, "write failed after the data (failpoint)", 0);
        CloseHandle(h);
        return false;
    }
    if (!FlushFileBuffers(h))
    {
        set_error(error, "flush", GetLastError());
        CloseHandle(h);
        return false;
    }
    CloseHandle(h);
    return failpoint("write:after-flush");
}

bool file_exists(const std::string& path)
{
    const DWORD a = GetFileAttributesW(wide(path).c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool file_size(const std::string& path, std::uint64_t& out)
{
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExW(wide(path).c_str(), GetFileExInfoStandard, &d))
        return false;
    out = (std::uint64_t(d.nFileSizeHigh) << 32) | d.nFileSizeLow;
    return true;
}

bool delete_file(const std::string& path, FileError* error)
{
    if (DeleteFileW(wide(path).c_str()))
        return true;
    const DWORD code = GetLastError();
    if (code == ERROR_FILE_NOT_FOUND)
        return true;
    set_error(error, "delete", code);
    return false;
}

bool rename_file(const std::string& from, const std::string& to, bool replace, FileError* error)
{
    if (!failpoint("rename:before"))
    {
        set_error(error, "rename refused (failpoint)", 0);
        return false;
    }
    DWORD flags = MOVEFILE_WRITE_THROUGH;
    if (replace)
        flags |= MOVEFILE_REPLACE_EXISTING;
    if (!MoveFileExW(wide(from).c_str(), wide(to).c_str(), flags))
    {
        set_error(error, "rename", GetLastError());
        return false;
    }
    return failpoint("rename:after");
}

bool flush_file(const std::string& path, FileError* error)
{
    HANDLE h = CreateFileW(wide(path).c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        set_error(error, "open for flush", GetLastError());
        return false;
    }
    const bool ok = FlushFileBuffers(h) != 0;
    if (!ok)
        set_error(error, "flush", GetLastError());
    CloseHandle(h);
    return ok;
}

bool truncate_file(const std::string& path, std::uint64_t size, FileError* error)
{
    HANDLE h = CreateFileW(wide(path).c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        set_error(error, "open for truncate", GetLastError());
        return false;
    }
    LARGE_INTEGER at{};
    at.QuadPart = LONGLONG(size);
    bool ok = SetFilePointerEx(h, at, nullptr, FILE_BEGIN) && SetEndOfFile(h) && FlushFileBuffers(h);
    if (!ok)
        set_error(error, "truncate", GetLastError());
    CloseHandle(h);
    return ok;
}

std::vector<std::string> list_files(const std::string& dir, const std::string& prefix)
{
    std::vector<std::string> out;
    WIN32_FIND_DATAW d{};
    HANDLE h = FindFirstFileW(wide(join_path(dir, prefix + "*")).c_str(), &d);
    if (h == INVALID_HANDLE_VALUE)
        return out;
    do
    {
        if (!(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            out.push_back(narrow(d.cFileName));
    } while (FindNextFileW(h, &d));
    FindClose(h);
    return out;
}

bool ensure_directory(const std::string& dir)
{
    const DWORD a = GetFileAttributesW(wide(dir).c_str());
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY))
        return true;
    return CreateDirectoryW(wide(dir).c_str(), nullptr) != 0;
}

AppendFile::~AppendFile() { close(); }

AppendFile::AppendFile(AppendFile&& o) noexcept { *this = std::move(o); }

AppendFile& AppendFile::operator=(AppendFile&& o) noexcept
{
    if (this != &o)
    {
        close();
        handle_ = o.handle_;
        path_ = std::move(o.path_);
        size_ = o.size_;
        failed_ = o.failed_;
        last_us_ = o.last_us_;
        o.handle_ = nullptr;
    }
    return *this;
}

bool AppendFile::open(const std::string& path, bool create, FileError* error)
{
    close();
    HANDLE h = CreateFileW(wide(path).c_str(), FILE_APPEND_DATA | GENERIC_READ, FILE_SHARE_READ, nullptr,
        create ? CREATE_NEW : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        set_error(error, create ? "create append file" : "open append file", GetLastError());
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size))
    {
        set_error(error, "size", GetLastError());
        CloseHandle(h);
        return false;
    }
    handle_ = h;
    path_ = path;
    size_ = std::uint64_t(size.QuadPart);
    failed_ = false;
    return true;
}

bool AppendFile::append(const void* data, std::size_t size, FileError* error)
{
    if (!handle_ || failed_)
    {
        set_error(error, handle_ ? "append file failed earlier" : "append file not open", 0);
        return false;
    }
    const double t0 = now_us();
    if (!failpoint("append:before"))
    {
        failed_ = true;
        set_error(error, "append refused (failpoint)", 0);
        return false;
    }
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    std::size_t at = 0;
    while (at < size)
    {
        DWORD put = 0;
        if (!WriteFile(handle_, p + at, DWORD(size - at), &put, nullptr) || put == 0)
        {
            failed_ = true;
            set_error(error, "append write", GetLastError());
            return false;
        }
        at += put;
    }
    if (!failpoint("append:after-write"))
    {
        failed_ = true;
        set_error(error, "append failed after the data (failpoint)", 0);
        return false;
    }
    if (!FlushFileBuffers(handle_))
    {
        failed_ = true;
        set_error(error, "append flush", GetLastError());
        return false;
    }
    size_ += size;
    last_us_ = now_us() - t0;
    return failpoint("append:after-flush");
}

void AppendFile::close()
{
    if (handle_)
        CloseHandle(handle_);
    handle_ = nullptr;
}

std::uint32_t crc32_bytes(const void* data, std::size_t size) { return proto::crc32(data, size); }

std::string sha256_hex(const void* data, std::size_t size)
{
    std::uint8_t d[32] = {};
    BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, static_cast<PUCHAR>(const_cast<void*>(data)), ULONG(size), d,
        ULONG(sizeof(d)));
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (std::uint8_t b : d)
    {
        out += hex[b >> 4];
        out += hex[b & 15];
    }
    return out;
}

std::uint64_t utc_now_ms()
{
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    const std::uint64_t t = (std::uint64_t(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    return (t - 116444736000000000ULL) / 10000ULL;
}
} // namespace coop::store
