#!/bin/bash
#
# install.sh - one-line installer for the macOS package.
#
#   curl -fsSL https://raw.githubusercontent.com/yushulx/virtual-camera/main/macos/scripts/install.sh | bash
#
# Why: release packages are unsigned (or ad-hoc signed), so Gatekeeper
# refuses to open a browser-downloaded copy ("Apple could not verify ...").
# curl does not set the quarantine attribute and `installer` skips the
# Gatekeeper assessment, so installing from the terminal just works.
#
# For ad-hoc/unsigned system extensions, developer mode must be enabled
# once beforehand (asked for interactively below):
#
#   sudo systemextensionsctl developer on
#
# The camera extension itself is activated on the first launch of
# VirtualCamera.app (macOS then shows an approval dialog in System
# Settings > Login Items & Extensions).

set -euo pipefail

REPO="${VCAM_REPO:-yushulx/virtual-camera}"
PKG_PREFIX="VirtualCamera-"

echo "==> fetching the latest release of $REPO"
ASSET_URL="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" \
    | grep -o "\"browser_download_url\": *\"[^\"]*\\\.pkg\"" \
    | head -1 | sed 's/.*"\(https[^\"]*\)"/\1/')"

if [[ -z "$ASSET_URL" ]]; then
    echo "error: no .pkg asset found in the latest release of $REPO" >&2
    echo "       build one yourself: ./macos/scripts/package.sh 1.0.0 --build" >&2
    exit 1
fi

PKG="$(mktemp -t virtual-camera)/$(basename "$ASSET_URL")"
mkdir -p "$(dirname "$PKG")"
echo "==> downloading $ASSET_URL"
curl -fSL "$ASSET_URL" -o "$PKG"

echo "==> enabling system extension developer mode (required for unsigned"
echo "    packages; skip with Ctrl-C if you already enabled it)"
if ! systemextensionsctl developer 2>/dev/null | grep -q "enabled"; then
    sudo systemextensionsctl developer on || \
        echo "note: could not enable developer mode automatically" >&2
fi

echo "==> installing $(basename "$PKG")"
sudo installer -pkg "$PKG" -target /

echo ""
echo "==> Installed. Next steps:"
echo "    1. Open VirtualCamera.app (Applications folder)."
echo "    2. Click 'Install camera extension...' and approve it in"
echo "       System Settings > Login Items & Extensions."
echo "    3. Pick a source file, press Start, then select 'Virtual Camera'"
echo "       in your browser (docs/web-camera-test.html)."
