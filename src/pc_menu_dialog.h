#pragma once
#include "pc_menus.h"
#include "mouse_bridge.h"
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/overlay/achievement_icon_cache.h>
#include <rex/system/achievement_manager.h>
#include <imgui.h>

// One UI-thread-owned dialog. Guest code exchanges only value snapshots and
// commands; it never touches ImGui or the achievement manager.
class PcMenuDialog final : public rex::ui::ImGuiDialog {
 public:
  PcMenuDialog(rex::ui::ImGuiDrawer* drawer, rex::ui::ImmediateDrawer* immediate,
      rex::Runtime* runtime, rex::system::AchievementManager* achievements)
      : ImGuiDialog(drawer), achievements_(achievements), icons_(immediate, runtime) {}
  ~PcMenuDialog() override { pvz::SetHostMenuOpen(false); }
  void Show(pvz::HostMenu menu) {
    menu_ = menu; active_ = true; selection_ = 0;
    imgui_drawer()->AddDialog(this);
    pvz::SetHostMenuOpen(true);
    pvz::PublishPointer(std::nullopt);
    pvz::RefreshHostSettings();
  }
 private:
  // A registered dialog keeps the presenter painting from the UI thread on
  // every refresh, so the closed menu leaves the drawer (see achievement_toast.h).
  void Hide() {
    active_ = false; pvz::SetHostMenuOpen(false); pvz::RefreshHostSettings();
    imgui_drawer()->RemoveDialog(this);
  }
  void OnDraw(ImGuiIO& io) override {
    if (!active_) return;
    const auto pad = pvz::TakeHostMenuButtons();
    if ((pad & ((1u << 7) | (1u << 5))) || ImGui::IsKeyPressed(ImGuiKey_Escape)) { Hide(); return; }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(650, 560), ImGuiCond_Appearing);
    bool keep = true;
    const char* title = menu_ == pvz::HostMenu::Achievements ? "Achievements" : "PvZ Cheats";
    if (ImGui::Begin(title, &keep, ImGuiWindowFlags_NoCollapse)) {
      if (menu_ == pvz::HostMenu::Achievements) {
        const auto entries = achievements_->ListAchievements();
        unsigned earned = 0, total = 0, unlocked = 0;
        for (const auto& a : entries) {
          total += a.gamerscore;
          if (achievements_->IsUnlocked(a.id)) { earned += a.gamerscore; ++unlocked; }
        }
        ImGui::Text("%u / %zu unlocked   %uG / %uG", unlocked, entries.size(), earned, total);
        ImGui::ProgressBar(entries.empty() ? 0.f : float(unlocked) / float(entries.size()));
        ImGui::BeginChild("achievements", ImVec2(0, -40));
        if (pad & 1u) ImGui::SetScrollY(ImGui::GetScrollY() - 70);
        if (pad & 2u) ImGui::SetScrollY(ImGui::GetScrollY() + 70);
        for (const auto& a : entries) {
          const bool done = achievements_->IsUnlocked(a.id);
          if (auto* icon = icons_.GetIcon(a)) {
            ImGui::Image(reinterpret_cast<ImTextureID>(icon), ImVec2(48, 48)); ImGui::SameLine();
          }
          ImGui::BeginGroup();
          ImGui::Text("%s %s (%uG)", done ? "[*]" : "[ ]", a.label.c_str(), a.gamerscore);
          ImGui::TextWrapped("%s", (done ? a.description : a.unachieved_description).c_str());
          ImGui::EndGroup(); ImGui::Separator();
        }
        ImGui::EndChild();
      } else {
        constexpr std::array names{"mustache", "trickedout", "future", "pinata", "dance", "daisies", "sukhbir"};
        constexpr std::array requirements{0, 0, 0, 1000, 500, 100, 0};
        const auto state = pvz::ReadCheatState();
        ImGui::TextWrapped("Original PvZ codes. Effects also apply to the next level.");
        if (pad & 1u) selection_ = (selection_ + 6) % 7;
        if (pad & 2u) selection_ = (selection_ + 1) % 7;
        for (size_t i = 0; i < names.size(); ++i) {
          ImGui::PushID(int(i));
          ImGui::BeginDisabled(!state.available || !state.allowed[i]);
          bool value = state.enabled[i];
          if (ImGui::Checkbox(names[i], &value) ||
              (selection_ == i && (pad & (1u << 6)) && state.available && state.allowed[i])) pvz::RequestCheat(i);
          ImGui::EndDisabled();
          if (selection_ == i) { ImGui::SameLine(); ImGui::TextUnformatted("<"); }
          if (state.available && !state.allowed[i]) {
            ImGui::SameLine(); ImGui::TextDisabled("Tree of Wisdom: %d feet", requirements[i]);
          }
          ImGui::PopID(); ImGui::Spacing();
        }
      }
      if (ImGui::Button("Back", ImVec2(120, 30))) keep = false;
      ImGui::SameLine(); ImGui::TextDisabled("Esc / B: Back    D-pad: Navigate    A: Toggle");
    }
    ImGui::End();
    if (!keep) Hide();
  }
  rex::system::AchievementManager* achievements_;
  rex::ui::AchievementIconCache icons_;
  pvz::HostMenu menu_ = pvz::HostMenu::Achievements;
  bool active_ = false;
  size_t selection_ = 0;
};
