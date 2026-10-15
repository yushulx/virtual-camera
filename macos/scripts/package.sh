#!/bin/bash
#
# package.sh - packages VirtualCamera.app (with the embedded CMIO camera
# extension) into a single .pkg installer.
#
# Usage:
#   ./macos/scripts/package.sh [version] [--build] [--universal]
#
#   version    e.g. 1.0.0 (default) -> dist/VirtualCamera-1.0.0.pkg
#   --build    rebuild first (build-macos.sh) instead of reusing build/
#   --universal build the Qt app as a universal binary (needs universal Qt)
#
# Signing / notarization (all optional - without them the package is
# unsigned/ad-hoc and macOS Gatekeeper blocks a browser-downloaded copy;
# users then install via the one-line installer or `sudo installer`):
#
#   SIGNING_IDENTITY    codesign identity for the app + extension, e.g.
#                       "Developer ID Application: YOUR NAME (TEAMID)"
#   INSTALLER_IDENTITY  pkgbuild signing identity, e.g. "Developer ID
#                       Installer: YOUR NAME (TEAMID)"
#   KEYCHAIN_PROFILE    `notarytool` keychain profile; when set the package
#                       is notarized and stapled
#   APPLE_ID / APPLE_PASSWORD / APPLE_TEAM_ID
#                       alternative notarytool auth (app-specific password)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MACOS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$MACOS_DIR/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/macos"
DIST_DIR="$MACOS_DIR/dist"
APP_NAME="VirtualCamera.app"
VERSION="1.0.0"
DO_BUILD=0
EXTRA_BUILD_ARGS=()

for arg in "$@"; do
    case "$arg" in
        --build)    DO_BUILD=1 ;;
        --universal) EXTRA_BUILD_ARGS+=(--universal) ;;
        -h|--help)
            echo "usage: $0 [version] [--build] [--universal]" >&2
            exit 0 ;;
        *)
            if [[ "$arg" =~ ^[0-9]+\.[0-9]+(\.[0-9]+)?$ ]]; then
                VERSION="$arg"
            else
                echo "unknown option: $arg" >&2
                exit 1
            fi ;;
    esac
done

if [[ "$DO_BUILD" -eq 1 ]]; then
    # macOS ships bash 3.2: "${arr[@]}" on an empty array trips `set -u`.
    if [[ "${#EXTRA_BUILD_ARGS[@]}" -gt 0 ]]; then
        "$SCRIPT_DIR/build-macos.sh" "${EXTRA_BUILD_ARGS[@]}"
    else
        "$SCRIPT_DIR/build-macos.sh"
    fi
fi

APP="$BUILD_DIR/app/$APP_NAME"
if [[ ! -d "$APP" ]]; then
    echo "error: no built app at $APP" >&2
    echo "       Run ./macos/scripts/build-macos.sh first (or pass --build)." >&2
    exit 1
fi

# --- stage the payload -------------------------------------------------------------
PKG_ROOT="$BUILD_DIR/pkgroot"
rm -rf "$PKG_ROOT"
mkdir -p "$PKG_ROOT/Applications"
cp -R "$APP" "$PKG_ROOT/Applications/$APP_NAME"

# --- build the installer package ---------------------------------------------------
mkdir -p "$DIST_DIR"
PKG_PATH="$DIST_DIR/VirtualCamera-$VERSION.pkg"

INSTALLER_IDENTITY="${INSTALLER_IDENTITY:-}"
pkgbuild_args=(
    --root "$PKG_ROOT"
    --identifier "com.dynamsoft.virtualcamera"
    --version "$VERSION"
    --install-location "/"
)

if [[ -n "$INSTALLER_IDENTITY" ]]; then
    echo "==> signing the package with: $INSTALLER_IDENTITY"
    pkgbuild_args+=(--sign "$INSTALLER_IDENTITY")
else
    echo "==> No INSTALLER_IDENTITY set - the package will be UNSIGNED"
    echo "    (install with: sudo installer -pkg <pkg> -target /"
    echo "     or use the one-line installer: macos/scripts/install.sh)"
fi

echo "==> building $PKG_PATH"
pkgbuild "${pkgbuild_args[@]}" "$PKG_PATH"

# --- notarize and staple (optional) -------------------------------------------------
KEYCHAIN_PROFILE="${KEYCHAIN_PROFILE:-}"
if [[ -n "$INSTALLER_IDENTITY" && -n "$KEYCHAIN_PROFILE" ]]; then
    echo "==> submitting the package for notarization (profile: $KEYCHAIN_PROFILE)"
    xcrun notarytool submit "$PKG_PATH" --keychain-profile "$KEYCHAIN_PROFILE" --wait
    echo "==> stapling the notarization ticket"
    xcrun stapler staple "$PKG_PATH"
elif [[ -n "$INSTALLER_IDENTITY" && -n "${APPLE_ID:-}" && -n "${APPLE_PASSWORD:-}" && -n "${APPLE_TEAM_ID:-}" ]]; then
    echo "==> submitting the package for notarization (Apple ID: $APPLE_ID)"
    xcrun notarytool submit "$PKG_PATH" \
        --apple-id "$APPLE_ID" --password "$APPLE_PASSWORD" \
        --team-id "$APPLE_TEAM_ID" --wait
    echo "==> stapling the notarization ticket"
    xcrun stapler staple "$PKG_PATH"
else
    echo "==> skipping notarization (set KEYCHAIN_PROFILE, or APPLE_ID +"
    echo "    APPLE_PASSWORD + APPLE_TEAM_ID, to enable it)"
fi

echo ""
echo "==> Done: $PKG_PATH"
echo "    Install on any Mac with:"
echo "    sudo installer -pkg $PKG_PATH -target /"
echo "    then launch VirtualCamera.app once and approve the system extension."
