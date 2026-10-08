#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "==> Configuring miquoverview ($BUILD_TYPE)..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="/usr" "$@"

echo "==> Building miquoverview..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Build finished successfully! Shared library is at $BUILD_DIR/libmiquoverview.so"

# If invoked with sudo/root, automatically install to system
if [ "${EUID}" -eq 0 ]; then
    echo "==> Installing miquoverview to system (/usr/lib/miquland/plugins/libmiquoverview.so)..."
    cmake --install "$BUILD_DIR"
    echo "==> miquoverview successfully installed to /usr/lib/miquland/plugins/libmiquoverview.so!"
else
    echo "==> Built locally in $BUILD_DIR. To install system-wide, run: sudo ./make.sh"
fi
