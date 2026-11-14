set -eu

PDFIUM_SOURCE="$HUXERUI_CORE_BUILD_DIR/pdfium-runtime/libpdfium.dylib"
if [ ! -f "$PDFIUM_SOURCE" ]; then
  echo "error: PDFium dylib is missing: $PDFIUM_SOURCE" >&2
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
