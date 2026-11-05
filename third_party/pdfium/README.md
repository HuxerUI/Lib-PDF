# PDFium notices

Lib-PDF uses the public C API of PDFium. Default desktop builds download the
non-V8 `chromium/8138` release from
[bblanchon/pdfium-binaries](https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium/8138).
Archives are pinned by SHA-256 in `cmake/PDFium.cmake`; no binary is committed here.

`LICENSE` and `licenses/` are copied without modification from the pinned Windows
x64 distribution. They cover the distributor, PDFium, and its bundled third-party
dependencies. Redistribute the license files from the actual SDK shipped with
your application, especially when supplying a custom SDK or different platform.
