#include <huxerui/pdf/document.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <mutex>
#include <fpdf_doc.h>
#include <fpdf_text.h>
#include <fpdfview.h>
#include <cpp/fpdf_scopers.h>
#include "unicode.h"

namespace huxerui::pdf {
namespace {
struct Runtime {
  std::mutex mutex;
  Runtime() {
    FPDF_LIBRARY_CONFIG config{};
    config.version = 2;
    FPDF_InitLibraryWithConfig(&config);
  }
  ~Runtime() { FPDF_DestroyLibrary(); }
};

std::shared_ptr<Runtime> GetRuntime() {
  static const auto runtime = std::make_shared<Runtime>();
  return runtime;
}

[[noreturn]] void Fail(const char* operation) {
  const auto error = FPDF_GetLastError();
  auto code = PdfErrorCode::Unknown;
  switch (error) {
    case FPDF_ERR_FILE: code = PdfErrorCode::File; break;
    case FPDF_ERR_FORMAT: code = PdfErrorCode::Format; break;
    case FPDF_ERR_PASSWORD: code = PdfErrorCode::Password; break;
    case FPDF_ERR_SECURITY: code = PdfErrorCode::Security; break;
    case FPDF_ERR_PAGE: code = PdfErrorCode::Page; break;
    default: break;
  }
  throw PdfError(code, std::string(operation) + " (PDFium error " + std::to_string(error) + ")");
}

void CheckInput(std::size_t size, const OpenOptions& options) {
  if (options.max_bytes == 0 || size > options.max_bytes)
    throw std::invalid_argument("PDF exceeds the input byte limit");
  if (options.password.find('\0') != std::string::npos)
    throw std::invalid_argument("PDF password contains NUL");
}

ScopedFPDFPage LoadPage(FPDF_DOCUMENT doc, int index) {
  ScopedFPDFPage page(FPDF_LoadPage(doc, index));
  if (!page) Fail("Cannot load PDF page");
  return page;
}

ScopedFPDFTextPage LoadText(FPDF_PAGE page) {
  ScopedFPDFTextPage text(FPDFText_LoadPage(page));
  if (!text) Fail("Cannot load PDF text");
  return text;
}
} // namespace

struct PdfDocument::Impl {
  std::shared_ptr<Runtime> runtime = GetRuntime();
  std::vector<std::byte> bytes;
  FPDF_DOCUMENT document = nullptr;
  int pages = 0;
  ~Impl() {
    std::lock_guard lock(runtime->mutex);
    if (document) FPDF_CloseDocument(document);
  }
};

PdfDocument PdfDocument::Open(const std::filesystem::path& path, OpenOptions options) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) throw PdfError(PdfErrorCode::File, "Cannot open PDF file");
  const auto end = stream.tellg();
  if (end < 0 || static_cast<std::uintmax_t>(end) > std::numeric_limits<std::size_t>::max()
      || static_cast<std::uintmax_t>(end) > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
    throw PdfError(PdfErrorCode::File, "Cannot determine PDF file size");
  const auto size = static_cast<std::size_t>(end);
  CheckInput(size, options);
  std::vector<std::byte> bytes(size);
  stream.seekg(0);
  if (size && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
    throw PdfError(PdfErrorCode::File, "Cannot read complete PDF file");
  return FromOwnedBytes(std::move(bytes), options);
}

PdfDocument PdfDocument::FromBytes(std::span<const std::byte> bytes, OpenOptions options) {
  CheckInput(bytes.size(), options);
  return FromOwnedBytes({bytes.begin(), bytes.end()}, options);
}

PdfDocument PdfDocument::FromOwnedBytes(std::vector<std::byte> bytes, const OpenOptions& options) {
  if (bytes.empty()) throw PdfError(PdfErrorCode::Format, "Empty PDF input");
  auto impl = std::make_shared<Impl>();
  impl->bytes = std::move(bytes);
  std::lock_guard lock(impl->runtime->mutex);
  impl->document = FPDF_LoadMemDocument64(impl->bytes.data(), impl->bytes.size(),
      options.password.empty() ? nullptr : options.password.c_str());
  if (!impl->document) Fail("Cannot open PDF document");
  impl->pages = FPDF_GetPageCount(impl->document);
  if (impl->pages <= 0) throw PdfError(PdfErrorCode::Page, "PDF has no readable pages");
  return PdfDocument(std::move(impl));
}

int PdfDocument::PageCount() const noexcept { return impl_ ? impl_->pages : 0; }

void PdfDocument::CheckPage(int page) const {
  if (page < 0 || page >= PageCount()) throw std::out_of_range("PDF page index is out of range");
}

PageSize PdfDocument::Size(int page) const {
  CheckPage(page);
  std::lock_guard lock(impl_->runtime->mutex);
  FS_SIZEF size{};
  if (!FPDF_GetPageSizeByIndexF(impl_->document, page, &size)) Fail("Cannot read PDF page size");
  if (!std::isfinite(size.width) || !std::isfinite(size.height) || size.width <= 0 || size.height <= 0)
    throw PdfError(PdfErrorCode::Page, "Invalid PDF page dimensions");
  return {size.width, size.height};
}

std::string PdfDocument::Metadata(std::string_view tag) const {
  if (!impl_) throw std::logic_error("Moved-from PDF document");
  if (tag.empty() || tag.find('\0') != std::string_view::npos) throw std::invalid_argument("Invalid metadata tag");
  const std::string key(tag);
  std::lock_guard lock(impl_->runtime->mutex);
  const auto size = FPDF_GetMetaText(impl_->document, key.c_str(), nullptr, 0);
  if (size <= 2) return {};
  if (size > 16U * 1024U * 1024U || size % 2) throw PdfError(PdfErrorCode::Format, "Invalid PDF metadata size");
  std::vector<unsigned short> buffer(size / 2);
  if (FPDF_GetMetaText(impl_->document, key.c_str(), buffer.data(), size) != size)
    throw PdfError(PdfErrorCode::Format, "Cannot read PDF metadata");
  return detail::FromUtf16(buffer.data(), buffer.size() - 1);
}

PdfBitmap PdfDocument::Render(int index, RenderOptions options) const {
  CheckPage(index);
  const int rotation = static_cast<int>(options.rotation);
  if (options.width <= 0 || options.width > 32768 || rotation < 0 || rotation > 3
      || options.background_rgb > 0xFFFFFF || options.max_pixels == 0)
    throw std::invalid_argument("Invalid PDF render options");
  const auto size = Size(index);
  const auto ratio = rotation % 2 ? size.width / size.height : size.height / size.width;
  const auto height = std::ceil(options.width * ratio);
  if (!std::isfinite(height) || height < 1 || height > 32768
      || height * options.width > static_cast<double>(std::min<std::size_t>(options.max_pixels, 64U * 1024U * 1024U)))
    throw std::invalid_argument("PDF render exceeds the pixel limit");
  PdfBitmap result{options.width, static_cast<int>(height), {}};
  result.pixels.resize(static_cast<std::size_t>(result.width) * result.height * 4);
  std::lock_guard lock(impl_->runtime->mutex);
  const auto page = LoadPage(impl_->document, index);
  ScopedFPDFBitmap bitmap(FPDFBitmap_CreateEx(result.width, result.height, FPDFBitmap_BGRA,
      result.pixels.data(), result.width * 4));
  if (!bitmap) throw PdfError(PdfErrorCode::Unknown, "Cannot allocate PDF bitmap");
  FPDFBitmap_FillRect(bitmap.get(), 0, 0, result.width, result.height, 0xFF000000U | options.background_rgb);
  FPDF_RenderPageBitmap(bitmap.get(), page.get(), 0, 0, result.width, result.height, rotation,
      options.annotations ? FPDF_ANNOT : 0);
  for (std::size_t i = 0; i < result.pixels.size(); i += 4) std::swap(result.pixels[i], result.pixels[i + 2]);
  return result;
}

std::string PdfDocument::Text(int index) const {
  CheckPage(index);
  std::lock_guard lock(impl_->runtime->mutex);
  const auto page = LoadPage(impl_->document, index);
  const auto text = LoadText(page.get());
  const int count = FPDFText_CountChars(text.get());
  if (count < 0 || count > 16 * 1024 * 1024) throw PdfError(PdfErrorCode::Page, "Invalid PDF text size");
  std::vector<unsigned short> buffer(static_cast<std::size_t>(count) * 2 + 1);
  const int written = FPDFText_GetText(text.get(), 0, count, buffer.data());
  if (written < 0 || static_cast<std::size_t>(written) > buffer.size()) throw PdfError(PdfErrorCode::Page, "Cannot extract PDF text");
  return written > 0 ? detail::FromUtf16(buffer.data(), static_cast<std::size_t>(written) - 1) : std::string{};
}

std::vector<SearchMatch> PdfDocument::Search(int index, std::string_view query, SearchOptions options) const {
  CheckPage(index);
  if (query.size() > 1024U * 1024U || options.max_results > 1000000)
    throw std::invalid_argument("PDF search exceeds query or result limit");
  const auto wide = detail::ToUtf16(query);
  if (query.empty() || options.max_results == 0) return {};
  std::lock_guard lock(impl_->runtime->mutex);
  const auto page = LoadPage(impl_->document, index);
  const auto text = LoadText(page.get());
  const unsigned long flags = (options.match_case ? FPDF_MATCHCASE : 0) | (options.whole_word ? FPDF_MATCHWHOLEWORD : 0);
  ScopedFPDFTextFind search(FPDFText_FindStart(text.get(), wide.data(), flags, 0));
  if (!search) throw PdfError(PdfErrorCode::Page, "Cannot start PDF text search");
  std::vector<SearchMatch> matches;
  while (matches.size() < options.max_results && FPDFText_FindNext(search.get())) {
    SearchMatch match{FPDFText_GetSchResultIndex(search.get()), FPDFText_GetSchCount(search.get()), {}};
    const int rects = FPDFText_CountRects(text.get(), match.start, match.count);
    for (int i = 0; i < rects; ++i) {
      TextRect rect;
      if (FPDFText_GetRect(text.get(), i, &rect.left, &rect.top, &rect.right, &rect.bottom)) match.bounds.push_back(rect);
    }
    matches.push_back(std::move(match));
  }
  return matches;
}
} // namespace huxerui::pdf
