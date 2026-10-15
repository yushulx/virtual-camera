#!/usr/bin/env bash
#
# make_deb.sh [version] - builds the Debian/Ubuntu package containing the
# virtual camera controller and everything needed to expose the v4l2loopback
# device as "Virtual Camera":
#
#   dist/virtual-camera_<version>_<arch>.deb
#
# Install it with:  sudo apt install ./dist/virtual-camera_<version>_<arch>.deb
#
# Requires: dpkg-deb (dpkg-dev), cmake, a C++17 compiler, Qt 6 development
# packages (qt6-base-dev, qt6-multimedia-dev). The runtime dependencies of
# the binary are computed from its NEEDED entries, so the control file is
# always right for the distribution it was built on.
#
set -euo pipefail

VERSION="${1:-1.0.0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LINUX_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$LINUX_DIR/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/linux"
DIST_DIR="$LINUX_DIR/dist"

PACKAGE="virtual-camera"

if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "error: dpkg-deb is required (install it with: sudo apt install dpkg-dev)" >&2
    exit 1
fi

ARCH="$(dpkg --print-architecture 2>/dev/null || dpkg-architecture -qDEB_HOST_ARCH 2>/dev/null || echo amd64)"

# --- 1. Build -----------------------------------------------------------------
if [ ! -x "$BUILD_DIR/app/virtual-camera" ] && [ ! -x "$BUILD_DIR/app/VirtualCamera" ]; then
    echo "==> Building the project (cmake)"
    cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
        -DVCAM_BUILD_TESTS=ON
    cmake --build "$BUILD_DIR" --parallel
fi

APP_BIN="$BUILD_DIR/app/virtual-camera"
if [ ! -x "$APP_BIN" ]; then
    # CMake lowercases the install name on some generators.
    APP_BIN="$BUILD_DIR/app/VirtualCamera"
fi
if [ ! -x "$APP_BIN" ]; then
    echo "error: virtual-camera binary not found in $BUILD_DIR/app" >&2
    exit 1
fi

# --- 2. Stage the payload --------------------------------------------------------
PKG_ROOT="$(mktemp -d)"
trap 'rm -rf "$PKG_ROOT"' EXIT

mkdir -p "$PKG_ROOT/usr/bin" \
         "$PKG_ROOT/usr/share/applications" \
         "$PKG_ROOT/usr/share/icons/hicolor/scalable/apps" \
         "$PKG_ROOT/usr/share/doc/$PACKAGE"

# cmake --install places the binary where the install() rule says; make
# sure it landed with the expected (lowercase) name.
cmake --install "$BUILD_DIR" --prefix /usr --strip >/dev/null 2>&1 || true
if [ -f "$PKG_ROOT/usr/bin/VirtualCamera" ] && [ ! -f "$PKG_ROOT/usr/bin/virtual-camera" ]; then
    mv "$PKG_ROOT/usr/bin/VirtualCamera" "$PKG_ROOT/usr/bin/virtual-camera"
fi
if [ ! -x "$PKG_ROOT/usr/bin/virtual-camera" ]; then
    install -m 755 "$APP_BIN" "$PKG_ROOT/usr/bin/virtual-camera"
fi

install -m 644 "$LINUX_DIR/packaging/virtual-camera.desktop" \
    "$PKG_ROOT/usr/share/applications/"
install -m 644 "$LINUX_DIR/packaging/virtual-camera.svg" \
    "$PKG_ROOT/usr/share/icons/hicolor/scalable/apps/"
install -m 644 "$REPO_ROOT/README.md" "$PKG_ROOT/usr/share/doc/$PACKAGE/README.md"
install -m 644 "$REPO_ROOT/LICENSE" "$PKG_ROOT/usr/share/doc/$PACKAGE/copyright"

# --- 3. Runtime dependencies -----------------------------------------------------
# Compute from the binary's NEEDED libraries so the list matches the build
# distribution; add the GStreamer plugins Qt Multimedia dlopens plus the
# kernel module.
DEPS="$(objdump -p "$APP_BIN" 2>/dev/null | awk '/NEEDED/ {print $2}' \
    | sort -u \
    | while read -r soname; do
        dpkg -S "*/$soname" 2>/dev/null | head -1 | cut -d: -f1
    done | sort -u | grep -v '^$' | tr '\n' ',' | sed 's/,$//')"

echo "==> detected library dependencies: $DEPS"

mkdir -p "$PKG_ROOT/DEBIAN"

cat > "$PKG_ROOT/DEBIAN/control" <<EOF
Package: $PACKAGE
Version: $VERSION
Architecture: $ARCH
Maintainer: Dynamsoft <support@dynamsoft.com>
Section: video
Priority: optional
Depends: v4l2loopback-dkms | v4l2loopback-modules, ${DEPS}, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-libav
Installed-Size: $(du -sk "$PKG_ROOT" | cut -f1)
Description: Virtual camera that plays a video file or image as a webcam
 A virtual camera for testing camera workflows, demos and CI without a
 webcam: it loops a local video file or a static image into a
 v4l2loopback device that browsers (getUserMedia), conferencing apps
 and every V4L2 application see as "Virtual Camera".
 .
 Start the "Virtual Camera" application, pick a source, press Start.
 The resolution (up to 1920x1080) and frame rate are configurable and
 the configuration persists between runs.
EOF

install -m 755 "$LINUX_DIR/packaging/postinst" "$PKG_ROOT/DEBIAN/postinst"
install -m 755 "$LINUX_DIR/packaging/prerm" "$PKG_ROOT/DEBIAN/prerm"

# --- 4. Build the package ---------------------------------------------------------
mkdir -p "$DIST_DIR"
DEB="$DIST_DIR/${PACKAGE}_${VERSION}_${ARCH}.deb"
rm -f "$DEB"

echo "==> Building $DEB"
dpkg-deb --root-owner-group --build "$PKG_ROOT" "$DEB"

echo ""
echo "==> Done: $DEB"
echo "    Install it with: sudo apt install $DEB"
echo "    Then check:      v4l2-ctl --list-devices   (shows 'Virtual Camera')"
dpkg-deb --info "$DEB" | sed -n '1,12p'
