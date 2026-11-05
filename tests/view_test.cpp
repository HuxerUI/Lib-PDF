#include <huxerui/pdf.h>
#include <huxerui/huxerui.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <limits>
#include <thread>
#include "test.h"
#ifdef PDF_HAS_UI_TESTING
#include <huxerui/testing/ui_test.h>
#endif

using namespace huxerui;
using namespace huxerui::pdf;

#ifdef PDF_HAS_UI_TESTING
namespace {
using namespace huxerui::testing;
struct Controls {
  State<PdfSource> source;
  State<PdfOptions> options;
  State<bool> mounted;
  State<int> tick;
  TaskScope tasks;
};
std::optional<Controls> controls;
std::optional<PdfSession> active;
std::optional<PdfSession> secondary;
std::optional<PdfResult<std::string>> text_result;
std::optional<PdfResult<std::vector<SearchMatch>>> search_result;
std::optional<PdfResult<PdfBitmap>> render_result;

File Fixture(const char* name) { return File((std::filesystem::path(PDF_FIXTURE_DIR) / name).generic_string()); }

View Root() {
  auto source = UseState(PdfSource(Fixture("sample.pdf")));
  auto options = UseState(PdfOptions{.page_width = 300, .render_scale = 1});
  auto mounted = UseState(true);
  auto tick = UseState(0);
  controls = Controls{source, options, mounted, tick, UseTaskScope()};
  View child = mounted.Get() ? Scope([source, options]() -> View {
    auto session = UsePdf(source.Get(), options.Get());
    secondary = UsePdf(PdfSource(Fixture("sample.pdf")), {.page = 1, .page_width = 40, .render_scale = 1});
    active = session;
    const auto snapshot = session.Snapshot();
    const auto size = snapshot.output ? snapshot.output->image.IntrinsicSize() : Size{300, 400};
    return PdfPage(session).With(Frame{.width = size.width, .height = size.height});
  }) : View(Text("Unmounted"));
  return Column{Text(std::to_string(tick.Get())), std::move(child)};
}

void Wait(UiTest& ui, const std::function<bool()>& predicate) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!predicate()) {
    if (std::chrono::steady_clock::now() >= deadline) {
      const auto state = active->Snapshot();
      throw std::runtime_error("PDF session wait timed out: " + (state.error ? state.error->message : std::to_string(static_cast<int>(state.status))));
    }
    ui.Pump(std::chrono::milliseconds(16));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

void Ready(UiTest& ui, const std::function<bool(const PdfPageOutput&)>& predicate = [](const auto&) { return true; }) {
  Wait(ui, [&] { const auto state = active->Snapshot(); return state.status == PdfStatus::Ready && state.output && predicate(*state.output); });
}

Task<void> Probe(PdfSession session) {
  text_result = co_await session.ReadTextAsync(0);
  search_result = co_await session.SearchAsync(0, "hello");
  render_result = co_await session.RenderAsync(1, {.width = 40});
}

Task<void> Read(PdfSession session, int page) { text_result = co_await session.ReadTextAsync(page); }
} // namespace
#endif

int main() { return Run([] {
  CHECK(!PdfSource{}.HasValue());
  CHECK(PdfSource(File("a.pdf")) == PdfSource(File("a.pdf")));
  CHECK(PdfSource(File("a.pdf")) != PdfSource(File("b.pdf")));
  const auto asset = RawAsset::FromBytes({std::byte{'x'}});
  CHECK(PdfSource(asset) == PdfSource(asset));
  Throws<std::invalid_argument>([] { (void)UsePdf({}, {.zoom = 0}); });
  Throws<std::invalid_argument>([] { (void)UsePdf({}, {.page = -1}); });
  Throws<std::invalid_argument>([] { (void)UsePdf({}, {.render_scale = std::numeric_limits<float>::infinity()}); });
  Throws<std::logic_error>([] { (void)UsePdf(PdfSource(File("x.pdf"))); });

#ifdef PDF_HAS_UI_TESTING
  Application app(Root, {.window = {.initial_size = {900, 700}}, .show_debug_overlay = false});
  UiTest ui(app);
  Ready(ui);
  Wait(ui, [] { return secondary->Snapshot().status == PdfStatus::Ready; });
  CHECK(*active != *secondary);
  CHECK(secondary->Snapshot().output->page == 1);
  const auto secondary_image = secondary->Snapshot().output->image;
  CHECK(active->IsConnected());
  CHECK(active->Snapshot().metadata->pages == 2);
  CHECK(active->Snapshot().metadata->title == "HuxerUI PDF sample");
  CHECK(ui.Find(UiSelector::Type<Button>()).All().empty()); // A page never creates viewer chrome.
  CHECK(ui.Find(UiSelector::Type<Image>()).One().bounds.width == 300);
  CHECK(ui.Find(UiSelector::Type<Image>()).One().bounds.height == 400);
  const auto image = active->Snapshot().output->image;
  const auto identity = *active;
  controls->tick = 1;
  ui.Pump();
  CHECK(*active == identity);
  CHECK(active->Snapshot().output->image == image); // Ordinary recomposition does not rerasterize.

  controls->options.Update([](PdfOptions& value) { value.page = 1; });
  ui.Pump();
  Ready(ui, [](const auto& page) { return page.page == 1; });
  CHECK(active->Snapshot().output->size.width == 400);
  controls->options.Update([](PdfOptions& value) { value.zoom = 1.25F; value.rotation = PageRotation::Clockwise90; });
  ui.Pump();
  Ready(ui, [](const auto& page) { return page.zoom == 1.25F && page.rotation == PageRotation::Clockwise90; });
  CHECK(ui.Find(UiSelector::Type<Image>()).One().bounds.width == 375);
  CHECK(ui.Find(UiSelector::Type<Image>()).One().bounds.height == 500);

  controls->tasks.Launch(Probe(*active));
  Wait(ui, [] { return render_result.has_value(); });
  CHECK(text_result->Succeeded() && text_result->Value().find("中文😀") != std::string::npos);
  CHECK(search_result->Succeeded() && search_result->Value().size() == 3);
  CHECK(render_result->Succeeded() && render_result->Value().width == 40 && render_result->Value().height == 30);
  CHECK(active->Snapshot().output->page == 1 && active->Snapshot().output->zoom == 1.25F);
  CHECK(secondary->Snapshot().output->image == secondary_image);
  Throws<std::invalid_argument>([] { (void)active->ReadTextAsync(-1); });
  Throws<std::invalid_argument>([] { (void)active->SearchAsync(0, "\xC0\xAF"); });
  Throws<std::invalid_argument>([] { (void)active->RenderAsync(0, {.width = 0}); });
  Throws<std::invalid_argument>([] { (void)PdfPage(*active, {.horizontal_alignment = HorizontalAlignment::Stretch}); });
  std::atomic<bool> rejected{false};
  std::thread other([&] { try { (void)identity.Snapshot(); } catch (const std::logic_error&) { rejected = true; } });
  other.join(); CHECK(rejected.load());

  const auto last_image = active->Snapshot().output->image;
  controls->options.Update([](PdfOptions& value) { value.page = 99; });
  ui.Pump();
  Wait(ui, [] { return active->Snapshot().status == PdfStatus::Failed; });
  CHECK(active->Snapshot().error->code == PdfErrorCode::Page && active->Snapshot().error->stage == PdfErrorStage::Render);
  CHECK(active->Snapshot().output->image == last_image);
  text_result.reset(); controls->tasks.Launch(Read(*active, 99));
  Wait(ui, [] { return text_result.has_value(); });
  CHECK(!text_result->Succeeded() && text_result->Error().code == PdfErrorCode::Page);
  CHECK(active->Snapshot().error->stage == PdfErrorStage::Render); // Independent requests do not replace display errors.
  controls->options = PdfOptions{.page_width = 300, .render_scale = 1};
  ui.Pump(); Ready(ui, [](const auto& page) { return page.page == 0; });

  controls->source = PdfSource(Fixture("encrypted.pdf"));
  ui.Pump();
  Wait(ui, [] { return active->Snapshot().status == PdfStatus::Failed; });
  CHECK(active->Snapshot().error->code == PdfErrorCode::Password);
  CHECK(!active->Snapshot().output && !active->Snapshot().metadata);
  controls->options.Update([](PdfOptions& value) { value.open.password = "huxerui"; });
  ui.Pump(); Ready(ui);

  controls->source = PdfSource(asset);
  controls->options.Update([](PdfOptions& value) { value.open.max_bytes = 1; });
  ui.Pump();
  Wait(ui, [] { return active->Snapshot().status == PdfStatus::Failed; });
  CHECK(active->Snapshot().error->code == PdfErrorCode::Format);
  controls->source = PdfSource(RawAsset::FromBytes({std::byte{'x'}, std::byte{'y'}}));
  ui.Pump();
  Wait(ui, [] { return active->Snapshot().status == PdfStatus::Failed; });
  CHECK(active->Snapshot().error->code == PdfErrorCode::Limit);

  controls->source = PdfSource{};
  ui.Pump();
  CHECK(active->Snapshot().status == PdfStatus::Empty && !active->Snapshot().output);
  text_result.reset(); controls->tasks.Launch(Read(*active, 0));
  Wait(ui, [] { return text_result.has_value(); });
  CHECK(text_result->Error().code == PdfErrorCode::NotReady);

  const auto temporary = std::filesystem::temp_directory_path() /
      ("huxerui-pdf-reload-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".pdf");
  std::filesystem::copy_file(std::filesystem::path(PDF_FIXTURE_DIR) / "sample.pdf", temporary);
  controls->options = PdfOptions{.page_width = 300, .render_scale = 1};
  controls->source = PdfSource(File(temporary.generic_string()));
  ui.Pump(); Ready(ui);
  const auto cached = active->Snapshot().output->image;
  { std::ofstream changed(temporary, std::ios::binary); changed << 'x'; }
  controls->tick = 2;
  ui.Pump();
  CHECK(active->Snapshot().output->image == cached); // Same File identity does not watch changes.
  controls->options.Update([](PdfOptions& value) { ++value.reload; });
  ui.Pump();
  Wait(ui, [] { return active->Snapshot().status == PdfStatus::Failed; });
  CHECK(active->Snapshot().error->code == PdfErrorCode::Format && !active->Snapshot().output);
  std::filesystem::remove(temporary);

  // Replacing source/reload intents before the next publication keeps only the latest request.
  controls->options = PdfOptions{.page_width = 300, .render_scale = 1};
  controls->source = PdfSource(Fixture("sample.pdf"));
  ui.Pump();
  controls->source = PdfSource(Fixture("encrypted.pdf"));
  controls->options.Update([](PdfOptions& value) { value.open.password = "huxerui"; });
  ui.Pump();
  controls->source = PdfSource(Fixture("sample.pdf"));
  ui.Pump(); Ready(ui);
  CHECK(!active->Snapshot().error);
  controls->options.Update([](PdfOptions& value) { value.page_width = 1800; });
  ui.Pump();
  controls->mounted = false;
  ui.Pump();
  CHECK(!identity.IsConnected() && identity.Snapshot().status == PdfStatus::Closed);
  CHECK(!secondary->IsConnected() && secondary->Snapshot().status == PdfStatus::Closed);
  text_result.reset(); controls->tasks.Launch(Read(identity, 0));
  Wait(ui, [] { return text_result.has_value(); });
  CHECK(text_result->Error().code == PdfErrorCode::Closed);
  CHECK(identity.Snapshot().status == PdfStatus::Closed && !identity.Snapshot().output);
#endif
}); }
