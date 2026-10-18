#define PVZ_NATIVE_REPLACEMENT_TEST
#include "native/replacement.h"
#include <array>
#include <iostream>
#include <limits>
union Register { uint64_t u64; int64_t s64; double f64; };
struct PPCContext { Register r3{}, r4{}, f1{}, r9{}; uint64_t lr = 0; };
using namespace pvz::native;
using Out = Output<PPCContext, Register>;
void Require(bool ok) { if (!ok) throw std::runtime_error("native replacement assertion"); }
int main() {
  try {
    Stats stats;
    struct Checkpoint { uint64_t calls = 0, errors = 0, publishes = 0; } checkpoint;
    stats.checkpoint_data = &checkpoint;
    stats.checkpoint = [](void* data, uint64_t calls, uint64_t errors) {
      auto& out = *static_cast<Checkpoint*>(data);
      out.calls = calls; out.errors = errors; ++out.publishes;
    };
    PPCContext ctx{};
    std::array<uint8_t, 64> memory{};
    const std::array outputs{Out{"r3", &PPCContext::r3, 32, Kind::Signed},
                            Out{"f1", &PPCContext::f1, 64, Kind::FloatBits}};
    int resolves = 0, native_calls = 0, original_calls = 0, errors = 0, summaries = 0;
    auto ranges = [&](const PPCContext& input) {
      ++resolves; Require(input.r4.u64 == 8); return std::array{Range{8, 8}, Range{32, 4}};
    };
    auto native = [&](PPCContext& c, uint8_t* b) {
      ++native_calls; Require(b[8] == 0 && b[32] == 0);
      b[8] = 42; b[32] = 71; c.r3.u64 = 0xFFFFffff; c.f1.u64 = 0x8000000000000000ull;
      c.r9.u64 = 33;
    };
    auto original = [&](PPCContext& c, uint8_t* b) {
      ++original_calls; Require(b[8] == 0 && b[32] == 0); // restored before original
      b[8] = 42; b[32] = 71; c.r3.u64 = ~uint64_t{}; c.f1.u64 = 0x8000000000000000ull;
      c.r9.u64 = 99;
    };
    auto log = [&](const PPCContext& input, const std::string& detail) {
      Require(input.r4.u64 == 8 && !detail.empty()); ++errors;
    };
    auto summary = [&](const char*, uint64_t n, uint64_t e) {
      Require(n == stats.calls && e == stats.errors); ++summaries;
    };
    auto run = [&](Mode mode, Safety safety = Safety::DeclaredOutputsOnly) {
      ctx = {}; ctx.r4.u64 = 8; memory.fill(0);
      Invoke(ctx, memory.data(), mode, safety, stats, outputs, ranges, native, original, log, summary);
    };
    run({false, true, false});
    Require(stats.calls == 1 && stats.errors == 0 && ctx.r9.u64 == 99 && memory[32] == 71);
    run({true, true, false}); Require(ctx.r3.s64 == -1 && ctx.r9.u64 == 99);
    run({false, true, true}); Require(stats.errors == 1 && memory[8] == 42);
    run({true, true, true}); Require(stats.errors == 2 && memory[8] == 42 && ctx.r3.s64 == -2);
    const auto comparisons = stats.calls;
    run({true, false, true}); Require(memory[8] == 42 && stats.calls == comparisons);
    // Disabled path: neither range resolver, native, logger nor memory touched.
    const int old_resolves = resolves, old_native = native_calls;
    Invoke(ctx, nullptr, Mode{}, Safety::SnapshotsOnly, stats, outputs,
      [&](const PPCContext&) -> std::array<Range, 0> { throw std::runtime_error("resolved off path"); },
      native, [&](PPCContext&, uint8_t* b) { Require(b == nullptr); ++original_calls; }, log, summary);
    Require(resolves == old_resolves && native_calls == old_native);
    bool refused = false;
    try { run({false, true, false}, Safety::SnapshotsOnly); } catch (const std::logic_error&) { refused = true; }
    Require(refused);
    // Pure register compare: catch exact floating bits (+0 != -0), width,
    // unsigned extension, selected native, and preserve original scratch.
    Stats reg_stats;
    const std::array regs{Out{"r3", &PPCContext::r3, 8, Kind::Unsigned},
                          Out{"f1", &PPCContext::f1, 64, Kind::FloatBits}};
    Invoke(ctx, nullptr, Mode{true, true, true}, Safety::DeclaredOutputsOnly, reg_stats, regs,
      [](const PPCContext&) { return std::array<Range, 0>{}; },
      [](PPCContext& c, uint8_t*) { c.r3.u64 = 255; c.f1.u64 = 0x8000000000000000ull; },
      [](PPCContext& c, uint8_t*) { c.r3.u64 = 255; c.f1.u64 = 0; c.r9.u64 = 123; },
      [&](const PPCContext&, const std::string& d) { Require(d.find("f1") != std::string::npos); },
      [](const char*, uint64_t, uint64_t) {});
    Require(reg_stats.errors == 1 && ctx.r3.u64 == 254 && ctx.r9.u64 == 123);
    const std::array f32{Out{"f1", &PPCContext::f1, 32, Kind::FloatBits}};
    Invoke(ctx, nullptr, Mode{true, true, true}, Safety::DeclaredOutputsOnly, reg_stats, f32,
      [](const PPCContext&) { return std::array<Range, 0>{}; },
      [](PPCContext& c, uint8_t*) { c.f1.f64 = 1.25; },
      [](PPCContext& c, uint8_t*) { c.f1.f64 = 1.25; },
      [](const PPCContext&, const std::string& d) { Require(d.find("f1") != std::string::npos); },
      [](const char*, uint64_t, uint64_t) {});
    Require(reg_stats.errors == 2 && ctx.f1.f64 != 1.25);
    // Input/output alias: original reads entry bytes, not native's changed bytes.
    Stats alias_stats;
    const std::array<Out, 0> no_regs{};
    auto mutate = [](PPCContext&, uint8_t* b) { b[8] += 7; };
    memory.fill(3);
    Invoke(ctx, memory.data(), Mode{false, true}, Safety::DeclaredOutputsOnly, alias_stats, no_regs,
      [](const PPCContext&) { return std::array{Range{8, 1}}; }, mutate, mutate,
      [](const PPCContext&, const std::string&) { throw std::runtime_error("alias mismatch"); },
      [](const char*, uint64_t, uint64_t) {});
    Require(memory[8] == 10 && alias_stats.errors == 0);
    memory.fill(3);
    Invoke(ctx, memory.data(), Mode{true, true, true}, Safety::DeclaredOutputsOnly, alias_stats, no_regs,
      [](const PPCContext&) { return std::array{Range{8, 1}}; }, mutate, mutate,
      [](const PPCContext&, const std::string&) {}, [](const char*, uint64_t, uint64_t) {});
    Require(alias_stats.errors == 1 && memory[8] == 11);
    // Native failure cannot leak changed output bytes.
    try {
      Invoke(ctx, memory.data(), Mode{false, true}, Safety::DeclaredOutputsOnly, alias_stats, no_regs,
        [](const PPCContext&) { return std::array{Range{8, 1}}; },
        [](PPCContext&, uint8_t* b) { b[8] = 55; throw std::runtime_error("native failed"); }, mutate,
        log, summary);
    } catch (const std::runtime_error&) {}
    Require(memory[8] == 11);
    // A difference at the LAST byte of the SECOND range must be detected;
    // restoration also covers bytes that are not at a range's first address.
    Stats tail_stats;
    memory.fill(3);
    Invoke(ctx, memory.data(), Mode{false, true}, Safety::DeclaredOutputsOnly, tail_stats, no_regs,
      [](const PPCContext&) { return std::array{Range{8, 8}, Range{32, 4}}; },
      [](PPCContext&, uint8_t* b) { b[15] = 20; b[35] = 30; },
      [](PPCContext&, uint8_t* b) {
        Require(b[15] == 3 && b[35] == 3); b[15] = 20; b[35] = 31;
      },
      [](const PPCContext&, const std::string& d) { Require(d.find("byte=0x23") != std::string::npos); },
      [](const char*, uint64_t, uint64_t) {});
    Require(tail_stats.errors == 1 && memory[15] == 20 && memory[35] == 31);
    // Periodic accounting has a hard 1000-call bound.
    while (stats.calls < 1000) run({false, true});
    Require(summaries == 1 && errors == 2);
    Require(checkpoint.calls == 1000 && checkpoint.errors == 2 && checkpoint.publishes == 1000);
    Require(stats.caller_count == 1 && stats.callers[0].lr == 0 &&
        stats.callers[0].calls == 1000 && stats.callers[0].errors == 2);
    // Keep the first sixteen distinct entry LRs; later addresses are counted
    // in an explicit overflow bucket, never silently omitted from the total.
    Stats caller_stats;
    for (uint32_t i = 0; i < 18; ++i) {
      ctx.lr = 0x82120000 + i * 4;
      Invoke(ctx, nullptr, Mode{false, true, i == 1 || i == 17},
        Safety::DeclaredOutputsOnly, caller_stats, regs,
        [](const PPCContext&) { return std::array<Range, 0>{}; },
        [](PPCContext& c, uint8_t*) { c.r3.u64 = 1; c.f1.u64 = 0; c.lr = 0; },
        [](PPCContext& c, uint8_t*) { c.r3.u64 = 1; c.f1.u64 = 0; c.lr = 0; },
        [](const PPCContext&, const std::string&) {}, [](const char*, uint64_t, uint64_t) {});
    }
    Require(caller_stats.calls == 18 && caller_stats.errors == 2 && caller_stats.caller_count == 16);
    Require(caller_stats.callers[0].lr == 0x82120000 && caller_stats.callers[15].lr == 0x8212003C);
    Require(caller_stats.callers[1].errors == 1 && caller_stats.other_calls == 2 && caller_stats.other_errors == 1);
    std::cout << "native replacement contracts passed\n";
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
