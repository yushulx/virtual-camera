#!/usr/bin/env bash
#
# build-extension.sh - compiles the CMIO camera extension (Swift), assembles
# the VirtualCameraExtension.appex bundle and (ad-hoc) codesigns it.
#
# Usage:
#   ./macos/scripts/build-extension.sh [arm64|x86_64|universal]
#
# Signing: ad-hoc ("-") by default. Pass SIGNING_IDENTITY to sign properly
# (Developer ID Application). For ad-hoc builds the target Mac must have
# developer mode enabled once:
#   sudo systemextensionsctl developer on
#
# Output: build/macos/extension/VirtualCameraExtension.appex

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MACOS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$MACOS_DIR/.." && pwd)"
EXT_DIR="$MACOS_DIR/extension"
BUILD_DIR="$REPO_ROOT/build/macos/extension"

ARCHS="${1:-universal}"
IDENTITY="${SIGNING_IDENTITY:--}"
MACOS_MIN="13.0"  # camera extensions require macOS 13 (Ventura) or newer

mkdir -p "$BUILD_DIR"

build_arch() {
    local arch="$1"
    local out="$BUILD_DIR/bin-$arch"
    mkdir -p "$out"
    echo "==> compiling the camera extension ($arch)"
    # NOTE: no -parse-as-library: main.swift carries top-level statements
    # (the program entry), exactly like Apple's camera extension template.
    swiftc -O \
        -target "$arch-apple-macos$MACOS_MIN" \
        -framework CoreMediaIO -framework CoreMedia -framework CoreVideo \
        -framework Foundation \
        "$EXT_DIR/Sources"/*.swift \
        -o "$out/VirtualCameraExtension"
}

BIN="$BUILD_DIR/bin-universal/VirtualCameraExtension"
if [[ "$ARCHS" == "universal" ]]; then
    build_arch arm64
    build_arch x86_64
    mkdir -p "$BUILD_DIR/bin-universal"
    lipo -create \
        "$BUILD_DIR/bin-arm64/VirtualCameraExtension" \
        "$BUILD_DIR/bin-x86_64/VirtualCameraExtension" \
        -output "$BIN"
else
    build_arch "$ARCHS"
    BIN="$BUILD_DIR/bin-$ARCHS/VirtualCameraExtension"
fi

# --- assemble the .appex bundle ---------------------------------------------
APPEX="$BUILD_DIR/VirtualCameraExtension.appex"
rm -rf "$APPEX"
mkdir -p "$APPEX/Contents/MacOS"

cp "$BIN" "$APPEX/Contents/MacOS/VirtualCameraExtension"
chmod 755 "$APPEX/Contents/MacOS/VirtualCameraExtension"
cp "$EXT_DIR/Info.plist" "$APPEX/Contents/Info.plist"

# --- codesign -----------------------------------------------------------------
if [[ "$IDENTITY" != "-" ]]; then
    echo "==> codesigning the extension with: $IDENTITY"
    codesign --force --options runtime --timestamp \
        --entitlements "$EXT_DIR/VirtualCameraExtension.entitlements" \
        --sign "$IDENTITY" "$APPEX"
else
    echo "==> codesigning the extension ad-hoc (developer mode required:"
    echo "    sudo systemextensionsctl developer on)"
    codesign --force \
        --entitlements "$EXT_DIR/VirtualCameraExtension.entitlements" \
        --sign - "$APPEX"
fi

echo "==> done: $APPEX"
