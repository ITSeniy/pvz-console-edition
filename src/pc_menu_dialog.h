#pragma once
#include "pc_menus.h"
#include "mouse_bridge.h"
#include "pc_text_runtime.h"
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
    if (text_.back.empty()) text_ = Texts();
    imgui_drawer()->AddDialog(this);
    pvz::SetHostMenuOpen(true);
    pvz::PublishPointer(std::nullopt);
    pvz::RefreshHostSettings();
  }
 private:
  // The menus' words in the game's language, read once the game has loaded
  // its LawnStrings. ImGui's font has Latin-1 and Japanese.
  struct MenuTexts {
    std::string achievements, cheats, unlocked, codes, tree, back, help;
  };
  static MenuTexts Texts() {
    using pvz::PortText;
    using pvz::Utf8;
    return {
        Utf8(PortText({u"Achievements"}, u"ACHIEVEMENTS")),
        Utf8(PortText({u"PvZ Cheats", u"PvZ-Cheats", u"Trucos de PvZ", u"Codes de PvZ", u"Trucchi di PvZ",
            u"PvZ チート"})),
        Utf8(PortText({u"unlocked", u"freigeschaltet", u"desbloqueados", u"débloqués", u"sbloccati", u"解除"})),
        Utf8(PortText({u"Original PvZ codes. Effects also apply to the next level.",
            u"Originale PvZ-Codes. Die Effekte gelten auch im nächsten Level.",
            u"Códigos originales de PvZ. Los efectos también se aplican al siguiente nivel.",
            u"Codes originaux de PvZ. Les effets s'appliquent aussi au niveau suivant.",
            u"Codici originali di PvZ. Gli effetti valgono anche per il livello successivo.",
            u"PvZ のオリジナルコード。効果は次のステージにも続きます。"})),
        // The European versions measure the Tree of Wisdom in metres.
        Utf8(PortText({u"Tree of Wisdom: %d feet", u"Baum der Weisheit: %d Meter",
            u"Árbol de la sabiduría: %d metros", u"Arbre de sagesse : %d mètres",
            u"Albero della saggezza: %d metri", u"賢者の木:%dメートル"})),
        Utf8(PortText({u"Back"}, u"BACK")),
        Utf8(PortText({u"Esc / B: Back    D-pad: Navigate    A: Toggle",
            u"Esc / B: Zurück    Steuerkreuz: Auswählen    A: Umschalten",
            u"Esc / B: Atrás    Cruceta: Navegar    A: Cambiar",
            u"Échap / B : Retour    Croix : Naviguer    A : Activer",
            u"Esc / B: Indietro    Croce: Naviga    A: Attiva",
            u"Esc / B:戻る    十字ボタン:選択    A:切り替え"}))};
  }
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
    const auto title = (menu_ == pvz::HostMenu::Achievements ? text_.achievements : text_.cheats) + "###pvz_menu";
    if (ImGui::Begin(title.c_str(), &keep, ImGuiWindowFlags_NoCollapse)) {
      if (menu_ == pvz::HostMenu::Achievements) {
        const auto entries = achievements_->ListAchievements();
        unsigned earned = 0, total = 0, unlocked = 0;
        for (const auto& a : entries) {
          total += a.gamerscore;
          if (achievements_->IsUnlocked(a.id)) { earned += a.gamerscore; ++unlocked; }
        }
        ImGui::Text("%u / %zu %s   %uG / %uG", unlocked, entries.size(), text_.unlocked.c_str(), earned, total);
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
        ImGui::TextWrapped("%s", text_.codes.c_str());
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
            ImGui::SameLine(); ImGui::TextDisabled(text_.tree.c_str(), requirements[i]);
          }
          ImGui::PopID(); ImGui::Spacing();
        }
      }
      if (ImGui::Button(text_.back.c_str(), ImVec2(120, 30))) keep = false;
      ImGui::SameLine(); ImGui::TextDisabled("%s", text_.help.c_str());
    }
    ImGui::End();
    if (!keep) Hide();
  }
  rex::system::AchievementManager* achievements_;
  rex::ui::AchievementIconCache icons_;
  pvz::HostMenu menu_ = pvz::HostMenu::Achievements;
  MenuTexts text_;
  bool active_ = false;
  size_t selection_ = 0;
};
