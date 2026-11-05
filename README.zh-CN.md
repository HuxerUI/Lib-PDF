# Lib-PDF

[English](README.md)

为 [HuxerUI](https://github.com/HuxerUI/HuxerUI) 提供基于 PDFium 的原生 PDF 扩展。
C++20，CMake 目标 `HuxerUI::PDF`，命名空间 `huxerui::pdf`。

- 从文件或内存加载 PDF，支持密码文档及 Windows 中文路径。
- 读取页数、页面尺寸、元数据和 UTF-8 文本；支持大小写、整词搜索与匹配区域。
- 输出 RGBA8 位图或 PNG，支持旋转、批注显示和背景色。
- 提供单页组件及带翻页、缩放、双向滚动的查看器。
- 文档 API 可独立构建，页面图像可在工作线程准备后交给 UI。
- PDFium 固定版本并校验 SHA-256，也可指定本地 SDK 离线构建。

## 接入

```cmake
# 在 huxerui_add_app(your_app ...) 之后：
huxerui_use_library(your_app TARGET HuxerUI::PDF PATH "/path/to/Lib-PDF")
huxerui_pdf_copy_runtime(your_app) # Windows：复制 pdfium.dll 到程序旁。
```

```cpp
#include <huxerui/huxerui.h>
#include <huxerui/pdf.h>

using namespace huxerui;
using namespace huxerui::pdf;

// 放在应用模型中，加载一次并保留。
auto document = PdfDocument::Open("guide.pdf");

View Reader() {
  return PdfViewer(document).With(Grow{});
}
```

查看器需要有界的可用高度，缓存当前页面，翻页或缩放时同步重新渲染。
大页面建议在应用工作线程执行 `PreparePdfPage(document, 0, {.width = 720, .render_scale = 2})`，
然后将返回的 `PdfPageImage` 交给 UI，通过 `PdfPage(prepared)` 显示。
单页组件本身不带滚动，准备好的图像可跨重组复用。

## 独立文档 API

只需包含 `<huxerui/pdf/document.h>`，无需 HuxerUI 头文件。

```cpp
auto doc = PdfDocument::Open("protected.pdf", {.password = "secret"});
auto text = doc.Text(0);
auto matches = doc.Search(0, "HuxerUI", {.whole_word = true});
auto bitmap = doc.Render(0, {.width = 1440, .rotation = Rotation::Clockwise90});
auto png = bitmap.ToPng();
```

页码从 **0** 开始。页面尺寸以 PDF 点为单位（1/72 英寸），已包含文档自身的页面旋转。
搜索返回 PDFium 字符索引及原始未旋转页面坐标，原点在左下角；索引不是 UTF-8 字节偏移或
UTF-16 偏移，显示高亮时需自行转换坐标。文本提取不包含 OCR。

`PdfError::Code()` 区分文件、格式、密码、安全、页面和未知后端错误；非法参数抛出
`std::invalid_argument`，越界页码抛出 `std::out_of_range`。
默认输入限制为 256 MiB，渲染限制为约 3200 万像素（硬上限约 6400 万，单轴最多 32768），
每页文本最多约 1600 万字符，搜索最多 1000 条。输入、渲染和搜索上限可通过参数降低。
文档共享所有权，内存输入会复制；库内所有 PDFium 调用（含销毁）通过同一互斥锁串行执行，
可以在多个工作线程调用文档 API。不要在外部独立初始化、销毁 PDFium，或并发调用其 C API。

## 构建与测试

需要 CMake 3.20+ 和 C++20；UI 构建还需要 HuxerUI。已验证的框架提交为
`cadb1696cfa7b87d9d2d316b3ecba07b85a46962`。

```sh
cmake -S . -B build -DHUXERUI_HOME=/path/to/HuxerUI \
  -DHUXERUI_PDF_BUILD_TESTS=ON -DHUXERUI_BUILD_TESTING_LIBRARY=ON \
  -DHUXERUI_PDF_BUILD_EXAMPLES=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

运行输出目录中的 `pdf_preview` 可查看内置示例，也可传入 PDF 文件路径作为第一个参数。
无 UI 依赖的构建方式：

```sh
cmake -S . -B build-core -DHUXERUI_PDF_BUILD_VIEW=OFF -DHUXERUI_PDF_BUILD_TESTS=ON
cmake --build build-core --config Release --parallel
ctest --test-dir build-core -C Release --output-on-failure
```

默认下载 PDFium `chromium/8138` 非 V8 版本并校验哈希，覆盖 Windows x64/ARM64、
Linux glibc x64/ARM64、macOS x64/ARM64/universal。
离线构建可设置 `HUXERUI_PDF_FETCH_PDFIUM=OFF` 和
`HUXERUI_PDF_PDFIUM_ROOT=/path/to/pdfium`。SDK 需包含 `include/`、`lib/`，Windows
动态库还需 `bin/pdfium.dll`。也可预先定义 `PDFium::PDFium` 目标来提供依赖。
Android、iOS、Web 等交叉编译需要自行提供匹配的 SDK，目前尚未验证。

发布应用时需要携带 PDFium 动态库及其完整许可证；Windows 示例与测试会自动复制 DLL。
Linux/macOS 构建目录使用 CMake 运行时搜索路径，分发应用时仍需部署动态库。
完整 SDK 布局、部署方式、参数和 API 说明见 [英文文档](README.md)。

## 范围与许可

当前提供只读查看，不包含编辑、保存、表单交互、签名、OCR、文本选择、可点击链接或流式加载。
支持非表单批注的外观渲染，不初始化表单控件、JavaScript 或文档动作。

本库使用 MIT 许可；PDFium 及其依赖保留各自许可证，见
[third_party/pdfium](third_party/pdfium/README.md)。测试 PDF 由本项目生成，构建和测试无需 Python。
