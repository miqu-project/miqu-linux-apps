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
    "miqudm"
    "miqutest"
)

echo "================================================================"
echo " Starting build & install for miqu-linux-apps"
echo " Target directory: $ROOT_DIR"
echo " Applications: ${APPS[*]}"
echo "================================================================"

for app in "${APPS[@]}"; do
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
    echo "==> Processing [$app]..."
    echo "----------------------------------------------------------------"
    (
        cd "$app_dir"
        chmod +x make.sh
        ./make.sh "$@"
    )
    echo "==> [$app] completed successfully."
done

echo ""
echo "================================================================"
echo " All miqu-linux-apps built & installed successfully!"
echo "================================================================"
