#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "${EUID}" -ne 0 ]; then
    echo "==> Warning: Running without root/sudo privileges."
    echo "    To install system-wide to /usr, run: sudo $0"
    echo ""
fi

APPS=(
    "miquidle"
    "miqulock"
    "miqubg"
    "miqudesk"
    "miqupolkit"
    "miqulauncher"
    "miquinfo"
    "miqumusic"
    "miqugallery"
    "miqusecure"
    "miquoverview"
    "miqubar"
    "miqudm"
    "miqutest"
)

TOTAL=${#APPS[@]}

echo "================================================================"
echo " Starting complete build & install for miqu-linux-apps"
echo " Target directory: $ROOT_DIR"
echo " Total applications: $TOTAL"
echo " Applications: ${APPS[*]}"
echo "================================================================"

COUNT=0
for app in "${APPS[@]}"; do
    COUNT=$((COUNT + 1))
    app_dir="$ROOT_DIR/$app"
    if [ ! -d "$app_dir" ]; then
        echo "==> Error: Directory $app_dir not found! Skipping..." >&2
        continue
    fi

    if [ ! -f "$app_dir/make.sh" ]; then
        echo "==> Error: $app_dir/make.sh not found! Skipping..." >&2
        continue
    fi

    echo ""
    echo "----------------------------------------------------------------"
    echo "==> [$COUNT/$TOTAL] Building & installing $app..."
    echo "----------------------------------------------------------------"
    (
        cd "$app_dir"
        if [ -f "build/CMakeCache.txt" ]; then
            if ! grep -q "$app_dir" "build/CMakeCache.txt" 2>/dev/null; then
                echo "==> Stale CMakeCache detected for $app. Resetting build directory..."
                rm -rf build
            fi
        fi
        chmod +x make.sh
        ./make.sh "$@"
    )
    echo "==> [$app] completed successfully."
done

echo ""
echo "================================================================"
echo " All $TOTAL miqu-linux-apps built & installed successfully!"
echo "================================================================"
