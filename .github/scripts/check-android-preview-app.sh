#!/usr/bin/env bash
set -euo pipefail

: "${APK:?APK is required}"
PREVIEW_ID="${PREVIEW_ID:-org.huxerui.lib.pdf.preview}"
ADB="${ADB:-adb}"

"$ADB" install -r "$APK"
"$ADB" shell pm clear "$PREVIEW_ID"
"$ADB" shell am start -W -n "$PREVIEW_ID/.MainActivity"
for attempt in {1..30}; do
    if pid=$("$ADB" shell pidof "$PREVIEW_ID" | tr -d '\r'); then
        echo "The Android preview is running: pid=$pid"
        exit 0
    fi
    sleep 1
done

printf 'The Android preview did not stay running after launch.\n' >&2
"$ADB" logcat -d -t 250 -s AndroidRuntime HuxerUI >&2 || true
exit 1
