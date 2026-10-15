// Diagnostics: while the game thread goes without presenting, sample its host
// stack and log the guest functions it spent the stall in.
#include "hitch_profiler.h"
#include <rex/cvar.h>
#include <rex/logging.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <DbgHelp.h>
#include <TlHelp32.h>
#pragma comment(lib, "dbghelp.lib")
#endif

REXCVAR_DEFINE_BOOL(pvz_hitch_profile, false, "Diagnostics", "Sample the game thread during frame stalls");

namespace {
using Clock = std::chrono::steady_clock;
std::atomic<int64_t> last_present{0};
std::atomic<uint32_t> present_thread{0};
std::jthread sampler;

int64_t Now() { return Clock::now().time_since_epoch().count(); }

#ifdef _WIN32
constexpr size_t kDepth = 24;
using Stack = std::vector<uint64_t>;

// Unwinds without allocating: the suspended thread may hold the heap lock.
size_t Walk(CONTEXT context, uint64_t* frames) {
  size_t count = 0;
  while (count < kDepth && context.Rip) {
    frames[count++] = context.Rip;
    DWORD64 image = 0;
    auto* function = RtlLookupFunctionEntry(context.Rip, &image, nullptr);
    if (!function) break; // Leaf without unwind data; stop rather than guess.
    void* handler = nullptr; DWORD64 frame = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, context.Rip, function, &context, &handler, &frame, nullptr);
  }
  return count;
}

std::string Symbol(uint64_t address, std::map<uint64_t, std::string>& cache) {
  if (auto found = cache.find(address); found != cache.end()) return found->second;
  alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256]{};
  auto* info = reinterpret_cast<SYMBOL_INFO*>(buffer);
  info->SizeOfStruct = sizeof(SYMBOL_INFO); info->MaxNameLen = 255;
  DWORD64 displacement = 0;
  std::string name = SymFromAddr(GetCurrentProcess(), address, &displacement, info) ?
      std::string(info->Name, info->NameLen) : "?";
  return cache[address] = name;
}

// The innermost frames, plus the innermost recompiled guest function, which
// is the useful anchor when the thread is inside the CRT or the kernel.
std::string Describe(const Stack& stack, std::map<uint64_t, std::string>& cache) {
  std::string text, guest;
  for (size_t i = 0; i < stack.size(); ++i) {
    const auto name = Symbol(stack[i], cache);
    if (i < 3) text += (i ? " < " : "") + name;
    if (guest.empty() && name.starts_with("sub_")) {
      guest = name;
      for (size_t j = i + 1, n = 0; j < stack.size() && n < 3; ++j) {
        const auto caller = Symbol(stack[j], cache);
        if (caller.starts_with("sub_") && caller != guest) { guest += " < " + caller; ++n; }
      }
    }
  }
  return text + (guest.empty() ? "" : " | guest " + guest);
}

struct Thread { uint32_t id; HANDLE handle; std::string name; };

std::vector<Thread> OpenThreads(uint32_t game) {
  std::vector<Thread> threads;
  const auto self = GetCurrentThreadId();
  const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return threads;
  THREADENTRY32 entry{}; entry.dwSize = sizeof(entry);
  for (bool ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry)) {
    if (entry.th32OwnerProcessID != GetCurrentProcessId() || entry.th32ThreadID == self) continue;
    const auto handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_LIMITED_INFORMATION,
        FALSE, entry.th32ThreadID);
    if (!handle) continue;
    std::string name = entry.th32ThreadID == game ? "game" : std::to_string(entry.th32ThreadID);
    PWSTR description = nullptr;
    if (entry.th32ThreadID != game && SUCCEEDED(GetThreadDescription(handle, &description)) && description && *description) {
      name.clear();
      for (auto* c = description; *c; ++c) name += char(*c < 128 ? *c : '?');
    }
    if (description) LocalFree(description);
    threads.push_back({entry.th32ThreadID, handle, name});
  }
  CloseHandle(snapshot);
  return threads;
}

bool Idle(const std::string& text) {
  return text.starts_with("NtWait") || text.starts_with("NtDelay") || text.starts_with("NtRemoveIoCompletion") ||
      text.starts_with("ZwWait") || text.starts_with("NtUserMsgWait") || text.starts_with("NtYield") ||
      text.starts_with("NtAlertThreadByThreadId") || text.starts_with("ZwRemoveIoCompletion");
}

void Sample(std::stop_token stop) {
  SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
  SymInitialize(GetCurrentProcess(), nullptr, TRUE);
  std::vector<Thread> threads;
  std::vector<std::pair<size_t, Stack>> samples; // thread index, stack
  int64_t stall_start = 0;
  const auto close = [&] { for (auto& t : threads) CloseHandle(t.handle); threads.clear(); };
  while (!stop.stop_requested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const auto present = last_present.load();
    const double idle = std::chrono::duration<double, std::milli>(Clock::duration(Now() - present)).count();
    if (idle < 50) {
      if (stall_start && !samples.empty()) {
        const double stall = std::chrono::duration<double, std::milli>(Clock::duration(present - stall_start)).count();
        std::map<uint64_t, std::string> cache;
        std::map<std::string, int> counts;
        size_t game_samples = 0;
        for (const auto& [index, stack] : samples) {
          const auto text = Describe(stack, cache);
          const bool game = threads[index].id == present_thread.load();
          game_samples += game;
          if (game || !Idle(text)) ++counts[threads[index].name + ": " + text];
        }
        std::vector<std::pair<int, std::string>> ranked;
        for (auto& [text, count] : counts) ranked.emplace_back(count, text);
        std::sort(ranked.rbegin(), ranked.rend());
        REXLOG_INFO("PvZ hitch profile: {:.0f} ms, {} game thread samples", stall, game_samples);
        for (size_t i = 0; i < ranked.size() && i < 14; ++i)
          REXLOG_INFO("PvZ hitch profile {:4}x {}", ranked[i].first, ranked[i].second);
      }
      samples.clear(); stall_start = 0; close();
      continue;
    }
    if (!stall_start) { stall_start = present; threads = OpenThreads(present_thread.load()); }
    // A hung guest must not grow a diagnostics buffer without bound.
    if (samples.size() >= 4096) continue;
    for (size_t i = 0; i < threads.size() && samples.size() < 4096; ++i) {
      if (SuspendThread(threads[i].handle) == DWORD(-1)) continue;
      CONTEXT context{}; context.ContextFlags = CONTEXT_FULL;
      uint64_t frames[kDepth]; size_t count = 0;
      if (GetThreadContext(threads[i].handle, &context)) count = Walk(context, frames);
      ResumeThread(threads[i].handle);
      if (count) samples.emplace_back(i, Stack(frames, frames + count));
    }
  }
  close();
  SymCleanup(GetCurrentProcess());
}
#endif
}

void pvz::NotePresent() {
  if (!REXCVAR_GET(pvz_hitch_profile)) return;
  last_present.store(Now(), std::memory_order_relaxed);
#ifdef _WIN32
  if (!present_thread.load(std::memory_order_relaxed) && REXCVAR_GET(pvz_hitch_profile)) {
    present_thread.store(GetCurrentThreadId());
    sampler = std::jthread(Sample);
  }
#endif
}
void pvz::StopHitchProfiler() {
  sampler.request_stop();
  if (sampler.joinable()) sampler.join();
}
