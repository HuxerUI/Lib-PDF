#!/usr/bin/env bash
set -euo pipefail

: "${APK:?APK is required}"
PREVIEW_ID="${PREVIEW_ID:-org.huxerui.lib.pdf.preview}"
ADB="${ADB:-adb}"

"$ADB" install -r "$APK"
"$ADB" shell pm clear "$PREVIEW_ID"
"$ADB" shell am start -W -n "$PREVIEW_ID/.MainActivity"

# Release APKs are not debuggable, so verify the version the running build
# reports through the accessibility tree instead of an app-private file.
for attempt in {1..30}; do
    if "$ADB" shell pidof "$PREVIEW_ID" >/dev/null 2>&1 \
        && "$ADB" shell uiautomator dump /sdcard/pdf-preview-ui.xml >/dev/null 2>&1 \
        && "$ADB" shell cat /sdcard/pdf-preview-ui.xml 2>/dev/null | grep -Fq "\"v${EXPECTED_VERSION:-} · Android\""; then
        echo "The Android preview reports v${EXPECTED_VERSION} · Android"
        exit 0
    fi
    sleep 1
done

printf 'The Android preview did not report version %s at runtime.\n' "${EXPECTED_VERSION:-unknown}" >&2
"$ADB" shell uiautomator dump /sdcard/pdf-preview-ui.xml >/dev/null 2>&1 || true
"$ADB" shell cat /sdcard/pdf-preview-ui.xml 2>/dev/null | tr '>' '\n' | grep -o 'text="v[^"]*"' >&2 || true
"$ADB" logcat -d -t 250 -s AndroidRuntime HuxerUI >&2 || true
exit 1
