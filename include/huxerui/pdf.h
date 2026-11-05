#pragma once

#include <huxerui/data.h>
#include <huxerui/file.h>
#include <huxerui/pdf/document.h>
#include <huxerui/resource.h>
#include <huxerui/task.h>
#include <huxerui/view.h>
#include <optional>
#include <source_location>
#include <variant>

namespace huxerui::pdf {
namespace detail {
struct PdfSessionData;
struct PdfAccess;
}

enum class PdfErrorStage { Source, Load, Render, Text, Search, Session };

/// Operational failure. Code/stage are stable; message is diagnostic text, not a machine-readable contract.
struct PdfError {
  PdfErrorCode code = PdfErrorCode::Unknown;
  PdfErrorStage stage = PdfErrorStage::Session;
  std::string message;
  bool operator==(const PdfError&) const = default;
};

template<class T> using PdfResult = Result<T, PdfError>;

/// Explicit input identity. No path/URL guessing or automatic network access.
/// Equal File, RawResource, RawAsset or PdfDocument values preserve the loaded document on recomposition.
/// File contents are not watched. Change PdfOptions::reload to reread an unchanged source.
/// A FileReference source retains its grant. Copies share identity; retain the constructed PdfSource instead
/// of constructing a new FileReference source on each recomposition. An empty source clears the session.
class PdfSource final {
public:
  PdfSource() = default;
  explicit PdfSource(RawResource resource);
  explicit PdfSource(RawAsset asset);
  explicit PdfSource(File file);
  explicit PdfSource(FileReference reference);
  explicit PdfSource(PdfDocument document);
  [[nodiscard]] bool HasValue() const noexcept;
  bool operator==(const PdfSource&) const = default;
private:
  std::variant<std::monostate, RawResource, RawAsset, File, std::shared_ptr<const FileReference>, PdfDocument> value_;
  friend struct detail::PdfAccess;
};

/// Application-controlled intent. Display changes rerender the current document without rereading its source.
struct PdfOptions {
  OpenOptions open;
  int page = 0; ///< Zero-based. An index outside the loaded document reports a Render/Page failure.
  float page_width = 640.0F; ///< Logical raster width before zoom; presentation layout remains application-owned.
  float zoom = 1.0F; ///< Positive finite multiplier; does not change the document's point dimensions.
  float render_scale = 1.5F; ///< Raster pixels per logical unit.
  PageRotation rotation = PageRotation::None; ///< Additional clockwise rotation.
  bool annotations = true;
  std::uint32_t background_rgb = 0xFFFFFF;
  std::uint64_t reload = 0; ///< Change to retry/reload the same source, including a failed load.
  bool operator==(const PdfOptions&) const = default;
};

enum class PdfStatus { Empty, Loading, Rendering, Ready, Failed, Closed };

struct PdfMetadata {
  int pages = 0;
  std::string title;
  std::string author;
  std::string subject;
  bool operator==(const PdfMetadata&) const = default;
};

/// Published page, independent of later requests. Image dimensions include zoom and additional rotation.
struct PdfPageOutput {
  ImageAsset image;
  PageSize size; ///< PDF points, including intrinsic document rotation, excluding additional display rotation.
  int page = 0;
  float zoom = 1.0F;
  PageRotation rotation = PageRotation::None;
  bool annotations = true;
  bool operator==(const PdfPageOutput&) const = default;
};

/// Observed result, distinct from PdfOptions intent. Copies are point-in-time values.
/// Loading a new source clears old metadata/output. Rerendering or a render failure retains the previous page.
struct PdfSnapshot {
  PdfStatus status = PdfStatus::Empty;
  std::optional<PdfMetadata> metadata;
  std::optional<PdfPageOutput> output;
  std::optional<PdfError> error;
  bool operator==(const PdfSnapshot&) const = default;
};

/// Shared UI-thread handle owned by one UsePdf composition. Copies do not extend its active lifetime.
/// Unmounting closes the session and suppresses pending publications; in-flight native work may finish.
/// No Install hook is needed: PDFium is owned by the underlying immutable document handles.
class PdfSession final {
public:
  bool operator==(const PdfSession&) const noexcept = default;
  /// UI-thread query; false before mounting or after the owning composition unmounts.
  [[nodiscard]] bool IsConnected() const;
  /// Reads current state; subscribes the active composition to changes. Throws on wrong-thread/app access.
  [[nodiscard]] PdfSnapshot Snapshot() const;

  /// Lazy worker operations. Start from an event or Task, not unconditionally during composition.
  /// Tasks resume on their owning UI sequence. Closed/NotReady are typed failures. Source replacement or
  /// reload before completion yields Superseded; changing the displayed page does not invalidate these requests.
  /// Task cancellation suppresses delivery. These operations do not overwrite the session's displayed page/error.
  /// Invalid scalar options/query encoding throw synchronously; document-dependent bounds yield Page failures.
  [[nodiscard]] Task<PdfResult<std::string>> ReadTextAsync(int page) const;
  [[nodiscard]] Task<PdfResult<std::vector<SearchMatch>>> SearchAsync(
      int page, std::string query, SearchOptions options = {}) const;
  [[nodiscard]] Task<PdfResult<PdfBitmap>> RenderAsync(int page, RenderOptions options = {}) const;
private:
  explicit PdfSession(std::shared_ptr<detail::PdfSessionData> data) : data_(std::move(data)) {}
  std::shared_ptr<detail::PdfSessionData> data_;
  friend PdfSession UsePdf(PdfSource, PdfOptions, const std::source_location&);
};

/// Retains a session at this composition call site and applies controlled source/display intent after commit.
/// Source reads, decoding and rasterization are asynchronous. Equal inputs preserve the current document/page;
/// display changes retain it and render again. A replaced source/reload cancels obsolete deliveries.
/// Call on the UI thread in a composition; mark reusable callers [[huxerui::composable]].
/// Invalid options throw std::invalid_argument; operational failures appear in Snapshot().error.
[[nodiscard]] PdfSession UsePdf(PdfSource source, PdfOptions options = {},
    const std::source_location& location = std::source_location::current());

/// Presentation only: fitting/alignment do not reload, rerender, or change session ownership.
struct PdfPageProperties {
  ImageFit fit = ImageFit::Contain;
  HorizontalAlignment horizontal_alignment = HorizontalAlignment::Center;
  VerticalAlignment vertical_alignment = VerticalAlignment::Center;
  bool operator==(const PdfPageProperties&) const = default;
};

/// Displays the last published page. Supply bounded layout through the parent or Frame.
/// No toolbar, scrolling, file dialogs, loading text or error placeholder is supplied. Without output it draws nothing.
/// Multiple views may share a session. Unmounting a page view does not close its owning UsePdf session.
[[nodiscard]] View PdfPage(PdfSession session, PdfPageProperties properties = {});
} // namespace huxerui::pdf
