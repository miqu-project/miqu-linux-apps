# miqusettings

<p align="center">
  <b>Modern Miller Columns Centralized System Settings & Desktop Configuration Manager for the Miqu Desktop Ecosystem.</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C%2B%2B20-blue?style=flat-square" alt="C++20" />
  <img src="https://img.shields.io/badge/Toolkit-miqutoolkit-purple?style=flat-square" alt="miqutoolkit" />
  <img src="https://img.shields.io/badge/Display-Wayland%20Native-orange?style=flat-square" alt="Wayland Native" />
  <img src="https://img.shields.io/badge/License-GPL--3.0-green?style=flat-square" alt="GPLv3" />
</p>

---

## 🌟 Overview

`miqusettings` (Display name: **Settings**) provides a unified, keyboard- and mouse-accessible control center for system and desktop configuration across the Miqu ecosystem.

It adopts an ergonomic **3-pillar Miller Columns architecture**:
1. **Left Pillar (Category Navigator)**: Primary root categories and system configuration sections (Applications & Defaults, Wireless & Network, Display, Appearance, Audio, Power, Privacy & Security, System Info).
2. **Center Pillar (Subcategory / Setting Items)**: Hierarchical setting lists, actionable modules, or toggles within the active category.
3. **Right Pillar (Rich Interactive Preview / Setting Detail)**: In-depth setting editor, live network status, Bluetooth pairing wizards, Wi-Fi credentials modals, and device telemetry.

---

## 🔌 Modules & Features

- **Applications & Defaults**:
  - Preferred default applications (Web Browser, File Manager, Terminal, Email, Video/Audio Players).
  - Native `.desktop` parsing, MIME association detection, and direct fallback handlers.
- **Wireless & Network**:
  - **Airplane Mode**: Master radio kill-switch coordinating `rfkill`, NetworkManager Wi-Fi/WWAN radios, and BlueZ Bluetooth controllers with live per-subsystem telemetry.
  - **Wi-Fi**: Instant cached network scanning (`--rescan no` for sub-second startup), active network telemetry, secure WPA2/WPA3 and 802.1x Enterprise authentication modals, and QR sharing.
  - **Ethernet**: Wired interface link detection, speed/duplex metrics, IPv4/IPv6 address inspection, gateway and DNS telemetry, and DHCP/Static configuration.
  - **Bluetooth**: BlueZ D-Bus integration, agent authorization and PIN/passkey pairing modals, paired and nearby device management, and bounded scanning with 0% idle CPU dormancy.

---

## ⌨️ Controls & Navigation

| Action | Shortcuts |
|---|---|
| **Navigate Up / Down** | `k` / `j`, `Up` / `Down` Arrow, Mouse Click / Scroll |
| **Drill In (Expand / Select)** | `l`, `Right Arrow`, `Return` / `Enter` |
| **Navigate Back (Parent / Collapse)** | `h`, `Left Arrow`, `Backspace`, Breadcrumb Click |
| **Search Settings** | `/`, Toolbar Search |
| **Quit** | `q`, `Escape` (when search is inactive), Window Close Button |

---

## ⚙️ Configuration Architecture

`miqusettings` strictly follows the **Uniform Two-Tier Single-File Authority Model**:
- **Tier 1 (System Baseline)**: `/usr/share/miqusettings/miqusettings.conf`
- **Tier 2 (User Standalone)**: `~/.config/miqusettings/miqusettings.conf`

To generate a documented user configuration with safe timestamped archiving:
```bash
miqusettings --init-config
```

---

## 🔨 Build & Install

```bash
# Build locally
./make.sh

# Install system-wide
sudo ./make.sh
```
