#!/usr/bin/env bash
set -euo pipefail

: "${TEST_BINARY:?TEST_BINARY is required}"
: "${PDFIUM_LIBRARY:?PDFIUM_LIBRARY is required}"
ADB="${ADB:-adb}"

"$ADB" wait-for-device
"$ADB" shell 'rm -rf /data/local/tmp/libpdf-tests && mkdir -p /data/local/tmp/libpdf-tests/fixtures'
"$ADB" push "$TEST_BINARY" /data/local/tmp/libpdf-tests/pdf_document_tests
"$ADB" push "$PDFIUM_LIBRARY" /data/local/tmp/libpdf-tests/libpdfium.so
"$ADB" push tests/fixtures/sample.pdf tests/fixtures/encrypted.pdf /data/local/tmp/libpdf-tests/fixtures/
"$ADB" shell chmod 755 /data/local/tmp/libpdf-tests/pdf_document_tests

if ! "$ADB" shell 'cd /data/local/tmp/libpdf-tests && LD_LIBRARY_PATH=/data/local/tmp/libpdf-tests ./pdf_document_tests'; then
    "$ADB" logcat -d -t 250 >&2 || true
    exit 1
fi
