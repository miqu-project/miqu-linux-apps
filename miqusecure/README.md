<p align="center">
  <img src="assets/miqusecure.png" alt="MiquSecure Logo" width="140" />
</p>

# MiquSecure 🛡️

A modern, native Wayland security and privacy control center engineered with **`miqutoolkit`** for the Miqu desktop ecosystem.

MiquSecure unifies system hardening, network defenses, hardware kill switches, and application isolation into a streamlined, high-performance interface.

---

## ✨ Features

- 🛡️ **Firewall Management**: Real-time UFW status, default policy toggling (incoming/outgoing deny/allow), and custom rule management.
- 🔒 **DNS Privacy & Encryption**: Encrypted DNS configuration (systemd-resolved / DoH / DNSCrypt) with preconfigured privacy resolvers (Mullvad, Quad9, Cloudflare).
- 👁️ **Hardware Shields**: Hardware kill-switches to instantly disable webcam devices, microphone inputs, Bluetooth, and Wi-Fi radios.
- 📦 **Application Sandboxing**: Bubblewrap & AppArmor integration to launch untrusted applications in isolated sandboxes.
- 🧹 **Metadata Scrubber**: Drag-and-drop privacy cleaner powered by MAT2 to purge EXIF, geolocation tags, and author metadata from photos, PDFs, and documents.
- 📊 **Traffic & Connection Monitor**: Live bandwidth monitoring per network interface and active TCP/UDP socket inspection.

---

## 🛠️ Build & Installation

### Prerequisites
- C++20 compiler (`gcc` or `clang`)
- CMake 3.20+
- `miqutoolkit` (installed system-wide or adjacent directory)
- Optional runtime utilities: `ufw`, `bubblewrap`, `mat2`, `systemd-resolved`

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

MiquSecure follows the Miqu ecosystem configuration doctrine:
- Shipped root template: `/usr/share/miqusecure/miqusecure.conf`
- Generate user configuration:
```bash
miqusecure --init-config
```
This creates `~/.config/miqusecure/miqusecure.conf` with full live inotify reload support.
