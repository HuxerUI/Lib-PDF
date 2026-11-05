#include <huxerui/pdf/document.h>
#include <atomic>
#include <fstream>
#include <iterator>
#include <limits>
#include <thread>
#include "test.h"
#include "../src/unicode.h"

using namespace huxerui::pdf;
namespace fs = std::filesystem;

int main() { return Run([] {
  const fs::path fixtures(PDF_FIXTURE_DIR);
  const auto doc = PdfDocument::Open(fixtures / "sample.pdf");
  CHECK(doc.PageCount() == 2);
  CHECK(doc.Size(0).width == 300 && doc.Size(0).height == 400);
  CHECK(doc.Size(1).width == 400 && doc.Size(1).height == 300);
  CHECK(doc.Metadata("Title") == "HuxerUI PDF sample");
  CHECK(doc.Metadata("Author") == "HuxerUI");
  CHECK(doc.Metadata("Subject") == "中文 / 😀");
  CHECK(doc.Metadata("Unknown").empty());
  CHECK(doc.Text(0).find("Hello PDF hello PDF") != std::string::npos);
  CHECK(doc.Text(0).find("中文😀") != std::string::npos);
  CHECK(doc.Text(1).find("Second page") != std::string::npos);
  const auto matches = doc.Search(0, "hello");
  CHECK(matches.size() == 3);
  CHECK(matches[0].start == 0 && matches[0].count == 5);
  CHECK(!matches[0].bounds.empty());
  CHECK(matches[0].bounds[0].right > matches[0].bounds[0].left);
  CHECK(matches[0].bounds[0].top > matches[0].bounds[0].bottom);
  CHECK(doc.Search(0, "hello", {.match_case = true}).size() == 2);
  CHECK(doc.Search(0, "hello", {.whole_word = true}).size() == 2);
  CHECK(doc.Search(0, "hello", {.max_results = 1}).size() == 1);
  CHECK(doc.Search(0, "hello", {.max_results = 0}).empty());
  CHECK(doc.Search(0, "missing").empty() && doc.Search(0, "").empty());
  CHECK(doc.Search(0, "中文").size() == 1);

  const auto bitmap = doc.Render(0, {.width = 300});
  CHECK(bitmap.width == 300 && bitmap.height == 400);
  CHECK(bitmap.pixels.size() == 300U * 400U * 4U);
  const auto offset = (350U * 300U + 20U) * 4U;
  CHECK(bitmap.pixels[offset] > 240 && bitmap.pixels[offset + 1] < 15 && bitmap.pixels[offset + 2] < 15);
  for (std::size_t i = 3; i < bitmap.pixels.size(); i += 4) CHECK(bitmap.pixels[i] == 255);
  const auto background = doc.Render(0, {.width = 30, .background_rgb = 0x123456});
  CHECK(background.pixels[0] == 0x12 && background.pixels[1] == 0x34 && background.pixels[2] == 0x56);
  const auto annotation_pixel = (350U * 300U + 160U) * 4U;
  CHECK(bitmap.pixels[annotation_pixel + 2] > 240 && bitmap.pixels[annotation_pixel] < 15);
  const auto without_annotations = doc.Render(0, {.width = 300, .annotations = false});
  CHECK(without_annotations.pixels[annotation_pixel] == 255 && without_annotations.pixels[annotation_pixel + 2] == 255);
  const auto rotated = doc.Render(0, {.width = 400, .rotation = PageRotation::Clockwise90});
  CHECK(rotated.width == 400 && rotated.height == 300);
  CHECK(doc.Render(0, {.width = 300, .rotation = PageRotation::Clockwise180}).height == 400);
  CHECK(doc.Render(0, {.width = 400, .rotation = PageRotation::Clockwise270}).height == 300);
  const auto png = bitmap.ToPng();
  CHECK(png.size() > bitmap.pixels.size());
  CHECK(png[0] == std::byte{137} && png[1] == std::byte{'P'});
  // Retained as a build artifact for decoding with an independent image decoder.
  std::ofstream image("rendered-page.png", std::ios::binary);
  image.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));

  std::ifstream input(fixtures / "sample.pdf", std::ios::binary);
  const std::string source((std::istreambuf_iterator<char>(input)), {});
  auto memory = std::vector<std::byte>(reinterpret_cast<const std::byte*>(source.data()),
      reinterpret_cast<const std::byte*>(source.data()) + source.size());
  const auto owned = PdfDocument::FromBytes(memory);
  memory.clear(); memory.shrink_to_fit();
  CHECK(owned.Text(0) == doc.Text(0));
  const auto copy = doc;
  CHECK(copy == doc && owned != doc && copy.Text(1) == doc.Text(1));
  const auto unicode_path = fs::temp_directory_path() / fs::path(u8"huxerui-pdf-测试.pdf");
  fs::copy_file(fixtures / "sample.pdf", unicode_path, fs::copy_options::overwrite_existing);
  const auto unicode_doc = PdfDocument::Open(unicode_path);
  fs::remove(unicode_path);
  CHECK(unicode_doc.PageCount() == 2 && !unicode_doc.Text(0).empty());

  try { (void)PdfDocument::Open(fixtures / "encrypted.pdf"); CHECK(false); }
  catch (const PdfException& error) { CHECK(error.Code() == PdfErrorCode::Password); }
  try { (void)PdfDocument::Open(fixtures / "encrypted.pdf", {.password = "wrong"}); CHECK(false); }
  catch (const PdfException& error) { CHECK(error.Code() == PdfErrorCode::Password); }
  CHECK(PdfDocument::Open(fixtures / "encrypted.pdf", {.password = "huxerui"}).Text(0) == doc.Text(0));
  CHECK(PdfDocument::Open(fixtures / "encrypted.pdf", {.password = "owner"}).PageCount() == 2);
  Throws<PdfException>([&] { (void)PdfDocument::Open(fixtures / "missing.pdf"); });
  Throws<PdfException>([] { (void)PdfDocument::FromBytes({}); });
  Throws<PdfException>([] { const std::byte bad[]{std::byte{'x'}}; (void)PdfDocument::FromBytes(bad); });
  Throws<std::invalid_argument>([&] { (void)PdfDocument::Open(fixtures / "sample.pdf", {.max_bytes = 1}); });
  Throws<std::invalid_argument>([&] { (void)PdfDocument::Open(fixtures / "sample.pdf", {.password = std::string("a\0b", 3)}); });
  Throws<std::out_of_range>([&] { (void)doc.Size(-1); });
  Throws<std::out_of_range>([&] { (void)doc.Render(2); });
  Throws<std::out_of_range>([&] { (void)doc.Text(2); });
  Throws<std::out_of_range>([&] { (void)doc.Search(2, "hello"); });
  Throws<std::invalid_argument>([&] { (void)doc.Render(0, {.width = 0}); });
  Throws<std::invalid_argument>([&] { (void)doc.Render(0, {.width = std::numeric_limits<int>::max()}); });
  Throws<std::invalid_argument>([&] { (void)doc.Render(0, {.rotation = static_cast<PageRotation>(9)}); });
  Throws<std::invalid_argument>([&] { (void)doc.Render(0, {.background_rgb = 0xFFFFFFFF}); });
  Throws<std::invalid_argument>([&] { (void)doc.Render(0, {.max_pixels = 10}); });
  Throws<std::invalid_argument>([&] { (void)doc.Metadata(std::string("a\0b", 3)); });
  Throws<std::invalid_argument>([&] { (void)doc.Search(0, "\xC0\xAF"); });
  Throws<std::invalid_argument>([&] { (void)doc.Search(0, "\xED\xA0\x80"); });
  Throws<std::invalid_argument>([&] { (void)doc.Search(0, std::string("a\0b", 3)); });
  Throws<std::invalid_argument>([] { (void)PdfBitmap{}.ToPng(); });
  const auto wide = huxerui::pdf::detail::ToUtf16("中文😀");
  CHECK(huxerui::pdf::detail::FromUtf16(wide.data(), wide.size() - 1) == "中文😀");

  std::atomic<bool> success{true};
  std::vector<std::thread> workers;
  for (int i = 0; i < 4; ++i) workers.emplace_back([&, copy] {
    try {
      for (int n = 0; n < 8; ++n) {
        const auto concurrent = PdfDocument::Open(fixtures / "sample.pdf");
        if (copy.Search(0, "PDF").size() != 2 || concurrent.Render(1, {.width = 40}).height != 30) success = false;
      }
    } catch (...) { success = false; }
  });
  for (auto& worker : workers) worker.join();
  CHECK(success.load());
}); }
