#pragma once
#include "mouse_prompt.h"
#include <array>

namespace pvz {
// Only Escape remains on the keyboard (Start); the mouse covers every other
// action, so controller markup is explained in mouse terms.
struct TextBindings {
  std::u16string start = u"Escape";
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
    Entry{u"NOT_LOGGED_IN", u"Online Unavailable"},
    Entry{u"NOT_LOGGED_IN_HOUSES", u"Online features are not available in this PC build yet, so friends' houses cannot be shown."},
    Entry{u"NOT_LOGGED_IN_MESSAGE", u"Online features are not available in this PC build yet, so the online leaderboard cannot be shown."},
    // Letters were downloaded from Xbox LIVE (mailbox_<lang>.pak) for
    // signed-in profiles only, so offline the mailbox stays empty.
    Entry{u"MAILBOX_EMPTY", u"No letters yet! Zombie mail arrives online,\nand online features are not available in this PC build yet."},
    Entry{u"CROW_SELECTION_7", u"Exit the game."},
    Entry{u"CROW_SELECTION_DLC", u"Spend virtual money on fictional items."},
    Entry{u"NOT_ENOUGH_SPACE", u"There is not enough free space to save. Free up disk space and try again."},
    Entry{u"SAVEGAME_ERROR_DISKFULL", u"There is not enough free disk space to save.\nFree up disk space and try again."},
    Entry{u"SAVING_LONG", u"Saving. Please keep the game open."},
    Entry{u"SAVING_LIVE_DIALOG", u"Saving game data"},
    Entry{u"GO_TO_MARKETPLACE", u"Additional Content"},
    Entry{u"UPSELL_MARKETPLACE", u"Additional Content"},
    Entry{u"NOT_SIGNED_IN_MARKETPLACE", u"Online content purchases are unavailable in this PC build."},
    Entry{u"TRIAL_LOCKED_MESSAGE", u"Unlock the full game from the main menu to unearth all that is wonderful about Plants vs. Zombies."},
    Entry{u"REGISTER", u"Unlock the full game from the main menu to unearth all that is wonderful about Plants vs. Zombies."},
    Entry{u"ACHIEVEMENT_EARNED_UPSELL_BODY", u"Achievement earned!"},
    Entry{u"HELP_TEXT_5_1", u"Welcome to your house! Crazy Dave will renovate it every so often as you get better at defending your lawn!"},
    Entry{u"UPSELL_QUIT_GAME", u"<B> Quit Game"},
    Entry{u"CREDITS_PAUSE_BODY", u"Resume to continue the credits.\n\nYou can also view the credits later\nfrom the options on the main menu."},
    Entry{u"UPSELL_PAUSE_BODY", u"Resume to continue."}};
  for (const auto& e : platform) if (key == e.key) { text = e.value; break; }
  // Select Second Player: player 2 joins with A (or Cross; the glyph
  // alternates while the window is open). Cancelling belongs to player 1:
  // RMB on the mouse, B on a pad. A mouse can also join a pad's game by LMB.
  if (key == u"2P_JOIN_TEXT" || key == u"2P_JOINED") {
    const auto swap = [&](std::u16string_view from, std::u16string_view to) {
      if (const auto at = text.find(from); at != std::u16string::npos) text.replace(at, from.size(), to);
    };
    if (mouse) { swap(u"Press <B> to cancel", u"Right-click to cancel"); return text; }
    swap(u"press <A>", u"press <A>\nor click the left mouse button");
    return text;
  }
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
    u"Point at a plant, then hold the right mouse button to dig!";
  // How to Play "Controls" shows our mouse picture: its leader lines end at
  // the LSTICK (left), B (right) and BUMPERS (top) labels; the rest is unused.
  if (key == u"HELP_TEXT_2_LSTICK") return u"LMB: Main Action";
  if (key == u"HELP_TEXT_2_B") return pc ? u"" : u"RMB: Secondary Action";
  if (key == u"HELP_TEXT_2_BUMPERS") return pc ? u"" : u"MMB: Third Action";
  if (key == u"HELP_TEXT_2_A" || key == u"HELP_TEXT_2_X" || key == u"HELP_TEXT_2_START") return {};
  if (key == u"ADVICE_HEAVY_WEAPON") return pc ? u"Move the mouse to drive and aim!\nHold LMB to fire. Click a seed packet to buy an upgrade!" : u"Move the mouse to drive and aim!\nHold LMB to fire. Wheel: Select upgrade; RMB: Buy.";
  if (key == u"ADVICE_SLOT_MACHINE_SPIN_AGAIN" || key == u"SLOT_MACHINE_PULL_TOOLTIP") return pc ? u"Click the lever to spin again!" : u"Scroll the wheel to spin again!";
  if (key == u"ADVICE_SLOT_MACHINE_PULL") return pc ? u"Click the lever to spin the slot machine!" : u"Scroll the wheel to spin the slot machine!";
  // Actions the original console text gives to X/Y, bumpers and sticks,
  // worded after the PC version where it has the same action.
  struct Action { std::u16string_view key, pc, console; };
  constexpr std::array actions{
    Action{u"ADVICE_BEGHOULED_DRAG_TO_MATCH_3", u"Click-and-drag plants to make matches of 3",
        u"Hold LMB on a plant and move the mouse to make matches of 3"},
    Action{u"ADVICE_BEGHOULED_TWIST_TO_MATCH_3", u"Click to twist plants to make matches of 3", {}},
    Action{u"ADVICE_ZOMBIQUARIUM_CLICK_TO_FEED", u"Click on the tank to feed the zombies", {}},
    Action{u"ADVICE_ZOMBIQUARIUM_BUY_SNORKEL", u"Click the seed packet to buy more snorkel zombies!",
        u"Click a seed packet, then right-click to buy more snorkel zombies!"},
    Action{u"ADVICE_ZEN_GARDEN_WATER_PLANT", u"Click on a sprout to water it!", {}},
    Action{u"ADVICE_USE_SHOVEL_ON_POTS", u"Click on a vase to see what's inside!", {}},
    Action{u"LAST_STAND_BEGIN_ONSLAUGHT", u"Click Start Onslaught to begin the onslaught!", {}},
    Action{u"ZEN_GARDEN_TUTORIAL_COMPLETE", u"Click Menu, then Exit Game, and choose Adventure to continue!", {}},
    Action{u"CLICK_TO_START", u"Click to Start!", {}},
    Action{u"CLICK_TO_RESUME", u"Click to resume game", {}},
    Action{u"ROLL_CREDITS", u"Roll Credits!", {}},
    Action{u"COB_CANNON_TOOLTIP", u"Click to launch deadly cobs of corn\n(requires 2 kernel-pults)", {}},
    Action{u"COB_CANNON_DESCRIPTION_HEADER", u"Click on the Cob Cannon to launch deadly cobs of corn.", {}},
    Action{u"UPSELL_BUY_GAME", u"Unlock Full Game", {}},
    Action{u"TREE_OF_WISDOM_5", u"Pssst! Turn on 'future' in PC Settings > Cheats to experience zombies... from the FUTURE!", {}},
    Action{u"TREE_OF_WISDOM_48", u"If you turn on 'trickedout' in PC Settings > Cheats, don't be surprised if you see something wacky happen to your lawnmowers.", {}},
    Action{u"TREE_OF_WISDOM_33", u"Please do not tap on the glass! Or actually, go ahead; right-click during Zombiquarium to deafen your underwater creatures.",
        u"Please do not tap on the glass! Or actually, go ahead; press the middle mouse button during Zombiquarium to deafen your underwater creatures."}};
  for (const auto& action : actions)
    if (key == action.key) return std::u16string(pc || action.console.empty() ? action.pc : action.console);
  const auto replace = [&](std::u16string_view from, std::u16string_view to) {
    size_t at = 0;
    while ((at = text.find(from, at)) != std::u16string::npos) {
      text.replace(at, from.size(), to); at += to.size();
    }
  };
  replace(u"<L><R>", pc ? u"LMB: " : u"Wheel: ");
  replace(u"<l>", u"the mouse"); replace(u"<r>", u"the mouse");
  // <X>/<Y> remain controller glyphs: those actions have no mouse button.
  text = MousePrompt(std::move(text), pc);
  replace(u"Escape", b.start);
  return text;
}
}
