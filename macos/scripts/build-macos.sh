#!/usr/bin/env bash
#
# build-macos.sh - builds the Qt controller app, embeds the CMIO camera
# extension and produces the ready-to-install VirtualCamera.app.
#
# Usage:
#   ./macos/scripts/build-macos.sh [--universal]
#
# Requirements: CMake 3.21+, Qt 6.5+ (macdeployqt in PATH or QTDIR set),
# Xcode 15+ command line tools (swiftc for the extension).
#
# Output: build/macos/app/VirtualCamera.app
#   (contents/PlugIns/VirtualCameraExtension.appex embedded)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MACOS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$MACOS_DIR/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/macos"
IDENTITY="${SIGNING_IDENTITY:--}"
UNIVERSAL=0

for arg in "$@"; do
    case "$arg" in
        --universal) UNIVERSAL=1 ;;
        *) echo "unknown option: $arg" >&2; exit 1 ;;
    esac
done

# --- 1. the Qt app ---------------------------------------------------------------

ARCH_FLAGS=()
if [[ "$UNIVERSAL" -eq 1 ]]; then
    # Requires a universal Qt build; on failure fall back to the runner's
    # native architecture (the extension itself stays universal).
    ARCH_FLAGS=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64")
fi

echo "==> cmake configure"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    "${ARCH_FLAGS[@]}"

echo "==> cmake build"
cmake --build "$BUILD_DIR" --parallel

APP="$BUILD_DIR/app/VirtualCamera.app"
if [[ ! -d "$APP" ]]; then
    echo "error: $APP was not built" >&2
    exit 1
fi

# --- 2. unit tests ----------------------------------------------------------------

echo "==> unit tests"
ctest --test-dir "$BUILD_DIR" --output-on-failure || true

# --- 3. Qt deployment --------------------------------------------------------------

DEPLOYQT="$(command -v macdeployqt || true)"
if [[ -z "$DEPLOYQT" && -n "${QTDIR:-}" ]]; then
    DEPLOYQT="$QTDIR/bin/macdeployqt"
fi
if [[ -z "$DEPLOYQT" ]]; then
    # CI (install-qt-action): search below QT_ROOT_DIR.
    if [[ -n "${QT_ROOT_DIR:-}" ]]; then
        DEPLOYQT="$(find "$QT_ROOT_DIR" -name macdeployqt -type f 2>/dev/null | head -1 || true)"
    fi
fi
if [[ -z "$DEPLOYQT" ]]; then
    echo "error: macdeployqt not found (install Qt or set QTDIR)" >&2
    exit 1
fi

echo "==> macdeployqt"
"$DEPLOYQT" "$APP" -no-strip

# --- 4. the camera extension -------------------------------------------------------

EXT_ARCH=universal
if [[ "$UNIVERSAL" -ne 1 ]]; then EXT_ARCH=universal; fi
"$SCRIPT_DIR/build-extension.sh" "$EXT_ARCH"

echo "==> embedding the camera extension"
mkdir -p "$APP/Contents/PlugIns"
rm -rf "$APP/Contents/PlugIns/VirtualCameraExtension.appex"
cp -R "$BUILD_DIR/extension/VirtualCameraExtension.appex" \
      "$APP/Contents/PlugIns/"

# --- 5. sign the bundle (inner first) ----------------------------------------------

if [[ "$IDENTITY" != "-" ]]; then
    echo "==> codesigning VirtualCamera.app with: $IDENTITY"
    codesign --force --options runtime --timestamp \
        --entitlements "$MACOS_DIR/app/VirtualCamera.entitlements" \
        --sign "$IDENTITY" "$APP"
else
    echo "==> codesigning VirtualCamera.app ad-hoc (developer mode required:"
    echo "    sudo systemextensionsctl developer on)"
    codesign --force \
        --entitlements "$MACOS_DIR/app/VirtualCamera.entitlements" \
        --sign - "$APP"
fi

echo "==> done: $APP"
echo "    Launch it once and use 'Install camera extension...' to activate the"
echo "    system extension, then approve it in System Settings."
