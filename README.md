# Lib-PDF

[简体中文](README.zh-CN.md)

PDF content for [HuxerUI](https://github.com/HuxerUI/HuxerUI), powered by
[PDFium](https://pdfium.googlesource.com/pdfium/). C++20; target `HuxerUI::PDF`;
namespace `huxerui::pdf`. The 0.2 API follows the session and presentation patterns
used by Scave's Camera and Media libraries.

- `PdfSource`: explicit packaged resource, raw asset, File, FileReference, or immutable document.
- `UsePdf(source, options)`: retains a composition-owned session and applies controlled display intent.
- `PdfSession::Snapshot()`: observable metadata, last published page, status, and typed error.
- `PdfPage(session, properties)`: presentation only, using ordinary HuxerUI layout.
- `ReadTextAsync`, `SearchAsync`, and `RenderAsync`: library-scheduled `Task<PdfResult<T>>` operations.
- Independent synchronous document API when HuxerUI is not needed.

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

[[huxerui::composable]]
View Reader() {
  auto options = UseState(pdf::PdfOptions{});
  auto session = pdf::UsePdf(pdf::PdfSource(File("/absolute/path/to/guide.pdf")), options);
  const auto state = session.Snapshot();
  const bool next = state.status == pdf::PdfStatus::Ready && state.metadata
      && options->page + 1 < state.metadata->pages;
  return Column{
    Button("Next").OnClick([options] {
      options.Update([](pdf::PdfOptions& value) { ++value.page; });
    }).With(Enabled{next}),
    pdf::PdfPage(session).With(Grow{}),
  }.With(Grow{}, CrossAlign(CrossAxisAlignment::Stretch));
}
```

Use an accessible File path or a file-picker-provided FileReference.
No `Install` hook is needed: immutable document handles own the shared PDFium
runtime, and HuxerUI's TaskScope owns the session's asynchronous work.

## Controlled input and observed state

`PdfOptions` contains zero-based `page`, positive `zoom`, `page_width` (640 logical
units by default), `render_scale` (1.5), `PageRotation`, annotations, opaque
`background_rgb`, open options, and a `reload` token. Changes are applied after
composition commits. Page/display changes reuse the open document; source,
password/open-option, or reload-token changes reopen it.
For an already-open immutable PdfDocument source, reload reuses that supplied
document and prepares its output again; open options apply to byte/file sources.

`Snapshot()` subscribes an active composition. It returns a point-in-time value:
`Empty`, `Loading`, `Rendering`, `Ready`, `Failed`, or `Closed`; optional metadata,
optional output, and optional `PdfError`. Output identifies the page/settings that
actually finished rendering. Input changes can precede publication; input intent
does not imply that the new page has already been rendered.

Source replacement clears old metadata and output. Rendering another page keeps
the previous published page until success; a render failure preserves it. A page
outside the newly loaded document reports `Page` at the `Render` stage.

`PdfPage` supplies no toolbar, scrolling, dialogs, loading text, or error UI. It
draws nothing without output, and otherwise fits the published image into bounded
layout. The default is centered `ImageFit::Contain`; properties only change
presentation. Multiple page views can share one session.

Zoom changes the published image's intrinsic logical size and raster resolution.
For a scrollable zoomed page, frame `PdfPage` to
`state.output->image.IntrinsicSize()` and wrap it in application-owned scroll views,
as the demo does. Fitting a page into a fixed viewport continues to obey its fit
policy. PDF dimensions in `output.size` remain in points (1/72 inch).

## Source identity and lifetime

`PdfSource` accepts `RawResource`, `RawAsset`, `File`, `FileReference`, or
`PdfDocument` through explicit constructors. The default source is empty and
clears the session. No string/URL guessing, implicit downloads, or file watching
occurs.

Equal resource/asset/File/document values preserve the current document during
recomposition. Change `reload` to reread an unchanged File, retry a failed load,
or reopen with the same options. FileReference sources retain the access grant;
copies share source identity. Retain the constructed PdfSource instead of
reconstructing a FileReference source every composition.

`UsePdf` owns the active session lifetime. Copies of PdfSession share identity
and state; they do not keep it active after its owner unmounts. Removing only a
PdfPage does not close the session. Replaced/unmounted requests are canceled and
obsolete results cannot publish; work already running may finish before
releasing its document. All handle access belongs to the owning UI thread and
application. Reads/commands throw `std::logic_error` on misuse. After owner unmount,
the live application's retained handle reports Closed; after application shutdown,
its SDK state lifetime has expired and access is invalid.

## Asynchronous document operations

Start from a UI event or lifecycle task, using `UseTaskScope()`:

```cpp
tasks.Launch([session, notice]() -> Task<void> {
  auto found = co_await session.SearchAsync(0, "HuxerUI", {.whole_word = true});
  notice = found.Succeeded()
      ? std::to_string(found.Value().size()) + " matches"
      : found.Error().message;
});
```

`ReadTextAsync(page)` yields UTF-8 text, `SearchAsync(page, query, options)` yields
character indices and rectangles, and `RenderAsync(page, options)` yields owned
RGBA8 pixels. Work runs on library-managed workers and awaits resume on the
owning UI sequence. These operations do not alter display options or replace the
session's output/error. They can run once a document is loaded, including while
its display is rendering.

Failures use the SDK's `Result<T, PdfError>` (`PdfResult<T>`). Check `Succeeded()`
before accessing `Value()` or `Error()`. Error code/stage are stable categories;
message is diagnostic text. NotReady/Closed report unavailable session work.
Replacing/reloading the source before an operation completes yields Superseded;
changing the displayed page does not invalidate an explicit page request. Tasks
are lazy and check readiness when executed. Cancellation suppresses delivery.

Invalid scalar options/query encoding throw synchronously. Document-dependent
page bounds yield a typed Page failure. No OCR is performed. Search rectangles
use unrotated PDF coordinates with bottom-left origin; indices are PDFium
character indices, neither UTF-8 byte offsets nor UTF-16 offsets.

## Standalone document API

`<huxerui/pdf/document.h>` does not include HuxerUI/PDFium headers:

```cpp
auto doc = huxerui::pdf::PdfDocument::Open("protected.pdf", {.password = "secret"});
auto text = doc.Text(0);
auto matches = doc.Search(0, "PDF");
auto pixels = doc.Render(0, {.width = 1440,
    .rotation = huxerui::pdf::PageRotation::Clockwise90});
auto png = pixels.ToPng();
```

These explicitly synchronous methods may run on workers. Input memory is copied,
document handles share ownership, and Unicode filesystem paths are supported. Backend
failures throw `PdfException`, invalid options `std::invalid_argument`, and bad
page indices `std::out_of_range`. All library PDFium calls, including destruction,
share a mutex. Do not initialize/destroy or concurrently call PDFium independently.

Defaults limit input to 256 MiB, display rendering to 32 million pixels (hard ceiling
64 million and 32768 per axis), extracted text to 16 million characters per page,
and search to 1000 results. Resource/reference sources are read in bounded chunks
with the input limit applied before accepting excess bytes.

## Build, demo, and tests

Requires CMake 3.20+, C++20, and HuxerUI for UI components. Verified SDK revision:
`cadb1696cfa7b87d9d2d316b3ecba07b85a46962`.

```sh
cmake -S . -B build -DHUXERUI_HOME=/path/to/HuxerUI \
  -DHUXERUI_PDF_BUILD_TESTS=ON -DHUXERUI_BUILD_TESTING_LIBRARY=ON \
  -DHUXERUI_PDF_BUILD_EXAMPLES=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The desktop [demo](examples/preview/README.md) provides file/password opening,
paging, zoom, rotation, annotations, search, text inspection and theme switching.
It uses only the session API for PDF work, with no caller-side RunWorker pipeline.
It follows `examples/preview/{src,resources,platform}` and builds independently:

```sh
cmake -S examples/preview -B build-demo -DHUXERUI_HOME=/path/to/HuxerUI
cmake --build build-demo --config Release --target example_pdf --parallel
```

Without UI dependencies:

```sh
cmake -S . -B build-core -DHUXERUI_PDF_BUILD_VIEW=OFF -DHUXERUI_PDF_BUILD_TESTS=ON
cmake --build build-core --config Release --parallel
ctest --test-dir build-core -C Release --output-on-failure
```

CI checks the document backend on Windows/Linux/macOS and the session/demo
on Windows. Headless tests cover controlled updates, source reloads, typed async
results, wrong-thread access, request replacement and unmount behavior, plus the
demo's real packaged resources. Compiler-free project-plan/library-graph discovery
does not download PDFium. Design references are recorded in
[the Scave API study](docs/API-CONVENTIONS.zh-CN.md).

## PDFium SDK and deployment

Default builds fetch non-V8 PDFium `chromium/8138` from
[pdfium-binaries](https://github.com/bblanchon/pdfium-binaries), with per-archive
SHA-256 verification. Automatic downloads cover Windows x64/ARM64, Linux glibc
x64/ARM64, and macOS x64/ARM64/universal. Other architectures, musl, Android, iOS,
and Web require a matching, independently built SDK or an existing
`PDFium::PDFium` CMake target. Those cross-platform configurations have not been
validated by this project.

The pinned macOS SDK requires macOS 13 or later. CMake normalizes and ad-hoc signs
a build-local copy of its dylib for `@rpath` loading, leaving supplied SDK files
untouched. Apply your application's distribution signing during packaging.

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
