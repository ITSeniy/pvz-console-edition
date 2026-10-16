#pragma once
#include "pc_menus.h"
#include "mouse_bridge.h"
#include "pc_text_runtime.h"
#include "key_bindings.h"
#include <cstdio>
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
    menu_ = menu; active_ = true; selection_ = 0; capturing_ = -1; message_.clear();
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
    std::string keyboard, keys_help, press_key, unbound, reserved, reset, clear;
    std::array<std::string, 4> actions; // Pause, Screenshot, Shovel, "Seed packet %d"
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
            u"Esc / B:戻る    十字ボタン:選択    A:切り替え"})),
        Utf8(PortText({u"Keyboard", u"Tastatur", u"Teclado", u"Clavier", u"Tastiera", u"キーボード"})),
        Utf8(PortText({u"Click a key, then press the new key. Click it again to keep the old one.",
            u"Taste anklicken, dann die neue Taste drücken. Erneut klicken behält die alte.",
            u"Haz clic en una tecla y pulsa la nueva. Otro clic conserva la anterior.",
            u"Cliquez sur une touche, puis appuyez sur la nouvelle. Recliquez pour garder l'ancienne.",
            u"Fai clic su un tasto, poi premi quello nuovo. Un altro clic mantiene il vecchio.",
            u"キーをクリックして、新しいキーを押してください。もう一度クリックで元のまま。"})),
        Utf8(PortText({u"Press a key...", u"Taste drücken...", u"Pulsa una tecla...", u"Appuyez sur une touche...",
            u"Premi un tasto...", u"キーを押してください"})),
        Utf8(PortText({u"(none)", u"(keine)", u"(ninguna)", u"(aucune)", u"(nessuno)", u"(なし)"})),
        Utf8(PortText({u"%s opens an overlay and cannot be used.", u"%s öffnet ein Overlay und ist nicht belegbar.",
            u"%s abre una superposición y no se puede usar.", u"%s ouvre une superposition et ne peut pas servir.",
            u"%s apre un overlay e non si può usare.", u"%sはオーバーレイ用のため使えません。"})),
        Utf8(PortText({u"Reset to defaults", u"Standard wiederherstellen", u"Restablecer", u"Valeurs par défaut",
            u"Ripristina predefiniti", u"初期設定に戻す"})),
        Utf8(PortText({u"Clear", u"Löschen", u"Borrar", u"Effacer", u"Cancella", u"消去"})),
        {Utf8(PortText({u"Pause / Back", u"Pause / Zurück", u"Pausa / Atrás", u"Pause / Retour", u"Pausa / Indietro",
             u"ポーズ / 戻る"})),
         Utf8(PortText({u"Screenshot", u"Screenshot", u"Captura de pantalla", u"Capture d'écran", u"Screenshot",
             u"スクリーンショット"})),
         Utf8(PortText({u"Shovel", u"Schaufel", u"Pala", u"Pelle", u"Pala", u"シャベル"})),
         Utf8(PortText({u"Seed packet %d", u"Samentüte %d", u"Sobre de semillas %d", u"Sachet de graines %d",
             u"Bustina di semi %d", u"タネ袋 %d"}))}};
  }
  // A registered dialog keeps the presenter painting from the UI thread on
  // every refresh, so the closed menu leaves the drawer (see achievement_toast.h).
  void Hide() {
    pvz::CancelKeyCapture(); capturing_ = -1;
    active_ = false; pvz::SetHostMenuOpen(false); pvz::RefreshHostSettings();
    imgui_drawer()->RemoveDialog(this);
  }
  void OnDraw(ImGuiIO& io) override {
    if (!active_) return;
    const auto pad = pvz::TakeHostMenuButtons();
    // Escape can be the key being bound; ImGui sees that press as well.
    if (const auto key = pvz::TakeCapturedKey()) { Bind(*key); escape_guard_ = 3; }
    if (escape_guard_) --escape_guard_;
    const bool escape = ImGui::IsKeyPressed(ImGuiKey_Escape) && !escape_guard_ && capturing_ < 0;
    if ((pad & ((1u << 7) | (1u << 5))) || escape) { Hide(); return; }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(650, 560), ImGuiCond_Appearing);
    bool keep = true;
    const auto title = (menu_ == pvz::HostMenu::Achievements ? text_.achievements :
        menu_ == pvz::HostMenu::Keyboard ? text_.keyboard : text_.cheats) + "###pvz_menu";
    if (ImGui::Begin(title.c_str(), &keep, ImGuiWindowFlags_NoCollapse)) {
      if (menu_ == pvz::HostMenu::Keyboard) DrawKeyboard(pad);
      else if (menu_ == pvz::HostMenu::Achievements) {
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
  // One row per action: its name and its key. A click (or A on the selected
  // row) waits for the next key; a second click keeps the old key.
  void DrawKeyboard(uint32_t pad) {
    constexpr int count = pvz::kKeyActionCount;
    ImGui::TextWrapped("%s", text_.keys_help.c_str());
    if (!message_.empty()) ImGui::TextColored(ImVec4(1.f, .6f, .3f, 1.f), "%s", message_.c_str());
    if (pad & 1u) selection_ = (selection_ + count - 1) % count;
    if (pad & 2u) selection_ = (selection_ + 1) % count;
    ImGui::BeginChild("keys", ImVec2(0, -76));
    for (int i = 0; i < count; ++i) {
      ImGui::PushID(i);
      const auto action = pvz::KeyAction(i);
      char name[96];
      if (action >= pvz::KeyAction::Seed1)
        std::snprintf(name, sizeof(name), text_.actions[3].c_str(), i - int(pvz::KeyAction::Seed1) + 1);
      else std::snprintf(name, sizeof(name), "%s", text_.actions[size_t(i)].c_str());
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(name);
      ImGui::SameLine(300);
      const auto bound = pvz::KeyBinding(action);
      const auto label = (capturing_ == i ? text_.press_key : bound.empty() ? text_.unbound : bound);
      if (ImGui::Button(label.c_str(), ImVec2(200, 0)) || (selection_ == size_t(i) && (pad & (1u << 6)))) {
        message_.clear();
        if (capturing_ == i) { pvz::CancelKeyCapture(); capturing_ = -1; }
        else { capturing_ = i; pvz::BeginKeyCapture(); }
      }
      ImGui::SameLine();
      ImGui::BeginDisabled(bound.empty());
      if (ImGui::SmallButton(text_.clear.c_str())) pvz::BindKey(action, "");
      ImGui::EndDisabled();
      if (selection_ == size_t(i)) { ImGui::SameLine(); ImGui::TextUnformatted("<"); }
      ImGui::PopID();
    }
    ImGui::EndChild();
    if (ImGui::Button(text_.reset.c_str(), ImVec2(240, 30))) {
      pvz::CancelKeyCapture(); capturing_ = -1; message_.clear();
      pvz::ResetKeyBindings();
    }
  }
  void Bind(rex::ui::VirtualKey key) {
    const int action = std::exchange(capturing_, -1);
    if (action < 0 || menu_ != pvz::HostMenu::Keyboard) return;
    const auto name = pvz::KeyName(key);
    if (name.empty()) return;
    if (pvz::ReservedKey(key)) {
      char text[160];
      std::snprintf(text, sizeof(text), text_.reserved.c_str(), name.c_str());
      message_ = text;
      return;
    }
    pvz::BindKey(pvz::KeyAction(action), name);
  }
  rex::system::AchievementManager* achievements_;
  rex::ui::AchievementIconCache icons_;
  int capturing_ = -1;
  int escape_guard_ = 0;
  std::string message_;
  pvz::HostMenu menu_ = pvz::HostMenu::Achievements;
  MenuTexts text_;
  bool active_ = false;
  size_t selection_ = 0;
};
