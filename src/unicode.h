#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace huxerui::pdf::detail {
inline void AppendUtf8(std::string& out, std::uint32_t cp) {
  if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
  if (cp < 0x80) out += static_cast<char>(cp);
  else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 63));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 63));
    out += static_cast<char>(0x80 | (cp & 63));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 63));
    out += static_cast<char>(0x80 | ((cp >> 6) & 63));
    out += static_cast<char>(0x80 | (cp & 63));
  }
}

inline std::string FromUtf16(const unsigned short* text, std::size_t size) {
  std::string out;
  for (std::size_t i = 0; i < size; ++i) {
    std::uint32_t cp = text[i];
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < size && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) {
      cp = 0x10000 + ((cp - 0xD800) << 10) + (text[++i] - 0xDC00);
    }
    AppendUtf8(out, cp);
  }
  return out;
}

inline std::vector<unsigned short> ToUtf16(std::string_view text) {
  std::vector<unsigned short> out;
  for (std::size_t i = 0; i < text.size();) {
    auto next = [&]() { return static_cast<unsigned char>(text[i++]); };
    std::uint32_t cp = next();
    int extra = 0;
    std::uint32_t minimum = 0;
    if (cp >= 0xC2 && cp <= 0xDF) { cp &= 31; extra = 1; minimum = 0x80; }
    else if (cp >= 0xE0 && cp <= 0xEF) { cp &= 15; extra = 2; minimum = 0x800; }
    else if (cp >= 0xF0 && cp <= 0xF4) { cp &= 7; extra = 3; minimum = 0x10000; }
    else if (cp >= 0x80 || cp == 0) throw std::invalid_argument("Query must be valid UTF-8 without NUL");
    for (int n = 0; n < extra; ++n) {
      if (i == text.size()) throw std::invalid_argument("Truncated UTF-8 query");
      const auto b = next();
      if ((b & 0xC0) != 0x80) throw std::invalid_argument("Invalid UTF-8 query");
      cp = (cp << 6) | (b & 63);
    }
    if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
      throw std::invalid_argument("Invalid UTF-8 scalar in query");
    if (cp < 0x10000) out.push_back(static_cast<unsigned short>(cp));
    else {
      cp -= 0x10000;
      out.push_back(static_cast<unsigned short>(0xD800 + (cp >> 10)));
      out.push_back(static_cast<unsigned short>(0xDC00 + (cp & 1023)));
    }
  }
  out.push_back(0);
  return out;
}
} // namespace huxerui::pdf::detail
