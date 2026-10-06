#!/bin/bash
# macOS deployment script
# Usage: ./scripts/deploy-macos.sh "EZ4Connect" "build" "x86_64" "false"

set -euo pipefail

TARGET_NAME="${1:-EZ4Connect}"
BUILD_DIR="${2:-build}"
ARCH="${3:-arm64}"
NIGHTLY="${4:-false}"
APP_PATH="$TARGET_NAME.app"
case "$ARCH" in
    arm64|x86_64) ;;
    *) echo "Unsupported macOS architecture: $ARCH" >&2; exit 2 ;;
esac

# Copy app bundle
cp -R "$BUILD_DIR/$TARGET_NAME.app" .
cp docs/THIRD_PARTY_NOTICES.txt "$APP_PATH/Contents/Resources/"

# Download and extract zju-connect
ZJU_ARCH="${ARCH}"
if [ "$ARCH" = "x86_64" ]; then
    ZJU_ARCH="amd64"
fi

ZJU_RELEASE_PATH="latest/download"
if [ "$NIGHTLY" = "true" ]; then
    ZJU_RELEASE_PATH="download/nightly"
fi

curl -LO "https://github.com/Mythologyli/zju-connect/releases/$ZJU_RELEASE_PATH/zju-connect-darwin-$ZJU_ARCH.zip"
unzip -o "zju-connect-darwin-$ZJU_ARCH.zip"
rm "zju-connect-darwin-$ZJU_ARCH.zip"

# Copy zju-connect into app bundle
cp zju-connect "$APP_PATH/Contents/MacOS/"

# Run macdeployqt
macdeployqt "$APP_PATH"

# Reduce bundle size by stripping unused slices from universal Mach-O binaries.
thinned_count=0
before_size=0
after_size=0
while IFS= read -r -d '' binary; do
    file_output="$(file "$binary")"
    if [[ "$file_output" != *"Mach-O universal binary"* ]]; then
        continue
    fi

    lipo_info="$(lipo -info "$binary" 2>/dev/null || true)"
        if [[ "$lipo_info" != *"$ARCH"* ]]; then
            echo "Skipping universal binary without $ARCH slice: $binary" >&2
            continue
        fi

        original_mode="$(stat -f%Lp "$binary")"
        thin_output="$(mktemp)"
        lipo "$binary" -thin "$ARCH" -output "$thin_output"
        chmod "$original_mode" "$thin_output"
        mv "$thin_output" "$binary"

done < <(find "$APP_PATH" -type f -print0)

# macdeployqt has a bug that it doesn't copy the Qt translations to the bundle
# so we need to copy them manually
QT_TRANSLATIONS=$(qmake -query QT_INSTALL_TRANSLATIONS)
mkdir -p "$APP_PATH/Contents/translations"
if [ -d "$QT_TRANSLATIONS" ]; then
    cp -R "$QT_TRANSLATIONS"/qtbase_*.qm "$APP_PATH/Contents/translations"
fi

# macdeployqt always installs the SDK's original Cocoa plugin. Replace it only
# after deployment and architecture thinning, using the matching Qt 6.10.3 SDK.
# A build, validation, or regression failure aborts packaging instead of shipping
# the unpatched plugin which can crash on macOS 27 tray callbacks.
./scripts/build-macos-cocoa-tray-fix.sh "$BUILD_DIR" "$ARCH"
COCOA_FIX_DIR="$BUILD_DIR/macos-cocoa-tray-fix"
cp "$COCOA_FIX_DIR/platforms/libqcocoa.dylib" \
    "$APP_PATH/Contents/PlugIns/platforms/libqcocoa.dylib"

# Ship the precise source provenance, applied patch, and licensing information
# alongside the modified LGPL/BSD platform plugin.
SOURCE_OFFER_PATH="$APP_PATH/Contents/Resources/qt-cocoa-tray-fix"
mkdir -p "$SOURCE_OFFER_PATH"
cp docs/macos-cocoa-tray-fix.md "$SOURCE_OFFER_PATH/README.md"
cp "$COCOA_FIX_DIR/qtbase-source-manifest.txt" "$SOURCE_OFFER_PATH/"
cp "$COCOA_FIX_DIR/macos27-tray.patch" "$SOURCE_OFFER_PATH/"
cp -R "$COCOA_FIX_DIR/licenses" "$SOURCE_OFFER_PATH/"

# All deployment edits precede signing. Preserve entitlements, but rebuild code
# requirements/seals after thinning and replacing nested Mach-O binaries.
codesign --force --sign - "$APP_PATH/Contents/PlugIns/platforms/libqcocoa.dylib"
codesign --force --deep --sign - --preserve-metadata=entitlements "$APP_PATH"
codesign --verify --deep --strict --verbose=2 "$APP_PATH"

# Load the actual packaged plugin and frameworks, rather than accidentally
# passing against the SDK copy. The probe performs no VPN or website requests.
APP_ABSOLUTE_PATH="$(cd "$APP_PATH" && pwd)"
TRAY_TEST_PLUGIN_ROOT="$APP_ABSOLUTE_PATH/Contents/PlugIns" \
TRAY_TEST_FRAMEWORK_ROOT="$APP_ABSOLUTE_PATH/Contents/Frameworks" \
DYLD_FRAMEWORK_PATH="$APP_ABSOLUTE_PATH/Contents/Frameworks" \
    "$COCOA_FIX_DIR/macos_tray_event_test"
