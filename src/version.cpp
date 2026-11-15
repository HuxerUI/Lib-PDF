#include <huxerui/pdf/version.h>

namespace huxerui::pdf {

std::string_view Version() noexcept {
  return HUXERUI_PDF_VERSION;
}

} // namespace huxerui::pdf
