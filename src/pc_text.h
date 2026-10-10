#pragma once
#include "mouse_prompt.h"
#include <array>

namespace pvz {
struct TextBindings {
  std::u16string x = u"L", y = u"P", start = u"Escape";
  std::u16string lb = u"1", rb = u"3", lt = u"Q", rt = u"E";
};
inline std::u16string PcGameText(std::u16string_view key, std::u16string text,
    bool mouse, bool pc, const TextBindings& b = {}) {
  struct Entry { std::u16string_view key, value; };
  constexpr std::array platform{
    Entry{u"SELECT_DEVICE", u"Save folder"},
    Entry{u"INVALID_LEVEL", u"This save is from an earlier level. Your profile's current level will be used."},
    Entry{u"LB_VIEW_GAMERCARD", u"View player profile"},
    Entry{u"VIEW_GAMERCARD", u"View Player Profile"},
    Entry{u"GAMERTAG", u"Player name"},
    Entry{u"NOT_LOGGED_IN_HOUSES", u"Online friends' houses are unavailable in this PC build."},
    Entry{u"NOT_LOGGED_IN_MESSAGE", u"Online leaderboards are unavailable in this PC build."},
    Entry{u"NOT_ENOUGH_SPACE", u"There is not enough free space to save. Free up disk space and try again."},
    Entry{u"SAVEGAME_ERROR_DISKFULL", u"There is not enough free disk space to save.\nFree up disk space and try again."},
    Entry{u"SAVING_LONG", u"Saving. Please keep the game open."},
    Entry{u"SAVING_LIVE_DIALOG", u"Saving game data"},
    Entry{u"GO_TO_MARKETPLACE", u"Additional Content"},
    Entry{u"UPSELL_MARKETPLACE", u"Additional Content"},
    Entry{u"NOT_SIGNED_IN_MARKETPLACE", u"Online content purchases are unavailable in this PC build."},
    Entry{u"TRIAL_LOCKED_MESSAGE", u"This content is unavailable in the current build."},
    Entry{u"REGISTER", u"This content is unavailable in the current build."},
    Entry{u"ACHIEVEMENT_EARNED_UPSELL_BODY", u"Achievement earned!"},
    Entry{u"HELP_TEXT_5_1", u"Welcome to your house! Crazy Dave will renovate it every so often as you get better at defending your lawn!"},
    Entry{u"UPSELL_QUIT_GAME", u"<B> Quit Game"},
    Entry{u"CREDITS_PAUSE_BODY", u"Resume to continue the credits.\n\nYou can also view the credits later\nfrom the options on the main menu."},
    Entry{u"UPSELL_PAUSE_BODY", u"Resume to continue."}};
  for (const auto& e : platform) if (key == e.key) { text = e.value; break; }
  // Player 2 must press the buttons on their own controller to join.
  if (!mouse || key.starts_with(u"2P_") || key == u"P2_JOIN" ||
      key == u"RECONNECT_SECOND_CONTROLLER_FMT") return text;
  if (key == u"CLICK_TO_CONTINUE") return {};
  if (key == u"ADVICE_CLICK_SEED_PACKET")
    return pc ? u"Click a seed packet to select it!" : u"Use the mouse wheel to select a seed packet!";
  if (key == u"ADVICE_CLICK_ON_SUN")
    return pc ? u"Click the falling sun to collect it!" : u"Move over to the falling sun to collect it!";
  if (key == u"ADVICE_CLICK_SHOVEL") return pc ?
    u"Click the shovel, then click the plant you want to dig up!" :
    u"Point at a plant, then right-click to dig!";
  if (key == u"HELP_TEXT_2_LSTICK") return u"Mouse: Move Cursor";
  if (key == u"HELP_TEXT_2_BUMPERS") return pc ? u"LMB: Select Seed Packet" : u"Wheel: Select Seed Packet";
  if (key == u"HELP_TEXT_2_B") return pc ? u"LMB: Shovel / Dig; RMB: Cancel" : u"RMB: Dig / Cancel";
  if (key == u"ADVICE_HEAVY_WEAPON") return pc ? u"Move the mouse to drive and aim!\nHold LMB to fire. Click a seed packet to buy an upgrade!" : u"Move the mouse to drive and aim!\nHold LMB to fire. Wheel: Select upgrade; RMB: Buy.";
  if (key == u"ADVICE_SLOT_MACHINE_SPIN_AGAIN" || key == u"SLOT_MACHINE_PULL_TOOLTIP") return pc ? u"Click the lever to spin again!" : u"Scroll the wheel to spin again!";
  if (key == u"ADVICE_SLOT_MACHINE_PULL") return pc ? u"Click the lever to spin the slot machine!" : u"Scroll the wheel to spin the slot machine!";
  if (key == u"TREE_OF_WISDOM_5") return u"Try entering " + b.lb + u", " + b.rb + u", " + b.lt + u", " + b.rt + u" during a game to unlock some cool commands.";
  // Also covers the second Tree of Wisdom hint without changing the cheat name.
  const auto replace = [&](std::u16string_view from, std::u16string_view to) {
    size_t at = 0;
    while ((at = text.find(from, at)) != std::u16string::npos) {
      text.replace(at, from.size(), to); at += to.size();
    }
  };
  replace(u"LB RB LT RT", b.lb + u" " + b.rb + u" " + b.lt + u" " + b.rt);
  replace(u"<X>", b.x); replace(u"<Y>", b.y);
  replace(u"<L>", b.lb); replace(u"<R>", b.rb);
  replace(u"<l>", u"W/A/S/D"); replace(u"<r>", u"Arrow keys");
  text = MousePrompt(std::move(text), pc);
  replace(u"Escape", b.start);
  return text;
}
}
