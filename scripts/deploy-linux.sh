#!/bin/bash
# Linux deployment script
# Usage: ./scripts/deploy-linux.sh "EZ4Connect" "EZ4Connect" "build" "x86_64"

TARGET_NAME="${1:-EZ4Connect}"
DISPLAY_NAME="${2:-EZ4Connect}"
BUILD_DIR="${3:-build}"
ARCH="${4:-x86_64}"

# Determine AppImage tool architecture
if [ "$ARCH" = "x86_64" ]; then
    APPIMAGE_ARCH="x86_64"
else
    APPIMAGE_ARCH="aarch64"
fi

# Download linuxdeploy tools if not present
if [ ! -f linuxdeploy ]; then
    wget -O linuxdeploy "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$APPIMAGE_ARCH.AppImage"
fi

if [ ! -f linuxdeploy-plugin-qt ]; then
    wget -O linuxdeploy-plugin-qt "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$APPIMAGE_ARCH.AppImage"
fi

chmod a+x linuxdeploy linuxdeploy-plugin-qt

# Create AppDir structure
mkdir -p AppDir/usr/bin AppDir/usr/lib AppDir/usr/share/applications AppDir/usr/share/icons/hicolor/scalable/apps

# Copy executable
cp "$BUILD_DIR/$TARGET_NAME" AppDir/usr/bin/

# Download and extract zju-connect
ZIJU_ARCH="${ARCH}"
if [ "$ARCH" = "x86_64" ]; then
    ZIJU_ARCH="amd64"
fi

wget -O "zju-connect-linux-$ZIJU_ARCH.zip" "https://github.com/Mythologyli/zju-connect/releases/download/nightly/zju-connect-linux-$ZIJU_ARCH.zip"
unzip -o "zju-connect-linux-$ZIJU_ARCH.zip"
cp zju-connect AppDir/usr/bin/
rm "zju-connect-linux-$ZIJU_ARCH.zip"

# Copy icon
cp resource/icon.png "AppDir/usr/share/icons/hicolor/scalable/apps/$TARGET_NAME.png"

# Create desktop file
cat > "AppDir/usr/share/applications/$TARGET_NAME.desktop" <<EOF
[Desktop Entry]
Name=$DISPLAY_NAME
Exec=$TARGET_NAME
Icon=$TARGET_NAME
Type=Application
Categories=Network;Security;
EOF

# Build AppImage
./linuxdeploy --appdir AppDir --output appimage --plugin qt
