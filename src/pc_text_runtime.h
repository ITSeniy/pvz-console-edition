#pragma once
#include "pc_locale.h"
#include <string>
#include <string_view>
namespace pvz {
std::u16string AdaptMouseMessage(std::u16string text, bool pc);
// The running language's LawnStrings entry, empty when it has none.
std::u16string GameString(std::u16string_view key);
// Text the port adds, in the running language: its own translation, else
// the game's localized `key` (when given), else English.
std::u16string PortText(const Localized& text, std::u16string_view key = {});
// The title and button of the second Mini-Games page.
inline constexpr Localized kBonusGamesText{u"Bonus Games", u"Bonusspiele", u"Juegos extra",
    u"Jeux bonus", u"Giochi bonus", u"ボーナスゲーム"};
}
