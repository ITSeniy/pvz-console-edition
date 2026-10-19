// B002. Behavior and offsets: Xbox image / generated bodies, T15 evidence.
// Address names are intentional: no unverified Android method annotations.
#include "replacement.h"
#include "pvz_pch.h"
#include "guest_memory.h"
#include "xbox/Gamepad.h"
#include "xbox/BaseGamepadControls.h"
#include <cmath>
#ifndef PVZ_T15_SYNTHETIC_TEST
#include "guest_call.h"
#endif

#ifndef PVZ_T15_SYNTHETIC_TEST
PVZ_NATIVE_BATCH(b002)
#endif
REX_EXTERN(sub_82580870);
REX_EXTERN(sub_822CB598);
REX_EXTERN(sub_825809E8);
REX_EXTERN(sub_825F8F50);
REX_EXTERN(sub_82166AE0);
REX_EXTERN(sub_82189728);
REX_EXTERN(sub_82128580);
REX_EXTERN(sub_825FCDE0);
REX_EXTERN(sub_82189A58);
REX_EXTERN(sub_821538D8);
REX_EXTERN(sub_821388A0);
REX_EXTERN(sub_82149538);
REX_EXTERN(sub_821495B0);
REX_EXTERN(sub_82149728);
REX_EXTERN(sub_82149848);
REX_EXTERN(sub_82149A08);
REX_EXTERN(sub_82149AE0);
REX_EXTERN(sub_82291D20);
REX_EXTERN(sub_82139808);
namespace {
using pvz::native::Range;
using pvz::native::RegisterOutput;
const std::array<RegisterOutput, 1> r3out{{{"r3", &PPCContext::r3, 64, pvz::native::Kind::Unsigned}}};
const std::array<RegisterOutput, 0> noOutputs{};
auto NoRanges(const PPCContext&) {
  return std::array<Range, 0>{};
}
void Identity(PPCContext&, uint8_t*) {}
void Zero(PPCContext& c, uint8_t*) {
  c.r3.u64 = 0;
}
auto& G(uint8_t* b, uint32_t at) {
  return *reinterpret_cast<pvz::xbox::Gamepad*>(b + at);
}
auto& C(uint8_t* b, uint32_t at) {
  return *reinterpret_cast<pvz::xbox::BaseGamepadControls*>(b + at);
}
void Connected(PPCContext& c, uint8_t* b) {
  c.r3.u64 = G(b, c.r3.u32).connected;
}
const std::array<RegisterOutput, 1> f1out{{{"f1", &PPCContext::f1, 64, pvz::native::Kind::FloatBits}}};
auto AxisRange(const PPCContext& c) {
  return std::array{Range{c.r1.u32 - 16, 8}};
}
auto VibrationRange(const PPCContext& c) {
  return std::array{Range{c.r3.u32 + 0x18C, 16}};
}
void Store64(uint8_t* b, uint32_t at, uint64_t v) {
  pvz::guest::Write(b, at, uint32_t(v >> 32));
  pvz::guest::Write(b, at + 4, uint32_t(v));
}
void FloatZero(PPCContext& c, uint8_t* b) {
  c.fpscr.disableFlushMode();
  c.f1.f64 = double(pvz::guest::Float(b, 0x82000A94));
}
template <bool Y> void Axis(PPCContext& c, uint8_t* b) {
  const int64_t v = Y ? int32_t(G(b, c.r3.u32).axis_y) : int32_t(G(b, c.r3.u32).axis_x);
  Store64(b, c.r1.u32 - 16, uint64_t(v));
  c.fpscr.disableFlushMode();
  c.f1.f64 = double(float(double(float(double(v))) * double(pvz::guest::Float(b, 0x8201827C))));
}
void Vibration(PPCContext& c, uint8_t* b) {
  c.fpscr.disableFlushMode();
  const double limit = pvz::guest::Float(b, 0x82000B80);
  if (c.f1.f64 > limit)
    c.f1.f64 = limit;
  auto& g = G(b, c.r3.u32);
  g.vibration_left = float(c.f1.f64);
  // Xbox bgt treats unordered as false: NaN in f2 propagates into right.
  g.vibration_right = float(c.f2.f64 > limit ? limit : c.f2.f64);
  g.vibration_duration = float(c.f3.f64);
  g.vibration_remaining = float(c.f3.f64);
}
#ifndef PVZ_T15_SYNTHETIC_TEST
// Child calls keep Xbox LR sites and exact frame sizes. No host object lives
// in guest memory; frame scratch is deliberately SnapshotsOnly.
using pvz::native::Args;
using pvz::native::Call;
using pvz::native::Frame;
using pvz::native::Virtual;

// 82190D80: end old state, publish new state, enter with previous state.
void SetState(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, state = c.r4.u32;
  Frame frame(c, b, 0x70);
  Virtual(c, b, self, 24, 0x82190DA4);
  const uint32_t previous = C(b, self).state;
  C(b, self).state = state;
  Args(c, {self, previous});
  Virtual(c, b, self, 20, 0x82190DC4);
  if (state != previous) {
    c.fpscr.disableFlushMode();
    C(b, self).state_elapsed = pvz::guest::Float(b, 0x820B5244);
  }
}
void GamepadConstruct(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, index = c.r4.u32;
  Frame f(c, b, 0x70);
  pvz::guest::Write(b, self, 0x8207DA7C);
  Call(c, b, sub_82580870, 0x82580918, {self + 12});
  G(b, self).user_index = index;
  constexpr uint32_t map[]{0x20, 0x10, 0x1000, 0x2000, 0x4000, 0x8000, 0x100, 0x200, 0, 0,
                           0x40, 0x80, 1,      2,      4,      8,      0,     0,     0, 0};
  for (uint32_t i = 0; i < 20; ++i)
    G(b, self).button_map[i] = map[i];
  G(b, self).field_08 = 0;
  const float zero = pvz::guest::Float(b, 0x82000A94);
  G(b, self).vibration_left = zero;
  G(b, self).vibration_right = zero;
  G(b, self).vibration_duration = zero;
  G(b, self).vibration_remaining = zero;
  c.r3.u64 = self;
}
void GamepadDestruct(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  pvz::guest::Write(b, c.r1.u32 + 0x50, 0);
  pvz::guest::Write(b, self, 0x8207DA7C);
  Call(c, b, sub_822CB598, 0x82580A24, {G(b, self).user_index, c.r1.u32 + 0x50});
}
void GamepadDelete(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_825809E8, 0x825818B8, {self});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x825818C8, {self});
  c.r3.u64 = self;
}
void DigitalAxis(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Virtual(c, b, self, 24, 0x82580AAC);
  c.fpscr.disableFlushMode();
  c.f1.f64 = pvz::guest::Float(b, c.r3.u8 ? 0x82000B80 : 0x82000A94);
}
template <bool Y> void Stick(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  c.fpscr.disableFlushMode();
  const double x = float(int32_t(G(b, self).axis_x)), y = float(int32_t(G(b, self).axis_y));
  const double magnitude = float(std::sqrt(double(float(double(float(y * y)) + double(float(x * x))))));
  const double dead = pvz::guest::Float(b, 0x8207DAC0);
  if (magnitude < dead) {
    FloatZero(c, b);
    return;
  }
  const double scale = float(magnitude - dead);
  pvz::guest::Write(b, c.r1.u32 + 0x50, std::bit_cast<uint32_t>(float(x * scale)));
  pvz::guest::Write(b, c.r1.u32 + 0x54, std::bit_cast<uint32_t>(float(y * scale)));
  Call(c, b, sub_82166AE0, Y ? 0x82580C40 : 0x82580BA8, {c.r1.u32 + 0x58, c.r1.u32 + 0x50});
  c.f1.f64 = pvz::guest::Float(b, c.r3.u32 + (Y ? 4 : 0));
}
void ControlsConstruct(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, board = c.r4.u32, player = c.r5.u32, user = c.r6.u32;
  Frame f(c, b, 0xB0);
  pvz::guest::Write(b, c.r1.u32 + 0xC4, self);
  C(b, self).vtable = 0x82060D04;
  Call(c, b, sub_82189728, 0x821898F8, {self + 4});
  auto& w = C(b, self);
  w.board = board;
  w.vtable = 0x820BA5A4;
  w.listener_vtable = 0x820BA5D4;
  const auto zero = pvz::guest::Read(b, 0x820B5244);
  for (uint32_t at :
       {0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x74, 0x78, 0x7C, 0x80, 0x84, 0x88, 0x8C, 0x90, 0x94, 0x98})
    pvz::guest::Write(b, self + at, zero);
  // Original registers two protected byte regions in its listener vector,
  // then clears them; preserve the order including the redundant FP stores.
  for (auto [offset, size, lr1, lr2] : std::array<std::array<uint32_t, 4>, 2>{
           {{0x48, 8, 0x821899C0, 0x821899D0}, {0x50, 0x60, 0x821899EC, 0x821899FC}}}) {
    pvz::guest::Write(b, c.r1.u32 + 0x58, self + offset);
    pvz::guest::Write(b, c.r1.u32 + 0x5C, size);
    pvz::guest::Write(b, c.r1.u32 + 0x50, self + 0x94);
    Call(c, b, sub_82128580, lr1, {self + 8, c.r1.u32 + 0x58});
    Call(c, b, sub_825FCDE0, lr2, {self + offset, 0, size});
  }
  w.player_index = player;
  w.user_index = user;
  w.flag_48 = 1;
  pvz::guest::Write(b, self + 0x40, 0xAAE56);
  w.field_ac = 0;
  c.r3.u64 = self;
}
void ControlsDelete(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  C(b, self).listener_vtable = 0x820B4828;
  const uint32_t region = pvz::guest::Read(b, self + 0x14);
  if (region)
    Call(c, b, sub_825F8F50, 0x82189A90, {region});
  const uint32_t first = pvz::guest::Read(b, self + 8);
  for (uint32_t at : {0x14, 0x18, 0x1C})
    pvz::guest::Write(b, self + at, 0);
  Call(c, b, sub_825F8F50, 0x82189AA8, {first});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x82189ABC, {self});
  c.r3.u64 = self;
}
void ControlsDeleteThunk(PPCContext& c, uint8_t* b) {
  c.r3.u64 -= 4;
  sub_82189A58(c, b);
}
void Challenge(PPCContext& c, uint8_t* b) {
  c.r3.u64 = pvz::guest::Read(b, uint32_t(C(b, c.r3.u32).board) + 0x1D4);
  if (c.r3.u32)
    sub_821538D8(c, b);
}
uint32_t Truncate(double v) {
  // Generated fctiwz saturates positive overflow and returns INT_MIN for
  // NaN/negative overflow. Avoid undefined host float-to-int conversions.
  if (std::isnan(v) || v < double(INT_MIN))
    return 0x80000000u;
  if (v >= double(INT_MAX))
    return INT_MAX;
  return uint32_t(int32_t(v));
}
void EndState(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  auto& w = C(b, self);
  if (int32_t(w.state) == 2) {
    w.vector_84[0] = pvz::guest::Float(b, 0x820B5244);
    w.vector_84[1] = pvz::guest::Float(b, 0x820B5244);
  } else if (int32_t(w.state) == 6 && float(w.state_elapsed) > pvz::guest::Float(b, 0x820B526C)) {
    const uint32_t y = Truncate(float(w.target_y)), x = Truncate(float(w.target_x));
    Store64(b, c.r1.u32 + 0x50, int64_t(int32_t(y)));
    Store64(b, c.r1.u32 + 0x58, int64_t(int32_t(x)));
    Call(c, b, sub_821388A0, 0x82190468, {w.board, x, y, 6});
  }
}
void PredictedCell(PPCContext& c, uint8_t* b) {
  const uint32_t out = c.r3.u32, self = c.r4.u32;
  Frame f(c, b, 0x90);
  auto& w = C(b, self);
  const uint32_t app = w.app, board = w.board;
  const double scale = pvz::guest::Float(b, pvz::guest::Read(b, app + 0x9CC) == 44 ? 0x820B52B0 : 0x820B53E0);
  const uint32_t y = Truncate(float(std::fma(double(float(w.delta_y)), scale, double(float(w.target_y)))));
  const uint32_t x = Truncate(float(std::fma(double(float(w.delta_x)), scale, double(float(w.target_x)))));
  Call(c, b, sub_82149538, 0x82190E5C, {board, x, y});
  const uint32_t column = std::max(0, c.r3.s32);
  Call(c, b, sub_821495B0, 0x82190E88, {board, uint32_t(std::max(40, int32_t(x))), y});
  pvz::guest::Write(b, out, std::bit_cast<uint32_t>(float(int32_t(column))));
  pvz::guest::Write(b, out + 4, std::bit_cast<uint32_t>(float(std::max(0, c.r3.s32))));
  c.r3.u64 = out;
}
// Return pixel anchor for the selected column/row. The original helpers keep
// r4/r5/r6/r7 live across calls; use those registers rather than a PC formula.
void CellAnchor(PPCContext& c, uint8_t* b) {
  const uint32_t out = c.r3.u32, self = c.r4.u32;
  Frame f(c, b, 0x80);
  auto& w = C(b, self);
  pvz::guest::Write(b, out, pvz::guest::Read(b, self + 0x74));
  pvz::guest::Write(b, out + 4, pvz::guest::Read(b, self + 0x78));
  Args(c, {c.r1.u32 + 0x50, self});
  Virtual(c, b, self, 40, 0x82190F18);
  const uint32_t board = w.board;
  c.r7.u64 = board;
  Args(c, {board, Truncate(pvz::guest::Float(b, c.r1.u32 + 0x50)),
           Truncate(pvz::guest::Float(b, c.r1.u32 + 0x54))});
  c.lr = 0x82190F44;
  sub_82149A08(c, b);
  const float halfWidth = float(c.r3.s32 / 2);
  c.r3.u64 = board;
  c.lr = 0x82190F68;
  sub_82149AE0(c, b);
  const float halfHeight = float(c.r3.s32 / 2);
  const uint32_t app = pvz::guest::Read(b, board + 0xF4);
  const auto state = pvz::guest::Read(b, board + 0x55E0);
  if (pvz::guest::Read(b, app + 0x9CC) == 44 && (state == 6 || state == 7 || state == 8)) {
    Call(c, b, sub_82291D20, 0x82190FB8, {pvz::guest::Read(b, app + 0x9F0)});
    c.r6.u64 = c.r3.u64;
  } else
    c.r6.u64 = c.r4.u32 * 80u + 40;
  c.r3.u64 = board;
  c.lr = 0x82190FD8;
  sub_82149848(c, b);
  pvz::guest::Write(b, out, std::bit_cast<uint32_t>(float(float(c.r6.s32) + halfWidth)));
  pvz::guest::Write(b, out + 4, std::bit_cast<uint32_t>(float(float(c.r3.s32) + halfHeight)));
  c.r3.u64 = out;
}
void BeginState(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, next = c.r4.u32;
  Frame f(c, b, 0xA0);
  auto& w = C(b, self);
  const int32_t state = w.state;
  if (state == 3 || (state == 4 && !b[0x82D0BC31])) {
    Args(c, {c.r1.u32 + (state == 3 ? 0x60 : 0x68), self});
    Virtual(c, b, self, 36, state == 3 ? 0x821900A4 : 0x82190140);
    w.cursor_x = std::bit_cast<float>(pvz::guest::Read(b, c.r3.u32));
    w.cursor_y = std::bit_cast<float>(pvz::guest::Read(b, c.r3.u32 + 4));
    if (state == 4)
      return;
    w.field_ac = 0;
  }
  const auto zero = pvz::guest::Float(b, 0x820B5244);
  if (state == 1 || state == 3) {
    w.delta_x = zero;
    w.delta_y = zero;
    w.vector_84[0] = zero;
    w.vector_84[1] = zero;
    if (next == 4 || int32_t(w.state) == 3) {
      w.target_x = float(w.cursor_x);
      w.target_y = float(w.cursor_y);
    }
    return;
  }
  if (state == 2) {
    w.delta_x = zero;
    w.delta_y = zero;
    return;
  }
  if (state != 5 && state != 6)
    return;
  const uint32_t board = w.board, x = Truncate(float(w.target_x)), y = Truncate(float(w.target_y));
  const uint32_t bias = state == 5 ? 0 : 0x150;
  Call(c, b, sub_82149538, 0x8219018C + bias, {board, x, y});
  uint32_t col = 0;
  if (c.r3.s32 > 0) {
    Call(c, b, sub_82149538, 0x821901A4 + bias, {board, x, y});
    col = c.r3.u32;
  }
  Call(c, b, sub_82149728, 0x821901C0 + bias, {board, x, y});
  c.r5.u64 = c.r3.u64;
  const uint32_t app = pvz::guest::Read(b, board + 0xF4), bs = pvz::guest::Read(b, board + 0x55E0);
  if (pvz::guest::Read(b, app + 0x9CC) == 44 && (bs == 6 || bs == 7 || bs == 8)) {
    Call(c, b, sub_82291D20, 0x821901FC + bias, {pvz::guest::Read(b, app + 0x9F0), col});
    c.r6.u64 = c.r3.u64;
  } else
    c.r6.u64 = col * 80u + 40;
  Call(c, b, sub_82149AE0, 0x82190220 + bias, {board, col});
  const float halfHeight = float(c.r3.s32 / 2);
  c.r3.u64 = board;
  c.lr = 0x82190238 + bias;
  sub_82149848(c, b);
  const float anchorY = float(c.r3.s32);
  c.r3.u64 = board;
  c.lr = 0x82190264 + bias;
  sub_82149A08(c, b);
  w.cursor_y = float(double(halfHeight) + double(anchorY));
  w.cursor_x = float(double(float(c.r3.s32 / 2)) + double(float(c.r6.s32)));
  if (state == 6)
    Call(c, b, sub_82139808, 0x82190400, {board, 5, uint32_t(w.player_index)});
}
// Single-precision PPC operations round after each operation; fmadds first
// fuses double operands, then rounds. Keep that order even for NaN/-0.
double S(double x) {
  return double(float(x));
}
double Mul(double x, double y) {
  return S(x * y);
}
double Add(double x, double y) {
  return S(x + y);
}
double Sub(double x, double y) {
  return S(x - y);
}
double Fma(double x, double y, double z) {
  return S(std::fma(x, y, z));
}
double Length(double x, double y) {
  return S(std::sqrt(Fma(x, x, Mul(y, y))));
}
void ControlsUpdate(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  const double dt = c.f1.f64;
  Frame f(c, b, 0xC0);
  auto& w = C(b, self);
  c.fpscr.disableFlushMode();
  const int32_t state = w.state;
  const double elapsed = Add(dt, float(w.state_elapsed));
  w.state_elapsed = float(elapsed);
  const auto k = [&](uint32_t at) { return double(pvz::guest::Float(b, at)); };
  const double zero = k(0x820B5244), one = k(0x820B526C);
  auto transition = [&](uint32_t value, uint32_t lr) {
    Args(c, {self, value});
    Virtual(c, b, self, 32, lr);
  };
  auto vectorLength = [&] {
    return Fma(float(w.vector_8c[0]), float(w.vector_8c[0]),
               Mul(float(w.vector_8c[1]), float(w.vector_8c[1])));
  };
  auto normalize = [&](uint32_t output, uint32_t lr) {
    Call(c, b, sub_82166AE0, lr, {c.r1.u32 + output, self + 0x7C});
    return c.r3.u32;
  };
  auto steer = [&](double speed, uint32_t output, uint32_t lr) {
    w.delta_x = float(Sub(float(w.cursor_x), float(w.target_x)));
    w.delta_y = float(Sub(float(w.cursor_y), float(w.target_y)));
    const auto p = normalize(output, lr);
    w.delta_x = float(Mul(pvz::guest::Float(b, p), speed));
    w.delta_y = float(Mul(pvz::guest::Float(b, p + 4), speed));
  };
  if (state == 1) {
    if (vectorLength() > zero)
      transition(2, 0x82190938);
    else if (elapsed > one)
      pvz::guest::Write(b, self + 0x6C, std::bit_cast<uint32_t>(float(zero)));
    return;
  }
  if (state == 3) {
    double vx = float(w.vector_8c[0]), vy = float(w.vector_8c[1]);
    const uint32_t pad = pvz::guest::Read(b, uint32_t(w.app) + (uint32_t(w.user_index) + 13) * 4);
    auto down = [&](uint32_t key, uint32_t lr) {
      Args(c, {pad, key});
      Virtual(c, b, pad, 24, lr);
      return c.r3.u8 != 0;
    };
    if (down(16, 0x82190688))
      vy = one;
    else if (down(17, 0x821906C8))
      vy = k(0x820B5344);
    else if (down(18, 0x821906F8))
      vx = k(0x820B5344);
    else if (down(19, 0x82190728))
      vx = one;
    double scratchF0 = zero;
    if (Fma(vx, vx, Mul(vy, vy)) > zero) {
      const int32_t n = std::bit_cast<int32_t>(uint32_t(w.field_ac) - 1);
      w.field_ac = uint32_t(n);
      if (n <= 0) {
        const double factor = Mul(Length(vx, -vy), k(0x820B554C));
        w.target_x = float(Add(float(w.cursor_x), Mul(factor, vx)));
        w.target_y = float(Add(float(w.cursor_y), Mul(factor, -vy)));
        return;
      }
    }
    const double dx = Sub(float(w.cursor_x), float(w.target_x)),
                 dy = Sub(float(w.cursor_y), float(w.target_y));
    if (S(std::sqrt(Fma(dy, dy, Mul(dx, dx)))) < k(0x820B5448)) {
      // f0 is zero on this path; a nonzero input vector only changes it in
      // the branch above which returns immediately.
      w.delta_x = float(scratchF0);
      w.delta_y = float(scratchF0);
      w.vector_84[0] = float(scratchF0);
      w.vector_84[1] = float(scratchF0);
      w.target_x = float(w.cursor_x);
      w.target_y = float(w.cursor_y);
      return;
    }
    double speed = S(-std::fma(dt, k(0x820B5290), -Length(float(w.delta_x), float(w.delta_y))));
    const double scale = b[0x82D0BC31] ? (float(w.state_elapsed) > k(0x820B5348) ? k(0x820B5298) : one) : one;
    const double floor = Mul(scale, k(0x820B5288));
    if (!(speed > floor && speed < floor))
      speed = floor; // exact Xbox ble / blt sequence
    steer(speed, 0x58, 0x821908D0);
    return;
  }
  if (state == 2) {
    pvz::guest::Write(b, self + 0x6C,
                      std::bit_cast<uint32_t>(float(Add(dt, pvz::guest::Float(b, self + 0x6C)))));
    const double vector = vectorLength();
    const auto mode = pvz::guest::Read(b, uint32_t(w.app) + 0x9CC);
    if (vector == zero && mode == 20)
      transition(7, 0x821909C4);
    else if (vector == zero && mode != 24) {
      transition(4, 0x82190AF4);
      return;
    }
    double accel = k(0x820B5284);
    w.vector_84[0] = float(Mul(float(w.vector_8c[0]), accel));
    w.vector_84[1] = float(Mul(float(w.vector_8c[1]), accel));
    if (pvz::guest::Read(b, uint32_t(w.app) + 0x9CC) == 25) {
      accel = k(0x820B52A0);
      w.vector_84[0] = float(Mul(float(w.vector_8c[0]), accel));
      w.vector_84[1] = float(Mul(float(w.vector_8c[1]), accel));
    }
    w.vector_84[1] = -float(w.vector_84[1]);
    const double damping = k(0x820B5348);
    if (!(Fma(float(w.delta_y), float(w.vector_84[1]), Mul(float(w.delta_x), float(w.vector_84[0]))) >
          zero)) {
      w.delta_x = float(Mul(float(w.delta_x), damping));
      w.delta_y = float(Mul(float(w.delta_y), damping));
    }
    w.delta_y = float(Add(float(w.delta_y), Mul(float(w.vector_84[1]), dt)));
    w.delta_x = float(Add(float(w.delta_x), Mul(float(w.vector_84[0]), dt)));
    const double limit = float(w.state_elapsed) > damping ? k(0x820B5550) : k(0x820B5288);
    if (Length(float(w.delta_x), float(w.delta_y)) > limit) {
      const auto p = normalize(0x60, 0x82190B30);
      w.delta_x = pvz::guest::Float(b, p);
      w.delta_y = pvz::guest::Float(b, p + 4);
      w.delta_x = float(Mul(float(w.delta_x), limit));
      w.delta_y = float(Mul(limit, float(w.delta_y)));
    }
    return;
  }
  if (state == 4) {
    if (vectorLength() > zero) {
      transition(2, 0x82190938);
      return;
    }
    const double dx = Sub(float(w.cursor_x), float(w.target_x)),
                 dy = Sub(float(w.cursor_y), float(w.target_y));
    if (S(std::sqrt(Fma(dy, dy, Mul(dx, dx)))) < k(0x820B533C)) {
      transition(1, 0x82190BE0);
      return;
    }
    const auto mode = pvz::guest::Read(b, uint32_t(w.app) + 0x9CC);
    double speed = mode == 25 ? Sub(Length(float(w.delta_x), float(w.delta_y)), k(0x820B52A4))
                              : S(-std::fma(dt, k(0x820B5290), -Length(float(w.delta_x), float(w.delta_y))));
    const double scale = b[0x82D0BC31] ? (elapsed > k(0x820B5348) ? k(0x820B5298) : one) : one;
    const double floor = Mul(scale, k(0x820B5288));
    if (!(speed > floor && speed < floor))
      speed = floor;
    steer(speed, 0x68, 0x82190CB4);
    if (float(w.state_elapsed) > one)
      pvz::guest::Write(b, self + 0x6C, std::bit_cast<uint32_t>(float(zero)));
    return;
  }
  if (state == 7) {
    if (vectorLength() > zero) {
      transition(2, 0x82190938);
      return;
    }
    if (Fma(float(w.delta_x), float(w.delta_x), Mul(float(w.delta_y), float(w.delta_y))) < k(0x820B5318))
      transition(1, 0x82190D54);
    w.delta_x = float(Mul(float(w.delta_x), k(0x820B54BC)));
    w.delta_y = float(Mul(float(w.delta_y), k(0x820B54BC)));
  }
}
#endif
} // namespace
#ifndef PVZ_T15_SYNTHETIC_TEST
#define LEAF(A, P, B, O, R, I)                                                                               \
  PVZ_NATIVE_BATCH_REPLACEMENT(b002, P, gamepad_##A, sub_##A, B, pvz::native::Safety::DeclaredOutputsOnly,   \
                               O, R)
#define SNAP(A, P, B)                                                                                        \
  PVZ_NATIVE_BATCH_REPLACEMENT(b002, P, gamepad_##A, sub_##A, B, pvz::native::Safety::SnapshotsOnly,         \
                               noOutputs, NoRanges)
#endif
LEAF(82580A38, true, Connected, r3out, NoRanges, Connected)
LEAF(82580A50, true, Identity, r3out, NoRanges, Identity)
LEAF(82580A58, true, Zero, r3out, NoRanges, Identity)
LEAF(82580A60, true, Identity, r3out, NoRanges, Identity)
LEAF(82580CF0, true, Identity, r3out, NoRanges, Identity)
LEAF(82580A68, true, FloatZero, f1out, NoRanges, FloatZero)
LEAF(82580AD8, true, Axis<false>, f1out, AxisRange, Axis)
LEAF(82580B00, true, Axis<true>, f1out, AxisRange, Axis)
LEAF(82580CA8, true, Vibration, f1out, VibrationRange, Vibration)
SNAP(825808E8, true, GamepadConstruct)
SNAP(825809E8, false, GamepadDestruct)
SNAP(82581898, false, GamepadDelete)
SNAP(82580A90, false, DigitalAxis)
SNAP(82580B28, true, Stick<false>)
SNAP(82580BC0, true, Stick<true>)
SNAP(821898C0, true, ControlsConstruct)
SNAP(82189A58, false, ControlsDelete)
SNAP(82192C10, false, ControlsDeleteThunk)
SNAP(82189B28, false, Challenge)
SNAP(82190D80, true, SetState)
SNAP(82190408, true, EndState)
SNAP(82190DE0, true, PredictedCell)
SNAP(82190ED8, true, CellAnchor)
SNAP(82190048, true, BeginState)
SNAP(821905F8, true, ControlsUpdate)
#undef LEAF
#undef SNAP
