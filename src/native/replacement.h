// Native-port comparison contract. The engine is independent of the SDK so a
// fake PPCContext and ordinary byte buffer can exercise exactly the same path.
#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace pvz::native {
enum class Safety { DeclaredOutputsOnly, SnapshotsOnly };
enum class Kind { Unsigned, Signed, FloatBits };
struct Range { uint32_t address; size_t size; };
struct Mode { bool enabled = false, compare = false, inject = false; };
// Shared by live comparison and isolated synthetic controls. In particular,
// FPR32 must perturb the declared float bits, not a truncated double mantissa.
template<class Context, class Outputs, class Ranges>
void InjectError(Context& candidate,uint8_t* base,const Outputs& outputs,const Ranges& ranges) {
  if(!outputs.empty()) {
    const auto& out=outputs.front();
    if(out.kind==Kind::FloatBits && out.bits==32)
      (candidate.*out.member).f64=double(std::bit_cast<float>(uint32_t(out.Read(candidate))^1));
    else (candidate.*out.member).u64^=1;
  } else if(!ranges.empty()) base[ranges.front().address]^=1;
  else throw std::invalid_argument("inject_error needs declared outputs");
}
template<class Context> struct SyntheticInputs {
  const char* description;
  size_t boundaries, random;
  uint64_t seed;
  void (*generate)(size_t, Context&, uint8_t*, uint64_t&);
};
struct SyntheticResult { uint64_t cases = 0, errors = 0, negative_errors = 0; };

// Independent guest buffers: neither implementation can contaminate the next
// case or the other implementation. Only the declared ABI outputs are compared.
template<class Context, class Outputs, class Resolve, class Native, class Original>
SyntheticResult VerifySynthetic(const SyntheticInputs<Context>& inputs, size_t bytes,
    const Outputs& outputs, Resolve resolve, Native native, Original original) {
  SyntheticResult result;
  // REX_FUNC_PROLOGUE requires a 32-byte aligned guest base.
  auto buffer = [bytes] {
    return std::unique_ptr<uint8_t, void(*)(uint8_t*)>(
        static_cast<uint8_t*>(::operator new(bytes, std::align_val_t{32})),
        [](uint8_t* p) {::operator delete(p, std::align_val_t{32});});
  };
  auto entry=buffer(), candidate=buffer(), reference=buffer();
  uint64_t seed = inputs.seed;
  for (size_t i = 0; i < inputs.boundaries + inputs.random; ++i) {
    std::memset(entry.get(), 0xCD, bytes);
    Context ctx{};
    inputs.generate(i, ctx, entry.get(), seed);
    const auto ranges = resolve(ctx);
    for (const auto& range : ranges)
      if (!range.size || uint64_t(range.address) + range.size > bytes)
        throw std::invalid_argument("synthetic output outside isolated memory");
    std::memcpy(candidate.get(), entry.get(), bytes); std::memcpy(reference.get(), entry.get(), bytes);
    Context a = ctx, b = ctx;
    native(a, candidate.get()); original(b, reference.get());
    auto equal = [&] {
      for (const auto& out : outputs) if (out.Read(a) != out.Read(b)) return false;
      for (const auto& range : ranges)
        if (std::memcmp(candidate.get() + range.address, reference.get() + range.address, range.size)) return false;
      return true;
    };
    ++result.cases; result.errors += !equal();
    // Same one-bit corruption as Invoke's inject_error, including pure memory
    // outputs. Every single input is also a negative control.
    const Mode negative_control{false,true,true};
    if(negative_control.inject) InjectError(a,candidate.get(),outputs,ranges);
    result.negative_errors += !equal();
  }
  return result;
}
struct Stats {
  std::recursive_mutex mutex;
  uint64_t calls = 0, errors = 0;
  struct Caller { uint32_t lr = 0; uint64_t calls = 0, errors = 0; };
  std::array<Caller, 16> callers{};
  size_t caller_count = 0;
  uint64_t other_calls = 0, other_errors = 0;
  void Record(uint32_t lr, bool mismatch) {
    ++calls;
    errors += mismatch;
    for (size_t i = 0; i < caller_count; ++i) {
      if (callers[i].lr == lr) {
        ++callers[i].calls;
        callers[i].errors += mismatch;
        return;
      }
    }
    if (caller_count < callers.size()) callers[caller_count++] = {lr, 1, uint64_t(mismatch)};
    else { ++other_calls; other_errors += mismatch; }
  }
  void* checkpoint_data = nullptr;
  void (*checkpoint)(void*, uint64_t, uint64_t) = nullptr;
};
template<class Context, class Register> struct Output {
  const char* name;
  Register Context::*member;
  unsigned bits;
  Kind kind;
  uint64_t Mask() const {
    if (bits != 8 && bits != 16 && bits != 32 && bits != 64)
      throw std::invalid_argument("register width must be 8/16/32/64");
    if (kind == Kind::FloatBits && bits != 32 && bits != 64)
      throw std::invalid_argument("FPR width must be 32/64");
    return bits == 64 ? ~uint64_t{} : (uint64_t{1} << bits) - 1;
  }
  uint64_t Read(const Context& ctx) const {
    (void)Mask();
    if (kind == Kind::FloatBits && bits == 32)
      return std::bit_cast<uint32_t>(static_cast<float>((ctx.*member).f64));
    return (ctx.*member).u64 & Mask();
  }
  void Select(Context& ctx, const Context& from) const {
    auto value = Read(from);
    if (kind == Kind::Signed && bits < 64 && (value & (uint64_t{1} << (bits - 1))))
      value |= ~Mask();
    if (kind == Kind::FloatBits && bits == 32) {
      (ctx.*member).f64 = static_cast<double>(std::bit_cast<float>(uint32_t(value)));
      return;
    }
    (ctx.*member).u64 = value;
  }
};

// Ranges must be valid writable guest bytes, disjoint, and resolved solely
// from entry arguments. Input/output aliasing is supported. No other writes,
// child effects, RNG, IO, allocations or asynchronous observers are permitted.
// log(error, detail) and summary(reason, calls, errors) run under stats.mutex.
template<class Context, class Outputs, class Resolve, class Native, class Original,
         class Log, class Summary>
void Invoke(Context& ctx, uint8_t* base, Mode mode, Safety safety, Stats& stats,
            const Outputs& outputs, Resolve resolve, Native native, Original original,
            Log log, Summary summary) {
  if (!mode.enabled && !mode.compare) { original(ctx, base); return; }
  if (!mode.compare) { native(ctx, base); return; }
  if (safety == Safety::SnapshotsOnly)
    throw std::logic_error("SnapshotsOnly replacement cannot run compare");
  std::lock_guard lock(stats.mutex);
  const Context input = ctx;
  const auto ranges = resolve(input);
  struct Saved { Range range; std::vector<uint8_t> before, after; };
  std::vector<Saved> saved;
  for (auto range : ranges) {
    if (!range.size || range.size > (uint64_t{1} << 32) - range.address)
      throw std::invalid_argument("invalid guest output range");
    for (const auto& other : saved)
      if (uint64_t(range.address) < uint64_t(other.range.address) + other.range.size &&
          uint64_t(other.range.address) < uint64_t(range.address) + range.size)
        throw std::invalid_argument("overlapping guest output ranges");
    saved.push_back({range, std::vector<uint8_t>(base + range.address,
        base + range.address + range.size), std::vector<uint8_t>(range.size)});
  }
  for (const auto& output : outputs) (void)output.Mask();
  if (outputs.empty() && saved.empty()) throw std::invalid_argument("compare needs outputs");
  auto restore = [&] {
    for (const auto& s : saved) std::memcpy(base + s.range.address, s.before.data(), s.range.size);
  };
  Context candidate = input;
  try {
    native(candidate, base);
    // Injection belongs to the harness, and cannot affect native-only mode.
    if (mode.inject) InjectError(candidate,base,outputs,ranges);
    for (auto& s : saved) std::memcpy(s.after.data(), base + s.range.address, s.range.size);
  } catch (...) { restore(); throw; }
  restore();
  original(ctx, base);
  std::ostringstream differences;
  bool mismatch = false;
  const bool detail = stats.errors < 20;
  for (const auto& output : outputs) {
    if (output.Read(candidate) != output.Read(ctx)) {
      mismatch = true;
      if (detail) differences << ' ' << output.name << ':' << output.bits << ':' << int(output.kind)
                  << " native=0x" << std::hex << output.Read(candidate)
                  << " original=0x" << output.Read(ctx) << std::dec;
    }
  }
  for (const auto& s : saved) {
    for (size_t i = 0; i < s.range.size; ++i) {
      if (s.after[i] != base[s.range.address + i]) {
        mismatch = true;
        if (detail) differences << " byte=0x" << std::hex << uint64_t(s.range.address) + i
                    << " native=" << unsigned(s.after[i])
                    << " original=" << unsigned(base[s.range.address + i]) << std::dec;
      }
    }
  }
  stats.Record(uint32_t(input.lr), mismatch);
  if (stats.checkpoint) stats.checkpoint(stats.checkpoint_data, stats.calls, stats.errors);
  // Count every mismatch, but bound expensive details and log IO per process.
  if (mismatch && stats.errors <= 20) log(input, differences.str());
  if (mode.enabled) {
    for (const auto& output : outputs) output.Select(ctx, candidate);
    for (const auto& s : saved) std::memcpy(base + s.range.address, s.after.data(), s.range.size);
  }
  if (stats.calls % 1000 == 0) summary("periodic", stats.calls, stats.errors);
}
} // namespace pvz::native

#ifndef PVZ_NATIVE_REPLACEMENT_TEST
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
REXCVAR_DECLARE(bool, pvz_native_compare_all);
namespace pvz::native {
using RegisterOutput = Output<PPCContext, PPCRegister>;
struct Registration {
  const char* name;
  bool (*comparing)();
  Stats stats;
  Registration(const char* name, bool (*comparing)());
};
void Summarize(Registration& registration, const char* reason);
void Mismatch(Registration& registration, const PPCContext& input, const std::string& detail);
bool Prepare(Registration& registration);
void Observe(Registration& registration, uint32_t lr);
void ReplayCompleted();
bool RenderTracing();
struct RenderScope {
  const char* previous;
  explicit RenderScope(const char* name);
  ~RenderScope();
};
} // namespace pvz::native

// Declare once in the batch TU, before its replacement registrations.
#define PVZ_NATIVE_BATCH(BATCH) \
  REXCVAR_DEFINE_BOOL(pvz_native_##BATCH, false, "Native port", "Use proven " #BATCH " replacements") \
      .lifecycle(rex::cvar::Lifecycle::kInitOnly); \
  REXCVAR_DEFINE_BOOL(pvz_native_##BATCH##_compare, false, "Diagnostics", "Compare/observe " #BATCH) \
      .lifecycle(rex::cvar::Lifecycle::kInitOnly);

// OUTPUTS: container of RegisterOutput. RANGES: callable (entry context) ->
// container<Range>. NATIVE: callable (context, base). All flags are init-only:
// the fast path reads one cached bool and immediately delegates to __imp__.
#define PVZ_NATIVE_REPLACEMENT(NAME, SYMBOL, NATIVE, SAFETY, OUTPUTS, RANGES) \
  REXCVAR_DEFINE_BOOL(pvz_native_##NAME, false, "Native port", "Use native " #NAME) \
      .lifecycle(rex::cvar::Lifecycle::kInitOnly); \
  REXCVAR_DEFINE_BOOL(pvz_native_##NAME##_compare, false, "Diagnostics", "Compare " #NAME) \
      .lifecycle(rex::cvar::Lifecycle::kInitOnly); \
  REXCVAR_DEFINE_BOOL(pvz_native_##NAME##_inject_error, false, "Diagnostics", "Perturb compare " #NAME) \
      .lifecycle(rex::cvar::Lifecycle::kInitOnly); \
  namespace { \
  pvz::native::Registration replacement_##NAME{#NAME, [] { \
    return REXCVAR_GET(pvz_native_##NAME##_compare) || REXCVAR_GET(pvz_native_compare_all); }}; \
  } \
  REX_EXTERN(__imp__##SYMBOL); \
  REX_HOOK_RAW(SYMBOL) { \
    static const bool active = REXCVAR_GET(pvz_native_##NAME) || \
        REXCVAR_GET(pvz_native_##NAME##_compare) || REXCVAR_GET(pvz_native_compare_all); \
    if (!active) { __imp__##SYMBOL(ctx, base); return; } \
    static const pvz::native::Mode mode{REXCVAR_GET(pvz_native_##NAME), \
        replacement_##NAME.comparing(), REXCVAR_GET(pvz_native_##NAME##_inject_error)}; \
    static const bool prepared = !mode.compare || pvz::native::Prepare(replacement_##NAME); \
    pvz::native::Invoke(ctx, base, mode, SAFETY, replacement_##NAME.stats, OUTPUTS, RANGES, \
      NATIVE, __imp__##SYMBOL, \
      [](const PPCContext& input, const std::string& detail) { \
        pvz::native::Mismatch(replacement_##NAME, input, detail); }, \
      [](const char* reason, uint64_t, uint64_t) { \
        pvz::native::Summarize(replacement_##NAME, reason); }); \
  }
// PROVEN gates only batch activation. Individual flags remain available for
// discovery. Snapshot-only compare requests execute the original exactly once
// and record coverage; they never enter the differential engine.
#define PVZ_NATIVE_BATCH_REPLACEMENT(BATCH, PROVEN, NAME, SYMBOL, NATIVE, SAFETY, OUTPUTS, RANGES) \
  REXCVAR_DEFINE_BOOL(pvz_native_##NAME, false, "Native port", "Use native " #NAME) \
      .lifecycle(rex::cvar::Lifecycle::kInitOnly); \
  REXCVAR_DEFINE_BOOL(pvz_native_##NAME##_compare, false, "Diagnostics", "Compare/observe " #NAME) \
      .lifecycle(rex::cvar::Lifecycle::kInitOnly); \
  REXCVAR_DEFINE_BOOL(pvz_native_##NAME##_inject_error, false, "Diagnostics", "Perturb compare " #NAME) \
      .lifecycle(rex::cvar::Lifecycle::kInitOnly); \
  namespace { \
  bool enabled_##NAME() { return REXCVAR_GET(pvz_native_##NAME) || \
      ((PROVEN) && REXCVAR_GET(pvz_native_##BATCH)); } \
  bool compare_##NAME() { return REXCVAR_GET(pvz_native_##NAME##_compare) || \
      REXCVAR_GET(pvz_native_compare_all) || ((PROVEN) && REXCVAR_GET(pvz_native_##BATCH##_compare)); } \
  pvz::native::Registration replacement_##NAME{#NAME, [] { \
    return enabled_##NAME() || compare_##NAME(); }}; \
  } \
  REX_EXTERN(__imp__##SYMBOL); \
  REX_HOOK_RAW(SYMBOL) { \
    static const bool active = enabled_##NAME() || compare_##NAME() || pvz::native::RenderTracing(); \
    if (!active) { __imp__##SYMBOL(ctx, base); return; } \
    static const pvz::native::Mode mode{enabled_##NAME(), \
        compare_##NAME() && SAFETY == pvz::native::Safety::DeclaredOutputsOnly, \
        REXCVAR_GET(pvz_native_##NAME##_inject_error)}; \
    static const bool prepared = pvz::native::Prepare(replacement_##NAME); \
    const auto entry_lr = uint32_t(ctx.lr); \
    pvz::native::RenderScope render_scope{#NAME}; \
    pvz::native::Invoke(ctx, base, mode, SAFETY, replacement_##NAME.stats, OUTPUTS, RANGES, \
      NATIVE, __imp__##SYMBOL, \
      [](const PPCContext& input, const std::string& detail) { \
        pvz::native::Mismatch(replacement_##NAME, input, detail); }, \
      [](const char* reason, uint64_t, uint64_t) { pvz::native::Summarize(replacement_##NAME, reason); }); \
    if (!mode.compare) pvz::native::Observe(replacement_##NAME, entry_lr); \
  }
#endif
