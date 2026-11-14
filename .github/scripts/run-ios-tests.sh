#!/usr/bin/env bash
set -euo pipefail

: "${TEST_BINARY:?TEST_BINARY is required}"
: "${PDFIUM_DYLIB:?PDFIUM_DYLIB is required}"

test_binary=$(realpath "$TEST_BINARY")
pdfium_dylib=$(realpath "$PDFIUM_DYLIB")

simulator=$(xcrun simctl list devices available --json | python3 -c '
import json
import sys

for runtime, devices in reversed(list(json.load(sys.stdin)["devices"].items())):
    if ".iOS-" not in runtime:
        continue
    for device in devices:
        if device["name"].startswith("iPhone"):
            print(device["udid"])
            sys.exit(0)
sys.exit("No available iPhone Simulator")
')
xcrun simctl boot "$simulator" 2>/dev/null || true
trap 'xcrun simctl shutdown "$simulator" || true' EXIT
xcrun simctl bootstatus "$simulator" -b

# Resolve the SDK's @rpath/libpdfium.dylib from the executable directory.
cp "$pdfium_dylib" "$(dirname "$test_binary")/libpdfium.dylib"
install_name_tool -add_rpath "@executable_path" "$test_binary" 2>/dev/null || true
codesign --force --sign - "$(dirname "$test_binary")/libpdfium.dylib"
codesign --force --sign - "$test_binary"

xcrun simctl spawn "$simulator" "$test_binary"
