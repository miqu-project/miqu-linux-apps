#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "==> Configuring miquinfo ($BUILD_TYPE)..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="/usr" "$@"

echo "==> Building miquinfo..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Build finished successfully! Binary is at $BUILD_DIR/miquinfo"

# If invoked with sudo/root, automatically install to system
if [ "${EUID}" -eq 0 ]; then
    echo "==> Installing miquinfo to system (/usr/bin)..."
    cmake --install "$BUILD_DIR"

    # Install miquland icon to system icon theme
    ICON_DIR="/usr/share/icons/hicolor"
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    install -Dm644 "$SCRIPT_DIR/assets/miquland.png"     "$ICON_DIR/256x256/apps/miquland.png"
    install -Dm644 "$SCRIPT_DIR/assets/miquland-128.png"  "$ICON_DIR/128x128/apps/miquland.png"
    install -Dm644 "$SCRIPT_DIR/assets/miquland-64.png"   "$ICON_DIR/64x64/apps/miquland.png"
    install -Dm644 "$SCRIPT_DIR/assets/miquland-48.png"   "$ICON_DIR/48x48/apps/miquland.png"
    gtk-update-icon-cache -f -t "$ICON_DIR" 2>/dev/null || true

    echo "==> miquinfo successfully installed to /usr/bin/miquinfo"
else
    echo "==> To install system-wide, run: sudo ./make.sh"
fi
