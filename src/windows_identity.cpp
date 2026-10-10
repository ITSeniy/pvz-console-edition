#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <string>
#include <cstring>
#include "input_backend.h"
#include "guest_memory.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

REX_EXTERN(__imp__sub_82616510); // Native HRESULT wrapper for XamUserGetSigninInfo.
namespace {
const std::string& WindowsName() {
  static const std::string name = [] {
#ifdef _WIN32
    wchar_t wide[256]{};
    DWORD length = DWORD(std::size(wide));
    if (GetUserNameW(wide, &length) && length > 1) {
      const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide, int(length - 1), nullptr, 0, nullptr, nullptr);
      std::string utf8(bytes, '\0');
      WideCharToMultiByte(CP_UTF8, 0, wide, int(length - 1), utf8.data(), bytes, nullptr, nullptr);
      // Xbox's local sign-in buffer is 16 bytes including its terminator.
      size_t end = std::min<size_t>(utf8.size(), 15);
      while (end < utf8.size() && end && (static_cast<unsigned char>(utf8[end]) & 0xC0) == 0x80) --end;
      utf8.resize(end);
      if (!utf8.empty()) return utf8;
    }
#endif
    return std::string("Player");
  }();
  return name;
}
}
REX_HOOK_RAW(sub_82616510) {
  const auto user = ctx.r3.u32, info = ctx.r5.u32;
  const bool guest = user == 1 && pvz::LocalPlayersActive();
  if (guest) ctx.r3.u32 = 0;
  __imp__sub_82616510(ctx, base);
  if ((!user || guest) && info && !ctx.r3.u32) {
    const auto name = guest ? std::string("Player 2") : WindowsName();
    static bool reported = false;
    if (!guest && !reported) { reported = true; REXLOG_INFO("PvZ local player name {}", name); }
    std::memset(base + info + 0x18, 0, 16);
    std::memcpy(base + info + 0x18, name.data(), name.size());
    if (guest) {
      pvz::guest::Write(base, info + 4, pvz::guest::Read(base, info + 4) ^ 1u);
      pvz::guest::Write(base, info + 0xC, 1); // Local sign-in, never an online account.
    }
  }
}
REX_EXTERN(__imp__sub_822C9E48);
REX_HOOK_RAW(sub_822C9E48) {
  if (ctx.r3.u32 == 1 && pvz::LocalPlayersActive()) { ctx.r3.u64 = 1; return; }
  __imp__sub_822C9E48(ctx, base);
}
REX_EXTERN(__imp__sub_822CA0B0);
REX_HOOK_RAW(sub_822CA0B0) {
  const bool guest = ctx.r3.u32 == 1 && pvz::LocalPlayersActive();
  const auto out = ctx.r4.u32;
  if (guest) ctx.r3.u32 = 0;
  __imp__sub_822CA0B0(ctx, base);
  if (guest && out && !ctx.r3.u32) pvz::guest::Write(base, out + 4, pvz::guest::Read(base, out + 4) ^ 1u);
}
