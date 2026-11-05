#include <huxerui/huxerui.h>
#include <huxerui/testing/ui_test.h>
#include <chrono>
#include <source_location>
#include <thread>
#include "../src/app.h"
#include "../../../tests/test.h"

using namespace huxerui;
using namespace huxerui::testing;

namespace {
class PackageResources final : public PlatformResources {
public:
  ResourceConfiguration Configuration() const override { return {}; }
  std::optional<InputStream> OpenRead(std::string_view path) override {
    auto stream = File(std::string(PDF_DEMO_PACKAGE) + "/" + std::string(path)).OpenRead();
    if (!stream.Succeeded()) return std::nullopt;
    return std::move(stream).Value();
  }
};

void Wait(UiTest& ui, const std::function<bool()>& predicate,
          std::source_location location = std::source_location::current()) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!predicate()) {
    if (std::chrono::steady_clock::now() >= deadline) {
      for (const auto& node : ui.Find(UiSelector::Type<Text>()).All()) std::cerr << node.text.value_or("") << '\n';
      for (const auto& node : ui.Find(UiSelector::Type<Image>()).All()) std::cerr << "Image " << node.bounds.width << "x" << node.bounds.height << '\n';
      throw std::runtime_error("Demo wait timed out at line " + std::to_string(location.line()));
    }
    ui.Pump(std::chrono::milliseconds(16));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

bool IsEnabled(UiTest& ui, const char* label) {
  return ui.Find(UiSelector::Text(label)).One().enabled;
}
} // namespace

int main() { return Run([] {
  UiTest ui(PdfPreviewApplication(), {.viewport = {1280, 860}, .resource_provider = std::make_shared<PackageResources>()});
  Wait(ui, [&] { return IsEnabled(ui, "Next"); });
  CHECK(ui.Find(UiSelector::Text("Page 1 / 3")).Exists());
  CHECK(ui.Find(UiSelector::Text("The PDF field guide")).Exists());
  CHECK(!IsEnabled(ui, "Previous"));
  ui.Find(UiSelector::Text("Search")).Tap();
  Wait(ui, [&] { return ui.Find(UiSelector::Text("5 matches for \"PDF\" on this page.")).Exists(); });
  ui.Find(UiSelector::Text("Next")).Tap();
  Wait(ui, [&] { return ui.Find(UiSelector::Text("Page 2 / 3")).Exists() && IsEnabled(ui, "Next"); });
  CHECK(IsEnabled(ui, "Previous"));
  const auto portrait = ui.Find(UiSelector::Type<Image>()).One().bounds;
  CHECK(portrait.height > portrait.width);
  ui.Find(UiSelector::Text("Rotate 90°")).Tap();
  Wait(ui, [&] { return ui.Find(UiSelector::Text("Rotation: 90°")).Exists() && IsEnabled(ui, "Next"); });
  Wait(ui, [&] {
    const auto bounds = ui.Find(UiSelector::Type<Image>()).One().bounds;
    return bounds.width > bounds.height && IsEnabled(ui, "Next");
  });
  const auto landscape = ui.Find(UiSelector::Type<Image>()).One().bounds;
  CHECK(landscape.width > landscape.height);
  ui.Find(UiSelector::Text("Zoom in")).Tap();
  Wait(ui, [&] { return ui.Find(UiSelector::Text("125%")).Exists() && IsEnabled(ui, "Next"); });
  CHECK(ui.Find(UiSelector::Type<Image>()).One().bounds.width > landscape.width);
  ui.Find(UiSelector::Text("Annotations: on")).Tap();
  Wait(ui, [&] { return ui.Find(UiSelector::Text("Annotations: off")).Exists() && IsEnabled(ui, "Next"); });
  ui.Find(UiSelector::Text("Page text")).Tap();
  CHECK(ui.Find(UiSelector::Key("page-text-preview")).Exists());
  ui.Find(UiSelector::Text("Dark theme")).Tap();
  CHECK(ui.Find(UiSelector::Text("Light theme")).Exists());
  ui.Find(UiSelector::Text("Load sample")).Tap();
  Wait(ui, [&] { return ui.Find(UiSelector::Text("Page 1 / 3")).Exists() && IsEnabled(ui, "Next"); });
  CHECK(ui.Find(UiSelector::Text("100%")).Exists());
  CHECK(ui.Find(UiSelector::Text("Annotations: on")).Exists());
}); }
