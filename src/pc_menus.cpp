#include "pc_menus.h"
#include "guest_memory.h"
#include "widget_tree.h"
#include "mouse_bridge.h"
#include "input_backend.h"
#include "game_settings.h"
#include "pc_text_runtime.h"
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <mutex>
#include <atomic>
#include <deque>
#include <utility>
#include <optional>
#include <string>

REX_EXTERN(__imp__sub_821C0630);
REX_EXTERN(__imp__sub_82289228); // TypingCheck::Check
REX_EXTERN(sub_821475A8); // Board::DoTypingCheck
REX_EXTERN(sub_821258D8);
REX_EXTERN(sub_821256C0);
REX_EXTERN(sub_821A3FF8); // LawnApp::DoDialog, modal result 1000/1001
REX_EXTERN(sub_824A5F48);
REX_EXTERN(sub_824A6208); // WidgetManager::MouseMove
REX_EXTERN(__imp__sub_822CA130); // XamShowMarketplaceUI wrapper
REX_EXTERN(sub_821AFCA0); // LawnApp license check; r4 = 0 runs the unlock transition
REX_EXTERN(sub_821A9678); // LawnApp::CanShowStore

namespace {
using pvz::guest::Read;
using pvz::guest::Write;
std::mutex mutex;
std::function<void(pvz::HostMenu)> callback;
pvz::CheatState state;
std::deque<size_t> commands;
std::atomic_bool open{false};
std::atomic<uint32_t> buttons{0};
std::atomic<uint32_t> held_buttons{0};
// Guest thread: the panel whose button last left the main menu, and whether
// the current MainMenu instance has been handled.
uint32_t return_panel = 1;
bool menu_seen = false;
// $ in the full game slides to Extras first and presses its Shop there.
uint32_t pending_shop_menu = 0;
// Override matching for exactly one native evaluation, on the guest thread.
// All effects and Tree requirements still run inside Board::DoTypingCheck.
thread_local uint32_t requested_check = 0;
constexpr std::array<uint32_t, 7> checks{2684, 2692, 2700, 2704, 2708, 2712, 2716};
constexpr std::array<int, 7> heights{0, 0, 0, 1000, 500, 100, 0};
uint32_t Call(PPCContext& ctx, uint8_t* b, PPCFunc* fn, std::initializer_list<uint32_t> args) {
  PPCContext work = ctx; work.r1.u32 -= 0x800;
  const std::array regs{&work.r3, &work.r4, &work.r5, &work.r6, &work.r7, &work.r8, &work.r9, &work.r10};
  size_t i = 0; for (auto arg : args) regs[i++]->u64 = arg;
  fn(work, b); return work.r3.u32;
}
// LawnApp::DoDialog with raw UTF-16 strings; it constructs its own copies.
// Mode 1 shows two buttons (result 1000/1001), mode 3 one footer button.
uint32_t NativeDialog(PPCContext& ctx, uint8_t* b, uint32_t app, uint32_t id,
    std::array<std::u16string, 4> text, uint32_t mode) {
  std::array<uint32_t, 4> strings{};
  for (size_t i = 0; i < text.size(); ++i) {
    // Scratch between the hook's stack pointer and the callee frame (0x800).
    const auto chars = ctx.r1.u32 - 0x7C0 + uint32_t(i * 0x1E0);
    strings[i] = chars;
    const size_t length = std::min<size_t>(text[i].size(), 0xEE);
    for (size_t j = 0; j <= length; ++j) {
      const auto ch = j < length ? text[i][j] : char16_t(0);
      b[chars + j * 2] = uint8_t(ch >> 8); b[chars + j * 2 + 1] = uint8_t(ch);
    }
  }
  return Call(ctx, b, sub_821A3FF8, {app, id, strings[0], strings[1], strings[2], strings[3], mode});
}
std::u16string OkText() { return pvz::PortText({u"OK"}, u"DIALOG_BUTTON_OK"); }
std::u16string CancelText() { return pvz::PortText({u"Cancel"}, u"BUTTON_CANCEL"); }
std::u16string UnlockTitle() { return pvz::PortText({u"Unlock Full Game"}, u"UNLOCK_FULL_VERSION"); }
uint32_t ConfirmExit(PPCContext& ctx, uint8_t* b, uint32_t app) {
  // The header sets LawnDialog's width; a short one keeps the minimum size.
  return NativeDialog(ctx, b, app, 42, {
      pvz::PortText({u"Quit Game?", u"Spiel beenden?", u"¿Salir del juego?", u"Quitter le jeu ?",
          u"Uscire dal gioco?", u"終了しますか?"}),
      pvz::PortText({u"Are you sure you want to quit the game?"}, u"QUIT_MESSAGE"),
      pvz::PortText({u"Quit"}, u"QUIT_BUTTON"), CancelText()}, 1);
}
// Full-game unlock from the player's own XBLA package. The host picks and
// checks the file; the guest applies it like a Marketplace purchase.
std::atomic_bool unlock_busy{false};
std::optional<pvz::UnlockResult> unlock_result;
std::u16string Widen(std::string_view text) { return {text.begin(), text.end()}; }
// Native depress accepts input only on a settled panel after the crow intro.
bool StableMenuPanel(uint8_t* b, uint32_t menu) {
  return Read(b, menu + 0x118) == Read(b, menu + 0x11C) && !b[menu + 0xD8] && int32_t(Read(b, menu + 0x168)) <= 0;
}
}
void pvz::SetHostMenuCallback(std::function<void(HostMenu)> fn) {
  std::lock_guard lock(mutex); callback = std::move(fn);
}
void pvz::RequestHostMenu(HostMenu menu) {
  std::lock_guard lock(mutex); if (callback) callback(menu);
}
void pvz::PostUnlockResult(UnlockResult result) {
  std::lock_guard lock(mutex); unlock_result = std::move(result);
}
void pvz::SetHostMenuOpen(bool value) { open.store(value); buttons.store(0); held_buttons.store(0); }
bool pvz::HostMenuOpen() { return open.load(); }
bool pvz::RouteHostMenuButton(uint32_t button, uint32_t player) {
  if (!open.load()) return false;
  if (player == 0 && button < 32) {
    const auto bit = 1u << button;
    if (!(held_buttons.fetch_or(bit) & bit)) buttons.fetch_or(bit);
  }
  return true;
}
bool pvz::RouteHostMenuButtonUp(uint32_t button, uint32_t player) {
  if (!open.load()) return false;
  if (player == 0 && button < 32) held_buttons.fetch_and(~(1u << button));
  return true;
}
uint32_t pvz::TakeHostMenuButtons() { return buttons.exchange(0); }
pvz::CheatState pvz::ReadCheatState() { std::lock_guard lock(mutex); return state; }
void pvz::RequestCheat(size_t index) {
  if (index < checks.size()) { std::lock_guard lock(mutex); commands.push_back(index); }
}
void pvz::UpdatePcMenus(PPCContext& ctx, uint8_t* b, uint32_t manager) {
  uint32_t app = 0;
  guest::VisitWidgets(b, manager, [&](uint32_t widget) {
    const auto vt = Read(b, widget);
    if (vt == 0x820C2784) app = Read(b, widget + 0x110);
    else if (vt == 0x820B728C) app = Read(b, widget + 0xF4);
    else if (vt == 0x820BAA54) app = Read(b, widget + 0x1A4);
    return !app;
  });
  std::optional<UnlockResult> unlocked;
  if (app) { std::lock_guard lock(mutex); unlocked = std::exchange(unlock_result, std::nullopt); }
  if (unlocked) {
    if (unlocked->full) {
      // XamContentGetLicenseMask now reports bit 0. A non-cached check runs
      // the native trial-to-full transition (821AFD38), as after a purchase.
      rex::cvar::SetFlagByName("pvz_full_version", "true");
      rex::cvar::SetFlagByName("license_mask", "1");
      const auto full = Call(ctx, b, sub_821AFCA0, {app, 0});
      REXLOG_INFO("PvZ full game unlocked in game; native license check {}", full);
      NativeDialog(ctx, b, app, 43, {
          pvz::PortText({u"Full Game Unlocked!", u"Vollversion freigeschaltet!", u"¡Juego completo desbloqueado!",
              u"Jeu complet débloqué !", u"Gioco completo sbloccato!", u"完全版を解除しました!"}),
          pvz::PortText({u"Thank you! Every level, mode and plant is now available.",
              u"Vielen Dank! Alle Level, Modi und Pflanzen sind jetzt verfügbar.",
              u"¡Gracias! Ya tienes disponibles todos los niveles, modos y plantas.",
              u"Merci ! Tous les niveaux, modes et plantes sont maintenant disponibles.",
              u"Grazie! Ora tutti i livelli, le modalità e le piante sono disponibili.",
              u"ありがとう!すべてのステージ、モード、\nプラントが遊べるようになりました。"}),
          OkText(), u""}, 3);
    } else if (!unlocked->message.empty()) {
      REXLOG_INFO("PvZ in-game unlock failed: {}", unlocked->message);
      NativeDialog(ctx, b, app, 43, {UnlockTitle(), Widen(unlocked->message), OkText(), u""}, 3);
    }
    unlock_busy.store(false);
  }
  CheatState snapshot;
  if (app) {
    snapshot.available = true;
    const auto profile = Read(b, app + 0x9FC);
    const int height = profile ? int32_t(Read(b, profile + 0x2E8)) : 0;
    std::deque<size_t> pending;
    { std::lock_guard lock(mutex); pending.swap(commands); }
    for (auto index : pending) {
      const auto board = Read(b, app + 0x900);
      if (board && guest::LiveWidget(b, manager, board) && Read(b, board) == 0x820B728C) {
        requested_check = Read(b, app + checks[index]);
        if (requested_check) Call(ctx, b, sub_821475A8, {board});
        requested_check = 0;
      } else if (height >= heights[index]) {
        // The constructor copies these native app preferences to a new Board.
        b[app + 0xAA0 + index] = !b[app + 0xAA0 + index];
      }
      REXLOG_INFO("PvZ native cheat {} enabled {}", index, bool(b[app + 0xAA0 + index]));
    }
    for (size_t i = 0; i < 7; ++i) {
      snapshot.enabled[i] = b[app + 0xAA0 + i] != 0;
      snapshot.allowed[i] = height >= heights[i];
    }
  }
  { std::lock_guard lock(mutex); state = snapshot; }
}
REX_HOOK_RAW(sub_82289228) {
  if (requested_check) { ctx.r3.u64 = ctx.r3.u32 == requested_check; return; }
  __imp__sub_82289228(ctx, base);
}
REX_HOOK_RAW(sub_822CA128) {
  pvz::RequestHostMenu(pvz::HostMenu::Achievements);
  ctx.r3.u64 = 0;
}
REX_HOOK_RAW(sub_822CA130) {
  // Every "Unlock Full Game" / "Get Full Game" path ends in Marketplace.
  const auto app = Read(base, 0x82D0BC88);
  if (!app) { __imp__sub_822CA130(ctx, base); return; }
  if (rex::cvar::Query<bool>("pvz_full_version")) {
    NativeDialog(ctx, base, app, 43, {
        pvz::PortText({u"Full Game", u"Vollversion", u"Juego completo", u"Jeu complet", u"Gioco completo", u"完全版"}),
        pvz::PortText({u"The full game is unlocked.\nAdditional content is unavailable in this PC build.",
            u"Das vollständige Spiel ist freigeschaltet.\nZusatzinhalte gibt es in dieser PC-Version nicht.",
            u"El juego completo está desbloqueado.\nEl contenido adicional no está disponible en esta versión para PC.",
            u"Le jeu complet est débloqué.\nLe contenu supplémentaire n'est pas disponible dans cette version PC.",
            u"Il gioco completo è sbloccato.\nI contenuti aggiuntivi non sono disponibili in questa versione per PC.",
            u"完全版は解除されています。\nこのPC版では追加コンテンツは使えません。"}),
        OkText(), u""}, 3);
  } else if (!unlock_busy.load()) {
    // Imported game files were already checked; only the purchase remains.
    const auto result = pvz::HasImportedPackage() ?
        NativeDialog(ctx, base, app, 43, {UnlockTitle(), pvz::PortText({
            u"The trial and the full version share the same game files.\n\n"
            u"Unlock only if you bought the full version of Plants vs. Zombies.",
            u"Testversion und Vollversion verwenden dieselben Spieldateien.\n\n"
            u"Schalte es nur frei, wenn du die Vollversion von Pflanzen gegen Zombies gekauft hast.",
            u"La versión de prueba y la completa usan los mismos archivos.\n\n"
            u"Desbloquéalo solo si compraste la versión completa de Plantas contra Zombis.",
            u"La version d'essai et la version complète utilisent les mêmes fichiers.\n\n"
            u"Ne déverrouillez le jeu que si vous avez acheté la version complète de Plantes contre Zombies.",
            u"La versione di prova e quella completa usano gli stessi file.\n\n"
            u"Sbloccalo solo se hai acquistato la versione completa di Piante contro zombi.",
            u"体験版と完全版のゲームファイルは同じです。\n\n"
            u"プラント vs. ゾンビ の完全版を購入した場合のみ\n解除してください。"}),
            pvz::PortText({u"I Bought It", u"Gekauft", u"Lo he comprado", u"Je l'ai acheté", u"L'ho acquistato",
                u"購入しました"}), CancelText()}, 1) :
        NativeDialog(ctx, base, app, 43, {UnlockTitle(), pvz::PortText({
            u"Choose your Plants vs. Zombies package or Xbox 360 disc image.\n\n"
            u"By continuing, you confirm it is your own full copy.",
            u"Wähle dein Pflanzen gegen Zombies-Paket oder Xbox 360-Disc-Image.\n\n"
            u"Wenn du fortfährst, bestätigst du, dass es deine eigene Vollversion ist.",
            u"Elige tu paquete de Plantas contra Zombis o la imagen de disco de Xbox 360.\n\n"
            u"Al continuar, confirmas que es tu propia copia completa.",
            u"Choisissez votre paquet Plantes contre Zombies ou votre image disque Xbox 360.\n\n"
            u"En continuant, vous confirmez qu'il s'agit de votre propre copie complète.",
            u"Scegli il pacchetto di Piante contro zombi o l'immagine del disco Xbox 360.\n\n"
            u"Continuando, confermi che si tratta della tua copia completa.",
            u"プラント vs. ゾンビ のパッケージか\nXbox 360 のディスクイメージを選んでください。\n\n"
            u"このファイルが自分の完全版である場合のみ\n続けてください。"}),
            pvz::PortText({u"Choose File", u"Datei wählen", u"Elegir archivo", u"Choisir un fichier",
                u"Scegli file", u"ファイルを選ぶ"}), CancelText()}, 1);
    REXLOG_INFO("PvZ unlock prompt result {}", result);
    if (result == 1000 && !unlock_busy.exchange(true)) pvz::RequestHostMenu(pvz::HostMenu::Unlock);
  }
  ctx.r3.u64 = 0;
}
REX_HOOK_RAW(sub_821C0630) {
  const auto menu = ctx.r3.u32 - 0xD4;
  // The full game's $ opens Crazy Dave's shop (Extras' Shop, 16) instead of
  // Marketplace, but only once Adventure has opened it (LawnApp::CanShowStore,
  // 821A9678): a full import, such as from a disc, must not skip that.
  // The trial keeps the native unlock path.
  if (ctx.r4.u32 == 6 && rex::cvar::Query<bool>("pvz_full_version")) {
    const auto app = Read(base, menu + 0x110);
    if (!app || !Call(ctx, base, sub_821A9678, {app})) {
      if (app && StableMenuPanel(base, menu))
        NativeDialog(ctx, base, app, 43, {
            pvz::PortText({u"Crazy Dave's Shop", u"Deppie Daves Laden", u"Tienda de Crazy Dave",
                u"Magasin de Dave le Dingo", u"Negozio di Dave il pazzo", u"クレイジーデイブのショップ"}),
            pvz::PortText({u"Crazy Dave opens his shop as you progress through Adventure. Keep playing!",
                u"Deppie Dave öffnet seinen Laden, wenn du im Abenteuer weiterkommst. Spiel weiter!",
                u"Crazy Dave abrirá su tienda a medida que avances en el modo Aventura. ¡Sigue jugando!",
                u"Dave le Dingo ouvre son magasin au fil du mode Aventure. Continuez à jouer !",
                u"Dave il pazzo aprirà il negozio man mano che procedi nell'Avventura. Continua a giocare!",
                u"アドベンチャーを進めると、\nクレイジーデイブがショップを開くよ。\nプレイを続けよう!"}),
            OkText(), u""}, 3);
      return;
    }
    // Shop (16) lives on Extras: pressing it from the main panel plays its
    // animation off screen. Slide to Extras (8); RestoreMenuPanel presses
    // Shop once that panel settles.
    if (!StableMenuPanel(base, menu)) return;
    pending_shop_menu = menu;
    ctx.r4.u64 = 8;
  }
  // Native depress accepts input only on a settled panel after the intro.
  if (Read(base, menu + 0x118) == Read(base, menu + 0x11C) && !base[menu + 0xD8] &&
      int32_t(Read(base, menu + 0x168)) <= 0) return_panel = Read(base, menu + 0x118);
  if ((ctx.r4.u32 == 10 || ctx.r4.u32 == 11) && Read(base, menu + 0x118) == Read(base, menu + 0x11C) &&
      !base[menu + 0xD8]) pvz::BeginLocalPlayers(pvz::MouseOwnsInput());
  const bool options = ctx.r4.u32 == 5;
  const auto manager = Read(base, menu + 0x20);
  if (ctx.r4.u32 == 7 && Read(base, menu + 0x118) == Read(base, menu + 0x11C) &&
      !base[menu + 0xD8] && int32_t(Read(base, menu + 0x168)) <= 0) {
    const auto app = Read(base, menu + 0x110);
    const auto result = ConfirmExit(ctx, base, app);
    REXLOG_INFO("PvZ PC quit confirmation result {}", result);
    if (result == 1000) pvz::RequestHostMenu(pvz::HostMenu::Quit);
    return;
  }
  // MainMenu::GotFocus restores this native button ID after the modal closes.
  // Restoring only WidgetManager's child pointer leaves Adventure remembered.
  if (options) Write(base, menu + 0x148, 5);
  __imp__sub_821C0630(ctx, base);
  // The native modal caller takes focus back after RemovedFromManager.
  // Restore Options after that final handoff, so Adventure is not selected.
  if (options && !pvz::MouseOwnsInput() && pvz::guest::LiveWidget(base, manager, menu)) {
    uint32_t target = 0;
    pvz::guest::VisitWidgets(base, manager, [&](uint32_t widget) {
      if (Read(base, widget + 0x24) == menu && Read(base, widget + 0x6C) == 5) target = widget;
      return !target;
    });
    if (target) {
      Write(base, menu + 0x148, 5);
      // The modal restores the cached mouse position, which may lie on
      // Adventure although a controller owns input. Flush that hover too.
      Call(ctx, base, sub_824A6208, {manager, uint32_t(-1), uint32_t(-1)});
      pvz::guest::VisitWidgets(base, menu, [&](uint32_t widget) {
        if (Read(base, widget + 0x24) == menu) {
          base[widget + 0xA2] = widget == target;
          base[widget + 0x75] = widget == target;
          base[widget + 0x74] = 0;
        }
        return true;
      });
      Call(ctx, base, sub_824A5F48, {manager, target});
    }
    REXLOG_INFO("PvZ settings returned gamepad focus to Options {}", bool(target));
  }
}
REX_EXTERN(__imp__sub_821A25C8);
REX_HOOK_RAW(sub_821A25C8) {
  pvz::ResetLocalPlayers();
  __imp__sub_821A25C8(ctx, base);
}

void pvz::RestoreMenuPanel(PPCContext& ctx, uint8_t* b, uint32_t manager) {
  uint32_t menu = 0;
  int budget = 64;
  guest::VisitChildren(b, manager, budget, [&](uint32_t child) {
    if (Read(b, child) == 0x820C2784 && b[child + 0x70]) menu = child;
    return !menu;
  });
  if (!menu) { menu_seen = false; pending_shop_menu = 0; return; }
  if (pending_shop_menu) {
    if (pending_shop_menu != menu) { pending_shop_menu = 0; }
    else if (Read(b, menu + 0x118) == 2 && StableMenuPanel(b, menu)) {
      pending_shop_menu = 0;
      Call(ctx, b, __imp__sub_821C0630, {menu + 0xD4, 16u});
      REXLOG_INFO("PvZ $ opened the shop from Extras");
    }
  }
  // Wait for the new menu to settle on its main panel (1).
  if (menu_seen || Read(b, menu + 0x118) != 1 || Read(b, menu + 0x11C) != 1 || b[menu + 0xD8]) return;
  menu_seen = true;
  const auto panel = std::exchange(return_panel, 1u);
  if (panel != 0 && panel != 2) return;
  // Returning from a sub-screen skips the crow intro, then slides through the
  // native More Ways to Play (2) or Extras (8) button.
  Write(b, menu + 0x168, 0);
  Call(ctx, b, __imp__sub_821C0630, {menu + 0xD4, panel == 0 ? 2u : 8u});
  REXLOG_INFO("PvZ main menu returned to panel {}", panel);
}
