#pragma once
#include <string>
#include <string_view>

namespace pvz {
// Adapt action markup, preserving localization and unrelated text. The caller
// substitutes this label only while drawing; controller labels stay intact.
inline std::u16string MousePrompt(std::u16string text, bool pc_mode = false) {
  const auto replace = [&](std::u16string_view from, std::u16string_view to) {
    size_t at = 0;
    while ((at = text.find(from, at)) != std::u16string::npos) {
      text.replace(at, from.size(), to);
      at += to.size();
    }
  };
  replace(u"press and hold <B> to dig", pc_mode ? u"click LMB to dig" : u"click RMB to dig");
  replace(u"Press <A>", u"Left-click");
  replace(u"Press <B>", u"Right-click");
  replace(u"<A>", u"LMB");
  replace(u"<B>", u"RMB");
  replace(u"<S>", u"Escape");
  return text;
}
}
