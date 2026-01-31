#!/bin/bash
# macOS deployment script
# Usage: ./scripts/deploy-macos.sh "EZ4Connect" "build" "amd64"

TARGET_NAME="${1:-EZ4Connect}"
BUILD_DIR="${2:-build}"
ARCH="${3:-amd64}"

# Copy app bundle
cp -R "$BUILD_DIR/$TARGET_NAME.app" .

# Download and extract zju-connect
curl -LO "https://github.com/Mythologyli/zju-connect/releases/download/nightly/zju-connect-darwin-$ARCH.zip"
unzip -o "zju-connect-darwin-$ARCH.zip"
rm "zju-connect-darwin-$ARCH.zip"

# Copy zju-connect into app bundle
cp zju-connect "$TARGET_NAME.app/Contents/MacOS/"

# Run macdeployqt
macdeployqt "$TARGET_NAME.app"

# macdeployqt has a bug that it doesn't copy the Qt translations to the bundle
# so we need to copy them manually
QT_TRANSLATIONS=$(qmake -query QT_INSTALL_TRANSLATIONS)
if [ -d "$QT_TRANSLATIONS" ]; then
    cp -R "$QT_TRANSLATIONS"/qtbase_*.qm "$TARGET_NAME.app/Contents/"
fi
