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

# Qt for find_package(): QTDIR / QT_ROOT_DIR (set by install-qt-action) when
# the Qt bin dir is not already on the default search path.
QT_PREFIX="${QTDIR:-${QT_ROOT_DIR:-}}"
ARCH_FLAGS=()
CMAKE_PREFIX_ARGS=()
if [[ -n "$QT_PREFIX" ]]; then
    echo "==> Qt root: $QT_PREFIX"
    CMAKE_PREFIX_ARGS=(-DCMAKE_PREFIX_PATH="$QT_PREFIX")
fi
if [[ "$UNIVERSAL" -eq 1 ]]; then
    # Requires a universal Qt build; on failure fall back to the runner's
    # native architecture (the extension itself stays universal).
    ARCH_FLAGS=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64")
fi

echo "==> cmake configure"
# macOS bash 3.2: guard empty-array expansion under `set -u`.
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    ${CMAKE_PREFIX_ARGS[@]+"${CMAKE_PREFIX_ARGS[@]}"} \
    ${ARCH_FLAGS[@]+"${ARCH_FLAGS[@]}"}

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

# macdeployqt misses frameworks in some Qt builds (seen with Homebrew Qt,
# which enables D-Bus: QtGui links QtDBus.framework but it is not deployed).
# Scan every Mach-O inside the app for Qt framework references and copy any
# missing one from the Qt installation.
QT_LIB_DIR="$(cd "$(dirname "$DEPLOYQT")/../lib" && pwd)"
for ref in $(find "$APP/Contents" -type f -print0 |
                 xargs -0 file 2>/dev/null | grep Mach-O | cut -d: -f1 |
                 xargs otool -L 2>/dev/null |
                 grep -oE '@rpath/Qt[A-Za-z]+\.framework' | sort -u); do
    fw="${ref#@rpath/}"
    if [[ ! -e "$APP/Contents/Frameworks/$fw" && -d "$QT_LIB_DIR/$fw" ]]; then
        echo "==> macdeployqt missed $fw - copying from Qt"
        cp -R "$QT_LIB_DIR/$fw" "$APP/Contents/Frameworks/"
    fi
done

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
#
# macdeployqt copies/patches Qt frameworks and plugin dylibs AFTER their
# signatures were created, which leaves them broken. Signing only the outer
# app bundle does NOT repair a broken nested signature - macOS then refuses
# to spawn the app (RBSRequestErrorDomain Code=5 / NSPOSIXErrorDomain 163).
# So re-seal every nested component first, then the app itself.

sign_inner() {
    local identity="$1"; shift
    # bare dylibs: Qt plugins under PlugIns/ AND helper libs deployed at the
    # top of Frameworks/ (e.g. liblcms2.2.dylib, libtiff.dylib, ...)
    find "$APP/Contents" -name '*.dylib' -type f -print0 |
        xargs -0 -I@ codesign --force "$@" --sign "$identity" @
    # Qt frameworks (top level; they contain no nested bundles)
    find "$APP/Contents/Frameworks" -maxdepth 1 -name '*.framework' -print0 |
        xargs -0 -I@ codesign --force "$@" --sign "$identity" @
}

if [[ "$IDENTITY" != "-" ]]; then
    echo "==> codesigning VirtualCamera.app with: $IDENTITY"
    sign_inner "$IDENTITY" --options runtime --timestamp
    codesign --force --options runtime --timestamp \
        --entitlements "$MACOS_DIR/app/VirtualCamera.entitlements" \
        --sign "$IDENTITY" "$APP"
else
    # NOTE: do NOT apply VirtualCamera.entitlements on the ad-hoc path.
    # com.apple.developer.system-extension.install is a RESTRICTED
    # entitlement that requires an Apple-provisioned profile; an ad-hoc
    # signature carrying it is rejected by AMFI at spawn ("Launch failed",
    # POSIX error 163, no crash report). With developer mode enabled the
    # entitlement is not needed to activate the extension, so ad-hoc
    # builds are signed without any entitlements.
    echo "==> codesigning VirtualCamera.app ad-hoc (developer mode required:"
    echo "    sudo systemextensionsctl developer on)"
    sign_inner "-"
    codesign --force --sign - "$APP"
fi

# Fail the build instead of shipping a bundle macOS refuses to launch.
echo "==> verifying the signature"
codesign --verify --deep --strict "$APP"

echo "==> done: $APP"
echo "    Launch it once and use 'Install camera extension...' to activate the"
echo "    system extension, then approve it in System Settings."
