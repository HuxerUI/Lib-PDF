#include <huxerui/huxerui.h>
#include <huxerui/pdf.h>
#include <pdf_demo_resources.h>
#include "app.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

using namespace huxerui;
using namespace huxerui::pdf;
using PdfRotation = huxerui::pdf::Rotation;

namespace {
struct PageSettings {
  int page = 0;
  float zoom = 1;
  PdfRotation rotation = PdfRotation::None;
  bool annotations = true;
};

struct PageData {
  PdfPageImage image;
  PageSize size;
  std::string text;
  std::size_t matches = 0;
};

struct DemoState {
  std::shared_ptr<PdfDocument> document;
  std::shared_ptr<const Bytes> pending_bytes;
  PageSettings settings;
  PageData page;
  std::string name;
  std::string title;
  std::string author;
  std::string query;
  std::string status = "Loading the sample document...";
  bool busy = true;
  bool needs_password = false;
};

PageData Prepare(const PdfDocument& document, PageSettings settings, const std::string& query) {
  auto text = document.Text(settings.page);
  if (text.size() > 12000) {
    std::size_t end = 12000;
    while ((static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) --end;
    text.resize(end);
    text += "\n\n[Text preview truncated]";
  }
  return {
    PreparePdfPage(document, settings.page, {
      .width = 570.0F * settings.zoom, .render_scale = 1.5F,
      .rotation = settings.rotation, .annotations = settings.annotations,
    }),
    document.Size(settings.page), std::move(text), document.Search(settings.page, query).size(),
  };
}

Task<void> LoadDocument(State<DemoState> model, std::shared_ptr<const Bytes> bytes, std::string name, std::string password = {}) {
  model = DemoState{.pending_bytes = bytes, .name = std::move(name), .status = "Opening document..."};
  try {
    auto loaded = co_await RunWorker([bytes, password = std::move(password)] {
      DemoState state;
      state.document = std::make_shared<PdfDocument>(PdfDocument::FromBytes(*bytes, {.password = password}));
      state.title = state.document->Metadata("Title");
      state.author = state.document->Metadata("Author");
      state.page = Prepare(*state.document, state.settings, {});
      state.status = "Ready";
      state.busy = false;
      return state;
    });
    loaded.name = model.Get().name;
    model = std::move(loaded);
  } catch (const PdfError& error) {
    model.Update([&](DemoState& state) {
      state.busy = false;
      state.needs_password = error.Code() == PdfErrorCode::Password;
      state.status = state.needs_password ? "This PDF needs a password. Enter it below and choose Unlock." : error.what();
      if (!state.needs_password) state.pending_bytes.reset();
    });
  } catch (const std::exception& error) {
    model.Update([&](DemoState& state) { state.busy = false; state.pending_bytes.reset(); state.status = error.what(); });
  }
}

Task<void> LoadSample(State<DemoState> model, RawAsset asset) {
  model.Update([](DemoState& state) { state.busy = true; state.status = "Loading sample..."; });
  try {
    auto bytes = co_await RunWorker([asset] { return std::make_shared<const Bytes>(asset.ReadBytes()); });
    co_await LoadDocument(model, std::move(bytes), "HuxerUI PDF field guide.pdf");
  } catch (const std::exception& error) {
    model.Update([&](DemoState& state) { state.busy = false; state.status = error.what(); });
  }
}

Task<void> OpenDocument(State<DemoState> model, std::shared_ptr<FilePicker> picker) {
  model.Update([](DemoState& state) { state.busy = true; state.status = "Choose a PDF document..."; });
  try {
    auto file = co_await picker->OpenFileAsync({.name = "PDF documents", .extensions = {"pdf"}, .content_types = {"application/pdf"}});
    if (!file) {
      model.Update([](DemoState& state) { state.busy = false; state.status = "Open canceled"; });
      co_return;
    }
    if (const auto size = file->Size(); size && *size > OpenOptions{}.max_bytes)
      throw std::invalid_argument("This file exceeds the 256 MiB input limit");
    auto result = co_await file->ReadBytesAsync();
    if (!result.Succeeded()) throw std::runtime_error(result.Error().message);
    auto bytes = std::make_shared<const Bytes>(std::move(result).Value());
    co_await LoadDocument(model, std::move(bytes), file->Name());
  } catch (const std::exception& error) {
    model.Update([&](DemoState& state) { state.busy = false; state.status = error.what(); });
  }
}

Task<void> RefreshPage(State<DemoState> model, PageSettings settings) {
  const auto document = model.Get().document;
  const auto query = model.Get().query;
  model.Update([](DemoState& state) { state.busy = true; state.status = "Preparing page..."; });
  try {
    auto page = co_await RunWorker([document, settings, query] { return Prepare(*document, settings, query); });
    model.Update([&](DemoState& state) {
      state.settings = settings;
      state.page = std::move(page);
      state.busy = false;
      state.status = "Ready";
    });
  } catch (const std::exception& error) {
    model.Update([&](DemoState& state) { state.busy = false; state.status = error.what(); });
  }
}

Task<void> SearchPage(State<DemoState> model, std::string query) {
  const auto document = model.Get().document;
  const int page = model.Get().settings.page;
  model.Update([](DemoState& state) { state.busy = true; state.status = "Searching this page..."; });
  try {
    const auto matches = co_await RunWorker([document, page, query] { return document->Search(page, query).size(); });
    model.Update([&](DemoState& state) {
      state.query = std::move(query);
      state.page.matches = matches;
      state.busy = false;
      state.status = "Search complete";
    });
  } catch (const std::exception& error) {
    model.Update([&](DemoState& state) { state.busy = false; state.status = error.what(); });
  }
}

[[huxerui::composable]] View Workspace(State<bool> dark) {
  auto model = UseState(DemoState{});
  auto query = UseState(TextEditingValue::FromText("PDF"));
  auto password = UseState(TextEditingValue{});
  auto show_text = UseState(false);
  const auto tasks = UseTaskScope();
  const auto picker = UseService<FilePicker>();
  const auto sample = UseRawResource(pdf_demo::raw::guide_pdf);
  Lifecycle([=] {
    const auto request = tasks.Launch(LoadSample(model, sample));
    return [request] { request.Cancel(); };
  });
  const auto& state = model.Get();
  const auto& theme = UseTheme();
  const bool ready = state.document && !state.busy;
  const auto change_page = [model, tasks](PageSettings next) { tasks.Launch(RefreshPage(model, next)); };
  const auto search = [=] { if (model.Get().document && !model.Get().busy) tasks.Launch(SearchPage(model, query.Get().text)); };

  View page = state.page.image.image.HasValue() ? PdfPage(state.page.image) :
      Text(state.busy ? "Preparing your document..." : "Open a PDF to get started.").With(Padding(32.0F));
  // Materialize as View so the outer ScrollView wraps it instead of moving the same ScrollView.
  View horizontal_page = ScrollView(std::move(page)).ScrollAxis(Axis::Horizontal).With(ScrollBar{});
  View details = Column{
    Text("DOCUMENT", TextRole::Label).With(Foreground(theme.colors.primary)),
    Text(state.title.empty() ? state.name : state.title, TextRole::Title),
    Text(state.author.empty() ? "Author not specified" : "By " + state.author),
    Text(state.document ? std::to_string(state.document->PageCount()) + " pages" : "No document loaded"),
    Text(state.document ? std::to_string(static_cast<int>(state.page.size.width)) + " x " +
        std::to_string(static_cast<int>(state.page.size.height)) + " pt" : ""),
    Text("Rotation: " + std::to_string(static_cast<int>(state.settings.rotation) * 90) + "°"),
    Text("SEARCH THIS PAGE", TextRole::Label).With(Foreground(theme.colors.primary)),
    TextField(query).Label("Find text").Placeholder("Search this page")
        .OnChanged([query](const TextEditingValue& value) { query = value; })
        .OnSubmitted(search).With(Enabled{ready}),
    Button("Search").OnClick(search).With(Enabled{ready}),
    Text(state.query.empty() ? "Enter a query to find text." : std::to_string(state.page.matches) +
        " matches for \"" + state.query + "\" on this page."),
    Text("Use the page controls to continue your search on another page.", TextRole::Label),
  }.With(Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch));
  View inspector = show_text.Get() ?
      SelectionArea(Text(state.page.text.empty() ? "No extractable text on this page." : state.page.text)).Key("page-text-preview") : details;

  View unlock = state.needs_password ? Row{
    TextField(password).Label("Document password").Secure()
        .OnChanged([password](const TextEditingValue& value) { password = value; }).With(Grow{}, Enabled{!state.busy}),
    Button("Unlock").OnClick([=] {
      auto secret = password.Get().text;
      password = TextEditingValue{};
      tasks.Launch(LoadDocument(model, model.Get().pending_bytes, model.Get().name, std::move(secret)));
    }).With(Enabled{!state.busy}),
  }.With(Spacing(12.0F)) : View{};

  return Column{
    Row{
      Column{Text(pdf_demo::strings::title, TextRole::Title), Text(pdf_demo::strings::subtitle, TextRole::Label)}
          .With(Grow{}, Spacing(4.0F)),
      Button("Open PDF").OnClick([=] { tasks.Launch(OpenDocument(model, picker)); })
          .With(Enabled{!state.busy && picker->CanOpenFiles()}),
      Button("Load sample").OnClick([=] { tasks.Launch(LoadSample(model, sample)); }).With(Enabled{!state.busy}),
      Button(dark.Get() ? "Light theme" : "Dark theme").OnClick([dark] { dark = !dark.Get(); }),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
    Flow{
      Button("Previous").OnClick([=] { auto next = state.settings; --next.page; change_page(next); })
          .With(Enabled{ready && state.settings.page > 0}),
      Text(state.document ? "Page " + std::to_string(state.settings.page + 1) + " / " + std::to_string(state.document->PageCount()) : "Page - / -"),
      Button("Next").OnClick([=] { auto next = state.settings; ++next.page; change_page(next); })
          .With(Enabled{ready && state.settings.page + 1 < state.document->PageCount()}),
      Button("Zoom out").OnClick([=] { auto next = state.settings; next.zoom = std::max(0.25F, next.zoom / 1.25F); change_page(next); })
          .With(Enabled{ready && state.settings.zoom > 0.25F}),
      Button(std::to_string(static_cast<int>(state.settings.zoom * 100 + 0.5F)) + "%").OnClick([=] {
        auto next = state.settings; next.zoom = 1; change_page(next);
      }).With(Enabled{ready}),
      Button("Zoom in").OnClick([=] { auto next = state.settings; next.zoom = std::min(3.0F, next.zoom * 1.25F); change_page(next); })
          .With(Enabled{ready && state.settings.zoom < 3.0F}),
      Button("Rotate 90°").OnClick([=] {
        auto next = state.settings; next.rotation = static_cast<PdfRotation>((static_cast<int>(next.rotation) + 1) % 4); change_page(next);
      }).With(Enabled{ready}),
      Button(state.settings.annotations ? "Annotations: on" : "Annotations: off").OnClick([=] {
        auto next = state.settings; next.annotations = !next.annotations; change_page(next);
      }).With(Enabled{ready}),
    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
    unlock,
    Row{
      ScrollView(std::move(horizontal_page))
          .With(Grow{}, ScrollBar{}, Background(theme.colors.surface_container_low), CornerRadius(12.0F)),
      Column{
        Row{
          Button("Document").OnClick([show_text] { show_text = false; }),
          Button("Page text").OnClick([show_text] { show_text = true; }),
        }.With(Spacing(8.0F)),
        ScrollView(std::move(inspector)).With(Grow{}, ScrollBar{}),
      }.With(Frame{.width = 300.0F}, Padding(18.0F), Spacing(18.0F), Background(theme.colors.surface), CornerRadius(12.0F),
          CrossAlign(CrossAxisAlignment::Stretch)),
    }.With(Grow{}, Spacing(18.0F), CrossAlign(CrossAxisAlignment::Stretch)),
    Text((state.busy ? "Working · " : "") + state.status + (state.name.empty() ? "" : "  |  " + state.name), TextRole::Label),
  }.With(Padding(24.0F), Spacing(18.0F), CrossAlign(CrossAxisAlignment::Stretch), Background(theme.colors.background));
}

View App() {
  auto dark = UseState(false);
  return MaterialTheme(dark.Get() ? MaterialDarkThemeSpec() : MaterialLightThemeSpec(), Scope([dark] { return Workspace(dark); }));
}
} // namespace

const Application application{App, {
  .window = {.title = "PDF Preview", .initial_size = {1280.0F, 860.0F}, .minimum_size = Size{960.0F, 640.0F}},
  .show_debug_overlay = false,
}};

const huxerui::Application& PdfPreviewApplication() { return application; }
