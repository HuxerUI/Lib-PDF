#include <huxerui/pdf.h>
#include <huxerui/huxerui.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include "unicode.h"

#define PDF_DISCARD(...) static_cast<void>((__VA_ARGS__))

namespace huxerui::pdf {
namespace detail {
struct PdfAccess {
  static const auto& Input(const PdfSource& source) { return source.value_; }
};
}

namespace {
bool Positive(float value) { return std::isfinite(value) && value > 0; }

void Validate(const RenderOptions& options) {
  const auto rotation = static_cast<int>(options.rotation);
  if (options.width <= 0 || options.width > 32768 || rotation < 0 || rotation > 3
      || options.background_rgb > 0xFFFFFF || !options.max_pixels)
    throw std::invalid_argument("Invalid PDF render options");
}

void Validate(const PdfOptions& options) {
  const double logical_width = static_cast<double>(options.page_width) * options.zoom;
  if (!Positive(options.page_width) || !Positive(options.zoom) || !Positive(options.render_scale)
      || logical_width > std::numeric_limits<float>::max() || logical_width * options.render_scale > 32768
      || !options.open.max_bytes || options.open.password.find('\0') != std::string::npos || options.page < 0)
    throw std::invalid_argument("Invalid PDF source or display options");
  if (std::ceil(logical_width * options.render_scale) / logical_width > std::numeric_limits<float>::max())
    throw std::invalid_argument("PDF raster scale exceeds the supported range");
  Validate(RenderOptions{.width = 1, .rotation = options.rotation, .background_rgb = options.background_rgb});
}

template<class Function> auto Capture(PdfErrorStage stage, Function&& function) {
  using Value = std::invoke_result_t<Function>;
  try { return PdfResult<Value>::Success(function()); }
  catch (const PdfException& error) { return PdfResult<Value>::Failure({error.Code(), stage, error.what()}); }
  catch (const std::out_of_range& error) { return PdfResult<Value>::Failure({PdfErrorCode::Page, stage, error.what()}); }
  catch (const std::invalid_argument& error) { return PdfResult<Value>::Failure({PdfErrorCode::Limit, stage, error.what()}); }
  catch (const std::bad_alloc& error) { return PdfResult<Value>::Failure({PdfErrorCode::Limit, stage, error.what()}); }
  catch (const std::exception& error) { return PdfResult<Value>::Failure({PdfErrorCode::Unknown, stage, error.what()}); }
}

Task<PdfResult<Bytes>> ReadBounded(AsyncInputStream stream, std::size_t maximum) {
  Bytes bytes;
  for (;;) {
    const auto remaining = maximum - bytes.size();
    auto read = co_await stream.ReadAsync(remaining >= 65536 ? 65536 : remaining + 1);
    if (!read.Succeeded()) co_return PdfResult<Bytes>::Failure({PdfErrorCode::File, PdfErrorStage::Source, read.Error().message});
    auto chunk = std::move(read).Value();
    if (chunk.empty()) co_return PdfResult<Bytes>::Success(std::move(bytes));
    if (chunk.size() > remaining)
      co_return PdfResult<Bytes>::Failure({PdfErrorCode::Limit, PdfErrorStage::Source, "PDF exceeds the input byte limit"});
    bytes.insert(bytes.end(), chunk.begin(), chunk.end());
  }
}

Task<PdfResult<Bytes>> ReadSource(PdfSource source, std::size_t maximum) {
  try {
    const auto& input = detail::PdfAccess::Input(source);
    if (const auto* asset = std::get_if<RawAsset>(&input)) {
      if (!asset->HasValue())
        co_return PdfResult<Bytes>::Failure({PdfErrorCode::File, PdfErrorStage::Source, "PDF raw asset is empty"});
      auto stream = co_await asset->OpenReadAsync();
      co_return co_await ReadBounded(std::move(stream), maximum);
    }
    if (const auto* reference = std::get_if<std::shared_ptr<const FileReference>>(&input)) {
      if (!*reference)
        co_return PdfResult<Bytes>::Failure({PdfErrorCode::File, PdfErrorStage::Source, "PDF file reference is empty"});
      auto stream = co_await (*reference)->OpenReadAsync();
      if (!stream.Succeeded()) co_return PdfResult<Bytes>::Failure({PdfErrorCode::File, PdfErrorStage::Source, stream.Error().message});
      co_return co_await ReadBounded(std::move(stream).Value(), maximum);
    }
    co_return PdfResult<Bytes>::Failure({PdfErrorCode::File, PdfErrorStage::Source, "PDF source cannot be read"});
  } catch (const std::exception& error) {
    co_return PdfResult<Bytes>::Failure({PdfErrorCode::File, PdfErrorStage::Source, error.what()});
  }
}

struct LoadedDocument {
  std::shared_ptr<const PdfDocument> document;
  PdfMetadata metadata;
};

LoadedDocument Describe(PdfDocument document) {
  if (document.PageCount() <= 0) throw PdfException(PdfErrorCode::Page, "PDF document has no readable pages");
  PdfMetadata metadata{document.PageCount(), document.Metadata("Title"), document.Metadata("Author"), document.Metadata("Subject")};
  return {std::make_shared<const PdfDocument>(std::move(document)), std::move(metadata)};
}

Task<PdfResult<LoadedDocument>> OpenSource(PdfSource source, OpenOptions options) {
  const auto& input = detail::PdfAccess::Input(source);
  if (const auto* file = std::get_if<File>(&input)) {
    co_return co_await RunWorker([file = *file, options] {
      return Capture(PdfErrorStage::Load, [&] {
        const auto path = file.Path();
        const std::u8string utf8(reinterpret_cast<const char8_t*>(path.data()), path.size());
        return Describe(PdfDocument::Open(std::filesystem::path(utf8), options));
      });
    });
  }
  if (const auto* document = std::get_if<PdfDocument>(&input)) {
    co_return co_await RunWorker([document = *document] { return Capture(PdfErrorStage::Load, [&] { return Describe(document); }); });
  }
  auto bytes = co_await ReadSource(source, options.max_bytes);
  if (!bytes.Succeeded()) co_return PdfResult<LoadedDocument>::Failure(bytes.Error());
  co_return co_await RunWorker([bytes = std::move(bytes).Value(), options] {
    return Capture(PdfErrorStage::Load, [&] { return Describe(PdfDocument::FromBytes(bytes, options)); });
  });
}

PdfPageOutput RenderPage(const PdfDocument& document, const PdfOptions& options) {
  const double logical_width = static_cast<double>(options.page_width) * options.zoom;
  const auto pixels = static_cast<int>(std::ceil(logical_width * options.render_scale));
  auto bitmap = document.Render(options.page, {.width = pixels, .rotation = options.rotation,
      .annotations = options.annotations, .background_rgb = options.background_rgb});
  return {ImageAsset::FromEncoded(bitmap.ToPng(), static_cast<float>(pixels / logical_width)),
      document.Size(options.page), options.page, options.zoom, options.rotation, options.annotations};
}
} // namespace

namespace detail {
struct PdfSessionData : std::enable_shared_from_this<PdfSessionData> {
  explicit PdfSessionData(TaskScope scope) : tasks(std::move(scope)), snapshot(PdfSnapshot{}) {}
  TaskScope tasks;
  State<PdfSnapshot> snapshot;
  const std::thread::id owner = std::this_thread::get_id();
  bool connected = false;
  std::uint64_t source_generation = 0, render_generation = 0;
  std::optional<PdfSource> source;
  std::optional<PdfError> resolution_error;
  PdfOptions options;
  std::shared_ptr<const PdfDocument> document;
  TaskHandle loading, rendering;

  void Check() const {
    if (std::this_thread::get_id() != owner) throw std::logic_error("PDF session belongs to another UI thread");
    PDF_DISCARD(snapshot.Get()); // Also validates application lifetime/ownership.
  }

  void Close() {
    connected = false;
    ++source_generation; ++render_generation;
    loading.Cancel(); rendering.Cancel();
    document.reset(); source.reset(); options.open.password.clear();
    snapshot = PdfSnapshot{.status = PdfStatus::Closed};
  }

  static Task<void> Render(std::shared_ptr<PdfSessionData> data, std::shared_ptr<const PdfDocument> doc,
                           PdfOptions intent, std::uint64_t generation) {
    try {
      auto result = co_await RunWorker([doc, intent] { return Capture(PdfErrorStage::Render, [&] { return RenderPage(*doc, intent); }); });
      if (!data->connected || data->render_generation != generation) co_return;
      data->snapshot.Update([&](PdfSnapshot& state) {
        if (result.Succeeded()) {
          state.output = std::move(result).Value();
          state.error.reset(); state.status = PdfStatus::Ready;
        } else { state.error = result.Error(); state.status = PdfStatus::Failed; }
      });
    } catch (const std::exception& error) {
      if (data->connected && data->render_generation == generation) data->snapshot.Update([&](PdfSnapshot& state) {
        state.status = PdfStatus::Failed;
        state.error = PdfError{PdfErrorCode::Unknown, PdfErrorStage::Render, error.what()};
      });
    }
  }

  void RenderCurrent() {
    rendering.Cancel();
    const auto generation = ++render_generation;
    snapshot.Update([](PdfSnapshot& state) { state.status = PdfStatus::Rendering; state.error.reset(); });
    rendering = tasks.Launch(Render(shared_from_this(), document, options, generation));
  }

  static Task<void> Load(std::shared_ptr<PdfSessionData> data, PdfSource source, OpenOptions options,
                         std::uint64_t generation) {
    try {
      auto result = co_await OpenSource(std::move(source), std::move(options));
      if (!data->connected || data->source_generation != generation) co_return;
      if (!result.Succeeded()) {
        data->snapshot = PdfSnapshot{.status = PdfStatus::Failed, .error = result.Error()};
        co_return;
      }
      auto loaded = std::move(result).Value();
      data->document = std::move(loaded.document);
      data->snapshot = PdfSnapshot{.status = PdfStatus::Rendering, .metadata = std::move(loaded.metadata)};
      data->RenderCurrent();
    } catch (const std::exception& error) {
      if (data->connected && data->source_generation == generation)
        data->snapshot = PdfSnapshot{.status = PdfStatus::Failed,
            .error = PdfError{PdfErrorCode::Unknown, PdfErrorStage::Load, error.what()}};
    }
  }

  void Update(PdfSource next_source, PdfOptions intent, std::optional<PdfError> error) {
    const bool reload = !source || *source != next_source || options.open != intent.open
        || options.reload != intent.reload || resolution_error != error;
    const bool changed = options != intent;
    options = std::move(intent);
    resolution_error = error;
    if (reload) {
      loading.Cancel(); rendering.Cancel();
      const auto generation = ++source_generation;
      ++render_generation;
      source = next_source;
      document.reset();
      snapshot = PdfSnapshot{};
      if (error) { snapshot = PdfSnapshot{.status = PdfStatus::Failed, .error = std::move(error)}; return; }
      if (!next_source.HasValue()) return;
      snapshot = PdfSnapshot{.status = PdfStatus::Loading};
      loading = tasks.Launch(Load(shared_from_this(), std::move(next_source), options.open, generation));
    } else if (changed && document) { RenderCurrent(); }
  }
};
} // namespace detail

PdfSource::PdfSource(RawResource resource) : value_(std::move(resource)) {}
PdfSource::PdfSource(RawAsset asset) : value_(std::move(asset)) {}
PdfSource::PdfSource(File file) : value_(std::move(file)) {}
PdfSource::PdfSource(FileReference reference) : value_(std::make_shared<const FileReference>(std::move(reference))) {}
PdfSource::PdfSource(PdfDocument document) : value_(std::move(document)) {}
bool PdfSource::HasValue() const noexcept { return value_.index() != 0; }

namespace {
void Check(const std::shared_ptr<detail::PdfSessionData>& data) {
  if (!data) throw std::logic_error("Moved-from PDF session");
  data->Check();
}

template<class Function> auto Operate(std::shared_ptr<detail::PdfSessionData> data, PdfErrorStage stage, Function operation)
    -> Task<PdfResult<std::invoke_result_t<Function, const PdfDocument&>>> {
  using Value = std::invoke_result_t<Function, const PdfDocument&>;
  Check(data);
  if (!data->connected) co_return PdfResult<Value>::Failure({PdfErrorCode::Closed, stage, "PDF session is closed"});
  if (!data->document) co_return PdfResult<Value>::Failure({PdfErrorCode::NotReady, stage, "PDF document is not ready"});
  const auto generation = data->source_generation;
  std::optional<PdfResult<Value>> result;
  try {
    result.emplace(co_await RunWorker([document = data->document, stage, operation = std::move(operation)] {
      return Capture(stage, [&] { return operation(*document); });
    }));
  } catch (const std::exception& error) {
    result.emplace(PdfResult<Value>::Failure({PdfErrorCode::Unknown, stage, error.what()}));
  }
  Check(data);
  if (!data->connected) co_return PdfResult<Value>::Failure({PdfErrorCode::Closed, stage, "PDF session is closed"});
  if (data->source_generation != generation)
    co_return PdfResult<Value>::Failure({PdfErrorCode::Superseded, stage, "PDF source was replaced"});
  co_return std::move(*result);
}
} // namespace

bool PdfSession::IsConnected() const { Check(data_); return data_->connected; }
PdfSnapshot PdfSession::Snapshot() const { Check(data_); return data_->snapshot; }

Task<PdfResult<std::string>> PdfSession::ReadTextAsync(int page) const {
  Check(data_);
  if (page < 0) throw std::invalid_argument("PDF page must be nonnegative");
  return Operate(data_, PdfErrorStage::Text, [page](const PdfDocument& doc) { return doc.Text(page); });
}

Task<PdfResult<std::vector<SearchMatch>>> PdfSession::SearchAsync(int page, std::string query, SearchOptions options) const {
  Check(data_);
  if (page < 0 || query.size() > 1024U * 1024U || options.max_results > 1000000)
    throw std::invalid_argument("Invalid PDF search request");
  PDF_DISCARD(detail::ToUtf16(query));
  return Operate(data_, PdfErrorStage::Search, [page, query = std::move(query), options](const PdfDocument& doc) {
    return doc.Search(page, query, options);
  });
}

Task<PdfResult<PdfBitmap>> PdfSession::RenderAsync(int page, RenderOptions options) const {
  Check(data_); Validate(options);
  if (page < 0) throw std::invalid_argument("PDF page must be nonnegative");
  return Operate(data_, PdfErrorStage::Render, [page, options](const PdfDocument& doc) { return doc.Render(page, options); });
}

PdfSession UsePdf(PdfSource source, PdfOptions options, const std::source_location& location) {
  Validate(options);
  auto tasks = UseTaskScope();
  std::shared_ptr<detail::PdfSessionData> data = UseState(std::make_shared<detail::PdfSessionData>(tasks), location);
  std::optional<PdfError> resolution_error;
  if (const auto* resource = std::get_if<RawResource>(&detail::PdfAccess::Input(source))) {
    try { source = PdfSource(UseRawResource(*resource)); }
    catch (const std::exception& error) { resolution_error = PdfError{PdfErrorCode::File, PdfErrorStage::Source, error.what()}; }
  }
  Lifecycle([data] {
    data->connected = true;
    return [data] { data->Close(); };
  }, data);
  Lifecycle([data, source, options, resolution_error] {
    data->Update(source, options, resolution_error);
  }, data, source, options, resolution_error);
  return PdfSession(std::move(data));
}

View PdfPage(PdfSession session, PdfPageProperties properties) {
  const auto fit = properties.fit;
  if ((fit != ImageFit::Contain && fit != ImageFit::Cover && fit != ImageFit::Fill
       && fit != ImageFit::None && fit != ImageFit::ScaleDown)
      || (properties.horizontal_alignment != HorizontalAlignment::Start && properties.horizontal_alignment != HorizontalAlignment::Center
          && properties.horizontal_alignment != HorizontalAlignment::End)
      || (properties.vertical_alignment != VerticalAlignment::Start && properties.vertical_alignment != VerticalAlignment::Center
          && properties.vertical_alignment != VerticalAlignment::End))
    throw std::invalid_argument("Invalid PDF page fit or alignment");
  return Scope([session = std::move(session), properties]() -> View {
    const auto snapshot = session.Snapshot();
    if (!snapshot.output) return Canvas([](PaintContext&, Size) {});
    return Image(snapshot.output->image).Fit(properties.fit)
        .Align(properties.horizontal_alignment, properties.vertical_alignment);
  });
}
} // namespace huxerui::pdf

#undef PDF_DISCARD
