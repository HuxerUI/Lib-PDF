#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace huxerui::pdf {

enum class PdfErrorCode { Unknown, File, Format, Password, Security, Page };

class PdfError : public std::runtime_error {
public:
  PdfError(PdfErrorCode code, const std::string& message) : std::runtime_error(message), code_(code) {}
  [[nodiscard]] PdfErrorCode Code() const noexcept { return code_; }
private:
  PdfErrorCode code_;
};

struct OpenOptions {
  std::string password;
  std::size_t max_bytes = 256U * 1024U * 1024U;
};

/// Clockwise rotation applied in addition to the document's own page rotation.
enum class Rotation { None, Clockwise90, Clockwise180, Clockwise270 };

struct PageSize {
  double width = 0;  ///< PDF points (1/72 inch), including intrinsic page rotation.
  double height = 0;
};

struct RenderOptions {
  int width = 1024;  ///< Output pixel width; height preserves the rotated aspect ratio.
  Rotation rotation = Rotation::None;
  bool annotations = true;
  std::uint32_t background_rgb = 0xFFFFFF; ///< Opaque sRGB background, 0xRRGGBB.
  std::size_t max_pixels = 32U * 1024U * 1024U;
  bool operator==(const RenderOptions&) const = default;
};

/// Owned, top-down, tightly packed RGBA8 pixels. Alpha is always 255.
struct PdfBitmap {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> pixels;
  /// Lossless PNG with stored DEFLATE blocks; no image codec dependency.
  [[nodiscard]] std::vector<std::byte> ToPng() const;
};

struct SearchOptions {
  bool match_case = false;
  bool whole_word = false;
  std::size_t max_results = 1000;
};

struct TextRect {
  double left = 0, bottom = 0, right = 0, top = 0; ///< Unrotated PDF page coordinates.
};

struct SearchMatch {
  int start = 0;  ///< PDFium character index, not a UTF-8 byte or UTF-16 offset.
  int count = 0;
  std::vector<TextRect> bounds;
};

/// Immutable, copyable document handle. Copies share document ownership.
/// All PDFium calls made by this library are serialized, including destruction.
/// Do not call PDFium directly concurrently or initialize/destroy it independently.
class PdfDocument final {
public:
  /// Reads a native filesystem path, including Unicode paths on Windows.
  [[nodiscard]] static PdfDocument Open(const std::filesystem::path& path, OpenOptions options = {});
  /// Copies input before opening. The input span need not outlive this call.
  [[nodiscard]] static PdfDocument FromBytes(std::span<const std::byte> bytes, OpenOptions options = {});

  [[nodiscard]] int PageCount() const noexcept;
  /// All page indices are zero-based. Out-of-range indices throw std::out_of_range.
  [[nodiscard]] PageSize Size(int page) const;
  /// Common tags: Title, Author, Subject, Keywords, Creator, Producer, CreationDate.
  [[nodiscard]] std::string Metadata(std::string_view tag) const;
  [[nodiscard]] PdfBitmap Render(int page, RenderOptions options = {}) const;
  [[nodiscard]] std::string Text(int page) const;
  /// UTF-8 query, with PDFium's Unicode case/word matching semantics. No OCR.
  [[nodiscard]] std::vector<SearchMatch> Search(int page, std::string_view query, SearchOptions options = {}) const;
  bool operator==(const PdfDocument&) const noexcept = default;

private:
  struct Impl;
  explicit PdfDocument(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}
  std::shared_ptr<Impl> impl_;
  void CheckPage(int page) const;
  static PdfDocument FromOwnedBytes(std::vector<std::byte> bytes, const OpenOptions& options);
};

} // namespace huxerui::pdf
