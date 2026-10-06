#!/bin/bash
# Usage: ./scripts/macos-sign-app.sh "<app_path>"

set -euo pipefail

APP_PATH="${1:-}"
if [[ -z "${CODESIGN_IDENTITY:-}" ]]; then
  echo "CODESIGN_IDENTITY is required"
  exit 1
fi

if [[ -z "$APP_PATH" || ! -d "$APP_PATH" ]]; then
  echo "App not found: $APP_PATH"
  exit 1
fi

QTWEBENGINE_PATH="$APP_PATH/Contents/Frameworks/QtWebEngineCore.framework/Helpers/QtWebEngineProcess.app"
QTWEBENGINE_ENTITLEMENTS="$QTWEBENGINE_PATH/Contents/Resources/QtWebEngineProcess.entitlements"
if [[ ! -d "$QTWEBENGINE_PATH" || ! -f "$QTWEBENGINE_ENTITLEMENTS" ]]; then
  echo "Qt WebEngine helper or entitlements not found: $QTWEBENGINE_PATH" >&2
  exit 1
fi

# Sign the helper before its containing frameworks and application. Preserve
# its JIT entitlements while deep-signing the final bundle, including Cocoa.
# Requirements must be regenerated when replacing an ad-hoc Qt signature.
codesign --verbose --force --options runtime --sign "$CODESIGN_IDENTITY" \
  --entitlements "$QTWEBENGINE_ENTITLEMENTS" "$QTWEBENGINE_PATH"
codesign --verbose --force --options runtime --deep --sign "$CODESIGN_IDENTITY" \
  --preserve-metadata=entitlements "$APP_PATH"
codesign --verify --deep --strict --verbose=2 "$APP_PATH"
