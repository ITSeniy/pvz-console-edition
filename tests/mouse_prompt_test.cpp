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
  check(pvz::MousePrompt(u"then press and hold <B> to dig!", false) == u"then click RMB to dig!");
  const std::u16string join = u"Player 2: Press <A> to join. Press <B> to cancel.";
  check(pvz::PcGameText(u"2P_JOIN_TEXT", join, true, true) == join);
  check(pvz::PcGameText(u"RECONNECT_SECOND_CONTROLLER_FMT", u"Controller %d", true, true) == u"Controller %d");
  check(pvz::PcGameText(u"ADVICE_CLICK_SEED_PACKET", u"<L><R>", false, true) == u"<L><R>");
  check(pvz::PcGameText(u"ADVICE_CLICK_SEED_PACKET", u"<L><R>", true, true).find(u"Click") != std::u16string::npos);
  check(pvz::PcGameText(u"ADVICE_CLICK_SEED_PACKET", u"<L><R>", true, false).find(u"wheel") != std::u16string::npos);
  pvz::TextBindings custom{u"F", u"G", u"Tab", u"4", u"5", u"6", u"7"};
  check(pvz::PcGameText(u"TIP", u"{NO_SOUND}Press <X>, <Y>, <S>: %d", true, true, custom) == u"{NO_SOUND}Press F, G, Tab: %d");
  check(pvz::PcGameText(u"TREE_OF_WISDOM_48", u"LB RB LT RT: trickedout", true, true, custom) == u"4 5 6 7: trickedout");
  check(pvz::PcGameText(u"SAVING_LONG", u"console", false, false).find(u"console") == std::u16string::npos);
  check(pvz::PcGameText(u"PLANT_DESCRIPTION", plain, true, true) == plain);
  std::cout << "mouse prompt checks passed\n";
}
