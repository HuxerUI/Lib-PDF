# Lib-PDF

[English](README.md)

为 [HuxerUI](https://github.com/HuxerUI/HuxerUI) 提供基于 PDFium 的 PDF 扩展。
C++20，目标 `HuxerUI::PDF`，命名空间 `huxerui::pdf`。
0.2 接口按 Scave 的 Camera/Media 会话与展示模式组织。

- `PdfSource`：明确的资源、内存、文件、文件引用或独立文档来源。
- `UsePdf(source, options)`：组合期持有会话，应用受控的页码与显示参数。
- `PdfSession::Snapshot()`：读取并订阅元数据、状态、已发布页面与类型化错误。
- `PdfPage(session, properties)`：只负责展示，布局、工具栏及错误界面由应用提供。
- `ReadTextAsync`、`SearchAsync`、`RenderAsync`：库内调度的异步操作，返回 `Task<PdfResult<T>>`。
- 无 UI 依赖时仍可独立使用同步文档层。

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

[[huxerui::composable]]
View Reader() {
  auto options = UseState(pdf::PdfOptions{});
  auto session = pdf::UsePdf(pdf::PdfSource(File("/absolute/path/to/guide.pdf")), options);
  const auto state = session.Snapshot();
  return pdf::PdfPage(session).With(Grow{});
}
```

文件路径需要在应用中可访问，也可以传入文件选择器返回的 `FileReference`。
本库无需 `Install`：底层文档持有 PDFium 生命周期，组合的 TaskScope 管理会话任务。

## 控制、状态与生命周期

`PdfOptions` 是应用请求：`page` 从 0 开始，`zoom` 是正有限倍率，`page_width` 默认 640 逻辑单位，
`render_scale` 默认 1.5，还包含 `PageRotation`、批注开关、不透明 RGB 背景、打开参数与 `reload`。
参数在组合提交后应用。改变显示参数复用已打开的文档；改变来源、密码/打开参数或 reload 标记则重新打开。
若来源本身是已打开的不可变 PdfDocument，reload 会复用该文档重新准备输出；打开参数适用于字节和文件来源。

`Snapshot()` 返回时间点快照，包含 Empty/Loading/Rendering/Ready/Failed/Closed 状态、
可选元数据、可选 `output` 和可选 `PdfError`。`output` 的页码与参数表示已经完成的结果，
与尚未完成的输入意图区分。重组时读取会订阅后续变化。

切换来源会清空旧页面与元数据；同一文档重绘期间保留上一页，重绘失败也保留最后成功的画面。
已加载文档的页码越界以 Render 阶段的 Page 错误报告。

`PdfPage` 无工具栏、滚动、文件弹窗、加载文案或错误占位。没有 output 时绘制空内容；
有画面时按 `PdfPageProperties` 在有界布局中拟合，默认居中的 `ImageFit::Contain`。
多个展示组件可以共享会话，卸载展示组件不等于关闭拥有它的 `UsePdf` 会话。

缩放影响已发布图像的固有逻辑尺寸与像素分辨率；固定视口内的拟合仍遵循 fit 规则。
需要滚动缩放时，按 `output.image.IntrinsicSize()` 给页面设置 Frame，再在应用中组合滚动视图，
demo 即采用这种方式。`output.size` 是 PDF 点尺寸（1/72 英寸），不随显示缩放改变。

同一调用位置的 `UsePdf` 重组会复用会话。句柄复制共享身份，但不能延长卸载后的活跃寿命。
卸载会取消交付并阻止过期结果发布，已经执行的工作可能稍后才释放文档。
句柄只允许所属应用的 UI 线程访问；跨线程/应用误用抛出 `std::logic_error`。
拥有者卸载后、应用仍存活时会话报告 Closed；应用关闭后其 SDK State 生命周期失效。

## 来源与异步操作

`PdfSource` 显式接收 `RawResource`、`RawAsset`、`File`、`FileReference` 或 `PdfDocument`。
默认空来源清空会话，不猜测 URL 或路径，不自动下载或监视文件。
相同来源值重组不重新读取；改变 `reload` 可重新读取同一路径或重试失败。
FileReference 来源保留访问授权，应该保留构造后的 PdfSource，避免每次重组重新包装引用而产生新身份。

从事件或生命周期任务中启动操作：

```cpp
tasks.Launch([session, notice]() -> Task<void> {
  auto found = co_await session.SearchAsync(0, "PDF", {.whole_word = true});
  notice = found.Succeeded()
      ? std::to_string(found.Value().size()) + " matches"
      : found.Error().message;
});
```

`ReadTextAsync(page)` 提取 UTF-8 文本，`SearchAsync` 返回字符索引与矩形，`RenderAsync` 返回
独立 RGBA8 位图。库负责工作线程调度，await 后回到所属 UI 序列；这些请求不改写当前展示页或会话错误。
任务是惰性的，在执行时检查可用性。NotReady/Closed 表示当前不可操作；完成前来源被替换或重新加载时，
返回 Superseded。仅改变显示页码不会取消指定页面的独立请求。取消任务会抑制结果交付。

`PdfResult<T>` 复用 SDK `Result<T, PdfError>`，先检查 `Succeeded()` 再取 `Value()`/`Error()`。
错误的 code/stage 可供程序判断，message 只用于诊断。非法标量参数和查询编码同步抛出异常，
依赖已加载文档的页码边界通过结果返回。文本提取不包含 OCR；搜索使用左下角为原点的未旋转 PDF 坐标，
返回 PDFium 字符索引，不是 UTF-8 字节或 UTF-16 偏移。

## 独立同步文档 API

只需包含 `<huxerui/pdf/document.h>`，无需 HuxerUI 头文件：

```cpp
auto doc = huxerui::pdf::PdfDocument::Open("protected.pdf", {.password = "secret"});
auto text = doc.Text(0);
auto matches = doc.Search(0, "PDF");
auto pixels = doc.Render(0, {.width = 1440,
    .rotation = huxerui::pdf::PageRotation::Clockwise90});
auto png = pixels.ToPng();
```

这是显式同步接口，可在工作线程运行。文档共享所有权，内存输入会复制，支持 Windows 中文路径。
后端失败抛出 `PdfException`，参数错误抛出 `std::invalid_argument`，越界页码抛出 `std::out_of_range`。
所有库内 PDFium 调用（包括销毁）由同一互斥锁串行化；不要在外部独立初始化、销毁或并发调用 PDFium。

默认输入限制 256 MiB，渲染限制约 3200 万像素（硬上限约 6400 万，单轴 32768），
每页文本最多约 1600 万字符，搜索最多 1000 条。资源/文件引用按有界块读取，超过输入上限的字节不会被接受。

## 构建、demo 与测试

需要 CMake 3.20+、C++20，UI 部分需要 HuxerUI。已验证 SDK 提交：
`cadb1696cfa7b87d9d2d316b3ecba07b85a46962`。

```sh
cmake -S . -B build -DHUXERUI_HOME=/path/to/HuxerUI \
  -DHUXERUI_PDF_BUILD_TESTS=ON -DHUXERUI_BUILD_TESTING_LIBRARY=ON \
  -DHUXERUI_PDF_BUILD_EXAMPLES=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

demo 支持本地/密码 PDF、翻页、缩放、旋转、批注开关、搜索、文本复制和主题切换。
它只使用新的会话 API 执行 PDF 工作，应用不再手动建立 RunWorker 管线。
保留其他 Lib 的 `examples/preview/{src,resources,platform}` 结构，可独立构建：

```sh
cmake -S examples/preview -B build-demo -DHUXERUI_HOME=/path/to/HuxerUI
cmake --build build-demo --config Release --target example_pdf --parallel
```

Windows 运行 `build-demo/Release/example_pdf.exe`。详见 [demo 说明](examples/preview/README.md)。
独立文档构建使用 `-DHUXERUI_PDF_BUILD_VIEW=OFF`。CI 验证 Windows/Linux/macOS 文档层，以及
Windows 会话与 demo；测试覆盖受控参数、来源重载、异步结果、线程误用、请求替换和卸载。

默认下载固定 PDFium `chromium/8138` 非 V8 版本并校验 SHA-256。macOS SDK 要求 13 及以上。
离线可设置 `HUXERUI_PDF_FETCH_PDFIUM=OFF` 和 `HUXERUI_PDF_PDFIUM_ROOT=/path/to/pdfium`。
发布应用时需部署 PDFium 动态库、资源包与完整许可证。支持范围和完整部署说明见 [英文文档](README.md)。

本库为只读 PDF 扩展，不包含编辑、保存、表单交互、签名、OCR、页面文本选择、可点击链接和增量加载。
demo 的提取文本面板可以选择与复制。库使用 MIT 许可，PDFium 及其依赖保留各自许可。
设计依据见 [Scave 接口研究](docs/API-CONVENTIONS.zh-CN.md)。
