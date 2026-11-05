#include <huxerui/pdf.h>
#include <huxerui/huxerui.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>

namespace huxerui::pdf {
namespace {
bool Positive(float value) { return std::isfinite(value) && value > 0; }

std::string ZoomLabel(float factor) {
  std::ostringstream label;
  label << std::fixed << std::setprecision(0) << static_cast<double>(factor) * 100 << '%';
  return label.str();
}

struct ViewerCache {
  std::optional<PdfDocument> document;
  int page = -1;
  float width = 0;
  float scale = 0;
  PdfPageImage image;
};
} // namespace

PdfPageImage PreparePdfPage(const PdfDocument& document, int page, PdfPageOptions options) {
  if (!Positive(options.width) || !Positive(options.render_scale))
    throw std::invalid_argument("PDF page width and render scale must be finite and positive");
  const double pixels = std::ceil(static_cast<double>(options.width) * options.render_scale);
  if (pixels > 32768) throw std::invalid_argument("PDF page exceeds the raster width limit");
  const auto bitmap = document.Render(page, {
    .width = static_cast<int>(pixels), .rotation = options.rotation,
    .annotations = options.annotations, .background_rgb = options.background_rgb,
  });
  return {ImageAsset::FromEncoded(bitmap.ToPng(), static_cast<float>(pixels / options.width))};
}

View PdfPage(const PdfPageImage& page) {
  if (!page.image.HasValue()) throw std::invalid_argument("PDF page image is empty");
  const auto size = page.image.IntrinsicSize();
  return Image(page.image).Fit(ImageFit::Contain).With(Frame{.width = size.width, .height = size.height});
}

View PdfPage(const PdfDocument& document, int page, PdfPageOptions options) {
  return PdfPage(PreparePdfPage(document, page, options));
}

View PdfViewer(PdfDocument document, PdfViewerOptions options) {
  if (document.PageCount() <= 0 || options.initial_page < 0 || options.initial_page >= document.PageCount())
    throw std::out_of_range("PDF viewer initial page is out of range");
  if (!Positive(options.page_width) || !Positive(options.render_scale) || !Positive(options.min_zoom)
      || !Positive(options.max_zoom) || !Positive(options.initial_zoom)
      || options.min_zoom > options.max_zoom || options.initial_zoom < options.min_zoom
      || options.initial_zoom > options.max_zoom
      || static_cast<double>(options.page_width) * options.max_zoom * options.render_scale > 32768)
    throw std::invalid_argument("Invalid PDF viewer width, scale, or zoom range");
  return Scope([document = std::move(document), options] {
    auto page = UseState(options.initial_page);
    auto zoom = UseState(options.initial_zoom);
    auto cache = UseState(std::make_shared<ViewerCache>()).Get();
    const int index = std::clamp(page.Get(), 0, document.PageCount() - 1);
    const float factor = std::clamp(zoom.Get(), options.min_zoom, options.max_zoom);
    const float width = options.page_width * factor;
    View content;
    try {
      if (!cache->document || *cache->document != document || cache->page != index
          || cache->width != width || cache->scale != options.render_scale) {
        auto image = PreparePdfPage(document, index, {.width = width, .render_scale = options.render_scale});
        *cache = {document, index, width, options.render_scale, std::move(image)};
      }
      content = PdfPage(cache->image);
    } catch (const std::exception& error) {
      content = Text(std::string("Cannot render PDF page: ") + error.what());
    }
    // Passing a ScrollView directly would select its move constructor rather than nest a viewport.
    View horizontal_page = ScrollView(std::move(content)).ScrollAxis(Axis::Horizontal).With(ScrollBar{});
    return Column{
      Row{
        Button("Previous").OnClick([page, index] { page = index - 1; }).With(Enabled{index > 0}),
        Text("Page " + std::to_string(index + 1) + " / " + std::to_string(document.PageCount())),
        Button("Next").OnClick([page, index] { page = index + 1; }).With(Enabled{index + 1 < document.PageCount()}),
        Button("Zoom out").OnClick([zoom, factor, options] { zoom = std::max(options.min_zoom, factor / 1.25F); })
            .With(Enabled{factor > options.min_zoom}),
        Text(ZoomLabel(factor)),
        Button("Zoom in").OnClick([zoom, factor, options] { zoom = std::min(options.max_zoom, factor * 1.25F); })
            .With(Enabled{factor < options.max_zoom}),
      }.With(Spacing(8.0F)),
      ScrollView(std::move(horizontal_page))
          .With(Grow{}, ScrollBar{}),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Stretch));
  });
}
} // namespace huxerui::pdf
