#include <huxerui/pdf/document.h>

#include <algorithm>
#include <array>

namespace huxerui::pdf {
namespace {
using Buffer = std::vector<std::byte>;
void BigEndian(Buffer& out, std::uint32_t value) {
  for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::byte>((value >> shift) & 255));
}
std::uint32_t Crc(std::span<const std::byte> bytes) {
  static const auto table = [] {
    std::array<std::uint32_t, 256> values{};
    for (std::uint32_t i = 0; i < 256; ++i) {
      auto c = i;
      for (int n = 0; n < 8; ++n) c = (c >> 1) ^ ((c & 1) ? 0xEDB88320U : 0);
      values[i] = c;
    }
    return values;
  }();
  std::uint32_t crc = 0xFFFFFFFFU;
  for (auto b : bytes) crc = (crc >> 8) ^ table[(crc ^ std::to_integer<unsigned>(b)) & 255];
  return ~crc;
}
void Chunk(Buffer& png, const char* type, std::span<const std::byte> data) {
  BigEndian(png, static_cast<std::uint32_t>(data.size()));
  const auto start = png.size();
  for (int i = 0; i < 4; ++i) png.push_back(static_cast<std::byte>(type[i]));
  png.insert(png.end(), data.begin(), data.end());
  BigEndian(png, Crc(std::span(png).subspan(start)));
}
} // namespace

std::vector<std::byte> PdfBitmap::ToPng() const {
  if (width <= 0 || height <= 0 || width > 32768 || height > 32768
      || static_cast<std::uint64_t>(width) * height > 64U * 1024U * 1024U
      || pixels.size() != static_cast<std::size_t>(width) * height * 4)
    throw std::invalid_argument("Invalid RGBA PDF bitmap");
  const auto row_size = static_cast<std::size_t>(width) * 4;
  Buffer raw;
  raw.reserve((row_size + 1) * height);
  for (int y = 0; y < height; ++y) {
    raw.push_back(std::byte{0}); // PNG filter: none.
    const auto row = pixels.data() + static_cast<std::size_t>(y) * row_size;
    const auto* begin = reinterpret_cast<const std::byte*>(row);
    raw.insert(raw.end(), begin, begin + row_size);
  }
  Buffer zlib{std::byte{0x78}, std::byte{0x01}};
  zlib.reserve(raw.size() + (raw.size() / 65535 + 1) * 5 + 6);
  for (std::size_t offset = 0; offset < raw.size();) {
    const auto size = std::min<std::size_t>(65535, raw.size() - offset);
    zlib.push_back(static_cast<std::byte>(offset + size == raw.size() ? 1 : 0));
    const auto length = static_cast<std::uint16_t>(size);
    const auto inverse = static_cast<std::uint16_t>(~length);
    zlib.push_back(static_cast<std::byte>(length & 255));
    zlib.push_back(static_cast<std::byte>(length >> 8));
    zlib.push_back(static_cast<std::byte>(inverse & 255));
    zlib.push_back(static_cast<std::byte>(inverse >> 8));
    zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
        raw.begin() + static_cast<std::ptrdiff_t>(offset + size));
    offset += size;
  }
  std::uint32_t a = 1, b = 0;
  for (auto value : raw) { a = (a + std::to_integer<unsigned>(value)) % 65521; b = (b + a) % 65521; }
  BigEndian(zlib, (b << 16) | a);
  Buffer png;
  for (int value : {137, 80, 78, 71, 13, 10, 26, 10}) png.push_back(static_cast<std::byte>(value));
  Buffer header;
  BigEndian(header, static_cast<std::uint32_t>(width));
  BigEndian(header, static_cast<std::uint32_t>(height));
  header.insert(header.end(), {std::byte{8}, std::byte{6}, std::byte{0}, std::byte{0}, std::byte{0}});
  Chunk(png, "IHDR", header);
  Chunk(png, "IDAT", zlib);
  Chunk(png, "IEND", {});
  return png;
}
} // namespace huxerui::pdf
