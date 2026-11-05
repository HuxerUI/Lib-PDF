# Lib-PDF

[简体中文](README.zh-CN.md)

Native PDF content for [HuxerUI](https://github.com/HuxerUI/HuxerUI), powered by
[PDFium](https://pdfium.googlesource.com/pdfium/). C++20, CMake target
`HuxerUI::PDF` (`huxerui_pdf`), namespace `huxerui::pdf`.

- Open files or owned memory, including password-protected PDFs and Unicode paths.
- Read page sizes, metadata, and UTF-8 text; search with case/whole-word options and character bounds.
- Render RGBA8 images with rotation, annotation visibility, and a configurable opaque background.
- Display prepared page images or use a viewer with paging, zoom, and two-axis scrolling.
- Run document operations without HuxerUI; prepare immutable images on a worker for UI composition.
- Fixed PDFium dependency with SHA-256 verification; support an explicit SDK for offline/cross builds.

## Quick start

```cmake
# After huxerui_add_app(your_app ...):
huxerui_use_library(your_app TARGET HuxerUI::PDF PATH "/path/to/Lib-PDF")
huxerui_pdf_copy_runtime(your_app) # Deploy pdfium.dll beside the Windows executable.
```

```cpp
#include <huxerui/huxerui.h>
#include <huxerui/pdf.h>

using namespace huxerui;
using namespace huxerui::pdf;

// Open once and retain this handle in your application model.
auto document = PdfDocument::Open("guide.pdf");
// During composition, inside a bounded layout:
View Reader() {
  return PdfViewer(document).With(Grow{});
}
```

`PdfViewer` keeps one rendered page image and rerenders only when the document,
page, width, or raster scale changes. Its navigation and zoom operations render
synchronously. Page width is in logical UI units; `render_scale` controls raster
resolution. The viewer exposes errors as text. Defaults are page 0, width 640,
zoom 1, raster scale 1.5, and zoom range 0.25–4.

For large pages or custom navigation, prepare on your application's worker queue:

```cpp
// Worker thread. PdfDocument copies share ownership; input files can be closed/deleted after Open.
auto prepared = PreparePdfPage(document, 0, {.width = 720, .render_scale = 2});
// UI thread, after delivering prepared through application-owned state:
View page = PdfPage(prepared);
```

The convenience overload `PdfPage(document, page, options)` also renders
synchronously. Retain `PdfPageImage` across recompositions to avoid repeated work.
`PdfPage` has no scrolling; wrap it in bounded scroll views as appropriate.

## Document API

The independent header `<huxerui/pdf/document.h>` does not include HuxerUI or PDFium
headers. All page indices are **zero-based** and page dimensions use PDF points
(1/72 inch), with intrinsic page rotation already applied.

```cpp
auto doc = PdfDocument::Open("protected.pdf", {.password = "secret"});
int pages = doc.PageCount();
auto size = doc.Size(0);
std::string title = doc.Metadata("Title");
std::string text = doc.Text(0);
auto hits = doc.Search(0, "HuxerUI", {.match_case = true, .whole_word = true});
auto bitmap = doc.Render(0, {.width = 1440, .rotation = Rotation::Clockwise90});
auto png = bitmap.ToPng(); // std::vector<std::byte>, lossless, uncompressed DEFLATE.
```

`FromBytes(std::span<const std::byte>, OpenOptions)` copies its input. Document
handles share immutable data. File reads support native `std::filesystem::path`,
including Windows Unicode names. Passwords follow PDFium's UTF-8 password rules.

Search results contain PDFium character indices and bounding rectangles in
**unrotated PDF page coordinates**, with the origin at bottom-left. Indices are
neither UTF-8 byte offsets nor UTF-16 offsets; rectangles need transformation for
rotated/scaled UI highlights. Search uses PDFium's matching semantics. Empty
queries return no results. Text extraction does not perform OCR.

`PdfError::Code()` distinguishes file, format, password, security, page, and unknown
backend failures. Invalid options throw `std::invalid_argument`; invalid page
indices throw `std::out_of_range`. The default input limit is 256 MiB, render limit
32 million pixels (hard ceiling 64 million and 32768 pixels per axis), text limit
16 million characters per page, and search limit 1000 matches. Lower the input,
pixel, and result limits through options when appropriate.

All PDFium calls made by Lib-PDF, including handle destruction, share a mutex.
Document APIs can therefore run on workers concurrently, with backend calls
serialized. Do not independently initialize/destroy PDFium or use its C API
concurrently with this library.

## Build and test

Requires CMake 3.20+ and C++20. Native components require HuxerUI; the verified
framework revision is `cadb1696cfa7b87d9d2d316b3ecba07b85a46962`.

```sh
cmake -S . -B build -DHUXERUI_HOME=/path/to/HuxerUI \
  -DHUXERUI_PDF_BUILD_TESTS=ON -DHUXERUI_BUILD_TESTING_LIBRARY=ON \
  -DHUXERUI_PDF_BUILD_EXAMPLES=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Run `pdf_preview` from the build output for the included two-page PDF, or pass a
PDF path as its first argument. Headless interaction tests exercise page changes,
disabled navigation, and zoom when `HuxerUI::testing` is available.

Without UI dependencies:

```sh
cmake -S . -B build-core -DHUXERUI_PDF_BUILD_VIEW=OFF -DHUXERUI_PDF_BUILD_TESTS=ON
cmake --build build-core --config Release --parallel
ctest --test-dir build-core -C Release --output-on-failure
```

CI checks document operations on Windows, Linux, and macOS, and builds/tests the
native component and preview on Windows. HuxerUI project-plan and library-graph
discovery do not enable compilers or download PDFium.

## PDFium SDK and deployment

Default builds fetch non-V8 PDFium `chromium/8138` from
[pdfium-binaries](https://github.com/bblanchon/pdfium-binaries), with per-archive
SHA-256 verification. Automatic downloads cover Windows x64/ARM64, Linux glibc
x64/ARM64, and macOS x64/ARM64/universal. Other architectures, musl, Android, iOS,
and Web require a matching, independently built SDK or an existing
`PDFium::PDFium` CMake target. Those cross-platform configurations have not been
validated by this project.

To build offline:

```sh
cmake -S . -B build-core -DHUXERUI_PDF_BUILD_VIEW=OFF \
  -DHUXERUI_PDF_FETCH_PDFIUM=OFF -DHUXERUI_PDF_PDFIUM_ROOT=/path/to/pdfium
```

The SDK layout is `include/fpdfview.h`, `include/cpp/fpdf_scopers.h`, and
`lib/libpdfium.so`, `lib/libpdfium.dylib`, or `lib/pdfium.dll.lib` plus
`bin/pdfium.dll`. Static SDKs can supply `lib/pdfium.lib` or `lib/libpdfium.a`;
use a pre-existing `PDFium::PDFium` target to express additional system dependencies.
For an existing target on Windows, set `HUXERUI_PDF_RUNTIME_FILE` to the DLL path
before calling `huxerui_pdf_copy_runtime`.

The bundled test and example executables copy the Windows DLL automatically.
Consumers must deploy PDFium and its transitive runtime dependencies with their
application. Unix build trees use CMake's runtime search paths; application
packaging must deploy the corresponding shared library. Ship the SDK's `LICENSE`
and complete `licenses/` directory alongside redistributed binaries; notices for
the pinned distribution are also retained in `third_party/pdfium`.

## Scope and license

This version provides read-only document viewing. Editing, saving PDFs, form
interaction, signatures, OCR, selection, clickable links, and streaming are outside
its current API. Pages render content and optional non-widget annotation
appearances; form widgets are not initialized. The default backend has no V8, and
Lib-PDF does not initialize JavaScript or launch document actions.

Lib-PDF is MIT licensed. PDFium and its bundled dependencies retain their own
licenses; see [third_party/pdfium](third_party/pdfium/README.md). Test PDFs are
project-owned and can be regenerated with `scripts/generate_fixtures.py`; fixture
generation is not needed to build or run tests.
