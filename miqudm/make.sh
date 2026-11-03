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
    echo "==> Ensuring greeter user and hardware groups (video, input, seat)..."
    if ! id -u greeter >/dev/null 2>&1; then
        useradd -r -s /usr/bin/nologin -d /var/empty -m -G video,input,seat greeter || true
    else
        usermod -a -G video,input,seat greeter || true
    fi

    echo "==> Installing miqudm to system (/usr/bin, /etc)..."
    cmake --install "$BUILD_DIR"
    systemctl daemon-reload 2>/dev/null || true
    echo "==> miqudm successfully installed."
else
    echo "==> To install system-wide, run: sudo ./make.sh"
fi
