#include <huxerui/app.h>

#include <cstdio>
#include <cstring>

namespace {
bool WantsVersion(int argc, char** argv) {
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--version") == 0 || std::strcmp(argv[index], "-v") == 0) return true;
  }
  return false;
}
} // namespace

int main(int argc, char** argv) {
  if (WantsVersion(argc, argv)) {
    std::printf("PDF Preview %s (%s)\n", PDF_PREVIEW_VERSION, PDF_PREVIEW_PLATFORM);
    return 0;
  }
  return huxerui::RunApplication();
}
