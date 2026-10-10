#pragma once
#include <filesystem>
#include <functional>
#include <rex/ppc/context.h>

namespace pvz {
bool RequestIntroSkip();
void ConfigureGameSettings(const std::filesystem::path& config_path,
                           const std::filesystem::path& game_data_root);
void SetHostSettingsCallback(std::function<void()> callback);
void RefreshHostSettings();
void UpdateMouseNavigation(PPCContext& ctx, uint32_t manager, uint8_t* base);
void RemoveMouseNavigation(PPCContext& ctx, uint32_t parent, uint8_t* base);
bool CloseMouseHelp(PPCContext& ctx, uint32_t widget, uint8_t* base);
bool DrawChooserTextButton(PPCContext& ctx, uint8_t* base);
bool DrawGardenNavigationButton(PPCContext& ctx, uint8_t* base);
}
