<p align="center">
  <img src="assets/miqutheme.png" alt="MiquTheme Logo" width="140" />
</p>

# MiquTheme 🎨

Visual theme, appearance, and wallpaper manager for the Miqu desktop ecosystem, built natively with **`miqutoolkit`**.

MiquTheme allows users to browse, generate, and select dynamic Material You color schemes, manage system wallpapers, and propagate global design tokens across all running Miqu applications in real time.

---

## ✨ Features

- 🌈 **Material Design 3 Palette Generation**: Uses `matugen` to extract harmonious color schemes directly from wallpapers.
- 🖼️ **Wallpaper Management**: Visual gallery to browse, preview, and apply wallpapers with instant desktop updates via `miqubg`.
- 🔄 **Real-Time Token Propagation**: Instantly exports active tokens to `~/.config/miqutheme/current/` triggering live inotify reloads across `miqubar`, `miqulauncher`, `miqudesk`, and all other ecosystem applications.
- 🌓 **Light & Dark Mode Support**: Dynamic switching between light and dark palettes.

---

## 🛠️ Build & Installation

### Prerequisites
- C++20 compiler (`gcc` or `clang`)
- CMake 3.20+
- `miqutoolkit`
- `matugen` (runtime dependency for dynamic color generation)

### Local Build
```bash
./make.sh
```

### System Installation
To build and install system-wide to `/usr`:
```bash
sudo ./make.sh
```

---

## ⚙️ Configuration

MiquTheme stores its active runtime exports and configurations under:
- System assets & defaults: `/usr/share/miqutheme/`
- User schemes & wallpapers: `~/.config/miqutheme/`
- Current exported tokens: `~/.config/miqutheme/current/`
