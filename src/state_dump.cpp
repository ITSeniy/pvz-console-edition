// T05 diagnostics. Xbox evidence and schema: docs/native-port/state-dumps.md.
// No Android/PC layouts and no raw object/padding/pointer hashes.
#include "guest_memory.h"
#include "guest_layout.h"
#include "game_content_lock.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <fmt/format.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

REXCVAR_DEFINE_STRING(pvz_state_dump, "", "Diagnostics", "JSONL state after each root WidgetManager logic update");
REXCVAR_DEFINE_BOOL(pvz_state_profile, false, "Diagnostics", "Measure root update/dump cost; usable without state dumping");
REXCVAR_DEFINE_UINT32(pvz_state_mutate_frame, 0, "Diagnostics", "Sensitivity test: add one sun at dump frame N, write pre-mutation .control.jsonl too");
REXCVAR_DEFINE_STRING(pvz_state_inputs_sha256, "", "Diagnostics", "SHA-256 of the runner input manifest (provenance, not compared state)");
REXCVAR_DEFINE_UINT32(pvz_state_frames, 0, "Diagnostics", "Stop recording after N complete updates; 0 records until exit");

#ifndef PVZ_STATE_DUMP_TEST
REX_EXTERN(__imp__sub_82492618);
#endif
namespace {
using pvz::guest::Read;
using pvz::guest::Write;
using Clock = std::chrono::steady_clock;
constexpr uint32_t kApp = 0x82D0BC88, kRng = 0x828924E8;
constexpr uint64_t kBasis = 14695981039346656037ull;
struct Digest {
  uint64_t value = kBasis;
  void Word(uint32_t word) {
    for (int shift : {24, 16, 8, 0}) { value ^= (word >> shift) & 255; value *= 1099511628211ull; }
  }
};
bool Mapped(uint8_t* base, uint32_t address, uint64_t size) {
  if (address < 0x10000 || size > (uint64_t(1) << 32) - address) return false;
#ifdef _WIN32
  auto at = reinterpret_cast<uintptr_t>(base) + address;
  const auto end = at + size;
  while (at < end) {
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(reinterpret_cast<void*>(at), &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const auto next = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    if (next <= at) return false;
    at = next;
  }
#else
  (void)base;
#endif
  return true;
}
struct Pool {
  const char* name;
  uint32_t offset, stride, type, x, y, health, state, dead;
};
// Board constructor 8212C540; allocators 8214D8F0/DA40/DB90/DCE0.
// ID is the final word of each item, high 16 bits nonzero => allocated.
constexpr uint32_t kNone = std::numeric_limits<uint32_t>::max();
constexpr std::array kPools{
    Pool{"plants", 0x114, 0x16C, 0x40, 0x24, 0x28, 0x5C, 0x58, 0x15D},
    Pool{"zombies", 0xF8, 0x184, 0x40, 0x48, 0x4C, 0xE4, 0x44, 0x108},
    Pool{"projectiles", 0x130, 0xB0, 0x78, 0x4C, 0x50, kNone, 0x74, 0x6C},
    Pool{"coins", 0x14C, 0xC8, 0x80, 0x40, 0x44, kNone, 0x84, 0x5C}};

std::string PoolJson(uint8_t* base, uint32_t board, const Pool& pool) {
  const auto array = board + pool.offset;
  const auto block = Read(base, array), used = Read(base, array + 4), capacity = Read(base, array + 8);
  const auto allocated = Read(base, array + 16), key = Read(base, array + 20);
  if (used > capacity || allocated > used || capacity > 65536 ||
      (used && !Mapped(base, block, uint64_t(used) * pool.stride)))
    return R"({"valid":false})";
  std::array<Digest, 7> hashes;
  uint32_t count = 0, found = 0;
  for (uint32_t i = 0; i < used; ++i) {
    const auto item = block + i * pool.stride;
    const auto id = Read(base, item + pool.stride - 4);
    if (!(id & 0xFFFF0000)) continue;
    ++found;
    if (base[item + pool.dead]) continue;
    ++count;
    const std::array words{id, Read(base, item + pool.type), Read(base, item + pool.x),
        Read(base, item + pool.y), pool.health == kNone ? 0u : Read(base, item + pool.health),
        Read(base, item + pool.state)};
    for (size_t j = 0; j < words.size(); ++j) {
      hashes[j].Word(words[j]); hashes.back().Word(words[j]);
    }
  }
  return fmt::format(R"({{"valid":{},"count":{},"allocated":{},"used":{},"next_key":{},"hash":"{:016x}","ids":"{:016x}","type":"{:016x}","x":"{:016x}","y":"{:016x}","health":"{:016x}","state":"{:016x}"}})",
      found == allocated, count, allocated, used, key, hashes[6].value, hashes[0].value,
      hashes[1].value, hashes[2].value, hashes[3].value, hashes[4].value, hashes[5].value);
}
uint32_t RootVtable(uint8_t* base, uint32_t manager) {
  auto widget = Read(base, manager + 0xA8);
  for (int depth = 0; widget && depth < 32; ++depth) {
    if (!Mapped(base, widget, 0x28)) return 0;
    const auto parent = Read(base, widget + 0x24);
    if (!parent || parent == manager) return Read(base, widget);
    widget = parent;
  }
  return 0;
}
std::string Snapshot(uint8_t* base, uint32_t app, uint32_t manager, uint64_t frame) {
  const auto candidate = Read(base, app + 0x900);
  const auto board = candidate && Mapped(base, candidate, 0x5884) && Read(base, candidate) == pvz::layout::kBoard ? candidate : 0;
  Digest rng;
  for (uint32_t off = 0; off < 0x9C4; off += 4) rng.Word(Read(base, kRng + off));
  auto result = fmt::format(R"({{"record":"state","frame":{},"mode":{},"scene":{},"root_vtable":{},"board":{},"paused":{},"sun":{},"sun_p2":{},"rng_index":{},"rng_hash":"{:016x}")",
      frame, Read(base, app + 0x9CC), Read(base, app + 0x9D0), RootVtable(base, manager),
      board != 0, board && base[board + 0x1D9] != 0,
      board ? int32_t(Read(base, board + 0x55F4)) : 0,
      board ? int32_t(Read(base, board + 0x55F8)) : 0, Read(base, kRng + 0x9C0), rng.value);
  for (const auto& pool : kPools)
    result += fmt::format(",\"{}\":{}", pool.name, board ? PoolJson(base, board, pool) : "null");
  result += "}\n";
  return result;
}
struct Recorder {
  std::ofstream stream, control;
  bool attempted = false, started = false, failed = false;
  uint64_t frame = 0;
  void Dump(uint8_t* base, uint32_t app, uint32_t manager) {
    if (failed) return;
    if (REXCVAR_GET(pvz_state_frames) && frame >= REXCVAR_GET(pvz_state_frames)) return;
    // Scene 1 and a real MainMenu root (or board) avoid comparing loading
    // thread progress. Frame zero is the first root update after this gate.
    if (!started) {
      if (!(Read(base, app + 0x9D0) == 1 && RootVtable(base, manager) == pvz::layout::kMainMenu) &&
          !Read(base, app + 0x900)) return;
      started = true;
    }
    if (!attempted) {
      attempted = true;
      const auto path = std::filesystem::path(REXCVAR_GET(pvz_state_dump));
      std::error_code error;
      if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), error);
      stream.open(path, std::ios::binary | std::ios::trunc);
      if (REXCVAR_GET(pvz_state_mutate_frame)) control.open(path.string() + ".control.jsonl", std::ios::binary | std::ios::trunc);
      if (!stream || (REXCVAR_GET(pvz_state_mutate_frame) && !control)) {
        failed = true; REXLOG_ERROR("PvZ state dump cannot open {}", path.string()); return;
      }
      const auto manifest = REXCVAR_GET(pvz_state_inputs_sha256);
      const bool valid_manifest = manifest.size() == 64 && std::all_of(manifest.begin(), manifest.end(),
          [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); });
      const auto header = fmt::format("{{\"record\":\"metadata\",\"schema\":1,\"phase\":\"after-root-update\",\"hash\":\"fnv1a64-be-words\",\"rng\":\"Sexy-MT19937-624-plus-index\",\"xex_sha256\":\"{}\",\"input_manifest_sha256\":\"{}\"}}\n",
          pvz::kGameContentHashes[0].second, valid_manifest ? manifest : "");
      stream << header; if (control.is_open()) control << header;
      REXLOG_INFO("PvZ state dump opened {}", path.string());
    }
    auto row = Snapshot(base, app, manager, frame);
    if (control.is_open()) { control << row; control.flush(); }
    if (frame == REXCVAR_GET(pvz_state_mutate_frame) && REXCVAR_GET(pvz_state_mutate_frame)) {
      const auto board = Read(base, app + 0x900);
      if (board && Read(base, board) == pvz::layout::kBoard) {
        Write(base, board + 0x55F4, Read(base, board + 0x55F4) + 1);
        row = Snapshot(base, app, manager, frame);
        REXLOG_INFO("PvZ state sensitivity applied +1 sun at frame {}", frame);
      } else REXLOG_ERROR("PvZ state sensitivity frame {} has no Board", frame);
    }
    stream << row; stream.flush(); // smoke may terminate the process at any time.
    if (!stream || (control.is_open() && !control)) {
      failed = true; REXLOG_ERROR("PvZ state dump write failed at frame {}", frame);
    }
    ++frame;
  }
} recorder;
struct Timing {
  std::vector<double> update, dump;
  uint64_t batches = 0;
  void Record(double update_us, double dump_us) {
    update.push_back(update_us); dump.push_back(dump_us);
    if (update.size() < 500) return;
    auto percentile = [](std::vector<double> values, double q) {
      std::sort(values.begin(), values.end()); return values[size_t((values.size()-1)*q)];
    };
    REXLOG_INFO("PvZ state timing batch {} n {} update_us_p50 {:.3f} update_us_p95 {:.3f} dump_us_p50 {:.3f} dump_us_p95 {:.3f}",
        ++batches, update.size(), percentile(update,.5), percentile(update,.95), percentile(dump,.5), percentile(dump,.95));
    update.clear(); dump.clear();
  }
} timing;
}

// WidgetContainer::UpdateAll, called recursively. Only the root WidgetManager
// (app+36C) is recorded. Its children, including Board::Update, have returned.
// Xbox: 824A6140 increments the update counter, then calls vtable slot 10;
// WidgetManager slot 10 = 82492618. Fractional UpdateAllF slot 12 is excluded.
#ifndef PVZ_STATE_DUMP_TEST
REX_HOOK_RAW(sub_82492618) {
  static const bool dump = !REXCVAR_GET(pvz_state_dump).empty();
  static const bool profile = REXCVAR_GET(pvz_state_profile);
  if (!dump && !profile) { __imp__sub_82492618(ctx, base); return; }
  const auto manager = ctx.r3.u32, app = Read(base, kApp);
  if (!app || manager != Read(base, app + 0x36C)) { __imp__sub_82492618(ctx, base); return; }
  const auto start = profile ? Clock::now() : Clock::time_point{};
  __imp__sub_82492618(ctx, base);
  const auto updated = profile ? Clock::now() : Clock::time_point{};
  if (dump) recorder.Dump(base, app, manager);
  if (profile) timing.Record(std::chrono::duration<double, std::micro>(updated-start).count(),
      std::chrono::duration<double, std::micro>(Clock::now()-updated).count());
}
#else
// Same reader as the game, synthetic guest memory; never loads a game image.
int main() {
  std::vector<uint8_t> memory(0x80000);
  auto* base = memory.data();
  constexpr uint32_t board = 0x10000, block = 0x20000;
  const auto& pool = kPools[0];
  auto require = [](bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
  };
  try {
    Write(base, board + pool.offset, block);
    Write(base, board + pool.offset + 4, 3);
    Write(base, board + pool.offset + 8, 4);
    Write(base, board + pool.offset + 16, 2);
    Write(base, board + pool.offset + 20, 3);
    for (uint32_t i = 0; i < 2; ++i) {
      const auto item = block + i * pool.stride;
      Write(base, item + pool.stride - 4, ((i + 1) << 16) | i);
      Write(base, item + pool.type, i);
      Write(base, item + pool.x, 0x3F800000 + i);
      Write(base, item + pool.y, 0x40000000);
      Write(base, item + pool.health, 300);
    }
    const auto original = PoolJson(base, board, pool);
    require(original.find("\"valid\":true,\"count\":2") != std::string::npos, "live slot count");
    Write(base, block + 2 * pool.stride + pool.health, 999);
    require(PoolJson(base, board, pool) == original, "free payload must be excluded");
    Write(base, block + pool.health, 301);
    require(PoolJson(base, board, pool) != original, "health change must affect digest");
    Write(base, block + pool.health, 300);
    Write(base, block + pool.x, 0x3F800001);
    require(PoolJson(base, board, pool) != original, "one float bit must affect digest");
    Write(base, block + pool.x, 0x3F800000);
    base[block + pool.dead] = 1;
    require(PoolJson(base, board, pool).find("\"count\":1") != std::string::npos, "dead allocated slot");
    base[block + pool.dead] = 0;
    std::copy_n(base + block, 3 * pool.stride, base + 0x30000);
    Write(base, board + pool.offset, 0x30000);
    require(PoolJson(base, board, pool) == original, "guest allocation address must not enter digest");
    Write(base, board + pool.offset + 4, 5);
    require(PoolJson(base, board, pool) == R"({"valid":false})", "used exceeds capacity");
    Write(base, board + pool.offset + 4, 3);
    Write(base, board + pool.offset + 16, 3);
    require(PoolJson(base, board, pool).find("\"valid\":false") != std::string::npos, "allocation count mismatch");
    Write(base, board + pool.offset, 0);
    require(PoolJson(base, board, pool) == R"({"valid":false})", "null live block");
#ifdef _WIN32
    Write(base, board + pool.offset, 0xFFFFF000);
    require(PoolJson(base, board, pool) == R"({"valid":false})", "unmapped live block");
#endif
    require(!Mapped(base, 0xFFFFF000, 0x2000), "32-bit address overflow");
    std::puts("State dump reader: live/dead/free slots, field sensitivity, relocation and invalid memory checks passed");
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what()); return 1;
  }
}
#endif
