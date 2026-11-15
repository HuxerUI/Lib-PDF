#pragma once

#include <string_view>

namespace huxerui::pdf {

/// Library version captured from the repository VERSION file at configure time.
/// The value is a semantic version such as `0.2.0`; it matches the packaged
/// application version because both read the same source file.
[[nodiscard]] std::string_view Version() noexcept;

} // namespace huxerui::pdf
