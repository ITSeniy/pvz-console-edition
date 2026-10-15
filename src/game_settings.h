#pragma once
#include <filesystem>
#include <functional>
#include <rex/ppc/context.h>

namespace pvz {
bool RequestIntroSkip();
// True when the game data came from a checked package import (first launch
// or Import-Game.ps1): the in-game unlock then only confirms ownership.
bool HasImportedPackage();
void ConfigureGameSettings(const std::filesystem::path& config_path,
                           const std::filesystem::path& game_data_root);
void SetHostSettingsCallback(std::function<void()> callback);
void RefreshHostSettings();
void UpdateMouseNavigation(PPCContext& ctx, uint32_t manager, uint8_t* base);
void RemoveMouseNavigation(PPCContext& ctx, uint32_t parent, uint8_t* base);
bool CloseMouseHelp(PPCContext& ctx, uint32_t widget, uint8_t* base);
// All added buttons, before hit testing: a paging screen moves during its
// Update, so a rect from the previous frame toggles hover (and its sound).
void PinNavigationButtons(PPCContext& ctx, uint8_t* base);
// GameButton::Draw (r3 = button): reapplies an added button's screen rect.
void PinNavigationButton(PPCContext& ctx, uint8_t* base);
bool DrawChooserTextButton(PPCContext& ctx, uint8_t* base);
bool DrawGardenNavigationButton(PPCContext& ctx, uint8_t* base);
}
