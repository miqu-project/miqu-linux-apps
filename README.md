# miqu-linux-apps

<p align="center">
  <b>First-party native Linux desktop applications suite built with miqutoolkit.</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C%2B%2B20-blue?style=flat-square" alt="C++20" />
  <img src="https://img.shields.io/badge/Toolkit-miqutoolkit-purple?style=flat-square" alt="miqutoolkit" />
  <img src="https://img.shields.io/badge/Display-Wayland%20Native-orange?style=flat-square" alt="Wayland Native" />
  <img src="https://img.shields.io/badge/License-GPL--3.0-green?style=flat-square" alt="GPLv3" />
</p>

---

This repository contains the integrated first-party desktop applications for the [Miqu desktop ecosystem](https://github.com/miqu-project). All applications are written in modern C++20 and share a single unified UI framework ([`miqutoolkit`](https://github.com/miqu-project/miqutoolkit)) for consistent styling, instant theme propagation, fluid Wayland animations, and zero UI fragmentation.

---

## 📦 Application Directory

| Component | Category | Description |
|---|:---:|---|
| [**`miqubar`**](./miqubar) | Shell | Wayland status bar and taskbar with workspace indicators, system tray, battery/network telemetry, and flyout menus. |
| [**`miqudesk`**](./miqudesk) | Shell | Interactive desktop canvas & widget layer bringing familiar desktop ergonomics to Wayland tiling. |
| [**`miqulauncher`**](./miqulauncher) | Shell | Fast application launcher, fuzzy dmenu alternative, workspace navigator, and window switcher. |
| [**`miquoverview`**](./miquoverview) | Plugin | GPU-accelerated live workspace expose and overview plugin for `miquland`. |
| [**`miqunotify`**](./miqunotify) | Daemon | Native desktop notification daemon and slide-out notification history center. |
| [**`miqutheme`**](./miqutheme) | Theming | Visual theme and appearance manager for color schemes, wallpapers, and global design tokens. |
| [**`miqumusic`**](./miqumusic) | App | Fast, lightweight native MPD (Music Player Daemon) client and audio playback interface. |
| [**`miqusecure`**](./miqusecure) | Security | Privacy, firewall, and security command center (Tor routing, OpenSnitch rules, LUKS status, auditing). |
| [**`miqugallery`**](./miqugallery) | App | Hardware-accelerated image viewer and media gallery application. |
| [**`miquinfo`**](./miquinfo) | Diagnostic | Hardware monitoring, system telemetry, and resource diagnostic utility. |
| [**`miqudm`**](./miqudm) | Greeter | Wayland-native Display Manager & greeter with PAM and logind session management. |
| [**`miqubg`**](./miqubg) | Wallpaper | Independent wallpaper daemon responsible for rendering and transitioning desktop background images. |
| [**`miqulock`**](./miqulock) | Security | Session locking utility implementing the Wayland `ext-session-lock-v1` protocol. |
| [**`miquidle`**](./miquidle) | Daemon | Idle management daemon monitoring user activity via the Wayland `ext-idle-notify-v1` protocol. |
| [**`miqupolkit`**](./miqupolkit) | Security | Native graphical Polkit authentication agent handling privilege escalation prompts. |
| [**`miqutest`**](./miqutest) | Sandbox | Interactive widget showcase and testing harness for developing `miqutoolkit` controls. |

---

## ⚙️ Configuration Management

All applications strictly follow the **Uniform Two-Tier Configuration Architecture**:
1. **Zero-Config Baseline**: Applications run 100% out of the box directly from `/usr/share/<app>/<app>.conf` using `miqutoolkit` defaults. No files are auto-seeded on launch.
2. **On-Demand Configuration (`<app> --init-config`)**: Generates user configuration at `~/.config/<app>/<app>.conf`.
3. **Safe Timestamped Archiving**: If a user configuration already exists, running `--init-config` safely archives it to `<app>.conf.bak.<YYYYMMDD-HHMMSS>` before copying the fresh default, preventing accidental data loss.
4. **Single-File Authority**: Once the user configuration exists, the app executes exclusively from that file without hidden background merging.

---

## 🔨 Building & Installation

### Prerequisites
* [`miqutoolkit`](https://github.com/miqu-project/miqutoolkit) built and installed system-wide (`cd miqutoolkit && sudo ./make.sh`).
* Standard build tools (`cmake`, `ninja` or `make`, C++20 compiler).

### Build & Install All Applications
```bash
sudo ./install_all.sh
```

### Build & Install an Individual Application
Every application has a standalone `make.sh`:
```bash
# Test compile locally without root
cd miqubar
./make.sh

# Install system-wide to /usr
sudo ./make.sh
```

---

## 📄 License
Licensed under the GNU General Public License v3.0 (with `miqutoolkit` under Apache 2.0).
