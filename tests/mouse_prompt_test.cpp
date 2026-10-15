#include "mouse_prompt.h"
#include "pc_text.h"
#include <iostream>
#include <cstdlib>

int main() {
  const auto check = [](bool value) { if (!value) std::exit(1); };
  check(pvz::MousePrompt(u"Press <A> on the grass to plant your seed!") ==
      u"Left-click on the grass to plant your seed!");
  check(pvz::MousePrompt(u"Press <B> to cancel; <A> selects, <S> pauses.") ==
      u"Right-click to cancel; LMB selects, Escape pauses.");
  check(pvz::MousePrompt(u"Нажмите <A>, затем <B>.") == u"Нажмите LMB, затем RMB.");
  const std::u16string plain = u"Keep on collecting sun!\nYou'll need it to grow more plants!";
  check(pvz::MousePrompt(plain) == plain);
  check(pvz::MousePrompt(u"<A><A><B><unknown>") == u"LMBLMBRMB<unknown>");
  check(pvz::MousePrompt(u"then press and hold <B> to dig!", true) == u"then click LMB to dig!");
  check(pvz::MousePrompt(u"then press and hold <B> to dig!", false) == u"then hold RMB to dig!");
  const std::u16string join = u"Player 2: Press <A> to join. Press <B> to cancel.";
  // Player 2 keeps the controller's A; player 1 cancels with the mouse.
  const std::u16string native_join = u"Player 2\r\nPlease turn on a controller and press <A>\r\n\r\nPress <B> to cancel";
  check(pvz::PcGameText(u"2P_JOIN_TEXT", native_join, true, true) ==
      u"Player 2\r\nPlease turn on a controller and press <A>\r\n\r\nRight-click to cancel");
  check(pvz::PcGameText(u"2P_JOIN_TEXT", native_join, false, true) ==
      u"Player 2\r\nPlease turn on a controller and press <A>\nor click the left mouse button\r\n\r\nPress <B> to cancel");
  check(pvz::PcGameText(u"2P_JOINED", u"Press <A> to continue.\r\n\r\nPress <B> to cancel.", true, false) ==
      u"Press <A> to continue.\r\n\r\nRight-click to cancel.");
  check(pvz::PcGameText(u"P2_JOIN", join, true, true) == join);
  check(pvz::PcGameText(u"RECONNECT_SECOND_CONTROLLER_FMT", u"Controller %d", true, true) == u"Controller %d");
  check(pvz::PcGameText(u"ADVICE_CLICK_SEED_PACKET", u"<L><R>", false, true) == u"<L><R>");
  check(pvz::PcGameText(u"ADVICE_CLICK_SEED_PACKET", u"<L><R>", true, true).find(u"Click") != std::u16string::npos);
  check(pvz::PcGameText(u"ADVICE_CLICK_SEED_PACKET", u"<L><R>", true, false).find(u"wheel") != std::u16string::npos);
  // The keyboard keeps only Start; X/Y stay controller glyphs.
  pvz::TextBindings custom{u"Tab"};
  check(pvz::PcGameText(u"TIP", u"{NO_SOUND}Press <X>, <Y>, <S>: %d", true, true, custom) == u"{NO_SOUND}Press <X>, <Y>, Tab: %d");
  check(pvz::PcGameText(u"TREE_OF_WISDOM_48", u"LB RB LT RT: trickedout", true, true).find(u"Cheats") != std::u16string::npos);
  check(pvz::PcGameText(u"ADVICE_BEGHOULED_DRAG_TO_MATCH_3", u"Hold <X>", true, true).find(u"drag") != std::u16string::npos);
  check(pvz::PcGameText(u"ADVICE_BEGHOULED_DRAG_TO_MATCH_3", u"Hold <X>", true, false).find(u"Hold LMB") != std::u16string::npos);
  check(pvz::PcGameText(u"ADVICE_BEGHOULED_DRAG_TO_MATCH_3", u"Hold <X>", false, true) == u"Hold <X>");
  check(pvz::PcGameText(u"SAVING_LONG", u"console", false, false).find(u"console") == std::u16string::npos);
  check(pvz::PcGameText(u"PLANT_DESCRIPTION", plain, true, true) == plain);
  std::cout << "mouse prompt checks passed\n";
}
