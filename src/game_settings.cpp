#include "game_settings.h"
#include "game_edition.h"
#include "mouse_bridge.h"
#include "input_backend.h"
#include "pc_menus.h"
#include "bonus_games.h"
#include "widget_tree.h"
#include "guest_layout.h"
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <unordered_map>
#include <string_view>
#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <mutex>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>

REXCVAR_DEFINE_BOOL(pvz_vsync, true, "PvZ", "Synchronize host presentation without guest GPU wait throttling");
REXCVAR_DEFINE_INT32(pvz_frame_limit, 60, "PvZ", "Frames per second without VSync; with VSync the display refresh paces frames (simulation keeps its native clock)").range(30, 240);
REXCVAR_DEFINE_BOOL(pvz_skip_intro, false, "PvZ", "Skip startup intro videos (restart to replay)");
REXCVAR_DEFINE_BOOL(pvz_full_version, false, "PvZ", "Full edition status from validated own-copy import");
REXCVAR_DEFINE_STRING(pvz_unlock_package, "", "Diagnostics", "Replay-only: XBLA package for the in-game unlock instead of the file dialog");
REXCVAR_DEFINE_STRING(pvz_import_answer, "", "Diagnostics", "Replay-only: answer the first-run import unattended (full or trial)");
REXCVAR_DEFINE_BOOL(pvz_test_unlock, false, "Diagnostics", "Enable Full access for test saves without an import receipt (restart required)");
REXCVAR_DEFINE_BOOL(pvz_pause_on_focus_loss, true, "PvZ", "Pause gameplay when the game window loses focus");
REXCVAR_DEFINE_STRING(pvz_display_mode, "windowed", "PvZ", "Display mode")
    .allowed({"windowed", "fullscreen", "borderless"});
REXCVAR_DEFINE_STRING(pvz_language, "en", "PvZ", "Game language: en, de, es, fr, it or jp (restart required)")
    .allowed({"en", "de", "es", "fr", "it", "jp"});

REX_EXTERN(sub_82188F80); // MakeDialogButton(id, listener, parent, label)
REX_EXTERN(sub_82188EE0); // DialogButton::SetLabel
REX_EXTERN(sub_82188E20); // DialogButton::Resize (painted bounds)
REX_EXTERN(sub_821258D8); // UTF-16 string constructor
REX_EXTERN(sub_821256C0); // UTF-16 string destructor
REX_EXTERN(sub_82493E10); // WidgetContainer::AddWidget
REX_EXTERN(sub_824930D0); // WidgetContainer::RemoveWidget
REX_EXTERN(sub_82187918); // DialogButton deleting destructor
REX_EXTERN(__imp__sub_82198850);
REX_EXTERN(__imp__sub_821988D0);
REX_EXTERN(__imp__sub_82198938);
REX_EXTERN(__imp__sub_82198720);
REX_EXTERN(__imp__sub_82198B08);
REX_EXTERN(__imp__sub_82198B30);
REX_EXTERN(__imp__sub_821AFCA0);
REX_EXTERN(__imp__sub_821993F8);
REX_EXTERN(__imp__sub_8219B8A0);
REX_EXTERN(__imp__sub_82208950);
REX_EXTERN(__imp__sub_8220A0F8);
REX_EXTERN(sub_8214CDC0);
REX_EXTERN(sub_8214D1B8);
REX_EXTERN(sub_8219A210);
REX_EXTERN(sub_8219D480); // HouseChooserScreen::GamepadButtonDown
REX_EXTERN(__imp__sub_8219B580); // HouseChooserScreen deleting destructor
REX_EXTERN(sub_821B9618); // LeaderboardMenu::GamepadButtonDown
REX_EXTERN(__imp__sub_821B7F80); // LeaderboardMenu deleting destructor
REX_EXTERN(__imp__sub_824AE488);
REX_EXTERN(__imp__sub_8212D448);
REX_EXTERN(__imp__sub_8219A2F0);
REX_EXTERN(__imp__sub_8219A070);
REX_EXTERN(sub_825DDB78); // guest operator new
REX_EXTERN(sub_825F8F50); // guest operator delete
REX_EXTERN(__imp__sub_82187F70); // GameButton destructor
REX_EXTERN(sub_8220AE58); // SeedChooser button listener
REX_EXTERN(sub_821ABDE8); // LawnApp::PlaySample(sound, ...)
REX_EXTERN(sub_821A3B80); // LawnApp::DoPauseDialog(user)
REX_EXTERN(sub_821A9608); // Almanac availability
REX_EXTERN(sub_821A9678); // Shop availability
REX_EXTERN(sub_82292DA8); // ZenGarden::OpenStore (save, modal, restore garden)
REX_EXTERN(sub_82217888); // Store::GamepadButtonDown
REX_EXTERN(sub_8228B5C0); // VSSetupMenu::GamepadButtonDown
REX_EXTERN(sub_82491DE8); // XML WidgetContainer::GetWidget(id)
REX_EXTERN(__imp__sub_82473000); // Widget::GamepadButtonDown
REX_HOOK_RAW(sub_82473000) {
  if (pvz::LocalPlayersActive() && ctx.r3.u32 &&
      pvz::guest::Read(base, ctx.r3.u32) == 0x820CC2EC &&
      pvz::guest::Read(base, ctx.r3.u32 + 0x114) == 1) {
    sub_8228B5C0(ctx, base);
    return;
  }
  __imp__sub_82473000(ctx, base);
}
REX_EXTERN(__imp__sub_82188310);
REX_EXTERN(sub_82191B10);
REX_EXTERN(sub_8214C9F8); // Board::ToolVisible
REX_EXTERN(sub_82139808); // Board::PickUpTool
REX_EXTERN(sub_821388A0); // Board::MouseDownWithTool
REX_EXTERN(__imp__sub_821BE0B0);
REX_EXTERN(__imp__sub_82172788);
REX_EXTERN(__imp__sub_8228A4B0);
REX_EXTERN(sub_82289F50); // VSResults button listener; XML QUIT_BUTTON id 1
REX_EXTERN(sub_824A5F48); // WidgetManager::SetGamepadFocus
REX_EXTERN(sub_8249B508); // Graphics::SetScale(x,y,centreX,centreY)

namespace {
constexpr uint32_t kHelpOptions = 0x820BAA54;
constexpr uint32_t kMouseMode = 1002;
// Rows after Mouse use 1003..1006; 1007 is Cheats.
constexpr uint32_t kLanguageRow = 1008;
uint32_t ExtraRowId(size_t i) { return i < 4 ? kMouseMode + uint32_t(i) + 1 : kLanguageRow; }
// XConfig USER_LANGUAGE ids. The game reads the setting once at startup
// (826163F0, stored at LawnApp +804) and mounts <code>.pak by it (8219F528).
struct Language { std::string_view code; uint32_t id; std::u16string_view name; };
constexpr std::array kLanguages{Language{"en", 1, u"English"}, Language{"de", 3, u"German"},
    Language{"es", 5, u"Spanish"}, Language{"fr", 4, u"French"}, Language{"it", 6, u"Italian"},
    Language{"jp", 2, u"Japanese"}};
size_t LanguageIndex(std::string_view code) {
  for (size_t i = 0; i < kLanguages.size(); ++i) if (kLanguages[i].code == code) return i;
  return 0;
}
// The language this launch runs in; a new choice applies on the next launch.
std::string running_language = "en";
std::filesystem::path language_root;
bool LanguageAvailable(std::string_view code) {
  return code == "en" || std::filesystem::exists(language_root / (std::string(code) + ".pak"));
}
// VS Menu, board coordinates: the window's free corner right of the zombie
// bank (frame x 1100..1220), shared by the VS lawn and both choosers.
constexpr int32_t kVersusMenuLeft = 860, kVersusMenuTop = -46;
// LET'S ROCK! on the zombie chooser, relative to its panel.
constexpr int32_t kZombieRockLeft = 53;
struct SettingButton {
  uint32_t widget;
  std::string mode;
  bool page = false;
  std::array<uint32_t, 5> extra{};
  std::array<uint8_t, 4> visible{};
  std::array<std::array<uint32_t, 4>, 5> navigation{};
  uint32_t cheats = 0;
  uint32_t return_focus = 0;
};
// Every widget we add to the dialog: the page button, its rows and Cheats.
std::array<uint32_t, 7> AddedWidgets(const SettingButton& setting) {
  return {setting.widget, setting.extra[0], setting.extra[1], setting.extra[2], setting.extra[3],
      setting.extra[4], setting.cheats};
}
std::unordered_map<uint32_t, SettingButton> buttons;
std::unordered_map<uint32_t, std::array<uint32_t, 4>> chooser_buttons;
// VS setup whose mode stones are hidden during Pick Sides.
uint32_t vs_modes_hidden = 0;
bool imported_package = false;
struct NavigationButton { uint32_t widget, listener; };
std::unordered_map<uint32_t, NavigationButton> navigation_buttons;
std::unordered_map<uint32_t, NavigationButton> endless_buttons;
std::unordered_map<uint32_t, NavigationButton> garden_shop_buttons, shop_prev_buttons, shop_next_buttons;
std::unordered_map<uint32_t, NavigationButton> onslaught_buttons;
std::unordered_map<uint32_t, NavigationButton> service_prev_buttons, service_next_buttons, service_ready_buttons;
// Mail's Inbox/Read Messages switch: the controller's X (native action 8).
std::unordered_map<uint32_t, NavigationButton> mail_read_buttons, mail_inbox_buttons;
// Mini-Games -> Bonus Games (ChallengeScreen page 2), after all trophies.
std::unordered_map<uint32_t, NavigationButton> bonus_buttons;
// Credits: replay once the video ended (native X, action 8; state +DC 3).
std::unordered_map<uint32_t, NavigationButton> credits_replay_buttons;
// Screen rect of each added button. Paged screens slide their own position
// during their Update, after WidgetManager's; reapplying the rect relative
// to the parent just before drawing keeps the buttons still.
struct PinnedRect { int32_t x, y; uint32_t width, height; };
std::unordered_map<uint32_t, PinnedRect> pinned_buttons;
std::unordered_map<uint32_t, uint32_t> navigation_listeners;
std::filesystem::path settings_path;
std::mutex settings_mutex, callback_mutex;
std::function<void()> host_callback;

bool SavePreference(std::string_view key, std::string_view value) {
  std::lock_guard lock(settings_mutex);
  if (settings_path.empty()) return false;
  try {
    // Save only this preference. Saving all cvars here would also persist
    // temporary replay/capture arguments and paths from the current launch.
    std::ifstream input(settings_path);
    if (!input && std::filesystem::exists(settings_path))
      throw std::runtime_error("cannot read settings");
    std::string line;
    std::ostringstream output;
    output << key << " = " << value << '\n';
    bool root = true;
    while (std::getline(input, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      const auto start = line.find_first_not_of(" \t");
      const auto text = start == std::string::npos ? std::string_view{} : std::string_view(line).substr(start);
      if (text.starts_with("[")) root = false;
      if (root && text.starts_with(key)) {
        const auto tail = text.substr(key.size());
        const auto eq = tail.find_first_not_of(" \t");
        if (eq != std::string_view::npos && tail[eq] == '=') continue;
      }
      output << line << '\n';
    }
    if (input.bad()) throw std::runtime_error("cannot read settings");
    input.close(); // Windows cannot replace a file still open by this stream.
    auto temp = settings_path;
    temp += ".mouse.tmp";
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    file << output.str();
    file.close();
    if (!file) throw std::runtime_error("cannot write settings");
    std::filesystem::rename(temp, settings_path);
    return true;
  } catch (const std::exception& error) {
    REXLOG_ERROR("PvZ could not save {}: {}", key, error.what());
    return false;
  }
}

uint32_t Read(uint8_t* base, uint32_t at) {
  return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + at));
}
void Write(uint8_t* base, uint32_t at, uint32_t value) {
  *reinterpret_cast<uint32_t*>(base + at) = __builtin_bswap32(value);
}
uint32_t Call(PPCContext& ctx, uint8_t* base, PPCFunc* fn,
    std::initializer_list<uint32_t> args) {
  PPCContext work = ctx;
  work.r1.u32 -= 0x500;
  size_t i = 0;
  // PPC integer argument registers are r3-r10.
  // PPCContext places r0/r1/r2 between r3 and r4 in host memory.
  const std::array registers{&work.r3, &work.r4, &work.r5, &work.r6,
      &work.r7, &work.r8, &work.r9, &work.r10};
  for (auto arg : args) registers[i++]->u64 = arg;
  fn(work, base);
  return work.r3.u32;
}
void Pin(PPCContext& ctx, uint8_t* base, uint32_t button, uint32_t parent, PinnedRect rect) {
  pinned_buttons[button] = rect;
  const auto x = uint32_t(rect.x - int32_t(Read(base, parent + 0x40)));
  const auto y = uint32_t(rect.y - int32_t(Read(base, parent + 0x44)));
  if (Read(base, button + 0x190) != x || Read(base, button + 0x194) != y ||
      Read(base, button + 0x1A4) != rect.width || Read(base, button + 0x1A8) != rect.height)
    Call(ctx, base, sub_82188E20, {button, x, y, rect.width, rect.height});
}

// Strings live in the hook's reserved caller area, above nested guest frames.
// Every constructed guest string is destroyed by the native implementation.
template<class Fn>
uint32_t WithLabel(PPCContext& ctx, uint8_t* base, std::u16string_view text, Fn fn) {
  const auto chars = ctx.r1.u32 - 0x180;
  const auto string = ctx.r1.u32 - 0x80;
  for (size_t i = 0; i <= text.size(); ++i) {
    const auto code = i < text.size() ? text[i] : char16_t(0);
    base[chars + i * 2] = uint8_t(code >> 8);
    base[chars + i * 2 + 1] = uint8_t(code);
  }
  Write(base, string + 0x14, 0);
  Write(base, string + 0x18, 7);
  base[string + 4] = base[string + 5] = 0;
  Call(ctx, base, sub_821258D8, {string, chars, uint32_t(text.size())});
  const auto result = fn(string);
  Call(ctx, base, sub_821256C0, {string});
  return result;
}

void UpdateLabel(PPCContext& ctx, uint8_t* base, SettingButton& button) {
  const auto mode = rex::cvar::Query<std::string>("pvz_cursor_mode");
  const bool vsync = rex::cvar::Query<bool>("pvz_vsync");
  const bool pause = REXCVAR_GET(pvz_pause_on_focus_loss);
  const auto display = REXCVAR_GET(pvz_display_mode);
  const auto language = REXCVAR_GET(pvz_language);
  const auto signature = mode + display + language + std::to_string(vsync) + std::to_string(pause) + std::to_string(button.page) + std::to_string(REXCVAR_GET(pvz_skip_intro));
  if (button.mode == signature) return;
  // A language other than the running one is mounted on the next launch.
  const auto language_label = std::u16string(language == running_language ? u"Language: " : u"Next launch: ") +
      std::u16string(kLanguages[LanguageIndex(language)].name);
  const std::array<std::u16string_view, 6> labels{
      button.page ? (mode == "console" ? u"Mouse: Console" : u"Mouse: PC") : u"PC Settings",
      vsync ? u"VSync: On" : u"VSync: Off",
      pause ? u"Pause if unfocused: On" : u"Pause if unfocused: Off",
      display == "windowed" ? u"Display: Windowed" : display == "fullscreen" ? u"Display: Fullscreen" : u"Display: Borderless",
      REXCVAR_GET(pvz_skip_intro) ? u"Skip intro: On" : u"Skip intro: Off",
      language_label};
  for (size_t i = 0; i < labels.size(); ++i) {
    const auto widget = i ? button.extra[i - 1] : button.widget;
    if (widget) WithLabel(ctx, base, labels[i], [&](uint32_t label) {
      return Call(ctx, base, sub_82188EE0, {widget, label});
    });
  }
  button.mode = signature;
}
void Layout(PPCContext& ctx, uint8_t* base, uint32_t dialog, uint32_t button) {
  auto& setting = buttons.at(dialog);
  // Each page starts from the native graph, including Back's wrap to Help.
  // Otherwise the PC page's last Down link survives when it is closed.
  for (size_t i = 0; i < setting.navigation.size(); ++i) {
    const auto original = Read(base, dialog + 0x1C4 + uint32_t(i * 4));
    for (size_t j = 0; j < 4; ++j)
      Write(base, original + 0xB0 + uint32_t(j * 4), setting.navigation[i][j]);
  }
  const auto credits = Read(base, dialog + 0x1D0);
  const auto back = Read(base, dialog + 0x1D4);
  // The PC page has seven rows above Back. Keep Back at its native position
  // inside the dialog frame; the rows are shorter to fit above it.
  constexpr uint32_t kPageTop = 114, kPageStep = 36, kPageRow = 34;
  const auto x = Read(base, back + 0x190), width = Read(base, back + 0x1A4);
  Call(ctx, base, sub_82188E20, {back, x, 371, width, 46});
  base[setting.cheats + 0x70] = setting.page;
  base[setting.cheats + 0x72] = !setting.page;
  Call(ctx, base, sub_82188E20, {setting.cheats, x, kPageTop + uint32_t(setting.extra.size() + 1) * kPageStep, width, kPageRow});
  if (setting.page) Call(ctx, base, sub_82188E20, {button, x, kPageTop, width, kPageRow});
  else Call(ctx, base, sub_82188E20, {button, x, 306, width, 46});
  for (size_t i = 0; i < setting.extra.size(); ++i) {
    const auto widget = setting.extra[i];
    base[widget + 0x70] = setting.page;
    base[widget + 0x72] = !setting.page;
    Call(ctx, base, sub_82188E20, {widget, x, kPageTop + uint32_t(i + 1) * kPageStep, width, kPageRow});
  }
  for (size_t i = 0; i < setting.visible.size(); ++i) {
    const auto original = Read(base, dialog + 0x1C4 + uint32_t(i * 4));
    base[original + 0x70] = setting.page ? 0 : setting.visible[i];
  }
  if (setting.page) {
    const std::array row{button, setting.extra[0], setting.extra[1], setting.extra[2], setting.extra[3],
        setting.extra[4], setting.cheats, back};
    for (size_t i = 0; i < row.size(); ++i) {
      Write(base, row[i] + 0xB0, row[(i + row.size() - 1) % row.size()]);
      Write(base, row[i] + 0xB4, row[(i + 1) % row.size()]);
    }
    return;
  }
  // Up/down navigation inserts the extra row before Back. In-game Options
  // omits Credits, so Settings is the preceding row there.
  const auto app = Read(base, dialog + 0x1A4);
  const auto previous = app && Read(base, app + 0x9D0) == 1 ? credits : Read(base, dialog + 0x1CC);
  Write(base, previous + 0xB4, button);
  Write(base, button + 0xB0, previous);
  Write(base, button + 0xB4, back);
  Write(base, back + 0xB0, button);
}
void RemoveNavigation(PPCContext& ctx, uint8_t* base, uint32_t parent) {
  for (auto* collection : {&navigation_buttons, &endless_buttons, &garden_shop_buttons, &onslaught_buttons, &shop_prev_buttons, &shop_next_buttons,
      &service_prev_buttons, &service_next_buttons, &service_ready_buttons, &mail_read_buttons, &mail_inbox_buttons,
      &bonus_buttons, &credits_replay_buttons}) {
    const auto found = collection->find(parent);
    if (found == collection->end()) continue;
    const auto button = found->second.widget;
    collection->erase(found);
    // XML screens can dispose their child widgets before the screen's
    // deleting destructor reaches this hook. Their container head is then
    // zero: discard the attachment instead of destroying that child twice.
    if (!Read(base, button + 0x18)) continue;
    if (Read(base, button + 0x24) == parent) Call(ctx, base, sub_824930D0, {parent, button});
    Call(ctx, base, sub_82187918, {button, 1}); // destructor releases its own listener
  }
}
}

void pvz::RemoveMouseNavigation(PPCContext& ctx, uint32_t parent, uint8_t* base) {
  RemoveNavigation(ctx, base, parent);
}
void pvz::UpdateMouseNavigation(PPCContext& ctx, uint32_t manager, uint8_t* base) {
  const auto head = Read(base, manager + 0x18);
  if (!head) return;
  uint32_t foreground = 0;
  for (auto node = Read(base, head), count = 0u; node && node != head && count++ < 64; node = Read(base, node)) {
    const auto widget = Read(base, node + 8);
    if (widget && base[widget + 0x70] && Read(base, widget) != 0x820BA804) foreground = widget;
  }
  for (auto node = Read(base, head), count = 0u; node && node != head && count++ < 64; node = Read(base, node)) {
    const auto parent = Read(base, node + 8);
    const auto vt = parent ? Read(base, parent) : 0;
    const bool board = vt == 0x820B728C, help = vt == 0x820BB22C || vt == 0x820C1D24 || vt == 0x820BBF94 ||
        vt == 0x820CB23C || vt == 0x820B466C || vt == 0x820C21A4 || vt == 0x820B96EC || vt == 0x820CC2EC || vt == 0x820B899C;
    if (!board && !help) continue;
    // Board +56A0 counts down the level-complete fade after the award is
    // taken (-1 otherwise); PC disables its own Menu then. Hide ours.
    // Local multiplayer: menus (Menu, Back) belong to player 1's device.
    const bool dominant = !pvz::LocalPlayersActive() || pvz::MouseUserIndex() == 0;
    const bool visible = (parent == foreground || vt == 0x820CB23C || board) && base[parent + 0x70] && rex::cvar::Query<bool>("pvz_mouse_cursor") && MouseOwnsInput() && dominant &&
        (help || (Read(base, Read(base, parent + 0xF4) + 0x9D0) == 3 && int32_t(Read(base, parent + 0x56A0)) < 0));
    auto found = navigation_buttons.find(parent);
    if (found == navigation_buttons.end()) {
      if (!visible) continue;
      // Native MouseEnter calls the listener even when there is no action.
      // Use Help's verified no-op ButtonListener vtable; dispatch release below.
      const auto listener = Call(ctx, base, sub_825DDB78, {4});
      Write(base, listener, 0x820BB39C);
      // Credits: Back leaves the music video, so it reads Skip.
      const auto button = WithLabel(ctx, base, board ? u"Menu" : vt == 0x820B96EC ? u"Skip" : u"Back", [&](uint32_t label) {
        return Call(ctx, base, sub_82188F80, {board ? 1100u : 1101u, listener, parent, label});
      });
      found = navigation_buttons.emplace(parent, NavigationButton{button, listener}).first;
      navigation_listeners.emplace(button, listener);
      Call(ctx, base, sub_82493E10, {parent, button});
      REXLOG_INFO("PvZ mouse navigation added: {}", board ? "Menu" : "Back");
    }
    const auto button = found->second.widget;
    if (vt == 0x820CB23C && rex::cvar::Query<bool>("pvz_input_trace") &&
        (bool(base[button + 0x70]) != visible || bool(base[button + 0x72]) != (!visible || parent != foreground)))
      REXLOG_INFO("PvZ Shop mouse controls visible {} enabled {}", visible, visible && parent == foreground);
    base[button + 0x70] = visible; base[button + 0x72] = !visible || parent != foreground || (board && base[parent + 0x1D9]);
    // Help pages slide their parent horizontally. Keep Back fixed in the
    // viewport while its parent and native controller navigation animate.
    const auto app = board ? Read(base, parent + 0xF4) : 0;
    const bool zen = app && (Read(base, app + 0x9CC) == 44 || Read(base, app + 0x9CC) == 51);
    // Zen keeps PC's top-right Menu with Shop below it, moved past the Xbox
    // toolbar's extra garden icons into the window's free corner.
    // PC's Menu sits at y -10, clear of the zombies walking in the top row.
    // VS has a bank on each side of the top row; Menu takes the free corner.
    // Co-op's second bank reaches the same corner as VS's zombie bank.
    const bool versus = app && (Read(base, app + 0x9CC) == pvz::layout::kModeVersus ||
        pvz::layout::CoopMode(Read(base, app + 0x9CC)));
    Pin(ctx, base, button, parent, {zen ? 860 : versus ? kVersusMenuLeft : board ? 680 : 650,
        zen ? -46 : versus ? kVersusMenuTop : board ? -10 : vt == 0x820CC2EC ? 585 : vt == 0x820B96EC ? -36 : 548, 120, 46});
    auto service_button = [&](auto& collection, uint32_t id, std::u16string_view label, bool show,
        int left, int top, int width) {
      auto entry = collection.find(parent);
      if (entry == collection.end() && show) {
        const auto listener = Call(ctx, base, sub_825DDB78, {4});
        Write(base, listener, 0x820BB39C);
        const auto item = WithLabel(ctx, base, label, [&](uint32_t text) {
          return Call(ctx, base, sub_82188F80, {id, listener, parent, text});
        });
        entry = collection.emplace(parent, NavigationButton{item, listener}).first;
        navigation_listeners.emplace(item, listener); Call(ctx, base, sub_82493E10, {parent, item});
      }
      if (entry == collection.end()) return;
      const auto item = entry->second.widget;
      // Both signed-in players use the shared VS side/Ready controls. The
      // GameButton constructor otherwise restricts presses to user zero.
      if (vt == 0x820CC2EC) Write(base, item + 0xC0, uint32_t(-1));
      base[item + 0x70] = show; base[item + 0x72] = !show || parent != foreground;
      Pin(ctx, base, item, parent, {left, top, uint32_t(width), 46});
    };
    if (zen || garden_shop_buttons.contains(parent))
      service_button(garden_shop_buttons, 1103, u"Shop", visible && zen && Call(ctx, base, sub_821A9678, {app}), 860, 6, 120);
    const bool last_stand = app && Read(base, app + 0x9CC) == 32;
    if (last_stand || onslaught_buttons.contains(parent)) {
      const auto challenge = last_stand ? Read(base, parent + 0x1D4) : 0;
      service_button(onslaught_buttons, 1107, u"Start Onslaught", visible && last_stand &&
          !base[parent + 0x1D9] && challenge && Read(base, challenge + 0x74) == 0, 285, 548, 230);
    }
    if (vt == 0x820CB23C) {
      service_button(shop_prev_buttons, 1104, u"<", visible, 170, 375, 110);
      service_button(shop_next_buttons, 1105, u">", visible, 590, 375, 110);
    }
    const bool pages = vt == 0x820BB22C || vt == 0x820BBF94 || vt == 0x820C21A4 || vt == 0x820C1D24;
    if (pages || service_prev_buttons.contains(parent)) {
      service_button(service_prev_buttons, 1110, u"<", visible && pages, 360, 548, 80);
      service_button(service_next_buttons, 1111, u">", visible && pages, 450, 548, 80);
    }
    if (vt == 0x820CC2EC) {
      // Pick Sides (+114 = 1) is clicked directly (service_mouse.cpp): only
      // Back remains. The mode stones (XML ids 9..11) belong to the next
      // step; hide them until it starts.
      const bool pick = Read(base, parent + 0x114) == 1;
      if (pick || vs_modes_hidden == parent) {
        for (uint32_t id : {9u, 10u, 11u})
          if (const auto mode = Call(ctx, base, sub_82491DE8, {parent, id})) base[mode + 0x70] = !pick;
        vs_modes_hidden = pick ? parent : 0;
      }
    }
    if (vt == 0x820B96EC)
      service_button(credits_replay_buttons, 1116, u"Replay", visible && Read(base, parent + 0xDC) == 3 &&
          !base[parent + 0x148], 520, -36, 120);
    if (vt == 0x820B899C && (Read(base, parent + 0x258) == pvz::kMiniGamesPage || bonus_buttons.contains(parent)))
      service_button(bonus_buttons, 1115, u"Bonus Games", visible && Read(base, parent + 0x258) == pvz::kMiniGamesPage &&
          pvz::BonusGamesUnlocked(ctx, base, parent), 40, 548, 240);
    if (vt == 0x820C21A4) {
      // MailScreen +1CC: 0 Inbox, 1 Read Messages.
      const bool read = Read(base, parent + 0x1CC) == 1;
      service_button(mail_read_buttons, 1113, u"Read Mail", visible && !read, 40, 548, 240);
      service_button(mail_inbox_buttons, 1114, u"Inbox", visible && read, 40, 548, 240);
    }
    if (vt == 0x820BBF94) {
      auto endless = endless_buttons.find(parent);
      if (endless == endless_buttons.end() && visible) {
        const auto listener = Call(ctx, base, sub_825DDB78, {4});
        Write(base, listener, 0x820BB39C);
        const auto item = WithLabel(ctx, base, u"Survival Endless", [&](uint32_t label) {
          return Call(ctx, base, sub_82188F80, {1102u, listener, parent, label});
        });
        endless = endless_buttons.emplace(parent, NavigationButton{item, listener}).first;
        navigation_listeners.emplace(item, listener); Call(ctx, base, sub_82493E10, {parent, item});
      }
      if (endless != endless_buttons.end()) {
        const auto item = endless->second.widget;
        base[item + 0x70] = visible; base[item + 0x72] = !visible;
        Pin(ctx, base, item, parent, {40, 548, 240, 46});
      }
    }
  }
}
bool pvz::CloseMouseHelp(PPCContext& ctx, uint32_t widget, uint8_t* base) {
  if (widget && Read(base, widget) == 0x820CC0AC) {
    // Native listener queues exit animation before returning to MainMenu.
    Call(ctx, base, sub_82289F50, {widget + 0xD4, 1});
    if (rex::cvar::Query<bool>("pvz_input_trace")) REXLOG_INFO("PvZ Versus results back");
    return true;
  }
  if (widget && (Read(base, widget) == 0x820C21A4 || Read(base, widget) == 0x820B96EC || Read(base, widget) == 0x820CC2EC || Read(base, widget) == 0x820B899C)) {
    const auto vt = Read(base, widget);
    const auto user = vt == 0x820CC2EC ? Read(base, widget + 0x118) : vt == 0x820B899C ? Read(base, widget + 0xC0) : 0;
    Call(ctx, base, vt == 0x820CC2EC ? sub_8228B5C0 :
        rex::runtime::ResolveIndirectFunction(Read(base, vt + 0xC0)), {widget, 7, user, 0});
    if (rex::cvar::Query<bool>("pvz_input_trace")) REXLOG_INFO("PvZ service back vtable {:08X}", vt);
    return true;
  }
  if (widget && (Read(base, widget) == 0x820CB23C || Read(base, widget) == 0x820B466C)) {
    const auto vt = Read(base, widget);
    Call(ctx, base, rex::runtime::ResolveIndirectFunction(Read(base, vt + 0xC0)), {widget, 7, 0, 0});
    if (rex::cvar::Query<bool>("pvz_input_trace")) REXLOG_INFO("PvZ mouse {} back", vt == 0x820CB23C ? "Shop" : "Almanac");
    return true;
  }
  if (!widget || (Read(base, widget) != 0x820BB22C && Read(base, widget) != 0x820C1D24 && Read(base, widget) != 0x820BBF94)) return false;
  if (Read(base, widget) == 0x820BBF94) {
    Call(ctx, base, sub_8219D480, {widget, 7, 0, 0});
    REXLOG_INFO("PvZ mouse house leaderboard closed"); return true;
  }
  if (Read(base, widget) == 0x820C1D24) {
    Call(ctx, base, sub_821B9618, {widget, 7, 0, 0});
    REXLOG_INFO("PvZ mouse leaderboard closed"); return true;
  }
  if (rex::cvar::Query<bool>("pvz_input_trace")) REXLOG_INFO("PvZ closing help {:08X} app {:08X}", widget, Read(base, widget + 0x110));
  Call(ctx, base, sub_8219A210, {widget, 7, 0, 0});
  if (rex::cvar::Query<bool>("pvz_input_trace")) REXLOG_INFO("PvZ mouse help closed");
  return true;
}

REX_HOOK_RAW(sub_824AE488) {
  const auto button = ctx.r3.u32, parent = Read(base, button + 0x24);
  if (const auto found = onslaught_buttons.find(parent); found != onslaught_buttons.end() && found->second.widget == button) {
    if (ctx.r4.u32 == 6 && base[button + 0x74] && !base[button + 0x72]) {
      base[button + 0x74] = 0;
      // Last Stand starts on the native Y release, after its button sound.
      Call(ctx, base, sub_8214CDC0, {parent, 9, 0, 0});
      Call(ctx, base, sub_8214D1B8, {parent, 9, 0, 0});
      if (rex::cvar::Query<bool>("pvz_input_trace")) REXLOG_INFO("PvZ mouse Start Onslaught activated");
    }
    return;
  }
  for (auto* collection : {&service_prev_buttons, &service_next_buttons, &service_ready_buttons,
      &mail_read_buttons, &mail_inbox_buttons, &credits_replay_buttons}) {
    const auto entry = collection->find(parent);
    if (entry == collection->end() || entry->second.widget != button) continue;
    if (ctx.r4.u32 == 6 && base[button + 0x74] && !base[button + 0x72]) {
      base[button + 0x74] = 0;
      const auto vt = Read(base, parent), user = vt == 0x820CC2EC ?
          (pvz::LocalPlayersActive() ? ctx.r5.u32 : Read(base, parent + 0x118)) : 0;
      const bool rows = vt == 0x820C1D24;
      // Native X (action 8): Mail Inbox/Read switch, Credits replay.
      const bool x_action = collection == &mail_read_buttons || collection == &mail_inbox_buttons ||
          collection == &credits_replay_buttons;
      const auto action = x_action ? 8u : collection == &service_ready_buttons ? 6u : collection == &service_prev_buttons ?
          (rows ? 0u : 2u) : (rows ? 1u : 3u);
      Call(ctx, base, vt == 0x820CC2EC ? sub_8228B5C0 :
          rex::runtime::ResolveIndirectFunction(Read(base, vt + 0xC0)), {parent, action, user, 0});
      if (rex::cvar::Query<bool>("pvz_input_trace")) REXLOG_INFO("PvZ service navigation vtable {:08X} action {} user {}", vt, action, user);
    }
    return;
  }
  if (const auto chooser = chooser_buttons.find(parent); chooser != chooser_buttons.end() &&
      std::find(chooser->second.begin(), chooser->second.end(), button) != chooser->second.end()) {
    if (ctx.r4.u32 == 6 && base[button + 0x74] && !base[button + 0x72]) {
      base[button + 0x74] = 0;
      const auto visible = base[parent + 0x71];
      base[parent + 0x71] = 1; // Native listener otherwise rejects child-button hover.
      const auto id = Read(base, button + 0x6C);
      if (id == 1100) {
        // Board's Start pauses only while playing (8214D3A0 needs scene 3),
        // so on the chooser open the same pause menu as GamepadControls does:
        // the pause sound, then LawnApp::DoPauseDialog(user) (821A3B80).
        const auto app = Read(base, parent + 0xE60);
        const auto owner = int32_t(Read(base, parent + 0xEA8));
        const uint32_t user = owner >= 0 ? uint32_t(owner) : uint32_t(std::max(pvz::MouseUserIndex(), 0));
        Call(ctx, base, sub_821ABDE8, {app, Read(base, 0x82D09E14), 1});
        Call(ctx, base, sub_821A3B80, {app, user});
      }
      else Call(ctx, base, sub_8220AE58, {parent + 0xD4, id});
      if (chooser_buttons.contains(parent)) base[parent + 0x71] = visible;
      if (rex::cvar::Query<bool>("pvz_input_trace")) REXLOG_INFO("PvZ chooser button {} activated", id);
    }
    return;
  }
  if (const auto bonus = bonus_buttons.find(parent); bonus != bonus_buttons.end() && bonus->second.widget == button) {
    if (ctx.r4.u32 == 6 && base[button + 0x74] && !base[button + 0x72]) {
      base[button + 0x74] = 0;
      pvz::SwitchChallengePage(ctx, base, parent, pvz::kBonusPage);
    }
    return;
  }
  const auto endless = endless_buttons.find(parent);
  if (endless != endless_buttons.end() && endless->second.widget == button) {
    if (ctx.r4.u32 == 6 && base[button + 0x74]) {
      base[button + 0x74] = 0; Call(ctx, base, sub_8219D480, {parent, 9, 0, 0});
      REXLOG_INFO("PvZ mouse Survival Endless leaderboard opened");
    }
    return;
  }
  for (auto* collection : {&garden_shop_buttons, &shop_prev_buttons, &shop_next_buttons}) {
    const auto entry = collection->find(parent);
    if (entry == collection->end() || entry->second.widget != button) continue;
    if (ctx.r4.u32 == 6 && base[button + 0x74] && !base[button + 0x72]) {
      base[button + 0x74] = 0;
      // Clear the pressed state before native Shop enters its modal loop.
      if (collection == &garden_shop_buttons) {
        const auto app = Read(base, parent + 0xF4);
        if (Read(base, app + 0x9CC) == 51) Call(ctx, base, sub_82191B10, {Read(base, parent + 0x1B0), 8, 0, 0});
        else Call(ctx, base, sub_82292DA8, {Read(base, app + 0x9F0)});
        REXLOG_INFO("PvZ Zen Shop returned");
      } else {
        Call(ctx, base, sub_82217888, {parent, collection == &shop_prev_buttons ? 10u : 11u, Read(base, parent + 0x200), 0});
        REXLOG_INFO("PvZ Shop page button {}", collection == &shop_prev_buttons ? "previous" : "next");
      }
    }
    return;
  }
  const auto found = navigation_buttons.find(parent);
  if (Read(base, button + 0x6C) >= 1100 && rex::cvar::Query<bool>("pvz_input_trace"))
    REXLOG_INFO("PvZ navigation release {:08X} parent {:08X} found {} pressed {}", button, parent,
        found != navigation_buttons.end(), base[button + 0x74]);
  if (found == navigation_buttons.end() || found->second.widget != button) {
    __imp__sub_824AE488(ctx, base); return;
  }
  if (ctx.r4.u32 != 6 || !base[button + 0x74]) return;
  base[button + 0x74] = 0;
  if (Read(base, parent) == 0x820B728C) {
    Call(ctx, base, sub_8214CDC0, {parent, 5, 0, 0});
    if (rex::cvar::Query<bool>("pvz_input_trace")) REXLOG_INFO("PvZ mouse Menu opened");
  } else pvz::CloseMouseHelp(ctx, parent, base);
}
REX_HOOK_RAW(sub_8219B580) { RemoveNavigation(ctx, base, ctx.r3.u32); __imp__sub_8219B580(ctx, base); }
REX_HOOK_RAW(sub_821BE0B0) { RemoveNavigation(ctx, base, ctx.r3.u32); __imp__sub_821BE0B0(ctx, base); }
REX_HOOK_RAW(sub_82172788) { RemoveNavigation(ctx, base, ctx.r3.u32); __imp__sub_82172788(ctx, base); }
REX_HOOK_RAW(sub_8228A4B0) { RemoveNavigation(ctx, base, ctx.r3.u32); __imp__sub_8228A4B0(ctx, base); }
REX_HOOK_RAW(sub_821B7F80) { RemoveNavigation(ctx, base, ctx.r3.u32); __imp__sub_821B7F80(ctx, base); }
REX_HOOK_RAW(sub_8212D448) { RemoveNavigation(ctx, base, ctx.r3.u32); __imp__sub_8212D448(ctx, base); }
REX_EXTERN(__imp__sub_82213DF8);
REX_HOOK_RAW(sub_82213DF8) { RemoveNavigation(ctx, base, ctx.r3.u32); __imp__sub_82213DF8(ctx, base); }
REX_EXTERN(__imp__sub_82120C30);
REX_HOOK_RAW(sub_82120C30) { RemoveNavigation(ctx, base, ctx.r3.u32); __imp__sub_82120C30(ctx, base); }
REX_HOOK_RAW(sub_8219A2F0) {
  // ImageApplet removes and schedules destruction of all its children.
  // Let that native pass finish; deleting a child here invalidates the
  // outer Widget::RemovedFromManager iterator.
  navigation_buttons.erase(ctx.r3.u32);
  service_prev_buttons.erase(ctx.r3.u32);
  service_next_buttons.erase(ctx.r3.u32);
  mail_read_buttons.erase(ctx.r3.u32);
  mail_inbox_buttons.erase(ctx.r3.u32);
  __imp__sub_8219A2F0(ctx, base);
}
REX_HOOK_RAW(sub_8219A070) { RemoveNavigation(ctx, base, ctx.r3.u32); __imp__sub_8219A070(ctx, base); }
REX_HOOK_RAW(sub_82187F70) {
  const auto button = ctx.r3.u32;
  __imp__sub_82187F70(ctx, base);
  pinned_buttons.erase(button);
  if (const auto found = navigation_listeners.find(button); found != navigation_listeners.end()) {
    Call(ctx, base, sub_825F8F50, {found->second});
    navigation_listeners.erase(found);
  }
}

void pvz::ConfigureGameSettings(const std::filesystem::path& config_path,
                                const std::filesystem::path& game_data_root) {
  settings_path = config_path;
  // The SDK's vsync also changes guest WAIT_REG_MEM sleeping; it isn't the
  // host display switch. Keep that throttle off and preserve old UI settings.
  std::ifstream existing(config_path); std::string config((std::istreambuf_iterator<char>(existing)), {});
  if (config.find("pvz_vsync") == std::string::npos) {
    rex::cvar::SetFlagByName("pvz_vsync", rex::cvar::Query<bool>("vsync") ? "true" : "false");
    SavePreference("pvz_vsync", rex::cvar::Query<bool>("pvz_vsync") ? "true" : "false");
  }
  rex::cvar::SetFlagByName("vsync", "false");
  if (rex::cvar::Query<bool>("pvz_mouse_cursor"))
    rex::cvar::SetFlagByName("mnk_mouse", "false"); // Absolute cursor must not be captured for relative look.
  // Migrate the old default as well as new profiles; preserve custom bindings.
  if (rex::cvar::Query<std::string>("keybind_start") == "Return") {
    rex::cvar::SetFlagByName("keybind_start", "Escape");
    SavePreference("keybind_start", "\"Escape\"");
  }
  imported_package = !ImportedPackageSha256(game_data_root).empty();
  const auto edition = EvaluateGameEdition(game_data_root, rex::cvar::Query<bool>("pvz_test_unlock"));
  const auto full = edition.full;
  // Only the explicit test switch bypasses import detection. Legacy status
  // preferences and CLI license masks still do not select the edition.
  const auto reported = full ? 1u : 0u;
  rex::cvar::SetFlagByName("pvz_full_version", full ? "true" : "false");
  rex::cvar::SetFlagByName("license_mask", std::to_string(reported));
  REXLOG_INFO("PvZ edition: {} (license mask {}; {})", full ? "full" : "trial", reported, edition.reason);
  RefreshHostSettings();
}
void pvz::ApplyGameLanguage(const std::filesystem::path& game_data_root) {
  // The SDK reads user_language for the title's achievement strings while
  // loading the XEX, and the game reads XConfig USER_LANGUAGE at startup.
  language_root = game_data_root;
  auto language = REXCVAR_GET(pvz_language);
  if (!LanguageAvailable(language)) {
    REXLOG_WARN("PvZ language {}: {}.pak is missing from the game data, using English", language, language);
    language = "en";
  }
  running_language = language;
  const auto id = kLanguages[LanguageIndex(language)].id;
  rex::cvar::SetFlagByName("user_language", std::to_string(id));
  REXLOG_INFO("PvZ language: {} (user_language {})", language, id);
}
bool pvz::HasImportedPackage() { return imported_package; }
void pvz::SetHostSettingsCallback(std::function<void()> callback) {
  std::lock_guard lock(callback_mutex); host_callback = std::move(callback);
}
void pvz::RefreshHostSettings() {
  std::lock_guard lock(callback_mutex); if (host_callback) host_callback();
}
void pvz::PinNavigationButtons(PPCContext& ctx, uint8_t* base) {
  // Walk the per-screen collections: their entries follow screen lifetimes.
  for (auto* collection : {&navigation_buttons, &endless_buttons, &garden_shop_buttons, &onslaught_buttons,
      &shop_prev_buttons, &shop_next_buttons, &service_prev_buttons, &service_next_buttons,
      &service_ready_buttons, &mail_read_buttons, &mail_inbox_buttons, &bonus_buttons, &credits_replay_buttons})
    for (const auto& [parent, entry] : *collection)
      if (const auto pinned = pinned_buttons.find(entry.widget); pinned != pinned_buttons.end() &&
          Read(base, entry.widget + 0x24) == parent)
        Pin(ctx, base, entry.widget, parent, pinned->second);
}
void pvz::PinNavigationButton(PPCContext& ctx, uint8_t* base) {
  const auto button = ctx.r3.u32;
  const auto found = pinned_buttons.find(button);
  if (found == pinned_buttons.end()) return;
  if (const auto parent = Read(base, button + 0x24)) Pin(ctx, base, button, parent, found->second);
}
bool pvz::DrawChooserTextButton(PPCContext& ctx, uint8_t* base) {
  const auto button = ctx.r3.u32, parent = Read(base, button + 0x24);
  const auto found = chooser_buttons.find(parent);
  if (found == chooser_buttons.end() || found->second[0] != button) return false;
  // LET'S ROCK! as a caption in the console's white Dwarven Todcraft
  // lettering, without the stone: GameButton's plain label path with
  // FONT_DWARVENTODCRAFT18 and label colours 0 (normal) and 1 (highlighted).
  const auto font = Read(base, 0x82D09FD0);
  const auto colors = Read(base, button + 0x84);
  if (!font || !colors || Read(base, button + 0x88) - colors < 0x20) return false;
  const std::array<uint32_t, 8> saved_colors{Read(base, colors), Read(base, colors + 4), Read(base, colors + 8),
      Read(base, colors + 12), Read(base, colors + 16), Read(base, colors + 20), Read(base, colors + 24), Read(base, colors + 28)};
  const bool disabled = base[button + 0x72];
  const std::array<uint32_t, 4> normal = disabled ? std::array<uint32_t, 4>{128, 128, 128, 255} :
      std::array<uint32_t, 4>{255, 255, 255, 255};
  const std::array<uint32_t, 4> hover{255, 236, 130, 255};
  for (uint32_t i = 0; i < 4; ++i) { Write(base, colors + i * 4, normal[i]); Write(base, colors + 16 + i * 4, hover[i]); }
  const std::array<uint32_t, 6> offsets{0xF4, 0xF8, 0xFC, 0x100, 0x104, 0xF0};
  std::array<uint32_t, 6> saved{};
  for (size_t i = 0; i < offsets.size(); ++i) { saved[i] = Read(base, button + offsets[i]); Write(base, button + offsets[i], 0); }
  Write(base, button + 0xF4, font);
  const auto stone = base[button + 0x1B0]; base[button + 0x1B0] = 0;
  __imp__sub_82188310(ctx, base);
  base[button + 0x1B0] = stone;
  for (size_t i = 0; i < offsets.size(); ++i) Write(base, button + offsets[i], saved[i]);
  for (uint32_t i = 0; i < 8; ++i) Write(base, colors + i * 4, saved_colors[i]);
  return true;
}

bool pvz::DrawGardenNavigationButton(PPCContext& ctx, uint8_t* base) {
  const auto button = ctx.r3.u32, parent = Read(base, button + 0x24);
  if (!parent || Read(base, parent) != 0x820B728C || !MouseOwnsInput()) return false;
  const auto id = Read(base, button + 0x6C);
  if (id != 1100 && id != 1103) return false;
  const bool over = base[button + 0x75] && !base[button + 0x72];
  if (!over) return false;
  // Stone buttons bypass GameButton's image pulse. Transform their existing
  // native drawing, keeping the fixed painted bounds for hit testing.
  const auto graphics = ctx.r4.u32;
  std::array<uint32_t, 6> saved{};
  for (size_t i = 0; i < saved.size(); ++i) saved[i] = Read(base, graphics + 8 + uint32_t(i * 4));
  const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  const float pulse = 1.04f + 0.02f * float(std::sin(seconds * 7.0));
  PPCContext scale = ctx; scale.r1.u32 -= 0x100; scale.r3.u32 = graphics;
  scale.f1.f64 = std::bit_cast<float>(saved[2]) * pulse;
  scale.f2.f64 = std::bit_cast<float>(saved[3]) * pulse;
  scale.f3.f64 = int32_t(Read(base, button + 0x190)) + Read(base, button + 0x1A4) / 2.f;
  scale.f4.f64 = int32_t(Read(base, button + 0x194)) + Read(base, button + 0x1A8) / 2.f;
  sub_8249B508(scale, base);
  __imp__sub_82188310(ctx, base);
  for (size_t i = 0; i < saved.size(); ++i) Write(base, graphics + 8 + uint32_t(i * 4), saved[i]);
  if (rex::cvar::Query<bool>("pvz_input_trace")) {
    static uint32_t last = 0;
    if (last != button) REXLOG_INFO("PvZ garden button animated hover {}", id);
    last = button;
  }
  return true;
}

REX_HOOK_RAW(sub_821AFCA0) {
  // Native LawnApp license check tests bit 0 from XamContentGetLicenseMask
  // and caches success at +A59. Observe it without overriding its result.
  __imp__sub_821AFCA0(ctx, base);
  static int previous = -1;
  const int full = ctx.r3.u32 != 0;
  if (rex::cvar::Query<bool>("pvz_input_trace") && previous != full) {
    REXLOG_INFO("PvZ native full-game license check: {}", bool(full));
    previous = full;
  }
}

REX_HOOK_RAW(sub_821993F8) {
  const auto dialog = ctx.r3.u32;
  __imp__sub_821993F8(ctx, base);
  if (Read(base, dialog) != 0x820BABEC) return;
  const auto storage = Read(base, dialog + 0x1D8);
  const auto back = Read(base, dialog + 0x1D4);
  const auto volume = Read(base, dialog + 0x1D0);
  // Retain ownership so the native destructor still disposes the button.
  // Visibility and navigation are independent in Sexy's widget system.
  if (storage) { base[storage + 0x70] = 0; base[storage + 0x72] = 1; }
  if (back && volume) { Write(base, back + 0xB0, volume); Write(base, volume + 0xB4, back); }
  REXLOG_INFO("PvZ Change Storage hidden in PC settings");
}

REX_HOOK_RAW(sub_8219B8A0) {
  const auto chooser = ctx.r3.u32;
  __imp__sub_8219B8A0(ctx, base);
  if (Read(base, chooser) != 0x820CA94C || chooser_buttons.contains(chooser)) return;
  std::array<uint32_t, 4> items{};
  const std::array<std::u16string_view, 4> labels{u"LET'S ROCK!", u"SHOP", u"ALMANAC", u"MENU"};
  const std::array<uint32_t, 4> ids{100, 105, 103, 1100};
  // PC layout: LET'S ROCK inside the panel, Shop and Almanac to its lower
  // right on the lawn, Menu where Board keeps it in the top-right corner.
  // LET'S ROCK is centred under the seed grid (frame x = guest x + 241).
  const std::array<uint32_t, 4> left{130, 680, 560, 680}, top{548, 572, 572, uint32_t(-10)};
  // Shop/Almanac share Menu's height, which centres their labels lower.
  const std::array<uint32_t, 4> width{205, 120, 120, 120}, height{42, 46, 46, 46};
  for (int i = 0; i < 4; ++i) {
    const auto button = WithLabel(ctx, base, labels[i], [&](uint32_t label) {
      return Call(ctx, base, sub_82188F80, {ids[i], chooser + 0xD4, chooser, label});
    });
    items[i] = button;
    Call(ctx, base, sub_82188E20, {button, left[i], top[i], width[i], height[i]});
    Call(ctx, base, sub_82493E10, {chooser, button});
  }
  chooser_buttons.emplace(chooser, items);
}
REX_HOOK_RAW(sub_8220A0F8) {
  const auto chooser = ctx.r3.u32;
  __imp__sub_8220A0F8(ctx, base);
  if (pvz::LocalPlayersActive() && rex::cvar::Query<bool>("pvz_input_trace")) {
    static std::array<uint32_t, 5> previous{};
    const std::array<uint32_t, 5> state{chooser, Read(base, chooser + 0xE70),
        Read(base, chooser + 0xE74), Read(base, chooser + 0xE78), base[chooser + 0xD8]};
    if (state != previous) {
      REXLOG_INFO("PvZ local chooser selected {} banks {},{} ready {}", state[1], state[2], state[3], state[4]);
      previous = state;
    }
  }
  if (auto found = chooser_buttons.find(chooser); found != chooser_buttons.end()) {
    // VS: each side has its own chooser, owned by the signed-in user at +EA8.
    // Only the mouse player's panel gets the mouse buttons; Shop and Almanac
    // do not apply to a match, and Menu moves clear of both banks.
    const bool versus = pvz::layout::VersusChooser(base, chooser);
    const bool owner = !versus || !pvz::LocalPlayersActive() ||
        int32_t(Read(base, chooser + 0xEA8)) == pvz::MouseUserIndex();
    const bool mouse = pvz::MouseOwnsInput() && Read(base, chooser + 0xE9C) == 0 && owner;
    // The chooser slides in from below (and out after LET'S ROCK!). Its lawn
    // buttons (Shop, Almanac, Menu) are placed relative to it, so they would
    // rise through the lawn with it; show them once it has settled at y 0.
    // VS panels keep bobbing a few pixels around their resting place.
    const int32_t x = int32_t(Read(base, chooser + 0x40)), y = int32_t(Read(base, chooser + 0x44));
    const bool settled = versus ? std::abs(y) <= 8 : y == 0;
    const auto chooser_app = Read(base, chooser + 0xE60);
    const bool coop = chooser_app && pvz::layout::CoopMode(Read(base, chooser_app + 0x9CC));
    if (versus || coop) {
      // Menu stays in the free top-right corner beside the zombie (or second
      // co-op) bank, as on the lawn; the zombie panel centres LET'S ROCK! on
      // its narrower frame.
      Pin(ctx, base, found->second[3], chooser, {kVersusMenuLeft, kVersusMenuTop, 120, 46});
      if (base[chooser + 0xEB8]) Call(ctx, base, sub_82188E20, {found->second[0], uint32_t(kZombieRockLeft), 548, 205, 42});
    }
    // Shop and Almanac appear only once Adventure has opened them (Almanac
    // from 2-5, Shop from 3-5, or after the game is finished), as on PC.
    const auto app = Read(base, chooser + 0xE60), profile = Read(base, app + 0x9FC);
    const int level = profile ? int(Read(base, profile + 0x21C)) : 0;
    const bool finished = profile && (Read(base, profile + 0x218) & 1);
    auto available = [&](int i) {
      return (finished || level >= (i == 1 ? 25 : 15)) &&
          Call(ctx, base, i == 1 ? sub_821A9678 : sub_821A9608, {app});
    };
    for (int i = 0; i < 4; ++i) {
      const auto button = found->second[i];
      const bool service = i == 1 || i == 2;
      // Menu, Shop and Almanac open screens that belong to player 1's device.
      const bool dominant = !pvz::LocalPlayersActive() || pvz::MouseUserIndex() == 0;
      // VS starts by itself once both banks are full: no LET'S ROCK!.
      base[button + 0x70] = mouse && (i == 0 ? !versus :
          settled && dominant && !(versus && service) && (!service || available(i)));
      base[button + 0x72] = !mouse || (i == 0 ? !base[chooser + 0xD8] : false);
    }
  }
}
REX_HOOK_RAW(sub_82208950) {
  const auto chooser = ctx.r3.u32;
  if (auto found = chooser_buttons.find(chooser); found != chooser_buttons.end()) {
    for (const auto button : found->second) {
      if (Read(base, button + 0x24) == chooser) Call(ctx, base, sub_824930D0, {chooser, button});
      Call(ctx, base, sub_82187918, {button, 1});
    }
    chooser_buttons.erase(found);
  }
  __imp__sub_82208950(ctx, base);
}

REX_HOOK_RAW(sub_82198850) {
  const auto dialog = ctx.r3.u32;
  __imp__sub_82198850(ctx, base);
  if (Read(base, dialog) != kHelpOptions) return;
  auto found = buttons.find(dialog);
  if (found == buttons.end()) {
    const auto widget = WithLabel(ctx, base, u"Mouse: PC", [&](uint32_t label) {
      return Call(ctx, base, sub_82188F80, {kMouseMode, dialog + 0xD4, dialog, label});
    });
    found = buttons.emplace(dialog, SettingButton{widget, ""}).first;
    auto& setting = found->second;
    setting.cheats = WithLabel(ctx, base, u"Cheats", [&](uint32_t label) {
      return Call(ctx, base, sub_82188F80, {1007, dialog + 0xD4, dialog, label});
    });
    const auto app = Read(base, dialog + 0x1A4), manager = Read(base, dialog + 0x20);
    const auto menu = app ? Read(base, app + 0x908) : 0;
    if (menu && manager && pvz::guest::LiveWidget(base, manager, menu)) {
      pvz::guest::VisitWidgets(base, manager, [&](uint32_t widget) {
        if (Read(base, widget + 0x24) == menu && Read(base, widget + 0x6C) == 5) setting.return_focus = widget;
        return !setting.return_focus;
      });
    }
    for (size_t i = 0; i < setting.navigation.size(); ++i) {
      const auto original = Read(base, dialog + 0x1C4 + uint32_t(i * 4));
      for (size_t j = 0; j < 4; ++j)
        setting.navigation[i][j] = Read(base, original + 0xB0 + uint32_t(j * 4));
    }
    for (size_t i = 0; i < setting.visible.size(); ++i)
      setting.visible[i] = base[Read(base, dialog + 0x1C4 + uint32_t(i * 4)) + 0x70];
    for (size_t i = 0; i < setting.extra.size(); ++i) {
      setting.extra[i] = WithLabel(ctx, base, u"PC Settings", [&](uint32_t label) {
        return Call(ctx, base, sub_82188F80, {ExtraRowId(i), dialog + 0xD4, dialog, label});
      });
    }
    REXLOG_INFO("PvZ in-game mouse setting added to dialog {:08X}", dialog);
  }
  auto& button = found->second;
  UpdateLabel(ctx, base, button);
  Layout(ctx, base, dialog, button.widget);
  for (auto widget : AddedWidgets(button))
    if (Read(base, widget + 0x24) != dialog) Call(ctx, base, sub_82493E10, {dialog, widget});
}
REX_HOOK_RAW(sub_821988D0) {
  const auto dialog = ctx.r3.u32;
  const auto manager = Read(base, dialog + 0x20);
  const auto entry = buttons.find(dialog);
  const auto return_focus = entry != buttons.end() ? entry->second.return_focus : 0;
  if (const auto found = buttons.find(dialog); found != buttons.end())
    for (auto widget : AddedWidgets(found->second))
      if (Read(base, widget + 0x24) == dialog) Call(ctx, base, sub_824930D0, {dialog, widget});
  __imp__sub_821988D0(ctx, base);
  if (!pvz::MouseOwnsInput() && manager && pvz::guest::LiveWidget(base, manager, return_focus))
    Call(ctx, base, sub_824A5F48, {manager, return_focus});
}
REX_HOOK_RAW(sub_82198938) {
  const auto dialog = ctx.r3.u32;
  __imp__sub_82198938(ctx, base);
  if (const auto found = buttons.find(dialog); found != buttons.end()) {
    // Native Resize creates the navigation links after AddedToManager.
    // Capture that graph before inserting our PC rows.
    for (size_t i = 0; i < found->second.navigation.size(); ++i) {
      const auto original = Read(base, dialog + 0x1C4 + uint32_t(i * 4));
      for (size_t j = 0; j < 4; ++j)
        found->second.navigation[i][j] = Read(base, original + 0xB0 + uint32_t(j * 4));
    }
    Layout(ctx, base, dialog, found->second.widget);
  }
}
REX_HOOK_RAW(sub_82198720) {
  const auto dialog = ctx.r3.u32;
  if (const auto found = buttons.find(dialog); found != buttons.end()) {
    for (auto widget : AddedWidgets(found->second)) {
      if (Read(base, widget + 0x24) == dialog) Call(ctx, base, sub_824930D0, {dialog, widget});
      Call(ctx, base, sub_82187918, {widget, 1});
    }
    buttons.erase(found);
    if (rex::cvar::Query<bool>("pvz_input_trace")) REXLOG_INFO("PvZ in-game mouse setting destroyed");
  }
  __imp__sub_82198720(ctx, base);
}
REX_HOOK_RAW(sub_82198B08) {
  if (const auto found = buttons.find(ctx.r3.u32); found != buttons.end())
    UpdateLabel(ctx, base, found->second);
  __imp__sub_82198B08(ctx, base);
}
REX_HOOK_RAW(sub_82198B30) {
  const auto dialog = ctx.r3.u32 - 0xD4;
  if (rex::cvar::Query<bool>("pvz_input_trace"))
    REXLOG_INFO("PvZ settings listener {:08X} dialog {:08X} button {} found {}", ctx.r3.u32, dialog, ctx.r4.u32, buttons.contains(dialog));
  if (auto found = buttons.find(dialog); found != buttons.end()) {
    auto& setting = found->second;
    const auto id = ctx.r4.u32;
    const bool leave_pc_page = (id == 4 || id == 1000) && setting.page;
    if (id == 1007 && setting.page) { pvz::RequestHostMenu(pvz::HostMenu::Cheats); return; }
    if (id == kMouseMode && !setting.page) setting.page = true;
    else if ((id == 4 || id == 1000) && setting.page) setting.page = false;
    else if (((id >= kMouseMode && id <= kMouseMode + 4) || id == kLanguageRow) && setting.page) {
      std::string key, value;
      bool quoted = false;
      if (id == kLanguageRow) { key = "pvz_language"; quoted = true;
        // Cycle through the languages whose pak the imported game has.
        auto next = LanguageIndex(REXCVAR_GET(pvz_language));
        do next = (next + 1) % kLanguages.size();
        while (next && !LanguageAvailable(kLanguages[next].code));
        value = kLanguages[next].code;
      } else if (id == kMouseMode) { key = "pvz_cursor_mode"; quoted = true;
        value = rex::cvar::Query<std::string>("pvz_cursor_mode") == "pc" ? "console" : "pc";
      } else if (id == kMouseMode + 1) { key = "pvz_vsync";
        value = rex::cvar::Query<bool>(key) ? "false" : "true";
      } else if (id == kMouseMode + 2) { key = "pvz_pause_on_focus_loss";
        value = REXCVAR_GET(pvz_pause_on_focus_loss) ? "false" : "true";
      } else if (id == kMouseMode + 4) { key = "pvz_skip_intro";
        value = REXCVAR_GET(pvz_skip_intro) ? "false" : "true";
      } else { key = "pvz_display_mode"; quoted = true;
        const auto mode = REXCVAR_GET(pvz_display_mode);
        value = mode == "windowed" ? "fullscreen" : mode == "fullscreen" ? "borderless" : "windowed";
      }
      rex::cvar::SetFlagByName(key, value);
      if (key == "pvz_cursor_mode") pvz::NotifyMouseModeChanged();
      const bool saved = SavePreference(key, quoted ? "\"" + value + "\"" : value);
      pvz::RefreshHostSettings();
      REXLOG_INFO("PvZ in-game setting {} = {} (saved {})", key, value, saved);
    } else { __imp__sub_82198B30(ctx, base); return; }
    UpdateLabel(ctx, base, setting);
    Layout(ctx, base, dialog, setting.widget);
    if (leave_pc_page && !pvz::MouseOwnsInput())
      Call(ctx, base, sub_824A5F48, {Read(base, dialog + 0x20), Read(base, dialog + 0x1D4)});
    return;
  }
  __imp__sub_82198B30(ctx, base);
}
