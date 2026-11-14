#!/usr/bin/env bash
set -euo pipefail

: "${HUXERUI_HOME:?HUXERUI_HOME is required}"

root=$(pwd)
project="$root/examples/preview"
core_build="$root/build-ios-core"
arch=$(uname -m)
if [[ "$arch" != "arm64" ]]; then
    arch=x86_64
fi

export HUXERUI_PROJECT_ROOT="$project"
export HUXERUI_CORE_BUILD_DIR="$core_build"
export ARCHS="$arch"
export SDKROOT
SDKROOT=$(xcrun --sdk iphonesimulator --show-sdk-path)
export IPHONEOS_DEPLOYMENT_TARGET=17.0
export CONFIGURATION=Release
export PDF_PREVIEW_MOBILE_SMOKE=ON

/bin/sh "$project/platform/ios/Scripts/build_huxerui_core.sh"

core_dir="$core_build/huxerui-ios/example_pdf"
test -f "$core_dir/libexample_pdf_huxerui.a"
test -f "$core_dir/link.rsp"

app="$project/dist/ios/PDF Preview.app"
rm -rf "$app"
mkdir -p "$app/Frameworks"
cp "$project/platform/ios/App/Info.plist" "$app/Info.plist"
sed -i '' \
    -e 's/\$(DEVELOPMENT_LANGUAGE)/en/g' \
    -e 's/\$(EXECUTABLE_NAME)/PDF Preview/g' \
    -e 's/\$(PRODUCT_BUNDLE_IDENTIFIER)/org.huxerui.lib.pdf.preview/g' \
    -e 's/\$(PRODUCT_NAME)/PDF Preview/g' \
    -e 's/\$(MARKETING_VERSION)/0.1.0/g' \
    -e 's/\$(CURRENT_PROJECT_VERSION)/1/g' \
    "$app/Info.plist"
/usr/libexec/PlistBuddy -c 'Delete :UILaunchStoryboardName' "$app/Info.plist"
/usr/libexec/PlistBuddy -c 'Add :MinimumOSVersion string 17.0' "$app/Info.plist"
/usr/libexec/PlistBuddy -c 'Add :UIDeviceFamily array' "$app/Info.plist"
/usr/libexec/PlistBuddy -c 'Add :UIDeviceFamily:0 integer 1' "$app/Info.plist"
/usr/libexec/PlistBuddy -c 'Add :UIDeviceFamily:1 integer 2' "$app/Info.plist"
/usr/libexec/PlistBuddy -c 'Add :CFBundleSupportedPlatforms array' "$app/Info.plist"
/usr/libexec/PlistBuddy -c 'Add :CFBundleSupportedPlatforms:0 string iPhoneSimulator' "$app/Info.plist"

pdfium=$(find "$core_build" -path '*/pdfium-runtime/libpdfium.dylib' -print -quit)
test -n "$pdfium"

if ! xcrun --sdk iphonesimulator clang++ \
    -arch "$arch" \
    -mios-simulator-version-min=17.0 \
    -isysroot "$SDKROOT" \
    -std=c++20 \
    -fobjc-arc \
    -I "$HUXERUI_HOME/include" \
    "$project/platform/ios/App/main.mm" \
    @"$core_dir/link.rsp" \
    "$pdfium" \
    -Wl,-rpath,@executable_path/Frameworks \
    -o "$app/PDF Preview"; then
    echo "link.rsp:" >&2
    cat "$core_dir/link.rsp" >&2
    exit 1
fi

resources="$core_build/huxerui-ios/example_pdf/resources/package"
test -f "$resources/huxerui/resources.bin"
mkdir -p "$app/HuxerUI"
cp -R "$resources/." "$app/HuxerUI/"

cp "$pdfium" "$app/Frameworks/libpdfium.dylib"

codesign --force --sign - "$app/Frameworks/libpdfium.dylib"
codesign --force --deep --sign - "$app"

echo "Built $app"
