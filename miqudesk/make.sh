#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "==> Configuring miqudesk ($BUILD_TYPE)..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="/usr" "$@"

echo "==> Building miqudesk..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Build finished successfully! Binary is at $BUILD_DIR/miqudesk"

# If invoked with sudo/root, automatically install to system
if [ "${EUID}" -eq 0 ]; then
    echo "==> Installing miqudesk to system (/usr/bin/miqudesk)..."
    cmake --install "$BUILD_DIR"
    echo "==> miqudesk successfully installed to /usr!"
else
    echo "==> Built locally in $BUILD_DIR. To install system-wide, run: sudo ./make.sh"
fi
