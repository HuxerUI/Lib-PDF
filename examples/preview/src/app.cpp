#include <huxerui/huxerui.h>
#include <huxerui/pdf.h>
#include <pdf_demo_resources.h>
#include "app.h"

#include <algorithm>

using namespace huxerui;
using namespace huxerui::pdf;

namespace {
Task<void> InspectPage(PdfSession session, int page, std::string query,
                      State<std::string> text, State<std::size_t> matches, State<std::string> notice) {
  auto extracted = co_await session.ReadTextAsync(page);
  if (!extracted.Succeeded()) { notice = extracted.Error().message; co_return; }
  auto preview = std::move(extracted).Value();
  if (preview.size() > 12000) {
    std::size_t end = 12000;
    while ((static_cast<unsigned char>(preview[end]) & 0xC0) == 0x80) --end;
    preview.resize(end);
    preview += "\n\n[Text preview truncated]";
  }
  text = std::move(preview);
  auto found = co_await session.SearchAsync(page, query);
  if (found.Succeeded()) { matches = found.Value().size(); notice = query.empty() ? "Ready" : "Search complete"; }
  else { notice = found.Error().message; }
}

Task<void> ChooseDocument(std::shared_ptr<FilePicker> picker, State<PdfSource> source, State<PdfOptions> options,
                          State<std::string> name, State<bool> choosing, State<std::string> notice) {
  choosing = true;
  try {
    auto selected = co_await picker->OpenFileAsync({.name = "PDF documents", .extensions = {"pdf"}, .content_types = {"application/pdf"}});
    if (selected) {
      name = selected->Name();
      source = PdfSource(*selected); // The library retains the grant and reads it with the input limit.
      options = PdfOptions{.page_width = 570};
      notice = "Opening document...";
    } else { notice = "Open canceled"; }
  } catch (const std::exception& error) { notice = error.what(); }
  choosing = false;
}

[[huxerui::composable]] View Workspace(State<bool> dark) {
  auto source = UseState(PdfSource(pdf_demo::raw::guide_pdf));
  auto options = UseState(PdfOptions{.page_width = 570});
  auto session = UsePdf(source, options);
  const auto snapshot = session.Snapshot();
  auto name = UseState(std::string("HuxerUI PDF field guide.pdf"));
  auto query = UseState(TextEditingValue::FromText("PDF"));
  auto applied_query = UseState(std::string{});
  auto password = UseState(TextEditingValue{});
  auto page_text = UseState(std::string{});
  auto matches = UseState(std::size_t{0});
  auto notice = UseState(std::string("Loading sample..."));
  auto choosing = UseState(false);
  auto show_text = UseState(false);
  const auto tasks = UseTaskScope();
  const auto picker = UseService<FilePicker>();
  const auto image = snapshot.output ? snapshot.output->image : ImageAsset{};
  const std::string requested_query = applied_query;

  Lifecycle([=] {
    TaskHandle request;
    if (snapshot.output && snapshot.status == PdfStatus::Ready)
      request = tasks.Launch(InspectPage(session, snapshot.output->page, requested_query, page_text, matches, notice));
    return [request] { request.Cancel(); };
  }, image, requested_query, snapshot.status);

  const PdfOptions intent = options;
  const bool pending_display = snapshot.output && (snapshot.output->page != intent.page || snapshot.output->zoom != intent.zoom
      || snapshot.output->rotation != intent.rotation || snapshot.output->annotations != intent.annotations);
  const bool busy = choosing || snapshot.status == PdfStatus::Loading || snapshot.status == PdfStatus::Rendering
      || (snapshot.status == PdfStatus::Ready && pending_display);
  const bool ready = snapshot.status == PdfStatus::Ready && !busy;
  const bool needs_password = snapshot.error && snapshot.error->code == PdfErrorCode::Password;
  const auto& theme = UseTheme();
  const auto search = [=] { if (ready) applied_query = query->text; };

  View page = snapshot.output ? PdfPage(session).With(Frame{.width = image.IntrinsicSize().width, .height = image.IntrinsicSize().height}) :
      Text(busy ? "Preparing your document..." : "Open a PDF to get started.").With(Padding(32.0F));
  View horizontal_page = ScrollView(std::move(page)).ScrollAxis(Axis::Horizontal).With(ScrollBar{});
  View details = Column{
    Text("DOCUMENT", TextRole::Label).With(Foreground(theme.colors.primary)),
    Text(snapshot.metadata && !snapshot.metadata->title.empty() ? snapshot.metadata->title : name, TextRole::Title),
    Text(snapshot.metadata && !snapshot.metadata->author.empty() ? "By " + snapshot.metadata->author : "Author not specified"),
    Text(snapshot.metadata ? std::to_string(snapshot.metadata->pages) + " pages" : "No document loaded"),
    Text(snapshot.output ? std::to_string(static_cast<int>(snapshot.output->size.width)) + " x " +
        std::to_string(static_cast<int>(snapshot.output->size.height)) + " pt" : ""),
    Text("Rotation: " + std::to_string(static_cast<int>(intent.rotation) * 90) + "°"),
    Text("SEARCH THIS PAGE", TextRole::Label).With(Foreground(theme.colors.primary)),
    TextField(query).Label("Find text").Placeholder("Search this page")
        .OnChanged([query](const TextEditingValue& value) { query = value; }).OnSubmitted(search).With(Enabled{ready}),
    Button("Search").OnClick(search).With(Enabled{ready}),
    Text(requested_query.empty() ? "Enter a query to find text." : std::to_string(matches) +
        " matches for \"" + requested_query + "\" on this page."),
    Text("Use the page controls to continue your search on another page.", TextRole::Label),
  }.With(Spacing(14.0F), CrossAlign(CrossAxisAlignment::Stretch));
  View inspector = show_text ?
      SelectionArea(Text(page_text->empty() ? "No extractable text on this page." : page_text.Get())).Key("page-text-preview") : details;

  View unlock = needs_password ? Row{
    TextField(password).Label("Document password").Secure()
        .OnChanged([password](const TextEditingValue& value) { password = value; }).With(Grow{}, Enabled{!busy}),
    Button("Unlock").OnClick([=] {
      options.Update([&](PdfOptions& next) { next.open.password = password->text; ++next.reload; });
      password = TextEditingValue{};
    }).With(Enabled{!busy}),
  }.With(Spacing(12.0F)) : View{};

  std::string status = snapshot.error ? snapshot.error->message : notice;
  if (snapshot.status == PdfStatus::Loading) status = "Opening document...";
  if (snapshot.status == PdfStatus::Rendering || pending_display) status = "Preparing page...";
  return Column{
    Row{
      Column{Text(pdf_demo::strings::title, TextRole::Title), Text(pdf_demo::strings::subtitle, TextRole::Label)}
          .With(Grow{}, Spacing(4.0F)),
      Button("Open PDF").OnClick([=] { tasks.Launch(ChooseDocument(picker, source, options, name, choosing, notice)); })
          .With(Enabled{!busy && picker->CanOpenFiles()}),
      Button("Load sample").OnClick([=] {
        source = PdfSource(pdf_demo::raw::guide_pdf);
        options = PdfOptions{.page_width = 570, .reload = options->reload + 1};
        name = "HuxerUI PDF field guide.pdf";
        applied_query = std::string{};
      }).With(Enabled{!busy}),
      Button(dark ? "Light theme" : "Dark theme").OnClick([dark] { dark = !dark; }),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
    Flow{
      Button("Previous").OnClick([=] { options.Update([](PdfOptions& next) { --next.page; }); })
          .With(Enabled{ready && intent.page > 0}),
      Text(snapshot.metadata ? "Page " + std::to_string(snapshot.output ? snapshot.output->page + 1 : 0) + " / " +
          std::to_string(snapshot.metadata->pages) : "Page - / -"),
      Button("Next").OnClick([=] { options.Update([](PdfOptions& next) { ++next.page; }); })
          .With(Enabled{ready && snapshot.metadata && intent.page + 1 < snapshot.metadata->pages}),
      Button("Zoom out").OnClick([=] { options.Update([](PdfOptions& next) { next.zoom = std::max(0.25F, next.zoom / 1.25F); }); })
          .With(Enabled{ready && intent.zoom > 0.25F}),
      Button(std::to_string(static_cast<int>(intent.zoom * 100 + 0.5F)) + "%").OnClick([=] {
        options.Update([](PdfOptions& next) { next.zoom = 1; });
      }).With(Enabled{ready}),
      Button("Zoom in").OnClick([=] { options.Update([](PdfOptions& next) { next.zoom = std::min(3.0F, next.zoom * 1.25F); }); })
          .With(Enabled{ready && intent.zoom < 3.0F}),
      Button("Rotate 90°").OnClick([=] { options.Update([](PdfOptions& next) {
        next.rotation = static_cast<PageRotation>((static_cast<int>(next.rotation) + 1) % 4);
      }); }).With(Enabled{ready}),
      Button(intent.annotations ? "Annotations: on" : "Annotations: off").OnClick([=] {
        options.Update([](PdfOptions& next) { next.annotations = !next.annotations; });
      }).With(Enabled{ready}),
    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
    unlock,
    Row{
      ScrollView(std::move(horizontal_page)).With(Grow{}, ScrollBar{}, Background(theme.colors.surface_container_low), CornerRadius(12.0F)),
      Column{
        Row{Button("Document").OnClick([show_text] { show_text = false; }),
            Button("Page text").OnClick([show_text] { show_text = true; })}.With(Spacing(8.0F)),
        ScrollView(std::move(inspector)).With(Grow{}, ScrollBar{}),
      }.With(Frame{.width = 300.0F}, Padding(18.0F), Spacing(18.0F), Background(theme.colors.surface), CornerRadius(12.0F),
          CrossAlign(CrossAxisAlignment::Stretch)),
    }.With(Grow{}, Spacing(18.0F), CrossAlign(CrossAxisAlignment::Stretch)),
    Text((busy ? "Working · " : "") + status + "  |  " + name.Get(), TextRole::Label),
  }.With(Padding(24.0F), Spacing(18.0F), CrossAlign(CrossAxisAlignment::Stretch), Background(theme.colors.background));
}

View App() {
  auto dark = UseState(false);
  return MaterialTheme(dark ? MaterialDarkThemeSpec() : MaterialLightThemeSpec(), Scope([dark] { return Workspace(dark); }));
}
} // namespace

const Application application{App, {
  .window = {.title = "PDF Preview", .initial_size = {1280.0F, 860.0F}, .minimum_size = Size{960.0F, 640.0F}},
  .show_debug_overlay = false,
}};

const huxerui::Application& PdfPreviewApplication() { return application; }
