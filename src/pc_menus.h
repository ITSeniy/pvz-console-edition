#pragma once
#include <array>
#include <functional>
#include <cstdint>
#include <rex/ppc/context.h>

namespace pvz {
enum class HostMenu { Achievements, Cheats, Quit };
void SetHostMenuCallback(std::function<void(HostMenu)> callback);
void RequestHostMenu(HostMenu menu);
void SetHostMenuOpen(bool open);
bool HostMenuOpen();
bool RouteHostMenuButton(uint32_t button, uint32_t player);
bool RouteHostMenuButtonUp(uint32_t button, uint32_t player);
uint32_t TakeHostMenuButtons();
struct CheatState { bool available = false; std::array<bool, 7> enabled{}, allowed{}; };
CheatState ReadCheatState();
void RequestCheat(size_t index);
void UpdatePcMenus(PPCContext& ctx, uint8_t* base, uint32_t manager);
}
