<p align="center">
  <img src="assets/miqumusic.png" alt="MiquMusic Logo" width="140" />
</p>

# MiquMusic 🎵

A fast, modern native Wayland music player designed for MPD (Music Player Daemon), engineered with **`miqutoolkit`** for the Miqu desktop environment.

MiquMusic delivers smooth local audio playback, full queue controls, responsive seek mechanics, and integrated album artwork display.

---

## ✨ Features

- 🎧 **Native MPD Integration**: Seamless socket connection to local or remote Music Player Daemon instances.
- 🎛️ **Full Playback Controls**: Play, pause, seek, track skip, shuffle, repeat, and volume management.
- 📜 **Queue & Playlist Management**: Real-time playlist viewer, dynamic queue ordering, and quick search.
- 🖼️ **Album Art & Track Metadata**: Embedded album artwork display with artist, album, and track typography.
- ⚡ **Wayland Native & Zero Bloat**: Built with `miqutoolkit` widgets and Cairo rendering with zero Electron/web overhead.

---

## 🛠️ Build & Installation

### Prerequisites
- C++20 compiler (`gcc` or `clang`)
- CMake 3.20+
- `miqutoolkit`
- `libmpdclient`

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

MiquMusic adheres to the Miqu configuration doctrine:
- Shipped root template: `/usr/share/miqumusic/miqumusic.conf`
- Generate user configuration:
```bash
miqumusic --init-config
```
This generates `~/.config/miqumusic/miqumusic.conf` with live inotify reload support.
