#include "challenge_mouse.h"
#include "bonus_games.h"
#include "mouse_bridge.h"
#include "game_settings.h"
#include "guest_memory.h"
#include "guest_call.h"
#include "guest_layout.h"
#include "input_trace.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <array>
#include <algorithm>
#include <bit>
#include <cstring>

REX_EXTERN(sub_82169968); // ChallengeScreen::GamepadButtonDown
REX_EXTERN(sub_82169CD8); // ChallengeScreen::ScrollTo
REX_EXTERN(__imp__sub_82167958); // ChallengeScreen::DrawChallenge
REX_EXTERN(__imp__sub_821684F0); // ChallengeScreen::Draw
REX_EXTERN(__imp__sub_82169058); // ChallengeScreen::Update
REX_EXTERN(__imp__sub_82166FB0); // ChallengeScreen deleting destructor
REX_EXTERN(__imp__sub_8249C6A8); // Graphics::ClipRect

using namespace pvz::layout;
using pvz::GuestCall;

namespace {
using pvz::guest::Read;
using pvz::guest::Write;
using pvz::guest::Float;
struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  bool Contains(pvz::Point p) const { return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h; }
};
struct Row { Rect rect; int id = -1; };
struct State {
  uint32_t screen = 0;
  std::array<Row, 94> rows{};
  Rect clip;
  int hover = -1, pressed = -1;
  int wheel_target = -1;
  Rect scroll_track, scroll_thumb;
  bool dragging = false;
  float grip = 0;
} state;
uint32_t drawing_screen = 0;
int drawing_row = -1, drawing_id = -1;
float row_origin_x = 0, row_origin_y = 0;
bool IsScreen(uint8_t* b, uint32_t s) { return s && Read(b, s) == kChallengeScreen; }
uint32_t Button(uint8_t* b, uint32_t s, int id) {
  return id >= 0 && id < 94 ? Read(b, s + 0xD8 + id * 4) : 0;
}
int Hit(uint8_t* b, uint32_t s, std::optional<pvz::Point> point) {
  if (!point || !IsScreen(b, s) || s != state.screen) return -1;
  const pvz::Point pixel{point->x * 1.6f, point->y * 1.2f};
  if (!state.clip.Contains(pixel)) return -1;
  for (const auto& row : state.rows) {
    const auto button = Button(b, s, row.id);
    if (button && b[button + 0x70] && row.rect.Contains(pixel)) return row.id;
  }
  return -1;
}
void Press(uint8_t* b, uint32_t s, int id, bool pressed) {
  if (auto button = Button(b, s, id)) b[button + 0x74] = pressed;
}
}

void pvz::CancelChallengeMouse(uint8_t* b, uint32_t live_screen) {
  if (live_screen == state.screen && IsScreen(b, live_screen)) Press(b, live_screen, state.pressed, false);
  state.pressed = -1;
  state.hover = -1;
  state.dragging = false;
}
void pvz::UpdateChallengePointer(PPCContext&, uint8_t* b, uint32_t screen, std::optional<Point> point) {
  if (state.dragging && point && screen == state.screen && IsScreen(b, screen)) {
    const int count = std::clamp(int(Read(b, screen + 0x5B4)), 0, 94);
    const float travel = state.scroll_track.h - state.scroll_thumb.h;
    if (travel > 0) state.wheel_target = std::clamp(int(std::lround(
        (point->y * 1.2f - state.grip - state.scroll_track.y) / travel * std::max(0, count - 4))),
        0, std::max(0, count - 4));
    return;
  }
  const int hover = Hit(b, screen, point);
  if (hover != state.hover && REXCVAR_GET(pvz_input_trace) && IsScreen(b, screen))
    REXLOG_INFO("PvZ challenge hover {}", hover);
  state.hover = hover;
  if (hover >= 0) Write(b, screen + 0x5BC, hover);
  if (state.pressed >= 0 && screen == state.screen && IsScreen(b, screen))
    Press(b, screen, state.pressed, hover == state.pressed);
}
void pvz::ScrollChallenge(PPCContext& ctx, uint8_t* b, uint32_t screen, int steps) {
  if (!steps || !IsScreen(b, screen) || screen != state.screen) return;
  CancelChallengeMouse(b, screen);
  const int count = std::clamp(int(Read(b, screen + 0x5B4)), 0, 94);
  const int before = state.wheel_target >= 0 ? state.wheel_target : int(Read(b, screen + 0x2BC));
  const int after = std::clamp(before - steps, 0, std::max(0, count - 4));
  state.wheel_target = after;
  // Wheel navigation can leave the native gamepad selection off screen.
  // Keep it on this page so switching to a controller has a visible focus.
  int selected = -1;
  for (int i = 0; i < count; ++i)
    if (Read(b, screen + 0x2C4 + i * 4) == Read(b, screen + 0x5BC)) selected = i;
  if (count && (selected < after || selected >= after + 4))
    Write(b, screen + 0x5BC, Read(b, screen + 0x2C4 + after * 4));
  PVZ_TRACE("PvZ challenge scroll {} -> {} ({} rows)", before, after, count);
}
bool pvz::ChallengeMouseButton(PPCContext& ctx, uint8_t* b, uint32_t screen, Point point, int button, bool down) {
  if (!IsScreen(b, screen)) return false;
  if (button == 2) return true;
  if (button < 0) {
    CancelChallengeMouse(b, screen);
    if (down) GuestCall(ctx, b, sub_82169968, screen, 7, int(Read(b, screen + 0xC0)), 0);
    return true;
  }
  const Point pixel{point.x * 1.6f, point.y * 1.2f};
  if (button == 1 && (state.dragging || (down && state.scroll_track.Contains(pixel)))) {
    if (down) {
      CancelChallengeMouse(b, screen);
      state.dragging = true;
      state.grip = state.scroll_thumb.Contains(pixel) ? pixel.y - state.scroll_thumb.y : state.scroll_thumb.h / 2;
      UpdateChallengePointer(ctx, b, screen, point);
    } else state.dragging = false;
    return true;
  }
  const int id = Hit(b, screen, point);
  if (down) {
    state.pressed = id;
    Press(b, screen, id, true);
    PVZ_TRACE("PvZ challenge pressed {}", id);
  } else {
    const int pressed = state.pressed;
    CancelChallengeMouse(b, screen);
    if (id >= 0 && pressed == id) {
      Write(b, screen + 0x5BC, id);
      PVZ_TRACE("PvZ challenge activate {} locked {}", id, b[Button(b, screen, id) + 0x72] != 0);
      GuestCall(ctx, b, sub_82169968, screen, 6, int(Read(b, screen + 0xC0)), 0);
    } else PVZ_TRACE("PvZ challenge click cancelled");
  }
  return true;
}

void pvz::RecordChallengeRowPaint(PPCContext& ctx, uint8_t* b) {
  if (!drawing_screen || ctx.lr != 0x82167BB8 || drawing_row < 0 || drawing_row >= 94) return;
  const auto g = ctx.r3.u32;
  const auto button = Button(b, drawing_screen, drawing_id);
  const float pressed = button && b[button + 0x74] ? 4.0f : 0.0f;
  const float painted_x = Float(b, g + 8) + float(ctx.r5.s32) - pressed;
  const float resting_x = row_origin_x + float(ctx.r5.s32) - pressed;
  const float left = std::min(painted_x, resting_x);
  // The slide and pressed offsets must not move a stationary pointer out
  // of its own row. Include both the resting and expanded horizontal area.
  state.rows[drawing_row] = {{left, row_origin_y + float(ctx.r6.s32) - pressed,
      std::max(painted_x, resting_x) + float(ctx.r7.s32) - left, float(ctx.r8.s32)}, drawing_id};
}
REX_HOOK_RAW(sub_8249C6A8) {
  if (drawing_screen && ctx.lr == 0x82168DD0) {
    const auto g = ctx.r3.u32;
    state.clip = {Float(base, g + 8) + float(ctx.r4.s32), Float(base, g + 12) + float(ctx.r5.s32),
        float(ctx.r6.s32), float(ctx.r7.s32)};
  }
  if (drawing_screen && ctx.lr == 0x82168D0C) {
    // Native scrollbar clip is the track; its animated thumb is derived
    // from ScrollTo's current page, matching the renderer's four-row page.
    const auto g = ctx.r3.u32;
    state.scroll_track = {Float(base, g + 8) + float(ctx.r4.s32), Float(base, g + 12) + float(ctx.r5.s32),
        float(ctx.r6.s32), float(ctx.r7.s32)};
    const int count = std::clamp(int(Read(base, drawing_screen + 0x5B4)), 1, 94);
    const float h = std::min(state.scroll_track.h, state.scroll_track.h * 4 / count);
    const float page = float(Read(base, drawing_screen + 0x2B8));
    const float y = state.scroll_track.y + (state.scroll_track.h - h) * page / std::max(1, count - 4);
    state.scroll_thumb = {state.scroll_track.x, y, state.scroll_track.w, h};
  }
  __imp__sub_8249C6A8(ctx, base);
}
REX_HOOK_RAW(sub_821684F0) {
  const auto screen = ctx.r3.u32;
  if (state.screen != screen) { state = {}; state.screen = screen; }
  state.rows = {};
  const auto saved = drawing_screen; drawing_screen = screen;
  pvz::SetDrawingBonusPage(Read(base, screen + 0x258) == pvz::kBonusPage);
  __imp__sub_821684F0(ctx, base);
  pvz::SetDrawingBonusPage(false);
  drawing_screen = saved;
}
REX_HOOK_RAW(sub_82167958) {
  const int old_row = drawing_row, old_id = drawing_id;
  const float old_x = row_origin_x, old_y = row_origin_y;
  drawing_row = ctx.r6.s32; drawing_id = ctx.r5.s32;
  row_origin_x = Float(base, ctx.r4.u32 + 8); row_origin_y = Float(base, ctx.r4.u32 + 12);
  const auto screen = ctx.r3.u32;
  const auto selected = Read(base, screen + 0x5BC);
  if (pvz::MouseOwnsInput() && state.screen == screen) Write(base, screen + 0x5BC, uint32_t(state.hover));
  {
    pvz::BonusIconScope icon(base, screen, ctx.r5.s32);
    __imp__sub_82167958(ctx, base);
  }
  Write(base, screen + 0x5BC, selected);
  drawing_row = old_row; drawing_id = old_id;
  row_origin_x = old_x; row_origin_y = old_y;
}
REX_HOOK_RAW(sub_82169058) {
  const auto screen = ctx.r3.u32;
  std::array<float, 94> before{};
  const int count = std::clamp(int(Read(base, screen + 0x5B4)), 0, 94);
  const bool mouse = pvz::MouseOwnsInput() && state.screen == screen;
  if (mouse) for (int i = 0; i < count; ++i) before[i] = Float(base, screen + 0x43C + i * 4);
  __imp__sub_82169058(ctx, base);
  if (!mouse) state.wheel_target = -1;
  if (mouse && state.wheel_target >= 0 && Read(base, screen + 0x2B8) == Read(base, screen + 0x2BC)) {
    const int current = int(Read(base, screen + 0x2BC));
    if (current != state.wheel_target) {
      if (state.dragging && REXCVAR_GET(pvz_input_trace))
        REXLOG_INFO("PvZ challenge scrollbar page {} target {}", current, state.wheel_target);
      GuestCall(ctx, base, sub_82169CD8, screen, current + (state.wheel_target > current ? 1 : -1), 0, 0);
    } else state.wheel_target = -1;
  }
  if (mouse && state.hover < 0) {
    const auto tooltip = Read(base, screen + 0x254);
    if (tooltip) base[tooltip + 0x64] = 0;
  }
  // Native hover expansion uses 0.016 per update and saturates at 0.25.
  // Retain its renderer and easing, including contraction outside the rows.
  if (mouse) for (int i = 0; i < count; ++i) {
    const bool over = int(Read(base, screen + 0x2C4 + i * 4)) == state.hover;
    const float animation = std::clamp(before[i] + (over ? 0.016f : -0.016f), 0.0f, 0.25f);
    Write(base, screen + 0x43C + i * 4, std::bit_cast<uint32_t>(animation));
  }
}
REX_HOOK_RAW(sub_82166FB0) {
  if (state.screen == ctx.r3.u32) state = {};
  pvz::RemoveMouseNavigation(ctx, ctx.r3.u32, base);
  __imp__sub_82166FB0(ctx, base);
}
