#!/usr/bin/env bash
set -euo pipefail

: "${APP:?APP is required}"
PREVIEW_ID="${PREVIEW_ID:-org.huxerui.lib.pdf.preview}"

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

xcrun simctl install "$simulator" "$APP"
container=$(xcrun simctl get_app_container "$simulator" "$PREVIEW_ID" data)
result_file="$container/Library/Caches/pdf-preview-result.txt"
rm -f "$result_file"
xcrun simctl launch "$simulator" "$PREVIEW_ID"
for attempt in {1..30}; do
    if [[ -f "$result_file" ]]; then
        result=$(cat "$result_file")
        if printf '%s' "$result" | grep -Eq '^The PDF field guide\|3\|0\|[0-9]+x[0-9]+$'; then
            echo "$result"
            exit 0
        fi
    fi
    sleep 1
done

printf 'The iOS Simulator preview did not report a rendered sample page.\n' >&2
xcrun simctl spawn "$simulator" log show --last 1m --predicate 'process == "PDF Preview"' >&2 || true
exit 1
