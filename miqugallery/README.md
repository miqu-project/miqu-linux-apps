<p align="center">
  <img src="assets/miqugallery.png" alt="MiquGallery Logo" width="140" />
</p>

# MiquGallery 🖼️

A fast, modern native Wayland gallery and media viewer for high-resolution images and videos, built with **`miqutoolkit`**.

MiquGallery provides fluid panning, responsive zooming, intuitive folder navigation, and hardware-accelerated playback for the Miqu desktop ecosystem.

---

## ✨ Features

- 📸 **Extensive Format Support**: Full viewing support for PNG, JPEG, WebP, AVIF, HEIC, TIFF, BMP, and SVG.
- 🎬 **Video Playback**: Native video streaming and playback for MP4, MKV, WebM, and AVI containers.
- 🔍 **Smooth Zoom & Pan**: Fluid mouse/trackpad panning, gesture zooming, fit-to-window, and 1:1 pixel inspection.
- 🗂️ **Folder & Album Browsing**: Dynamic thumbnail filmstrip and quick folder navigation.
- ℹ️ **Metadata & EXIF Inspector**: Clean inspection of camera parameters, resolution, color profiles, and timestamps.

---

## 🛠️ Build & Installation

### Prerequisites
- C++20 compiler (`gcc` or `clang`)
- CMake 3.20+
- `miqutoolkit`
- Media libraries: `gstreamer`, `libjpeg-turbo`, `libpng`, `libwebp`

### Build
```bash
cmake -B build -S .
cmake --build build -j$(nproc)
```

### Install
To build and install system-wide to `/usr`:
```bash
sudo ./make.sh
```

---

## ⚙️ Configuration

MiquGallery adheres to the Miqu configuration doctrine:
- Shipped root template: `/usr/share/miqugallery/miqugallery.conf`
- Generate user configuration:
```bash
miqugallery --init-config
```
This generates `~/.config/miqugallery/miqugallery.conf` with live inotify reload support.
