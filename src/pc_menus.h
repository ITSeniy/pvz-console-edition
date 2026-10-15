#pragma once
#include <array>
#include <functional>
#include <string>
#include <cstdint>
#include <rex/ppc/context.h>

namespace pvz {
enum class HostMenu { Achievements, Cheats, Quit, Unlock };
void SetHostMenuCallback(std::function<void(HostMenu)> callback);
void RequestHostMenu(HostMenu menu);
// Outcome of HostMenu::Unlock; the next WidgetManager update applies it.
struct UnlockResult { bool full = false; std::string message; };
void PostUnlockResult(UnlockResult result);
void SetHostMenuOpen(bool open);
bool HostMenuOpen();
bool RouteHostMenuButton(uint32_t button, uint32_t player);
bool RouteHostMenuButtonUp(uint32_t button, uint32_t player);
uint32_t TakeHostMenuButtons();
struct CheatState { bool available = false; std::array<bool, 7> enabled{}, allowed{}; };
CheatState ReadCheatState();
void RequestCheat(size_t index);
void UpdatePcMenus(PPCContext& ctx, uint8_t* base, uint32_t manager);
// Reopens More Ways to Play or Extras when the main menu returns from a
// screen started there. Runs every WidgetManager update, for any input.
void RestoreMenuPanel(PPCContext& ctx, uint8_t* base, uint32_t manager);
}
