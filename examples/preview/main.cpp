#include <huxerui/huxerui.h>
#include <huxerui/pdf.h>
#include <iostream>

using namespace huxerui;
using namespace huxerui::pdf;

std::shared_ptr<PdfDocument> document;

View App() {
  auto dark = UseState(false);
  return MaterialTheme(dark.Get() ? MaterialDarkThemeSpec() : MaterialLightThemeSpec(), Scope([=] {
    return Column{
      Row{
        Text("Lib-PDF", TextRole::Title).With(Grow{}),
        Button(dark.Get() ? "Light theme" : "Dark theme").OnClick([dark] { dark = !dark.Get(); }),
      }.With(Spacing(12.0F)),
      Text(document->Metadata("Title")),
      PdfViewer(*document).With(Grow{}),
    }.With(Padding(20.0F), Spacing(16.0F), CrossAlign(CrossAxisAlignment::Stretch), Background{UseTheme().colors.background});
  }));
}

const Application application{App, {.window = {.title = "HuxerUI PDF", .initial_size = {1000.0F, 850.0F}}}};

int main(int argc, char** argv) {
  try {
    document = std::make_shared<PdfDocument>(PdfDocument::Open(argc > 1 ? std::filesystem::u8path(argv[1]) : std::filesystem::path(PDF_SAMPLE_PATH)));
    return RunApplication();
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
