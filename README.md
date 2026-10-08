# miqu-linux-apps

> **First-party native Linux desktop applications suite built with `miqutoolkit`.**

This repository contains the integrated first-party desktop applications for the [Miqu ecosystem](https://github.com/miqu-project). All applications are written in modern C++20 and share a single unified UI library ([`miqutoolkit`](https://github.com/miqu-project/miqutoolkit)) for consistent styling, instant theme propagation, and fluid Wayland animations.

---

## 📦 Application Directory

| Component | Description |
|---|---|
| [**`miqudesk`**](./miqudesk) | Interactive desktop canvas & widget layer bringing familiar desktop ergonomics to Wayland tiling. |
| [**`miqusecure`**](./miqusecure) | Privacy, security, and firewall management utility (Tor, OpenSnitch, LUKS, audit policies). |
| [**`miqumusic`**](./miqumusic) | Lightweight, fast native MPD client and audio player. |
| [**`miqulauncher`**](./miqulauncher) | Application launcher, dmenu alternative, and workspace/window switcher. |
| [**`miquoverview`**](./miquoverview) | GPU-accelerated live workspace expose and overview plugin for `miquland`. |
| [**`miqugallery`**](./miqugallery) | Image viewer and media gallery application. |
| [**`miquinfo`**](./miquinfo) | System telemetry and hardware monitoring utility. |
| [**`miqudm`**](./miqudm) | Wayland-native Display Manager & greeter with PAM/logind session management. |
| [**`miqubg`**](./miqubg) | Wallpaper daemon for setting and transitioning background images. |
| [**`miqulock`**](./miqulock) | Session locking utility using the Wayland `ext-session-lock-v1` protocol. |
| [**`miquidle`**](./miquidle) | Idle management daemon using the Wayland `ext-idle-notify-v1` protocol. |
| [**`miqupolkit`**](./miqupolkit) | Polkit authentication agent built with `miqutoolkit`. |
| [**`miqutest`**](./miqutest) | Widget testing playground and interactive showcase for `miqutoolkit` controls. |

---

## 🔨 Building & Installation

### Prerequisites
- [`miqutoolkit`](https://github.com/miqu-project/miqutoolkit) built and installed on the system.
- Standard build tools (`cmake`, `ninja` or `make`, C++20 compatible compiler).

### Build All Applications
```bash
sudo ./install_all.sh
```

### Build a Single Application
Navigate to any individual application folder and execute its `make.sh`:
```bash
cd miqudesk
sudo ./make.sh
```
