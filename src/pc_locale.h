#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace pvz {
// The game languages, in pvz_language order (en, de, es, fr, it, jp).
enum class Language : uint8_t { En, De, Es, Fr, It, Jp };
// One string the port adds, in every game language. The game fonts have
// Latin-1 for the European languages (no curly quotes, ellipsis or
// guillemets) and only the kana and kanji the Japanese game itself uses.
struct Localized {
  std::u16string_view en, de, es, fr, it, jp;
  // The translation, empty when there is none.
  constexpr std::u16string_view In(Language language) const {
    return std::array{en, de, es, fr, it, jp}[size_t(language)];
  }
  // The translation, English when there is none.
  constexpr std::u16string_view operator()(Language language) const {
    const auto text = In(language);
    return text.empty() ? en : text;
  }
};
// ImGui draws UTF-8.
inline std::string Utf8(std::u16string_view text) {
  std::string out;
  for (size_t i = 0; i < text.size(); ++i) {
    uint32_t c = text[i];
    if (c >= 0xD800 && c < 0xDC00 && i + 1 < text.size())
      c = 0x10000 + ((c - 0xD800) << 10) + (text[++i] - 0xDC00);
    if (c < 0x80) out += char(c);
    else if (c < 0x800) { out += char(0xC0 | c >> 6); out += char(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) {
      out += char(0xE0 | c >> 12); out += char(0x80 | (c >> 6 & 0x3F)); out += char(0x80 | (c & 0x3F));
    } else {
      out += char(0xF0 | c >> 18); out += char(0x80 | (c >> 12 & 0x3F));
      out += char(0x80 | (c >> 6 & 0x3F)); out += char(0x80 | (c & 0x3F));
    }
  }
  return out;
}
}
