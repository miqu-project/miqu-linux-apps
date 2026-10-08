#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "==> Configuring miqudm ($BUILD_TYPE)..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="/usr" -DCMAKE_INSTALL_SYSCONFDIR="/etc" "$@"

echo "==> Building miqudm..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Build finished successfully! Binaries are at $BUILD_DIR/miqudm and $BUILD_DIR/miqudm-greeter"

# If invoked with sudo/root, automatically install to system
if [ "${EUID}" -eq 0 ]; then
    echo "==> Installing miqudm to system (/usr/bin, /etc)..."
    cmake --install "$BUILD_DIR"
    echo "==> miqudm successfully installed."
else
    echo "==> To install system-wide, run: sudo ./make.sh"
fi
