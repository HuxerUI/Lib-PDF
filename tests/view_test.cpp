#include <huxerui/pdf.h>
#include <huxerui/huxerui.h>
#include <limits>
#include "test.h"
#ifdef PDF_HAS_UI_TESTING
#include <huxerui/testing/ui_test.h>
#endif

using namespace huxerui;
using namespace huxerui::pdf;

int main() { return Run([] {
  const auto doc = PdfDocument::Open(std::filesystem::path(PDF_FIXTURE_DIR) / "sample.pdf");
  const auto image = PreparePdfPage(doc, 0, {.width = 300, .render_scale = 2});
  CHECK(image.image.PixelWidth() == 600 && image.image.PixelHeight() == 800);
  CHECK(image.image.IntrinsicSize().width == 300 && image.image.IntrinsicSize().height == 400);
  (void)PdfPage(image);
  Throws<std::invalid_argument>([] { (void)PdfPage(PdfPageImage{}); });
  Throws<std::invalid_argument>([&] { (void)PreparePdfPage(doc, 0, {.width = 0}); });
  Throws<std::invalid_argument>([&] { (void)PreparePdfPage(doc, 0, {.render_scale = std::numeric_limits<float>::infinity()}); });
  Throws<std::out_of_range>([&] { (void)PdfViewer(doc, {.initial_page = 2}); });
  Throws<std::invalid_argument>([&] { (void)PdfViewer(doc, {.initial_zoom = 0}); });
  Throws<std::invalid_argument>([&] { (void)PdfViewer(doc, {.min_zoom = 5, .max_zoom = 1}); });

#ifdef PDF_HAS_UI_TESTING
  using namespace huxerui::testing;
  Application app([]() -> View {
    static const auto root_doc = PdfDocument::Open(std::filesystem::path(PDF_FIXTURE_DIR) / "sample.pdf");
    return PdfViewer(root_doc, {.page_width = 300, .render_scale = 1});
  },
      {.window = {.initial_size = {900, 700}}, .show_debug_overlay = false});
  UiTest ui(app);
  CHECK(ui.Find(UiSelector::Text("Page 1 / 2")).Exists());
  CHECK(ui.Find(UiSelector::Text("100%")).Exists());
  ui.Find(UiSelector::Text("Next")).Tap();
  CHECK(ui.Find(UiSelector::Text("Page 2 / 2")).Exists());
  const auto disabled_next = ui.Find(UiSelector::Text("Next")).One();
  CHECK(!disabled_next.enabled);
  ui.TapAt({disabled_next.bounds.x + disabled_next.bounds.width / 2,
      disabled_next.bounds.y + disabled_next.bounds.height / 2});
  CHECK(ui.Find(UiSelector::Text("Page 2 / 2")).Exists());
  ui.Find(UiSelector::Text("Previous")).Tap();
  CHECK(ui.Find(UiSelector::Text("Page 1 / 2")).Exists());
  ui.Find(UiSelector::Text("Zoom in")).Tap();
  CHECK(ui.Find(UiSelector::Text("125%")).Exists());
  ui.Find(UiSelector::Text("Zoom out")).Tap();
  CHECK(ui.Find(UiSelector::Text("100%")).Exists());
  ui.PumpAndSettle();
#endif
}); }
