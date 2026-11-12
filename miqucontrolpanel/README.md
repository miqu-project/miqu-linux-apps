# MiquControlPanel 🎛️

Modern Miller-columns centralized desktop control panel and system configuration manager built natively with **`miqutoolkit`** for the Miqu desktop ecosystem.

---

## ✨ Features

- 🌐 **Network & Connectivity**: Native Wi-Fi management, Bluetooth agent, WireGuard VPN tunnel integration, RFC 7858 Private DNS (DoT), OpenSnitch per-app rules, and firewall management.
- 🖥️ **Monitors & Displays**: Live multi-monitor discovery, resolution, scaling, and orientation control.
- 🎨 **Appearance & Themes**: Live token inspections, wallpaper synchronization, and custom styling.
- 📦 **Applications & Defaults**: System application launcher and package exploration.

---

## 🛠️ Build & Installation

### Prerequisites
- C++20 compiler (`gcc` or `clang`)
- CMake 3.20+
- `miqutoolkit`

### Local Build
```bash
./make.sh
```

### System Installation
To build and install system-wide to `/usr`:
```bash
sudo ./make.sh
```
