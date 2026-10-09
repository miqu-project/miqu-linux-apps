<p align="center">
  <img src="assets/miquinfo.png" alt="MiquInfo Logo" width="140" />
</p>

# MiquInfo 💻

A lightweight, modern native Wayland system information viewer and hardware monitor, built with **`miqutoolkit`**.

MiquInfo offers an elegant graphical system summary—displaying OS details, Wayland compositor state, CPU/GPU hardware profiles, and live memory statistics.

---

## ✨ Features

- 🖥️ **System & OS Diagnostics**: Real-time identification of kernel version, distribution details, uptime, and shell environment.
- ⚡ **Compositor & Session Info**: Native detection of the running Wayland compositor (`miquland`), display server protocols, and active outputs.
- ⚙️ **Hardware Overview**: Detailed inspection of CPU model, core topologies, GPU acceleration, and PCI devices.
- 📊 **Resource Utilization**: Live graphical indicators for RAM usage, swap status, and storage allocations.
- 🎨 **Miqu Design System**: Polished UI adhering strictly to Miqu theme tokens and typography.

---

## 🛠️ Build & Installation

### Prerequisites
- C++20 compiler (`gcc` or `clang`)
- CMake 3.20+
- `miqutoolkit`
- `libpci`, `libudev`

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

MiquInfo adheres to the Miqu configuration doctrine:
- Shipped root template: `/usr/share/miquinfo/miquinfo.conf`
- Generate user configuration:
```bash
miquinfo --init-config
```
This generates `~/.config/miquinfo/miquinfo.conf` with live inotify reload support.
