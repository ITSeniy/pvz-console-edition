#include "replacement.h"
#include <atomic>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

REXCVAR_DEFINE_BOOL(pvz_native_compare_all, false, "Diagnostics", "Compare all registered native replacements")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(pvz_native_b001, false, "Native port", "Use proven B001 Widget replacements")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(pvz_native_b001_compare, false, "Diagnostics", "Compare safe B001 replacements; observe snapshot-only entries")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace pvz::native {
namespace {
auto& Registry() { static std::vector<Registration*> entries; return entries; }
}
Registration::Registration(const char* value, bool (*compare)()) : name(value), comparing(compare) {
  Registry().push_back(this);
}
void ReplayCompleted() {
  for (auto* entry : Registry()) if (entry->comparing()) Summarize(*entry, "replay_end");
}
void Observe(Registration& entry, uint32_t lr) {
  std::lock_guard lock(entry.stats.mutex);
  entry.stats.Record(lr, false);
  if (entry.stats.checkpoint)
    entry.stats.checkpoint(entry.stats.checkpoint_data, entry.stats.calls, entry.stats.errors);
}
// Two committed slots, little-endian u64 generation/calls/errors, after the
// 8-byte magic. The previous slot remains valid if termination interrupts a
// publish. OS-backed shared pages survive smoke's TerminateProcess; no log
// record or filesystem syscall is needed on each comparison.
bool Prepare(Registration& entry) {
  auto path = REXCVAR_GET(log_file);
  if (path.empty()) return true; // Interactive runs still have periodic logs.
  path += std::string(".") + entry.name + ".native-counts.bin";
  void* bytes = nullptr;
#ifdef _WIN32
  const auto file = CreateFileW(std::filesystem::path(path).c_str(), GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot create native counter checkpoint");
  const auto mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0, 56, nullptr);
  if (mapping) bytes = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 56);
  if (mapping) CloseHandle(mapping);
  CloseHandle(file);
#else
  const auto file = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (file >= 0 && ftruncate(file, 56) == 0) {
    bytes = mmap(nullptr, 56, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    if (bytes == MAP_FAILED) bytes = nullptr;
  }
  if (file >= 0) close(file);
#endif
  if (!bytes) throw std::runtime_error("cannot map native counter checkpoint");
  std::memset(bytes, 0, 56);
  std::memcpy(bytes, "PVZNR001", 8);
  entry.stats.checkpoint_data = bytes;
  entry.stats.checkpoint = [](void* data, uint64_t calls, uint64_t errors) {
    auto* slot = reinterpret_cast<uint64_t*>(static_cast<uint8_t*>(data) + 8) + (calls % 2) * 3;
    static_assert(std::atomic_ref<uint64_t>::is_always_lock_free);
    std::atomic_ref(slot[0]).exchange(0, std::memory_order_acq_rel);
    std::atomic_ref(slot[1]).store(calls, std::memory_order_relaxed);
    std::atomic_ref(slot[2]).store(errors, std::memory_order_relaxed);
    std::atomic_ref(slot[0]).store(calls, std::memory_order_release);
  };
  return true;
}
void Summarize(Registration& entry, const char* reason) {
  std::lock_guard lock(entry.stats.mutex);
  if (std::string_view(reason) == "replay_end") {
    std::string callers;
    for (size_t i = 0; i < entry.stats.caller_count; ++i) {
      const auto& c = entry.stats.callers[i];
      if (i) callers += ',';
      callers += fmt::format("{:08X}:{}:{}", c.lr, c.calls, c.errors);
    }
    REXLOG_INFO("PvZ native {} summary reason {} comparisons {} errors {} callers [{}] other {}:{}",
        entry.name, reason, entry.stats.calls, entry.stats.errors, callers,
        entry.stats.other_calls, entry.stats.other_errors);
    return;
  }
  REXLOG_INFO("PvZ native {} summary reason {} comparisons {} errors {}",
      entry.name, reason, entry.stats.calls, entry.stats.errors);
}
void Mismatch(Registration& entry, const PPCContext& input, const std::string& detail) {
  REXLOG_ERROR("PvZ native {} mismatch call {} r1 {:016X} r3 {:016X} r4 {:016X} r5 {:016X} r6 {:016X} r7 {:016X} r8 {:016X} r9 {:016X} r10 {:016X} f1 {:016X} f2 {:016X} f3 {:016X} f4 {:016X} f5 {:016X} f6 {:016X} f7 {:016X} f8 {:016X} f9 {:016X} f10 {:016X} f11 {:016X} f12 {:016X} f13 {:016X}{}",
      entry.name, entry.stats.calls, input.r1.u64, input.r3.u64, input.r4.u64, input.r5.u64, input.r6.u64,
      input.r7.u64, input.r8.u64, input.r9.u64, input.r10.u64, input.f1.u64, input.f2.u64,
      input.f3.u64, input.f4.u64, input.f5.u64, input.f6.u64, input.f7.u64, input.f8.u64,
      input.f9.u64, input.f10.u64, input.f11.u64, input.f12.u64, input.f13.u64, detail);
}
} // namespace pvz::native
