// Test runner: transport-tests [--filter <substring>[,<substring>...]] [--list] [--json <file>]
#include "check.h"

#include <windows.h>
#include <timeapi.h>

#include <chrono>
#include <cstdarg>
#include <cstring>
#include <exception>
#include <mutex>

namespace coop::test
{
namespace
{
struct Case
{
    std::string name;
    std::function<void()> fn;
};

std::vector<Case>& cases()
{
    static std::vector<Case> c;
    return c;
}

struct Result
{
    std::vector<std::string> failures;
    std::vector<std::pair<std::string, double>> metrics;
    std::vector<std::string> notes;
};

std::mutex g_mutex; // CHECK may run on test-owned threads
Result* g_current = nullptr;

std::string Escape(const std::string& s)
{
    std::string out;
    for (char c : s)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += c;
        }
        else if (c == '\n')
            out += "\\n";
        else if (static_cast<unsigned char>(c) < 0x20)
            out += ' ';
        else
            out += c;
    }
    return out;
}
} // namespace

Registrar::Registrar(std::string name, std::function<void()> fn) { cases().push_back({std::move(name), std::move(fn)}); }

void fail(const char* file, int line, const std::string& what)
{
    const char* base = std::strrchr(file, '\\');
    base = base ? base + 1 : file;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_current)
        g_current->failures.push_back(std::string(base) + ":" + std::to_string(line) + " " + what);
    std::printf("    FAIL %s:%d %s\n", base, line, what.c_str());
    std::fflush(stdout);
}

void metric(const std::string& name, double value)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_current)
        g_current->metrics.push_back({name, value});
}

void note(const std::string& text)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_current)
        g_current->notes.push_back(text);
}

bool failed_now()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_current && !g_current->failures.empty();
}

std::string fmt(const char* format, ...)
{
    char buf[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return buf;
}
} // namespace coop::test

int main(int argc, char** argv)
{
    using namespace coop::test;
    std::vector<std::string> filters;
    const char* jsonPath = nullptr;
    bool list = false;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--filter") && i + 1 < argc)
        {
            std::string f = argv[++i];
            for (std::size_t at = 0; at <= f.size();)
            {
                const std::size_t comma = f.find(',', at);
                const std::string part = f.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
                if (!part.empty())
                    filters.push_back(part);
                if (comma == std::string::npos)
                    break;
                at = comma + 1;
            }
        }
        else if (!std::strcmp(argv[i], "--json") && i + 1 < argc)
            jsonPath = argv[++i];
        else if (!std::strcmp(argv[i], "--list"))
            list = true;
        else
        {
            std::fprintf(stderr, "usage: transport-tests [--filter a,b] [--list] [--json file]\n");
            return 2;
        }
    }

    // A windowless process may be power-throttled on Windows 11, which ignores
    // timeBeginPeriod and makes Sleep(1) last ~15 ms (S04-T01 finding 5).
    PROCESS_POWER_THROTTLING_STATE pt = {};
    pt.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    pt.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    pt.StateMask = 0;
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &pt, sizeof(pt));
    timeBeginPeriod(1);

    std::string json = "{\"tests\":[";
    int run = 0, passed = 0;
    for (const Case& c : cases())
    {
        bool selected = filters.empty();
        for (const auto& f : filters)
            if (c.name.find(f) != std::string::npos)
                selected = true;
        if (!selected)
            continue;
        if (list)
        {
            std::printf("%s\n", c.name.c_str());
            continue;
        }
        Result r;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_current = &r;
        }
        std::printf("%-55s ", c.name.c_str());
        std::fflush(stdout);
        const auto t0 = std::chrono::steady_clock::now();
        try
        {
            c.fn();
        }
        catch (const Abort&)
        {
        }
        catch (const std::exception& e)
        {
            fail(__FILE__, __LINE__, std::string("exception: ") + e.what());
        }
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_current = nullptr;
        }
        const bool ok = r.failures.empty();
        std::printf("%s %.0f ms\n", ok ? "PASS" : "FAIL", ms);
        for (const auto& m : r.metrics)
            std::printf("      %s = %.3f\n", m.first.c_str(), m.second);
        std::fflush(stdout);
        ++run;
        passed += ok ? 1 : 0;
        if (run > 1)
            json += ",";
        json += "{\"name\":\"" + Escape(c.name) + "\",\"pass\":" + (ok ? "true" : "false") +
                ",\"elapsed_ms\":" + fmt("%.1f", ms) + ",\"failures\":[";
        for (std::size_t i = 0; i < r.failures.size(); ++i)
            json += (i ? ",\"" : "\"") + Escape(r.failures[i]) + "\"";
        json += "],\"metrics\":{";
        for (std::size_t i = 0; i < r.metrics.size(); ++i)
            json += (i ? ",\"" : "\"") + Escape(r.metrics[i].first) + "\":" + fmt("%.4f", r.metrics[i].second);
        json += "},\"notes\":[";
        for (std::size_t i = 0; i < r.notes.size(); ++i)
            json += (i ? ",\"" : "\"") + Escape(r.notes[i]) + "\"";
        json += "]}";
    }
    if (list)
        return 0;
    json += "],\"run\":" + std::to_string(run) + ",\"passed\":" + std::to_string(passed) + "}\n";
    if (jsonPath)
    {
        if (FILE* f = std::fopen(jsonPath, "wb"))
        {
            std::fwrite(json.data(), 1, json.size(), f);
            std::fclose(f);
        }
    }
    std::printf("%d/%d passed\n", passed, run);
    return passed == run && run > 0 ? 0 : 1;
}
