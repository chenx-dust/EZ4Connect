#!/bin/bash
# Usage: ./scripts/macos-sign-app.sh "<app_path>"

set -euo pipefail

APP_PATH="${1:-}"
FRAMEWORKS_PATH="$APP_PATH/Contents/Frameworks"

if [[ -z "${CODESIGN_IDENTITY:-}" ]]; then
  echo "CODESIGN_IDENTITY is required"
  exit 1
fi

if [[ -z "$APP_PATH" || ! -d "$APP_PATH" ]]; then
  echo "App not found: $APP_PATH"
  exit 1
fi

# Align with resource/macos-fix.sh order:
# sign framework versions first, then helper apps, then main app.
if [[ -d "$FRAMEWORKS_PATH" ]]; then
  for framework in "$FRAMEWORKS_PATH"/*.framework; do
    [[ -d "$framework" ]] || continue
    if [[ -d "$framework/Versions" ]]; then
      for version in "$framework/Versions"/*; do
        if [[ -d "$version" && ! -L "$version" ]]; then
          codesign --verbose --force --timestamp --options runtime --sign "$CODESIGN_IDENTITY" "$version"
        fi
      done
    else
      codesign --verbose --force --timestamp --options runtime --sign "$CODESIGN_IDENTITY" "$framework"
    fi
  done

  for helper_app in "$FRAMEWORKS_PATH"/*.app; do
    [[ -d "$helper_app" ]] || continue
    codesign --verbose --force --timestamp --options runtime --sign "$CODESIGN_IDENTITY" "$helper_app"
  done
fi

codesign --verbose --force --timestamp --options runtime --deep --sign "$CODESIGN_IDENTITY" "$APP_PATH"
codesign --verify --deep --strict --verbose=2 "$APP_PATH"
