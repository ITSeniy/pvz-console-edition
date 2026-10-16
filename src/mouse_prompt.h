#pragma once
#include "pc_locale.h"
#include <string>
#include <string_view>

namespace pvz {
// Adapt action markup, preserving localization and unrelated text. The caller
// substitutes this label only while drawing; controller labels stay intact.
// Each language words "press <A>" its own way; the phrases come from that
// language's LawnStrings, and any other glyph becomes a mouse button name.
inline std::u16string MousePrompt(std::u16string text, bool pc_mode = false,
    Language language = Language::En) {
  const auto replace = [&](std::u16string_view from, std::u16string_view to) {
    size_t at = 0;
    while ((at = text.find(from, at)) != std::u16string::npos) {
      text.replace(at, from.size(), to);
      at += to.size();
    }
  };
  // A label that starts with a glyph ("<B> Main Menu") names the button.
  const auto label = [&](std::u16string_view glyph, std::u16string_view to) {
    if (text.starts_with(glyph) && text.size() > glyph.size() && text[glyph.size()] == u' ')
      text.replace(0, glyph.size() + 1, to);
  };
  switch (language) {
  case Language::De:
    replace(u"Drücke <A>", u"Klicke"); replace(u"Drück <A>", u"Klick");
    replace(u"<A> drücken", u"klicken"); replace(u"<B> drücken", u"rechtsklicken");
    label(u"<A>", u"Linksklick: "); label(u"<B>", u"Rechtsklick: ");
    replace(u"<A>", u"Linksklick"); replace(u"<B>", u"Rechtsklick");
    break;
  case Language::Es:
    replace(u"Pulsa <A>", u"Haz clic"); replace(u"pulsa <A>", u"haz clic");
    replace(u"Pulsa <B>", u"Haz clic derecho"); replace(u"pulsa <B>", u"haz clic derecho");
    label(u"<A>", u"Clic izq.: "); label(u"<B>", u"Clic der.: ");
    replace(u"<A>", u"clic izquierdo"); replace(u"<B>", u"clic derecho");
    break;
  case Language::Fr:
    replace(u"Appuyez sur <A>", u"Cliquez"); replace(u"appuyez sur <A>", u"cliquez");
    replace(u"Appuyer sur <A>", u"Cliquer");
    replace(u"Appuyez sur <B>", u"Faites un clic droit"); replace(u"appuyez sur <B>", u"faites un clic droit");
    label(u"<A>", u"Clic gauche : "); label(u"<B>", u"Clic droit : ");
    replace(u"<A>", u"clic gauche"); replace(u"<B>", u"clic droit");
    break;
  case Language::It:
    replace(u"Premi <A>", u"Fai clic"); replace(u"premi <A>", u"fai clic");
    replace(u"Premi <B>", u"Fai clic destro"); replace(u"premi <B>", u"fai clic destro");
    label(u"<A>", u"Clic sinistro: "); label(u"<B>", u"Clic destro: ");
    replace(u"<A>", u"clic sinistro"); replace(u"<B>", u"clic destro");
    break;
  case Language::Jp:
    replace(u"<A>を押して", u"左クリックして"); replace(u"<A>を押し", u"左クリックし");
    replace(u"<A>を押す", u"左クリックする"); replace(u"<B>を押して", u"右クリックして");
    replace(u"<A>", u"左クリック"); replace(u"<B>", u"右クリック");
    break;
  case Language::En:
    replace(u"press and hold <B> to dig", pc_mode ? u"click LMB to dig" : u"hold RMB to dig");
    replace(u"Press <A>", u"Left-click");
    replace(u"Press <B>", u"Right-click");
    replace(u"<A>", u"LMB");
    replace(u"<B>", u"RMB");
    break;
  }
  replace(u"<S>", u"Escape");
  return text;
}
}
