set -eu

PDFIUM_SOURCE=$(find "$HUXERUI_CORE_BUILD_DIR" -path '*/pdfium-runtime/libpdfium.dylib' -print -quit)
if [ -z "$PDFIUM_SOURCE" ] || [ ! -f "$PDFIUM_SOURCE" ]; then
  echo "error: PDFium dylib is missing under $HUXERUI_CORE_BUILD_DIR" >&2
  exit 1
fi
if [ -z "${FRAMEWORKS_FOLDER_PATH:-}" ]; then
  echo "error: the app bundle has no Frameworks folder" >&2
  exit 1
fi

PDFIUM_DESTINATION="$TARGET_BUILD_DIR/$FRAMEWORKS_FOLDER_PATH"
cmake -E make_directory "$PDFIUM_DESTINATION"
cmake -E copy_if_different "$PDFIUM_SOURCE" "$PDFIUM_DESTINATION/libpdfium.dylib"

if [ "${CODE_SIGNING_ALLOWED:-YES}" != "NO" ] && [ -n "${EXPANDED_CODE_SIGN_IDENTITY:-}" ]; then
  codesign --force --sign "$EXPANDED_CODE_SIGN_IDENTITY" "$PDFIUM_DESTINATION/libpdfium.dylib"
fi
