# Scave 的 Lib 接口约定与 Lib-PDF 对齐依据

本文根据 GitHub 用户 **FinalScave**（Git 提交作者 `Scave`）的公开代码归纳。
这是对实际接口的研究记录，不代表组织另行发布的强制标准。参考固定提交，避免把其他作者后续修改混入样本。

## 参考样本

| 库 | Scave 提交 | 重点接口 |
| --- | --- | --- |
| Lib-Lottie | [7dad800](https://github.com/HuxerUI/Lib-Lottie/blob/7dad80083cdd48ed2c88ac228969fc8027d55e07/include/huxerui/lottie.h) | `LottieSource`、`LottieControl`、`LottieEvents`、`Install`、`Lottie` |
| Lib-Media | [cf1f31b](https://github.com/HuxerUI/Lib-Media/blob/cf1f31b7b4fa5b4488fe0f0591d8b5b01cf8eb85/include/huxerui/mediaplayer.h) | `MediaSource`、`UseMediaPlayer`、`MediaPlayer::Snapshot`、`VideoSurface` |
| Lib-Camera | [cada053](https://github.com/HuxerUI/Lib-Camera/blob/cada0539098791019bc5330ffd87009acc29d68a/include/huxerui/camera.h) | `UseCamera`、`CameraSession`、`CameraStatus`、`CapturePhotoAsync`、`CameraPreview` |
| Lib-Charts | [e281c5e](https://github.com/HuxerUI/Lib-Charts/blob/e281c5e38576a19bc50501e97543dafc9922ac8c/include/huxerui/charts.h) | 不可变数据快照、链式声明、受控选择、`ChartEvents` |
| Lib-SQLite | [5e3d040](https://github.com/HuxerUI/Lib-SQLite/blob/5e3d040079629e698ca2f456ad79042cbbcd6aba/include/huxerui/sqlite.h) | `Database`、`OpenAsync`、`Task<Result<T>>`、串行工作队列 |
| Lib-WebView | [d078203](https://github.com/HuxerUI/Lib-WebView/blob/d07820374b4a5f75659cd29e4e4fe2bcc899b8ef/include/huxerui/webview.h) | `WebViewProperties`、`UseWebViewController`、命令接受语义 |

不同样本对应不同 SDK 版本。例如 Camera/WebView 的上述版本使用 `RootContext` 和
`root_hooks`，较新的 Lottie/Media 使用 `ApplicationContext` 和 `application_hooks`。
接入时应匹配目标 SDK，不能直接复制旧类型名。

## 1. 接口形态取决于对象职责

不存在“所有组件必须是链式类”这一共同规则：

- Charts 的组件继承 `View`，用 `.XAxis(...)`、`.Selection(...)` 等右值成员函数配置声明。
- Lottie 接收 source/control 值，直接返回普通 `View`。
- Camera/Media 用 `Use*` 在组合中保留会话，再将句柄交给展示组件。
- SQLite 的数据库是可复制的异步连接句柄，不是 UI 组件，也没有对应的 `UseDatabase` 要求。

共同点是职责清楚：声明描述希望呈现或执行的内容；句柄管理可操作对象；展示组件接入
HuxerUI 布局。不要仅为了外形统一而把所有接口改成同一种类。

命名使用 PascalCase 类型及函数、snake_case 数据字段；内部实现隐藏在 `detail`/实现类中。
命名空间既有 `huxerui` 下带前缀的 Charts/WebView，也有 `huxerui::camera`、
`huxerui::media`、`huxerui::lottie`。避免与 SDK 已有类型形成歧义比机械统一前缀更重要。

## 2. 输入意图与观测结果分开

- `CameraOptions::active` 是应用请求；`CameraStatus::state` 是后端实际状态。
- `LottiePlayback`/`LottieProgress` 是输入控制值，不冒充已经播放到的位置。
- Charts 的 `Selection(...)` 接受权威值；`SelectionRequested` 只提出请求，应用更新
  `State` 后才改变受控选择。
- Media 的命令返回 `bool` 表示请求是否被接受，异步结果从状态和通知观察。

因此，页码、缩放等若设计成受控属性，就必须允许应用在重组时更新；不能只提供首次挂载的
`initial_*` 参数，却让应用无法读取或控制当前值。若使用命令句柄，也要说明接受、完成和失败
分别如何表示。

## 3. 状态与生命周期有明确所有者

`UseCamera` 和 `UseMediaPlayer` 把活跃会话绑定到组合生命周期：同一调用位置重组复用会话，
卸载时停止或断开；复制句柄不会让已卸载的会话继续活跃。

展示组件的寿命与会话的寿命可以不同：卸载 `CameraPreview` 不等于停止捕获，卸载
`VideoSurface` 不等于停止音频。对独立数据对象则需要另行说明共享所有权，例如 SQLite
句柄共享连接和关闭状态。

文档必须回答：谁创建、谁释放、复制是否共享、哪个线程可访问、卸载后句柄是否仍可使用。
不能把组合会话的所有权规则直接套在可独立使用的文档对象上。

## 4. 库承担耗时工作的调度

Lottie 的加载和渲染进入后端队列；SQLite 对外提供异步操作并按连接串行执行。
Camera 的拍照返回 `Task<CameraResult<ImageAsset>>`，Media 的原生准备也异步完成。

应用通过 `UseTaskScope()` 管理任务，在所属 UI 序列接收结果。控件正常组合不应要求调用者
每次手动创建线程或同步解析大型文件。取消、替换 source、卸载时要抑制过期结果和回调；
“取消交付”与“立即中断原生操作”不是同一承诺。

显式的底层同步 API 可以保留，但应与默认 UI 接入路径分开，并标清阻塞和线程要求。

## 5. 错误区分参数误用与运行失败

- 参数越界、非法枚举、错误线程、缺少组合上下文等通常使用异常，并写清触发时机。
- 加载、解码、设备或文件操作失败，通过带类型的 error、状态、事件或 `Result` 交付。
- 新版 Camera/Media 的相关结果复用 SDK `Result<T, Error>`；参考版本的 SQLite 有自己的
  `sqlite::Result<T>`，不能据此声称所有历史库都使用完全相同的 Result 类型。
- 错误代码/stage 可用于程序判断，message 是诊断文本，不保证措辞稳定。

UI 组件通常不直接把失败转换为固定语言的 `Text`；应用决定错误提示、重试和空态。
Lottie 的失败事件还明确区分加载失败清空画面与渲染失败保留最后一帧。

## 6. 事件使用 SDK 的机制，句柄订阅说明解绑

声明组件可通过 `Events` 中的 `Event<void(...)>` 和 `.On<Events::...>(handler)` 暴露事件，
例如 Lottie/Charts。句柄也可以提供订阅接口，例如 Media 的组合期 `OnProgress` 与手动
`ConnectProgress`；两者需要写明订阅、解绑和回调线程。

事件名称区分请求与事实：`SelectionRequested` 是请求，`Loaded`/`Failed` 是结果。
`const T&` 参数说明只在回调期间借用，调用者要保留就复制。不要让业务解析内部字符串。

## 7. 与 SDK 的资源、布局和样式系统协作

- Lottie source 使用 `RawResource`/`RawAsset`/`File`；Media source 使用 `File`/
  `FileReference`/显式 HTTP source，保留来源语义及访问授权。
- 不把任意字符串同时猜测为路径、URL、资源 ID。
- source 的相等性、重组是否重新读取、同一路径文件改变后如何重载都要有约定。
- 使用 `ImageAsset`、`Bytes`、`ImageFit`、`Size`、`StringVariant` 等已有 SDK 类型，
  不为同一概念再造一组不兼容的 UI 类型。
- 通用布局通过 `.With(Frame{...})` 等 SDK 修饰符提供。配置或原生分辨率不应暗中替代布局。
- 核心 Camera/Media 展示组件不附带拍照/播放工具栏、权限弹窗或固定错误文案。
  完整交互工作区由示例应用组合。Charts 的可配置标题/图例说明这一点不是“组件不能有任何装饰”。

需要应用级平台服务的库提供 `Install`，注册本身不应自动打开文件、启动播放或请求权限。
纯数据和不需要应用级服务的组件不必为了形式统一增加空的 Install。

## 8. 注释是行为契约

接口注释除功能名称外，应说明：默认值、单位、范围、参数有效期、返回值所有权、
源身份、重组行为、线程、取消/卸载、错误、平台限制及最小用例。

尤其不能混淆：PDF 点与逻辑 UI 单位、像素；UTF-8 字节位置与 PDF 字符索引；命令被接受
与渲染完成；一份状态快照与会继续更新的资源句柄。

## Lib-PDF 0.2 的对应设计

本次直接替换公开 UI 接口，不保留旧的阅读器接口或兼容层。

| 研究约定 | 当前接口 |
| --- | --- |
| 来源有明确语义与身份 | `PdfSource` 显式包装 SDK 资源、文件、文件引用或独立文档；`reload` 明确重新读取 |
| 控制意图与观测结果分开 | `PdfOptions` 由应用 State 控制；`PdfSnapshot` 报告已完成的状态与页面 |
| 组合期拥有会话 | `UsePdf` 保留 `PdfSession`；复制共享身份，拥有者卸载后关闭 |
| 核心展示只负责呈现 | `PdfPage(session, properties)` 不创建工具栏、滚动、弹窗或错误占位 |
| 库内负责耗时工作 | 会话自动异步加载/渲染；独立操作为 `ReadTextAsync`、`SearchAsync`、`RenderAsync` |
| 类型化失败 | `PdfResult<T>` 复用 SDK Result，`PdfError` 提供 code/stage/message；标量误用同步抛出异常 |
| 过期请求不能覆盖当前来源 | 来源与渲染请求分代，替换/取消/卸载抑制旧发布；独立操作可返回 Superseded |
| 无需形式化地增加 Install | PDFium 由不可变文档持有，不注册原生平台模块，因而不增加空 Install |
| 避免 SDK 命名冲突 | 旋转类型使用 `PageRotation` |
| 独立底层保留明确用途 | `PdfDocument` 是同步、可跨工作线程共享的文档句柄，后端异常为 `PdfException` |

这里采用 Camera/Media 的“会话 + 展示”模式，状态经 Snapshot 观察，操作结果通过 Task/Result
交付；没有为了凑齐另一套事件命名而再添加一组重复的回调接口。完整阅读器仍由 demo 组合。

对应的行为测试涵盖受控更新、普通重组不重新渲染、文件主动重载、并列会话独立性、密码重载、
异步结果、线程误用、请求替换、错误保留画面以及卸载后的关闭状态。
