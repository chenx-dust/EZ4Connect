#!/bin/bash
# Build a source backport of QTBUG-147449 against the application's exact SDK.
# Usage: build-macos-cocoa-tray-fix.sh <app-build-dir> <arm64|x86_64> [options]
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: build-macos-cocoa-tray-fix.sh <app-build-dir> <arm64|x86_64>
       [--qt-prefix PATH] [--source-archive PATH] [--cmake PATH] [--parallel N]
Outputs: <app-build-dir>/macos-cocoa-tray-fix/platforms/libqcocoa.dylib
         <app-build-dir>/macos-cocoa-tray-fix/macos_tray_event_test
The verified Qt source and licenses stay alongside these outputs. No SDK or
installed application is modified. CXXFLAGS/OBJCXXFLAGS are honored by CMake.
USAGE
}
fail() { printf 'Cocoa tray backport: %s\n' "$*" >&2; exit 1; }

if [[ "${1:-}" == '--help' ]]; then usage; exit 0; fi
[[ $# -ge 2 ]] || { usage >&2; exit 2; }
[[ "$(uname -s)" == Darwin ]] || fail 'macOS is required'
cocoa_app_build_dir=$1
cocoa_arch=$2
shift 2
case "$cocoa_arch" in arm64|x86_64) ;; *) fail 'architecture must be arm64 or x86_64' ;; esac
cocoa_qt_prefix=''
cocoa_source_archive=''
cocoa_cmake='cmake'
cocoa_parallel=2
while [[ $# -gt 0 ]]; do
    [[ $# -ge 2 ]] || fail "missing value for $1"
    case "$1" in
        --qt-prefix) cocoa_qt_prefix=$2 ;;
        --source-archive) cocoa_source_archive=$2 ;;
        --cmake) cocoa_cmake=$2 ;;
        --parallel) cocoa_parallel=$2 ;;
        *) fail "unknown option $1" ;;
    esac
    shift 2
done
[[ "$cocoa_parallel" =~ ^[1-9][0-9]*$ ]] || fail 'parallelism must be a positive integer'
command -v "$cocoa_cmake" >/dev/null || fail 'CMake is required'
cocoa_script_dir=$(cd "$(dirname "$0")" && pwd -P)
cocoa_repo_dir=$(cd "$cocoa_script_dir/.." && pwd -P)
mkdir -p "$cocoa_app_build_dir"
cocoa_app_build_dir=$(cd "$cocoa_app_build_dir" && pwd -P)
cocoa_fix_dir="$cocoa_app_build_dir/macos-cocoa-tray-fix"
mkdir -p "$cocoa_fix_dir"

# Prefer the SDK already selected for the application over an unrelated qmake
# on PATH. If explicitly supplied, it must agree with the application's cache.
cocoa_cached_qt_dir=''
if [[ -f "$cocoa_app_build_dir/CMakeCache.txt" ]]; then
    cocoa_cached_qt_dir=$(sed -n 's/^Qt6_DIR:PATH=//p' "$cocoa_app_build_dir/CMakeCache.txt")
fi
if [[ -n "$cocoa_cached_qt_dir" ]]; then
    [[ -d "$cocoa_cached_qt_dir" ]] || fail 'application cache points to a missing Qt SDK'
    cocoa_cached_qt_prefix=$(cd "$cocoa_cached_qt_dir/../../.." && pwd -P)
    if [[ -n "$cocoa_qt_prefix" ]]; then
        [[ -d "$cocoa_qt_prefix" ]] || fail 'requested Qt SDK is missing'
        [[ "$(cd "$cocoa_qt_prefix" && pwd -P)" == "$cocoa_cached_qt_prefix" ]] \
            || fail 'requested SDK differs from the application build cache'
    fi
    cocoa_qt_prefix=$cocoa_cached_qt_prefix
elif [[ -z "$cocoa_qt_prefix" ]]; then
    command -v qmake >/dev/null || fail 'provide --qt-prefix or put the matching qmake on PATH'
    cocoa_qt_prefix=$(qmake -query QT_INSTALL_PREFIX)
fi
[[ -d "$cocoa_qt_prefix" ]] || fail 'Qt SDK prefix is missing'
cocoa_qt_prefix=$(cd "$cocoa_qt_prefix" && pwd -P)
[[ -x "$cocoa_qt_prefix/bin/qmake" ]] || fail 'matching SDK qmake is missing'
[[ "$("$cocoa_qt_prefix/bin/qmake" -query QT_VERSION)" == '6.10.3' ]] \
    || fail 'only the verified Qt 6.10.3 SDK is supported'
for cocoa_module in Core Gui; do
    cocoa_framework="$cocoa_qt_prefix/lib/Qt$cocoa_module.framework/Versions/A/Qt$cocoa_module"
    [[ -f "$cocoa_framework" ]] || fail "Qt$cocoa_module framework is missing"
    lipo "$cocoa_framework" -verify_arch "$cocoa_arch" \
        || fail "Qt$cocoa_module does not contain the requested architecture"
done

cocoa_source_url='https://codeload.github.com/qt/qtbase/tar.gz/refs/tags/v6.10.3'
cocoa_source_sha='ab962447c44a03e02d8164bb9c2db80e2822121cead8bc18ffb63f8c30a413a5'
cocoa_download_tmp=''
trap 'if [[ -n "$cocoa_download_tmp" ]]; then rm -f "$cocoa_download_tmp"; fi' EXIT
if [[ -z "$cocoa_source_archive" ]]; then
    cocoa_source_archive="$cocoa_fix_dir/qtbase-v6.10.3.tar.gz"
    if [[ ! -f "$cocoa_source_archive" ]]; then
        cocoa_download_tmp=$(mktemp "$cocoa_fix_dir/qtbase-download.XXXXXX")
        curl --fail --location --retry 3 --output "$cocoa_download_tmp" "$cocoa_source_url"
        [[ "$(shasum -a 256 "$cocoa_download_tmp" | awk '{print $1}')" == "$cocoa_source_sha" ]] \
            || fail 'downloaded official source does not match the pinned SHA256'
        mv "$cocoa_download_tmp" "$cocoa_source_archive"
        cocoa_download_tmp=''
    fi
fi
[[ -f "$cocoa_source_archive" ]] || fail 'supplied source archive is missing'
[[ "$(shasum -a 256 "$cocoa_source_archive" | awk '{print $1}')" == "$cocoa_source_sha" ]] \
    || fail 'source archive does not match the pinned SHA256; no patch will be built'

cocoa_source_dir="$cocoa_fix_dir/source"
mkdir -p "$cocoa_source_dir" "$cocoa_fix_dir/licenses"
# Re-extract the verified files on each invocation, so a previous patched or
# interrupted build cannot silently change the next build's source input.
tar -xzf "$cocoa_source_archive" -C "$cocoa_source_dir" --strip-components=1 \
    qtbase-6.10.3/src/plugins/platforms/cocoa \
    qtbase-6.10.3/LICENSES/LGPL-3.0-only.txt \
    qtbase-6.10.3/LICENSES/GPL-2.0-only.txt \
    qtbase-6.10.3/LICENSES/GPL-3.0-only.txt \
    qtbase-6.10.3/LICENSES/BSD-3-Clause.txt
patch --batch --forward -p1 -d "$cocoa_source_dir" \
    < "$cocoa_repo_dir/cmake/macos-cocoa-tray-fix/macos27-tray.patch"
cp "$cocoa_repo_dir/cmake/macos-cocoa-tray-fix/macos27-tray.patch" "$cocoa_fix_dir/"
cp "$cocoa_source_dir/LICENSES/"*.txt "$cocoa_fix_dir/licenses/"
cp "$cocoa_source_dir/src/plugins/platforms/cocoa/LICENSE.COCOA.txt" "$cocoa_fix_dir/licenses/"

cocoa_configure_args=(
    -S "$cocoa_repo_dir/cmake/macos-cocoa-tray-fix"
    -B "$cocoa_fix_dir/build"
    -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_OSX_ARCHITECTURES=$cocoa_arch"
    "-DCMAKE_PREFIX_PATH=$cocoa_qt_prefix"
    "-DCOCOA_FIX_QT_PREFIX=$cocoa_qt_prefix"
    "-DCOCOA_FIX_SOURCE_DIR=$cocoa_source_dir"
    "-DCOCOA_FIX_OUTPUT_DIR=$cocoa_fix_dir"
)
"$cocoa_cmake" "${cocoa_configure_args[@]}"
"$cocoa_cmake" --build "$cocoa_fix_dir/build" --config Release --parallel "$cocoa_parallel"
cocoa_plugin="$cocoa_fix_dir/platforms/libqcocoa.dylib"
[[ -f "$cocoa_plugin" && -x "$cocoa_fix_dir/macos_tray_event_test" ]] \
    || fail 'expected plugin or native regression probe was not produced'
lipo "$cocoa_plugin" -verify_arch "$cocoa_arch"
"$cocoa_cmake" "-DCOCOA_FIX_PLUGIN=$cocoa_plugin" \
    "-DCOCOA_FIX_MANIFEST=$cocoa_fix_dir/qtbase-source-manifest.txt" \
    -P "$cocoa_repo_dir/cmake/macos-cocoa-tray-fix/VerifyPlugin.cmake"
printf 'Cocoa tray backport built: %s\nNative event probe: %s\n' \
    "$cocoa_plugin" "$cocoa_fix_dir/macos_tray_event_test"
