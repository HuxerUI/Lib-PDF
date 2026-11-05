#pragma once

#include <huxerui/pdf/document.h>
#include <huxerui/resource.h>
#include <huxerui/view.h>

namespace huxerui::pdf {

struct PdfPageOptions {
  float width = 640.0F; ///< Logical width; wrap in a horizontal ScrollView if needed.
  float render_scale = 1.5F; ///< Raster pixels per logical unit.
  Rotation rotation = Rotation::None;
  bool annotations = true;
  std::uint32_t background_rgb = 0xFFFFFF;
};

/// An immutable encoded page image, safe to prepare on a worker and retain across recompositions.
struct PdfPageImage {
  ImageAsset image;
};

/// Synchronous rasterization and PNG encoding; may run on a worker.
[[nodiscard]] PdfPageImage PreparePdfPage(const PdfDocument& document, int page, PdfPageOptions options = {});
/// No document I/O or PDF rendering; displays a prepared page at its intrinsic logical size.
[[nodiscard]] View PdfPage(const PdfPageImage& page);
/// Synchronous convenience overload. Retain a prepared image for repeated composition.
[[nodiscard]] View PdfPage(const PdfDocument& document, int page, PdfPageOptions options = {});

struct PdfViewerOptions {
  float page_width = 640.0F;
  float render_scale = 1.5F;
  int initial_page = 0;
  float initial_zoom = 1.0F;
  float min_zoom = 0.25F;
  float max_zoom = 4.0F;
};

/// A bounded viewer with previous/next, zoom controls, two-axis scrolling, and one-page image caching.
/// Rasterization runs synchronously on page/zoom changes. Large-document apps should use
/// PreparePdfPage on their worker queue and PdfPage on the UI thread instead.
/// Give this component bounded height (for example .With(Grow{})). Initial values apply at mount.
[[nodiscard]] View PdfViewer(PdfDocument document, PdfViewerOptions options = {});

} // namespace huxerui::pdf
